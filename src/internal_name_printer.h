#pragma once

/// \file
/// Definition of \ref apigen::internal_name_printer.

#include <string_view>
#include <sstream>
#include <stack>

#include <clang/AST/DeclCXX.h>
#include <clang/AST/DeclTemplate.h>

#include "types.h"

namespace apigen {
	/// Used to obtain internal spellings of entities and types.
	class internal_name_printer {
	public:
		/// Initializes \ref policy.
		explicit internal_name_printer(clang::PrintingPolicy p) : policy(std::move(p)) {
		}

		/// Returns the C++ spelling of an operator.
		[[nodiscard]] static std::string_view get_internal_operator_spelling(clang::OverloadedOperatorKind);
		/// Returns the name of a function, without any scope information.
		[[nodiscard]] static std::string_view get_internal_function_name(clang::FunctionDecl*);
		/// Returns the internal name of a function or a type.
		[[nodiscard]] std::string get_internal_entity_name(clang::DeclContext*) const;
		/// Handles \p clang::BuiltinType for \ref get_internal_entity_name(). To add qualifiers to this name, use
		/// \ref get_internal_qualified_type_name() to avoid problems with function types.
		[[nodiscard]] std::string get_internal_type_name(const clang::Type*) const;
		/// Returns the internal spelling of the given \ref qualified_type.
		[[nodiscard]] std::string get_internal_qualified_type_name(const qualified_type &qty) const {
			return get_internal_qualified_type_name(
				qty.type, qty.ref_kind, {}, qty.qualifiers.data(), qty.qualifiers.size()
			);
		}
		/// \overload
		///
		/// \param type The \p clang::Type.
		/// \param ref_kind Indicates what kind of reference this type is.
		/// \param extra_quals Extra qualifiers appended to the outer layers of this type (extra levels of pointers).
		/// \param quals Original qualifiers.
		/// \param qual_count The number of qualifiers in \p quals.
		[[nodiscard]] std::string get_internal_qualified_type_name(
			const clang::Type *type, reference_kind ref_kind,
			std::initializer_list<qualifier> extra_quals = {},
			const qualifier *quals = nullptr, std::size_t qual_count = 0
		) const;

		clang::PrintingPolicy policy; ///< The printing policy for builtin types.
	private:
		/// Returns the spelling of the given \p clang::TemplateArgument.
		[[nodiscard]] std::string _get_template_argument_spelling(const clang::TemplateArgument&) const;
		/// Returns the spelling of a whole template argument list, excluding angle brackets.
		[[nodiscard]] std::string _get_template_argument_list_spelling(
			llvm::ArrayRef<clang::TemplateArgument>
		) const;

		/// Writes to the given stream the part of the return type that's before the name part of the function type
		/// definition.
		void _begin_return_type(
			std::ostream &out, std::stack<const clang::FunctionProtoType*> &def,
			const clang::Type *type, reference_kind ref_kind, const qualifier *quals, std::size_t qual_count
		) const {
			// functions and arrays cannot be returned
			if (auto *functy = llvm::dyn_cast<clang::FunctionProtoType>(type)) { // nested function pointer types
				def.emplace(functy);
				auto retty = qualified_type::from_clang_type(functy->getReturnType(), nullptr);
				_begin_return_type(
					out, def, retty.type, retty.ref_kind, retty.qualifiers.data(), retty.qualifiers.size()
				);
				out << "(";
				_write_qualifiers_and_pointers(out, ref_kind, {}, quals, qual_count);
			} else {
				out << get_internal_type_name(type);
				_write_qualifiers_and_pointers(out, ref_kind, {}, quals, qual_count);
			}
		}
		/// Writes to the given stream the part of the return type that's after the name part of the function type
		/// definition.
		void _end_return_type(std::ostream &out, const clang::FunctionProtoType *type) const {
			out << ")(";
			bool first = true;
			for (const clang::QualType &param : type->param_types()) {
				if (first) {
					first = false;
				} else {
					out << ", ";
				}
				out << get_internal_qualified_type_name(qualified_type::from_clang_type(param, nullptr));
			}
			out << ")";
		}

		/// Returns the total number of qualifiers.
		inline static std::size_t _get_qualifier_count(
			std::initializer_list<qualifier> extra_quals, std::size_t count
		) {
			return extra_quals.size() + count;
		}
		/// Returns the qualifier at the given index.
		inline static qualifier _get_qualifier_at(
			std::size_t index,
			std::initializer_list<qualifier> extra_quals, const qualifier *quals, std::size_t count
		) {
			if (index < extra_quals.size()) {
				return *(extra_quals.begin() + index);
			}
			index -= extra_quals.size();
			assert_true(index < count);
			return quals[index];
		}

		/// Writes the given qualifiers, pointers, and references to the given stream.
		inline static void _write_qualifiers_and_pointers(
			std::ostream &out, reference_kind ref, std::initializer_list<qualifier> extra_quals,
			const qualifier *quals, std::size_t count
		) {
			std::size_t total_count = _get_qualifier_count(extra_quals, count);
			assert_true(total_count >= 1, "too few qualifiers");
			out << " ";
			for (std::size_t i = total_count - 1; i > 0; --i) {
				out << _get_qualifier_at(i, extra_quals, quals, count) << "*";
			}
			out << _get_qualifier_at(0, extra_quals, quals, count);
			switch (ref) {
			case reference_kind::reference:
				out << "&";
				break;
			case reference_kind::rvalue_reference:
				out << "&&";
				break;
			}
		}
	};
}
