#include <vector>
#include <iostream>
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

	reg.propagate_export();
	std::cout << "\n\n";
	std::cout << "exported entities:\n";
	for (entity *ent : reg.exported_entities) {
		std::cout << ent->declaration->getDeclKindName() << ", " << ent->declaration->getName().str() << "\n";
	}

	return 0;
}
