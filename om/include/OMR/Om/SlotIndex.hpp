#if !defined(OMR_OM_SLOTINDEX_HPP_)
#define OMR_OM_SLOTINDEX_HPP_

#include <cstdlib>

namespace OMR {
namespace Om {

using Index = std::size_t;

/// An index into an object. A SlotIndex can be used to find the address of a
/// slot inside an object. Turning an index into a useful value also requires
/// that you know the type of the slot.
///
/// TODO: Make SlotIndex only constructible by ObjectMaps and their iterators.
class SlotIndex
{
public:
	SlotIndex() noexcept = default;

	constexpr SlotIndex(const SlotIndex&) noexcept = default;

	constexpr SlotIndex(std::size_t offset) noexcept : offset_(offset) {}

	SlotIndex& operator=(const SlotIndex& rhs) {
		offset_ = rhs.offset_;
		return *this;
	}

	constexpr bool operator==(const SlotIndex& rhs) const noexcept { return offset_ == rhs.offset_; }

	constexpr bool operator!=(const SlotIndex& rhs) const noexcept { return offset_ != rhs.offset_; }

	/// Convert the SlotIndex into a raw offset into an object.
	std::size_t offset() const noexcept { return offset_; }

private:
	std::size_t offset_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_SLOTINDEX_HPP_
