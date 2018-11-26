cmake_minimum_required(VERSION 3.13)
project(apigen)


# LLVM and clang related stuff
set(LLVM_CONFIG llvm-config)

execute_process(COMMAND ${LLVM_CONFIG} --src-root OUTPUT_VARIABLE LLVM_SRC_DIRECTORY OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --obj-root OUTPUT_VARIABLE LLVM_OBJ_DIRECTORY OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG} --cxxflags OUTPUT_VARIABLE LLVM_CXX_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_CXX_FLAGS)
execute_process(COMMAND ${LLVM_CONFIG} --ldflags OUTPUT_VARIABLE LLVM_LD_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_LD_FLAGS)
execute_process(COMMAND ${LLVM_CONFIG} --libs OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_LIBS)
execute_process(COMMAND ${LLVM_CONFIG} --system-libs OUTPUT_VARIABLE LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(LLVM_SYSTEM_LIBS)

set(CLANG_INCLUDE_DIRECTORIES ${LLVM_SRC_DIRECTORY}/tools/clang/include ${LLVM_OBJ_DIRECTORY}/tools/clang/include)

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


add_executable(apigen)
target_compile_features(apigen
	PRIVATE
		cxx_std_17)

target_sources(apigen
	PRIVATE
		apigen_definitions.h
		entity.h
		entity_registry.h
		entity_types.h
		parsing.h
		main.cpp)
target_include_directories(apigen
	PRIVATE
		${CLANG_INCLUDE_DIRECTORIES})
target_compile_options(apigen
	PRIVATE
		${LLVM_CXX_FLAGS})

target_link_libraries(apigen
	PRIVATE
		${LLVM_LIBS}
		${LLVM_SYSTEM_LIBS}
		${CLANG_LIBRARIES})
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
			/W3)
elseif(CMAKE_COMPILER_IS_GNUCXX)
	target_compile_options(apigen
		PRIVATE
			-Wall -Wextra -Wconversion)
endif()
