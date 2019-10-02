#include "record_entity.h"

/// \file
/// Implementation of certain methods of \ref apigen::entities::record_entity.

#include "../dependency_analyzer.h"
#include "../entity_registry.h"

namespace apigen::entities {
	void record_entity::gather_dependencies(entity_registry &reg, dependency_analyzer &queue) {
		if (_recursive) {
			if (auto *def_decl = llvm::cast<clang::CXXRecordDecl>(_decl->getDefinition())) {
				for (clang::Decl *decl : def_decl->decls()) {
					if (auto *named_decl = llvm::dyn_cast<clang::NamedDecl>(decl)) {
						if (entity *ent = reg.find_entity(named_decl)) {
							queue.try_queue(*ent);
						}
					}
				}
			}
		}
	}
}
