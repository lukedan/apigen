#pragma once

/// \file
/// Used to generate the exported code.

#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <fmt/ostream.h> // TODO C++20

#include "entity_kinds/function_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/user_type_entity.h"
#include "entity_registry.h"
#include "naming_convention.h"

namespace apigen {
	/// A wrapper around \p std::ostream that provides additional functionalities for printing C++ code.
	class cpp_writer {
	public:
		/// Represents a scope.
		struct scope {
			/// Initializes \ref begin and \ref end.
			constexpr scope(std::string_view b, std::string_view e) noexcept : begin(b), end(e) {
			}

			std::string_view
				begin, ///< The opening sequence of this scope.
				end; ///< The closing sequence of this scope.
		};

		inline const static scope
			parentheses_scope{"(", ")"}, ///< A scope surrounded by parentheses.
			braces_scope{"{", "}"}; ///< A scope surrounded by brackets.

		/// Initializes \ref _out.
		explicit cpp_writer(std::ostream &out) : _out(out) {
		}
		/// No move construction.
		cpp_writer(cpp_writer&&) = delete;
		/// No copy constructor.
		cpp_writer(const cpp_writer&) = delete;
		/// No move assignment.
		cpp_writer &operator=(cpp_writer&&) = delete;
		/// No copy assignment.
		cpp_writer &operator=(const cpp_writer&) = delete;

		/// RTTI wrapper for scopes.
		struct scope_token {
			friend cpp_writer;
		public:
			/// Default constructor.
			scope_token() = default;
			/// Move constructor.
			scope_token(scope_token &&other) noexcept : _out(other._out) {
			}
			/// No copy construction.
			scope_token(const scope_token&) = delete;
			/// Move assignment.
			scope_token &operator=(scope_token &&other) noexcept {
				end();
				_out = other._out;
				other._out = nullptr;
				return *this;
			}
			/// No copy assignment.
			scope_token &operator=(const scope_token&) = delete;
			/// Calls \ref end().
			~scope_token() {
				end();
			}

			/// Ends the current scope.
			void end() {
				if (_out) {
					_out->_end_scope();
					_out = nullptr;
				}
			}
		protected:
			/// Initializes \ref _out.
			explicit scope_token(cpp_writer *w) : _out(w) {
			}

			cpp_writer *_out = nullptr; ///< The associated \ref cpp_writer.
		};
		/// RTTI wrapper for allocated variable names.
		struct variable_token {
			friend cpp_writer;
		public:
			/// Default constructor.
			variable_token() = default;
			/// Move constructor.
			variable_token(variable_token &&src) noexcept : _iter(src._iter), _writer(src._writer) {
				src._writer = nullptr;
			}
			/// No copy construction.
			variable_token(const variable_token&) = delete;
			/// Move assignment.
			variable_token &operator=(variable_token &&src) noexcept {
				discard();
				_iter = src._iter;
				_writer = src._writer;
				src._writer = nullptr;
				return *this;
			}
			/// No copy assignment.
			variable_token &operator=(const variable_token&) = delete;
			/// Calls \ref discard() automatically.
			~variable_token() {
				discard();
			}

			/// Returns whether this variable token is empty.
			[[nodiscard]] bool empty() const {
				return _writer == nullptr;
			}
			/// Returns the variable name.
			[[nodiscard]] std::string_view get() const {
				if (_writer) {
					return *_iter;
				}
				return "";
			}

			/// Discards this variable.
			void discard() {
				if (_writer) {
					_writer->_discard_variable(_iter);
					_writer = nullptr;
				}
			}
		protected:
			/// Directly initializes the fields of this struct.
			variable_token(cpp_writer &w, std::set<std::string>::const_iterator it) : _iter(it), _writer(&w) {
			}

			std::set<std::string>::const_iterator _iter; ///< The iterator to the underlying allocated name.
			cpp_writer *_writer = nullptr; ///< The associated \ref cpp_writer object.
		};

