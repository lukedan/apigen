#include "entity_registry.h"

#include "entity_types.h"

using clang::Type;
using clang::CXXRecordDecl;
using clang::ClassTemplateDecl;
using clang::ClassTemplateSpecializationDecl;
using clang::ClassTemplatePartialSpecializationDecl;
using clang::TemplateSpecializationKind;

namespace apigen {
	template_specialization_entity *entity_registry::register_template_specialization_entity(
		ClassTemplateSpecializationDecl *decl
	) {
		decl = llvm::cast<ClassTemplateSpecializationDecl>(decl->getCanonicalDecl());
		if (decl->getSpecializationKind() == TemplateSpecializationKind::TSK_Undeclared) { // cannot find pattern
			return nullptr;
		}
		auto [tempit, inserted] = entities.try_emplace(decl);
		if (inserted) {
			auto uniqueent = std::make_unique<template_specialization_entity>(decl);
			template_specialization_entity &ent = *uniqueent;
			tempit->second = std::move(uniqueent);
			// find instantiation pattern & copy flags
			CXXRecordDecl *patt = nullptr;
			if (decl->getSpecializationKind() == TemplateSpecializationKind::TSK_ExplicitSpecialization) {
				patt = decl;
			} else {
				auto pattpair = decl->getInstantiatedFrom();
				if (pattpair.is<ClassTemplateDecl*>()) {
					patt = pattpair.get<ClassTemplateDecl*>()->getTemplatedDecl();
				} else {
					patt = pattpair.get<ClassTemplatePartialSpecializationDecl*>();
				}
			}
			ent.register_all_declarations(patt);
		}
		return cast<template_specialization_entity>(tempit->second.get());
	}

	void entity_registry::register_type_alias_decl(clang::TypedefNameDecl *decl) {
		if (auto *usingdecl = llvm::dyn_cast<clang::TypeAliasDecl>(decl); usingdecl) {
			if (usingdecl->getDescribedAliasTemplate()) { // only consider non-template aliases
				return;
			}
		}
		clang::QualType qty = decl->getUnderlyingType().getCanonicalType();
		if (qty.hasQualifiers() || !(qty->isRecordType() || qty->isEnumeralType())) {
			// only `pure' and user-defined type
			return;
		}
		deferred_typedefs.emplace_back(decl);
	}

	void entity_registry::prepare_for_export(const naming_conventions &conv) {
		// process typedef-like decls
		for (clang::TypedefNameDecl *decl : deferred_typedefs) {
			user_type_entity *ent = nullptr;
			clang::QualType qty = decl->getUnderlyingType().getCanonicalType();
			if (qty->isRecordType()) { // check & register template specialization
				auto *temp = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(qty->getAsCXXRecordDecl());
				if (temp) {
					ent = register_template_specialization_entity(temp);
					if (ent == nullptr) { // not instantiated, skip
						std::cerr << qty.getAsString() << ": template not instantiated\n";
						continue;
					}
				}
			}
			if (ent == nullptr) {
				ent = cast<user_type_entity>(find_entity(qty->getAsTagDecl()));
			}
			for (clang::Attr *attr : decl->attrs()) {
				if (attr->getKind() == clang::attr::Kind::Annotate) {
					auto *annotate = clang::cast<clang::AnnotateAttr>(attr);
					if (!ent->consume_annotation(annotate->getAnnotation())) {
						if (annotate->getAnnotation() == APIGEN_ANNOTATION_ADOPT_NAME) { // adopt this alias
							if (ent->set_substitute_name(decl->getName().str())) {
								std::cerr <<
									ent->declaration->getName().str() <<
									": alias name adopted, previous name discarded\n";
							}
						} else {
							std::cerr <<
								ent->declaration->getName().str() <<
								": unknown annotation " << annotate->getAnnotation().str() << "\n";
						}
					}
				}
			}
		}

		// propagate export flag
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

		// cache names
		for (entity *ent : exported_entities) {
			ent->cache_export_names(*this, conv);
		}
	}


	user_type_entity *export_propagation_queue::queue_dependent_type(const Type *ty) {
		if (ty->isRecordType()) {
			auto *recdecl = ty->getAsCXXRecordDecl();
			if (auto *tempdecl = llvm::dyn_cast<ClassTemplateSpecializationDecl>(recdecl); tempdecl) {
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
