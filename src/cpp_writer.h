#pragma once

/// \file
/// Contains the \ref apigen::cpp_writer class.

#include <vector>
#include <set>
#include <ostream>
#include <string_view>
#include <variant>

#include "internal_name_printer.h"

namespace apigen {
	/// Allocates names for variables.
	class name_allocator {
	public:
		/// Replaces all invalid characters in the \p std::string with `_'.
		inline static void replace_invalid_identifier_characters_in(std::string &s) {
			for (char &c : s) {
				if (!(std::isdigit(c) || std::isalpha(c) || c == '_')) {
					c = '_';
				}
			}
		}
		/// Similar to \ref replace_invalid_identifier_characters_in(), except that this function returns the
		/// processed string.
		[[nodiscard]] inline static std::string replace_invalid_identifier_characters(std::string s) {
			replace_invalid_identifier_characters_in(s);
			return s;
		}
		/// Information of a single named entity.
		struct name_info {
			/// Default constructor.
			name_info() = default;
			/// Initializes all fields of this struct.
			name_info(std::string n, std::string disambig) :
				name(std::move(n)), disambiguation_postfix(std::move(disambig)) {
				replace_invalid_identifier_characters_in(name);
				replace_invalid_identifier_characters_in(disambiguation_postfix);
			}

			/// Returns the final composited name.
			[[nodiscard]] std::string get_name() const {
				if (!postfix_used) {
					return name;
				}
				if (numbering == 0) {
					return name + disambiguation_postfix;
				}
				// TODO add some sort of separator?
				return name + disambiguation_postfix + std::to_string(numbering);
			}

			std::string
				name, ///< The name of this object.
				disambiguation_postfix; ///< The postfix that is used for disambiguation.
			/// The numbering appended to this name when there're still conflicts after appending the postfix to this
			/// name. If this is zero, there's no numbering for this entity.
			std::size_t numbering = 0;
			bool postfix_used = false; ///< Whether \ref disambiguation_postfix is used in this name.
		};
		using token = std::unique_ptr<name_info>; ///< The token returned to the caller.

		/// Default constructor.
		name_allocator() = default;

		/// Tries to register a variable with the given name. A postfix will be appended to the name if there are
		/// conflicts.
		///
		/// \param name The shorter name of the object that will be used if there are no conflicts.
		/// \param disambig The postfix that would be appended to the name to try and resolve conflicts.
		token allocate_variable_custom(std::string name, std::string disambig) {
			auto token = std::make_unique<name_info>(std::move(name), std::move(disambig));
			bool occupied = false;
			if (is_immutable_mode()) {
				occupied = _is_name_occupied(token->name);
			} else {
				auto &&[scope, it] = _find_occupied_name(token->name);
				if (scope) {
					if (it->second) {
						name_info *occupant = it->second;
						it->second = nullptr;
						_resolve_first_level_conflict(occupant);
					}
					occupied = true;
				}
			}
			if (occupied) { // resolve first level name conflicts
				_resolve_first_level_conflict(token.get());
			} else {
				_names.emplace(token->get_name(), token.get());
			}
			return token;
		}
		/// Tries to register a variable name with a prefix.
		token allocate_variable_prefix(std::string_view prefix, std::string name, std::string disambig) {
			if (name.empty()) {
				name = "unnamed";
			}
			return allocate_variable_custom(std::string(prefix) + std::move(name), std::move(disambig));
		}

		/// Allocates the name for a function parameter.
		token allocate_function_parameter(std::string name, std::string disambig) {
			return allocate_variable_prefix("_apigen_priv_param_", std::move(name), std::move(disambig));
		}
		/// Allocates the name for a local variable.
		token allocate_local_variable(std::string name, std::string disambig) {
			return allocate_variable_prefix("_apigen_priv_local_", std::move(name), std::move(disambig));
		}

		/// Indicates whether names allocated by this allocator can be changed afterwards.
		[[nodiscard]] bool is_immutable_mode() const {
			return std::holds_alternative<const name_allocator*>(_parent);
		}

