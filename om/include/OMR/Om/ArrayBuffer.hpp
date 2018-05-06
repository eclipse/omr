#if !defined(OMR_OM_ARRAYBUFFER_HPP_)
#define OMR_OM_ARRAYBUFFER_HPP_

#include <OMR/Infra/HashUtilities.hpp>
#include <OMR/Om/ArrayBufferShape.hpp>
#include <OMR/Om/Handle.hpp>

#include <cassert>
#include <type_traits>

namespace OMR
{
namespace Om
{
/// A managed cell of memory providing general-purpose raw storage.
/// Note that the ArrayBuffer is considered a leaf-object, references stored
/// into the buffer must be explicitly marked.
class ArrayBuffer
{
public:
	using Size = std::size_t;

	/// @group Allocation
	/// @{

	/// Allocate a new ArrayBuffer<T>. GC Safepoint. size is the number of
	/// elements, not the size in bytes of the buffer.
	static ArrayBuffer* allocate(Context& cx, Size size);

	/// @}

	Cell& baseCell() noexcept { return baseCell_; }

	/// The array buffer has a generic method for calculating a hash of the data.
	/// In general, it is preferable that the user provides a less-general hashing
	/// function.
	std::size_t hash() const noexcept
	{
		std::size_t hash = size_ + 7;
		for (std::size_t i = 0; i < size_; i++) {
			hash = Infra::Hash::mix(hash, data_[i]);
		}
		return hash;
	}

	/// The full size of this heap object, in bytes.
	std::size_t allocSize() const noexcept { return calcAllocSize(size_); }

	/// The raw data pointer
	void* data() noexcept { return data_; }

	const void* data() const noexcept { return data_; }

	/// The size of the data buffer, in bytes.
	Size size() const noexcept { return size_; }

	/// @group GC Support
	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		baseCell_.visit(visitor);
	}

protected:
	friend class ArrayBufferInitializer;
	friend class ArrayBufferOffsets;

	ArrayBuffer(ArrayBufferShape* map, std::size_t size);

	Cell baseCell_;
	Size size_;  //< size in bytes of the data array.
	std::uintptr_t data_[0];

private:
	/// Get the allocation size of an array buffer with storage for `dataSize`
	/// bytes.
	static std::size_t calcAllocSize(std::size_t dataSize)
	{
		return dataSize + sizeof(ArrayBuffer);
	}
};

static_assert(
	std::is_standard_layout<ArrayBuffer>::value, "ArrayBuffer must be a StandardLayoutType");

class ArrayBufferOffsets
{
public:
	ArrayBufferOffsets()              = delete;
	static constexpr std::size_t size = offsetof(ArrayBuffer, size_);
	static constexpr std::size_t data = offsetof(ArrayBuffer, data_);
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAYBUFFER_HPP_
