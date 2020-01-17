#include "exporter.h"

/// \file
/// Implementation of actual exporting the entities.

namespace apigen {
	// exporting of api types
	std::string_view exporter::get_exported_type_name(const clang::Type *type, entity *entity) const {
		if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
			// still use the policy for internal names so that the code compiles without problems
			return to_string_view(builtin->getName(printing_policy));
		} else if (llvm::isa<clang::EnumType>(type)) {
			return _enum_names.at(cast<entities::enum_entity>(entity)).name.get_cached();
		} else if (llvm::isa<clang::RecordType>(type)) {
			return _record_names.at(cast<entities::record_entity>(entity)).name.get_cached();
		}
		return "$UNSUPPORTED";
	}

	void exporter::export_api_parameter_type(cpp_writer &writer, const qualified_type &type, bool mark_move) const {
		writer.write_fmt("{} ", get_exported_type_name(type.type, type.type_entity));
		if (type.is_reference_or_pointer()) {
			export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		} else {
			if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
				if (!complex_ty->has_move_constructor()) {
					writer.write("const "); // this parameter will be copied
				} else {
					if (mark_move) {
						writer.write(APIGEN_STR(APIGEN_MOVED)); // an empty marker that indicates this parameter is moved
					}
				}
				writer.write("*");
			} // nothing to do for other primitive types
		}
	}

	void exporter::export_api_return_type(cpp_writer &writer, const qualified_type &type) const {
		writer.write_fmt("{} ", get_exported_type_name(type.type, type.type_entity));
		if (type.is_reference_or_pointer()) {
			export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		} else {
			if (type.type_entity && isa<entities::record_entity>(*type.type_entity)) {
				writer.write('*');
			}
		}
	}

	void exporter::_export_api_field_getter_return_type_pointers_and_qualifiers(
		cpp_writer &writer, const qualified_type &type, entities::field_kind kind, bool is_const
	) {
		export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		// const fields already have this const in their qualifiers
		if (kind == entities::field_kind::normal_field && is_const) {
			writer.write("const ");
		}
		// for references, use the pointer directly
		if (kind != entities::field_kind::reference_field) {
			writer.write("*");
		}
	}

	void exporter::export_api_pointers_and_qualifiers(
		cpp_writer &writer, reference_kind ref, const std::vector<qualifier> &quals
	) {
		for (auto it = quals.rbegin(); it != --quals.rend(); ++it) {
			writer.write_fmt("{}*", *it);
		}
		writer.write(quals.front());
		if (ref != reference_kind::none) {
			writer.write("*const ");
		}
	}


	// exporting of actual functions and structs
	void exporter::_export_api_enum_type(
		cpp_writer &writer, entities::enum_entity *entity, const enum_naming &name
	) const {
		writer.write("enum ");
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			for (auto &&enumerator : name.enumerators) {
				writer
					.new_line()
					.write_fmt("{} = {}", enumerator.second.get_cached(), enumerator.first)
					.maybe_separate(",");
			}
		}
		writer
			.write(";")
			.new_line()
			.write_fmt(
				"typedef {} {};",
				get_exported_type_name(entity->get_integer_type(), nullptr),
				name.name.get_cached()
			);
	}

	void exporter::_export_api_type(cpp_writer &writer, const record_naming &name) {
		writer.write_fmt("typedef struct {0} {0};", name.name.get_cached());
	}

	void exporter::_export_api_function_pointer_definition(
		cpp_writer &writer, entities::function_entity *entity, const function_naming &name
	) const {
		if (auto &return_type = entity->get_api_return_type()) {
			export_api_return_type(writer, return_type.value());
		}
		writer.write_fmt("(*{})", name.api_name.get_cached());
		{
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
			for (auto &&param : entity->get_parameters()) {
				writer.new_line();
				export_api_parameter_type(writer, param.type, true);
				writer.maybe_separate(",");
			}
			if (auto &return_type = entity->get_api_return_type()) {
				if (return_type->is_record_type()) {
					writer.write("void*");
				}
			}
		}
		writer.write(";");
	}

	void exporter::_export_api_destructor_definition(
		cpp_writer &writer, entities::record_entity*, const record_naming &name
	) const {
		writer.write_fmt("void (*{})({} *);", name.destructor_api_name.get_cached(), name.name.get_cached());
	}

	void exporter::_export_api_field_getter_definitions(
		cpp_writer &writer, entities::field_entity *entity, const field_naming &name
	) const {
		auto &type = entity->get_type();
		auto parent_it = _record_names.find(entity->get_parent());
		assert_true(parent_it != _record_names.end());

		// only normal fields have non-const getters
		if (entity->get_field_kind() == entities::field_kind::normal_field) {
			writer.write_fmt("{} ", get_exported_type_name(type.type, type.type_entity));
			_export_api_field_getter_return_type_pointers_and_qualifiers(
				writer, type, entity->get_field_kind(), false
			);
			writer
				.write_fmt("(*{})({} *);", name.getter_api_name.get_cached(), parent_it->second.name.get_cached())
				.new_line();
		}

		writer.write_fmt("{} ", get_exported_type_name(type.type, type.type_entity));
		_export_api_field_getter_return_type_pointers_and_qualifiers(
			writer, type, entity->get_field_kind(), true
		);
		writer.write_fmt(
			"(*{})({} const *);", name.const_getter_api_name.get_cached(), parent_it->second.name.get_cached()
		);
	}


	void exporter::_export_pass_parameter(
		cpp_writer &writer, const qualified_type &type, std::string_view param
	) const {
		if (type.is_reference_or_pointer()) { // passing a reference
			cpp_writer::scope_token scope;
			if (type.ref_kind == reference_kind::rvalue_reference) { // cast to rvalue reference
				writer.write_fmt("static_cast<{}>", writer.name_printer.get_internal_qualified_type_name(type));
				scope = writer.begin_scope(cpp_writer::parentheses_scope);
			}
			if (type.is_reference()) { // dereference if necessary
				writer.write('*');
			}
			// cast to type
			if (type.ref_kind != reference_kind::none) {
				// for references, first cast to the corresponding pointer type
				writer.write_fmt(
					"reinterpret_cast<{}>({})",
					writer.name_printer.get_internal_qualified_type_name(
						type.type, reference_kind::none, { qualifier::const_qual },
						type.qualifiers.data(), type.qualifiers.size()
					),
					param
				);
			} else {
				// pass pointers directly
				writer.write_fmt(
					"reinterpret_cast<{}>({})", writer.name_printer.get_internal_qualified_type_name(type), param
				);
			}
		} else { // passing an object
			if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
				cpp_writer::scope_token possible_move;
				std::string cast_type;
				if (complex_ty->has_move_constructor()) { // move
					writer.write("::std::move");
					possible_move = writer.begin_scope(cpp_writer::parentheses_scope);
					cast_type = writer.name_printer.get_internal_qualified_type_name(
						type.type, reference_kind::none, { qualifier::none, qualifier::none }
					);
				} else {
					cast_type = writer.name_printer.get_internal_qualified_type_name(
						type.type, reference_kind::none, { qualifier::none, qualifier::const_qual }
					);
				}
				writer.write_fmt("*reinterpret_cast<{}>({})", cast_type, param);
			} else { // primitive types
				if (type.type->isEnumeralType()) { // cast enums
					writer.write_fmt(
						"static_cast<{}>({})", writer.name_printer.get_internal_type_name(type.type), param
					);
				} else { // pass directly
					writer.write(param);
				}
			}
		}
	}


	// exporting of internal function implementations
	void exporter::_export_plain_function_call(
		cpp_writer &writer, entities::function_entity *entity,
		const std::vector<std::string> &parameter_names
	) const {
		auto param_it = entity->get_parameters().begin();
		auto param_name_it = parameter_names.begin();
		if (auto *method_ent = dyn_cast<entities::method_entity>(entity)) {
			auto *method_decl = llvm::cast<clang::CXXMethodDecl>(method_ent->get_declaration());
			auto *decl = method_decl->getParent();
			if (!isa<entities::constructor_entity>(*method_ent)) {
				// if this is a method and not a constructor
				if (method_ent->is_static()) { // export static member function call
					writer.write_fmt("{}::", writer.name_printer.get_internal_entity_name(decl));
				} else { // non-static, export member function call
					assert_true(param_it->type.qualifiers.size() == 2);
					assert_true(param_it->type.ref_kind == reference_kind::none);
					// the first parameter is the "this" parameter
					writer.write_fmt(
						"reinterpret_cast<{} {}*>({})->",
						writer.name_printer.get_internal_entity_name(decl),
						param_it->type.qualifiers.back(),
						*param_name_it
					);
					++param_it;
					++param_name_it;
				}
				// here scope is unnecessary
				writer.write(writer.name_printer.get_internal_function_name(method_decl));
			} else { // constructor, write class name
				writer.write(writer.name_printer.get_internal_entity_name(decl));
			}
		} else { // normal function, print function name with scope
			writer.write(writer.name_printer.get_internal_entity_name(entity->get_declaration()));
		}
		{ // parameters
			auto param_scope = writer.begin_scope(cpp_writer::parentheses_scope);
			for (; param_it != entity->get_parameters().end(); ++param_it, ++param_name_it) {
				writer.new_line();
				_export_pass_parameter(writer, param_it->type, *param_name_it);
				writer.maybe_separate(",");
			}
		}
	}

	void exporter::_export_function_impl(
		cpp_writer &writer, entities::function_entity *entity, const function_naming &name
	) const {
		name_allocator alloc = name_allocator::from_parent_immutable(_impl_scope);
		std::vector<name_allocator::token> param_tokens;
		std::vector<std::string> parameters;

		writer.write("inline static ");
		if (auto &return_type = entity->get_api_return_type()) {
			export_api_return_type(writer, return_type.value());
		}
		writer.write_fmt("{}", name.impl_name.get_cached());
		bool complex_return = false; // indicates whether the function call should be wrapped in a placement new
		{
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
			for (auto &&param : entity->get_parameters()) {
				writer.new_line();
				export_api_parameter_type(writer, param.type, false);
				param_tokens.emplace_back(alloc.allocate_function_parameter(param.name, ""));
				parameters.emplace_back(param_tokens.back()->get_name());
				writer
					.write(parameters.back())
					.maybe_separate(",");
			}
			if (auto &return_type = entity->get_api_return_type()) {
				if (return_type->is_record_type()) {
					// additional input pointing to the memory block that receives the returned object
					param_tokens.emplace_back(alloc.allocate_function_parameter("output", ""));
					parameters.emplace_back(param_tokens.back()->get_name());
					writer.write_fmt("void *{}", parameters.back());
					complex_return = true;
				}
			}
		}
		writer.write(" ");
		{ // function body
			auto scope = writer.begin_scope(cpp_writer::braces_scope);

			// the actual function call
			writer.new_line();
			if (complex_return) {
				auto *return_type = entity->get_api_return_type().value().type;
				writer.write_fmt("new ({}) ", parameters.back());
				if (isa<entities::constructor_entity>(*entity)) {
					// for constructors, this should be directly followed by the constructor call
					_export_plain_function_call(writer, entity, parameters);
				} else {
					// otherwise the call to the actual function need to be surrounded by parentheses
					writer.write(writer.name_printer.get_internal_type_name(return_type));
					auto new_scope = writer.begin_scope(cpp_writer::parentheses_scope);
					_export_plain_function_call(writer, entity, parameters);
				}
			} else { // simple or no return
				auto &return_type = entity->get_api_return_type();
				if (return_type && !return_type->is_void()) { // simple return
					writer.write("return ");
					if (return_type->is_reference_or_pointer()) { // references and pointers
						cpp_writer::scope_token cast_scope;
						if (!return_type->type->isBuiltinType()) { // cast the pointer to the correct type
							writer.write("reinterpret_cast<");
							export_api_return_type(writer, return_type.value());
							writer.write(">");
							cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
						}
						if (return_type->is_reference()) { // for references, pointers are returned
							writer.write("&");
						}
						_export_plain_function_call(writer, entity, parameters);
					} else { // plain builtin object or enum
						if (auto *enum_ent = dyn_cast<entities::enum_entity>(return_type->type_entity)) {
							// enums need static_cast
							auto &enum_name = _enum_names.at(enum_ent);
							writer.write_fmt("static_cast<{}>", enum_name.name.get_cached());
							auto cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
							_export_plain_function_call(writer, entity, parameters);
						} else { // plain builtin data types such as int, float, etc.
							_export_plain_function_call(writer, entity, parameters);
						}
					}
				} else { // no return
					_export_plain_function_call(writer, entity, parameters);
				}
			}
			writer.write(";");

			// final return if the return type is complex
			if (complex_return) {
				auto it = _record_names.find(cast<entities::record_entity>(
					entity->get_api_return_type().value().type_entity
					));
				assert_true(it != _record_names.end());
				writer
					.new_line()
					.write_fmt("return static_cast<{}*>({});", it->second.name.get_cached(), parameters.back());
			}
		}
	}

	void exporter::_export_field_getter_impls(
		cpp_writer &writer, entities::field_entity *entity, const field_naming &name
	) const {
		auto &type = entity->get_type();
		auto parent_it = _record_names.find(entity->get_parent());
		assert_true(parent_it != _record_names.end());
		std::string_view exported_type_name = get_exported_type_name(type.type, type.type_entity);

		if (entity->get_field_kind() == entities::field_kind::normal_field) { // non-const getter
			name_allocator func_scope = name_allocator::from_parent_immutable(_impl_scope);
			auto input = func_scope.allocate_function_parameter("object", "");
			writer.write_fmt("inline static {} ", exported_type_name);
			_export_api_field_getter_return_type_pointers_and_qualifiers(
				writer, type, entity->get_field_kind(), false
			);
			writer.write_fmt(
				"{}({} *{}) ",
				name.getter_impl_name.get_cached(), parent_it->second.name.get_cached(), input->get_name()
			);
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				writer
					.new_line()
					.write("return ");
				{
					cpp_writer::scope_token cast_scope;
					if (!entity->get_type().type->isBuiltinType()) {
						writer.write_fmt("reinterpret_cast<{} ", exported_type_name);
						_export_api_field_getter_return_type_pointers_and_qualifiers(
							writer, type, entity->get_field_kind(), false
						);
						writer.write(">");
						cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
					}
					writer.write_fmt(
						"&reinterpret_cast<{} *>({})->{}",
						writer.name_printer.get_internal_entity_name(entity->get_parent()->get_declaration()),
						input->get_name(), to_string_view(entity->get_declaration()->getName())
					);
				}
				writer.write(";");
			}
			writer.new_line();
		}

		{ // const getter
			name_allocator func_scope = name_allocator::from_parent_immutable(_impl_scope);
			auto input = func_scope.allocate_function_parameter("object", "");
			writer.write_fmt("inline static {} ", exported_type_name);
			_export_api_field_getter_return_type_pointers_and_qualifiers(
				writer, type, entity->get_field_kind(), true
			);
			writer.write_fmt(
				"{}({} const *{}) ",
				name.const_getter_impl_name.get_cached(), parent_it->second.name.get_cached(), input->get_name()
			);
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				writer
					.new_line()
					.write_fmt("return ");
				{
					cpp_writer::scope_token cast_scope;
					if (!entity->get_type().type->isBuiltinType()) {
						writer.write_fmt("reinterpret_cast<{} ", exported_type_name);
						_export_api_field_getter_return_type_pointers_and_qualifiers(
							writer, type, entity->get_field_kind(), true
						);
						writer.write(">");
						cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
					}
					writer.write_fmt(
						"&reinterpret_cast<{} const *>({})->{}",
						writer.name_printer.get_internal_entity_name(entity->get_parent()->get_declaration()),
						input->get_name(), to_string_view(entity->get_declaration()->getName())
					);
				}
				writer.write(";");
			}
		}
	}

	void exporter::_export_destructor_impl(
		cpp_writer &writer, entities::record_entity *entity, const record_naming &name
	) const {
		name_allocator alloc = name_allocator::from_parent_immutable(_impl_scope);
		auto input = alloc.allocate_function_parameter("object", "");
		writer.write_fmt(
			"inline static void {}({} *{}) ",
			name.destructor_impl_name.get_cached(), name.name.get_cached(), input->get_name()
		);
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			writer
				.new_line()
				.write_fmt(
					"reinterpret_cast<{} *>({})->~{}();",
					writer.name_printer.get_internal_entity_name(entity->get_declaration()),
					input->get_name(), to_string_view(entity->get_declaration()->getName())
				);
		}
	}


	// exporting of whole files
	void exporter::export_api_header(std::ostream &out) const {
		cpp_writer writer(out, printing_policy);

		// a couple of definitions so that the user doesn't have to #include "apigen_definitions.h"
		writer
			.write("#define " APIGEN_STR(APIGEN_MOVED))
			.new_line()
			.write("#define " APIGEN_STR(APIGEN_TEMPORARY))
			.new_line()
			.new_line();

		for (auto &&[ent, name] : _enum_names) {
			_export_api_enum_type(writer, ent, name);
			writer
				.new_line()
				.new_line();
		}
		for (auto &&[ent, name] : _record_names) {
			_export_api_type(writer, name);
			writer
				.new_line()
				.new_line();
		}

		writer
			.new_line()
			.write_fmt("typedef struct {} ", naming->api_struct_name);
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			for (auto &&[ent, name] : _function_names) {
				writer.new_line();
				_export_api_function_pointer_definition(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _record_names) {
				writer.new_line();
				_export_api_destructor_definition(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _field_names) {
				writer.new_line();
				_export_api_field_getter_definitions(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _custom_func_names) {
				writer.new_line();
				ent->export_pointer_declaration(writer, *this, name.api_name.get_cached());
				writer.new_line();
			}
		}
		writer.write_fmt(" {};", naming->api_struct_name);
	}

	void exporter::export_host_h(std::ostream &out) const {
		cpp_writer writer(out, printing_policy);
		writer
			.write_fmt("struct {};", naming->api_struct_name)
			.new_line()
			.write_fmt("void {}({}&);", naming->api_struct_init_function_name, naming->api_struct_name);
	}

	void exporter::export_host_cpp(std::ostream &out) const {
		cpp_writer writer(out, printing_policy);

		// custom dependencies
		for (auto &header : _entities.get_custom_host_dependencies()) {
			writer
				.write_fmt("#include <{}>", header)
				.new_line();
		}

		writer.write_fmt("struct {} ", APIGEN_API_CLASS_NAME_STR);
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			writer
				.new_line()
				.write("public:");
			for (auto &&[ent, name] : _function_names) {
				writer.new_line();
				_export_function_impl(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _record_names) {
				writer.new_line();
				_export_destructor_impl(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _field_names) {
				writer.new_line();
				_export_field_getter_impls(writer, ent, name);
				writer.new_line();
			}
			for (auto &&[ent, name] : _custom_func_names) {
				writer.new_line();
				ent->export_definition(writer, *this, name.impl_name.get_cached());
				writer.new_line();
			}
		}
		writer
			.write(";")
			.new_line()
			.new_line();
		{
			name_allocator alloc = name_allocator::from_parent_immutable(_global_scope);
			auto result_var = alloc.allocate_function_parameter("result", "");

			writer.write_fmt(
				"void {}({} &{}) ",
				naming->api_struct_init_function_name,
				naming->api_struct_name,
				result_var->get_name()
			);
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				for (auto &&[func, name] : _function_names) {
					writer
						.new_line()
						.write_fmt(
							"{}.{} = {}::{};",
							result_var->get_name(), name.api_name.get_cached(),
							APIGEN_API_CLASS_NAME_STR, name.impl_name.get_cached()
						);
				}
				for (auto &&[record, name] : _record_names) {
					writer
						.new_line()
						.write_fmt(
							"{}.{} = {}::{};",
							result_var->get_name(), name.destructor_api_name.get_cached(),
							APIGEN_API_CLASS_NAME_STR, name.destructor_impl_name.get_cached()
						);
				}
				for (auto &&[field, name] : _field_names) {
					if (field->get_field_kind() == entities::field_kind::normal_field) {
						writer
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var->get_name(), name.getter_api_name.get_cached(),
								APIGEN_API_CLASS_NAME_STR, name.getter_impl_name.get_cached()
							);
					}
					writer
						.new_line()
						.write_fmt(
							"{}.{} = {}::{};",
							result_var->get_name(), name.const_getter_api_name.get_cached(),
							APIGEN_API_CLASS_NAME_STR, name.const_getter_impl_name.get_cached()
						);
				}
				for (auto &&[func, name] : _custom_func_names) {
					writer
						.new_line()
						.write_fmt(
							"{}.{} = {}::{};",
							result_var->get_name(), name.api_name.get_cached(),
							APIGEN_API_CLASS_NAME_STR, name.impl_name.get_cached()
						);
				}
			}
		}
	}

	/// Type declaration for record type size and alignment data.
	const std::string_view _size_alignment_type_decl = "const size_t "; // TODO is this good practice?
	void exporter::export_data_collection_cpp(std::ostream &out) const {
		cpp_writer writer(out, printing_policy);

		writer
			.write("#include <iostream>")
			.new_line()
			.new_line()
			.write("int main() ");
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			for (auto &&[rec, name] : _record_names) {
				std::string internal_name = writer.name_printer.get_internal_entity_name(rec->get_declaration());
				writer
					.new_line()
					.write_fmt(R"(std::cout << "{})", _size_alignment_type_decl)
					.write_fmt(naming->size_name_pattern, name.name.get_cached())
					.write_fmt(R"( = " << sizeof({}) << ";\n";)", internal_name)
					.new_line()
					.write_fmt(R"(std::cout << "{})", _size_alignment_type_decl)
					.write_fmt(naming->align_name_pattern, name.name.get_cached())
					.write_fmt(R"( = " << alignof({}) << ";\n\n";)", internal_name)
					.new_line();
			}
			writer
				.new_line()
				.write("return 0;");
		}
	}
}
