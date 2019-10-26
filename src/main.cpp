#include <fstream>

#include <clang/Lex/PreprocessorOptions.h>

#include "dependency_analyzer.h"
#include "entity_registry.h"
#include "exporter.h"
#include "parser.h"
#include "basic_naming_convention.h"

using namespace apigen;

int main(int argc, char **argv) {
	llvm::ArrayRef<char*> args(argv, argc);
	auto invocation = clang::createInvocationFromCommandLine(args);
	invocation->getPreprocessorOpts().addMacroDef("APIGEN_ACTIVE");

	entity_registry reg;
	parser p(std::move(invocation));
	dependency_analyzer dep_analyzer;

	reg.analyzer = &dep_analyzer;

	p.parse(reg);
	dep_analyzer.analyze(reg);

	basic_naming_convention naming(reg);
	naming.api_struct_name = "api";
	naming.api_struct_init_function_name = "init_api";

	exporter exp(p.get_compiler().getASTContext().getPrintingPolicy(), naming);
	exp.collect_exported_entities(reg);

	{
		std::ofstream out("api.h");
		exp.export_api_header(out);
	}
	{
		std::ofstream out("host.h");
		exp.export_host_h(out);
	}
	{
		std::ofstream out("host.cpp");
		exp.export_host_cpp(out);
	}
	{
		std::ofstream out("collect.cpp");
		exp.export_data_collection_cpp(out);
	}

	return 0;
}
