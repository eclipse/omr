#if !defined(OMR_OM_MEMARRAY_HPP_)
#define OMR_OM_MEMARRAY_HPP_

#include <OMR/Om/Array.hpp>
#include <OMR/Om/MemHandle.hpp>
#include <OMR/Om/BasicSlotHandle.hpp>

#include <cstdint>

namespace OMR
{
namespace Om
{
/// A simple array API backed by a heap-allocated Array.
/// The array understands it's size, but has no notion of capacity, or unfilled
/// slots. MemArrays can be resized.
/// @see-also: MemArrayOps
template <typename T>
class MemArray
{
public:
	static_assert(std::is_trivial<T>::value, "The MemArray only supports simple type storage.");

	constexpr MemArray() : buffer_(nullptr) {}

	constexpr bool initialized() const noexcept { return buffer_ != nullptr; }

	constexpr bool empty() const noexcept { return buffer_ == nullptr; }

	T* data() noexcept { return reinterpret_cast<T*>(buffer_->data()); }

	const T* data() const noexcept { return reinterpret_cast<T*>(buffer_->data()); }

	/// Number of elements in this array.
	std::size_t size() const noexcept { return buffer_->dataSize() / sizeof(T); }

	T& at(std::size_t index) noexcept { return data()[index]; }

	const T& at(std::size_t index) const noexcept { return data()[index]; }

	T& operator[](std::size_t index) noexcept { return at(index); }

	const T& operator[](std::size_t index) const noexcept { return at(index); }

	template <typename VisitorT>
	void visit(VisitorT visitor)
	{
		if (buffer_ != nullptr)
			visitor.edge(this, BasicSlotHandle(&buffer_));
	}

	Array* buffer_;
};

static_assert(
	std::is_standard_layout<MemArray<int>>::value, "MemArray must be a StandardLayoutType.");

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MEMARRAY_HPP_
