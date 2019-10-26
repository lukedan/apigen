#include "exporter.h"

/// \file
/// Implementation of actual exporting the entities.

namespace apigen {
	std::string_view exporter::_get_internal_operator_name(clang::OverloadedOperatorKind kind) {
		switch (kind) {
		case clang::OO_New:
			return "operator new";
		case clang::OO_Delete:
			return "operator delete";
		case clang::OO_Array_New:
			return "operator new[]";
		case clang::OO_Array_Delete:
			return "operator delete[]";
		case clang::OO_Plus:
			return "operator+";
		case clang::OO_Minus:
			return "operator-";
		case clang::OO_Star:
			return "operator*";
		case clang::OO_Slash:
			return "operator/";
		case clang::OO_Percent:
			return "operator%";
		case clang::OO_Caret:
			return "operator^";
		case clang::OO_Amp:
			return "operator&";
		case clang::OO_Pipe:
			return "operator|";
		case clang::OO_Tilde:
			return "operator~";
		case clang::OO_Exclaim:
			return "operator!";
		case clang::OO_Equal:
			return "operator=";
		case clang::OO_Less:
			return "operator<";
		case clang::OO_Greater:
			return "operator>";
		case clang::OO_PlusEqual:
			return "operator+=";
		case clang::OO_MinusEqual:
			return "operator-=";
		case clang::OO_StarEqual:
			return "operator*=";
		case clang::OO_SlashEqual:
			return "operator/=";
		case clang::OO_PercentEqual:
			return "operator%=";
		case clang::OO_CaretEqual:
			return "operator^=";
		case clang::OO_AmpEqual:
			return "operator&=";
		case clang::OO_PipeEqual:
			return "operator|=";
		case clang::OO_LessLess:
			return "operator<<";
		case clang::OO_GreaterGreater:
			return "operator>>";
		case clang::OO_LessLessEqual:
			return "operator<<=";
		case clang::OO_GreaterGreaterEqual:
			return "operator>>=";
		case clang::OO_EqualEqual:
			return "operator==";
		case clang::OO_ExclaimEqual:
			return "operator!=";
		case clang::OO_LessEqual:
			return "operator<=";
		case clang::OO_GreaterEqual:
			return "operator>=";
		case clang::OO_Spaceship:
			return "operator<=>";
		case clang::OO_AmpAmp:
			return "operator&&";
		case clang::OO_PipePipe:
			return "operator||";
		case clang::OO_PlusPlus:
			return "operator++";
		case clang::OO_MinusMinus:
			return "operator--";
		case clang::OO_Comma:
			return "operator,";
		case clang::OO_ArrowStar:
			return "operator->*";
		case clang::OO_Arrow:
			return "operator->";
		case clang::OO_Call:
			return "operator()";
		case clang::OO_Subscript:
			return "operator[]";
		case clang::OO_Coawait:
			return "operator co_await";
		default:
			return "$BAD_OPERATOR";
		}
	}


	// exporting of internal types
	std::string exporter::_get_template_argument_spelling(const clang::TemplateArgument &arg) const {
		switch (arg.getKind()) {
		case clang::TemplateArgument::Null:
			return "$ERROR_NULL";
		case clang::TemplateArgument::Type:
			{
				// TODO this breaks for array types
				auto type = qualified_type::from_clang_type(arg.getAsType(), nullptr);
				std::string result = _get_internal_type_name(type.type) + " ";
				result += _get_internal_pointer_and_qualifiers(type.ref_kind, type.qualifiers);
				return result;
			}
	    // The template argument is a declaration that was provided for a pointer,
	    // reference, or pointer to member non-type template parameter.
		case clang::TemplateArgument::Declaration:
			break;
		case clang::TemplateArgument::NullPtr:
			return "nullptr";
		case clang::TemplateArgument::Integral:
			return arg.getAsIntegral().toString(10);
	    // The template argument is a template name that was provided for a
	    // template template parameter.
		case clang::TemplateArgument::Template:
			break;
	    // The template argument is a pack expansion of a template name that was
	    // provided for a template template parameter.
		case clang::TemplateArgument::TemplateExpansion:
			break;
		case clang::TemplateArgument::Expression:
			return "$UNSUPPORTED_TEMPLATE_ARG";
		case clang::TemplateArgument::Pack:
			return _get_template_argument_list_spelling(arg.getPackAsArray());
		}
		return "$UNSUPPORTED_TEMPLATE_ARG";

		/*std::string result;
		llvm::raw_string_ostream stream(result);
		arg.print(internal_printing_policy, stream);
		stream.str();
		return result;*/
	}

