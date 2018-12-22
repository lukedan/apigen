#pragma once

#include <iostream>
#include <string>
#include <cctype>
#undef NDEBUG // TODO very dirty debug hack
#include <cassert>
#include <type_traits>

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

#include "apigen_definitions.h"
#include "enum_bitwise.h"
#include "export_writer.h"

namespace apigen {
	class entity_registry;
	struct export_propagation_queue;

	/// Returns a \p clang::PrintingPolicy for printing internal types.
	clang::PrintingPolicy &get_internal_type_printing_policy();

	/// Dictates how objects in the generated code are named.
	///
	/// \todo Document each field.
	struct naming_conventions {
		std::string
			host_function_name_pattern,
			api_function_name_pattern,

			api_enum_name_pattern,
			api_enum_entry_name_pattern,

			api_record_name_pattern,
			api_move_struct_name_pattern, ///< Name pattern of move wrapper for structs.
			api_templated_record_name_pattern,
			api_templated_record_move_struct_name_pattern, ///< Name pattern of move wrapper for templated structs.

			host_method_name_pattern,
			api_method_name_pattern,


			host_field_getter_name_pattern,
			api_field_getter_name_pattern,

			env_separator,

			api_struct_name, ///< Name of the API struct.
			/// The name of the class containing all API function implementations. This is mainly to make friend
			/// declarations more convenient.
			host_function_class_name,
			host_init_api_function_name; ///< The name of the function used to initialize the API struct.

		int
			/// The number of namespaces levels, starting from top-level namespaces, that are prepended to the
			/// environment name. If this is less than zero, then all namespace names will be added.
			namespace_levels = -1,
			/// The number of parent classes, starting from top-level classes, that are prepended to theenvironment
			/// name. If this is less than zero, then all class names will be added.
			parent_class_levels = -1;
	};


	/// Specifies the kind of an entity through \ref entity::get_kind(). This exists since RTTI is disabled for clang.
	enum class entity_kind {
		base, ///< The base class.
		user_type, ///< A user-defined type.
		enumeral, ///< An enum.
		record, ///< A struct or class, possibly a template specialization.
		template_specialization, ///< A template specialization, either explicit or implicit.
		field, ///< A field of a record.
		function, ///< A function, possibly a method.
		method, ///< A method.

		max_value ///< The maximum value, used when generating the base table.
	};
	/// Dynamic version of \ref is_entity_base_of_v.
	bool is_entity_base_of_dynamic(entity_kind base, entity_kind derived);


	/// Specifies the properties of a type.
	enum class type_property {
		empty = 0, ///< Non-pointer non-array value type without any qualifiers.

		const_flag = 1, ///< Const-qualified type.

		pointer_flag = 2, ///< Either a pointer type or an array type.
		array_flag = 4, ///< Array type.
		reference_flag = 8, ///< Reference type.
		rvalue_reference_flag = 16, ///< Rvalue reference type.

		pointer = pointer_flag, ///< Pointer type.
		array = pointer_flag | array_flag, ///< Array type.
		lvalue_reference = reference_flag, ///< Lvalue reference type.
		rvalue_reference = reference_flag | rvalue_reference_flag ///< Rvalue reference type.
	};
	/// \ref type_property is a bit mask.
	template <> struct enable_enum_bitwise_operators<type_property> : std::true_type {
	};

	/// Stores information about an entity.
	class entity {
	public:
		/// Used by RAII helpers.
		constexpr static entity_kind kind = entity_kind::base;
		/// Returns the \ref entity_kind that corresponds to the type of this instance. All derived classes must
		/// override this method.
		virtual entity_kind get_kind() const {
			return entity_kind::base;
		}

		/// Initializes \ref declaration.
		explicit entity(clang::NamedDecl *decl) {
			declaration = decl;
		}
		/// Default virtual destructor.
		virtual ~entity() = default;

