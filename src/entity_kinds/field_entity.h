#pragma once

/// \file
/// An entity that represents a field.

#include <clang/AST/Decl.h>

#include "record_entity.h"
#include "../entity.h"
#include "../dependency_analyzer.h"
#include "../types.h"

namespace apigen::entities {
	/// The special property of a field.
	enum class field_kind : unsigned char {
		normal_field, ///< A normal field.
		reference_field, ///< This field is a reference.
		const_field, ///< This field is not a reference, but is marked const.
		mutable_field ///< This field is marked mutable.
	};

	/// An entity that represents a field.
	class field_entity : public entity {
	public:
		constexpr static entity_kind kind = entity_kind::field; ///< The kind of this entity.
		/// Returns the kind of this entilty.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit field_entity(clang::FieldDecl *decl) : entity(), _decl(decl) {
		}

		/// Also exports the type of this declaration and the parent type.
		void gather_dependencies(entity_registry&, dependency_analyzer&) override;

		/// Returns the type of this field.
		[[nodiscard]] const qualified_type &get_type() const {
			return _type;
		}
		/// Returns the parent type.
		[[nodiscard]] record_entity *get_parent() const {
			return _parent;
		}

		/// Returns the special property of this field.
		[[nodiscard]] field_kind get_field_kind() const {
			return _field_kind;
		}

		/// Returns the declaration of this entity.
		[[nodiscard]] clang::FieldDecl *get_declaration() const {
			return _decl;
		}
		/// Returns the declaration associated with this entity.
		[[nodiscard]] clang::NamedDecl *get_generic_declaration() const override {
			return _decl;
		}

		/// Returns the user-defined name used when exporting.
		[[nodiscard]] const std::string &get_substitute_name() const {
			return _export_name;
		}
	protected:
		qualified_type _type; ///< The type of this field.
		std::string _export_name; ///< The actual name used when exporting.
		field_kind _field_kind = field_kind::normal_field; ///< The special property of this field.
		record_entity *_parent = nullptr; ///< The parent type.
		clang::FieldDecl *_decl = nullptr; ///< The declaration associated with this entity.
	};
}
