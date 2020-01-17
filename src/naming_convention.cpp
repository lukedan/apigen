/// \file
/// Default implementation of certain naming related functions.

#include "naming_convention.h"

#include "entity_kinds/constructor_entity.h"
#include "entity_kinds/enum_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/function_entity.h"
#include "entity_kinds/method_entity.h"
#include "entity_kinds/record_entity.h"
#include "entity_kinds/user_type_entity.h"

namespace apigen {
	naming_convention::name_info naming_convention::get_record_name(const entities::record_entity &ent) {
		return get_user_type_name(ent);
	}

	naming_convention::name_info naming_convention::get_enum_name(const entities::enum_entity &ent) {
		return get_user_type_name(ent);
	}

	naming_convention::name_info naming_convention::get_user_type_name_dynamic(
		const entities::user_type_entity &ent
	) {
		if (auto *record_ent = dyn_cast<entities::record_entity>(&ent)) {
			return get_record_name(*record_ent);
		}
		if (auto *enum_ent = dyn_cast<entities::enum_entity>(&ent)) {
			return get_enum_name(*enum_ent);
		}
		return name_info("$BADTYPE", "$BAD");
	}

	naming_convention::name_info naming_convention::get_function_name_dynamic(
		const entities::function_entity &ent
	) {
		if (auto *method_ent = dyn_cast<entities::method_entity>(&ent)) {
			if (auto *constructor_ent = dyn_cast<entities::constructor_entity>(method_ent)) {
				return get_constructor_name(*constructor_ent);
			}
			return get_method_name(*method_ent);
		}
		return get_function_name(ent);
	}

	naming_convention::name_info naming_convention::get_entity_name_dynamic(const entity &ent) {
		if (auto *user_type_ent = dyn_cast<entities::user_type_entity>(&ent)) {
			return get_user_type_name_dynamic(*user_type_ent);
		}
		if (auto *func_ent = dyn_cast<entities::function_entity>(&ent)) {
			return get_function_name_dynamic(*func_ent);
		}
		return name_info("$UNKNOWN_ENTITY_TYPE", "$UNKNOWN");
	}
}
