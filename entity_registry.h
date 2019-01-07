#pragma once

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>
#include <string_view>

#include <clang/AST/DeclTemplate.h>

#include "entity.h"

namespace apigen {
	class user_type_entity;
	class template_specialization_entity;

	/// Used to prevent duplicate names in the same scope.
	class name_scope {
	public:
		/// Used to record names. The integer value keeps track of how many times that name appears, which is appended
		/// to it when \ref token::compose() is called.
		using registry = std::map<std::string, size_t, std::less<>>;
		/// Token for a registered name.
		struct token {
			friend name_scope;
		public:
			/// Default constructor. Creates an empty token.
			token() = default;

			/// Returns the final name composed of the original name and the number used for disambiguation. The caller
			/// is responsible of checking \ref is_empty().
			std::string compose() const {
				size_t n = _number;
				if (_scope->parent()) {
					n += _scope->parent()->find_occurences(_strit->first);
				}
				if (n == 0) {
					return _strit->first;
				}
				return _strit->first + std::to_string(n);
			}
			/// Returns whether this token is empty.
			bool is_empty() const {
				return _strit == registry::const_iterator();
			}
		protected:
			/// Constructor used by \ref name_scope to initialize all fields of this struct.
			token(registry::const_iterator it, size_t n, const name_scope &sc) : _strit(it), _number(n), _scope(&sc) {
			}

			registry::const_iterator _strit; ///< Iterator to the actual name and number of occurences.
			size_t _number = 0; ///< The number used for dismbiguation.
			const name_scope *_scope = nullptr; ///< The \ref name_scope that contains this name.
		};


		/// Default constructor.
		name_scope() = default;
		/// Constructs this \ref name_scope with a parent.
		explicit name_scope(const name_scope *p) : _parent(p) {
		}


		/// Registers a name and returns the corresponding \ref token.
		token register_name(std::string n) {
			auto [it, inserted] = _reg.try_emplace(std::move(n), 0);
			++it->second;
			return token(it, it->second - 1, *this);
		}
		/// Finds how many times the given name has been registered in this scope or parent scopes.
		size_t find_occurences(std::string_view name) const {
			size_t n = 0;
			if (auto found = _reg.find(name); found != _reg.end()) {
				n += found->second;
			}
			if (_parent) {
				n += _parent->find_occurences(name);
			}
			return n;
		}

		/// Returns the parent name scope.
		const name_scope *parent() const {
			return _parent;
		}
	protected:
		registry _reg; ///< The registry.
		const name_scope *const _parent = nullptr; ///< The parent scope.
	};
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

			/// Name pattern of non-static member operators (including normal operators and conversions) in host code.
			host_member_operator_name_pattern,
			/// Name pattern of non-static member operators (including normal operators and conversions) in API code.
			api_member_operator_name_pattern,

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
	/// Registry of all entities and names.
	class entity_registry {
	public:
		/// Registers a declaration. If the associated entity was not previously registered, this function creates an
		/// entity of the given type. \ref entity::register_declaration() is then called.
		template <typename T> T *register_entity(clang::NamedDecl *decl) {
			auto *canonical = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
			auto [it, inserted] = entities.try_emplace(canonical);
			if (inserted) {
				it->second = std::make_unique<T>(canonical);
			}
			it->second->register_declaration(decl);
			return static_cast<T*>(it->second.get());
		}
		/// Registers a template specialization. If it's not previously registered an a
		/// \ref template_specialization_entity is created, then
		/// \ref template_specialization_entity::copy_export_options() will be called.
		template_specialization_entity *register_template_specialization_entity(
			clang::ClassTemplateSpecializationDecl*
		);
		/// Extracts annotations from the given type alias. This is mainly used for template specializations since
		/// there's no direct way to add annotations to them individually.
		///
		/// \todo For whatever reason \p using doesn't work with annotations.
		void register_type_alias_decl(clang::TypedefNameDecl*);

		/// Performs preparations before exporting by calling \ref entity::propagate_export(),
		/// \ref entity::gather_extra_dependencies(), and \ref entity::cache_export_names().
		void prepare_for_export(const naming_conventions&);

