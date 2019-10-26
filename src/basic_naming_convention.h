#pragma once

/// \file
/// Implementation of \ref apigen::basic_naming_convention.

#include "naming_convention.h"

namespace apigen {
	/// A basic naming convention that uses the given separator to separate scopes.
	class basic_naming_convention : public naming_convention {
	public:
		/// Initializes \ref entities, and \ref printing_policy with the default \p clang::LangOptions.
		explicit basic_naming_convention(entity_registry &reg) :
			printing_policy(clang::LangOptions()), entities(&reg) {
		}

		/// Returns the function name.
		[[nodiscard]] name_info get_function_name(const entities::function_entity &entity) override {
			return name_info(
				std::string(_get_entity_name(entity.get_generic_declaration())),
				_get_function_parameter_list_spelling(entity.get_declaration()->parameters())
			);
		}
		/// Returns the method name.
		[[nodiscard]] name_info get_method_name(const entities::method_entity &entity) override {
			return name_info(
				std::string(_get_entity_name(entity.get_generic_declaration())),
				_get_function_parameter_list_spelling(entity.get_declaration()->parameters())
			);
		}
		/// Returns the constructor name.
		[[nodiscard]] name_info get_constructor_name(const entities::constructor_entity &entity) override {
			std::string name =
				std::string(
					_get_entity_name(llvm::cast<clang::NamedDecl>(entity.get_declaration()->getParent()))
				) +
				std::string(scope_separator) + std::string(func_naming.constructor_name);
			return name_info(
				std::move(name), _get_function_parameter_list_spelling(entity.get_declaration()->parameters())
			);
		}

		/// Returns the type name.
		[[nodiscard]] name_info get_user_type_name(const entities::user_type_entity &entity) override {
			return name_info(std::string(_get_entity_name(entity.get_generic_declaration())), "");
		}

		/// Returns the exported name of the destructor of the given \ref entities::record_entity.
		[[nodiscard]] name_info get_record_destructor_name(
			const entities::record_entity &entity
		) override {
			std::string name =
				std::string(_get_entity_name(entity.get_declaration())) +
				std::string(scope_separator) +
				std::string(func_naming.destructor_name);
			return name_info(std::move(name), "");
		}

		/// Returns the name of an enumerator in the enum declaration.
		[[nodiscard]] name_info get_enumerator_name(
			const entities::enum_entity &entity, clang::EnumConstantDecl *enumerator
		) override {
			std::string name =
				std::string(_get_entity_name(llvm::cast<clang::NamedDecl>(entity.get_declaration()))) +
				std::string(scope_separator) +
				enumerator->getName().str();
			return name_info(std::move(name), "");
		}

		/// Returns the exported name of the non-const getter of the given field.
		[[nodiscard]] name_info get_field_getter_name(const entities::field_entity &entity) override {
			// do not call _get_entity_name() on it directly since FieldDecl is not a DeclContext
			std::string name =
				std::string(_get_entity_name(entity.get_declaration()->getParent())) +
				std::string(scope_separator) +
				entity.get_declaration()->getName().str() +
				std::string(scope_separator) +
				std::string(func_naming.getter_name);
			return name_info(std::move(name), "");
		}
		/// Returns the exportedname of the const getter of the given field.
		[[nodiscard]] name_info get_field_const_getter_name(const entities::field_entity &entity) override {
			// do not call _get_entity_name() on it directly since FieldDecl is not a DeclContext
			std::string name =
				std::string(_get_entity_name(entity.get_declaration()->getParent())) +
				std::string(scope_separator) +
				entity.get_declaration()->getName().str() +
				std::string(scope_separator) +
				std::string(func_naming.const_getter_name);
			return name_info(std::move(name), "");
		}

		special_function_naming func_naming; ///< Naming information of overloaded operators.
		clang::PrintingPolicy printing_policy; ///< The printing policy used when generating type names.
		std::string_view
			scope_separator{"_"}, ///< Separator between scopes.

			template_args_begin{"_"}, ///< Separates class names and template argument lists.
			template_args_end{""}, ///< Appended after template argument lists.
			template_arg_separator{"_"}, ///< Separator between template arguments.

			params_begin{"_"}, ///< Separates the base name and the parameter type list.
			params_end{""}, ///< Appended after parameter type lists.
			param_separator{"_"}, ///< Separator between parameter types.
			params_empty{"_void"}; ///< The spelling used when the parameter type list is empty.
		entity_registry *entities = nullptr; ///< All registered entities.
	protected:
		/// Cached names of declarations, without its parent scopes.
		std::map<clang::NamedDecl*, std::string> _decl_self_names;
		std::map<clang::NamedDecl*, std::string> _decl_names; ///< Cached names of declarations.

		/// Appends short qualifiers to the given string.
		inline static void _append_qualifiers(std::string &str, qualifier quals) {
			if ((quals & qualifier::const_qual) != qualifier::none) {
				str += "c";
			}
			if ((quals & qualifier::volatile_qual) != qualifier::none) {
				str += "v";
			}
		}
		/// Appends short qualifiers, pointers, and references to the given string.
		inline static void _append_qualifiers_and_pointers(
			std::string &str, reference_kind ref, const std::vector<qualifier> &quals
		) {
			switch (ref) {
			case reference_kind::reference:
				str += "r";
				break;
			case reference_kind::rvalue_reference:
				str += "x";
				break;
			default:
				break;
			}
			_append_qualifiers(str, quals.front());
			for (auto it = ++quals.begin(); it != quals.end(); ++it) {
				str += "p";
				_append_qualifiers(str, *it);
			}
		}

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
					std::string result;
					_append_qualifiers_and_pointers(result, type.ref_kind, type.qualifiers);
					result += _get_type_name(type.type);
					return result;
				}
		    // The template argument is a declaration that was provided for a pointer,
		    // reference, or pointer to member non-type template parameter.
			case clang::TemplateArgument::Declaration:
				break;
			case clang::TemplateArgument::NullPtr:
				return "nullptr";
			case clang::TemplateArgument::Integral:
				{
					std::string result = arg.getAsIntegral().toString(10);
					for (char &c : result) {
						if (c < '0' || c > '9') {
							switch (c) {
							case '-':
								c = 'n';
								break;
							case '.':
								c = 'd';
								break;
							default:
								std::cerr << "unknown character in APSInt: " << result << "\n";
								c = '_';
								break;
							}
						}
					}
					return result;
				}
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
			decl = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
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
		/// Returns the exported name for the given \p clang::Type.
		[[nodiscard]] std::string_view _get_type_name(const clang::Type *type) {
			if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
				return to_string_view(builtin->getName(printing_policy));
			}
			if (auto *tag = llvm::dyn_cast<clang::TagType>(type)) {
				return _get_entity_name(tag->getAsTagDecl());
			}
			return "$UNSUPPORTED_TYPE";
		}

		/// Returns the spelling of the types of the parameter list of a function, used for disambiguation purposes.
		[[nodiscard]] std::string _get_function_parameter_list_spelling(
			const llvm::ArrayRef<clang::ParmVarDecl*> &ps
		) {
			if (ps.empty()) {
				return std::string(params_empty);
			}
			std::string result(params_begin);
			bool first = true;
			for (clang::ParmVarDecl *decl : ps) {
				if (first) {
					first = false;
				} else {
					result += std::string(param_separator);
				}
				auto type = qualified_type::from_clang_type(decl->getType(), nullptr);
				_append_qualifiers_and_pointers(result, type.ref_kind, type.qualifiers);
				result += _get_type_name(type.type);
			}
			result += std::string(params_end);
			return result;
		}
	};
}
