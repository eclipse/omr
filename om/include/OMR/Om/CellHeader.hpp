#if !defined(OMR_OM_CELLHEADER_HPP_)
#define OMR_OM_CELLHEADER_HPP_

#include <cassert> // for broken atomicWriteReference
#include <cstdint>
#include <cstdlib>
#include <type_traits>

namespace OMR {
namespace Om {

class Cell;
class Shape;

/// Encodes a pointer to the shape that lays out this cell, and GC flags.
class CellHeader {
public:
	static constexpr std::uintptr_t FLAGS_MASK = 0xFF;
	static constexpr std::size_t LAYOUT_REF_SHIFT = 8;

	CellHeader() = default;

	explicit CellHeader(Shape* shape, std::uint8_t flags = 0)
	        : data_((std::uintptr_t(shape) << LAYOUT_REF_SHIFT) | (flags & FLAGS_MASK)) {}

	/// Set the shape and the flags.
	CellHeader& set(Shape* shape, std::uint8_t flags) noexcept {
		data_ = (std::uintptr_t(shape) << LAYOUT_REF_SHIFT) | (flags & FLAGS_MASK);
		return *this;
	}

	/// Get the shape that describes this cell's layout.
	Shape* layout() const noexcept {
		return reinterpret_cast<Shape*>(data_ >> LAYOUT_REF_SHIFT);
	}

	/// Set the shape reference. No write barrier.
	CellHeader& layout(Shape* shape) noexcept { return set(shape, flags()); }

	/// The flags are a byte of reserved data for the OMR GC. Undefined value.
	std::uint8_t flags() const noexcept { return std::uint8_t(data_ & FLAGS_MASK); }

	CellHeader& flags(std::uint8_t flags) noexcept { return set(layout(), flags); }

protected:
	friend struct CellHeaderOffsets;

	std::uintptr_t data_;
};

static_assert(std::is_standard_layout<CellHeader>::value,
              "CellHeader must be a standard layout type");

struct CellHeaderOffsets {
	static constexpr std::size_t DATA = offsetof(CellHeader, data_);
};

/// A Handle to the layout ref in a Cell's header. This is a special kind of SlotHandle used to
/// read/write a cell header's layout reference from the header. Used by the GC to fixup the layout
/// pointer.
class CellHeaderLayoutHandle {
public:
	constexpr explicit CellHeaderLayoutHandle(CellHeader* header) noexcept : header_(header) {}

	void writeReference(Cell* ref) const noexcept { header_->layout((Shape*)ref); }

	/// TODO: Implement atomics
	void atomicWriteReference(Cell* ref) const noexcept { assert(0); }

	Cell* readReference() const noexcept { return reinterpret_cast<Cell*>(header_->layout()); }

private:
	CellHeader* header_;
};

template<typename VisitorT>
void visitHeader(CellHeader& header, VisitorT& visitor) {
	visitor.edge(&header, CellHeaderLayoutHandle(&header));
}

} // namespace Om
} // namespace OMR

#endif // OMR_OM_CELLHEADER_HPP_