		// variable management
		/// Tries to register a variable with the given name. A postfix will be appended to the name if there are
		/// conflicts.
		variable_token allocate_variable_custom(std::string name) {
			auto it = _vars.lower_bound(name);
			if (it != _vars.end() && *it == name) {
				name += '_';
				for (size_t i = 2; ; ++i) {
					std::string num = std::to_string(i);
					name += num;
					it = _vars.lower_bound(name);
					if (it == _vars.end() || *it != name) {
						break;
					}
					name.erase(name.size() - num.size());
				}
			}
			auto result = _vars.emplace_hint(it, std::move(name));
			return variable_token(*this, result);
		}
		/// Tries to register a variable name with a prefix.
		variable_token allocate_variable_prefix(const char *prefix, std::string name) {
			if (name.empty()) {
				name = "unnamed";
			}
			return allocate_variable_custom(prefix + std::move(name));
		}

		/// Allocates the name for a function parameter.
		variable_token allocate_function_parameter(std::string name) {
			return allocate_variable_prefix("_apigen_priv_param_", std::move(name));
		}
		/// Allocates the name for a local variable.
		variable_token allocate_local_variable(std::string name) {
			return allocate_variable_prefix("_apigen_priv_local_", std::move(name));
		}

		// low-level output formatting
		/// Directly writes the given object to the output.
		template <typename T> cpp_writer &write(T &&obj) {
			_maybe_print_sepearator();
			_out << std::forward<T>(obj);
			return *this;
		}
		/// Uses \p fmt to format and write to the output stream.
		template <typename ...Args> cpp_writer &write_fmt(Args &&...args) {
			_maybe_print_sepearator();
			fmt::print(_out, std::forward<Args>(args)...);
			return *this;
		}
		/// Starts a new line.
		cpp_writer &new_line() {
			write("\n");
			for (size_t i = 0; i < _scopes.size(); ++i) {
				write("\t");
			}
			for (_scope_rec &scp : _scopes) {
				scp.has_newline = true;
			}
			return *this;
		}
		/// Pushes a scope onto \ref _scopes and starts that scope.
		[[nodiscard]] scope_token begin_scope(scope s) {
			_scopes.emplace_back(s);
			write(s.begin);
			return scope_token(this);
		}
		/// Adds a pending separator to the writer. If the next operation is \ref _end_scope(), this separator will be
		/// discarded; otherwise it will be written to the output before anything else.
		cpp_writer &maybe_separate(std::string_view text) {
			_maybe_print_sepearator();
			_separator = text;
			return *this;
		}
	protected:
		/// Records the state of a scope.
		struct _scope_rec {
			/// Default constructor.
			_scope_rec() = default;
			/// Records the given scope.
			explicit _scope_rec(scope s) : end(s.end) {
			}

			std::string_view end; ///< The end delimiter of this scope.
			bool has_newline = false; ///< Whether or not a new line has been written in this scope.
		};

		/// Prints and clears the separator if it's not empty.
		void _maybe_print_sepearator() {
			if (!_separator.empty()) {
				_out << _separator;
				_separator = std::string_view();
			}
		}
		/// Ends the bottom-level scope.
		void _end_scope() {
			_separator = std::string_view(); // discard separator
			_scope_rec ended = _scopes.back();
			_scopes.pop_back();
			if (ended.has_newline) {
				new_line();
			}
			write(ended.end);
		}

		/// Removes the allocated variable name from \ref _vars.
		void _discard_variable(std::set<std::string>::const_iterator iter) {
			_vars.erase(iter);
		}

		std::vector<_scope_rec> _scopes; ///< All current scopes.
		std::set<std::string> _vars; ///< All registered variable names.
		std::string_view _separator; ///< The pending separator.
		std::ostream &_out; ///< The output.
	};

