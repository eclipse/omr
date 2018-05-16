#if !defined(OMR_OM_SLOTDESCRIPTOR_HPP_)
#define OMR_OM_SLOTDESCRIPTOR_HPP_

#include <OMR/Om/SlotIndex.hpp>
#include <OMR/Om/SlotAttr.hpp>

namespace OMR {
namespace Om {

/// Describes a slot's index and attributes. The SlotAttr is unowned data,
/// typically stored in a managed Shape, so it's not normally safe to hold a
/// SlotDescriptor across a GC safepoint.
class SlotDescriptor : public SlotIndex
{
public:
	SlotDescriptor() = default;

	SlotDescriptor(const SlotDescriptor&) = default;

	SlotDescriptor(SlotIndex index, const SlotAttr* attr) : SlotIndex(index), attr_(attr) {}

	const SlotAttr& attr() const noexcept { return *attr_; }

	bool operator==(const SlotDescriptor& rhs) const noexcept {
		return (offset() == rhs.offset()) && (attr() == rhs.attr());
	}

	bool operator!=(const SlotDescriptor& rhs) const noexcept {
		return (offset() != rhs.offset()) || (attr() != rhs.attr());
	}

private:
	const SlotAttr* attr_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_SLOTDESCRIPTOR_HPP_
