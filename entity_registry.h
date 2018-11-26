#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <variant>

#include <clang/AST/DeclTemplate.h>

#include "entity.h"

namespace apigen {
	class enum_entity;
	class record_entity;

	using user_type_info = std::variant<std::nullptr_t, enum_entity*, record_entity*>;

	class entity_registry {
	public:
		using name_registry = std::unordered_map<std::string, size_t>;
		struct name_token {
			friend entity_registry;
		public:
			name_token() = default;

			std::string compose() const {
				if (_number == 0) {
					return _strit->first;
				}
				return _strit->first + std::to_string(_number);
			}
		protected:
			name_token(name_registry::const_iterator it, size_t n) : _strit(it), _number(n) {
			}

			name_registry::const_iterator _strit;
			size_t _number = 0;
		};

		template <typename T> bool register_entity(clang::NamedDecl *decl) {
			auto *canonical = llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl());
			auto [it, inserted] = entities.try_emplace(canonical);
			if (inserted) {
				it->second = std::make_unique<T>(canonical);
			}
			it->second->register_declaration(decl);
			return inserted;
		}
		bool register_template_specialization_entity(clang::ClassTemplateSpecializationDecl*);

	protected:
		std::deque<entity*> _expq;
		bool _queue_export(entity &ent) {
			if (exported_entities.emplace(&ent).second) {
				if (ent.is_excluded) {
					std::cerr << ent.declaration->getName().str() << ": overriden exclude flag\n";
					ent.is_excluded = false;
				}
				_expq.emplace_back(&ent);
				ent.is_exported = true;
				return true;
			}
			return false;
		}
	public:
		bool queue_exported_entity(entity &ent) {
			if (auto *tempdecl = llvm::dyn_cast<clang::TemplateDecl>(ent.declaration); tempdecl) {
				std::cerr << tempdecl->getName().str() << ": cannot export template\n";
				return false;
			}
			return _queue_export(ent);
		}
		user_type_info queue_dependent_type(clang::QualType);
		void propagate_export() {
			for (auto &pair : entities) {
				if (pair.second->is_exported) {
					queue_exported_entity(*pair.second);
				}
			}
			while (!_expq.empty()) {
				entity *ent = _expq.front();
				_expq.pop_front();
				ent->propagate_export(*this);
			}
		}

		name_token register_name(std::string n) {
			auto [it, inserted] = names.try_emplace(n, 0);
			if (!inserted) {
				++it->second;
			}
			return name_token(it, it->second);
		}

		entity *find_entity(clang::NamedDecl *decl) const {
			auto it = entities.find(llvm::cast<clang::NamedDecl>(decl->getCanonicalDecl()));
			return it == entities.end() ? nullptr : it->second.get();
		}

		std::unordered_map<clang::NamedDecl*, std::unique_ptr<entity>> entities;
		std::unordered_set<entity*> exported_entities;
		name_registry names;
	};


	void entity::propagate_export(entity_registry &reg) {
	}

	std::string entity::get_environment_name(
		clang::Decl *decl, const entity_registry &reg, const naming_conventions &conv, bool include_parent
	) {
		std::vector<clang::DeclContext*> decls;
		auto *cur = decl;
		while (cur) {
			auto *ctx = cur->getDeclContext();
			if (ctx == nullptr) {
				break;
			}
			decls.emplace_back(ctx);
			cur = llvm::dyn_cast<clang::Decl>(ctx);
		}
		int nspc = conv.namespace_levels, clsc = conv.parent_class_levels;
		std::string res = "";
		auto it = decls.rbegin();
		for (; it != decls.rend(); ++it) {
			if (nspc != 0) {
				if (auto *nsdecl = llvm::dyn_cast<clang::NamespaceDecl>(*it); nsdecl) {
					if (!res.empty()) {
						res += conv.separator;
					}
					entity *et = reg.find_entity(nsdecl);
					res += et ? et->get_declaration_name() : "NOTFOUND";
					--nspc;
					continue;
				}
			}
			if (clsc == 0) {
				break;
			}
			if (auto *clsdecl = llvm::dyn_cast<clang::CXXRecordDecl>(*it); clsdecl) {
				if (!res.empty()) {
					res += conv.separator;
				}
				entity *et = reg.find_entity(clsdecl);
				res += et ? et->get_declaration_name() : "NOTFOUND";
				--clsc;
				continue;
			}
		}
		if (include_parent && it != decls.rend()) {
			if (auto *pcdecl = llvm::dyn_cast<clang::CXXRecordDecl>(decls.front()); pcdecl) {
				if (!res.empty()) {
					res += conv.separator;
				}
				entity *et = reg.find_entity(pcdecl);
				res += et ? et->get_declaration_name() : "NOTFOUND";
			} else {
				std::cerr << "include_parent required but parent is not a record\n";
			}
		}
		return res;
	}

	void entity::cache_export_names(const entity_registry &reg, const naming_conventions &conv) {
		// TODO
	}
}
