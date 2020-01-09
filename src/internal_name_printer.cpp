#include "internal_name_printer.h"

/// \file
/// Implementation of \ref apigen::internal_name_printer.

#include "misc.h"
#include "types.h"

namespace apigen {
	std::string_view internal_name_printer::get_internal_operator_spelling(clang::OverloadedOperatorKind kind) {
		switch (kind) {
		case clang::OO_New:                 return "operator new";
		case clang::OO_Delete:              return "operator delete";
		case clang::OO_Array_New:           return "operator new[]";
		case clang::OO_Array_Delete:        return "operator delete[]";
		case clang::OO_Plus:                return "operator+";
		case clang::OO_Minus:               return "operator-";
		case clang::OO_Star:                return "operator*";
		case clang::OO_Slash:               return "operator/";
		case clang::OO_Percent:             return "operator%";
		case clang::OO_Caret:               return "operator^";
		case clang::OO_Amp:                 return "operator&";
		case clang::OO_Pipe:                return "operator|";
		case clang::OO_Tilde:               return "operator~";
		case clang::OO_Exclaim:             return "operator!";
		case clang::OO_Equal:               return "operator=";
		case clang::OO_Less:                return "operator<";
		case clang::OO_Greater:             return "operator>";
		case clang::OO_PlusEqual:           return "operator+=";
		case clang::OO_MinusEqual:          return "operator-=";
		case clang::OO_StarEqual:           return "operator*=";
		case clang::OO_SlashEqual:          return "operator/=";
		case clang::OO_PercentEqual:        return "operator%=";
		case clang::OO_CaretEqual:          return "operator^=";
		case clang::OO_AmpEqual:            return "operator&=";
		case clang::OO_PipeEqual:           return "operator|=";
		case clang::OO_LessLess:            return "operator<<";
		case clang::OO_GreaterGreater:      return "operator>>";
		case clang::OO_LessLessEqual:       return "operator<<=";
		case clang::OO_GreaterGreaterEqual: return "operator>>=";
		case clang::OO_EqualEqual:          return "operator==";
		case clang::OO_ExclaimEqual:        return "operator!=";
		case clang::OO_LessEqual:           return "operator<=";
		case clang::OO_GreaterEqual:        return "operator>=";
		case clang::OO_Spaceship:           return "operator<=>";
		case clang::OO_AmpAmp:              return "operator&&";
		case clang::OO_PipePipe:            return "operator||";
		case clang::OO_PlusPlus:            return "operator++";
		case clang::OO_MinusMinus:          return "operator--";
		case clang::OO_Comma:               return "operator,";
		case clang::OO_ArrowStar:           return "operator->*";
		case clang::OO_Arrow:               return "operator->";
		case clang::OO_Call:                return "operator()";
		case clang::OO_Subscript:           return "operator[]";
		case clang::OO_Coawait:             return "operator co_await";
		default:
			return "$BAD_OPERATOR";
		}
	}

	std::string internal_name_printer::_get_template_argument_spelling(const clang::TemplateArgument &arg) const {
		switch (arg.getKind()) {
		case clang::TemplateArgument::Null:
			return "$ERROR_NULL";
		case clang::TemplateArgument::Type:
			return get_internal_qualified_type_name(qualified_type::from_clang_type(arg.getAsType(), nullptr));
		// TODO The template argument is a declaration that was provided for a pointer,
		// reference, or pointer to member non-type template parameter.
		case clang::TemplateArgument::Declaration:
			break;
		case clang::TemplateArgument::NullPtr:
			return "nullptr";
		case clang::TemplateArgument::Integral:
			return arg.getAsIntegral().toString(10);
		// TODO The template argument is a template name that was provided for a
		// template template parameter.
		case clang::TemplateArgument::Template:
			break;
		// TODO The template argument is a pack expansion of a template name that was
		// provided for a template template parameter.
		case clang::TemplateArgument::TemplateExpansion:
			break;
		case clang::TemplateArgument::Expression:
			return "$UNSUPPORTED_TEMPLATE_ARG";
		case clang::TemplateArgument::Pack:
			return _get_template_argument_list_spelling(arg.getPackAsArray());
		}
		return "$UNSUPPORTED_TEMPLATE_ARG";

		/*std::string result;
		llvm::raw_string_ostream stream(result);
		arg.print(internal_printing_policy, stream);
		stream.str();
		return result;*/
	}

