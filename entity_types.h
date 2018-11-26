#pragma once

#include <variant>

#include "entity.h"
#include "entity_registry.h"

namespace apigen {
	class method_entity;

	class enum_entity : public entity {
	public:
		enum_entity(clang::NamedDecl *decl) : entity(decl) {
		}
	};

	class record_entity : public entity {
	public:
		record_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		bool consume_annotation(llvm::StringRef anno) override {
			if (anno == APIGEN_ANNOTATION_RECURSIVE) {
				is_recursive = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_BASE) {
				is_base = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_NOBASE) {
				is_nobase = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_BASETREE) {
				is_basetree = true;
				return true;
			}
			return entity::consume_annotation(anno);
		}

		void propagate_export(entity_registry &reg) override {
			if (is_recursive) { // recursively export subclasses, methods, and fields
				auto *decl = llvm::cast<clang::CXXRecordDecl>(declaration);
				if (auto *def = decl->getDefinition(); def) {
					for (clang::FieldDecl *field : def->fields()) {
						if (entity *ent = reg.find_entity(field); ent) {
							// these are naturally overriden in entity_registry::propagate_export()
							// if it is explicitly exported
							if (field->getAccess() == clang::AS_public && !ent->is_excluded) {
								reg.queue_exported_entity(*ent);
							}
						}
					}
					for (clang::CXXMethodDecl *method : def->methods()) {
						if (entity *ent = reg.find_entity(method); ent) {
							if (method->getAccess() == clang::AS_public && !ent->is_excluded) {
								reg.queue_exported_entity(*ent);
							}
						}
					}
					using record_iterator = clang::DeclContext::specific_decl_iterator<clang::CXXRecordDecl>;
					record_iterator recend(def->decls_end());
					for (record_iterator it(def->decls_begin()); it != recend; ++it) {
						if (auto *ent = static_cast<record_entity*>(reg.find_entity(*it)); ent) {
							if (it->getAccess() == clang::AS_public && !ent->is_excluded) {
								ent->is_recursive = true;
								reg.queue_exported_entity(*ent);
							}
						}
					}
				} else {
					std::cerr <<
						decl->getName().str() << ": recursively exported class lacks definition\n";
				}
			}
		}

		std::vector<method_entity*> virtual_methods;
		bool
			is_recursive = false,
			is_base = false,
			is_nobase = false,
			is_basetree = false;
	};
	class template_specialization_entity : public record_entity {
	public:
		template_specialization_entity(clang::NamedDecl *decl) : record_entity(decl) {
		}

		void copy_export_options(const record_entity &entity) {
			substitute_name = entity.substitute_name;

			is_exported = entity.is_exported;
			is_excluded = entity.is_excluded;

			is_recursive = entity.is_recursive;
			is_base = entity.is_base;
			is_nobase = entity.is_nobase;
			is_basetree = entity.is_basetree;
		}
	};

	class field_entity : public entity {
	public:
		field_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		void propagate_export(entity_registry &reg) override {
			auto *field = llvm::cast<clang::FieldDecl>(declaration);
			parent = static_cast<record_entity*>(reg.find_entity(field->getParent()));
			reg.queue_exported_entity(*parent);
			type = reg.queue_dependent_type(field->getType());
		}

		record_entity *parent = nullptr;
		user_type_info type;
	};

	class function_entity : public entity {
	public:
		function_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		void propagate_export(entity_registry &reg) override {
			auto *func = llvm::cast<clang::FunctionDecl>(declaration);
			for (clang::ParmVarDecl *decl : func->parameters()) {
				arg_types.emplace_back(reg.queue_dependent_type(decl->getType()));
			}
			return_type = reg.queue_dependent_type(func->getReturnType());
		}

		user_type_info return_type;
		std::vector<user_type_info> arg_types;
	};
	class method_entity : public function_entity {
	public:
		method_entity(clang::NamedDecl *decl) : function_entity(decl) {
		}

		void propagate_export(entity_registry &reg) override {
			auto *memfunc = llvm::cast<clang::CXXMethodDecl>(declaration);
			parent = static_cast<record_entity*>(reg.find_entity(memfunc->getParent()));
			reg.queue_exported_entity(*parent);
			function_entity::propagate_export(reg);
		}

		record_entity *parent = nullptr;
	};


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
			entity *pattent = find_entity(patt);
			if (pattent == nullptr) {
				std::cerr << decl->getName().str() << ": no template found, aborting\n";
				std::abort();
			}
			ent.copy_export_options(static_cast<record_entity&>(*pattent));
			return true;
		}
		return false;
	}

	user_type_info entity_registry::queue_dependent_type(clang::QualType qty) {
		const clang::Type *ty = entity::strip_type(qty);
		if (ty->isRecordType()) {
			auto *recdecl = ty->getAsCXXRecordDecl();
			if (auto *tempdecl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(recdecl); tempdecl) {
				register_template_specialization_entity(tempdecl);
			}
			if (entity *ent = find_entity(recdecl); ent) {
				_queue_export(*ent);
				return user_type_info(std::in_place_type<record_entity*>, static_cast<record_entity*>(ent));
			}
		} else if (ty->isEnumeralType()) {
			if (entity *ent = find_entity(ty->getAsTagDecl()); ent) {
				_queue_export(*ent);
				return user_type_info(std::in_place_type<enum_entity*>, static_cast<enum_entity*>(ent));
			}
		} else {
			return user_type_info();
		}
		std::cerr <<
			ty->getAsTagDecl()->getName().str() <<
			": unindexed but required type, aborting\n";
		std::abort();
		return user_type_info();
	}
}
