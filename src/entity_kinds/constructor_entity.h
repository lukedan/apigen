#pragma once

/// \file
/// Contains the \ref apigen::entities::constructor_entity class.

#include <clang/AST/DeclCXX.h>

#include "../entity.h"
#include "method_entity.h"

namespace apigen::entities {
	/// An entity that represents a constructor.
	class constructor_entity : public method_entity {
	public:
		constexpr static entity_kind kind = entity_kind::constructor; ///< The kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes this entity with the corresponding \p clang::CXXConstructorDecl.
		explicit constructor_entity(clang::CXXConstructorDecl *decl) : method_entity(decl) {
		}

		/// The API return type is the type of the created object.
		void _collect_api_return_type(entity_registry &reg) override {
			auto *decl = llvm::cast<clang::CXXConstructorDecl>(get_declaration());
			_api_return_type.emplace(qualified_type::from_clang_type_pointer(
				decl->getThisType().getCanonicalType().getTypePtr(), reg
			));
		}
	};
}
