#include "record_entity.h"

/// \file
/// Implementation of certain methods of \ref apigen::entities::record_entity.

#include "../cpp_writer.h"
#include "../dependency_analyzer.h"
#include "../entity_registry.h"
#include "../exporter.h"

namespace apigen::entities {
	std_function_custom_function_entity::std_function_custom_function_entity(record_entity &ent) : _entity(ent) {
		auto *decl = llvm::cast<clang::ClassTemplateSpecializationDecl>(_entity.get_declaration());
		_func_type = llvm::cast<clang::FunctionProtoType>(decl->getTemplateArgs()[0].getAsType().getTypePtr());
	}

	void std_function_custom_function_entity::gather_dependencies(entity_registry &reg, dependency_analyzer &dep) {
		_return_type = qualified_type::from_clang_type(_func_type->getReturnType(), &reg);
		for (const clang::QualType &param : _func_type->param_types()) {
			_param_types.emplace_back(qualified_type::from_clang_type(param, &reg));
		}

		if (_return_type.type_entity) {
			dep.try_queue(*_return_type.type_entity);
		}
		for (const qualified_type &qty : _param_types) {
			if (qty.type_entity) {
				dep.try_queue(*qty.type_entity);
			}
		}

		if (_return_type.is_record_type()) { // complex return requires <type_traits>
			reg.register_custom_host_dependency("type_traits");
		}
	}

	naming_convention::name_info std_function_custom_function_entity::get_suggested_name(
		naming_convention &conv
	) const {
		auto res = conv.get_record_name(_entity);
		res.name += "_from_raw";
		return res;
	}

	void std_function_custom_function_entity::_export_parameter_type(
		cpp_writer &writer, const exporter &ex, const qualified_type &qty, bool mark_temp
	) const {
		writer.write_fmt("{} ", ex.get_exported_type_name(qty.type, qty.type_entity));
		if (qty.is_reference_or_pointer()) {
			exporter::export_api_pointers_and_qualifiers(writer, qty.ref_kind, qty.qualifiers);
		} else {
			if (auto *complex_ty = dyn_cast<entities::record_entity>(qty.type_entity)) { // temporary object
				if (mark_temp) {
					// an empty marker that indicates this parameter is a temporary object
					writer.write(APIGEN_STR(APIGEN_TEMPORARY));
				}
				writer.write("*");
			} // nothing to do for other primitive types
		}
	}

	void std_function_custom_function_entity::_export_function_pointer_parameters(
		cpp_writer &writer, const exporter &ex, bool mark_temp
	) const {
		auto param_scope = writer.begin_scope(cpp_writer::parentheses_scope);
		for (const qualified_type &qty : _param_types) { // original parameters
			_export_parameter_type(writer, ex, qty, mark_temp);
			writer.maybe_separate(", ");
		}
		if (_return_type.is_record_type()) {
			// additional input pointing to the memory block that receives the return value
			writer
				.write("void*")
				.maybe_separate(", ");
		}
		// final input, user data
		writer.write("void*");
	}

	void std_function_custom_function_entity::_export_function_call(
		cpp_writer &writer, const exporter &ex,
		std::string_view fptr, const std::vector<std::string> &names, std::string_view output, std::string_view user
	) const {
		writer.write(fptr);
		{
			auto param_scope = writer.begin_scope(cpp_writer::parentheses_scope);
			auto param_it = names.begin();
			for (const qualified_type &qty : _param_types) { // pass parameters
				if (qty.is_reference_or_pointer()) { // cast references & pointers
					writer.write("reinterpret_cast<");
					_export_parameter_type(writer, ex, qty, false);
					writer.write(">(");
					if (qty.is_reference()) { // convert references to pointers
						writer.write("&");
					}
					writer.write_fmt("{})", *param_it);
				} else {
					if (auto *recty = dyn_cast<entities::record_entity>(qty.type_entity)) { // record
						// simply cast & pass the pointer to the function
						writer.write_fmt(
							"reinterpret_cast<{}*>(&{})",
							ex.get_record_names().at(recty).name.get_cached(), *param_it
						);
					} else { // primitive types
						if (auto *enumty = dyn_cast<entities::enum_entity>(qty.type_entity)) { // cast enums
							writer.write_fmt(
								"static_cast<{}>({})", ex.get_enum_names().at(enumty).name.get_cached(), *param_it
							);
						} else { // pass directly since they're the same type
							writer.write(*param_it);
						}
					}
				}
				writer.maybe_separate(", ");
				++param_it;
			}
			if (_return_type.is_record_type()) { // complex return type
				writer
					.write(output)
					.maybe_separate(", ");
			}
			writer
				.write(user)
				.maybe_separate(", ");
		}
	}

