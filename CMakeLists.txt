cmake_minimum_required(VERSION 3.13)
project(apigen)

# LLVM and clang related stuff
set(LLVM_CONFIG "llvm-config" CACHE FILEPATH "Path to the llvm-config executable.")

execute_process(COMMAND "${LLVM_CONFIG}" --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${LLVM_CONFIG}" --obj-root OUTPUT_VARIABLE LLVM_OBJ_DIRECTORY OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${LLVM_CONFIG}" --cxxflags OUTPUT_VARIABLE LLVM_CXX_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${LLVM_CONFIG}" --ldflags OUTPUT_VARIABLE LLVM_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND "${LLVM_CONFIG}" --libs OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_LIBS)
execute_process(COMMAND "${LLVM_CONFIG}" --system-libs OUTPUT_VARIABLE LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_SYSTEM_LIBS)

set(CLANG_LIBRARIES
	clangFrontendTool
	clangFrontend
	clangDriver
	clangSerialization
	clangCodeGen
	clangParse
	clangSema
	clangStaticAnalyzerFrontend
	clangStaticAnalyzerCheckers
	clangStaticAnalyzerCore
	clangAnalysis
	clangARCMigrate
	clangRewrite
	clangRewriteFrontend
	clangEdit
	clangAST
	clangLex
	clangBasic)

# packages
find_package(fmt CONFIG REQUIRED)
set(GFLAGS_USE_TARGET_NAMESPACE YES)
find_package(gflags CONFIG REQUIRED)

# TODO this is a workaround for vcpkg not having RelWithDebInfo builds
set_target_properties(fmt::fmt gflags::gflags
	PROPERTIES
		MAP_IMPORTED_CONFIG_MINSIZEREL Release
		MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)

add_executable(apigen)
target_compile_features(apigen
	PRIVATE cxx_std_17)

set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/src")
target_sources(apigen
	PUBLIC
		"${SOURCE_PATH}/apigen_definitions.h"
	PRIVATE
		"${SOURCE_PATH}/entity_kinds/constructor_entity.h"
		"${SOURCE_PATH}/entity_kinds/enum_entity.h"
		"${SOURCE_PATH}/entity_kinds/field_entity.cpp"
		"${SOURCE_PATH}/entity_kinds/field_entity.h"
		"${SOURCE_PATH}/entity_kinds/function_entity.h"
		"${SOURCE_PATH}/entity_kinds/method_entity.h"
		"${SOURCE_PATH}/entity_kinds/record_entity.cpp"
		"${SOURCE_PATH}/entity_kinds/record_entity.h"
		"${SOURCE_PATH}/entity_kinds/user_type_entity.h"
		"${SOURCE_PATH}/basic_naming_convention.h"
		"${SOURCE_PATH}/cpp_writer.h"
		"${SOURCE_PATH}/dependency_analyzer.cpp"
		"${SOURCE_PATH}/dependency_analyzer.h"
		"${SOURCE_PATH}/entity.cpp"
		"${SOURCE_PATH}/entity.h"
		"${SOURCE_PATH}/entity_registry.h"
		"${SOURCE_PATH}/exporter.cpp"
		"${SOURCE_PATH}/exporter.h"
		"${SOURCE_PATH}/internal_name_printer.cpp"
		"${SOURCE_PATH}/internal_name_printer.h"
		"${SOURCE_PATH}/main.cpp"
		"${SOURCE_PATH}/misc.h"
		"${SOURCE_PATH}/naming_convention.cpp"
		"${SOURCE_PATH}/naming_convention.h"
		"${SOURCE_PATH}/parser.h"
		"${SOURCE_PATH}/types.cpp"
		"${SOURCE_PATH}/types.h")
target_include_directories(apigen
	PRIVATE "${LLVM_INCLUDE_DIR}")
target_compile_options(apigen
	PRIVATE ${LLVM_CXX_FLAGS})

target_link_libraries(apigen
	PRIVATE ${LLVM_LIBS} ${LLVM_SYSTEM_LIBS} ${CLANG_LIBRARIES})
target_link_options(apigen
	PRIVATE ${LLVM_LD_FLAGS})

target_link_libraries(apigen
	PRIVATE fmt::fmt gflags::gflags)

if(WIN32)
	target_compile_definitions(apigen
		PRIVATE _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
	target_link_libraries(apigen
		PRIVATE version)
endif()

if(MSVC)
	target_compile_options(apigen
		PRIVATE /W4 /experimental:external /external:anglebrackets /external:W0)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(apigen
		PRIVATE -Wall -Wextra -Wconversion)
endif()
