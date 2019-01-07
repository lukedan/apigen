#include "qualified_type.h"

#include "entity_types.h"
#include "type_exporters.h"

using std::string;
using clang::LangOptions;
using clang::PrintingPolicy;
using clang::QualType;
using clang::Type;

namespace apigen {
	string qualified_type::get_exported_base_type_name() const {
		const Type *basety = base_type;
		if (auto *recdecl = dyn_cast<record_entity>(entity); recdecl) {
			return entity->name.compose();
		}
		if (auto *enumdecl = dyn_cast<enum_entity>(entity); enumdecl) {
			basety = enumdecl->get_integral_type();
		}
		return QualType(basety, 0).getAsString(get_cpp_printing_policy());
	}


	void type_exporter::export_prepare_pass_argument(
		export_writer&, const qualified_type&, std::string_view, std::string_view
	) const {
	}

	const type_exporter *type_exporter::get_matching_exporter(QualType qty) {
		static value_type_exporter _value;
		static pointer_type_exporter _pointer;
		static lvalue_reference_type_exporter _lvalue_ref;
		static rvalue_reference_to_object_exporter _rvalue_ref_to_obj;
		static rvalue_reference_to_pointer_exporter _rvalue_ref_to_ptr;

		if (qty->isRValueReferenceType()) {
			QualType pty = qty->getPointeeType();
			if (pty->isPointerType() || pty->isArrayType()) {
				return &_rvalue_ref_to_ptr;
			} else {
				return &_rvalue_ref_to_obj;
			}
		} else if (qty->isLValueReferenceType()) {
			return &_lvalue_ref;
		} else if (qty->isPointerType() || qty->isArrayType()) {
			return &_pointer;
		} else {
			return &_value;
		}
	}
}