		/// Returns a \ref name_allocator initialized with the given parent.
		[[nodiscard]] inline static name_allocator from_parent(name_allocator &alloc) {
			return name_allocator(std::in_place_type<name_allocator*>, &alloc);
		}
		/// Returns an immutable \ref name_allocator initialized with the given parent.
		[[nodiscard]] inline static name_allocator from_parent_immutable(const name_allocator &alloc) {
			return name_allocator(std::in_place_type<const name_allocator*>, &alloc);
		}
	protected:
		/// The mapping between names and tokens.
		using _name_mapping = std::map<std::string, name_info*, std::less<void>>;

		/// All registered variable names. If a name is `conflicted', it will still exist but the pointer will be
		/// empty.
		_name_mapping _names;
		/// The \ref name_allocator for the parent scope.
		std::variant<name_allocator*, const name_allocator*> _parent;

		/// Initializes \ref _parent directly.
		template <typename ...Args> name_allocator(Args &&...args) : _parent(std::forward<Args>(args)...) {
		}

		/// Returns the parent as \p const.
		[[nodiscard]] const name_allocator *_get_const_parent() const {
			if (std::holds_alternative<name_allocator*>(_parent)) {
				return std::get<name_allocator*>(_parent);
			}
			return std::get<const name_allocator*>(_parent);
		}
		/// Checks if the given name is occupied, and if so, returns the \ref name_allocator that owns that name, and
		/// an iterator to that name.
		[[nodiscard]] bool _is_name_occupied(std::string_view name) const {
			for (const name_allocator *cur = this; cur; cur = cur->_get_const_parent()) {
				if (auto it = cur->_names.find(name); it != cur->_names.end()) {
					return true;
				}
			}
			return false;
		}
		/// Checks if the given name is occupied, and if so, returns the \ref name_allocator that owns that name, and
		/// an iterator to that name.
		std::pair<name_allocator*, _name_mapping::iterator> _find_occupied_name(std::string_view name) {
			for (name_allocator *cur = this; cur; cur = std::get<name_allocator*>(cur->_parent)) {
				if (auto it = cur->_names.find(name); it != cur->_names.end()) {
					return {cur, it};
				}
			}
			return {nullptr, _name_mapping::iterator()};
		}

		/// Resolves the conflict of disambiguated names by appending a number to the name.
		void _resolve_second_level_conflict(name_info *name) {
			if (name->numbering == 0) {
				for (name->numbering = 1; ; ++name->numbering) {
					std::string name_str = name->get_name();
					if (!_is_name_occupied(name_str)) {
						_names.emplace(std::move(name_str), name);
						break;
					}
				}
			} // otherwise this is an already existing name, and the conflict will be resolved by the new name
		}
		/// Resolves the conflict of base names by appending the disambiguation postfix to the name.
		void _resolve_first_level_conflict(name_info *name) {
			if (name->postfix_used) {
				_resolve_second_level_conflict(name);
			} else {
				name->postfix_used = true;
				std::string name_str = name->get_name();
				bool occupied = false;
				if (is_immutable_mode()) {
					occupied = _is_name_occupied(name_str);
				} else {
					auto &&[scope, ent] = _find_occupied_name(name_str);
					if (scope) {
						if (ent->second) {
							_resolve_second_level_conflict(ent->second);
							ent->second = nullptr;
						}
						occupied = true;
					}
				}
				if (occupied) { // try to resolve second level conflict
					_resolve_second_level_conflict(name);
				} else { // otherwise register the name for now
					_names.emplace(std::move(name_str), name);
				}
			}
		}
	};

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
		explicit cpp_writer(std::ostream &out, clang::PrintingPolicy policy) :
			name_printer(std::move(policy)), _out(out) {
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
			if (!_scopes.empty() && !_scopes.back().has_newline) {
				++_indent;
				_scopes.back().has_newline = true;
			}
			for (std::size_t i = 0; i < _indent; ++i) {
				write("\t");
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

		internal_name_printer name_printer; ///< The \ref internal_name_printer.
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
			if (_scopes.back().has_newline) {
				--_indent;
				new_line();
			}
			write(_scopes.back().end);
			_scopes.pop_back();
		}

		std::vector<_scope_rec> _scopes; ///< All current scopes.
		std::string_view _separator; ///< The pending separator.
		std::ostream &_out; ///< The output.
		std::size_t _indent = 0; ///< The level of indentation.
	};
}
