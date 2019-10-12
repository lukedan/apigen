#pragma once

#include <set>

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Parse/ParseAST.h>

#include "misc.h"
#include "entity_registry.h"

namespace apigen {
	/// Parses files and keeps a registry of all entities in the code.
	class parser {
	public:
		/// Initializes this parser from the given \p clang::CompilerInvocation.
		explicit parser(std::unique_ptr<clang::CompilerInvocation> invocation) {
			_compiler.createDiagnostics();
			_compiler.setInvocation(std::move(invocation));
			_compiler.setTarget(clang::TargetInfo::CreateTargetInfo(
				_compiler.getDiagnostics(), _compiler.getInvocation().TargetOpts
			));

			_compiler.createFileManager();
			_compiler.createSourceManager(_compiler.getFileManager());
			_compiler.createPreprocessor(clang::TU_Complete);
			_compiler.getPreprocessor().getBuiltinInfo().initializeBuiltins(
				_compiler.getPreprocessor().getIdentifierTable(), *_compiler.getInvocation().getLangOpts()
			); // this is necessary

			_compiler.createASTContext();

			assert_true(!_compiler.getFrontendOpts().Inputs.empty(), "no input file");
		}

		/// Carries out actual parsing.
		void parse(entity_registry &reg) {
			_ast_visitor visitor(reg);
			_compiler.setASTConsumer(llvm::make_unique<_ast_consumer>(visitor));

			if (_compiler.getFrontendOpts().Inputs.size() > 1) {
				std::cerr << "warning: main file not unique\n";
			}
			const clang::FileEntry *file =
				_compiler.getFileManager().getFile(_compiler.getFrontendOpts().Inputs[0].getFile());
			_compiler.getSourceManager().setMainFileID(_compiler.getSourceManager().createFileID(
				file, clang::SourceLocation(), clang::SrcMgr::C_User
			));

			_compiler.getDiagnosticClient().BeginSourceFile(_compiler.getLangOpts(), &_compiler.getPreprocessor());
			clang::ParseAST(_compiler.getPreprocessor(), &_compiler.getASTConsumer(), _compiler.getASTContext());
			_compiler.getDiagnosticClient().EndSourceFile();
		}

		/// Returns the underlying \p clang::CompilerInstance.
		const clang::CompilerInstance &get_compiler() const {
			return _compiler;
		}
	protected:
		/// Used when parsing files to extract definitions.
		struct _ast_visitor : public clang::RecursiveASTVisitor<_ast_visitor> {
		private:
			using _base = clang::RecursiveASTVisitor<_ast_visitor>; ///< The base class.
		public:
			/// Initializes \ref registry.
			explicit _ast_visitor(entity_registry &reg) : clang::RecursiveASTVisitor<_ast_visitor>(), _registry(reg) {
			}

			/// Handles function declarations.
			bool TraverseFunctionDecl(clang::FunctionDecl *d) {
				_check_register_decl(d);
				return _base::TraverseFunctionDecl(d);
			}
			/// Handles method declarations.
			bool TraverseCXXMethodDecl(clang::CXXMethodDecl *d) {
				_check_register_decl(d);
				return _base::TraverseCXXMethodDecl(d);
			}
			/// Handles constructors.
			bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl *d) {
				_check_register_decl(d);
				return _base::TraverseCXXConstructorDecl(d);
			}
			/// Handles field declarations.
			bool TraverseFieldDecl(clang::FieldDecl *d) {
				_check_register_decl(d);
				return _base::TraverseFieldDecl(d);
			}
			/// Handles record declarations.
			bool TraverseCXXRecordDecl(clang::CXXRecordDecl *d) {
				_check_register_decl(d);
				return _base::TraverseCXXRecordDecl(d);
			}
			/// Handles enum declarations.
			bool TraverseEnumDecl(clang::EnumDecl *d) {
				_check_register_decl(d);
				return _base::TraverseEnumDecl(d);
			}

			/// Handles replacement information in type alias declarations.
			bool TraverseTypeAliasDecl(clang::TypeAliasDecl *d) {
				_check_register_decl(d);
				return _base::TraverseTypeAliasDecl(d);
			}
			/// Handles replacement information in \p typedef declarations.
			bool TraverseTypedefDecl(clang::TypedefDecl *d) {
				_check_register_decl(d);
				return _base::TraverseTypedefDecl(d);
			}
		protected:
			entity_registry &_registry; ///< The associated \ref entity_registry.

			/// Checks if the given declaration is \p nullptr, and if not, calls
			/// \ref entity_registry::register_declaration().
			template <typename Decl> void _check_register_decl(Decl *d) {
				if (d) {
					_registry.register_parsing_declaration(d);
				}
			}
		};
		/// AST consumer that lets the associated \ref _ast_visitor handle all the declarations.
		class _ast_consumer : public clang::ASTConsumer {
		public:
			/// Initializes \ref visitor.
			explicit _ast_consumer(_ast_visitor &vis) : clang::ASTConsumer(), _visitor(vis) {
			}

			/// Calls \ref _ast_visitor::TraverseDecl() for each declaration in the \p clang::DeclGroupRef.
			bool HandleTopLevelDecl(clang::DeclGroupRef decl) override {
				for (clang::Decl *d : decl) {
					_visitor.TraverseDecl(d);
				}
				return true;
			}
		protected:
			_ast_visitor &_visitor; ///< The associated \ref ast_visitor.
		};

		clang::CompilerInstance _compiler; ///< The \p clang::CompilerInstance used to parse code.
	};
}
