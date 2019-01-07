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
	struct naming_conventions;

	/// Returns a \p clang::PrintingPolicy for printing internal types.
	clang::PrintingPolicy &get_cpp_printing_policy();


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
		constructor ///< A constructor.
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
		/// Returns the traits of a type, given the list of \ref type_property produced by \ref strip_type(). Only
		/// pointers (including arrays) and \p const qualifiers are considered.
		inline static std::string get_type_traits(const std::vector<type_property> &props) {
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
		/// Returns a human-readable prefix for the given list of \ref type_property that's suitable to be used as part
		/// of an identifier.
		inline static std::string get_type_traits_identifier(const std::vector<type_property> &props) {
			std::string res;
			for (type_property p : props) {
				if ((p & type_property::const_flag) != type_property::empty) {
					res += 'c';
				}
				if ((p & type_property::reference_flag) != type_property::empty) {
					res += 'r';
				}
				if ((p & type_property::pointer_flag) != type_property::empty) {
					res += 'p';
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
					std::vector<type_property> typeprops;
					const clang::Type *ty = strip_type(arg.getAsType(), &typeprops);
					std::string name = get_type_traits_identifier(typeprops);
					if (ty->isRecordType() || ty->isEnumeralType()) {
						// TODO rename?
						name = ty->getAsTagDecl()->getName().str();
					} else {
						name = clang::QualType(ty, 0).getAsString();
					}
					return name;
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
		/// Returns a string representation of the environment (i.e., namespaces, parent classes) of the given
		/// \p clang::Decl.
		static std::string get_environment_name(clang::Decl*, const entity_registry&, const naming_conventions&);

		/// Returns the name used when exporting this entity. If a substitute name is specified, \ref substitute_name
		/// will be used; otherwise uses the name of \ref declaration.
		std::string get_exported_name() const {
			return get_substitute_name().empty() ? declaration->getName().str() : get_substitute_name();
		}


		/// Called to register an appearance of this entity in the code. Registers all attributes of the declaration
		/// via \ref consume_annotation().
		virtual void register_declaration(clang::NamedDecl *decl) {
			for (clang::Attr *attr : decl->attrs()) { // process annotations
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
				size_t prefixlen = std::strlen(APIGEN_ANNOTATION_RENAME_PREFIX);
				if (set_substitute_name(std::string(anno.begin() + prefixlen, anno.end()))) {
					std::cerr <<
						declaration->getName().str() <<
						": substitute name discarded (guess which)\n";
				}
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

		/// Returns the substitute name.
		const std::string &get_substitute_name() const {
			return _substitute_name;
		}
		/// Sets the substitute name. Previously set names will be discarded.
		///
		/// \return Whether a different substitute name has been discarded.
		bool set_substitute_name(std::string s) {
			std::swap(_substitute_name, s);
			return !s.empty() && _substitute_name != s;
		}

		clang::NamedDecl *declaration; ///< The canonical declaration of this entity.
		bool
			is_exported = false, ///< Indicates whether this entity is exported.
			is_excluded = false; ///< Indicates if this entity should not be exported.
	protected:
		std::string _substitute_name; ///< The name used when exporting this entity.
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
