#if !defined(OMR_OM_REF_HPP_)
#define OMR_OM_REF_HPP_

#include <OMR/Om/Address.hpp>

#include <cstdint>
#include <type_traits>

namespace OMR {
namespace Om {

template<typename T = void>
class Ref;

/// Define accessor operations if T is not void
template<typename T>
class RefOperations {
public:
	constexpr T* operator->() const noexcept { return static_cast<const Ref<T>*>(this)->get(); }

	constexpr T& operator*() const noexcept { return *static_cast<const Ref<T>*>(this)->get(); }
};

/// Empty operation set for T
template<>
class RefOperations<void> {
public:
};

/// A pointer to managed memory. It is GC-Unsafe. This is a very simple
/// pointer-wrapper.
template<typename T>
class Ref : public RefOperations<T> {
public:
	Ref() = default;

	constexpr Ref(std::nullptr_t) : value_(nullptr) {}

	template<typename U>
	constexpr Ref(U* other) : value_(other) {}

	template<typename U>
	constexpr Ref(Ref<U> other) : value_(other.get()) {}

	inline Ref<T> operator=(std::nullptr_t) noexcept { value_ = nullptr; }

	template<typename U>
	inline Ref<T>& operator=(U* rhs) noexcept {
		value_ = rhs;
	}

	template<typename U>
	inline Ref<T> operator=(const Ref<U>& rhs) noexcept {
		value_ = rhs.template get<T>();
	}

	/// The address of the referrent.
	Address toAddress() const { return reinterpret_cast<Address>(value_); }

	/// Cast Ref<A> to Ref<B>
	template<typename U>
	constexpr Ref<U> to() const noexcept {
		return Ref<U>(value_);
	}

	/// Obtain the raw pointer
	template<typename U = T>
	constexpr U* get() const noexcept {
		return value_;
	}

	constexpr bool operator==(std::nullptr_t) const noexcept { return value_ == nullptr; }

	template<typename U>
	constexpr bool operator==(const Ref<U>& rhs) const noexcept {
		return value_ == rhs.get();
	}

	template<typename U>
	constexpr bool operator==(U* rhs) const noexcept {
		return value_ == rhs;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept { return value_ != nullptr; }

	template<typename U>
	constexpr bool operator!=(const Ref<U>& rhs) const noexcept {
		return value_ != rhs.get();
	}

	template<typename U>
	constexpr bool operator!=(U* rhs) const noexcept {
		return value_ != rhs;
	}

	template<typename U>
	constexpr explicit operator U*() const noexcept {
		return value_;
	}

	constexpr operator bool() const noexcept { return value_ != nullptr; }

private:
	T* value_;
};

template<typename T>
Ref<T> makeRef(T* ref) {
	return Ref<T>(ref);
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_REF_HPP_
