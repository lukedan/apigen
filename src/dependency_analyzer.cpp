#include "dependency_analyzer.h"

/// \file
/// Implementation of certain methods of \ref apigen::dependency_analyzer.

#include "entity_registry.h"

namespace apigen {
	void dependency_analyzer::analyze(entity_registry &reg) {
		for (auto &pair : reg.get_entities()) {
			if (pair.second->is_marked_for_exporting()) {
				queue(*pair.second);
			}
		}
		while (!_queue.empty()) {
			entity *ent = _queue.top();
			_queue.pop();
			ent->gather_dependencies(reg, *this);
		}
	}
}
