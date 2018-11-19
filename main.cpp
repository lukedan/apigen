#include <vector>
#include <iostream>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclGroup.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/TargetOptions.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>

#include "apigen_definitions.h"

std::string get_type_name(clang::QualType qty) {
	return qty.getAsString();
}
std::string get_type_name(const clang::Type *ty) {
	return get_type_name(clang::QualType(ty, 0));
}

struct entity {
	explicit entity(clang::NamedDecl *decl) {
		declaration = decl;
	}

	void register_declaration(clang::NamedDecl *decl) {
		for (clang::Attr *attr : decl->getAttrs()) {
			if (attr->getKind() == clang::attr::Kind::Annotate) {
				clang::AnnotateAttr *annotate = clang::cast<clang::AnnotateAttr>(attr);
				llvm::StringRef anno = annotate->getAnnotation();
				if (anno == APIGEN_ANNOTATION_EXPORT) {
					is_exported = true;
				} else if (anno == APIGEN_ANNOTATION_EXCLUDE) {
					is_excluded = true;
				} else if (anno == APIGEN_ANNOTATION_RECURSIVE) {
					if (llvm::dyn_cast<clang::CXXRecordDecl>(declaration) == nullptr) {
						std::cerr <<
							declaration->getName().str() <<
							": only classes and structs can be exported recursively\n";
					} else {
						is_recursive = true;
					}
				} else if (anno == APIGEN_ANNOTATION_BASE) {
					if (llvm::dyn_cast<clang::CXXRecordDecl>(declaration) == nullptr) {
						std::cerr <<
							declaration->getName().str() <<
							": only classes and structs can be bases\n";
					} else {
						is_base = true;
					}
				} else if (anno.startswith(APIGEN_ANNOTATION_RENAME_PREFIX)) {
					if (substitute_name.size() > 0) {
						std::cerr <<
							declaration->getName().str() <<
							": substitute name " << substitute_name << " discarded\n";
					}
					size_t prefixlen = std::strlen(APIGEN_ANNOTATION_RENAME_PREFIX);
					substitute_name = std::string(anno.begin() + prefixlen, anno.end());
				}
				if (is_excluded && is_exported) {
					std::cerr << declaration->getName().str() << ": conflicting flags; exclude flag dropped\n";
					is_excluded = false;
				}
			}
		}
	}

	inline static const clang::Type *strip_type(clang::QualType qty) {
		const clang::Type *ty = qty.getCanonicalType().getTypePtr();
		if (auto *refty = llvm::dyn_cast<clang::ReferenceType>(ty); refty) { // strip reference type
			ty = refty->getPointeeType().getTypePtr();
		}
		while (true) { // strip array and pointer types
			if (auto *ptrty = llvm::dyn_cast<clang::PointerType>(ty); ptrty) {
				ty = ptrty->getPointeeType().getTypePtr();
				continue;
			}
			if (auto *arrty = llvm::dyn_cast<clang::ArrayType>(ty); arrty) {
				ty = arrty->getPointeeType().getTypePtr();
				continue;
			}
			break;
		}
		return ty;
	}
	inline static void process_dependent_type(
		clang::QualType qualty,
		std::vector<clang::CXXRecordDecl*> &decls,
		std::vector<clang::ClassTemplateSpecializationDecl*> &tempdecls
	) {
		const clang::Type *ty = strip_type(qualty);
		if (ty->isRecordType()) {
			auto *decl = ty->getAsCXXRecordDecl();
			if (auto *tempdecl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl); tempdecl) {
				tempdecls.emplace_back(tempdecl);
			} else {
				decls.emplace_back(decl);
			}
		}
	}
	void get_dependent_types(
		std::vector<clang::CXXRecordDecl*> &recs,
		std::vector<clang::ClassTemplateSpecializationDecl*> &temprecs
	) const {
		if (auto *func = llvm::dyn_cast<clang::FunctionDecl>(declaration); func) {
			if (auto *memfunc = llvm::dyn_cast<clang::CXXMethodDecl>(declaration); memfunc) {
				process_dependent_type(memfunc->getThisType(memfunc->getASTContext()), recs, temprecs);
				func = memfunc;
			}
			for (clang::ParmVarDecl *decl : func->parameters()) {
				process_dependent_type(decl->getType(), recs, temprecs);
			}
		}
		if (auto *field = llvm::dyn_cast<clang::FieldDecl>(declaration); field) {
			process_dependent_type(field->getType(), recs, temprecs);
		}
	}
	std::pair<
		std::vector<clang::CXXRecordDecl*>, std::vector<clang::ClassTemplateSpecializationDecl*>
	> get_dependent_types() const {
		std::vector<clang::CXXRecordDecl*> recs;
		std::vector<clang::ClassTemplateSpecializationDecl*> temprecs;
		get_dependent_types(recs, temprecs);
		return {std::move(recs), std::move(temprecs)};
	}

	void copy_flags(const entity &ent) {
		is_exported = ent.is_exported;
		is_excluded = ent.is_excluded;
		is_recursive = ent.is_recursive;
		is_base = ent.is_base;
	}

	clang::NamedDecl *declaration;
	std::string substitute_name;
	bool
		is_exported = false,
		is_excluded = false,
		is_recursive = false,
		is_base = false,
		is_nobase = false,
		is_basetree = false;
};

