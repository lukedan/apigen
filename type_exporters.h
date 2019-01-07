#pragma once

#include "qualified_type.h"
#include "entity_types.h"

namespace apigen {
	/// Used to export values.
	class value_type_exporter : public type_exporter {
		/// Uses move structs for records, and pointers for other types.
		std::string get_parameter_spelling(const qualified_type &qty) const override {
			if (auto *rec_ent = dyn_cast<record_entity>(qty.entity); rec_ent) {
				return rec_ent->move_struct_name.compose() + qty.postfix; // use move struct for records
			}
			return qty.get_exported_base_type_name() + qty.postfix; // use type directly
		}
		/// The same as \ref get_parameter_spelling().
		std::string get_return_type_spelling(const qualified_type &qty) const override {
			return get_parameter_spelling(qty);
		}
		/// Uses the corresponding pointer type to the object.
		std::string get_field_return_type_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "*";
		}

		/// Asserts that the object supplied as the parameter is not \p nullptr.
		void export_prepare_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			if (isa<record_entity>(qty.entity)) {
				out.indent() << "assert(" << param << ".object != nullptr);" << new_line();
			}
		}
		/// Passes the argument differently based on whether it's a record, an enum, or a built-in type.
		void export_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			if (auto *rec_ent = dyn_cast<record_entity>(qty.entity); rec_ent) {
				std::string tyname = qty.get_internal_base_type_name();
				out <<
					"(" <<
						param << ".move ?" <<
						tyname << "(std::move(*reinterpret_cast<" << tyname << "*>(" << param << ".object))) :" <<
						tyname << "(*reinterpret_cast<const " << tyname << "*>(" << param << ".object))" <<
					")";
			} else if (isa<enum_entity>(qty.entity)) {
				out << "static_cast<" << qty.get_internal_base_type_name() << ">(" << param << ")";
			} else {
				out << param;
			}
		}

		/// Handles records, enums, and built-in types differently.
		void export_prepare_return(
			export_writer &out, const qualified_type &qty, std::string_view tmp
		) const override {
			if (isa<record_entity>(qty.entity)) {
				// TODO stupid implementation
				out.indent() << "auto *" << tmp << " = new " << qty.get_internal_base_type_name() << "(";
			} else if (isa<enum_entity>(qty.entity)) {
				out.indent() << "return static_cast<" << qty.get_exported_base_type_name() << qty.postfix << ">(";
			} else {
				out.indent() << "return ";
			}
		}
		/// Handles records, enums, and built-in types differently.
		void export_return(export_writer &out, const qualified_type &qty, std::string_view tmpname) const override {
			if (auto *rec_ent = dyn_cast<record_entity>(qty.entity); rec_ent) {
				out << ");" << new_line();
				out.indent() <<
					"return " << rec_ent->move_struct_name.compose() << "{reinterpret_cast<" <<
					qty.get_exported_base_type_name() << "*>(" << tmpname << "), false};";
			} else if (isa<enum_entity>(qty.entity)) {
				out << ");";
			} else {
				out << ";";
			}
		}
	};

	/// Used to export pointers.
	class pointer_type_exporter : public type_exporter {
		/// Returns the pointer type.
		std::string get_parameter_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix;
		}
		/// The same as \ref get_parameter_spelling().
		std::string get_return_type_spelling(const qualified_type &qty) const override {
			return get_parameter_spelling(qty);
		}
		/// Uses the corresponding pointer type to the pointer.
		std::string get_field_return_type_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "*";
		}

		/// Converts and passes the pointer.
		void export_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			out << "reinterpret_cast<" << qty.get_internal_type_spelling() << ">(" << param << ")";
		}

		/// Exports code that returns the converted pointer type.
		void export_prepare_return(
			export_writer &out, const qualified_type &qty, std::string_view
		) const override {
			out.indent() << "return reinterpret_cast<" << qty.get_exported_base_type_name() << qty.postfix << ">(";
		}
		/// Exports the enclosing parenthesis.
		void export_return(export_writer &out, const qualified_type&, std::string_view) const override {
			out << ");";
		}
	};

	/// Used to export lvalue references.
	class lvalue_reference_type_exporter : public type_exporter {
		/// Uses the corresponding pointer type.
		std::string get_parameter_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}
		/// The same as \ref get_parameter_spelling().
		std::string get_return_type_spelling(const qualified_type &qty) const override {
			return get_parameter_spelling(qty);
		}
		/// Uses the corresponding const pointer type.
		std::string get_field_return_type_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}

		/// Converts and passes the given pointer as a lvalue reference.
		void export_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			out << "reinterpret_cast<" << qty.get_internal_type_spelling() << ">(*" << param << ")";
		}

		/// Exports code that returns the corresponding pointer.
		void export_prepare_return(
			export_writer &out, const qualified_type &qty, std::string_view
		) const override {
			out.indent() << "return reinterpret_cast<" << qty.get_exported_base_type_name() << qty.postfix << "*>(&(";
		}
		/// Exports enclosing parentheses.
		void export_return(export_writer &out, const qualified_type&, std::string_view) const override {
			out << "));";
		}
	};

	/// Used to export rvalue references to objects.
	class rvalue_reference_to_object_exporter : public type_exporter {
		/// If the object is a record type, uses the move struct; otherwise use the corresponding pointer type.
		std::string get_parameter_spelling(const qualified_type &qty) const override {
			if (auto *rec_ent = dyn_cast<record_entity>(qty.entity); rec_ent) { // the move flag must be true
				return rec_ent->move_struct_name.compose() + qty.postfix;
			}
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}
		/// The same as \ref get_parameter_spelling().
		std::string get_return_type_spelling(const qualified_type &qty) const override {
			return get_parameter_spelling(qty);
		}
		/// Uses the corresponding const pointer type.
		std::string get_field_return_type_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}

		/// Asserts that the object supplied as the parameter is not \p nullptr and that \p move is \p true.
		void export_prepare_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			if (isa<record_entity>(qty.entity)) {
				out.indent() << "assert(" << param << ".object != nullptr);" << new_line();
				out.indent() << "assert(" << param << ".move);" << new_line();
			}
		}

		/// Passes the argument differently based on whether it's a record, an enum, or a built-in type.
		void export_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			std::string tyname = qty.get_internal_base_type_name();
			if (isa<record_entity>(qty.entity)) {
				out << "std::move(*reinterpret_cast<" << tyname << "*>(" << param << ".object))";
			} else if (isa<enum_entity>(qty.entity)) {
				out << "std::move(static_cast<" << tyname << "*>(" << param << "))";
			} else {
				out << "std;:move(" << param << ")";
			}
		}

		/// Handles records, enums, and built-in types differently.
		void export_prepare_return(
			export_writer &out, const qualified_type &qty, std::string_view
		) const override {
			// TODO is it possible to pass rvalue ref to temporary?
			// TODO should probably return object
			if (auto *rec_ent = dyn_cast<record_entity>(qty.entity); rec_ent) {
				out.indent() << "return " << rec_ent->move_struct_name.compose() << "{";
			} else if (isa<enum_entity>(qty.entity)) {
				out.indent() << "return reinterpret_cast<" << qty.get_exported_base_type_name() << qty.postfix << ">(&(";
			} else {
				out.indent() << "return &(";
			}
		}
		/// Handles records, enums, and built-in types differently.
		void export_return(export_writer &out, const qualified_type &qty, std::string_view tmpname) const override {
			if (isa<record_entity>(qty.entity)) {
				out << ", true};";
			} else if (isa<enum_entity>(qty.entity)) {
				out << "));";
			} else {
				out << ");";
			}
		}
	};

	/// Used to export rvalue references to pointers.
	class rvalue_reference_to_pointer_exporter : public type_exporter {
		/// Uses the corresponding pointer type.
		std::string get_parameter_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}
		/// The same as \ref get_parameter_spelling().
		std::string get_return_type_spelling(const qualified_type &qty) const override {
			return get_parameter_spelling(qty);
		}
		/// Uses the corresponding const pointer type.
		std::string get_field_return_type_spelling(const qualified_type &qty) const override {
			return qty.get_exported_base_type_name() + qty.postfix + "* const";
		}

		/// Converts and passes the given pointer as a rvalue reference.
		void export_pass_argument(
			export_writer &out, const qualified_type &qty, std::string_view param, std::string_view
		) const override {
			out << "reinterpret_cast<" << qty.get_internal_type_spelling() << ">(*" << param << ")";
		}

		/// Exports code that returns the corresponding pointer.
		void export_prepare_return(
			export_writer &out, const qualified_type &qty, std::string_view
		) const override {
			// TODO should probably return pointer directly
			out.indent() << "return reinterpret_cast<" << qty.get_exported_base_type_name() << qty.postfix << "*>(&(";
		}
		/// Exports enclosing parentheses.
		void export_return(export_writer &out, const qualified_type &qty, std::string_view tmpname) const override {
			out << "));";
		}
	};
}
