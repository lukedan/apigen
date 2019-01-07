#include "entity_types.h"

namespace apigen {
	std::string_view function_entity::get_operator_name(clang::OverloadedOperatorKind kind) {
		switch (kind) {
		case clang::OverloadedOperatorKind::OO_New:
			return "new";
		case clang::OverloadedOperatorKind::OO_Delete:
			return "delete";
		case clang::OverloadedOperatorKind::OO_Array_New:
			return "new_arr";
		case clang::OverloadedOperatorKind::OO_Array_Delete:
			return "delete_arr";
		case clang::OverloadedOperatorKind::OO_Plus:
			return "add";
		case clang::OverloadedOperatorKind::OO_Minus:
			return "subtract";
		case clang::OverloadedOperatorKind::OO_Star:
			return "multiply";
		case clang::OverloadedOperatorKind::OO_Slash:
			return "divide";
		case clang::OverloadedOperatorKind::OO_Percent:
			return "mod";
		case clang::OverloadedOperatorKind::OO_Caret:
			return "xor";
		case clang::OverloadedOperatorKind::OO_Amp:
			return "bitand";
		case clang::OverloadedOperatorKind::OO_Pipe:
			return "bitor";
		case clang::OverloadedOperatorKind::OO_Tilde:
			return "bitnot";
		case clang::OverloadedOperatorKind::OO_Exclaim:
			return "not";
		case clang::OverloadedOperatorKind::OO_Equal:
			return "assign";
		case clang::OverloadedOperatorKind::OO_Less:
			return "less";
		case clang::OverloadedOperatorKind::OO_Greater:
			return "greater";
		case clang::OverloadedOperatorKind::OO_PlusEqual:
			return "add_inplace";
		case clang::OverloadedOperatorKind::OO_MinusEqual:
			return "subtract_inplace";
		case clang::OverloadedOperatorKind::OO_StarEqual:
			return "multiply_inplace";
		case clang::OverloadedOperatorKind::OO_SlashEqual:
			return "divide_inplace";
		case clang::OverloadedOperatorKind::OO_PercentEqual:
			return "mod_inplace";
		case clang::OverloadedOperatorKind::OO_CaretEqual:
			return "xor_inplace";
		case clang::OverloadedOperatorKind::OO_AmpEqual:
			return "bitand_inplace";
		case clang::OverloadedOperatorKind::OO_PipeEqual:
			return "bitor_inplace";
		case clang::OverloadedOperatorKind::OO_LessLess:
			return "shift_left";
		case clang::OverloadedOperatorKind::OO_GreaterGreater:
			return "shift_right";
		case clang::OverloadedOperatorKind::OO_LessLessEqual:
			return "shift_left_inplace";
		case clang::OverloadedOperatorKind::OO_GreaterGreaterEqual:
			return "shift_right_inplace";
		case clang::OverloadedOperatorKind::OO_EqualEqual:
			return "equal";
		case clang::OverloadedOperatorKind::OO_ExclaimEqual:
			return "not_equal";
		case clang::OverloadedOperatorKind::OO_LessEqual:
			return "less_equal";
		case clang::OverloadedOperatorKind::OO_GreaterEqual:
			return "greater_equal";
		case clang::OverloadedOperatorKind::OO_Spaceship:
			return "spaceship";
		case clang::OverloadedOperatorKind::OO_AmpAmp:
			return "and";
		case clang::OverloadedOperatorKind::OO_PipePipe:
			return "or";
		case clang::OverloadedOperatorKind::OO_PlusPlus:
			return "increment";
		case clang::OverloadedOperatorKind::OO_MinusMinus:
			return "decrement";
		case clang::OverloadedOperatorKind::OO_Comma:
			return "comma";
		case clang::OverloadedOperatorKind::OO_ArrowStar:
			return "member_ptr";
		case clang::OverloadedOperatorKind::OO_Arrow:
			return "member";
		case clang::OverloadedOperatorKind::OO_Call:
			return "call";
		case clang::OverloadedOperatorKind::OO_Subscript:
			return "subscript";
		case clang::OverloadedOperatorKind::OO_Coawait:
			return "co_await";
		default:
			// includes OO_Conditional
			return "@INVALID";
		}
	}

