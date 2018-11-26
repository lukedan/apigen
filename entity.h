#pragma once

#include <iostream>
#include <string>

#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

#include "apigen_definitions.h"

namespace apigen {
	class entity_registry;

	struct naming_conventions {
		std::string
			host_function_prefix,
			api_prefix,
			separator,

			field_getter_prefix,
			field_getter_postfix,
			field_setter_prefix,
			field_setter_postfix,

			function_pointer_postfix;

		int
			namespace_levels = -1,
			parent_class_levels = -1;
	};

	class entity {
	public:
		explicit entity(clang::NamedDecl *decl) {
			declaration = decl;
		}
		virtual ~entity() = default;

		inline static const clang::Type *strip_type(clang::QualType qty) {
			const clang::Type *ty = qty.getCanonicalType().getTypePtr();
			if (auto *refty = llvm::dyn_cast<clang::ReferenceType>(ty); refty) { // strip reference type
				ty = refty->getPointeeType().getTypePtr();
			}
			while (true) { // strip array and pointer types
				if (auto *ptrty = llvm::dyn_cast<clang::PointerType>(ty); ptrty) {
					ty = ptrty->getPointeeType().getTypePtr();
					continue;
				}
				if (auto *arrty = llvm::dyn_cast<clang::ArrayType>(ty); arrty) {
					ty = arrty->getElementType().getTypePtr();
					continue;
				}
				break;
			}
			return ty;
		}
		inline static std::string get_pointer_type_postfix(clang::QualType qty) {
			std::string result;
			for (qty = qty.getCanonicalType(); ; result = "*" + result) {
				if (qty.isConstQualified()) {
					result = "const " + result;
				}
				const clang::Type *ty = qty.getTypePtr();
				if (auto *ptrty = llvm::dyn_cast<clang::PointerType>(ty); ptrty) {
					qty = ptrty->getPointeeType();
					continue;
				}
				if (auto *arrty = llvm::dyn_cast<clang::ArrayType>(ty); arrty) {
					qty = arrty->getElementType();
					continue;
				}
				break;
			}
			return result;
		}
		std::string get_environment_name(clang::Decl*, const entity_registry&, const naming_conventions&, bool);

		virtual void register_declaration(clang::NamedDecl *decl) {
			for (clang::Attr *attr : decl->getAttrs()) { // process annotations
				if (attr->getKind() == clang::attr::Kind::Annotate) {
					clang::AnnotateAttr *annotate = clang::cast<clang::AnnotateAttr>(attr);
					if (!consume_annotation(annotate->getAnnotation())) {
						std::cerr <<
							declaration->getName().str() <<
							": unknown annotation " << annotate->getAnnotation().str() << "\n";
					}
				}
			}
		}
		virtual bool consume_annotation(llvm::StringRef anno) {
			if (anno == APIGEN_ANNOTATION_EXPORT) {
				is_exported = true;
				return true;
			}
			if (anno == APIGEN_ANNOTATION_EXCLUDE) {
				is_excluded = true;
				return true;
			}
			if (anno.startswith(APIGEN_ANNOTATION_RENAME_PREFIX)) {
				if (substitute_name.size() > 0) {
					std::cerr <<
						declaration->getName().str() <<
						": substitute name " << substitute_name << " discarded\n";
				}
				size_t prefixlen = std::strlen(APIGEN_ANNOTATION_RENAME_PREFIX);
				substitute_name = std::string(anno.begin() + prefixlen, anno.end());
				return true;
			}
			return false;
		}

		virtual void propagate_export(entity_registry&);
		std::string get_declaration_name() const {
			return substitute_name.empty() ? declaration->getName().str() : substitute_name;
		}
		virtual void cache_export_names(const entity_registry&, const naming_conventions&);

		// virtual void export_host_additional(std::ostream&) = 0;
		// virtual void export_c_api_header(std::ostream&, const naming_conventions &conv) = 0;

		clang::NamedDecl *declaration;
		std::string substitute_name;
		bool
			is_exported = false,
			is_excluded = false;
	};
}
