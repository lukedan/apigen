#pragma once

/// \file
/// Generic abstract base class of user-defined types.

#include "../entity.h"

namespace apigen::entities {
	/// Generic abstract base class of user-defined types.
	class user_type_entity : public entity {
	public:
		constexpr static entity_kind kind = entity_kind::user_type; ///< The kind of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}
	};
}