	void enum_entity::export_api_types(const entity_registry &reg, export_writer &out) const {
		out.indent() << "enum " << name.compose() << " " << scopes::braces << new_line();
		{
			bool first = true;
			for (auto &c : constants) {
				if (first) {
					first = false;
				} else {
					out << "," << new_line();
				}
				out.indent() << c.name.compose() << " = " << c.value.toString(10);
			}
		}
		out.end_scope() << ";" << new_line();

		out << new_line();
	}

	void record_entity::export_api_types(const entity_registry &reg, export_writer &out) const {
		out.indent() << "typedef struct { /* opaque */ } " << name.compose() << ";" << new_line();
		out.indent() << "typedef struct " << scopes::braces << new_line();
		{
			out.indent() << name.compose() << " *object;" << new_line();
			out.indent() << "bool move;";
		}
		out.end_scope() << " " << move_struct_name.compose() << ";" << new_line();

		out << new_line();
	}


	void field_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		out.indent() <<
			type.get_field_return_type_spelling() << " (*" << api_getter_name.compose() << ")(" <<
			parent->name.compose() << "*);" << new_line();
		// TODO const version?
	}

	void function_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		out.indent() <<
			return_type.get_return_type_spelling() << " (*" << api_name.compose() << ")" << scopes::parentheses;
		{
			bool first = true;
			for (const parameter &p : params) {
				if (first) {
					first = false;
				} else {
					out << ", ";
				}
				out << p.type.get_parameter_spelling();
			}
		}
		out.end_scope() << ";" << new_line();
	}

	void method_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		auto *methoddecl = llvm::cast<clang::CXXMethodDecl>(declaration);
		if (methoddecl->isStatic()) {
			function_entity::export_api_struct_func_ptrs(reg, out);
		} else {
			out.indent() <<
				return_type.get_return_type_spelling() << " (*" << api_name.compose() << ")" << scopes::parentheses <<
				parent->name.compose() << get_this_pointer_qualifiers() << "*";
			for (const parameter &p : params) {
				out << ", " << p.type.get_parameter_spelling();
			}
			out.end_scope() << ";" << new_line();
		}
	}


	void field_entity::export_host_functions(const entity_registry&, export_writer &out) const {
		std::string rettype = type.get_field_return_type_spelling();
		clang::QualType parent_ty(llvm::cast<clang::CXXRecordDecl>(parent->declaration)->getTypeForDecl(), 0);

		out.indent() <<
			"static " << rettype << " " << host_getter_name.compose() << "(" << parent->name.compose() << " *obj) " <<
			scopes::braces << new_line();
		{
			out.indent() <<
				"auto *obj_internal = reinterpret_cast<" <<
				parent_ty.getAsString(get_cpp_printing_policy()) << "*>(obj);" << new_line();
			out.indent() <<
				"return reinterpret_cast<" << rettype << ">(&(obj_internal->" << declaration->getName().str() <<
				"));";
		}
		out.end_scope() << new_line();

		out << new_line();
	}

	/// Contains information about a parameter.
	struct _param_info {
		/// Initializes this struct from the given \ref name_scope and \ref function_entity::parameter.
		_param_info(name_scope &scope, const function_entity::parameter &p) : param(p) {
			type_spelling = param.type.get_parameter_spelling();
			std::string n(param.get_name());
			name = scope.register_name(n).compose();
			tmp_name = scope.register_name(n + "_tmp").compose(); // TODO magic string
		}

		std::string
			type_spelling, ///< The full spelling of its type.
			name, ///< The name of this parameter.
			tmp_name; ///< The name of the temporary variable.
		const function_entity::parameter &param; ///< The associated \ref function_entity::parameter.
	};
	void function_entity::export_host_functions(const entity_registry &reg, export_writer &out) const {
		name_scope func_scope(&reg.global_scope);
		std::vector<_param_info> params_info;
		for (const parameter &p : params) {
			params_info.emplace_back(func_scope, p);
		}
		std::string retname = func_scope.register_name("return_value").compose();

		auto *funcdecl = llvm::cast<clang::FunctionDecl>(declaration);
		std::vector<clang::NamedDecl*> env;
		for (auto *ctx = funcdecl->getParent(); !ctx->isTranslationUnit(); ctx = ctx->getParent()) {
			env.emplace_back(llvm::cast<clang::NamedDecl>(ctx));
		}

		out.indent() <<
			"static " << return_type.get_return_type_spelling() << " " << host_name.compose() << scopes::parentheses;
		bool first = true;
		for (const _param_info &p : params_info) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << p.type_spelling << " " << p.name;
		}
		out.end_scope() << scopes::braces << new_line();
		{
			for (const _param_info &p : params_info) {
				p.param.type.export_prepare_pass_argument(out, p.name, p.tmp_name);
			}
			return_type.export_prepare_return(out, retname);
			{
				// function name
				out << "::";
				for (auto it = env.rbegin(); it != env.rend(); ++it) {
					out << (*it)->getName().str() << "::";
				}
				out << funcdecl->getName().str() << scopes::parentheses;
				{ // parameters
					first = true;
					for (const _param_info &p : params_info) {
						if (first) {
							first = false;
						} else {
							out << ",";
						}
						out << new_line();
						out.indent();
						p.param.type.export_pass_argument(out, p.name, p.tmp_name);
					}
				}
				out.end_scope();
			}
			return_type.export_return(out, retname);
		}
		out.end_scope() << new_line();

		out << new_line();
	}

	void method_entity::export_host_functions(const entity_registry &reg, export_writer &out) const {
		auto *methoddecl = llvm::cast<clang::CXXMethodDecl>(declaration);
		if (methoddecl->isStatic()) {
			function_entity::export_host_functions(reg, out);
		} else {
			name_scope method_scope(reg.global_scope);
			std::vector<_param_info> params_info;
			for (const parameter &p : params) {
				params_info.emplace_back(method_scope, p);
			}
			std::string
				objname = method_scope.register_name("subject").compose(),
				retname = method_scope.register_name("return_value").compose();

			out.indent() <<
				"static " << return_type.get_return_type_spelling() << " " << host_name.compose() <<
				scopes::parentheses;
			{
				out << parent->name.compose() << get_this_pointer_qualifiers() << " *" << objname;
				for (const _param_info &p : params_info) {
					out << ", " << p.type_spelling << " " << p.name;
				}
			}
			out.end_scope() << scopes::braces << new_line();
			{
				for (const _param_info &p : params_info) {
					p.param.type.export_prepare_pass_argument(out, p.name, p.tmp_name);
				}
				return_type.export_prepare_return(out, retname);
				{
					out <<
						"reinterpret_cast<" << parent->get_internal_type_name() << get_this_pointer_qualifiers() <<
						"*>(" << objname << ")->" << methoddecl->getDeclName().getAsString() << scopes::parentheses;
					{
						bool first = true;
						for (const _param_info &p : params_info) {
							if (first) {
								first = false;
							} else {
								out << ",";
							}
							out << new_line();
							out.indent();
							p.param.type.export_pass_argument(out, p.name, p.tmp_name);
						}
					}
					out.end_scope();
				}
				return_type.export_return(out, retname);
			}
			out.end_scope() << new_line();

			out << new_line();
		}
	}


	void field_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.indent() <<
			api_struct_name << "." << api_getter_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_getter_name.compose() << ";" << new_line();
	}

	void function_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.indent() <<
			api_struct_name << "." << api_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_name.compose() << ";" << new_line();
	}

	void method_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.indent() <<
			api_struct_name << "." << api_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_name.compose() << ";" << new_line();
	}
}
