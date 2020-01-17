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
		/// Registers the given \ref custom_function_entity.
		custom_function_entity &register_custom_function(std::unique_ptr<custom_function_entity> entity) {
			return *_custom_funcs.emplace_back(std::move(entity));
		}
		/// Adds the given entry to \ref _custom_host_deps.
		void register_custom_host_dependency(std::string_view dep) {
			_custom_host_deps.emplace(dep);
		}

		/// Returns the list of all entities.
		[[nodiscard]] const std::map<clang::NamedDecl*, std::unique_ptr<entity>> &get_entities() const {
			return _decl_mapping;
		}
		/// Returns all registered custom function entities.
		[[nodiscard]] const std::vector<std::unique_ptr<custom_function_entity>> &get_custom_functions() const {
			return _custom_funcs;
		}
		/// Returns custom host-side dependencies.
		[[nodiscard]] const std::set<std::string> &get_custom_host_dependencies() const {
			return _custom_host_deps;
		}

		dependency_analyzer *analyzer = nullptr; ///< The associated \ref dependency_analyzer.
	protected:
		/// The type of \ref _decl_mapping.
		using _decl_mapping_t = std::map<clang::NamedDecl*, std::unique_ptr<entity>>;

		/// Mapping between \p clang::NamedDecl and entities.
		_decl_mapping_t _decl_mapping;
		/// The list of custom function entities.
		std::vector<std::unique_ptr<custom_function_entity>> _custom_funcs;
		std::set<std::string> _custom_host_deps; ///< Custom host-side dependencies.

		/// Returns the value indicating that entity creation is rejected.
		[[nodiscard]] std::pair<_decl_mapping_t::iterator, bool> _reject_entity_creation() {
			return {_decl_mapping.end(), false};
		}
		/// Tries to find the \ref entity that correspond to the given declaration, and if one is not found, creates
		/// a entity associated with it. This function should only be used when first parsing the source code.
		template <typename Decl> std::pair<_decl_mapping_t::iterator, bool> _find_or_create_parsing_entity_static(
			Decl *non_canon_decl
		) {
			if constexpr (!(
				std::is_base_of_v<clang::FunctionDecl, Decl> ||
				std::is_base_of_v<clang::CXXRecordDecl, Decl> ||
				std::is_base_of_v<clang::FieldDecl, Decl> ||
				std::is_base_of_v<clang::EnumDecl, Decl>
				)) {
				return _reject_entity_creation();
			}
			// it's possible for a clang::TypeAliasDecl to have a clang::TypedefDecl to be its canonical decl
			// (not sure if vice versa), however these types have already been filtered out above
			Decl *decl = llvm::cast<Decl>(non_canon_decl->getCanonicalDecl());
			
			bool bad = false;
			// check if this decl is valid
			bad = bad || decl->isInvalidDecl();
			// check if this is a dependent decl
			if constexpr (std::is_base_of_v<clang::DeclContext, Decl>) { // covers functions, records, and enums
				bad = bad || decl->isDependentContext();
			} else if constexpr (std::is_same_v<clang::FieldDecl, Decl>) { // fields
				bad = bad || decl->getParent()->isDependentContext();
			}
			// check if this is a local class
			if constexpr (std::is_same_v<clang::CXXRecordDecl, Decl>) {
				bad = bad || decl->isLocalClass();
			}
			// check if this function is deleted
			if constexpr (std::is_base_of_v<clang::FunctionDecl, Decl>) {
				bad = bad || decl->isDeleted();
			}
			if (bad) { // don't handle this decl
				return _reject_entity_creation();
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
					return _reject_entity_creation();
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
				if (decl->isInvalidDecl()) { // decl is invalid
					return _reject_entity_creation();
				}
				if (auto *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
					if (func_decl->isDeleted()) { // the function must not be deleted
						return _reject_entity_creation();
					}
					if (auto *method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(func_decl)) {
						// when analyzing dependency, destructor decls can be found. we do not want them since they
						// already come with the records
						if (llvm::isa<clang::CXXDestructorDecl>(method_decl)) {
							return _reject_entity_creation();
						}
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
				} else { // do not need to register this entity
					return _reject_entity_creation();
				}
				return {_decl_mapping.emplace_hint(found, decl, std::move(new_entity)), true};
			}
			return {found, false};
		}
	};
}
