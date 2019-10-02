#pragma once

/// \file
/// Used to determine the names of exported entities.

#include <string>

#include "entity.h"
#include "entity_kinds/constructor_entity.h"
#include "entity_kinds/enum_entity.h"
#include "entity_kinds/field_entity.h"
#include "entity_kinds/function_entity.h"
#include "entity_kinds/method_entity.h"
#include "entity_kinds/record_entity.h"
#include "entity_kinds/user_type_entity.h"

namespace apigen {
	/// Determines the names of exported entities.
	class naming_convention {
	public:
		/// Default virtual destructor.
		virtual ~naming_convention() = default;

		// functions used to get the names of an entity
		/// Returns the name of the given \ref entities::function_entity.
		[[nodiscard]] virtual std::string get_function_name(const entities::function_entity&) const = 0;
		/// Returns the name of the given \ref entities::method_entity.
		[[nodiscard]] virtual std::string get_method_name(const entities::method_entity&) const = 0;
		/// Returns the name of the given \ref entities::constructor_entity.
		[[nodiscard]] virtual std::string get_constructor_name(const entities::constructor_entity&) const = 0;

		/// Returns the name of the given \ref entities::user_type_entity.
		[[nodiscard]] virtual std::string get_user_type_name(const entities::user_type_entity&) const = 0;
		/// Returns the name of the given \ref entities::record_entity. By default this function returns the result of
		/// \ref get_user_type_name().
		[[nodiscard]] virtual std::string get_record_name(const entities::record_entity &ent) const {
			return get_user_type_name(ent);
		}
		/// Returns the name of the given \ref entities::enum_entity. By default this function returns the result of
		/// \ref get_user_type_name().
		[[nodiscard]] virtual std::string get_enum_name(const entities::enum_entity &ent) const {
			return get_user_type_name(ent);
		}

		// functions used to get names related to an entity
		/// Returns the exported name of the destructor of the given \ref entities::record_entity.
		[[nodiscard]] virtual std::string get_record_destructor_api_name(const entities::record_entity&) const = 0;

		/// Returns the name of an enumerator in the enum declaration.
		[[nodiscard]] virtual std::string get_enumerator_name(
			const entities::enum_entity&, clang::EnumConstantDecl*
		) const = 0;

		/// Returns the exported name of the non-const getter of the given field.
		[[nodiscard]] virtual std::string get_field_getter_name(const entities::field_entity&) const = 0;
		/// Returns the exportedname of the const getter of the given field.
		[[nodiscard]] virtual std::string get_field_const_getter_name(const entities::field_entity&) const = 0;

		// functions below are used to dispatch the call to the corresponding type
		/// Dispatches the call to \ref get_enum_name() or \ref get_record_name() depending on the actual type of the
		/// given \ref entities::uesr_type_entity.
		[[nodiscard]] std::string get_user_type_name_dynamic(const entities::user_type_entity &ent) const {
			if (auto *record_ent = dyn_cast<entities::record_entity>(&ent)) {
				return get_record_name(*record_ent);
			}
			if (auto *enum_ent = dyn_cast<entities::enum_entity>(&ent)) {
				return get_enum_name(*enum_ent);
			}
			return "$BADTYPE";
		}
		/// Dispatches the call to \ref get_function_name(), \ref get_method_name(), or \ref get_constructor_name()
		/// depending on the actual type of the given \ref entities::uesr_type_entity.
		[[nodiscard]] std::string get_function_name_dynamic(const entities::function_entity &ent) const {
			if (auto *method_ent = dyn_cast<entities::method_entity>(&ent)) {
				if (auto *constructor_ent = dyn_cast<entities::constructor_entity>(method_ent)) {
					return get_constructor_name(*constructor_ent);
				}
				return get_method_name(*method_ent);
			}
			return get_function_name(ent);
		}

