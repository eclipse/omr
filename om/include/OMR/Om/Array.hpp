#if !defined(OMR_OM_ARRAY_HPP_)
#define OMR_OM_ARRAY_HPP_

#include <OMR/Om/CellHeader.hpp>
#include <OMR/Om/Handle.hpp>

#include <OMR/Infra/HashUtilities.hpp>
#include <cassert>
#include <type_traits>

namespace OMR {
namespace Om {

/// A managed cell of memory providing general-purpose raw storage.
/// Note that the Array is considered a leaf-object, and references stored
/// into the buffer must be explicitly marked. Arrays are of a fixed, limited size.
/// For a more powerful array-type, see Vector and ArrayRef. Arrays are primarily low level storage
/// for other higher level abstractions.
class Array {
public:
	CellHeader& header() noexcept { return header_; }

	const CellHeader& header() const noexcept { return header_; }

	/// The array has a generic method for calculating a hash of the data.
	/// In general, it is preferable that the user provides a less-general hashing
	/// function.
	std::size_t hash() const noexcept {
		std::size_t hash = size_ + 7;
		for (std::size_t i = 0; i < size_; i++) {
			hash = Infra::Hash::mix(hash, data_[i]);
		}
		return hash;
	}

	/// The full size of this heap cell, in bytes.
	std::size_t allocSize() const noexcept { return calcAllocSize(size_); }

	/// The raw data pointer
	void* data() noexcept { return data_; }

	const void* data() const noexcept { return data_; }

	/// The size of the data buffer, in bytes.
	std::size_t dataSize() const noexcept { return size_; }

	/// @group GC Support
	template<typename VisitorT>
	void visit(VisitorT& visitor) {
		visitHeader(header_, visitor);
	}

protected:
	friend class ArrayInitializer;
	friend class ArrayOffsets;

	/// Get the allocation size of an array buffer with storage for `dataSize`
	/// bytes.
	static std::size_t calcAllocSize(std::size_t dataSize) { return dataSize + sizeof(Array); }

	inline Array() = default;

	inline Array(Shape* shape, std::size_t size) : header_(shape), size_(size) {}

	CellHeader header_;
	std::size_t size_; //< size in bytes of the data array.
	std::uintptr_t data_[0];
};

static_assert(std::is_standard_layout<Array>::value, "Array must be a StandardLayoutType");

class ArrayOffsets {
public:
	ArrayOffsets() = delete;
	static constexpr std::size_t size = offsetof(Array, size_);
	static constexpr std::size_t data = offsetof(Array, data_);
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_ARRAY_HPP_
