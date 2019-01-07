#include "entity.h"

#include <utility>

#include "entity_registry.h"
#include "entity_types.h"

namespace apigen {
	clang::PrintingPolicy &get_cpp_printing_policy() {
		static clang::PrintingPolicy _policy{clang::LangOptions()};
		static bool _init = false;

		if (!_init) {
			_policy.adjustForCPlusPlus();
			_init = true;
		}
		return _policy;
	}

	/// Contains an array that records if an \ref entity class is a base of another \ref entity class.
	template <typename ...Types> struct entity_base_table {
		/// Initializes \ref _tbl.
		entity_base_table() {
			_load_table();
		}

		/// Returns the result of \ref is_entity_base_of_v<base, derived>.
		constexpr bool is_base_of(entity_kind base, entity_kind derived) const {
			return _tbl[static_cast<size_t>(base)][static_cast<size_t>(derived)];
		}
	protected:
		/// The number of entity classes.
		constexpr static size_t _num_kinds = sizeof...(Types);
		using _row = std::array<bool, _num_kinds>; ///< A row of the base table.
		using _table = std::array<_row, _num_kinds>; ///< Type of the base table.

		_table _tbl; ///< The table.

		/// Generates a single row of the base table.
		template <typename Base> void _load_single() {
			((_tbl[static_cast<size_t>(Base::kind)][static_cast<size_t>(Types::kind)] =
				{std::is_base_of_v<Base, Types>}), ...);
		}
		/// Generates the whole table.
		void _load_table() {
			(_load_single<Types>(), ...);
		}
	};

	bool is_entity_base_of_dynamic(entity_kind base, entity_kind derived) {
		static entity_base_table<
			entity,
			function_entity,
			method_entity,
			constructor_entity,
			user_type_entity,
			enum_entity,
			record_entity,
			template_specialization_entity,
			field_entity
		> _tbl;
		return _tbl.is_base_of(base, derived);
	}


	void entity::propagate_export(export_propagation_queue&, entity_registry&) {
	}

	std::string entity::get_environment_name(
		clang::Decl *decl, const entity_registry &reg, const naming_conventions &conv
	) {
		std::vector<clang::DeclContext*> decls;
		auto *cur = decl;
		while (cur) {
			auto *ctx = cur->getDeclContext();
			if (ctx == nullptr) {
				break;
			}
			decls.emplace_back(ctx);
			cur = llvm::dyn_cast<clang::Decl>(ctx);
		}
		int nspc = conv.namespace_levels, clsc = conv.parent_class_levels;
		std::string res;
		auto it = decls.rbegin();
		for (; it != decls.rend(); ++it) {
			if (nspc != 0) {
				if (auto *nsdecl = llvm::dyn_cast<clang::NamespaceDecl>(*it); nsdecl) {
					append_with_sep(res, nsdecl->getName().str(), conv.env_separator);
					--nspc;
					continue;
				}
			}
			if (clsc == 0) {
				break;
			}
			if (auto *clsdecl = llvm::dyn_cast<clang::CXXRecordDecl>(*it); clsdecl) {
				append_with_sep(res, reg.get_exported_name(clsdecl), conv.env_separator);
				--clsc;
				continue;
			}
		}
		return res;
	}

	void entity::export_api_types(const entity_registry &reg, export_writer&) const {
	}

	void entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer&) const {
	}

	void entity::export_host_functions(const entity_registry&, export_writer&) const {
	}

	void entity::export_host_api_initializers(const entity_registry&, export_writer&, std::string_view) const {
	}
}