		/// Calls \ref entity::export_api_types() to export types, then calls
		/// \ref entity::export_api_struct_func_ptrs() to export function pointers.
		void export_api_header(export_writer &out, const naming_conventions &conv) const {
			for (const entity *ent : exported_entities) {
				ent->export_api_types(*this, out);
			}
			out << "typedef struct " << scopes::braces << new_line();
			for (const entity *ent : exported_entities) {
				ent->export_api_struct_func_ptrs(*this, out);
			}
			out.end_scope() << " " << conv.api_struct_name << ";" << new_line();
		}
		/// Calls \ref entity::export_host_functions() to export all host functions.
		void export_host_source(export_writer &out, const naming_conventions &conv) const {
			std::string_view api_obj_name = "api";

			out << "class " << APIGEN_STR(APIGEN_API_CLASS_NAME) << " " << scopes::class_def << new_line(1);
			out << "public:" << new_line();
			for (const entity *ent : exported_entities) {
				ent->export_host_functions(*this, out);
			}
			out.end_scope() << new_line() << new_line();
			out <<
				"void " << conv.host_init_api_function_name << "(" << conv.api_struct_name << " &" <<
				api_obj_name << ") " << scopes::braces << new_line();
			for (const entity *ent : exported_entities) {
				ent->export_host_api_initializers(*this, out, api_obj_name);
			}
			out.end_scope() << new_line();
		}

		/// Finds the \ref entity that corresponds to the given declaration. Returns \p nullptr if none is found.
		entity *find_entity(clang::NamedDecl *decl) const {
			auto it = entities.find(llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl()));
			return it == entities.end() ? nullptr : it->second.get();
		}

		/// If the given type is a built-in type, returns its name; otherwise returns the name of the corresponding
		/// \ref entity which may be a substitute name.
		std::string get_exported_name(const clang::Type *ty) const {
			if (ty->isRecordType() || ty->isEnumeralType()) {
				return get_exported_name(ty->getAsTagDecl());
			}
			return clang::QualType(ty, 0).getAsString(get_cpp_printing_policy());
		}
		/// \overload
		std::string get_exported_name(clang::TagDecl *decl) const {
			if (auto *ent = find_entity(decl); ent) {
				return ent->get_exported_name();
			}
			return decl->getName().str(); // shouldn't happen, except maybe for templates
		}

		std::unordered_map<clang::NamedDecl*, std::unique_ptr<entity>> entities; ///< The list of entities.
		std::unordered_set<entity*> exported_entities; ///< The list of entities that are exported.
		///< The list of \p typedef and \p using derivatives that are processed after parsing has completed.
		std::vector<clang::TypedefNameDecl*> deferred_typedefs;
		name_scope
			global_scope, ///< The global scope.
			api_struct_scope{&global_scope}, ///< The scope of the API struct containing function pointers.
			init_function_scope{&global_scope}; ///< The scope of the function that initializes the API struct.
	};

	/// A structure that contains queued dependencies.
	struct export_propagation_queue {
	public:
		/// Constructor. Initializes \ref _reg.
		explicit export_propagation_queue(entity_registry &r) : _reg(r) {
		}

		/// Calls \ref _queue_export() to queue that entity if it is not a template declaration.
		bool queue_exported_entity(entity &ent) {
			if (ent.declaration->getDescribedTemplate()) {
				std::cerr << ent.declaration->getName().str() << ": cannot export template\n";
				return false;
			}
			return _queue_export(ent);
		}
		/// Calls \ref _queue_export() to queue the entity associated with the given type if it's a record type or an
		/// enumerate type.
		user_type_entity *queue_dependent_type(const clang::Type*);

		/// Returns whether this queue is empty.
		bool empty() const {
			return _q.empty();
		}
		/// Takes an element from the queue.
		entity *dequeue() {
			entity *et = _q.front();
			_q.pop_front();
			return et;
		}
	protected:
		entity_registry &_reg; ///< The associated \ref entity_registry.
		std::deque<entity*> _q; ///< The underlying \p std::deque.

		/// Adds the given entity to \ref _q if it has not already been exported. The \ref entity::is_exported flag
		/// is set for that entity, and the \ref entity::is_excluded flag is cleared.
		bool _queue_export(entity &ent) {
			if (_reg.exported_entities.emplace(&ent).second) {
				if (ent.is_excluded) {
					std::cerr << ent.declaration->getName().str() << ": overriden exclude flag\n";
					ent.is_excluded = false;
				}
				_q.emplace_back(&ent);
				ent.is_exported = true;
				return true;
			}
			return false;
		}
	};
}
