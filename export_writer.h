#pragma once

#include <iostream>
#include <cassert>
#include <string_view>
#include <algorithm>
#include <vector>

namespace apigen {
	/// Represents an indented scope when generating code.
	struct indented_scope {
		/// Default constructor.
		constexpr indented_scope() = default;
		/// Initializes all fields of this struct.
		constexpr indented_scope(std::string_view b, std::string_view e) : begin(b), end(e) {
		}

		std::string_view
			begin, ///< The delimiter that starts this scope.
			end; ///< The delimiter that ends this scope.
	};
	/// Contains definitions of default \ref indented_scope's.
	namespace scopes {
		constexpr indented_scope
			braces{"{", "}"}, ///< An \ref indented_scope composed of braces.
			parentheses{"(", ")"}, ///< An \ref indented_scope composed of parentheses.

			class_def{"{", "};"}; ///< An \ref indented_scope for class definitions.
	}
	/// Utility class used when generating code.
	class export_writer {
	public:
		/// Initializes the output stream.
		explicit export_writer(std::ostream &out) : _out(out) {
		}

		/// Starts an indented scope with the given delimiters. The beginning delimiter should \emph not end with a new
		/// line.
		export_writer &start_indented_scope(std::string_view beg, std::string_view end) {
			*this << beg << "\n";
			_scopes.emplace_back(end);
			return *this;
		}
		/// \overload
		export_writer &start_indented_scope(indented_scope scope) {
			return start_indented_scope(scope.begin, scope.end);
		}
		/// Ends the current indented scope. Note that this function does not append the final new line if the
		/// delimiter does not contain one, nor does it output a new line before this line.
		///
		/// \return The associated output stream.
		export_writer &end_indented_scope() {
			assert(!_scopes.empty());
			std::string_view ends = _scopes.back().closing_delimiter;
			_scopes.pop_back();
			return start_line() << ends;
		}

		/// Writes appropriate indentation to the output stream.
		///
		/// \param deindents The number of indents to ignore temporarily for this line.
		/// \return The associated output stream.
		export_writer &start_line(size_t deindents = 0) {
			size_t ids = _scopes.size() - std::min(deindents, _scopes.size());
			for (size_t i = 0; i < ids; ++i) {
				*this << "\t";
			}
			return *this;
		}

		/// Supports using stream operators straight with the \ref export_writer object.
		template <typename Obj> export_writer &operator<<(Obj &&rhs) {
			_out << std::forward<Obj>(rhs);
			return *this;
		}
		/// Starts the given scope.
		export_writer &operator<<(indented_scope scope) {
			return start_indented_scope(scope);
		}
	protected:
		/// Contains information about an indented scope.
		struct _indented_scope_info {
			/// Default constructor.
			_indented_scope_info() = default;
			/// Initializes all fields of this struct.
			explicit _indented_scope_info(std::string_view closing) : closing_delimiter(closing) {
			}

			std::string_view closing_delimiter; ///< String written to the output stream when closing this scope.
		};

		std::ostream &_out; ///< The output stream.
		std::vector<_indented_scope_info> _scopes; ///< The scopes that this writer is currently in.
	};
}
