#pragma once

#include <variant>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <clang/AST/DeclFriend.h>

#include "entity.h"
#include "entity_registry.h"
#include "qualified_type.h"

namespace apigen {
	class method_entity;

	/// An entity that stands for a user-defined type (as opposed to a built-in type).
	class user_type_entity : public entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::user_type;
		/// Returns \ref entity_kind::user_type.
		entity_kind get_kind() const override {
			return entity_kind::user_type;
		}

		/// Initializes this entity from the canonical declaration.
		explicit user_type_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		name_scope::token name; ///< The name of this entity.
	};

	/// An entity that stands for an enumeration.
	class enum_entity : public user_type_entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::enumeral;
		/// Returns \ref entity_kind::enumeral.
		entity_kind get_kind() const override {
			return entity_kind::enumeral;
		}

		/// An enumeration constant.
		struct enum_constant {
			/// Default constructor.
			enum_constant() = default;
			/// Initializes all fields of this struct.
			enum_constant(name_scope::token tok, llvm::APSInt v) : name(tok), value(std::move(v)) {
			}

			name_scope::token name; ///< The name for this constant.
			llvm::APSInt value; ///< The value of this constant.
		};

		/// Initializes this entity from the canonical declaration.
		explicit enum_entity(clang::NamedDecl *decl) : user_type_entity(decl) {
		}

		/// Returns the associated integral type.
		const clang::Type *get_integral_type() const {
			return llvm::cast<clang::EnumDecl>(declaration)->getIntegerType().getTypePtr();
		}

		/// Caches the names of this entity and all enumeration constants.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			std::string
				env = get_environment_name(declaration, reg, conv),
				enumname = get_exported_name();
			name = reg.global_scope.register_name(fmt::format(conv.api_enum_name_pattern, env, enumname));

			auto *decl = llvm::cast<clang::EnumDecl>(declaration)->getDefinition();
			for (clang::EnumConstantDecl *constant : decl->enumerators()) {
				std::string name = fmt::format(
					conv.api_enum_entry_name_pattern, env, enumname, constant->getName().str()
				);
				constants.emplace_back(reg.global_scope.register_name(name), constant->getInitVal());
			}
		}

		/// Exports this enumeration as-is. Nothing else needs to be exported.
		void export_api_types(const entity_registry&, export_writer&) const override;

		std::vector<enum_constant> constants; ///< The list of enumeration constants.
	};

	/// An entity that stands for a \p struct or a \p class.
	class record_entity : public user_type_entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::record;
		/// Returns \ref entity_kind::record.
		entity_kind get_kind() const override {
			return entity_kind::record;
		}

		/// Initializes this entity from the canonical declaration.
		explicit record_entity(clang::NamedDecl *decl) : user_type_entity(decl) {
			auto recdecl = clang::cast<clang::CXXRecordDecl>(decl);
		}

		/// Returns the type name of the internal struct.
		std::string get_internal_type_name() const {
			return
				clang::QualType(
					llvm::cast<clang::CXXRecordDecl>(declaration)->getTypeForDecl(), 0
				).getAsString(get_cpp_printing_policy());
		}

		/// Checks if this record should be exported recursively, and if this should be exported as a base class.
		bool consume_annotation(llvm::StringRef anno) override {
			if (anno == APIGEN_ANNOTATION_RECURSIVE) {
				is_recursive = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_BASE) {
				is_base = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_NOBASE) {
				is_nobase = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_BASETREE) {
				is_basetree = true;
				return true;
			}
			return entity::consume_annotation(anno);
		}

		/// Populates \ref direct_derived_classes of parent classes.
		void gather_dependencies(entity_registry &reg) {
			if (
				auto *defdecl = llvm::cast<clang::CXXRecordDecl>(declaration)->getDefinition();
				defdecl && !defdecl->isInStdNamespace()
			) {
				for (const clang::CXXBaseSpecifier &base : defdecl->bases()) {
					if (auto *decl = strip_type(base.getType())->getAsCXXRecordDecl(); decl) {
						if (auto *ent = cast<record_entity>(reg.find_entity(decl)); ent) {
							ent->directly_derived_classes.emplace_back(this);
						}
					} else {
						std::cout << base.getType().getAsString() << ": cannot find declaration\n";
					}
				}
			}
		}
	protected:
		/// Decides if a member should be exported, based on whether this class is exported as a base class.
		bool _should_export(clang::AccessSpecifier spec) {
			return spec == clang::AS_public || (is_base && spec == clang::AS_protected);
		}
	public:
		/// Exports all public methods, fields, and subclasses, if \ref is_recursive is true.
		void propagate_export(export_propagation_queue &q, entity_registry &reg) override {
			auto *decl = llvm::cast<clang::CXXRecordDecl>(declaration);
			if (auto *def = decl->getDefinition(); def) {
				if (is_recursive) { // recursively export subclasses, methods, and fields
					for (clang::FieldDecl *field : def->fields()) {
						if (entity *ent = reg.find_entity(field); ent) {
							// these are naturally overriden in entity_registry::propagate_export()
							// if it is explicitly exported
							if (_should_export(field->getAccess()) && !ent->is_excluded) {
								q.queue_exported_entity(*ent);
							}
						}
					}

					for (clang::CXXMethodDecl *method : def->methods()) {
						if (entity *ent = reg.find_entity(method); ent) {
							if (_should_export(method->getAccess()) && !ent->is_excluded) {
								q.queue_exported_entity(*ent);
							}
						}
					}

					using record_iterator = clang::DeclContext::specific_decl_iterator<clang::CXXRecordDecl>;
					for (record_iterator it(def->decls_begin()), end(def->decls_end()); it != end; ++it) {
						if (auto *ent = cast<record_entity>(reg.find_entity(*it)); ent) {
							if (_should_export(it->getAccess()) && !ent->is_excluded) {
								ent->is_recursive = true;
								q.queue_exported_entity(*ent);
							}
						}
					}

					for (clang::FriendDecl *frnd : def->friends()) {
						if (auto *func = llvm::dyn_cast_or_null<clang::FunctionDecl>(frnd->getFriendDecl()); func) {
							if (func->isThisDeclarationADefinition()) {
								if (auto *ent = reg.find_entity(func); ent) {
									if (_should_export(func->getAccess()) && !ent->is_excluded) {
										// TODO
										// these function can only be accessed using ADL, i.e., one of the parameters
										// must be of the class' type.
										/*q.queue_exported_entity(*ent);*/
									}
								}
							}
						}
					}
				}
				// TODO base trees & this needs a separate pass
			} else {
				std::cerr <<
					decl->getName().str() << ": recursively exported class lacks definition\n";
			}
		}

		/// Caches the name of this record.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			std::string
				envname = get_environment_name(declaration, reg, conv),
				declname = get_exported_name();
			name = reg.global_scope.register_name(fmt::format(conv.api_record_name_pattern, envname, declname));
			move_struct_name = reg.global_scope.register_name(fmt::format(
				conv.api_move_struct_name_pattern, envname, declname
			));
		}

		/// Exports an opaque type if \ref is_directly_exported is \p false, or all fields of the original type.
		void export_api_types(const entity_registry&, export_writer&) const override;

		std::vector<record_entity*> directly_derived_classes; ///< Classes that directly derive from this class.
		std::vector<method_entity*> virtual_methods; ///< Stores the list of virtual functions.
		/// Name for the struct used when an object of this type is passed around.
		name_scope::token move_struct_name;
		bool
			is_recursive = false, ///< Indicates if public members of this class should also be exported.
			is_base = false, ///< Indicates if this record should be exported as a base class.
			/// Indicates if this record should \emph not be exported as a base class. This is useful if the base class
			/// is marked as a base tree root.
			is_nobase = false,
			is_basetree = false; ///< Indicates if all derived classes should be exported as base classes.
	};

	/// An entity that stands for a template specialization.
	class template_specialization_entity : public record_entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::template_specialization;
		/// Returns \ref entity_kind::template_specialization.
		entity_kind get_kind() const override {
			return entity_kind::template_specialization;
		}

		/// Initializes this entity from the canonical declaration.
		explicit template_specialization_entity(clang::NamedDecl *decl) : record_entity(decl) {
		}

		// TODO different set of methods?

		/// Caches the name of this template specialization.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			auto *tempdecl = llvm::cast<clang::ClassTemplateSpecializationDecl>(declaration);
			std::string args;
			for (const clang::TemplateArgument &arg : tempdecl->getTemplateArgs().asArray()) {
				append_with_sep(args, template_arg_to_string(arg), conv.env_separator);
			}
			std::string
				envname = get_environment_name(declaration, reg, conv),
				declname = get_exported_name();
			name = reg.global_scope.register_name(fmt::format(
				conv.api_templated_record_name_pattern, envname, declname, args
			));
			move_struct_name = reg.global_scope.register_name(fmt::format(
				conv.api_templated_record_move_struct_name_pattern, envname, declname, args
			));
		}
	};

	/// An entity that stands for a field.
	class field_entity : public entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::field;
		/// Returns \ref entity_kind::field.
		entity_kind get_kind() const override {
			return entity_kind::field;
		}

		/// Initializes this entity from the canonical declaration.
		explicit field_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		/// Queues the type of this field for exporting.
		void propagate_export(export_propagation_queue &q, entity_registry &reg) override {
			auto *field = llvm::cast<clang::FieldDecl>(declaration);
			parent = cast<record_entity>(reg.find_entity(field->getParent()));
			q.queue_exported_entity(*parent);
			type = qualified_type::queue_as_dependency(q, field->getType());
		}

		/// Caches the names of the setter and the getter.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			std::string
				envname = get_environment_name(llvm::cast<clang::Decl>(declaration->getDeclContext()), reg, conv),
				clsname = parent->get_exported_name(),
				myname = get_exported_name();
			host_getter_name = reg.global_scope.register_name(fmt::format(
				conv.host_field_getter_name_pattern, envname, clsname, myname
			));
			api_getter_name = reg.api_struct_scope.register_name(fmt::format(
				conv.api_field_getter_name_pattern, envname, clsname, myname
			));
		}

		/// Exports the getter.
		void export_api_struct_func_ptrs(const entity_registry&, export_writer&) const override;
		/// Exports the getter.
		void export_host_functions(const entity_registry&, export_writer&) const override;
		/// Initializes the getter function in the API struct.
		void export_host_api_initializers(const entity_registry&, export_writer&, std::string_view) const override;

		qualified_type type; ///< The type of this field.
		name_scope::token
			host_getter_name, ///< The getter's name in host code.
			api_getter_name; ///< The getter's name in API header code.
		record_entity *parent = nullptr; ///< The record of this field.
	};

	/// An entity that stands for a function.
	class function_entity : public entity {
	public:
		/// Represents a function call parameter.
		struct parameter {
			/// Default constructor.
			parameter() = default;
			/// Initializes all fields of this struct.
			parameter(qualified_type ty, std::string n) : type(std::move(ty)), name(std::move(n)) {
			}

			/// Returns \ref name if it's not empty, or the given default name.
			std::string_view get_name(std::string_view default_name = "param") const {
				if (name.empty()) {
					return default_name;
				}
				return name;
			}

			qualified_type type; ///< The type of this parameter.
			std::string name; ///< The name of this parameter.
		};

		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::function;
		/// Returns \ref entity_kind::function.
		entity_kind get_kind() const override {
			return entity_kind::function;
		}

		/// Initializes this entity from the canonical declaration.
		explicit function_entity(clang::NamedDecl *decl) : entity(decl) {
		}

		/// Returns the name used for the given operator.
		static std::string_view get_operator_name(clang::OverloadedOperatorKind);

		/// Queues argument types and the return type for exporting.
		void propagate_export(export_propagation_queue &q, entity_registry &reg) override {
			auto *func = llvm::cast<clang::FunctionDecl>(declaration);
			if (auto *def = func->getDefinition(); def) {
				func = def;
			}
			for (clang::ParmVarDecl *decl : func->parameters()) {
				params.emplace_back(qualified_type::queue_as_dependency(q, decl->getType()), decl->getName().str());
			}
			return_type = qualified_type::queue_as_dependency(q, func->getReturnType());
		}

		/// Caches the host name and the API header name.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			std::string
				envname = get_environment_name(declaration, reg, conv),
				myname = get_exported_name();
			host_name = reg.global_scope.register_name(fmt::format(conv.host_function_name_pattern, envname, myname));
			api_name = reg.api_struct_scope.register_name(fmt::format(
				conv.api_function_name_pattern, envname, myname
			));
		}

		/// Exports the function pointer.
		void export_api_struct_func_ptrs(const entity_registry&, export_writer&) const override;
		/// Exports the function.
		void export_host_functions(const entity_registry&, export_writer&) const override;
		/// Initializes the function in the API struct.
		void export_host_api_initializers(const entity_registry&, export_writer&, std::string_view) const override;

		std::vector<parameter> params; ///< Information about all parameters.
		qualified_type return_type; ///< Return type.
		name_scope::token
			host_name, ///< The name used in host code.
			api_name; ///< The name used in the API header.
	};

	/// An entity that represents a method.
	class method_entity : public function_entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::method;
		/// Returns \ref entity_kind::method.
		entity_kind get_kind() const override {
			return entity_kind::method;
		}

		/// Initializes this entity from the canonical declaration.
		explicit method_entity(clang::NamedDecl *decl) : function_entity(decl) {
		}

		/// Returns qualifiers for the `this' pointer.
		std::string get_this_pointer_qualifiers() const {
			// TODO what about constexpr?
			auto *metdecl = llvm::cast<clang::CXXMethodDecl>(declaration);
			std::string res;
			if (metdecl->isConst()) {
				res += " const";
			}
			if (metdecl->isVolatile()) {
				res += " volatile";
			}
			return res;
		}

		/// Queues the record type, as well as parameter types and return type, for exporting.
		void propagate_export(export_propagation_queue &q, entity_registry &reg) override {
			auto *memfunc = llvm::cast<clang::CXXMethodDecl>(declaration);
			parent = cast<record_entity>(reg.find_entity(memfunc->getParent()));
			q.queue_exported_entity(*parent);
			function_entity::propagate_export(q, reg);
		}

		/// Caches the host name and the API header name.
		void cache_export_names(entity_registry &reg, const naming_conventions &conv) override {
			std::string
				envname = get_environment_name(llvm::cast<clang::Decl>(declaration->getDeclContext()), reg, conv),
				clsname = parent->get_exported_name();
			if (declaration->getDeclName().isIdentifier()) {
				std::string myname = get_exported_name();
				host_name = reg.global_scope.register_name(fmt::format(
					conv.host_method_name_pattern, envname, clsname, myname
				));
				api_name = reg.api_struct_scope.register_name(fmt::format(
					conv.api_method_name_pattern, envname, clsname, myname
				));
			} else {
				clang::DeclarationName name = declaration->getDeclName();
				std::string myname;
				switch (name.getNameKind()) {
				case clang::DeclarationName::NameKind::CXXOperatorName:
					myname = get_operator_name(name.getCXXOverloadedOperator());
					break;
				case clang::DeclarationName::NameKind::CXXConversionFunctionName:
					{
						std::vector<type_property> props;
						const clang::Type *ty = strip_type(name.getCXXNameType(), &props);
						myname = get_type_traits_identifier(props) + reg.get_exported_name(ty);
					}
					break;
				default:
					myname = "UNHANDLED_METHOD_TYPE";
					break;
				}
				host_name = reg.global_scope.register_name(fmt::format(
					conv.host_member_operator_name_pattern, envname, clsname, myname
				));
				api_name = reg.api_struct_scope.register_name(fmt::format(
					conv.api_member_operator_name_pattern, envname, clsname, myname
				));
			}
		}

		/// Exports the function pointer. Uses different patterns depending on whether the method is static.
		void export_api_struct_func_ptrs(const entity_registry&, export_writer&) const override;
		/// Exports the method.
		void export_host_functions(const entity_registry&, export_writer&) const override;
		/// Initializes the method in the API struct.
		void export_host_api_initializers(const entity_registry&, export_writer&, std::string_view) const override;

		record_entity *parent = nullptr; ///< The record of this entity.
	};

	/// An entity that represents a constructor.
	class constructor_entity : public method_entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::constructor;
		/// Returns \ref entity_kind::constructor.
		entity_kind get_kind() const override {
			return entity_kind::constructor;
		}
	};
}
