#pragma once

#include <iostream>
#include <cassert>
#include <string_view>
#include <algorithm>
#include <vector>

namespace apigen {
	struct _details {
		/// Object for supporting trackable new lines.
		struct new_line_object {
			/// Default constructor.
			new_line_object() = default;
			/// Initializes all fields of this struct.
			explicit new_line_object(size_t di) : deindents(di) {
			}

			size_t deindents = 0; /// See \ref export_writer::start_line().
		};
	};
	/// Writes a trackable new line to an \ref export_writer.
	inline _details::new_line_object new_line(size_t deindents = 0) {
		return _details::new_line_object(deindents);
	}

	/// Utility class used when generating code.
	class export_writer {
	public:
		/// Initializes the output stream.
		explicit export_writer(std::ostream &out) : _out(out) {
		}

		/// Starts a scope with the given delimiters. The beginning delimiter should \emph not end with a new line.
		export_writer &start_scope(std::string_view beg, std::string_view end) {
			*this << beg;
			_scopes.emplace_back(end);
			return *this;
		}
		/// Ends the current scope. Note that this function does not append the final new line if the delimiter does
		/// not contain one. If a \ref new_line() has been written in this scope, then the closing delimiter will be
		/// on a new line.
		export_writer &end_scope() {
			assert(!_scopes.empty());
			_scope_info si = _scopes.back();
			_scopes.pop_back();
			if (si.has_new_line) {
				(*this << "\n").indent();
			}
			return *this << si.closing_delimiter;
		}

		/// Writes appropriate indentation to the output stream.
		///
		/// \param deindents The number of indents to ignore temporarily for this line.
		/// \return The associated output stream.
		export_writer &indent(size_t deindents = 0) {
			size_t ids = _scopes.size() - std::min(deindents, _scopes.size());
			for (size_t i = 0; i < ids; ++i) {
				*this << "\t";
			}
			return *this;
		}

		/// Support for manipulators.
		export_writer &operator<<(export_writer &(*func)(export_writer&)) {
			return func(*this);
		}
		/// Support for \ref new_line(). This sets \ref _scope_info::has_new_line to \p true for the top level
		/// scope.
		export_writer &operator<<(const _details::new_line_object &obj) {
			if (!_scopes.empty()) {
				_scopes.back().has_new_line = true;
			}
			return *this << "\n";
		}
	private:
		/// Helps overload resolution in \ref operator<<().
		template <typename T> struct _op_helper {
			using type = export_writer&; ///< The return type.
		};
	public:
		/// Supports using stream operators with the \ref export_writer object.
		template <typename Obj> auto operator<<(
			Obj &&rhs
		) -> typename _op_helper<decltype(_out << std::forward<Obj>(rhs))>::type {
			_out << std::forward<Obj>(rhs);
			return *this;
		}
	protected:
		/// Contains information about a scope.
		struct _scope_info {
			/// Default constructor.
			_scope_info() = default;
			/// Initializes all fields of this struct.
			explicit _scope_info(std::string_view closing) : closing_delimiter(closing) {
			}

			std::string_view closing_delimiter; ///< String written to the output stream when closing this scope.
			bool has_new_line = false; ///< Records if a \ref new_line() has been outputted during this scope.
		};

		std::ostream &_out; ///< The output stream.
		std::vector<_scope_info> _scopes; ///< The scopes that this writer is currently in.
	};


	/// Represents a scope in generated code.
	struct scope {
		/// Default constructor.
		constexpr scope() = default;
		/// Initializes all fields of this struct.
		constexpr scope(std::string_view b, std::string_view e) : begin(b), end(e) {
		}

		/// Starts the given scope.
		friend export_writer &operator<<(export_writer &writer, scope scope) {
			return writer.start_scope(scope.begin, scope.end);
		}

		std::string_view
			begin, ///< The delimiter that starts this scope.
			end; ///< The delimiter that ends this scope.
	};
	/// Contains definitions of default \ref scope's.
	namespace scopes {
		constexpr scope
			braces{"{", "}"}, ///< An \ref scope composed of braces.
			parentheses{"(", ")"}, ///< An \ref scope composed of parentheses.

			class_def{"{", "};"}; ///< An \ref scope for class definitions.
	}
}
