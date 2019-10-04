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
#include "entity_kinds/enum_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/function_entity.h"
#include "entity_kinds/method_entity.h"
#include "entity_kinds/record_entity.h"

namespace apigen {
	/// A class that collects information about entities and analyzes their dependencies.
	class entity_registry {
	public:
		/// Registers the given clang declaration, during the parsing process.
		template <typename Decl> entity *register_parsing_declaration(Decl *current_decl) {
			auto [found, created] = _find_or_create_parsing_entity_static(current_decl);
			if (found != _decl_mapping.end()) {
				found->second->handle_declaration(current_decl);
				return found->second.get();
			}
			return nullptr;
		}

		/// Returns the entity that correspond to the given \p clang::NamedDecl. This function should only be called to
		/// find an entity that some other exported entity depends on. A new entity is registered and queued if
		/// necessary if a corresponding entity is not found.
		entity *find_or_register_parsed_entity(clang::NamedDecl *decl) {
			decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
			auto it = _decl_mapping.lower_bound(decl);
			if (it != _decl_mapping.end() && it->first == decl) {
				return it->second.get();
			}
			// not found, register this entity
			auto [iter, created] = _find_or_create_entity_dynamic(decl);
			if (iter != _decl_mapping.end()) {
				assert_true(created);
				for (auto *redecl : decl->redecls()) {
					iter->second->handle_declaration(llvm::cast<clang::NamedDecl>(redecl));
				}
				if (analyzer && iter->second->is_marked_for_exporting()) {
					analyzer->queue(*iter->second);
				}
				return iter->second.get();
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
		/// a entity associated with it. This function should only be used when first parsing the source code.
		template <typename Decl> std::pair<_decl_mapping_t::iterator, bool> _find_or_create_parsing_entity_static(
			Decl *non_canon_decl
		) {
			Decl *decl = llvm::cast<Decl>(non_canon_decl->getCanonicalDecl());
			bool bad = false;
			// check if this is a dependent decl
			if constexpr (std::is_base_of_v<clang::DeclContext, Decl>) { // covers functions, records, and enums
				bad = bad || decl->isDependentContext();
			} else if constexpr (std::is_same_v<clang::FieldDecl, Decl>) { // fields
				bad = bad || decl->getParent()->isDependentContext();
			}
			if constexpr (std::is_same_v<clang::CXXRecordDecl, Decl>) { // check if this is a local class
				bad = bad || decl->isLocalClass();
			}
			if (bad) { // don't handle this decl
				return {_decl_mapping.end(), false};
			}

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
				} else if constexpr (std::is_same_v<Decl, clang::EnumDecl>) {
					new_entity = std::make_unique<entities::enum_entity>(decl);
				}
				if (new_entity) {
					return {_decl_mapping.emplace_hint(found, decl, std::move(new_entity)), true};
				} else { // do not need to register this entity or handle this declaration
					return {_decl_mapping.end(), false};
				}
			}
			return {found, false};
		}
		/// Dynamic version of \ref _find_or_create_entity_static().
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
				} else if (auto *enum_decl = llvm::dyn_cast<clang::EnumDecl>(decl)) {
					new_entity = std::make_unique<entities::enum_entity>(enum_decl);
				}
				if (new_entity) {
					return {_decl_mapping.emplace_hint(found, decl, std::move(new_entity)), true};
				} else { // do not need to register this entity or handle this declaration
					return {_decl_mapping.end(), false};
				}
			}
			return {found, false};
		}
	};
}