		/// Dispatches the call to the one corresponding to the specific type of the given entity. Note that this
		/// function does not handle \ref entities::field_entity, as their names are not needed when exporting.
		[[nodiscard]] std::string get_entity_name_dynamic(const entity &ent) const {
			if (auto *user_type_ent = dyn_cast<entities::user_type_entity>(&ent)) {
				return get_user_type_name_dynamic(*user_type_ent);
			}
			if (auto *func_ent = dyn_cast<entities::function_entity>(&ent)) {
				return get_function_name_dynamic(*func_ent);
			}
			return "$UNKNOWN_ENTITY_TYPE";
		}
	};

	/// Naming information of overloaded operators.
	struct overloaded_operator_naming {
		std::string_view
			new_name{"new"}, ///< The name of <cc>operator new</cc>.
			delete_name{"delete"}, ///< The name of <cc>operator delete</cc>.
			array_new_name{"array_new"}, ///< The name of <cc>operator new[]</cc>.
			array_delete_name{"array_delete"}, ///< The name of <cc>operator delete[]</cc>.
			plus_name{"add"}, ///< The name of \p operator+.
			minus_name{"subtract"}, ///< The name of \p operator-.
			star_name{"multiply"}, ///< The name of \p operator*.
			slash_name{"divide"}, ///< The name of \p operator/.
			percent_name{"mod"}, ///< The name of \p operator%.
			caret_name{"bitwise_xor"}, ///< The name of \p operator^.
			amp_name{"bitwise_and"}, ///< The name of \p operator&.
			pipe_name{"bitwise_or"}, ///< The name of \p operator|.
			tilde_name{"bitwise_not"}, ///< The name of \p operator~.
			exclaim_name{"not"}, ///< The name of \p operator!.
			equal_name{"assign"}, ///< The name of \p operator=.
			less_name{"less"}, ///< The name of \p operator<.
			greater_name{"greater"}, ///< The name of \p operator>.
			plus_equal_name{"add_inplace"}, ///< The name of \p operator+=.
			minus_equal_name{"subtract_inplace"}, ///< The name of \p operator-=.
			star_equal_name{"multiply_inplace"}, ///< The name of \p operator*=.
			slash_equal_name{"divide_inplace"}, ///< The name of \p operator/=.
			percent_equal_name{"mod_inplace"}, ///< The name of \p operator%=.
			caret_equal_name{"bitwise_xor_inplace"}, ///< The name of \p operator^=.
			amp_equal_name{"bitwise_and_inplace"}, ///< The name of \p operator&=.
			pipe_equal_name{"bitwise_or_inplace"}, ///< The name of \p operator|=.
			less_less_name{"left_shift"}, ///< The name of \p operator<<.
			greater_greater_name{"right_shift"}, ///< The name of \p operator>>.
			less_less_equal_name{"left_shift_inplace"}, ///< The name of \p operator<<=.
			greater_greater_equal_name{"right_shift_inplace"}, ///< The name of \p operator>>=.
			equal_equal_name{"equal"}, ///< The name of \p operator==.
			exclaim_equal_name{"not_equal"}, ///< The name of \p operator!=.
			less_equal_name{"less_equal"}, ///< The name of \p operator<=.
			greater_equal_name{"greater_equal"}, ///< The name of \p operator>=.
			spaceship_name{"spaceship"}, ///< The name of \p operator<=>.
			amp_amp_name{"and"}, ///< The name of \p operator&&.
			pipe_pipe_name{"or"}, ///< The name of \p operator||.
			plus_plus_name{"increment"}, ///< The name of \p operator++.
			minus_minus_name{"decrement"}, ///< The name of \p operator--.
			comma_name{"comma"}, ///< The name of \p operator,.
			arrow_star_name{"access_memptr"}, ///< The name of \p operator->*.
			arrow_name{"access"}, ///< The name of \p operator->.
			call_name{"call"}, ///< The name of \p operator().
			subscript_name{"index"}, ///< The name of \p operator[].
			coawait_name{"co_await"}; ///< The name of <cc>operator co_await</cc>.

