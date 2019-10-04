#pragma once

/// \file
/// An entity that represents a method.

#include <optional>

#include <clang/AST/DeclCXX.h>

#include "../entity.h"
#include "function_entity.h"

namespace apigen::entities {
	/// An entity that represents a method.
	class method_entity : public function_entity {
	public:
		constexpr static entity_kind kind = entity_kind::method; ///< The kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes this entity given the corresponding \ref clang::CXXMethodDecl.
		explicit method_entity(clang::CXXMethodDecl *decl) : function_entity(decl) {
		}

		/// Returns whether this method is static.
		[[nodiscard]] bool is_static() const {
			return llvm::cast<clang::CXXMethodDecl>(_decl)->isStatic();
		}

		/// Returns a \ref qualified_type representing the type for \p this.
		[[nodiscard]] virtual std::optional<qualified_type> get_this_type(entity_registry &reg) const {
			if (is_static()) {
				return std::nullopt;
			}
			return qualified_type::from_clang_type(llvm::cast<clang::CXXMethodDecl>(_decl)->getThisType(), reg);
		}
	protected:
		/// Prepends a this parameter to the parameter list if necessary.
		void _build_parameter_list(entity_registry &reg) override {
			if (auto this_param = get_this_type(reg)) {
				_parameters.emplace_back(this_param.value(), "this");
			}
			function_entity::_build_parameter_list(reg);
		}
	};
}
