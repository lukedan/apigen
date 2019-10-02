#pragma once

/// \file
/// Generic abstract base class of user-defined types.

#include "../entity.h"

namespace apigen::entities {
	/// Generic abstract base class of user-defined types.
	class user_type_entity : public entity {
	public:
		constexpr static entity_kind kind = entity_kind::user_type; ///< The kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit user_type_entity(clang::TagDecl *decl) : _decl(decl) {
		}

		/// Returns the name of this type in the host.
		[[nodiscard]] virtual std::string get_fully_qualified_name_in_host() const {
			return _decl->getQualifiedNameAsString();
		}

		/// Returns the declaration of this entity.
		[[nodiscard]] clang::NamedDecl *get_declaration() const override {
			return _decl;
		}
	protected:
		clang::TagDecl *_decl = nullptr; ///< The declaration of this entity.
	};
}
