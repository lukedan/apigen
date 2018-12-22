#include <vector>
#include <iostream>
#include <fstream>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Parse/ParseAST.h>

#include "apigen_definitions.h"
#include "entity.h"
#include "entity_registry.h"
#include "parsing.h"

using namespace apigen;

int main(int argc, char **argv) {
	clang::CompilerInstance compiler;
	initialize_compiler_instance(compiler, argc, argv, true);
	if (compiler.getFrontendOpts().Inputs.empty()) {
		return -1;
	}

	entity_registry reg;
	ast_visitor visitor(reg);
	compiler.setASTConsumer(llvm::make_unique<ast_consumer>(visitor));

	if (compiler.getFrontendOpts().Inputs.size() > 1) {
		std::cerr << "warning: main file not unique\n";
	}
	const clang::FileEntry *file = compiler.getFileManager().getFile(compiler.getFrontendOpts().Inputs[0].getFile());
	compiler.getSourceManager().setMainFileID(compiler.getSourceManager().createFileID(
		file, clang::SourceLocation(), clang::SrcMgr::C_User
	));

	compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(), &compiler.getPreprocessor());
	clang::ParseAST(compiler.getPreprocessor(), &compiler.getASTConsumer(), compiler.getASTContext());
	compiler.getDiagnosticClient().EndSourceFile();

	naming_conventions conv;
	conv.host_function_name_pattern = "_api_wrapper_{0}_{1}";
	conv.api_function_name_pattern = "{0}_{1}_{2}";

	conv.api_enum_name_pattern = "{0}_{1}";
	conv.api_enum_entry_name_pattern = "{0}_{1}_{2}";

	conv.api_record_name_pattern = "{0}_{1}";
	conv.api_move_struct_name_pattern = "{0}_{1}_moveref";
	conv.api_templated_record_name_pattern = "{0}_{1}_{2}";
	conv.api_templated_record_move_struct_name_pattern = "{0}_{1}_{2}_moveref";

	conv.host_method_name_pattern = "_api_wrapper_{0}_{1}_{2}";
	conv.api_method_name_pattern = "{0}_{1}_{2}";

	conv.host_field_getter_name_pattern = "_api_wrapper_{0}_{1}_{2}_getter";
	conv.api_field_getter_name_pattern = "{0}_{1}_get_{2}";

	conv.env_separator = "_";

	conv.api_struct_name = "codepad_api";
	conv.host_init_api_function_name = "init_api";

	reg.prepare_for_export(conv);
	{
		std::ofstream
			api_header_out("header.h"),
			host_source_out("src.cpp");
		export_writer
			api_header_writer(api_header_out),
			host_source_writer(host_source_out);
		host_source_writer << "#include \"header.h\"\n\n";
		reg.export_api_header(api_header_writer, conv);
		reg.export_host_source(host_source_writer, conv);
	}

	std::cout << "\n\n";
	std::cout << "exported entities:\n";
	for (entity *ent : reg.exported_entities) {
		std::cout << ent->declaration->getDeclKindName() << ", " << ent->declaration->getName().str() << "\n";
	}

	return 0;
}
