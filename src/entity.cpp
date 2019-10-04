#include "entity.h"

/// \file
/// Implementation of certain entity-related functions.

#include "entity_kinds/constructor_entity.h"
#include "entity_kinds/enum_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/function_entity.h"
#include "entity_kinds/method_entity.h"
#include "entity_kinds/record_entity.h"

namespace apigen {
	/// Contains an array that records if an \ref entity class is a base of another \ref entity class.
	template <typename ...Types> struct entity_base_table {
		/// Initializes \ref _tbl.
		entity_base_table() {
			_load_table();
		}

		/// Returns the result of \ref is_entity_base_of_v<base, derived>.
		[[nodiscard]] constexpr bool is_base_of(entity_kind base, entity_kind derived) const {
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

	bool is_entity_base_of(entity_kind base, entity_kind derived) {
		// always keep this list exactly as the same as the entity_kind enum, otherwise the result will be incorrect
		static entity_base_table<
			entity,
			entities::user_type_entity,
			entities::enum_entity,
			entities::record_entity,
			entities::field_entity,
			entities::function_entity,
			entities::method_entity,
			entities::constructor_entity
			/* template_specialization_entity, */
		> _tbl;
		return _tbl.is_base_of(base, derived);
	}
}