		/// Retrieves the name that corresponds to the given overloaded operator.
		[[nodiscard]] std::string_view get_name(clang::OverloadedOperatorKind op) const {
			switch (op) {
			case clang::OO_None:                return "";
			case clang::OO_New:                 return new_name;
			case clang::OO_Delete:              return delete_name;
			case clang::OO_Array_New:           return array_new_name;
			case clang::OO_Array_Delete:        return array_delete_name;
			case clang::OO_Plus:                return plus_name;
			case clang::OO_Minus:               return minus_name;
			case clang::OO_Star:                return star_name;
			case clang::OO_Slash:               return slash_name;
			case clang::OO_Percent:             return percent_name;
			case clang::OO_Caret:               return caret_name;
			case clang::OO_Amp:                 return amp_name;
			case clang::OO_Pipe:                return pipe_name;
			case clang::OO_Tilde:               return tilde_name;
			case clang::OO_Exclaim:             return exclaim_name;
			case clang::OO_Equal:               return equal_name;
			case clang::OO_Less:                return less_name;
			case clang::OO_Greater:             return greater_name;
			case clang::OO_PlusEqual:           return plus_equal_name;
			case clang::OO_MinusEqual:          return minus_equal_name;
			case clang::OO_StarEqual:           return star_equal_name;
			case clang::OO_SlashEqual:          return slash_equal_name;
			case clang::OO_PercentEqual:        return percent_equal_name;
			case clang::OO_CaretEqual:          return caret_equal_name;
			case clang::OO_AmpEqual:            return amp_equal_name;
			case clang::OO_PipeEqual:           return pipe_equal_name;
			case clang::OO_LessLess:            return less_less_name;
			case clang::OO_GreaterGreater:      return greater_greater_name;
			case clang::OO_LessLessEqual:       return less_less_equal_name;
			case clang::OO_GreaterGreaterEqual: return greater_greater_equal_name;
			case clang::OO_EqualEqual:          return equal_equal_name;
			case clang::OO_ExclaimEqual:        return exclaim_equal_name;
			case clang::OO_LessEqual:           return less_equal_name;
			case clang::OO_GreaterEqual:        return greater_equal_name;
			case clang::OO_Spaceship:           return spaceship_name;
			case clang::OO_AmpAmp:              return amp_amp_name;
			case clang::OO_PipePipe:            return pipe_pipe_name;
			case clang::OO_PlusPlus:            return plus_plus_name;
			case clang::OO_MinusMinus:          return minus_minus_name;
			case clang::OO_Comma:               return comma_name;
			case clang::OO_ArrowStar:           return arrow_star_name;
			case clang::OO_Arrow:               return arrow_name;
			case clang::OO_Call:                return call_name;
			case clang::OO_Subscript:           return subscript_name;
			case clang::OO_Conditional:         return "$ERROR_SHOULDNT_HAPPEN";
			case clang::OO_Coawait:             return coawait_name;
			}
		}
	};

	/// A naming convention that uses string formatters to generate names.
	class fmt_naming_convention : public naming_convention {
	public:
		/// Returns the function name.
		[[nodiscard]] std::string get_function_name(const entities::function_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::FunctionDecl>(entity.get_declaration());
			return fmt::format(
				function_name_pattern,
				"scope"_a = _get_scope_name(decl),
				"name"_a = _get_function_name(decl)
			);
		}
		/// Returns the method name.
		[[nodiscard]] std::string get_method_name(const entities::method_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::FunctionDecl>(entity.get_declaration());
			auto *parent = llvm::cast<clang::CXXRecordDecl>(decl->getParent());
			return fmt::format(
				method_name_pattern,
				"scope"_a = _get_scope_name(parent),
				"class"_a = to_string_view(parent->getName()),
				"name"_a = _get_function_name(decl)
			);
		}
		/// Returns the constructor name.
		[[nodiscard]] std::string get_constructor_name(const entities::constructor_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::DeclContext>(entity.get_declaration());
			auto *parent = llvm::cast<clang::CXXRecordDecl>(decl->getParent());
			return fmt::format(
				constructor_name_pattern,
				"scope"_a = _get_scope_name(parent),
				"class"_a = to_string_view(parent->getName())
			);
		}

