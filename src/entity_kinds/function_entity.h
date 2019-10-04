#pragma once

/// \file
/// Entity that represents a function.

#include <optional>

#include <clang/AST/Decl.h>

#include "../entity.h"
#include "../types.h"
#include "../dependency_analyzer.h"

namespace apigen::entities {
	/// Entity that represents a function.
	class function_entity : public entity {
	public:
		constexpr static entity_kind kind = entity_kind::function; ///< The \ref entity_kind.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Holds infomation about a function parameter.
		struct parameter_info {
			/// Initializes \ref type.
			explicit parameter_info(qualified_type ty) : type(std::move(ty)) {
			}
			/// Initializes all fields of this struct.
			parameter_info(qualified_type ty, std::string n) : type(std::move(ty)), name(std::move(n)) {
			}

			qualified_type type; ///< The type of this parameter.
			std::string name; ///< The name of this parameter.
		};

		/// Initializes \ref _decl.
		explicit function_entity(clang::FunctionDecl *decl) : _decl(decl) {
		}

		/// Also exports parameter types and the return type.
		void gather_dependencies(entity_registry &reg, dependency_analyzer &queue) override {
			_collect_api_return_type(reg);
			_build_parameter_list(reg);

			if (_api_return_type && _api_return_type->type_entity) {
				queue.try_queue(*_api_return_type->type_entity);
			}
			for (auto &param : _parameters) { // queue parameter types for exporting
				if (param.type.type_entity) {
					queue.try_queue(*param.type.type_entity);
				}
			}
		}

		/// Returns the list of parameters.
		[[nodiscard]] const std::vector<parameter_info> &get_parameters() const {
			return _parameters;
		}
		/// Returns the return type.
		[[nodiscard]] const std::optional<qualified_type> &get_api_return_type() const {
			return _api_return_type;
		}

		/// Returns \ref _decl.
		[[nodiscard]] clang::FunctionDecl *get_declaration() const {
			return _decl;
		}
		/// Returns \ref _decl.
		[[nodiscard]] clang::NamedDecl *get_generic_declaration() const override {
			return _decl;
		}
	protected:
		std::optional<qualified_type> _api_return_type; ///< The return type of the API function.
		std::vector<parameter_info> _parameters; ///< Information about all parameters.
		clang::FunctionDecl *_decl = nullptr; ///< The \p clang::FunctionDecl.

		/// Populates \ref _parameters with the set of parameters the exported function should have.
		virtual void _build_parameter_list(entity_registry &reg) {
			size_t pos = 0;
			for (auto &param : _decl->parameters()) { // gather parameter types
				_parameters.emplace_back(qualified_type::from_clang_type(param->getType(), reg));
				for (clang::FunctionDecl *redecl : _decl->redecls()) { // gather parameter name
					llvm::StringRef name = redecl->parameters()[pos]->getName();
					if (name.size() > _parameters.back().name.size()) {
						_parameters.back().name = name.str();
					}
				}
				++pos;
			}
		}
		/// Sets \ref _api_return_type.
		virtual void _collect_api_return_type(entity_registry &reg) {
			if (!_decl->isNoReturn()) {
				_api_return_type.emplace(qualified_type::from_clang_type(_decl->getReturnType(), reg));
			}
		}
	};
}