class entity_registry {
public:
	void register_entity(clang::NamedDecl *decl) {
		auto *canonical = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
		auto [it, inserted] = entities.try_emplace(canonical, canonical);
		it->second.register_declaration(decl);
	}

protected:
	bool _register_enqueue_export(entity &ent, std::deque<clang::NamedDecl*> &exports) {
		if (exported_entities.emplace(ent.declaration).second) {
			if (ent.is_excluded) {
				std::cerr << ent.declaration->getName().str() << ": overriden exclude flag\n";
				ent.is_excluded = false;
			}
			exports.emplace_back(ent.declaration);
			ent.is_exported = true;
			return true;
		}
		return false;
	}
public:
	void propagate_export() {
		std::deque<clang::NamedDecl*> exports;
		for (auto &pair : entities) {
			if (pair.second.is_exported) {
				_register_enqueue_export(pair.second, exports);
			}
		}
		while (!exports.empty()) {
			clang::NamedDecl *decl = exports.front();
			exports.pop_front();
			if (auto it = entities.find(decl); it != entities.end()) {
				/*for (const clang::TemplateArgument &arg : tempdecl->getTemplateArgs().asArray()) {
					if (arg.getKind() == clang::TemplateArgument::ArgKind::Type) {
						process_dependent_type(arg.getAsType(), decls);
					}
				}*/
				auto [recs, temprecs] = it->second.get_dependent_types();
				for (clang::CXXRecordDecl *depdecl : recs) {
					depdecl = depdecl->getCanonicalDecl();
					if (auto ent = entities.find(depdecl); ent != entities.end()) {
						if (_register_enqueue_export(ent->second, exports)) {
							std::cerr <<
								it->second.declaration->getName().str() <<
								": depends on " << depdecl->getName().str() << ", exporting\n";
						}
					} else {
						std::cerr <<
							depdecl->getName().str() <<
							": unindexed but required struct, aborting\n";
						std::abort();
					}
				}
				for (clang::ClassTemplateSpecializationDecl *tempdecl : temprecs) {
					tempdecl = llvm::cast<clang::ClassTemplateSpecializationDecl>(tempdecl->getCanonicalDecl());
					auto [tempit, inserted] = entities.try_emplace(tempdecl, tempdecl);
					if (inserted) {
						std::cerr <<
							it->second.declaration->getName().str() <<
							": depends on template specialization " << tempdecl->getName().str() << "<";
						for (const clang::TemplateArgument &param : tempdecl->getTemplateArgs().asArray()) {
							if (param.getKind() == clang::TemplateArgument::ArgKind::Type) {
								std::cerr << param.getAsType().getAsString();
							}
							std::cerr << ", ";
						}
						std::cerr << ">, registering\n";
						// find instantiation pattern & copy flags
						clang::CXXRecordDecl *patt = nullptr;
						if (tempdecl->getSpecializationKind() == clang::TSK_ExplicitSpecialization) {
							patt = tempdecl;
						} else {
							auto pattpair = tempdecl->getInstantiatedFrom();
							if (pattpair.is<clang::ClassTemplateDecl*>()) {
								patt = pattpair.get<clang::ClassTemplateDecl*>()->getTemplatedDecl();
							} else {
								patt = pattpair.get<clang::ClassTemplatePartialSpecializationDecl*>();
							}
						}
						auto pattit = entities.find(patt);
						if (pattit == entities.end()) {
							std::cerr << tempdecl->getName().str() << ": no template found, aborting\n";
							std::abort();
						}
						tempit->second.copy_flags(pattit->second);
						_register_enqueue_export(tempit->second, exports);
					}
				}
				if (it->second.is_recursive) {
					auto *decl = llvm::cast<clang::CXXRecordDecl>(it->second.declaration);
					if (auto *def = decl->getDefinition(); def) {
						for (clang::FieldDecl *field : def->fields()) {
							field = field->getCanonicalDecl();
							if (auto ent = entities.find(field); ent != entities.end()) {
								if (!ent->second.is_excluded) {
									_register_enqueue_export(ent->second, exports);
								}
							}
						}
						for (clang::CXXMethodDecl *method : def->methods()) {
							method = method->getCanonicalDecl();
							if (auto ent = entities.find(method); ent != entities.end()) {
								if (!ent->second.is_excluded) {
									_register_enqueue_export(ent->second, exports);
								}
							}
						}
						using record_iterator = clang::DeclContext::specific_decl_iterator<clang::CXXRecordDecl>;
						record_iterator recend(def->decls_end());
						for (record_iterator it(def->decls_begin()); it != recend; ++it) {
							clang::CXXRecordDecl *rec = it->getCanonicalDecl();
							if (auto ent = entities.find(rec); ent != entities.end()) {
								if (!ent->second.is_excluded) {
									ent->second.is_recursive = true;
									_register_enqueue_export(ent->second, exports);
								}
							}
						}
					} else {
						std::cerr <<
							decl->getName().str() << ": recursively exported class lacks definition\n";
					}
				}
			}
		}
	}

