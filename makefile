ifeq ($(OS), Windows_NT)
	CXX := clang-cl
	LIBNAME = $(1).lib
	PLATFORM_LIBRARIES := \
		$(call LIBNAME,version)
	PLATFORM_CXXFLAGS := -Xclang -std=c++17 /MD /Zi -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	LINKER_SEP := /link
	LDFLAGS := /DEBUG
else
	CXX := clang++
	LIBNAME = -l$(1)
	PLATFORM_LIBRARIES :=
	PLATFORM_CXXFLAGS := -std=c++17 -fno-rtti
	LINKER_SEP :=
endif

LLVMCOMPONENTS := cppbackend
LLVMCONFIG := llvm-config

CXXFLAGS := \
	-I$(shell $(LLVMCONFIG) --src-root)/tools/clang/include \
	-I$(shell $(LLVMCONFIG) --obj-root)/tools/clang/include \
	$(shell $(LLVMCONFIG) --cxxflags) \
	$(PLATFORM_CXXFLAGS)
LDFLAGS := $(LDFLAGS) \
	$(shell $(LLVMCONFIG) --ldflags --libs $(LLVMCOMPONENTS))

CLANGLIBS := \
	$(shell $(LLVMCONFIG) --libs) \
	$(shell $(LLVMCONFIG) --system-libs) \
	\
	$(call LIBNAME,clangTooling) \
	$(call LIBNAME,clangFrontendTool) \
	$(call LIBNAME,clangFrontend) \
	$(call LIBNAME,clangDriver) \
	$(call LIBNAME,clangSerialization) \
	$(call LIBNAME,clangCodeGen) \
	$(call LIBNAME,clangParse) \
	$(call LIBNAME,clangSema) \
	$(call LIBNAME,clangStaticAnalyzerFrontend) \
	$(call LIBNAME,clangStaticAnalyzerCheckers) \
	$(call LIBNAME,clangStaticAnalyzerCore) \
	$(call LIBNAME,clangAnalysis) \
	$(call LIBNAME,clangARCMigrate) \
	$(call LIBNAME,clangRewrite) \
	$(call LIBNAME,clangRewriteFrontend) \
	$(call LIBNAME,clangEdit) \
	$(call LIBNAME,clangAST) \
	$(call LIBNAME,clangLex) \
	$(call LIBNAME,clangBasic) \
	\
	$(PLATFORM_LIBRARIES)

SOURCES = main.cpp

OBJECTS = $(SOURCES:.cpp=.o)
EXES = $(OBJECTS:.o=)

all: $(OBJECTS) $(EXES)

%: %.o
	$(CXX) -o $@ $< $(LINKER_SEP) $(CLANGLIBS) $(LDFLAGS)

clean:
	-rm -f $(EXES) $(OBJECTS) *~
