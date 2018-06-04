#if !defined(OMR_OM_SLOTDESCRIPTORRANGE_HPP_)
#define OMR_OM_SLOTDESCRIPTORRANGE_HPP_

#include <OMR/Om/SlotDescriptor.hpp>

#include <OMR/Infra/Span.hpp>

namespace OMR {
namespace Om {

/// An iterable view of the slot described by an Shape. Slots attributes are immutable.
/// The Slot Attributes are unowned, heap-allocated data. Do not hold a range or iterator across GC
/// safepoints.
///
/// TODO: Provide a mutable view of SlotDescriptors inside Shape objects.
class SlotDescriptorRange {
public:
	/// An iterator that also tracks slot offsets using the width of the
	/// slot's type. Results in `SlotDescriptors`, fancy handles that describe a
	/// slot's type and offset.
	class Iterator {
	public:
		Iterator(const SlotAttr* current, std::size_t offset = 0)
		        : current_(current), offset_(offset) {}

		Iterator operator++(int) {
			Iterator copy(*this);
			offset_ += current_->type().width();
			++current_;
			return copy;
		}

		Iterator& operator++() {
			offset_ += current_->type().width();
			current_++;
			return *this;
		}

		bool operator==(const Iterator& rhs) const { return (current_ == rhs.current_); }

		bool operator!=(const Iterator& rhs) const { return !(*this == rhs); }

		SlotDescriptor operator*() const {
			return SlotDescriptor{SlotIndex{offset_}, current_};
		}

	private:
		const SlotAttr* current_;
		std::size_t offset_;
	};

	Iterator begin() const { return Iterator(attributes_.begin(), offset_); }

	Iterator end() const { return Iterator(attributes_.end(), offset_ + width_); }

	Infra::Span<const SlotAttr> attributes() const { return attributes_; }

protected:
	friend class Shape;

	SlotDescriptorRange(Infra::Span<const SlotAttr> attributes,
	                    std::size_t offset,
	                    std::size_t width)
	        : attributes_(attributes), offset_(offset), width_(width) {}

private:
	const Infra::Span<const SlotAttr> attributes_;
	std::size_t offset_;
	std::size_t width_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_SLOTDESCRIPTORRANGE_HPP_
