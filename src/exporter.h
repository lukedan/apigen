#pragma once

/// \file
/// Used to generate the exported code.

#include <unordered_map>
#include <sstream>
#include <variant>

#include <fmt/ostream.h> // TODO C++20

#include "entity_kinds/function_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/user_type_entity.h"
#include "entity_registry.h"
#include "cpp_writer.h"
#include "naming_convention.h"
#include "parser.h"

namespace apigen {
	/// Used to gather and export all entities.
	class exporter {
	public:
		/// A name of a variable, class, enum, etc.
		struct cached_name {
		public:
			/// Default constructor.
			cached_name() = default;
			/// Initializes this name using the given \ref name_allocator::token.
			explicit cached_name(name_allocator::token tok) :
				_data(std::in_place_type<name_allocator::token>, std::move(tok)) {
			}

			/// Concatenates the parts of the name and caches it for later use.
			void freeze() {
				if (std::holds_alternative<name_allocator::token>(_data)) {
					std::string str = std::get<name_allocator::token>(_data)->get_name();
					_data.emplace<std::string>(std::move(str));
				}
			}
			/// Returns the name. This should only be called after this name has been frozen.
			[[nodiscard]] std::string_view get_cached() const {
				return std::get<std::string>(_data);
			}

			/// Registers the given name to the \ref name_allocator, and returns the resulting \ref cached_name.
			[[nodiscard]] inline static cached_name register_name(
				name_allocator &alloc, naming_convention::name_info name
			) {
				return cached_name(alloc.allocate_variable_custom(
					std::move(name.name), std::move(name.disambiguation)
				));
			}
			/// Similar to \ref register_name(), but with a prefix.
			[[nodiscard]] inline static cached_name register_name_prefix(
				name_allocator &alloc, std::string_view prefix, naming_convention::name_info name
			) {
				return cached_name(alloc.allocate_variable_prefix(
					prefix, std::move(name.name), std::move(name.disambiguation)
				));
			}
		protected:
			std::variant<std::string, name_allocator::token> _data; ///< The data of this name.
		};

		struct function_naming {
			cached_name
				api_name, ///< The name of the exported function pointer.
				impl_name; ///< The name of the function that is the internal implementation.

			/// Constructs a \ref function_naming from the given \ref entities::field_entity.
			inline static function_naming from_entity(
				entities::function_entity &ent, naming_convention &conv,
				name_allocator &global_scope, name_allocator &impl_scope
			) {
				function_naming result;
				auto name = conv.get_function_name_dynamic(ent);
				result.impl_name = cached_name::register_name_prefix(impl_scope, "internal_", name);
				result.api_name = cached_name::register_name(global_scope, std::move(name));
				return result;
			}
		};
		/// Contains naming information of an \ref entities::field_entity.
		struct field_naming {
			cached_name
				getter_api_name, ///< The exported name of the getter.
				getter_impl_name, ///< The name of the getter's internal implementation.
				const_getter_api_name, ///< The exported name of the const getter.
				const_getter_impl_name; ///< The name of the const getter's internal implementation.

			/// Constructs a \ref field_naming from the given \ref entities::field_entity.
			inline static field_naming from_entity(
				entities::field_entity &ent, naming_convention &conv,
				name_allocator &api_table_scope, name_allocator &impl_scope
			) {
				field_naming result;
				if (ent.get_field_kind() == entities::field_kind::normal_field) {
					auto name = conv.get_field_getter_name(ent);
					result.getter_impl_name = cached_name::register_name_prefix(impl_scope, "internal_", name);
					result.getter_api_name = cached_name::register_name(api_table_scope, std::move(name));
				}
				auto name = conv.get_field_const_getter_name(ent);
				result.const_getter_impl_name = cached_name::register_name_prefix(impl_scope, "internal_", name);
				result.const_getter_api_name = cached_name::register_name(api_table_scope, std::move(name));
				return result;
			}
		};
		/// Contains naming information of an \ref entities::enum_entity.
		struct enum_naming {
			cached_name name; ///< The name of this enum definition.
			std::vector<std::pair<std::int64_t, cached_name>> enumerators; ///< The name of all enumerators.

