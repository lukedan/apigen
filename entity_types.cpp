#include "entity_types.h"

namespace apigen {
	void enum_entity::export_api_types(const entity_registry &reg, export_writer &out) const {
		out.start_line() << "enum " << name.compose() << " " << scopes::class_def;
		{
			bool first = true;
			for (auto &c : constants) {
				if (first) {
					first = false;
				} else {
					out << ",\n";
				}
				out.start_line() << c.name.compose() << " = " << c.value.toString(10);
			}
			out << "\n";
		}
		out.end_indented_scope() << "\n";

		out << "\n";
	}

	void record_entity::export_api_types(const entity_registry &reg, export_writer &out) const {
		out.start_line() << "typedef struct { /* opaque */ } " << name.compose() << ";\n";
		out.start_line() << "typedef struct " << scopes::braces;
		{
			out.start_line() << name.compose() << " *object;\n";
			out.start_line() << "bool move;\n";
		}
		out.end_indented_scope() << " " << move_struct_name.compose() << ";\n";

		out << "\n";
	}


	void field_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		out.start_line() <<
			type.get_field_rettype_spelling() << " (*" << api_getter_name.compose() << ")(" <<
			parent->name.compose() << "*);\n";
		// TODO const version?
	}

	void function_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		out.start_line() << return_type.get_temporary_spelling() << " (*" << api_name.compose() << ")(";
		bool first = true;
		for (const parameter &p : params) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << p.type.get_temporary_spelling();
		}
		out << ");\n";
	}

	void method_entity::export_api_struct_func_ptrs(const entity_registry &reg, export_writer &out) const {
		auto *methoddecl = llvm::cast<clang::CXXMethodDecl>(declaration);
		if (methoddecl->isStatic()) {
			function_entity::export_api_struct_func_ptrs(reg, out);
		} else {
			out.start_line() <<
				return_type.get_temporary_spelling() << " (*" << api_name.compose() << ")(" <<
				parent->name.compose() << get_this_pointer_qualifiers() << "*";
			for (const parameter &p : params) {
				out << ", " << p.type.get_temporary_spelling();
			}
			out << ");\n";
		}
	}


	void field_entity::export_host_functions(const entity_registry&, export_writer &out) const {
		std::string rettype = type.get_field_rettype_spelling();
		clang::QualType parent_ty(llvm::cast<clang::CXXRecordDecl>(parent->declaration)->getTypeForDecl(), 0);

		out.start_line() <<
			"static " << rettype << " " << host_getter_name.compose() << "(" << parent->name.compose() << " *obj) " <<
			scopes::braces;
		{
			out.start_line() <<
				"auto *obj_internal = reinterpret_cast<" <<
				parent_ty.getAsString(get_internal_type_printing_policy()) << "*>(obj);\n";
			out.start_line() <<
				"return reinterpret_cast<" << rettype << ">(&(obj_internal->" << declaration->getName().str() <<
				"));\n";
		}
		out.end_indented_scope() << "\n";

		out << "\n";
	}

	/// Contains information about a parameter.
	struct _param_info {
		/// Initializes this struct from the given \ref name_scope and \ref function_entity::parameter.
		_param_info(name_scope &scope, const function_entity::parameter &p) : param(p) {
			type_spelling = param.type.get_temporary_spelling();
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

		out.start_line() << "static " << return_type.get_temporary_spelling() << " " << host_name.compose() << "(";
		bool first = true;
		for (const _param_info &p : params_info) {
			if (first) {
				first = false;
			} else {
				out << ", ";
			}
			out << p.type_spelling << " " << p.name;
		}
		out << ") " << scopes::braces;
		{
			for (const _param_info &p : params_info) {
				p.param.type.export_convert_to_internal(out, p.name, p.tmp_name);
			}
			return_type.export_before_function_call(out, retname);
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
							out << ",\n";
						}
						p.param.type.export_pass_arg(out, p.name, p.tmp_name);
					}
					if (!first) {
						out << "\n";
					}
				}
				out.end_indented_scope();
			}
			return_type.export_after_function_call(out, retname);
		}
		out.end_indented_scope() << "\n";
		// TODO return value

		out << "\n";
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

			out.start_line() << "static " << return_type.get_temporary_spelling() << " " << host_name.compose() << "(";
			out << parent->name.compose() << get_this_pointer_qualifiers() << " *" << objname;
			for (const _param_info &p : params_info) {
				out << ", " << p.type_spelling << " " << p.name;
			}
			out << ") " << scopes::braces;
			{
				for (const _param_info &p : params_info) {
					p.param.type.export_convert_to_internal(out, p.name, p.tmp_name);
				}
				return_type.export_before_function_call(out, retname);
				{
					out <<
						"reinterpret_cast<" << parent->get_internal_type_name() << get_this_pointer_qualifiers() <<
						"*>(" << objname << ")->" << methoddecl->getName().str() << scopes::parentheses;
					{
						bool first = true;
						for (const _param_info &p : params_info) {
							if (first) {
								first = false;
							} else {
								out << ",\n";
							}
							p.param.type.export_pass_arg(out, p.name, p.tmp_name);
						}
						if (!first) {
							out << "\n";
						}
					}
					out.end_indented_scope();
				}
				return_type.export_after_function_call(out, retname);
			}
			out.end_indented_scope() << "\n";
			// TODO return value

			out << "\n";
		}
	}


	void field_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.start_line() <<
			api_struct_name << "." << api_getter_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_getter_name.compose() << ";\n";
	}

	void function_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.start_line() <<
			api_struct_name << "." << api_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_name.compose() << ";\n";
	}

	void method_entity::export_host_api_initializers(
		const entity_registry &reg, export_writer &out, std::string_view api_struct_name
	) const {
		out.start_line() <<
			api_struct_name << "." << api_name.compose() << " = " <<
			APIGEN_STR(APIGEN_API_CLASS_NAME) << "::" << host_name.compose() << ";\n";
	}
}
