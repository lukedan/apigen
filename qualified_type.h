#pragma once

#include "entity.h"
#include "entity_registry.h"

namespace apigen {
	struct qualified_type;

	/// Used to export types.
	class type_exporter {
	public:
		/// Default destructor.
		virtual ~type_exporter() = default;

		/// Returns the spelling of a \ref qualified_type when used as a function parameter.
		virtual std::string get_parameter_spelling(const qualified_type&) const = 0;
		/// Returns the spelling of a \ref qualified_type when used as the return type.
		virtual std::string get_return_type_spelling(const qualified_type&) const = 0;
		/// Returns the spelling of a \ref qualified_type when used as the return type for a field.
		virtual std::string get_field_return_type_spelling(const qualified_type&) const = 0;

		/// Exports code used to convert parameters passed in to corresponding internal types.
		virtual void export_prepare_pass_argument(
			export_writer&, const qualified_type&, std::string_view param, std::string_view tmp
		) const;
		/// Exports code used to pass an argument to the internal function.
		virtual void export_pass_argument(
			export_writer&, const qualified_type&, std::string_view param, std::string_view tmp
		) const = 0;
		/// Exports code used to prepare values for returning or returning directly. This code is placed before the
		/// function call.
		virtual void export_prepare_return(export_writer&, const qualified_type&, std::string_view) const = 0;
		/// Exports code used to convert and return the result of the function call. This code is placed after the
		/// function call.
		virtual void export_return(export_writer&, const qualified_type&, std::string_view) const = 0;

		/// Returns a global \ref type_exporter suitable for the given \p clang::QualType.
		static const type_exporter *get_matching_exporter(clang::QualType);
	};

	/// Stores information about the type of a parameter, a return value, or a field.
	struct qualified_type {
		/// Default constructor.
		qualified_type() = default;
		/// Initializes this struct with the given \p clang::QualType.
		explicit qualified_type(clang::QualType qty) : type(qty.getCanonicalType()) {
			std::vector<type_property> props;
			base_type = entity::strip_type(type, &props);
			postfix = entity::get_type_traits(props);
			exporter = type_exporter::get_matching_exporter(qty);
		}

		/// Tries to find the \ref user_type_entity that corresponds to this type.
		void find_entity(const entity_registry &reg) {
			if (base_type->isEnumeralType() || base_type->isRecordType()) {
				entity = cast<user_type_entity>(reg.find_entity(base_type->getAsTagDecl()));
			}
		}

		/// Stores the postfix for this type. This includes pointers (arrays) and qualifiers but not references.
		std::string postfix;
		clang::QualType type; ///< The type.
		user_type_entity *entity = nullptr; ///< The entity associated with this type.
		const clang::Type *base_type = nullptr; ///< The type of \ref entity.
		const type_exporter *exporter = nullptr; ///< The \ref type_exporter for this qualified type.

		/// Returns the name of the underlying type in exported code. For enum types, this returns the corresponding
		/// integral type since enums cannot have custom corresponding integral types in C.
		std::string get_exported_base_type_name() const;
		/// Returns the name of the underlying type used in host code.
		std::string get_internal_base_type_name() const {
			return clang::QualType(base_type, 0).getAsString(get_cpp_printing_policy());
		}
		/// Returns the spelling of this type used in host code.
		std::string get_internal_type_spelling() const {
			return type.getAsString(get_cpp_printing_policy());
		}

		/// Shorthand for \ref type_exporter::get_parameter_spelling().
		std::string get_parameter_spelling() const {
			return exporter->get_parameter_spelling(*this);
		}
		/// Shorthand for \ref type_exporter::get_return_type_spelling().
		std::string get_return_type_spelling() const {
			return exporter->get_return_type_spelling(*this);
		}
		/// Shorthand for \ref type_exporter::get_field_return_type_spelling().
		std::string get_field_return_type_spelling() const {
			return exporter->get_field_return_type_spelling(*this);
		}

		/// Shorthand for \ref type_exporter::export_prepare_pass_argument().
		void export_prepare_pass_argument(export_writer &out, std::string_view param, std::string_view tmp) const {
			exporter->export_prepare_pass_argument(out, *this, param, tmp);
		}
		/// Shorthand for \ref type_exporter::export_pass_argument().
		void export_pass_argument(export_writer &out, std::string_view param, std::string_view tmp) const {
			exporter->export_pass_argument(out, *this, param, tmp);
		}
		/// Shorthand for \ref type_exporter::export_prepare_return().
		void export_prepare_return(export_writer &out, std::string_view tmpname) const {
			exporter->export_prepare_return(out, *this, tmpname);
		}
		/// Shorthand for \ref type_exporter::export_return().
		void export_return(export_writer &out, std::string_view tmpname) const {
			exporter->export_return(out, *this, tmpname);
		}

		/// Creates a \ref qualified_type for the given \p clang::QualType and calls
		/// \ref entity_registry::queue_dependent_type() to queue the associated entity if necessary.
		///
		/// \return The \ref qualified_type created from the \p clang::QualType.
		inline static qualified_type queue_as_dependency(export_propagation_queue &q, clang::QualType qty) {
			qualified_type res(qty);
			res.entity = q.queue_dependent_type(res.base_type);
			return res;
		}
	};
}
