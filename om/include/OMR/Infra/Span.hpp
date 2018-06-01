#if !defined(OMR_INFRA_SPAN_HPP_)
#define OMR_INFRA_SPAN_HPP_

#include <array>
#include <cstddef>

namespace OMR {
namespace Infra {
/// A span of memory. Pointer and length pair.
template<typename Type>
class Span {
public:
	// Zeroing constructor.
	inline constexpr Span() : value_{nullptr}, length_{0} {}

	/// Full initialization.
	inline constexpr Span(Type* value, std::size_t length) : value_{value}, length_{length} {}

	template<std::size_t N>
	inline constexpr Span(std::array<Type, N>& array) : value_(array.data()), length_(N) {}

	template<std::size_t N>
	inline constexpr Span(const std::array<Type, N>& array)
	        : value_(array.data()), length_(N) {}

	/// Init from pointer. Span has zero length.
	explicit inline constexpr Span(Type* value) : value_{value}, length_{0} {}

	/// Init from length. Value is nullptr.
	explicit inline constexpr Span(std::size_t length) : value_{nullptr}, length_{length} {}

	/// Assignment from span.
	template<typename U>
	inline constexpr Span(const Span<U>& other)
	        : value_{other.value()}, length_{other.length()} {}

	/// Array-index operator.
	constexpr Type& operator[](std::size_t index) const { return at(index); }

	/// Array-index operator. TODO: Bounds checking
	constexpr Type& at(std::size_t index) const { return value()[index]; }

	/// Get the pointer.
	inline constexpr auto operator()() const -> Type* { return value_; }

	/// Get the pointer.
	constexpr Type* value() const noexcept { return value_; }

	/// Set the pointer value.
	inline auto value(Type* value) -> Span<Type>& {
		value_ = value;
		return *this;
	}

	/// Number of elements in the span.
	inline constexpr auto length() const -> std::size_t { return length_; }

	/// Set the number of elements in the span.
	inline auto length(std::size_t length) -> Span<Type>& {
		length_ = length;
		return *this;
	}

	/// The last instance of Type in this Span.
	inline auto last() -> Type* { return value_ + (length_ - 1); }

	/// Size of the referred span. Equal to sizeof(T)*length.
	inline constexpr auto memoryFootPrint() const -> std::size_t {
		return length() * sizeof(Type);
	}

	inline auto operator=(Span<Type>& rhs) -> Span<Type>& {
		value_ = rhs.value();
		length_ = rhs.length();
		return *this;
	}

	/// Return a pointer to the start of the span.
	Type* begin() const noexcept { return value(); }

	/// Return an pointer that is "one past the end".
	Type* end() const noexcept { return value() + length(); }

	const Type* cbegin() const noexcept { return value(); }

	const Type* cend() const noexcept { return value() + length(); }

protected:
	Type* value_;
	std::size_t length_;
};

} // namespace Infra
} // namespace OMR

#endif // OMR_INFRA_SPAN_HPP_
