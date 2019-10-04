#include "record_entity.h"

/// \file
/// Implementation of certain methods of \ref apigen::entities::record_entity.

#include "../dependency_analyzer.h"
#include "../entity_registry.h"

namespace apigen::entities {
	void record_entity::gather_dependencies(entity_registry &reg, dependency_analyzer &queue) {
		auto *def_decl = _decl->getDefinition();
		if (def_decl == nullptr) {
			return;
		}
		for (clang::Decl *decl : def_decl->decls()) {
			if (auto *named_decl = llvm::dyn_cast<clang::NamedDecl>(decl)) {
				if (auto *method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(named_decl)) {
					clang::FunctionDecl::TemplatedKind tk = method_decl->getTemplatedKind();
					if (
						tk == clang::FunctionDecl::TK_FunctionTemplate ||
						tk == clang::FunctionDecl::TK_DependentFunctionTemplateSpecialization
					) { // template, do not export
						continue;
					}
				} else if (auto *record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(named_decl)) {
					if (record_decl->getDescribedClassTemplate()) { // template, do not export
						continue;
					}
				}
				if (entity *ent = reg.find_or_register_parsed_entity(named_decl)) {
					if (_recursive) {
						queue.try_queue(*ent);
					}
				}
			}
		}
	}
}