	void std_function_custom_function_entity::export_pointer_declaration(
		cpp_writer &writer, const exporter &ex, std::string_view name
	) const {
		writer.write_fmt("{} *(*{})", ex.get_record_names().at(&_entity).name.get_cached(), name);
		{
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);
			// first parameter, a function pointer
			ex.export_api_return_type(writer, _return_type);
			writer.write("(*)"); // function pointer
			_export_function_pointer_parameters(writer, ex, true);
			writer
				.maybe_separate(", ")
				// second parameter, a void* pointing to the memory block that receives the returned std::function
				.write("void*")
				.maybe_separate(", ")
				// final parameter, a void* that holds additional user data
				.write("void*");
		}
		writer.write(";");
	}

	void std_function_custom_function_entity::export_definition(
		cpp_writer &writer, const exporter &ex, std::string_view name
	) const {
		name_allocator alloc = name_allocator::from_parent_immutable(ex.get_implmentation_scope());
		name_allocator::token fptr_token, ret_ptr_token, user_data_token;
		std::string fptr_name, ret_ptr_name, user_data_name;

		bool complex_return = _return_type.is_record_type();
		std::string_view api_func_type = ex.get_record_names().at(&_entity).name.get_cached();
		writer.write_fmt("inline static {} *{}", api_func_type, name);
		{ // function parameters
			auto scope = writer.begin_scope(cpp_writer::parentheses_scope);

			// first parameter
			fptr_token = alloc.allocate_function_parameter("func_ptr", "");
			fptr_name = fptr_token->get_name();
			ex.export_api_return_type(writer, _return_type);
			writer.write_fmt("(*{})", fptr_name);
			_export_function_pointer_parameters(writer, ex, false);
			writer.maybe_separate(", ");
			// second parameter, return memory
			ret_ptr_token = alloc.allocate_function_parameter("ret_ptr", "");
			ret_ptr_name = ret_ptr_token->get_name();
			writer
				.write_fmt("void *{}", ret_ptr_name)
				.maybe_separate(", ");
			// final parameter, user data
			user_data_token = alloc.allocate_function_parameter("user_data", "");
			user_data_name = user_data_token->get_name();
			writer.write_fmt("void *{}", user_data_name);
		}
		{ // function body
			auto scope = writer.begin_scope(cpp_writer::braces_scope);

			name_allocator body_alloc = name_allocator::from_parent_immutable(alloc);
			std::vector<name_allocator::token> param_tokens;
			std::vector<std::string> param_names;

			writer
				.new_line()
				.write_fmt(
					"return reinterpret_cast<{}*>(new ({}) ::std::function<{}>([{}, {}]",
					api_func_type, ret_ptr_name,
					writer.name_printer.get_internal_qualified_type_name(
						_func_type, reference_kind::none, { qualifier::none }
					),
					fptr_name, user_data_name
				);
			{ // lambda parameters
				auto param_scope = writer.begin_scope(cpp_writer::parentheses_scope);

				for (const qualified_type &qty : _param_types) {
					param_tokens.emplace_back(body_alloc.allocate_function_parameter("param", ""));
					param_names.emplace_back(param_tokens.back()->get_name());
					writer
						.write_fmt(
							"{} {}", writer.name_printer.get_internal_qualified_type_name(qty), param_names.back()
						)
						.maybe_separate(", ");
				}
			}
			// trailing lambda return type
			writer.write_fmt(
				" -> {} ", writer.name_printer.get_internal_qualified_type_name(_return_type)
			);
			{ // lambda body
				auto body_scope = writer.begin_scope(cpp_writer::braces_scope);

				writer.new_line();
				if (complex_return) {
					name_allocator::token return_mem_token, return_ptr_token, return_token;
					std::string return_mem_name, return_ptr_name, return_name;
					return_mem_token = body_alloc.allocate_local_variable("result_mem", "");
					return_mem_name = return_mem_token->get_name();
					return_ptr_token = body_alloc.allocate_local_variable("result_ptr", "");
					return_ptr_name = return_ptr_token->get_name();
					return_token = body_alloc.allocate_local_variable("result", "");
					return_name = return_token->get_name();

					std::string type_name = writer.name_printer.get_internal_type_name(_return_type.type);

					// storage & pointer
					writer
						.write_fmt("std::aligned_storage_t<sizeof({0}), alignof({0})> {};", type_name, return_mem_name)
						.new_line()
						.write_fmt(
							"{0} *{} = reinterpret_cast<{0}*>(&{});", type_name, return_ptr_name, return_mem_name
						)
						.new_line();
					// function call
					writer.write_fmt("new ({}) {}(", return_ptr_name, type_name);
					_export_function_call(writer, ex, fptr_name, param_names, return_mem_name, user_data_name);
					writer
						.write(");")
						.new_line();
					// move to a plain object & return
					writer
						.write_fmt("{} {} = std::move(*{});", type_name, return_name, return_ptr_name)
						.new_line()
						// destroy original object in raw memory
						.write_fmt("{}->~{}();", return_ptr_name, type_name)
						.new_line()
						.write_fmt("return {};", return_name);
				} else {
					writer.write("return ");
					if (_return_type.is_reference_or_pointer()) { // return value needs casting
						cpp_writer::scope_token rref_cast_scope; // reinterpret_cast for rvalue references
						if (_return_type.ref_kind == reference_kind::rvalue_reference) {
							writer.write_fmt(
								"reinterpret_cast<{}>",
								writer.name_printer.get_internal_qualified_type_name(_return_type)
							);
							rref_cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
						}
						if (_return_type.is_reference()) {
							writer.write("*");
						}
						{
							cpp_writer::scope_token type_cast_scope;
							if (!_return_type.type->isBuiltinType()) { // reinterpret_cast only non-builtin types
								if (_return_type.is_reference()) {
									// dereference & use corresponding pointer type
									writer.write_fmt(
										"reinterpret_cast<{}>",
										writer.name_printer.get_internal_qualified_type_name(
											_return_type.type,
											reference_kind::none, // remove reference
											{ qualifier::none }, // extra qualifier for pointer
											_return_type.qualifiers.data(), _return_type.qualifiers.size()
										)
									);
								} else {
									writer.write_fmt(
										"reinterpret_cast<{}>",
										writer.name_printer.get_internal_qualified_type_name(_return_type)
									);
								}
								type_cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
							}
							_export_function_call(writer, ex, fptr_name, param_names, "", user_data_name);
						}
					} else { // plain builtin object or enum
						if (auto *enum_ty = llvm::dyn_cast<clang::EnumType>(_return_type.type)) { // cast enums
							writer.write_fmt("static_cast<{}>", writer.name_printer.get_internal_type_name(enum_ty));
							auto enum_cast_scope = writer.begin_scope(cpp_writer::parentheses_scope);
							_export_function_call(writer, ex, fptr_name, param_names, "", user_data_name);
						} else { // return builtin types directly
							_export_function_call(writer, ex, fptr_name, param_names, "", user_data_name);
						}
					}
					writer.write(";");
				}
			}
			writer.write("));");
		}
	}


	bool record_entity::is_move_constructor(clang::CXXConstructorDecl *decl) {
		// ensure that this constructor accepts one parameter
		if (decl->parameters().empty()) {
			return false;
		}
		for (auto it = decl->param_begin() + 1; it != decl->param_end(); ++it) {
			if (!(*it)->hasDefaultArg()) {
				return false;
			}
		}
		// inspect the first parameter
		clang::ParmVarDecl *param = decl->parameters()[0];
		if (param->isParameterPack()) { // ignore packs, this may produce false negatives
			return false;
		}
		auto qty = qualified_type::from_clang_type(param->getType(), nullptr);
		if (
			qty.ref_kind == reference_kind::rvalue_reference &&
			qty.qualifiers.size() == 1 &&
			qty.qualifiers.front() == qualifier::none
			) {
			if (qty.type == decl->getParent()->getTypeForDecl()) {
				return true;
			}
		}
		return false;
	}

	void record_entity::gather_dependencies(entity_registry &reg, dependency_analyzer &queue) {
		auto *def_decl = _decl->getDefinition();
		if (def_decl == nullptr) {
			return;
		}
		for (clang::CXXConstructorDecl *decl : def_decl->ctors()) {
			if (!decl->isDeleted() && is_move_constructor(decl)) {
				_move_constructor = true;
				break;
			}
		}
		// here we iterate over all child entities so that entities in template classes that are not marked as
		// recursive export can be discovered & exported correctly
		for (clang::Decl *decl : def_decl->decls()) {
			if (decl->getAccess() != clang::AS_public && !export_private_members()) {
				continue;
			}
			if (auto *named_decl = llvm::dyn_cast<clang::NamedDecl>(decl)) {
				if (auto *method_decl = llvm::dyn_cast<clang::CXXMethodDecl>(named_decl)) {
					clang::FunctionDecl::TemplatedKind tk = method_decl->getTemplatedKind();
					if (
						tk == clang::FunctionDecl::TK_FunctionTemplate ||
						tk == clang::FunctionDecl::TK_DependentFunctionTemplateSpecialization
						) { // template, do not export
						continue;
					}
				} else if (auto *record_decl = llvm::dyn_cast<clang::CXXRecordDecl>(named_decl)) {
					if (record_decl->getDescribedClassTemplate()) { // template, do not export
						continue;
					}
					if (record_decl->isImplicit()) {
						// TODO HACK for some reason an `implicit referenced class` with the same name is generated by
						//           clang *inside* the definition, which causes problems
						continue;
					}
				}
				if (entity *ent = reg.find_or_register_parsed_entity(named_decl)) {
					if (_recursive && !ent->is_excluded()) {
						queue.try_queue(*ent);
					}
				}
			}
		}

		if (is_std_function()) { // special handling of std::function
			auto custom_ent = std::make_unique<std_function_custom_function_entity>(*this);
			custom_ent->gather_dependencies(reg, queue);
			reg.register_custom_function(std::move(custom_ent));
		}
	}
}
