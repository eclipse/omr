#ifndef OMR_OM_CELL_HPP_
#define OMR_OM_CELL_HPP_

#include <OMR/Om/Id.hpp>

#include <cstdint>

namespace OMR
{
namespace Om
{
struct Cell;
struct Shape;

class Context;

class MemoryManager;

using CellHeader = std::uintptr_t;

/// A managed blob of memory. All Cells have a one slot header.
struct Cell
{
	static constexpr CellHeader FLAGS_MASK = 0xFF;
	static constexpr std::size_t MAP_SHIFT = 8;

	static void construct(Context& cx, Cell* self, Shape* map, std::uint8_t flags = 0) {
		self->set(map, flags);
	}

	inline Cell(Shape* map, std::uint8_t flags = 0) { set(map, flags); }

	/// Get the map reference.
	Shape* map() const { return (Shape*)(header >> MAP_SHIFT); }

	/// Set the map reference. No write barrier.
	void map(Shape* map) { header = (CellHeader(map) << MAP_SHIFT) | (header & FLAGS_MASK); }

	std::uint8_t flags() const { return std::uint8_t(header & FLAGS_MASK); }

	/// Set the map and the flags.
	void set(Shape* m, std::uint8_t flags)
	{
		header = (CellHeader(m) << MAP_SHIFT) | (flags & FLAGS_MASK);
	}

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		visitor.edge(this, (Cell*)map());
	}

	CellHeader header;
};

static_assert(std::is_standard_layout<Cell>::value, "Cell must be a StandardLayoutType");

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_CELL_HPP_