			/// Constructs a \ref enum_naming from the given \ref entities::enum_entity.
			inline static enum_naming from_entity(
				entities::enum_entity &ent, naming_convention &conv, name_allocator &global_scope
			) {
				enum_naming result;
				result.name = cached_name::register_name(global_scope, conv.get_enum_name(ent));
				for (clang::EnumConstantDecl *enumerator : ent.get_declaration()->enumerators()) {
					result.enumerators.emplace_back(
						enumerator->getInitVal().getExtValue(),
						cached_name::register_name(global_scope, conv.get_enumerator_name(ent, enumerator))
					);
				}
				return result;
			}
		};
		/// Contains naming information of an \ref entities::record_entity.
		struct record_naming {
			cached_name
				name, ///< The name of the exported record reference type.
				destructor_api_name, ///< The exported name of the destructor.
				destructor_impl_name; ///< The name of the internal implementation of the function.

			/// Constructs a \ref record_naming from the given \ref entities::record_entity.
			inline static record_naming from_entity(
				entities::record_entity &ent, naming_convention &conv,
				name_allocator &global_scope, name_allocator &api_table_scope, name_allocator &impl_scope
			) {
				record_naming result;
				result.name = cached_name::register_name(global_scope, conv.get_record_name(ent));
				auto name = conv.get_record_destructor_name(ent);
				result.destructor_impl_name = cached_name::register_name_prefix(impl_scope, "internal_", name);
				result.destructor_api_name = cached_name::register_name(api_table_scope, std::move(name));
				return result;
			}
		};

		/// Initializes \ref internal_printing_policy.
		explicit exporter(clang::PrintingPolicy policy) :
			internal_printing_policy(std::move(policy)), _impl_scope(name_allocator::from_parent(_global_scope)) {
		}
		/// Initializes \ref internal_printing_policy and \ref naming.
		exporter(clang::PrintingPolicy policy, naming_convention &conv) : exporter(std::move(policy)) {
			naming = &conv;
		}

		/// Collects exported entities from the given \ref entity_registry.
		void collect_exported_entities(entity_registry &reg) {
			name_allocator api_table_scope = name_allocator::from_parent(_global_scope);
			for (auto &&[_, ent_ptr] : reg.get_entities()) {
				if (ent_ptr->is_marked_for_exporting()) {
					entity *ent = ent_ptr.get();
					if (auto *func_entity = dyn_cast<entities::function_entity>(ent)) {
						_function_names.emplace(func_entity, function_naming::from_entity(
							*func_entity, *naming, _global_scope, _impl_scope
						));
					} else if (auto *field_entity = dyn_cast<entities::field_entity>(ent)) {
						_field_names.emplace(field_entity, field_naming::from_entity(
							*field_entity, *naming, api_table_scope, _impl_scope
						));
					} else if (auto *enum_entity = dyn_cast<entities::enum_entity>(ent)) {
						_enum_names.emplace(enum_entity, enum_naming::from_entity(
							*enum_entity, *naming, _global_scope
						));
					} else if (auto *record_entity = dyn_cast<entities::record_entity>(ent)) {
						_record_names.emplace(record_entity, record_naming::from_entity(
							*record_entity, *naming, _global_scope, api_table_scope, _impl_scope
						));
					}
				}
			}

			// now freeze all names
			for (auto &[ent, name] : _function_names) {
				name.api_name.freeze();
				name.impl_name.freeze();
			}
			for (auto &[ent, name] : _enum_names) {
				name.name.freeze();
				for (auto &[val, enum_name] : name.enumerators) {
					enum_name.freeze();
				}
			}
			for (auto &[ent, name] : _record_names) {
				name.name.freeze();
				name.destructor_api_name.freeze();
				name.destructor_impl_name.freeze();
			}
			for (auto &[ent, name] : _field_names) {
				name.getter_impl_name.freeze();
				name.getter_api_name.freeze();
				name.const_getter_impl_name.freeze();
				name.const_getter_api_name.freeze();
			}
		}

