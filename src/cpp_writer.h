#pragma once

/// \file
/// Contains the \ref apigen::cpp_writer class.

#include <vector>
#include <set>
#include <ostream>
#include <string_view>

namespace apigen {
	/// A wrapper around \p std::ostream that provides additional functionalities for printing C++ code.
	class cpp_writer {
	public:
		/// Represents a scope.
		struct scope {
			/// Initializes \ref begin and \ref end.
			constexpr scope(std::string_view b, std::string_view e) noexcept : begin(b), end(e) {
			}

			std::string_view
				begin, ///< The opening sequence of this scope.
				end; ///< The closing sequence of this scope.
		};

		inline const static scope
			parentheses_scope{"(", ")"}, ///< A scope surrounded by parentheses.
			braces_scope{"{", "}"}; ///< A scope surrounded by brackets.

		/// Initializes \ref _out.
		explicit cpp_writer(std::ostream &out) : _out(out) {
		}
		/// No move construction.
		cpp_writer(cpp_writer&&) = delete;
		/// No copy constructor.
		cpp_writer(const cpp_writer&) = delete;
		/// No move assignment.
		cpp_writer &operator=(cpp_writer&&) = delete;
		/// No copy assignment.
		cpp_writer &operator=(const cpp_writer&) = delete;

		/// RTTI wrapper for scopes.
		struct scope_token {
			friend cpp_writer;
		public:
			/// Default constructor.
			scope_token() = default;
			/// Move constructor.
			scope_token(scope_token &&other) noexcept : _out(other._out) {
			}
			/// No copy construction.
			scope_token(const scope_token&) = delete;
			/// Move assignment.
			scope_token &operator=(scope_token &&other) noexcept {
				end();
				_out = other._out;
				other._out = nullptr;
				return *this;
			}
			/// No copy assignment.
			scope_token &operator=(const scope_token&) = delete;
			/// Calls \ref end().
			~scope_token() {
				end();
			}

			/// Ends the current scope.
			void end() {
				if (_out) {
					_out->_end_scope();
					_out = nullptr;
				}
			}
		protected:
			/// Initializes \ref _out.
			explicit scope_token(cpp_writer *w) : _out(w) {
			}

			cpp_writer *_out = nullptr; ///< The associated \ref cpp_writer.
		};
		/// RTTI wrapper for allocated variable names.
		struct variable_token {
			friend cpp_writer;
		public:
			/// Default constructor.
			variable_token() = default;
			/// Move constructor.
			variable_token(variable_token &&src) noexcept : _iter(src._iter), _writer(src._writer) {
				src._writer = nullptr;
			}
			/// No copy construction.
			variable_token(const variable_token&) = delete;
			/// Move assignment.
			variable_token &operator=(variable_token &&src) noexcept {
				discard();
				_iter = src._iter;
				_writer = src._writer;
				src._writer = nullptr;
				return *this;
			}
			/// No copy assignment.
			variable_token &operator=(const variable_token&) = delete;
			/// Calls \ref discard() automatically.
			~variable_token() {
				discard();
			}

			/// Returns whether this variable token is empty.
			[[nodiscard]] bool empty() const {
				return _writer == nullptr;
			}
			/// Returns the variable name.
			[[nodiscard]] std::string_view get() const {
				if (_writer) {
					return *_iter;
				}
				return "";
			}

			/// Discards this variable.
			void discard() {
				if (_writer) {
					_writer->_discard_variable(_iter);
					_writer = nullptr;
				}
			}
		protected:
			/// Directly initializes the fields of this struct.
			variable_token(cpp_writer &w, std::set<std::string>::const_iterator it) : _iter(it), _writer(&w) {
			}

			std::set<std::string>::const_iterator _iter; ///< The iterator to the underlying allocated name.
			cpp_writer *_writer = nullptr; ///< The associated \ref cpp_writer object.
		};

		// variable management
		/// Tries to register a variable with the given name. A postfix will be appended to the name if there are
		/// conflicts.
		variable_token allocate_variable_custom(std::string name) {
			auto it = _vars.lower_bound(name);
			if (it != _vars.end() && *it == name) {
				name += '_';
				for (size_t i = 2; ; ++i) {
					std::string num = std::to_string(i);
					name += num;
					it = _vars.lower_bound(name);
					if (it == _vars.end() || *it != name) {
						break;
					}
					name.erase(name.size() - num.size());
				}
			}
			auto result = _vars.emplace_hint(it, std::move(name));
			return variable_token(*this, result);
		}
		/// Tries to register a variable name with a prefix.
		variable_token allocate_variable_prefix(const char *prefix, std::string name) {
			if (name.empty()) {
				name = "unnamed";
			}
			return allocate_variable_custom(prefix + std::move(name));
		}

		/// Allocates the name for a function parameter.
		variable_token allocate_function_parameter(std::string name) {
			return allocate_variable_prefix("_apigen_priv_param_", std::move(name));
		}
		/// Allocates the name for a local variable.
		variable_token allocate_local_variable(std::string name) {
			return allocate_variable_prefix("_apigen_priv_local_", std::move(name));
		}

		// low-level output formatting
		/// Directly writes the given object to the output.
		template <typename T> cpp_writer &write(T &&obj) {
			_maybe_print_sepearator();
			_out << std::forward<T>(obj);
			return *this;
		}
		/// Uses \p fmt to format and write to the output stream.
		template <typename ...Args> cpp_writer &write_fmt(Args &&...args) {
			_maybe_print_sepearator();
			fmt::print(_out, std::forward<Args>(args)...);
			return *this;
		}
		/// Starts a new line.
		cpp_writer &new_line() {
			write("\n");
			for (size_t i = 0; i < _scopes.size(); ++i) {
				write("\t");
			}
			for (_scope_rec &scp : _scopes) {
				scp.has_newline = true;
			}
			return *this;
		}
		/// Pushes a scope onto \ref _scopes and starts that scope.
		[[nodiscard]] scope_token begin_scope(scope s) {
			_scopes.emplace_back(s);
			write(s.begin);
			return scope_token(this);
		}
		/// Adds a pending separator to the writer. If the next operation is \ref _end_scope(), this separator will be
		/// discarded; otherwise it will be written to the output before anything else.
		cpp_writer &maybe_separate(std::string_view text) {
			_maybe_print_sepearator();
			_separator = text;
			return *this;
		}
	protected:
		/// Records the state of a scope.
		struct _scope_rec {
			/// Default constructor.
			_scope_rec() = default;
			/// Records the given scope.
			explicit _scope_rec(scope s) : end(s.end) {
			}

			std::string_view end; ///< The end delimiter of this scope.
			bool has_newline = false; ///< Whether or not a new line has been written in this scope.
		};

		/// Prints and clears the separator if it's not empty.
		void _maybe_print_sepearator() {
			if (!_separator.empty()) {
				_out << _separator;
				_separator = std::string_view();
			}
		}
		/// Ends the bottom-level scope.
		void _end_scope() {
			_separator = std::string_view(); // discard separator
			_scope_rec ended = _scopes.back();
			_scopes.pop_back();
			if (ended.has_newline) {
				new_line();
			}
			write(ended.end);
		}

		/// Removes the allocated variable name from \ref _vars.
		void _discard_variable(std::set<std::string>::const_iterator iter) {
			_vars.erase(iter);
		}

		std::vector<_scope_rec> _scopes; ///< All current scopes.
		std::set<std::string> _vars; ///< All registered variable names.
		std::string_view _separator; ///< The pending separator.
		std::ostream &_out; ///< The output.
	};
}
