#pragma once

/// \file
/// Contains the \ref apigen::entities::record_entity class.

#include <clang/AST/DeclCXX.h>

#include "../entity.h"
#include "../types.h"
#include "user_type_entity.h"


namespace apigen::entities {
	class record_entity;

	/// Custom function used to create a \p std::function from a function pointer.
	class std_function_custom_function_entity : public custom_function_entity {
	public:
		/// Initializes all fields of this entity.
		explicit std_function_custom_function_entity(record_entity&);

		/// Marks the return type and parameter types as dependencies.
		void gather_dependencies(entity_registry&, dependency_analyzer&);

		/// Returns the name of the function type's constructor.
		naming_convention::name_info get_suggested_name(naming_convention&, const exporter&) const override;
		/// Exports the declaration of the function pointer of this conversion function.
		void export_pointer_declaration(cpp_writer&, const exporter&, std::string_view) const override;
		/// Exports the definition of the conversion function.
		void export_definition(cpp_writer&, const exporter&, std::string_view) const override;
	protected:
		qualified_type _return_type; ///< The return type.
		std::vector<qualified_type> _param_types; ///< Parameter types.
		record_entity &_entity; ///< The associated \ref record_entity.
		/// The type of the function (as the template parameter of \p std::function).
		const clang::FunctionProtoType *_func_type = nullptr;

		/// Exports the parameter list of the function pointer, including the parentheses.
		void _export_function_pointer_parameters(cpp_writer&, const exporter&, bool) const;
		/// Exports the call to the function pointer with conversions for all parameters.
		void _export_function_call(
			cpp_writer&, const exporter&,
			std::string_view fptr, const std::vector<std::string>&, std::string_view output, std::string_view user
		) const;
		/// Exports a parameter type of the function pointer.
		void _export_parameter_type(cpp_writer&, const exporter&, const qualified_type&, bool) const;
	};

	/// Custom function used to perform \p dynamic_cast operations.
	class dynamic_cast_custom_function_entity : public custom_function_entity {
	public:
		/// Initializes \ref _entity and \ref _base_type.
		dynamic_cast_custom_function_entity(record_entity &ent, record_entity &base, bool);

		/// Marks the base class as a dependency.
		void gather_dependencies(entity_registry&, dependency_analyzer&);

		/// Returns the suggested name of this \p dynamic_cast function.
		naming_convention::name_info get_suggested_name(naming_convention&, const exporter&) const override;
		/// Exports the declaration of the function pointer.
		void export_pointer_declaration(cpp_writer&, const exporter&, std::string_view) const override;
		/// Exports the definition of this function.
		void export_definition(cpp_writer&, const exporter&, std::string_view) const override;
	protected:
		record_entity
			&_entity, ///< The entity.
			&_base_type; ///< The base type to cast from.
		bool _const = false; ///< Indicates whether this dynamic cast function is for const objects.

		/// Returns a const qualifier if \ref _const is \p true.
		std::string_view _get_qualifier() const {
			return _const ? " const" : "";
		}
		/// Returns the exported name of the given \ref record_entity.
		std::string_view _get_record_name(const exporter&, record_entity&) const;
	};

	/// An entity that corresponds to a \p class or a \p struct.
	class record_entity : public user_type_entity {
	public:
		constexpr static entity_kind kind = entity_kind::record; ///< The type of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit record_entity(clang::CXXRecordDecl *decl) : user_type_entity(), _decl(decl) {
		}

		/// Gathers all dependencies for this record type.
		void gather_dependencies(entity_registry&, dependency_analyzer&) override;

		/// Handles the \p apigen_recursive attribute.
		bool handle_attribute(std::string_view anno) override {
			if (anno == APIGEN_ANNOTATION_PRIVATE_EXPORT) {
				_private_export = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_RECURSIVE) {
				_recursive = true;
				return true;
			}
			return entity::handle_attribute(anno);
		}

		/// Returns whether or not private members of this class can be exported.
		[[nodiscard]] bool export_private_members() const {
			// TODO private members that are explicitly marked as export will still be exported
			return _private_export;
		}
		/// Returns whether or not this class has a viable move constructor.
		[[nodiscard]] bool has_move_constructor() const {
			return _move_constructor;
		}

		/// Returns \p true if this class is \p std::function.
		[[nodiscard]] bool is_std_function() const {
			if (to_string_view(get_declaration()->getName()) == "function") {
				if (const clang::DeclContext *parent = get_declaration()->getParent()) {
					if (parent->isStdNamespace()) {
						return true;
					}
				}
			}
			return false;
		}

		/// Returns the declaration of this entity.
		[[nodiscard]] clang::CXXRecordDecl *get_declaration() const {
			return _decl;
		}
		/// Returns \ref _decl.
		[[nodiscard]] clang::NamedDecl *get_generic_declaration() const override {
			return _decl;
		}

		/// Checks if the given constructor is a move constructor.
		[[nodiscard]] static bool is_move_constructor(clang::CXXConstructorDecl*);
	protected:
		clang::CXXRecordDecl *_decl = nullptr; ///< The declaration of this entity.
		bool
			_move_constructor = false, ///< Indicates whether or not this class has a viable move constructor.
			_recursive = false, ///< Indicates whether members of this record should be exported.
			_private_export = false; ///< Whether or not to export private members.
	};
}