	std::string exporter::_get_template_argument_list_spelling(llvm::ArrayRef<clang::TemplateArgument> args) const {
		std::string result;
		for (auto &arg : args) {
			if (!result.empty()) {
				result += ", ";
			}
			result += _get_template_argument_spelling(arg);
		}
		return result;
	}

	std::string exporter::_get_internal_entity_name(clang::DeclContext *decl) const {
		std::string result;
		if (auto *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
			clang::OverloadedOperatorKind op_kind = func_decl->getOverloadedOperator();
			if (op_kind == clang::OO_None) {
				result = llvm::cast<clang::NamedDecl>(decl)->getName().str();
			} else {
				result = _get_internal_operator_name(op_kind);
			}
		} else {
			if (auto *template_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
				result =
					"<" + _get_template_argument_list_spelling(template_decl->getTemplateArgs().asArray()) + ">";
			}
			result = llvm::cast<clang::NamedDecl>(decl)->getName().str() + result;
		}
		result = "::" + result;

		for (
			clang::DeclContext *current = decl->getParent();
			current && !current->isTranslationUnit();
			current = current->getParent()
		) {
			if (auto *record_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(current)) {
				result =
					"<" + _get_template_argument_list_spelling(record_decl->getTemplateArgs().asArray()) +
					">" + result;
			}
			auto *named_decl = llvm::cast<clang::NamedDecl>(current);
			result = "::" + named_decl->getName().str() + result;
		}
		return result;
	}

