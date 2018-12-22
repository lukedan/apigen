#include "entity_registry.h"

#include "entity_types.h"

namespace apigen {
	bool entity_registry::register_template_specialization_entity(clang::ClassTemplateSpecializationDecl *decl) {
		decl = llvm::cast<clang::ClassTemplateSpecializationDecl>(decl->getCanonicalDecl());
		auto [tempit, inserted] = entities.try_emplace(decl);
		if (inserted) {
			auto uniqueent = std::make_unique<template_specialization_entity>(decl);
			template_specialization_entity &ent = *uniqueent;
			tempit->second = std::move(uniqueent);
			// find instantiation pattern & copy flags
			clang::CXXRecordDecl *patt = nullptr;
			if (decl->getSpecializationKind() == clang::TSK_ExplicitSpecialization) {
				patt = decl;
			} else {
				auto pattpair = decl->getInstantiatedFrom();
				if (pattpair.is<clang::ClassTemplateDecl*>()) {
					patt = pattpair.get<clang::ClassTemplateDecl*>()->getTemplatedDecl();
				} else {
					patt = pattpair.get<clang::ClassTemplatePartialSpecializationDecl*>();
				}
			}
			ent.register_all_declarations(patt);
			return true;
		}
		return false;
	}

	void entity_registry::prepare_for_export(const naming_conventions &conv) {
		export_propagation_queue q(*this);
		for (auto &pair : entities) {
			if (auto *record = dyn_cast<record_entity>(pair.second.get()); record) {
				record->gather_dependencies(*this);
			}
			if (pair.second->is_exported) {
				q.queue_exported_entity(*pair.second);
			}
		}
		while (!q.empty()) {
			q.dequeue()->propagate_export(q, *this);
		}
		for (entity *ent : exported_entities) {
			ent->cache_export_names(*this, conv);
		}
	}


	user_type_entity *export_propagation_queue::queue_dependent_type(const clang::Type *ty) {
		if (ty->isRecordType()) {
			auto *recdecl = ty->getAsCXXRecordDecl();
			if (auto *tempdecl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(recdecl); tempdecl) {
				_reg.register_template_specialization_entity(tempdecl);
			}
			if (entity *ent = _reg.find_entity(recdecl); ent) {
				_queue_export(*ent);
				return cast<user_type_entity>(ent);
			}
		} else if (ty->isEnumeralType()) {
			if (entity *ent = _reg.find_entity(ty->getAsTagDecl()); ent) {
				_queue_export(*ent);
				return cast<user_type_entity>(ent);
			}
		} else {
			return nullptr;
		}
		std::cerr <<
			ty->getAsTagDecl()->getName().str() <<
			": unindexed but required type, aborting\n";
		std::abort();
		return nullptr;
	}
}