		/// Strips pointer, reference, and array types, and qualifiers from the given qualified type, optionally saving
		/// each property in the given \p std::vector as it is removed.
		inline static const clang::Type *strip_type(clang::QualType qty, std::vector<type_property> *props = nullptr) {
			qty = qty.getCanonicalType();
			if (qty->isReferenceType()) {
				qty = qty->getPointeeType();
				if (props) {
					props->emplace_back(
						qty->isRValueReferenceType() ?
						type_property::rvalue_reference :
						type_property::lvalue_reference
					);
				}
			}
			while (true) { // peel off pointer types
				if (props) {
					props->emplace_back(type_property::empty);
					if (qty.isConstQualified()) {
						props->back() |= type_property::const_flag;
					}
				}
				if (qty->isPointerType()) {
					qty = qty->getPointeeType();
					if (props) {
						props->back() |= type_property::pointer;
					}
				} else if (auto *arrty = llvm::dyn_cast<clang::ArrayType>(qty.getTypePtr()); arrty) {
					qty = arrty->getElementType();
					if (props) {
						props->back() |= type_property::array;
					}
				} else {
					break;
				}
			}
			return qty.getTypePtr();
		}
		/// Returns the postfix of a type, given the list of \ref type_property produced by \ref strip_type(). Only
		/// pointers (including arrays) and \p const qualifiers are considered.
		inline static std::string get_type_postfix(const std::vector<type_property> &props) {
			std::string res;
			for (type_property p : props) {
				if ((p & type_property::const_flag) != type_property::empty) {
					res += " const";
				}
				if ((p & type_property::pointer_flag) != type_property::empty) {
					res += "*";
				}
			}
			return res;
		}
		/// Replaces all illegal characters in a \p std::string with the given character to form a legal identifier
		/// name.
		inline static std::string convert_into_identifier(const std::string &s, char rep = '_') {
			std::string res;
			for (char c : s) {
				if (!std::isalnum(c) && c != '_') {
					res += rep;
				} else {
					res += c;
				}
			}
			return res;
		}
		/// Returns the corresponding string representation of the value of a template argument. This string does not
		/// fully represent the original value and can only be used to help distinguish between template
		/// specializations.
		inline static std::string template_arg_to_string(const clang::TemplateArgument &arg) {
			switch (arg.getKind()) {
			case clang::TemplateArgument::Type:
				{
					clang::QualType qty = arg.getAsType();
					std::vector<type_property> typeprops;
					const clang::Type *ty = strip_type(qty, &typeprops);
					// type properties
					std::string qual;
					for (type_property prop : typeprops) {
						if ((prop & type_property::const_flag) != type_property::empty) {
							qual += 'c';
						}
						if ((prop & type_property::reference_flag) != type_property::empty) {
							qual += 'r';
						}
						if ((prop & type_property::pointer_flag) != type_property::empty) {
							qual += 'p';
						}
					}
					// base type name
					std::string corename;
					if (ty->isRecordType() || ty->isEnumeralType()) {
						// TODO rename?
						corename = ty->getAsTagDecl()->getName().str();
					} else {
						corename = convert_into_identifier(clang::QualType(ty, 0).getAsString());
					}
					return qual + corename;
				}
			case clang::TemplateArgument::NullPtr:
				return "nullptr";
			case clang::TemplateArgument::Integral:
				return convert_into_identifier(arg.getAsIntegral().toString(10));
			case clang::TemplateArgument::Null: // panic
				std::cout << "undeduced template parameter\n";
				std::abort();
				break;
				// TODO below
			case clang::TemplateArgument::Declaration:
				break;
			case clang::TemplateArgument::Template:
				break;
			case clang::TemplateArgument::TemplateExpansion:
				break;
			case clang::TemplateArgument::Expression:
				break;
			case clang::TemplateArgument::Pack:
				break;
			}
			return "_ARG_";
		}

		/// Appends the string \p n to \p s, but appends the separator to it first if the string is not empty.
		///
		/// \todo This function should probably be a global function.
		inline static void append_with_sep(std::string &s, const std::string &n, const std::string &sep) {
			if (!s.empty()) {
				s += sep;
			}
			s += n;
		}
		/// Appends the name of the given \p clang::NamedDecl to the given string by calling \ref append_with_sep(). If
		/// that \p NamdeDecl can be found in the \ref entity_registry, then the name returned by
		/// \ref entity::get_declaration_name() will be used; otherwise uses the name of that \p NamedDecl.
		static void append_environment_name(
			std::string&, clang::NamedDecl*, const entity_registry&, const naming_conventions&
		);
		/// Returns a string representation of the environment (i.e., namespaces, parent classes) of the given
		/// \p clang::Decl.
		static std::string get_environment_name(clang::Decl*, const entity_registry&, const naming_conventions&);

