#pragma once

/// \file
/// A class that collects information about entities and analyzes their dependencies.

#include <set>
#include <map>
#include <stack>
#include <memory>
#include <type_traits>

#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

#include "dependency_analyzer.h"
#include "entity.h"
#include "entity_kinds/constructor_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/function_entity.h"
#include "entity_kinds/method_entity.h"
#include "entity_kinds/record_entity.h"

namespace apigen {
	/// A class that collects information about entities and analyzes their dependencies.
	class entity_registry {
	public:
		/// Registers the given clang declaration.
		template <typename Decl> entity *register_declaration(Decl *current_decl) {
			if (auto [found, created] = _find_or_create_entity_static(current_decl); found != _decl_mapping.end()) {
				found->second->handle_declaration(current_decl);
				return found->second.get();
			}
			return nullptr;
		}
		/// Registers the given clang declaration, after parsing has finished. If the new entity is marked for
		/// exporting, this function also tries to queue this entity. This is used mostly for template specialization
		/// entities and their children.
		entity *register_parsed_declaration(clang::NamedDecl *decl) {
			auto [found, created] = _find_or_create_entity_dynamic(decl);
			if (found != _decl_mapping.end() && created) {
				for (auto *redecl : decl->redecls()) {
					found->second->handle_declaration(llvm::cast<clang::NamedDecl>(redecl));
				}
				if (!found->second->should_trim()) {
					if (analyzer && found->second->is_marked_for_exporting()) {
						analyzer->try_queue(*found->second);
					}
					return found->second.get();
				}
				_decl_mapping.erase(found);
			}
			return nullptr;
		}

		/// Trims entities that cannot be exported.
		void trim_entities() {
			for (auto it = _decl_mapping.begin(); it != _decl_mapping.end(); ) {
				if (it->second->should_trim()) {
					it = _decl_mapping.erase(it);
				} else {
					++it;
				}
			}
		}

		/// Returns the entity that correspond to the given \p clang::NamedDecl.
		entity *find_entity(clang::NamedDecl *decl) {
			decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
			auto it = _decl_mapping.lower_bound(decl);
			if (it != _decl_mapping.end() && it->first == decl) {
				return it->second.get();
			}
			// if this is a class template specialization declaration, then register this entity
			if (auto *template_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
				// must not be a partial specialization to be able to export
				if (!llvm::isa<clang::ClassTemplatePartialSpecializationDecl>(template_decl)) {
					return _register_template_specialization_declaration(template_decl, it);
				}
			}
			return nullptr;
		}

		/// Returns the list of all entities.
		[[nodiscard]] const std::map<clang::NamedDecl*, std::unique_ptr<entity>> &get_entities() const {
			return _decl_mapping;
		}

		dependency_analyzer *analyzer = nullptr; ///< The associated \ref dependency_analyzer.
	protected:
		/// The type of \ref _decl_mapping.
		using _decl_mapping_t = std::map<clang::NamedDecl*, std::unique_ptr<entity>>;

		/// Mapping between \p clang::NamedDecl and entities.
		_decl_mapping_t _decl_mapping;

		/// Tries to find the \ref entity that correspond to the given declaration, and if one is not found, creates
		/// a entity associated with it.
		template <typename Decl> std::pair<_decl_mapping_t::iterator, bool> _find_or_create_entity_static(
			Decl *non_canon_decl
		) {
			Decl *decl = llvm::cast<Decl>(non_canon_decl->getCanonicalDecl());
			auto found = _decl_mapping.lower_bound(decl);
			if (found == _decl_mapping.end() || found->first != decl) { // not found
				std::unique_ptr<entity> new_entity;
				if constexpr (std::is_same_v<Decl, clang::FunctionDecl>) {
					new_entity = std::make_unique<entities::function_entity>(decl);
				} else if constexpr (std::is_same_v<Decl, clang::CXXMethodDecl>) {
					new_entity = std::make_unique<entities::method_entity>(decl);
				} else if constexpr (std::is_same_v<Decl, clang::CXXConstructorDecl>) {
					new_entity = std::make_unique<entities::constructor_entity>(decl);
				} else if constexpr (std::is_same_v<Decl, clang::CXXRecordDecl>) {
					new_entity = std::make_unique<entities::record_entity>(decl);
				} else if constexpr (std::is_same_v<Decl, clang::FieldDecl>) {
					new_entity = std::make_unique<entities::field_entity>(decl);
				}
				if (new_entity) {
					return {_decl_mapping.emplace_hint(found, decl, std::move(new_entity)), true};
				} else { // do not need to register this entity or handle this declaration
					return {_decl_mapping.end(), false};
				}
			}
			return {found, false};
		}
		std::pair<_decl_mapping_t::iterator, bool> _find_or_create_entity_dynamic(clang::NamedDecl *non_canon_decl) {
			auto *decl = llvm::cast<clang::NamedDecl>(non_canon_decl->getCanonicalDecl());
			auto found = _decl_mapping.lower_bound(decl);
			if (found == _decl_mapping.end() || found->first != decl) { // not found
				std::unique_ptr<entity> new_entity;
				if (auto *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
					if (auto *method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
						if (auto *constructor_decl = llvm::dyn_cast<clang::CXXConstructorDecl>(method_decl)) {
							new_entity = std::make_unique<entities::constructor_entity>(constructor_decl);
						} else {
							new_entity = std::make_unique<entities::method_entity>(method_decl);
						}
					} else {
						new_entity = std::make_unique<entities::function_entity>(func_decl);
					}
				} else if (auto *record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(decl)) {
					new_entity = std::make_unique<entities::record_entity>(record_decl);
				} else if (auto *field_decl = llvm::dyn_cast<clang::FieldDecl>(decl)) {
					new_entity = std::make_unique<entities::field_entity>(field_decl);
				}
				if (new_entity) {
					return {_decl_mapping.emplace_hint(found, decl, std::move(new_entity)), true};
				} else { // do not need to register this entity or handle this declaration
					return {_decl_mapping.end(), false};
				}
			}
			return {found, false};
		}

		/// Recursively registers all children declarations of the given \p clang::CXXRecordDecl by calling
		/// \ref register_parsed_declarationi.
		void _register_parsed_children_decls(clang::CXXRecordDecl *decl) {
			std::stack<clang::CXXRecordDecl*> stk;
			stk.emplace(decl);
			while (!stk.empty()) {
				clang::CXXRecordDecl *current = stk.top();
				stk.pop();
				for (auto *child_decl : current->decls()) {
					if (auto *named_decl = llvm::dyn_cast<clang::NamedDecl>(child_decl)) {
						if (entity *ent = register_parsed_declaration(named_decl)) {
							if (auto *child_record = llvm::dyn_cast<clang::CXXRecordDecl>(named_decl)) {
								stk.emplace(child_record);
							}
						}
					}
				}
			}
		}
		/// Registers a template specialization declaration after parsing has finished. This function assumes that the
		/// given declaration is the canonical declaration, and does not check if the entity exists.
		entities::record_entity *_register_template_specialization_declaration(
			clang::ClassTemplateSpecializationDecl *decl, _decl_mapping_t::iterator hint
		) {
			auto entity = std::make_unique<entities::record_entity>(decl);
			entities::record_entity *res = entity.get();
			auto it = _decl_mapping.emplace_hint(hint, decl, std::move(entity));
			for (auto *redecl : decl->redecls()) {
				it->second->handle_declaration(redecl);
			}
			if (analyzer && it->second->is_marked_for_exporting()) {
				analyzer->queue(*it->second);
			}
			_register_parsed_children_decls(decl);
			return res;
		}
	};
}