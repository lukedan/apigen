#pragma once

/// \file
/// Used when analyzing the dependency between entities.

#include <stack>
#include <iostream>

#include "entity.h"

namespace apigen {
	class entity_registry;

	/// Used when analyzing the dependency between entities.
	class dependency_analyzer {
	public:
		/// Queues the entity if it's not already marked for exporting.
		void try_queue(entity &ent) {
			if (!ent.is_marked_for_exporting()) {
				ent.mark_for_exporting();
				queue(ent);
			}
		}
		/// Queues the given entity without checking if it has already been marked.
		void queue(entity &ent) {
			std::cerr << "exporting: " << to_string_view(ent.get_generic_declaration()->getName()) << "\n";
			_queue.emplace(&ent);
		}

		/// Analyzes dependencies in the given \ref entity_registry.
		void analyze(entity_registry&);
	protected:
		std::stack<entity*> _queue; ///< Queued entities that need exporting.
	};
}
