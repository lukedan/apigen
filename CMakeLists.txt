cmake_minimum_required(VERSION 3.13)
project(apigen)


# LLVM and clang related stuff
if (NOT DEFINED LLVM_CONFIG)
	set(LLVM_CONFIG llvm-config)
endif()

execute_process(COMMAND ${LLVM_CONFIG} --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --obj-root OUTPUT_VARIABLE LLVM_OBJ_DIRECTORY OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXX_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --ldflags OUTPUT_VARIABLE LLVM_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --libs OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_LIBS)
execute_process(COMMAND ${LLVM_CONFIG} --system-libs OUTPUT_VARIABLE LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
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


find_package(fmt CONFIG REQUIRED)


add_executable(apigen)
target_compile_features(apigen
	PRIVATE
		cxx_std_17)

target_sources(apigen
	PUBLIC
		apigen_definitions.h
	PRIVATE
		src/entity_kinds/constructor_entity.h
		src/entity_kinds/enum_entity.h
		src/entity_kinds/field_entity.cpp
		src/entity_kinds/field_entity.h
		src/entity_kinds/function_entity.h
		src/entity_kinds/method_entity.h
		src/entity_kinds/record_entity.cpp
		src/entity_kinds/record_entity.h
		src/entity_kinds/user_type_entity.h
		src/basic_naming_convention.h
		src/cpp_writer.h
		src/dependency_analyzer.cpp
		src/dependency_analyzer.h
		src/entity.cpp
		src/entity.h
		src/entity_registry.h
		src/exporter.h
		src/main.cpp
		src/misc.h
		src/naming_convention.h
		src/parser.h
		src/types.cpp
		src/types.h)
target_include_directories(apigen
	PRIVATE
		${LLVM_INCLUDE_DIR})
target_compile_options(apigen
	PRIVATE
		${LLVM_CXX_FLAGS})

target_link_libraries(apigen
	PRIVATE
		${LLVM_LIBS}
		${LLVM_SYSTEM_LIBS}
		${CLANG_LIBRARIES}
		fmt::fmt)
target_link_options(apigen
	PRIVATE
		${LLVM_LD_FLAGS})

if(WIN32)
	target_compile_definitions(apigen
		PRIVATE
			_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
	target_link_libraries(apigen
		PRIVATE
			version)
endif()

if(MSVC)
	target_compile_options(apigen
		PRIVATE
			/W4 /Zi /experimental:external /external:anglebrackets /external:W0)
	target_link_options(apigen
		PRIVATE
			/DEBUG)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(apigen
		PRIVATE
			-Wall -Wextra -Wconversion)
endif()
