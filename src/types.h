#pragma once

/// \file
/// Structs used to store information about qualified types.

#include <clang/AST/ASTContext.h>

#include "misc.h"
#include "entity.h"
#include "entity_kinds/user_type_entity.h"

namespace apigen {
	class entity_registry;
	class cpp_writer;

	/// Qualifiers of a type.
	enum class qualifier : unsigned char {
		none = 0, ///< No qualifiers.
		const_qual = 1, ///< The \p const qualifier.
		volatile_qual = 2 ///< The \p volatile qualifier. Appears to be of no use.
	};
	/// Enables bitwise operators for \ref qualifier.
	template <> struct enable_enum_bitwise_operators<qualifier> : std::true_type {
	};
	/// Prints the given \ref qualifier to the output stream. These qualifiers will be separated by spaces and a space
	/// will be appended at the end. However, if there are no qualifiers, nothing will be written.
	inline std::ostream &operator<<(std::ostream &out, qualifier qual) {
		if ((qual & qualifier::const_qual) != qualifier::none) {
			out << "const ";
		}
		if ((qual & qualifier::volatile_qual) != qualifier::none) {
			out << "volatile ";
		}
		return out;
	}


	/// Indicates which kind of reference a type is.
	enum class reference_kind : unsigned char {
		none, ///< Not a reference.
		reference, ///< Normal lvalue references.
		rvalue_reference ///< An rvalue reference.
	};

	/// A qualified type.
	struct qualified_type {
		/// Converts \p clang::Qualifiers to a \ref qualifier enum.
		[[nodiscard]] inline static qualifier convert_qualifiers(clang::Qualifiers quals) {
			qualifier result = qualifier::none;
			if (quals.hasConst()) {
				result |= qualifier::const_qual;
			}
			if (quals.hasVolatile()) {
				result |= qualifier::volatile_qual;
			}
			return result;
		}

		/// Returns \p true if this type is a reference type.
		[[nodiscard]] bool is_reference() const {
			return ref_kind != reference_kind::none;
		}
		/// Returns \p true if this type is a pointer or reference type.
		[[nodiscard]] bool is_reference_or_pointer() const {
			return is_reference() || qualifiers.size() > 1;
		}
		/// Returns \p true if this type is \p void.
		[[nodiscard]] bool is_void() const {
			return !is_reference_or_pointer() && type->isVoidType();
		}
		/// Returns \p true if this type is a record type (i.e., not a pointer, reference, enum, or builtin type).
		[[nodiscard]] bool is_record_type() const {
			return !is_reference_or_pointer() && llvm::isa<clang::RecordType>(type);
		}

		/// Constructs a \ref qualified_type from the given \p clang::QualType.
		[[nodiscard]] static qualified_type from_clang_type(const clang::QualType&, entity_registry*);
		/// Constructs a \ref qualified_type from the given \ref entities::user_type_entity.
		[[nodiscard]] static qualified_type from_clang_type_pointer(const clang::Type*, entity_registry&);

		/// The list of qualifiers for this type. For non-pointer types, this vector should have only one element.
		/// For each pointer level this vector should have one more element indicating that pointer level's
		/// qualifiers. The qualifiers in the front are those of the outer layers.
		std::vector<qualifier> qualifiers;
		reference_kind ref_kind = reference_kind::none; /// Indicates what kind of reference this type is (if any).
		const clang::Type *type = nullptr; ///< The underlying type.
		/// The entity associated with the base type, or \p nullptr if this is a primitive type.
		entities::user_type_entity *type_entity = nullptr;
	};
}
