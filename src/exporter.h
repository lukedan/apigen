#pragma once

/// \file
/// Used to generate the exported code.

#include <unordered_map>
#include <sstream>

#include <fmt/ostream.h> // TODO C++20

#include "entity_kinds/function_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/user_type_entity.h"
#include "entity_registry.h"
#include "cpp_writer.h"
#include "naming_convention.h"

namespace apigen {
	/// Used to gather and export all entities.
	class exporter {
	public:
		// TODO: resolve overload & duplicate names
		struct function_naming {
			std::string
				api_name, ///< The name of the exported function pointer.
				impl_name; ///< The name of the function that is the internal implementation.

			/// Constructs a \ref function_naming from the given \ref entities::field_entity.
			inline static function_naming from_entity(entities::function_entity &ent, naming_convention &conv) {
				function_naming result;
				result.api_name = conv.get_function_name_dynamic(ent);
				result.impl_name = "internal_" + result.api_name;
				return result;
			}
		};
		/// Contains naming information of an \ref entities::field_entity.
		struct field_naming {
			std::string
				getter_api_name, ///< The exported name of the getter.
				getter_impl_name, ///< The name of the getter's internal implementation.
				const_getter_api_name, ///< The exported name of the const getter.
				const_getter_impl_name; ///< The name of the const getter's internal implementation.

			/// Constructs a \ref field_naming from the given \ref entities::field_entity.
			inline static field_naming from_entity(entities::field_entity &ent, naming_convention &conv) {
				field_naming result;
				if (ent.get_field_kind() == entities::field_kind::normal_field) {
					result.getter_api_name = conv.get_field_getter_name(ent);
					result.getter_impl_name = "internal_" + result.getter_api_name;
				}
				result.const_getter_api_name = conv.get_field_const_getter_name(ent);
				result.const_getter_impl_name = "internal_" + result.const_getter_api_name;
				return result;
			}
		};
		/// Contains naming information of an \ref entities::enum_entity.
		struct enum_naming {
			std::string name; ///< The name of this enum definition.
			std::vector<std::pair<std::int64_t, std::string>> enumerators; ///< The name of all enumerators.

			/// Constructs a \ref enum_naming from the given \ref entities::enum_entity.
			inline static enum_naming from_entity(entities::enum_entity &ent, naming_convention &conv) {
				enum_naming result;
				result.name = conv.get_enum_name(ent);
				for (clang::EnumConstantDecl *enumerator : ent.get_declaration()->enumerators()) {
					result.enumerators.emplace_back(
						enumerator->getInitVal().getExtValue(),
						conv.get_enumerator_name(ent, enumerator)
					);
				}
				return result;
			}
		};
		/// Contains naming information of an \ref entities::record_entity.
		struct record_naming {
			std::string
				name, ///< The name of the exported record reference type.
				destructor_api_name, ///< The exported name of the destructor.
				destructor_impl_name; ///< The name of the internal implementation of the function.

			/// Constructs a \ref record_naming from the given \ref entities::record_entity.
			inline static record_naming from_entity(entities::record_entity &ent, naming_convention &conv) {
				record_naming result;
				result.name = conv.get_record_name(ent);
				result.destructor_api_name = conv.get_record_destructor_name(ent);
				result.destructor_impl_name = "internal_" + result.destructor_api_name;
				return result;
			}
		};

		/// Initializes \ref internal_printing_policy.
		explicit exporter(clang::PrintingPolicy policy) : internal_printing_policy(std::move(policy)) {
		}
		/// Initializes \ref naming.
		exporter(clang::PrintingPolicy policy, naming_convention &conv) :
			internal_printing_policy(std::move(policy)), naming(&conv) {
		}