	protected:
		/// Exports an API enum type.
		void _export_api_enum_type(cpp_writer&, entities::enum_entity*, const enum_naming&) const;
		/// Exports an API type.
		static void _export_api_type(cpp_writer&, const record_naming&);
		/// Exports the definition of an API function pointer.
		void _export_api_function_pointer_definition(
			cpp_writer&, entities::function_entity*, const function_naming&
		) const;
		/// Exports the definition of an API class destructor.
		void _export_api_destructor_definition(cpp_writer&, entities::record_entity*, const record_naming&) const;
		/// Exports the definition of API field getters.
		void _export_api_field_getter_definitions(cpp_writer&, entities::field_entity*, const field_naming&) const;
	public:
		/// Exports the API header.
		void export_api_header(std::ostream&) const;

		/// Exports the host header.
		void export_host_h(std::ostream&) const;

	protected:
		/// Exports the implementation of the given function.
		void _export_function_impl(cpp_writer&, entities::function_entity*, const function_naming&) const;
		/// Exports the implementations of field getters.
		void _export_field_getter_impls(cpp_writer&, entities::field_entity*, const field_naming&) const;
		/// Exports the destructor implementation.
		void _export_destructor_impl(cpp_writer&, entities::record_entity*, const record_naming&) const;
	public:
		/// Exports the host CPP file that holds the implementation of all API functions. The user has to manually add
		/// <cc>#include</cc> directives of the host header and the API header.
		void export_host_cpp(std::ostream&) const;

		/// Exports a \p cpp file that collects the sizes and alignments of data structures when ran. The user needs to
		/// manually add <cc>#include</cc> directives to the fromt of the output file.
		void export_data_collection_cpp(std::ostream&) const;

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
		name_allocator
			_global_scope, ///< The \ref name_allocator for the global scope.
			_impl_scope; ///< The \ref name_allocator for the scope that contain API implementations.

		/// Returns the internal name of an operator name.
		[[nodiscard]] static std::string_view _get_internal_operator_name(clang::OverloadedOperatorKind);

		// exporting of internal types
		/// Returns the spelling of the given \p clang::TemplateArgument.
		[[nodiscard]] std::string _get_template_argument_spelling(const clang::TemplateArgument&) const;
		/// Returns the spelling of a whole template argument list, excluding angle brackets.
		[[nodiscard]] std::string _get_template_argument_list_spelling(llvm::ArrayRef<clang::TemplateArgument>) const;
		/// Returns the internal name of a function or a type.
		[[nodiscard]] std::string _get_internal_entity_name(clang::DeclContext*) const;
		/// Handles \p clang::BuiltinType for \ref _get_internal_entity_name().
		[[nodiscard]] std::string _get_internal_type_name(const clang::Type*) const;

		/// Writes asterisks, amperesands, and qualifiers for an internal type to the given stream.
		static void _write_internal_pointer_and_qualifiers(
			std::ostream&, reference_kind, const std::vector<qualifier>&
		);
		/// Wrapper around \ref _write_internal_pointer_and_qualifiers() that returns the result as a string.
		[[nodiscard]] static std::string _get_internal_pointer_and_qualifiers(
			reference_kind, const std::vector<qualifier>&
		);

		// exporting of external types
		/// Returns the name of a type used in the API header.
		[[nodiscard]] std::string_view _get_exported_type_name(const clang::Type*, entity*) const;
		/// Exports a type as a parameter in the API header.
		void _export_api_parameter_type(cpp_writer&, const qualified_type&) const;
		/// Exports a type as a return type in the API.
		void _export_api_return_type(cpp_writer&, const qualified_type&) const;
		/// Exports pointers and qualifiers for a field getter return type.
		static void _export_api_field_getter_return_type_pointers_and_qualifiers(
			cpp_writer&, const qualified_type&, entities::field_kind, bool is_const
		);
		/// Exports the additional input for a return value in the API, if one is necessary.
		///
		/// \return Whether an additional input is necessary.
		bool _maybe_export_api_return_type_input(cpp_writer&, const qualified_type&) const;

		/// Exports asterisks and qualifiers for an exported type.
		static void _export_api_pointers_and_qualifiers(cpp_writer &writer, reference_kind ref, const std::vector<qualifier> &quals);


		/// Exports the code used to pass a parameter.
		void _export_pass_parameter(cpp_writer&, const qualified_type&, std::string_view) const;
	};
}