	std::string exporter::_get_internal_type_name(const clang::Type *type) const {
		if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
			return std::string(to_string_view(builtin->getName(internal_printing_policy)));
		}
		if (auto *tagty = llvm::dyn_cast<clang::TagType>(type)) {
			return _get_internal_entity_name(tagty->getAsTagDecl());
		}
		return "$UNSUPPORTED";
	}

	void exporter::_write_internal_pointer_and_qualifiers(
		std::ostream &out, reference_kind ref, const std::vector<qualifier> &quals
	) {
		for (auto it = quals.rbegin(); it != --quals.rend(); ++it) {
			out << *it << "*";
		}
		out << quals.front();
		switch (ref) { // ampersand
		case reference_kind::reference:
			out << "&";
			break;
		case reference_kind::rvalue_reference:
			out << "&&";
			break;
		default:
			break;
		}
	}

	std::string exporter::_get_internal_pointer_and_qualifiers(
		reference_kind ref, const std::vector<qualifier> &quals
	) {
		std::stringstream ss;
		_write_internal_pointer_and_qualifiers(ss, ref, quals);
		return ss.str();
	}


	// exporting of api types
	std::string_view exporter::_get_exported_type_name(const clang::Type *type, entity *entity) const {
		if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
			// still use the policy for internal names so that the code compiles without problems
			return to_string_view(builtin->getName(internal_printing_policy));
		} else if (llvm::isa<clang::EnumType>(type)) {
			auto it = _enum_names.find(cast<entities::enum_entity>(entity));
			assert_true(it != _enum_names.end());
			return it->second.name.get_cached();
		} else if (llvm::isa<clang::RecordType>(type)) {
			auto it = _record_names.find(cast<entities::record_entity>(entity));
			assert_true(it != _record_names.end());
			return it->second.name.get_cached();
		}
		return "$UNSUPPORTED";
	}

	void exporter::_export_api_parameter_type(cpp_writer &writer, const qualified_type &type) const {
		writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
		if (type.is_reference_or_pointer()) {
			_export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		} else {
			if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
				if (!complex_ty->has_move_constructor()) {
					writer.write("const "); // this parameter will be copied
				}
				writer.write("*");
			} // nothing to do for other primitive types
		}
	}

	void exporter::_export_api_return_type(cpp_writer &writer, const qualified_type &type) const {
		writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
		if (type.is_reference_or_pointer()) {
			_export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		} else {
			if (type.type_entity && isa<entities::record_entity>(*type.type_entity)) {
				writer.write('*');
			}
		}
	}

	void exporter::_export_api_field_getter_return_type_pointers_and_qualifiers(
		cpp_writer &writer, const qualified_type &type, entities::field_kind kind, bool is_const
	) {
		_export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
		// const fields already have this const in their qualifiers
		if (kind == entities::field_kind::normal_field && is_const) {
			writer.write("const ");
		}
		// for references, use the pointer directly
		if (kind != entities::field_kind::reference_field) {
			writer.write("*");
		}
	}

	bool exporter::_maybe_export_api_return_type_input(cpp_writer &writer, const qualified_type &type) const {
		if (!type.is_reference_or_pointer()) {
			if (type.type_entity && isa<entities::record_entity>(*type.type_entity)) {
				writer
					.new_line()
					.write("void *");
				return true;
			}
		}
		return false;
	}

	void exporter::_export_api_pointers_and_qualifiers(
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
				_get_exported_type_name(entity->get_integer_type(), nullptr),
				name.name.get_cached()
			);
	}

	void exporter::_export_api_type(cpp_writer &writer, const record_naming &name) {
		writer.write("typedef struct ");
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
		}
		writer.write_fmt(" {};", name.name.get_cached());
	}

	void exporter::_export_api_function_pointer_definition(
		cpp_writer &writer, entities::function_entity *entity, const function_naming &name
	) const {
		if (auto &return_type = entity->get_api_return_type()) {
			_export_api_return_type(writer, return_type.value());
		}
		writer.write_fmt("(*{})", name.api_name.get_cached());
		{
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
			for (auto &&param : entity->get_parameters()) {
				writer.new_line();
				_export_api_parameter_type(writer, param.type);
				writer.maybe_separate(",");
			}
			if (auto &return_type = entity->get_api_return_type()) {
				_maybe_export_api_return_type_input(writer, return_type.value());
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
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
			_export_api_field_getter_return_type_pointers_and_qualifiers(
				writer, type, entity->get_field_kind(), false
			);
			writer
				.write_fmt("(*{})({} *)", name.getter_api_name.get_cached(), parent_it->second.name.get_cached())
				.new_line();
		}

		writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
		_export_api_field_getter_return_type_pointers_and_qualifiers(
			writer, type, entity->get_field_kind(), true
		);
		writer.write_fmt(
			"(*{})({} const *)", name.const_getter_api_name.get_cached(), parent_it->second.name.get_cached()
		);
	}


	void exporter::_export_pass_parameter(
		cpp_writer &writer, const qualified_type &type, std::string_view param
	) const {
		if (type.is_reference_or_pointer()) {
			cpp_writer::scope_token scope;
			if (type.ref_kind == reference_kind::rvalue_reference) {
				writer.write_fmt(
					"static_cast<{} {}>",
					_get_internal_type_name(type.type),
					_get_internal_pointer_and_qualifiers(type.ref_kind, type.qualifiers)
				);
				scope = writer.begin_scope(cpp_writer::parentheses_scope);
			}
			if (type.is_reference()) {
				writer.write('*');
			}
			writer.write_fmt("reinterpret_cast<{} ", _get_internal_type_name(type.type));
			_export_api_pointers_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			writer.write_fmt(">({})", param);
		} else {
			if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
				cpp_writer::scope_token possible_move;
				if (complex_ty->has_move_constructor()) {
					writer.write("::std::move");
					possible_move = writer.begin_scope(cpp_writer::parentheses_scope);
				}
				writer.write_fmt("*reinterpret_cast<{} ", _get_internal_type_name(type.type));
				if (!complex_ty->has_move_constructor()) {
					writer.write("const ");
				}
				writer.write_fmt("*>({})", param);
			} else { // primitive types
				writer.write(param); // pass directly
			}
		}
	}


	// exporting of internal function implementations
	void exporter::_export_function_impl(
		cpp_writer &writer, entities::function_entity *entity, const function_naming &name
	) const {
		name_allocator alloc = name_allocator::from_parent_immutable(_impl_scope);
		std::vector<name_allocator::token> param_tokens;
		std::vector<std::string> parameters;

		if (auto &return_type = entity->get_api_return_type()) {
			_export_api_return_type(writer, return_type.value());
		}
		writer.write_fmt("{}", name.impl_name.get_cached());
		bool complex_return = false; // indicates whether the function call should be wrapped in a placement new
		{
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
			for (auto &&param : entity->get_parameters()) {
				writer.new_line();
				_export_api_parameter_type(writer, param.type);
				param_tokens.emplace_back(alloc.allocate_function_parameter(param.name, ""));
				parameters.emplace_back(param_tokens.back()->get_name());
				writer
					.write(parameters.back())
					.maybe_separate(",");
			}
			if (auto &return_type = entity->get_api_return_type()) {
				if (_maybe_export_api_return_type_input(writer, return_type.value())) {
					param_tokens.emplace_back(alloc.allocate_function_parameter("output", ""));
					parameters.emplace_back(param_tokens.back()->get_name());
					writer.write(parameters.back());
					complex_return = true;
				}
			}
		}
		writer.write(" ");
		{ // function body
			auto scope = writer.begin_scope(cpp_writer::braces_scope);

			// the actual function call
			writer.new_line();
			{ // the optional placement new scope
				cpp_writer::scope_token scope1;
				if (complex_return) {
					auto *return_type = entity->get_api_return_type().value().type;
					writer.write_fmt(
						"new ({}) {}", parameters.back(), _get_internal_type_name(return_type)
					);
					// for constructors, this will be directly followed by the argument list
					if (!isa<entities::constructor_entity>(*entity)) {
						// otherwise the call to the actual function need to be surrounded by parentheses
						scope1 = writer.begin_scope(cpp_writer::parentheses_scope);
					}
				} else if (auto &return_type = entity->get_api_return_type()) { // simple return
					if (!return_type->is_void()) {
						writer.write("return ");
					}
				}

				auto param_it = entity->get_parameters().begin();
				auto param_name_it = parameters.begin();
				if (auto *method_ent = dyn_cast<entities::method_entity>(entity)) {
					if (!isa<entities::constructor_entity>(*method_ent)) {
						// if this is a method and not a constructor
						auto *method_decl = llvm::cast<clang::CXXMethodDecl>(method_ent->get_declaration());
						auto *decl = method_decl->getParent();
						if (method_ent->is_static()) { // export static member function call
							writer.write_fmt("{}::", _get_internal_entity_name(decl));
						} else { // non-static, export member function call
							assert_true(param_it->type.qualifiers.size() == 2);
							assert_true(param_it->type.ref_kind == reference_kind::none);
							writer.write_fmt(
								"reinterpret_cast<{} {}*>({})->",
								_get_internal_entity_name(decl),
								param_it->type.qualifiers.back(),
								*param_name_it
							);
							++param_it;
							++param_name_it;
						}
						writer.write(to_string_view(method_decl->getName()));
					}
				} else { // normal function, print function name
					writer.write(_get_internal_entity_name(entity->get_declaration()));
				}
				{ // parameters
					auto scope2 = writer.begin_scope(cpp_writer::parentheses_scope);
					for (; param_it != entity->get_parameters().end(); ++param_it, ++param_name_it) {
						writer.new_line();
						_export_pass_parameter(writer, param_it->type, *param_name_it);
						writer.maybe_separate(",");
					}
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

		if (entity->get_field_kind() == entities::field_kind::normal_field) { // non-const getter
			name_allocator func_scope = name_allocator::from_parent_immutable(_impl_scope);
			auto input = func_scope.allocate_function_parameter("object", "");
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
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
					.write_fmt(
						"return &reinterpret_cast<{} *>({})->{};",
						_get_internal_entity_name(entity->get_parent()->get_declaration()),
						input->get_name(), to_string_view(entity->get_declaration()->getName())
					);
			}
			writer.new_line();
		}

		{ // const getter
			name_allocator func_scope = name_allocator::from_parent_immutable(_impl_scope);
			auto input = func_scope.allocate_function_parameter("object", "");
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
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
					.write_fmt(
						"return &reinterpret_cast<{} const *>({})->{};",
						_get_internal_entity_name(entity->get_parent()->get_declaration()),
						input->get_name(), to_string_view(entity->get_declaration()->getName())
					);
			}
		}
	}

	void exporter::_export_destructor_impl(
		cpp_writer &writer, entities::record_entity *entity, const record_naming &name
	) const {
		name_allocator alloc = name_allocator::from_parent_immutable(_impl_scope);
		auto input = alloc.allocate_function_parameter("object", "");
		writer.write_fmt(
			"void {}({} *{}) ", name.destructor_impl_name.get_cached(), name.name.get_cached(), input->get_name()
		);
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			writer
				.new_line()
				.write_fmt(
					"reinterpret_cast<{} *>({})->~{}();",
					_get_internal_entity_name(entity->get_declaration()),
					input->get_name(), to_string_view(entity->get_declaration()->getName())
				);
		}
	}


	// exporting of whole files
	void exporter::export_api_header(std::ostream &out) const {
		cpp_writer writer(out);
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
			.write("typedef struct ");
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
		}
		writer.write_fmt(" {};", naming->api_struct_name);
	}

	void exporter::export_host_h(std::ostream &out) const {
		cpp_writer writer(out);
		writer
			.write_fmt("struct {};", naming->api_struct_name)
			.new_line()
			.write_fmt("void {}({}&);", naming->api_struct_init_function_name, naming->api_struct_name);
	}

	void exporter::export_host_cpp(std::ostream &out) const {
		cpp_writer writer(out);
		writer.write_fmt("class {} ", APIGEN_API_CLASS_NAME_STR);
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
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
				auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
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
			}
		}
	}

	/// Type declaration for record type size and alignment data.
	const std::string_view _size_alignment_type_decl = "const size_t "; // TODO is this good practice?
	void exporter::export_data_collection_cpp(std::ostream &out) const {
		cpp_writer writer(out);

		writer
			.write("#include <iostream>")
			.new_line()
			.new_line()
			.write("int main() ");
		{
			auto scope = writer.begin_scope(cpp_writer::braces_scope);
			for (auto &&[rec, name] : _record_names) {
				std::string internal_name = _get_internal_entity_name(rec->get_declaration());
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