	std::string internal_name_printer::_get_template_argument_list_spelling(
		llvm::ArrayRef<clang::TemplateArgument> args
	) const {
		std::string result;
		for (auto &arg : args) {
			if (!result.empty()) {
				result += ", ";
			}
			result += _get_template_argument_spelling(arg);
		}
		return result;
	}

	std::string_view internal_name_printer::get_internal_function_name(clang::FunctionDecl *decl) {
		clang::OverloadedOperatorKind op_kind = decl->getOverloadedOperator();
		if (op_kind != clang::OO_None) {
			return get_internal_operator_spelling(op_kind);
		}
		return to_string_view(llvm::cast<clang::NamedDecl>(decl)->getName());
	}

	std::string internal_name_printer::get_internal_entity_name(clang::DeclContext *decl) const {
		std::string result;
		if (auto *func_decl = llvm::dyn_cast<clang::FunctionDecl>(decl)) {
			result = std::string(get_internal_function_name(func_decl));
		} else {
			if (auto *template_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
				result =
					"<" + _get_template_argument_list_spelling(template_decl->getTemplateArgs().asArray()) + ">";
			}
			result = llvm::cast<clang::NamedDecl>(decl)->getName().str() + result;
		}
		result = "::" + result;

		for (
			clang::DeclContext *current = decl->getParent();
			current && !current->isTranslationUnit();
			current = current->getParent()
			) {
			if (auto *record_decl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(current)) {
				result =
					"<" + _get_template_argument_list_spelling(record_decl->getTemplateArgs().asArray()) +
					">" + result;
			}
			auto *named_decl = llvm::cast<clang::NamedDecl>(current);
			result = "::" + named_decl->getName().str() + result;
		}
		return result;
	}

	std::string internal_name_printer::get_internal_type_name(const clang::Type *type) const {
		if (auto *builtin = llvm::dyn_cast<clang::BuiltinType>(type)) {
			return std::string(to_string_view(builtin->getName(policy)));
		}
		if (auto *tagty = llvm::dyn_cast<clang::TagType>(type)) {
			return get_internal_entity_name(tagty->getAsTagDecl());
		}
		assert_true(
			!llvm::isa<clang::FunctionProtoType>(type),
			"get_internal_type_name cannot handle function types; "
			"use get_internal_qualified_type_name() instead"
		);
		return "$UNSUPPORTED";
	}

	std::string internal_name_printer::get_internal_qualified_type_name(
		const clang::Type *type, reference_kind ref_kind,
		std::initializer_list<qualifier> extra_quals, const qualifier *quals, std::size_t qual_count
	) const {
		std::stringstream ss;
		if (auto *functy = llvm::dyn_cast<clang::FunctionProtoType>(type)) {
			std::stack<const clang::FunctionProtoType*> def;
			auto qty = qualified_type::from_clang_type(functy->getReturnType(), nullptr);
			_begin_return_type(ss, def, qty.type, qty.ref_kind, qty.qualifiers.data(), qty.qualifiers.size());

			std::size_t total_count = _get_qualifier_count(extra_quals, qual_count);
			if (ref_kind != reference_kind::none || total_count > 1) { // pointer or reference
				ss << "(";
				_write_qualifiers_and_pointers(ss, ref_kind, extra_quals, quals, qual_count);
				ss << ")";
			} else { // function type
				assert_true(_get_qualifier_at(0, extra_quals, quals, qual_count) == qualifier::none);
			}

			// parameters
			ss << "(";
			bool first = true;
			for (const clang::QualType &param : functy->param_types()) {
				if (first) {
					first = false;
				} else {
					ss << ", ";
				}
				ss << get_internal_qualified_type_name(qualified_type::from_clang_type(param, nullptr));
			}
			ss << ")";

			while (!def.empty()) {
				_end_return_type(ss, def.top());
				def.pop();
			}
		} else {
			ss << get_internal_type_name(type);
			_write_qualifiers_and_pointers(ss, ref_kind, extra_quals, quals, qual_count);
		}
		return ss.str();
	}
}