	std::unordered_map<clang::NamedDecl*, entity> entities;
	std::unordered_set<clang::NamedDecl*> exported_entities;
};

class ast_visitor : public clang::RecursiveASTVisitor<ast_visitor> {
private:
	using _base = clang::RecursiveASTVisitor<ast_visitor>;
public:
	explicit ast_visitor(entity_registry &reg) : registry(reg) {
	}

#define TRAVERSE_DECL_IMPL(TYPE)          \
	bool Traverse##TYPE(clang::TYPE *d) { \
		if (d) {                          \
			registry.register_entity(d);  \
		}                                 \
		return _base::Traverse##TYPE(d);  \
	}                                     \

	TRAVERSE_DECL_IMPL(FunctionDecl);
	TRAVERSE_DECL_IMPL(CXXMethodDecl);
	TRAVERSE_DECL_IMPL(FieldDecl);
	TRAVERSE_DECL_IMPL(CXXRecordDecl);

#undef TRAVERSE_DECL_IMPL

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

int main(int argc, char **argv) {
	clang::CompilerInstance compiler;
	compiler.createDiagnostics();
	std::vector<const char*> args(argv, argv + argc);
	args[0] = "clang"; // as per libclang
	compiler.setInvocation(clang::createInvocationFromCommandLine(args));
	compiler.getInvocation().getPreprocessorOpts().addMacroDef("APIGEN_ACTIVE");
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
	entity_registry reg;
	ast_visitor visitor(reg);
	compiler.setASTConsumer(llvm::make_unique<ast_consumer>(visitor));

	const clang::FileEntry *file = compiler.getFileManager().getFile("../codepad/codepad/main.cpp");
	compiler.getSourceManager().setMainFileID(compiler.getSourceManager().createFileID(
		file, clang::SourceLocation(), clang::SrcMgr::C_User
	));

	compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(), &compiler.getPreprocessor());
	clang::ParseAST(compiler.getPreprocessor(), &compiler.getASTConsumer(), compiler.getASTContext());
	compiler.getDiagnosticClient().EndSourceFile();

	reg.propagate_export();
	std::cout << "\n\n";
	std::cout << "exported entities:\n";
	for (clang::NamedDecl *decl : reg.exported_entities) {
		std::cout << decl->getDeclKindName() << ", " << decl->getName().str() << "\n";
	}

	std::system("pause");

	return 0;
}