		/// Returns the type name.
		[[nodiscard]] std::string get_user_type_name(const entities::user_type_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::TagDecl>(entity.get_declaration());
			return fmt::format(
				user_type_name_pattern,
				"scope"_a = _get_scope_name(decl),
				"name"_a = to_string_view(decl->getName())
			);
		}

		/// Returns the exported name of the destructor of the given \ref entities::record_entity.
		[[nodiscard]] std::string get_record_destructor_api_name(
			const entities::record_entity &entity
		) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::CXXRecordDecl>(entity.get_declaration());
			return fmt::format(
				destructor_name_pattern,
				"scope"_a = _get_scope_name(decl),
				"class"_a = to_string_view(decl->getName())
			);
		}

		/// Returns the name of an enumerator in the enum declaration.
		[[nodiscard]] std::string get_enumerator_name(
			const entities::enum_entity &entity, clang::EnumConstantDecl *enumerator
		) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::EnumDecl>(entity.get_declaration());
			return fmt::format(
				enumerator_name_pattern,
				"scope"_a = _get_scope_name(decl),
				"type"_a = to_string_view(decl->getName()),
				"name"_a = to_string_view(enumerator->getName())
			);
		}

		/// Returns the exported name of the non-const getter of the given field.
		[[nodiscard]] std::string get_field_getter_name(const entities::field_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::FieldDecl>(entity.get_declaration());
			auto *parent = decl->getParent();
			return fmt::format(
				field_getter_name_pattern,
				"scope"_a = _get_scope_name(parent),
				"class"_a = to_string_view(parent->getName()),
				"name"_a = to_string_view(decl->getName())
			);
		}
		/// Returns the exportedname of the const getter of the given field.
		[[nodiscard]] std::string get_field_const_getter_name(const entities::field_entity &entity) const override {
			using namespace fmt::literals;

			auto *decl = llvm::cast<clang::FieldDecl>(entity.get_declaration());
			auto *parent = decl->getParent();
			return fmt::format(
				field_const_getter_name_pattern,
				"scope"_a = _get_scope_name(parent),
				"class"_a = to_string_view(parent->getName()),
				"name"_a = to_string_view(decl->getName())
			);
		}

		overloaded_operator_naming operator_naming; ///< Naming information of overloaded operators.
		std::string_view
			scope_separator{"_"}, ///< Separator between scopes.
			function_name_pattern{"{scope}_{name}"}, ///< The pattern for function names.
			method_name_pattern{"{scope}_{class}_{name}"}, ///< The pattern for method names.
			constructor_name_pattern{"{scope}_{class}_ctor"}, ///< The pattern for constructor names.
			user_type_name_pattern{"{scope}_{name}"}, ///< The pattern for user type names.
			destructor_name_pattern{"{scope}_{class}_dtor"}, ///< The pattern for destructor names.
			enumerator_name_pattern{"{scope}_{type}_{name}"}, ///< The pattern for enumerator names.
			/// The pattern for field getter names.
			field_getter_name_pattern{"{scope}_{class}_{name}_getter"},
			/// The pattern for field const getter names.
			field_const_getter_name_pattern{"{scope}_{class}_{name}_const_getter"};
	protected:
		/// Returns the concatenated names of all parent scopes of the given declaration.
		[[nodiscard]] inline static std::string _get_scope_name(
			clang::DeclContext *context, std::string_view separator
		) {
			std::string result;
			for (clang::DeclContext *current = context->getParent(); current; current = current->getParent()) {
				if (auto *named = llvm::dyn_cast<clang::NamedDecl>(current)) {
					// TODO optimization opportunity
					if (!result.empty()) {
						result = std::string(separator) + result;
					}
					result = std::string(to_string_view(named->getName())) + result;
				}
			}
			return result;
		}
		/// \overload
		[[nodiscard]] std::string _get_scope_name(clang::DeclContext *context) const {
			return _get_scope_name(context, scope_separator);
		}

		/// Returns the name of the given function.
		[[nodiscard]] std::string_view _get_function_name(clang::FunctionDecl *decl) const {
			if (decl->isOverloadedOperator()) {
				return operator_naming.get_name(decl->getOverloadedOperator());
			}
			return to_string_view(decl->getName());
		}
	};
}