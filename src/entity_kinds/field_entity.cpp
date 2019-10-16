#include "field_entity.h"

/// \file
/// Implementation of the \ref apigen::entities::field_entity.

#include "../entity_registry.h"

namespace apigen::entities {
	void field_entity::gather_dependencies(entity_registry &reg, dependency_analyzer &queue) {
		_type = qualified_type::from_clang_type(_decl->getType(), &reg);
		_parent = cast<record_entity>(reg.find_or_register_parsed_entity(_decl->getParent()));

		if (_type.type_entity) {
			queue.try_queue(*_type.type_entity);
		}
		queue.try_queue(*_parent);

		if (_type.ref_kind != reference_kind::none) {
			_field_kind = field_kind::reference_field;
		} else if ((_type.qualifiers.front() & qualifier::const_qual) != qualifier::none) {
			_field_kind = field_kind::const_field;
		} else if (_decl->isMutable()) {
			_field_kind = field_kind::mutable_field;
		}
	}
}
