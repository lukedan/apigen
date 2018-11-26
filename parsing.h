#pragma once

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>

#include "entity.h"
#include "entity_registry.h"
#include "entity_types.h"

namespace apigen {
	class ast_visitor : public clang::RecursiveASTVisitor<ast_visitor> {
	private:
		using _base = clang::RecursiveASTVisitor<ast_visitor>;

		template <typename T> void _check_register_decl(clang::NamedDecl *d) {
			if (d) {
				registry.register_entity<T>(d);
			}
		}
	public:
		explicit ast_visitor(entity_registry &reg) : registry(reg) {
		}

		bool TraverseFunctionDecl(clang::FunctionDecl *d) {
			_check_register_decl<function_entity>(d);
			return _base::TraverseFunctionDecl(d);
		}
		bool TraverseCXXMethodDecl(clang::CXXMethodDecl *d) {
			_check_register_decl<method_entity>(d);
			return _base::TraverseCXXMethodDecl(d);
		}
		bool TraverseFieldDecl(clang::FieldDecl *d) {
			_check_register_decl<field_entity>(d);
			return _base::TraverseFieldDecl(d);
		}
		bool TraverseCXXRecordDecl(clang::CXXRecordDecl *d) {
			_check_register_decl<record_entity>(d);
			return _base::TraverseCXXRecordDecl(d);
		}
		bool TraverseEnumDecl(clang::EnumDecl *d) {
			_check_register_decl<enum_entity>(d);
			return _base::TraverseEnumDecl(d);
		}

		entity_registry &registry;
	};

	class ast_consumer : public clang::ASTConsumer {
	public:
		ast_consumer(ast_visitor &vis) : clang::ASTConsumer(), visitor(vis) {
		}

		bool HandleTopLevelDecl(clang::DeclGroupRef decl) override {
			for (clang::Decl *d : decl) {
				visitor.TraverseDecl(d);
			}
			return true;
		}

		ast_visitor &visitor;
	};

	void initialize_compiler_instance(clang::CompilerInstance &compiler, int argc, char **argv, bool add_active_def) {
		compiler.createDiagnostics();
		std::vector<const char*> args(argv, argv + argc);
		args[0] = "clang"; // as per libclang
		compiler.setInvocation(clang::createInvocationFromCommandLine(args));
		if (add_active_def) {
			compiler.getInvocation().getPreprocessorOpts().addMacroDef("APIGEN_ACTIVE");
		}
		compiler.setTarget(clang::TargetInfo::CreateTargetInfo(
			compiler.getDiagnostics(), compiler.getInvocation().TargetOpts
		));

		compiler.createFileManager();
		compiler.createSourceManager(compiler.getFileManager());
		compiler.createPreprocessor(clang::TU_Complete);
		compiler.getPreprocessor().getBuiltinInfo().initializeBuiltins(
			compiler.getPreprocessor().getIdentifierTable(), *compiler.getInvocation().getLangOpts()
		); // this is necessary for some reason

		compiler.createASTContext();
	}
}