	/// Used to gather and export all entities.
	class exporter {
	public:
		// TODO: resolve overload names
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
				result.getter_api_name = conv.get_field_getter_name(ent);
				result.getter_impl_name = "internal_" + result.getter_api_name;
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
				result.destructor_api_name = conv.get_record_destructor_api_name(ent);
				result.destructor_impl_name = "internal_" + result.destructor_api_name;
				return result;
			}
		};

		/// Default constructor.
		exporter() = default;
		/// Initializes \ref naming.
		explicit exporter(naming_convention &conv) : naming(&conv) {
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
		inline static void _export_api_enum_type(cpp_writer &writer, const enum_naming &name) {
			writer
				.new_line()
				.write("typedef enum ");
			{
				auto scope = writer.begin_scope(cpp_writer::braces_scope);
				for (auto &&enumerator : name.enumerators) {
					writer
						.new_line()
						.write_fmt("{} = {}", enumerator.second, enumerator.first)
						.maybe_separate(",");
				}
			}
			writer.write_fmt(" {};", name.name);
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
		/// Exports the definition of API field getters.
		void _export_api_field_getter_definitions(
			cpp_writer &writer, entities::field_entity *entity, const field_naming &name
		) const {
			auto &type = entity->get_type();
			auto parent_it = _record_names.find(entity->get_parent());
			assert_true(parent_it != _record_names.end());

			writer
				.new_line()
				.new_line()
				.write_fmt("{} *", _get_exported_type_name(type.type, type.type_entity));
			_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			writer.write_fmt("(*{})({} *)", name.getter_api_name, parent_it->second.name);

			writer
				.new_line()
				.write_fmt("{} const *", _get_exported_type_name(type.type, type.type_entity));
			_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			writer.write_fmt("(*{})({} const *)", name.const_getter_api_name, parent_it->second.name);
		}
	public:
		/// Exports the API header.
		void export_api_header(std::ostream &out) const {
			cpp_writer writer(out);
			for (auto &&[ent, name] : _enum_names) {
				_export_api_enum_type(writer, name);
			}
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
						auto *return_type = entity->get_api_return_type().value().type->getAsTagDecl();
						writer.write_fmt(
							"new ({}) {}", parameters.back().get(), return_type->getQualifiedNameAsString()
						);
						scope1 = writer.begin_scope(cpp_writer::parentheses_scope);
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
								writer.write_fmt("{}::", decl->getQualifiedNameAsString());
							} else { // non-static, export member function call
								assert_true(param_it->type.qualifiers.size() == 2);
								assert_true(param_it->type.ref_kind == reference_kind::none);
								writer.write_fmt(
									"reinterpret_cast<{} {}*>({})->",
									decl->getQualifiedNameAsString(),
									param_it->type.qualifiers.back(),
									param_name_it->get()
								);
								++param_it;
								++param_name_it;
							}
							writer.write(to_string_view(method_decl->getName()));
						}
					} else { // normal function, print function name
						writer.write(entity->get_declaration()->getQualifiedNameAsString());
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
					for (auto &&[field, name] : _field_names) {
						writer
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var.get(), name.getter_api_name,
								APIGEN_API_CLASS_NAME_STR, name.getter_impl_name
							)
							.new_line()
							.write_fmt(
								"{}.{} = {}::{};",
								result_var.get(), name.const_getter_api_name,
								APIGEN_API_CLASS_NAME_STR, name.const_getter_impl_name
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
				}
			}
		}

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
			clang::PrintingPolicy policy{clang::LangOptions()}; // TODO HACK

			if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
				return to_string_view(builtin->getName(policy));
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
		/// Returns the internal name of a type.
		std::string _get_internal_type_name(const clang::Type *type) const {
			clang::PrintingPolicy policy{clang::LangOptions()}; // TODO HACK

			if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
				return std::string(to_string_view(builtin->getName(policy)));
			}
			if (auto *tagty = llvm::dyn_cast<clang::TagType>(type)) {
				return tagty->getAsTagDecl()->getQualifiedNameAsString();
			}
			return "$UNSUPPORTED";
		}
		/// Exports asterisks and qualifiers for an exported type.
		void _export_api_pointer_and_qualifiers(
			cpp_writer &writer, reference_kind ref, const std::vector<qualifier> &quals
		) const {
			writer.write(quals.front());
			if (ref != reference_kind::none) {
				writer.write('*');
			}
			for (auto it = ++quals.begin(); it != quals.end(); ++it) {
				writer.write_fmt("*{}", *it);
			}
		}

		/// Exports a type as a parameter in the API header.
		void _export_api_parameter_type(cpp_writer &writer, const qualified_type &type) const {
			writer.write_fmt("{} ", _get_exported_type_name(type.type, type.type_entity));
			if (type.is_reference_or_pointer()) {
				_export_api_pointer_and_qualifiers(writer, type.ref_kind, type.qualifiers);
			} else {
				if (auto *complex_ty = dyn_cast<entities::record_entity>(type.type_entity)) {
					auto *decl = complex_ty->get_declaration();
					// TODO copy & move constructor
				}
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
					writer.write("void *");
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
				// TODO
			}
		}
	};
}