		/// Collects exported entities from the given \ref entity_registry.
		void collect_exported_entities(entity_registry &reg) {
			for (auto &&[_, ent_ptr] : reg.get_entities()) {
				if (ent_ptr->is_marked_for_exporting()) {
					entity *ent = ent_ptr.get();
					if (auto *func_entity = dyn_cast<entities::function_entity>(ent)) {
						_function_names.emplace(func_entity, function_naming::from_entity(*func_entity, *naming));
					} else if (auto *field_entity = dyn_cast<entities::field_entity>(ent)) {
						_field_names.emplace(field_entity, field_naming::from_entity(*field_entity, *naming));
					} else if (auto *enum_entity = dyn_cast<entities::enum_entity>(ent)) {
						_enum_names.emplace(enum_entity, enum_naming::from_entity(*enum_entity, *naming));
					} else if (auto *record_entity = dyn_cast<entities::record_entity>(ent)) {
						_record_names.emplace(record_entity, record_naming::from_entity(*record_entity, *naming));
					}
				}
			}
		}

	protected:
		/// Exports an API enum type.
		void _export_api_enum_type(
			cpp_writer &writer, entities::enum_entity *entity, const enum_naming &name
		) const {
			writer
				.new_line()
				.write("enum ");
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				for (auto &&enumerator : name.enumerators) {
					writer
						.new_line()
						.write_fmt("{} = {}", enumerator.second, enumerator.first)
						.maybe_separate(",");
				}
			}
			writer
				.write(";")
				.new_line()
				.write_fmt(
					"typedef {} {};",
					_get_exported_type_name(entity->get_integer_type(), nullptr),
					name.name
				);
		}
		/// Exports an API type.
		inline static void _export_api_type(cpp_writer &writer, const record_naming &name) {
			writer
				.new_line()
				.write("typedef struct ");
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
			}
			writer.write_fmt(" {};", name.name);
		}

		/// Exports pointers and qualifiers for a field getter return type.
		inline static void _export_api_field_getter_return_type_pointers_and_qualifiers(
			cpp_writer &writer, const qualified_type &type, entities::field_kind kind, bool is_const
		) {
			_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			// const fields already have this const in their qualifiers
			if (kind == entities::field_kind::normal_field && is_const) {
				writer.write("const ");
			}
			// for references, use the pointer directly
			if (kind != entities::field_kind::reference_field) {
				writer.write("*");
			}
		}

		/// Exports the definition of an API function pointer.
		void _export_api_function_pointer_definition(
			cpp_writer &writer, entities::function_entity *entity, const function_naming &name
		) const {
			writer.new_line();
			if (auto &return_type = entity->get_api_return_type()) {
				_export_api_return_type(writer, return_type.value());
			}
			writer.write_fmt("(*{})", name.api_name);
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
		/// Exports the definition of an API class destructor.
		void _export_api_destructor_definition(
			cpp_writer &writer, entities::record_entity*, const record_naming &name
		) const {
			writer
				.new_line()
				.write_fmt("void (*{})({} *);", name.destructor_api_name, name.name);
		}
		/// Exports the definition of API field getters.
		void _export_api_field_getter_definitions(
			cpp_writer &writer, entities::field_entity *entity, const field_naming &name
		) const {
			auto &type = entity->get_type();
			auto parent_it = _record_names.find(entity->get_parent());
			assert_true(parent_it != _record_names.end());

			writer.new_line();

			// only normal fields have non-const getters
			if (entity->get_field_kind() == entities::field_kind::normal_field) {
				writer
					.new_line()
					.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
				_export_api_field_getter_return_type_pointers_and_qualifiers(
					writer, type, entity->get_field_kind(), false
				);
				writer.write_fmt("(*{})({} *)", name.getter_api_name, parent_it->second.name);
			}

			writer
				.new_line()
				.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
			_export_api_field_getter_return_type_pointers_and_qualifiers(
				writer, type, entity->get_field_kind(), true
			);
			writer.write_fmt("(*{})({} const *)", name.const_getter_api_name, parent_it->second.name);
		}
	public:
		/// Exports the API header.
		void export_api_header(std::ostream &out) const {
			cpp_writer writer(out);
			for (auto &&[ent, name] : _enum_names) {
				_export_api_enum_type(writer, ent, name);
			}
			writer.new_line();
			for (auto &&[ent, name] : _record_names) {
				_export_api_type(writer, name);
			}

			writer
				.new_line()
				.new_line()
				.write("typedef struct ");
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				for (auto &&[ent, name] : _function_names) {
					_export_api_function_pointer_definition(writer, ent, name);
				}
				for (auto &&[ent, name] : _record_names) {
					_export_api_destructor_definition(writer, ent, name);
				}
				for (auto &&[ent, name] : _field_names) {
					_export_api_field_getter_definitions(writer, ent, name);
				}
			}
			writer.write_fmt(" {};", naming->api_struct_name);
		}

		/// Exports the host header.
		void export_host_h(std::ostream &out) const {
			cpp_writer writer(out);
			writer
				.write_fmt("struct {};", naming->api_struct_name)
				.new_line()
				.write_fmt("void {}({}&);", naming->api_struct_init_function_name, naming->api_struct_name);
		}

	protected:
		/// Exports the implementation of the given function.
		void _export_function_impl(
			cpp_writer &writer, entities::function_entity *entity, const function_naming &name
		) const {
			writer.new_line();
			if (auto &return_type = entity->get_api_return_type()) {
				_export_api_return_type(writer, return_type.value());
			}
			writer.write_fmt("{}", name.impl_name);
			std::vector<cpp_writer::variable_token> parameters;
			bool complex_return = false; // indicates whether the function call should be wrapped in a placement new
			{
				auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
				for (auto &&param : entity->get_parameters()) {
					writer.new_line();
					_export_api_parameter_type(writer, param.type);
					parameters.emplace_back(writer.allocate_function_parameter(param.name));
					writer
						.write(parameters.back().get())
						.maybe_separate(",");
				}
				if (auto &return_type = entity->get_api_return_type()) {
					if (_maybe_export_api_return_type_input(writer, return_type.value())) {
						parameters.emplace_back(writer.allocate_function_parameter("output"));
						writer.write(parameters.back().get());
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
							"new ({}) {}", parameters.back().get(), _get_internal_type_name(return_type)
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
						if (auto *constructor_ent = dyn_cast<entities::constructor_entity>(method_ent)) {
							// nothing to write
						} else { // if this is a method and not a constructor
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
									param_name_it->get()
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
						.write_fmt("return static_cast<{}*>({});", it->second.name, parameters.back().get());
				}
			}
		}
		/// Exports the implementations of field getters.
		void _export_field_getter_impls(
			cpp_writer &writer, entities::field_entity *entity, const field_naming &name
		) const {
			auto &type = entity->get_type();
			auto parent_it = _record_names.find(entity->get_parent());
			assert_true(parent_it != _record_names.end());

			writer.new_line();
			if (entity->get_field_kind() == entities::field_kind::normal_field) { // non-const getter
				auto input = writer.allocate_function_parameter("object");
				writer
					.new_line()
					.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
				_export_api_field_getter_return_type_pointers_and_qualifiers(
					writer, type, entity->get_field_kind(), false
				);
				writer.write_fmt("{}({} *{}) ", name.getter_impl_name, parent_it->second.name, input.get());
				{
					auto scope = writer.begin_scope(cpp_writer::braces_scope);
					writer
						.new_line()
						.write_fmt(
							"return &reinterpret_cast<{} *>({})->{};",
							_get_internal_entity_name(entity->get_parent()->get_declaration()),
							input.get(), to_string_view(entity->get_declaration()->getName())
						);
				}
			}

			{ // const getter
				auto input = writer.allocate_function_parameter("object");
				writer
					.new_line()
					.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
				_export_api_field_getter_return_type_pointers_and_qualifiers(
					writer, type, entity->get_field_kind(), true
				);
				writer.write_fmt(
					"{}({} const *{}) ", name.const_getter_impl_name, parent_it->second.name, input.get()
				);
				{
					auto scope = writer.begin_scope(cpp_writer::braces_scope);
					writer
						.new_line()
						.write_fmt(
							"return &reinterpret_cast<{} const *>({})->{};",
							_get_internal_entity_name(entity->get_parent()->get_declaration()),
							input.get(), to_string_view(entity->get_declaration()->getName())
						);
				}
			}
		}
		/// Exports the destructor implementation.
		void _export_destructor_impl(
			cpp_writer &writer, entities::record_entity *entity, const record_naming &name
		) const {
			auto input = writer.allocate_function_parameter("object");
			writer
				.new_line()
				.write_fmt("void {}({} *{}) ", name.destructor_impl_name, name.name, input.get());
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				writer
					.new_line()
					.write_fmt(
						"reinterpret_cast<{} *>({})->~{}();",
						_get_internal_entity_name(entity->get_declaration()),
						input.get(), to_string_view(entity->get_declaration()->getName())
					);
			}
		}
	public:
		/// Exports the host CPP file that holds the implementation of all API functions. The user has to manually add
		/// <cc>#include</cc> directives of the host header and the API header.
		void export_host_cpp(std::ostream &out) const {
			cpp_writer writer(out);
			writer.write_fmt("class {} ", APIGEN_API_CLASS_NAME_STR);
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				for (auto &&[ent, name] : _function_names) {
					_export_function_impl(writer, ent, name);
				}
				for (auto &&[ent, name] : _record_names) {
					_export_destructor_impl(writer, ent, name);
				}
				for (auto &&[ent, name] : _field_names) {
					_export_field_getter_impls(writer, ent, name);
				}
			}
			{
				auto result_var = writer.allocate_function_parameter("result");
				writer
					.write(";")
					.new_line()
					.new_line()
					.write_fmt(
						"void {}({} &{}) ",
						naming->api_struct_init_function_name,
						naming->api_struct_name,
						result_var.get()
					);
				{
					auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
					for (auto &&[func, name] : _function_names) {
						writer
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var.get(), name.api_name, APIGEN_API_CLASS_NAME_STR, name.impl_name
							);
					}
					for (auto &&[record, name] : _record_names) {
						writer
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var.get(), name.destructor_api_name,
								APIGEN_API_CLASS_NAME_STR, name.destructor_impl_name
							);
					}
					for (auto &&[field, name] : _field_names) {
						if (field->get_field_kind() == entities::field_kind::normal_field) {
							writer
								.new_line()
								.write_fmt(
									"{}.{} = {}::{};",
									result_var.get(), name.getter_api_name,
									APIGEN_API_CLASS_NAME_STR, name.getter_impl_name
								);
						}
						writer
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var.get(), name.const_getter_api_name,
								APIGEN_API_CLASS_NAME_STR, name.const_getter_impl_name
							);
					}
				}
			}
		}

		clang::PrintingPolicy internal_printing_policy; ///< The \p clang::PrintingPolicy of internal classes.
		naming_convention *naming = nullptr; ///< The naming convention of exported types and functions.
	protected:
		/// Mapping between functions and their exported names.
		std::unordered_map<entities::function_entity*, function_naming> _function_names;
		/// Mapping between enums and their exported names.
		std::unordered_map<entities::enum_entity*, enum_naming> _enum_names;
		/// Mapping between records and their exported names.
		std::unordered_map<entities::record_entity*, record_naming> _record_names;
		/// Mapping between fields and their exported names.
		std::unordered_map<entities::field_entity*, field_naming> _field_names;

		// exporting of types
		/// Returns the name of a type used in the API header.
		std::string_view _get_exported_type_name(const clang::Type *type, entity *entity) const {
			if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
				// still use the policy for internal names so that the code compiles without problems
				return to_string_view(builtin->getName(internal_printing_policy));
			}
			if (auto *enumty = llvm::dyn_cast<clang::EnumType>(type)) {
				auto it = _enum_names.find(cast<entities::enum_entity>(entity));
				assert_true(it != _enum_names.end());
				return it->second.name;
			}
			if (auto *recty = llvm::dyn_cast<clang::RecordType>(type)) {
				auto it = _record_names.find(cast<entities::record_entity>(entity));
				assert_true(it != _record_names.end());
				return it->second.name;
			}
			return "$UNSUPPORTED";
		}
		/// Returns the internal name of an operator name.
		inline static std::string_view _get_internal_operator_name(clang::OverloadedOperatorKind kind) {
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
		/// Returns the spelling of the given \p clang::TemplateArgument.
		[[nodiscard]] std::string _get_template_argument_spelling(const clang::TemplateArgument &arg) const {
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
		/// Returns the spelling of a whole template argument list, excluding angle brackets.
		[[nodiscard]] std::string _get_template_argument_list_spelling(
			llvm::ArrayRef<clang::TemplateArgument> args
		) const {
			std::string result;
			for (auto &arg : args) {
				if (!result.empty()) {
					result += ", ";
				}
				result += _get_template_argument_spelling(arg);
			}
			return result;
		}
		/// Returns the internal name of a function or a type.
		std::string _get_internal_entity_name(clang::DeclContext *decl) const {
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
		/// \overload
		std::string _get_internal_type_name(const clang::Type *type) const {
			if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
				return std::string(to_string_view(builtin->getName(internal_printing_policy)));
			}
			if (auto *tagty = llvm::dyn_cast<clang::TagType>(type)) {
				return _get_internal_entity_name(tagty->getAsTagDecl());
			}
			return "$UNSUPPORTED";
		}
		/// Wrapper around \ref _write_internal_pointer_and_qualifiers() that returns the result as a string.
		inline static std::string _get_internal_pointer_and_qualifiers(
			reference_kind ref, const std::vector<qualifier> &quals
		) {
			std::stringstream ss;
			_write_internal_pointer_and_qualifiers(ss, ref, quals);
			return ss.str();
		}
		/// Writes asterisks, amperesands, and qualifiers for an internal type to the given stream.
		inline static void _write_internal_pointer_and_qualifiers(
			std::ostream &out, reference_kind ref, const std::vector<qualifier> &quals
		) {
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
			for (auto it = ++quals.begin(); it != quals.end(); ++it) {
				out << "*" << *it;
			}
		}
		/// Exports asterisks and qualifiers for an exported type.
		inline static void _export_api_pointer_and_qualifiers(
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

		/// Exports a type as a parameter in the API header.
		void _export_api_parameter_type(cpp_writer &writer, const qualified_type &type) const {
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
			if (type.is_reference_or_pointer()) {
				_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			} else {
				if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
					if (!complex_ty->has_move_constructor()) {
						writer.write("const "); // this parameter will be copied
					}
					writer.write("*");
				} // nothing to do for other primitive types
			}
		}
		/// Exports a type as a return type in the API.
		void _export_api_return_type(cpp_writer &writer, const qualified_type &type) const {
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
			if (type.is_reference_or_pointer()) {
				_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			} else {
				if (auto *entity = dyn_cast<entities::record_entity>(type.type_entity)) {
					writer.write('*');
				}
			}
		}
		/// Exports the additional input for a return value in the API, if one is necessary.
		///
		/// \return Whether an additional input is necessary.
		bool _maybe_export_api_return_type_input(cpp_writer &writer, const qualified_type &type) const {
			if (!type.is_reference_or_pointer()) {
				if (auto *entity = dyn_cast<entities::record_entity>(type.type_entity)) {
					writer
						.new_line()
						.write("void *");
					return true;
				}
			}
			return false;
		}

		/// Exports the code used to pass a parameter.
		void _export_pass_parameter(
			cpp_writer &writer, const qualified_type &type, const cpp_writer::variable_token &param
		) const {
			if (type.is_reference_or_pointer()) {
				// TODO this may not work for enums, etc.
				if (type.is_reference()) {
					writer.write('*');
				}
				writer.write_fmt("reinterpret_cast<{} ", _get_internal_type_name(type.type));
				_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
				writer.write_fmt(">({})", param.get());
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
					writer.write_fmt("*>({})", param.get());
				} else { // primitive types
					writer.write(param.get()); // pass directly
				}
			}
		}
	};
}
