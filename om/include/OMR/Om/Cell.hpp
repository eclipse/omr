#ifndef OMR_OM_CELL_HPP_
#define OMR_OM_CELL_HPP_

#include <OMR/Om/CellHeader.hpp>

#include <cstdint>

namespace OMR {
namespace Om {

/// A managed blob of memory. All Cells have a one slot header.
class Cell {
public:
	/// Cells are not visitable. Cast the Cell to it's specific type before visiting.
	template<typename VisitorT>
	void visit(VisitorT& visitor) = delete;

	CellHeader& header() noexcept { return header_; }

	const CellHeader& header() const noexcept { return header_; }

	Shape* layout() const { return header().layout(); }

protected:
	friend struct CellOffsets;

	CellHeader header_;
};

static_assert(std::is_standard_layout<Cell>::value, "Cell must be a StandardLayoutType");

struct CellOffsets {
	static constexpr std::size_t HEADER = offsetof(Cell, header_);
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_CELL_HPP_
