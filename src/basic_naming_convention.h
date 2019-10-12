#pragma once

/// \file
/// Implementation of \ref apigen::basic_naming_convention.

#include "naming_convention.h"

namespace apigen {
	/// A basic naming convention that uses the given separator to separate scopes.
	class basic_naming_convention : public naming_convention {
	public:
		/// Returns the function name.
		[[nodiscard]] std::string get_function_name(const entities::function_entity &entity) const override {
			return "$TODO";
		}
		/// Returns the method name.
		[[nodiscard]] std::string get_method_name(const entities::method_entity &entity) const override {
			return "$TODO";
		}
		/// Returns the constructor name.
		[[nodiscard]] std::string get_constructor_name(const entities::constructor_entity &entity) const override {
			return "$TODO";
		}

		/// Returns the type name.
		[[nodiscard]] std::string get_user_type_name(const entities::user_type_entity &entity) const override {
			return "$TODO";
		}

		/// Returns the exported name of the destructor of the given \ref entities::record_entity.
		[[nodiscard]] std::string get_record_destructor_api_name(
			const entities::record_entity &entity
		) const override {
			return "$TODO";
		}

		/// Returns the name of an enumerator in the enum declaration.
		[[nodiscard]] std::string get_enumerator_name(
			const entities::enum_entity &entity, clang::EnumConstantDecl *enumerator
		) const override {
			return "$TODO";
		}

		/// Returns the exported name of the non-const getter of the given field.
		[[nodiscard]] std::string get_field_getter_name(const entities::field_entity &entity) const override {
			return "$TODO";
		}
		/// Returns the exportedname of the const getter of the given field.
		[[nodiscard]] std::string get_field_const_getter_name(const entities::field_entity &entity) const override {
			return "$TODO";
		}

		special_function_naming func_naming; ///< Naming information of overloaded operators.
		std::string_view
			scope_separator{"_"}, ///< Separator between scopes.
			template_args_begin{"_"}, ///< Separates class names and template argument lists.
			template_args_end{""}, ///< Appended after template argument lists.
			template_arg_separator{"_"}; ///< Separator between template arguments.
		entity_registry *entities = nullptr; ///< All registered entities.
	protected:
		/// Cached names of declarations, without its parent scopes.
		std::map<clang::NamedDecl*, std::string> _decl_self_names;
		std::map<clang::NamedDecl*, std::string> _decl_names; ///< Cached names of declarations.

		/// If the entity has a user-defined export name, returns that name; otherwise returns the declaration's name.
		template <typename Ent> [[nodiscard]] std::string_view _get_export_name(entity *ent) {
			std::string_view user = cast<Ent>(ent)->get_substitute_name();
			if (user.empty()) {
				user = to_string_view(ent->get_generic_declaration()->getName());
			}
			return user;
		}
		/// Returns the name of a single template argument to be used when exporting.
		[[nodiscard]] std::string _get_template_argument_spelling(const clang::TemplateArgument &arg) {
			switch (arg.getKind()) {
			case clang::TemplateArgument::Null:
				return "$ERROR_NULL";
			case clang::TemplateArgument::Type:
				{
					auto type = qualified_type::from_clang_type(arg.getAsType(), nullptr);
					// TODO
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
		}
		/// Returns the name of a template argument list to be used when exporting, without \ref template_args_begin
		/// or \ref template_args_end.
		[[nodiscard]] std::string _get_template_argument_list_spelling(llvm::ArrayRef<clang::TemplateArgument> args) {
			std::string result;
			for (auto &&arg : args) {
				if (!result.empty()) {
					result += template_arg_separator;
				}
				result += _get_template_argument_spelling(arg);
			}
			return result;
		}
		/// Returns the name of the given entity without scope names.
		[[nodiscard]] std::string_view _get_entity_self_name(clang::NamedDecl *decl) {
			decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
			auto it = _decl_self_names.find(decl);
			if (it == _decl_self_names.end() || it->first != decl) {
				std::string_view base_name;
				if (auto *tag_decl = llvm::dyn_cast<clang::TagDecl>(decl)) {
					base_name = _get_export_name<entities::user_type_entity>(
						entities->find_or_register_parsed_entity(tag_decl)
					);
				} else if (auto *field_decl = llvm::dyn_cast<clang::FieldDecl>(decl)) {
					base_name = _get_export_name<entities::field_entity>(
						entities->find_or_register_parsed_entity(field_decl)
					);
				} else if (auto *function_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
					auto *ent = cast<entities::function_entity>(
						entities->find_or_register_parsed_entity(function_decl)
					);
					base_name = ent->get_substitute_name();
					if (base_name.empty()) { // no user-defined name
						if (function_decl->isOverloadedOperator()) { // operator
							base_name = func_naming.get_operator_name(function_decl->getOverloadedOperator());
						} else if (llvm::isa<clang::CXXConstructorDecl>(function_decl)) { // constructor
							base_name = func_naming.constructor_name;
						} else { // normal function
							base_name = to_string_view(function_decl->getName());
						}
					}
				} else {
					base_name = to_string_view(decl->getName());
				}

				std::string name(base_name);
				if (auto *template_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
					name +=
						std::string(template_args_begin) +
						_get_template_argument_list_spelling(template_decl->getTemplateArgs().asArray()) +
						std::string(template_args_end);
				}
				it = _decl_self_names.emplace_hint(it, decl, std::move(name));
			}
			return it->second;
		}
		/// Returns the full name of the given entity, including all its parent scopes.
		[[nodiscard]] std::string_view _get_entity_name(clang::NamedDecl *decl) {
			decl = llvm::cast<clang::NamedDecl>(decl);
			auto it = _decl_names.lower_bound(decl);
			if (it == _decl_names.end() || it->first != decl) {
				std::string result;
				for (
					auto *context = llvm::cast<clang::DeclContext>(decl);
					context && !context->isTranslationUnit();
					context = context->getParent()
				) {
					if (!result.empty()) {
						result = std::string(scope_separator) + result;
					}
					result = std::string(_get_entity_self_name(llvm::cast<clang::NamedDecl>(context))) + result;
				}
				it = _decl_names.emplace_hint(it, decl, std::move(result));
			}
			return it->second;
		}

		/// Returns the name of the given function.
		[[nodiscard]] std::string_view _get_function_name(clang::FunctionDecl *decl) const {
			if (decl->isOverloadedOperator()) {
				return func_naming.get_operator_name(decl->getOverloadedOperator());
			}
			return to_string_view(decl->getName());
		}
	};
}
