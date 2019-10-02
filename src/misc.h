#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace apigen {
	/// Marks whether bitwise operators are enabled for a particular enum class type. Specialize this type to enable
	/// these operators for an enum type.
	template <typename> struct enable_enum_bitwise_operators : std::false_type {
	};
	/// Marks whether bitwise operators are enabled for a particular enum class type.
	template <typename T> constexpr static bool enable_enum_bitwise_operators_v =
		enable_enum_bitwise_operators<T>::value;

	/// Bitwise and for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator&(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) & static_cast<_base>(rhs));
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator|(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) | static_cast<_base>(rhs));
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator^(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) ^ static_cast<_base>(rhs));
	}
	/// Bitwise not for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator~(Enum v) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(~static_cast<_base>(v));
	}

	/// Bitwise and for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator&=(Enum &lhs, Enum rhs) {
		return lhs = lhs & rhs;
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator|=(Enum &lhs, Enum rhs) {
		return lhs = lhs | rhs;
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator^=(Enum &lhs, Enum rhs) {
		return lhs = lhs ^ rhs;
	}


	/// Assertion. This is so that the condition will always be executed.
	inline void assert_true(bool v, std::string_view message) {
		if (!v) {
			std::cerr << message << "\n";
			std::abort();
		}
	}
	/// \overload
	inline void assert_true(bool v) {
		if (!v) {
			std::abort();
		}
	}

	/// Converts a \p llvm::StringRef into a \p std::string_view.
	[[nodiscard]] inline std::string_view to_string_view(llvm::StringRef str) {
		return std::string_view(str.data(), str.size());
	}

	/// \p starts_with().
	inline bool TEMP_starts_with(std::string_view patt, std::string_view full) {
		if (full.size() < patt.size()) {
			return false;
		}
		for (auto i = patt.begin(), j = full.begin(); i != patt.end(); ++i, ++j) {
			if (*i != *j) {
				return false;
			}
		}
		return true;
	}
}