		/// Returns the name used when exporting this entity. If a substitute name is specified, \ref substitute_name
		/// will be used; otherwise uses the name of \ref declaration.
		std::string get_declaration_name() const {
			return substitute_name.empty() ? declaration->getName().str() : substitute_name;
		}


		/// Called to register an appearance of this entity in the code. Registers all attributes of the declaration
		/// via \ref consume_annotation().
		virtual void register_declaration(clang::NamedDecl *decl) {
			for (clang::Attr *attr : decl->getAttrs()) { // process annotations
				if (attr->getKind() == clang::attr::Kind::Annotate) {
					auto *annotate = clang::cast<clang::AnnotateAttr>(attr);
					if (!consume_annotation(annotate->getAnnotation())) {
						std::cerr <<
							declaration->getName().str() <<
							": unknown annotation " << annotate->getAnnotation().str() << "\n";
					}
				}
			}
		}
		/// Registers all declarations of the given \p clang::Decl with \ref register_declaration().
		void register_all_declarations(clang::NamedDecl *decl) {
			for (clang::Decl *d : decl->redecls()) {
				register_declaration(llvm::cast<clang::NamedDecl>(d));
			}
		}

		/// Processes the given annotation and updates corresponding properties of this entity.
		virtual bool consume_annotation(llvm::StringRef anno) {
			if (anno == APIGEN_ANNOTATION_EXPORT) {
				is_exported = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_EXCLUDE) {
				is_excluded = true;
				return true;
			}
			if (anno.startswith(APIGEN_ANNOTATION_RENAME_PREFIX)) {
				if (!substitute_name.empty()) {
					std::cerr <<
						declaration->getName().str() <<
						": substitute name " << substitute_name << " discarded\n";
				}
				size_t prefixlen = std::strlen(APIGEN_ANNOTATION_RENAME_PREFIX);
				substitute_name = std::string(anno.begin() + prefixlen, anno.end());
				return true;
			}
			return false;
		}

		/// Propagates the `export' flag to all other entities that this entity depends on. Can also gather dependency
		/// information during this process.
		virtual void propagate_export(export_propagation_queue&, entity_registry&);
		/// Called to cache names used when exporting this entity. Entities should call
		/// \ref entity_registry::register_name to register such names.
		virtual void cache_export_names(entity_registry&, const naming_conventions&) = 0;

		/// Exports the types that are needed in the C API header.
		virtual void export_api_types(const entity_registry&, export_writer&) const;
		/// Exports function pointers in the API interface in the C API header.
		virtual void export_api_struct_func_ptrs(const entity_registry&, export_writer&) const;
		/// Exports host functions whose pointers are to be passed to the API struct.
		virtual void export_host_functions(const entity_registry&, export_writer&) const;
		/// Exports statements to initialize a given API struct.
		virtual void export_host_api_initializers(const entity_registry&, export_writer&, std::string_view) const;

		clang::NamedDecl *declaration; ///< The canonical declaration of this entity.
		std::string substitute_name; ///< The name used when exporting this entity.
		bool
			is_exported = false, ///< Indicates whether this entity is exported.
			is_excluded = false; ///< Indicates if this entity should not be exported.
	};

	/// Casts an \ref entity to another entity type. Asserts if the types don't match.
	template <typename U> U *cast(entity *ent) {
		static_assert(std::is_base_of_v<entity, U>, "cast can only be used for entities");
		if (ent) {
			assert(is_entity_base_of_dynamic(U::kind, ent->get_kind()));
		}
		return static_cast<U*>(ent);
	}
	/// Casts an \ref entity to another entity type. Returns \p nullptr if types don't match.
	template <typename U> U *dyn_cast(entity *ent) {
		static_assert(std::is_base_of_v<entity, U>, "cast can only be used for entities");
		if (ent && is_entity_base_of_dynamic(U::kind, ent->get_kind())) {
			return static_cast<U*>(ent);
		}
		return nullptr;
	}
	/// Checks if a given \ref entity is of a certain type.
	template <typename U> bool isa(entity *ent) {
		static_assert(std::is_base_of_v<entity, U>, "cast can only be used for entities");
		return ent && is_entity_base_of_dynamic(U::kind, ent->get_kind());
	}
}
