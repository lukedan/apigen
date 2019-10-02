#pragma once

/// \file
/// Contains the \ref apigen::entities::record_entity class.

#include <clang/AST/DeclCXX.h>

#include "../entity.h"
#include "user_type_entity.h"

namespace apigen::entities {
	/// An entity that corresponds to a \p class or a \p struct.
	class record_entity : public user_type_entity {
	public:
		constexpr static entity_kind kind = entity_kind::record; ///< The type of this entity.
		/// Returns the kind of this entity.
		[[nodiscard]] entity_kind get_kind() const override {
			return kind;
		}

		/// Initializes \ref _decl.
		explicit record_entity(clang::CXXRecordDecl *decl) : user_type_entity(decl) {
		}

		/// Gathers all dependencies for this record type.
		void gather_dependencies(entity_registry&, dependency_analyzer&) override;

		/// Handles the \p apigen_recursive attribute.
		bool handle_attribute(std::string_view anno) override {
			if (anno == APIGEN_ANNOTATION_RECURSIVE) {
				_recursive = true;
				return true;
			}
			return entity::handle_attribute(anno);
		}

		/// This entity should be trimmed if it's a local class, if it has no definition, or if it is in a templated
		/// context.
		[[nodiscard]] bool should_trim() const override {
			if (auto *def = llvm::cast<clang::CXXRecordDecl>(_decl->getDefinition())) {
				return def->isLocalClass() || def->isDependentContext();
			}
			return true; // no definition, trim
		}
	protected:
		bool _recursive = false; ///< Indicates whether members of this record should be exported.
	};
}
