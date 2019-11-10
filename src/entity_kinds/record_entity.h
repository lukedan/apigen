#pragma once

/// \file
/// Contains the \ref apigen::entities::record_entity class.

#include <clang/AST/DeclCXX.h>

#include "../entity.h"
#include "user_type_entity.h"

namespace apigen::entities {
	/// An entity that corresponds to a \p class or a \p struct.
	class record_entity : public user_type_entity {
	public:
		constexpr static entity_kind kind = entity_kind::record; ///< The type of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit record_entity(clang::CXXRecordDecl *decl) : user_type_entity(), _decl(decl) {
		}

		/// Gathers all dependencies for this record type.
		void gather_dependencies(entity_registry&, dependency_analyzer&) override;

		/// Handles the \p apigen_recursive attribute.
		bool handle_attribute(std::string_view anno) override {
			if (anno == APIGEN_ANNOTATION_PRIVATE_EXPORT) {
				_private_export = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_RECURSIVE) {
				_recursive = true;
				return true;
			}
			return entity::handle_attribute(anno);
		}

		/// Returns whether or not private members of this class can be exported.
		[[nodiscard]] bool export_private_members() const {
			// TODO private members that are explicitly marked as export will still be exported
			return _private_export;
		}
		/// Returns whether or not this class has a viable move constructor.
		[[nodiscard]] bool has_move_constructor() const {
			return _move_constructor;
		}

		/// Returns the declaration of this entity.
		[[nodiscard]] clang::CXXRecordDecl *get_declaration() const {
			return _decl;
		}
		/// Returns \ref _decl.
		[[nodiscard]] clang::NamedDecl *get_generic_declaration() const override {
			return _decl;
		}

		/// Checks if the given constructor is a move constructor.
		[[nodiscard]] static bool is_move_constructor(clang::CXXConstructorDecl*);
	protected:
		clang::CXXRecordDecl *_decl = nullptr; ///< The declaration of this entity.
		bool
			_move_constructor = false, ///< Indicates whether or not this class has a viable move constructor.
			_recursive = false, ///< Indicates whether members of this record should be exported.
			_private_export = false; ///< Whether or not to export private members.
	};
}
