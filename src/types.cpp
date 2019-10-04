#include "types.h"

/// \file
/// Implementation of certain type-related functions.

#include "entity_registry.h"

namespace apigen {
	qualified_type qualified_type::from_clang_type(const clang::QualType &orig_type, entity_registry &registry) {
		qualified_type result;
		clang::QualType canon_type = orig_type.getCanonicalType();
		if (canon_type->isReferenceType()) { // reference
			result.ref_kind =
				canon_type->isRValueReferenceType() ?
				reference_kind::rvalue_reference :
				reference_kind::reference;
			canon_type = canon_type->getPointeeType();
		}
		while (true) {
			result.qualifiers.emplace_back(convert_qualifiers(canon_type.getQualifiers()));
			if (canon_type->isPointerType()) {
				canon_type = canon_type->getPointeeType();
			} else if (auto *arrty = llvm::dyn_cast<clang::ArrayType>(canon_type.getTypePtr())) {
				canon_type = arrty->getElementType();
			} else {
				break;
			}
		}
		result.type = canon_type.getTypePtr();
		if (auto *tag_type = llvm::dyn_cast<clang::TagType>(result.type)) {
			entity *ent = registry.find_or_register_parsed_entity(tag_type->getAsTagDecl());
			result.type_entity = cast<entities::user_type_entity>(ent);
		}
		return result;
	}

	qualified_type qualified_type::from_clang_type_pointer(const clang::Type *type, entity_registry &reg) {
		qualified_type result;
		result.qualifiers.emplace_back(qualifier::none);
		result.type_entity = cast<entities::user_type_entity>(
			reg.find_or_register_parsed_entity(type->getAsTagDecl()->getCanonicalDecl())
		);
		result.type = type;
		return result;
	}
}
