#pragma once

/// \file
/// The entity type that contains extra information and methods about

#include <type_traits>

#include <clang/AST/Decl.h>
#include <clang/AST/Attr.h>

#include "../apigen_definitions.h"
#include "misc.h"

namespace apigen {
	class entity_registry;
	class dependency_analyzer;

	/// Specifies the kind of an entity through \ref entity::get_kind(). This exists since RTTI is disabled for clang.
	enum class entity_kind {
		base, ///< The base class.
		user_type, ///< A user-defined type.
		enumeration, ///< An enum.
		record, ///< A struct or class, possibly a template specialization.
		/*template_specialization, ///< A template specialization, either explicit or implicit.*/
		field, ///< A field of a record.
		function, ///< A function, possibly a method.
		method, ///< A method.
		constructor ///< A constructor.
	};
	/// Function to check if an \ref entity_kind is the base of another \ref entity_kind dynamically.
	bool is_entity_base_of(entity_kind base, entity_kind derived);

	/// Stores additional information about an entity.
	class entity {
	public:
		constexpr static entity_kind kind = entity_kind::base; ///< The \ref entity_kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] virtual entity_kind get_kind() const {
			return kind;
		}

		/// Default virtual destructor.
		virtual ~entity() = default;

		/// Returns the associated \ref clang::NamedDecl.
		[[nodiscard]] virtual clang::NamedDecl *get_generic_declaration() const = 0;

		/// Marks this entity for exporting.
		void mark_for_exporting() {
			_export = true;
		}
		/// Returns whether this entity is marked for exporting.
		[[nodiscard]] bool is_marked_for_exporting() const {
			return _export;
		}
		/// Returns whether this entity is excluded from exporting.
		[[nodiscard]] bool is_excluded() const {
			return _exclude;
		}

		/// Called to process the attribute specified for this entity.
		virtual bool handle_attribute(std::string_view attr) {
			if (attr == APIGEN_ANNOTATION_EXPORT) {
				mark_for_exporting();
				return true;
			}
			if (attr == APIGEN_ANNOTATION_EXCLUDE) {
				_exclude = true;
				return true;
			}
			if (TEMP_starts_with(APIGEN_ANNOTATION_RENAME_PREFIX, attr)) {
				attr.remove_prefix(std::strlen(APIGEN_ANNOTATION_RENAME_PREFIX));
				if (!_substitute_name.empty() && _substitute_name != attr) {
					std::cerr << attr << ": conflicts with existing substitute name " << _substitute_name << "\n";
				}
				_substitute_name = attr;
				return true;
			}
			return false;
		}
		/// Handles a declaration of this entity.
		virtual void handle_declaration(clang::NamedDecl *decl) {
			for (clang::Attr *attr : decl->attrs()) { // process annotations
				if (auto *anno_attr = llvm::dyn_cast<clang::AnnotateAttr>(attr)) {
					llvm::StringRef attr_llvm = anno_attr->getAnnotation();
					std::string_view attr_str(attr_llvm.data(), attr_llvm.size());
					if (!handle_attribute(attr_str)) {
						std::cerr << "unknown annotation " << attr_str << "\n";
					}
				}
			}
		}
		/// Gathers dependencies for this entity.
		virtual void gather_dependencies(entity_registry&, dependency_analyzer&) = 0;
	protected:
		std::string _substitute_name; ///< The alternative name used when exporting this entity.
		bool
			_export = false, ///< Whether this entity is exported.
			_exclude = false; ///< Whether this entity is explicitly marked as excluded from exporting.
	};

	namespace _details {
		/// Used to determine the output type for entity cast operations.
		template <typename Entity, typename Desired> struct cast_output;
		/// Specialization of \ref cast_output when the input type is a non-const pointer.
		template <typename Base, typename Derived> struct cast_output<Base*, Derived> {
			using type = Derived*;
		};
		/// Specialization of \ref cast_output when the input type is a const pointer.
		template <typename Base, typename Derived> struct cast_output<const Base*, Derived> {
			using type = const Derived*;
		};
		/// Specialization of \ref cast_output when the input type is a non-const reference.
		template <typename Base, typename Derived> struct cast_output<Base&, Derived> {
			using type = Derived&;
		};
		/// Specialization of \ref cast_output when the input type is a const reference.
		template <typename Base, typename Derived> struct cast_output<const Base&, Derived> {
			using type = const Derived&;
		};
		/// Shorthand for \ref cast_output::type.
		template <typename Entity, typename Desired> using cast_output_t = typename cast_output<Entity, Desired>::type;
	}
	/// Checked casting of entity types.
	template <typename T, typename Ent> inline auto cast(Ent ent) {
		static_assert(std::is_base_of_v<entity, T>, "the target type must be derived from entity");
		entity_kind ent_kind = entity_kind::base;
		if constexpr (std::is_pointer_v<Ent>) {
			if (ent) {
				ent_kind = ent->get_kind();
			}
		} else {
			ent_kind = ent.get_kind();
		}
		assert_true(is_entity_base_of(T::kind, ent_kind), "cast failed");
		return reinterpret_cast<_details::cast_output_t<Ent, T>>(ent);
	}
	/// \p dynamic_cast of entity types.
	template <typename T, typename Ent> inline _details::cast_output_t<Ent, T> dyn_cast(Ent ent) {
		static_assert(std::is_base_of_v<entity, T>, "the target type must be derived from entity");
		if constexpr (std::is_pointer_v<Ent>) {
			if (ent == nullptr || !is_entity_base_of(T::kind, ent->get_kind())) {
				return nullptr;
			}
		} else {
			if (!is_entity_base_of(T::kind, ent.get_kind())) {
				throw;
			}
		}
		return reinterpret_cast<_details::cast_output_t<Ent, T>>(ent);
	}
	/// Checks if an entity is of the particular entity type.
	template <typename T> inline bool isa(const entity &ent) {
		static_assert(std::is_base_of_v<entity, T>, "the target type must be derived from entity");
		return is_entity_base_of(T::kind, ent.get_kind());
	}
}
