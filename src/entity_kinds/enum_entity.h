#pragma once

/// \file
/// An entity that represents an enum.

#include "user_type_entity.h"

namespace apigen::entities {
	/// An entity that represents an enum.
	class enum_entity : public user_type_entity {
	public:
		constexpr static entity_kind kind = entity_kind::enumeration; ///< The kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes this entity based on the given \p clang::EnumDecl.
		explicit enum_entity(clang::EnumDecl *decl) : user_type_entity(), _decl(decl) {
		}

		/// Returns the type used to store enumerators.
		[[nodiscard]] const clang::Type *get_enumerator_type() const {
			auto *decl = llvm::cast<clang::EnumDecl>(_decl->getDefinition());
			return decl->getIntegerType().getCanonicalType().getTypePtr();
		}

		/// An enum entity has no dependencies.
		void gather_dependencies(entity_registry&, dependency_analyzer&) override {
			// nothing to do
		}

		/// Returns the underlying integer type.
		[[nodiscard]] const clang::Type *get_integer_type() const {
			return _decl->getIntegerType().getTypePtr();
		}

		/// Returns \ref _decl.
		[[nodiscard]] clang::EnumDecl *get_declaration() const {
			return _decl;
		}
		/// Returns \ref _decl.
		[[nodiscard]] clang::NamedDecl *get_generic_declaration() const override {
			return _decl;
		}
	protected:
		clang::EnumDecl *_decl = nullptr; ///< The declaration.
	};
}
