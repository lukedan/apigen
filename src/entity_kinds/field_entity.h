#pragma once

/// \file
/// An entity that represents a field.

#include <clang/AST/Decl.h>

#include "../entity.h"
#include "../dependency_analyzer.h"
#include "../types.h"

namespace apigen::entities {
	/// An entity that represents a field.
	class field_entity : public entity {
	public:
		constexpr static entity_kind kind = entity_kind::field; ///< The kind of this entity.
		/// Returns the kind of this entilty.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit field_entity(clang::FieldDecl *decl) : _decl(decl) {
		}

		/// Also exports the type of this declaration.
		void gather_dependencies(entity_registry &reg, dependency_analyzer &queue) override {
			_type = qualified_type::from_clang_type(_decl->getType(), reg);
			if (_type.type_entity) {
				queue.try_queue(*_type.type_entity);
			}
		}

		/// Returns the declaration associated with this entity.
		[[nodiscard]] clang::NamedDecl *get_declaration() const override {
			return _decl;
		}
		/// Do not export if this is a field in a templated context.
		[[nodiscard]] bool should_trim() const override {
			return _decl->getParent()->isDependentContext();
		}
	protected:
		qualified_type _type; ///< The type of this field.
		clang::FieldDecl *_decl = nullptr; ///< The declaration associated with this entity.
	};
}
