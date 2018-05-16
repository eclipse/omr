#ifndef OMR_OM_SHAPE_HPP_
#define OMR_OM_SHAPE_HPP_

#include <OMR/Om/CellHeader.hpp>
#include <OMR/Om/CellKind.hpp>
#include <OMR/Om/TransitionSet.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Infra/Span.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Om/SlotAttr.hpp>
#include <OMR/Om/TransitionSet.hpp>
#include <OMR/Om/SlotDescriptorRange.hpp>
#include <OMR/Om/BasicSlotHandle.hpp>
#include <OMR/Om/ValueSlotHandle.hpp>

namespace OMR
{
namespace Om
{

/// Common properties shared by all Shapes in a Shape tree. Particularly, invariant facts about the instance are stored here.ß
struct ShapeTreeData
{
	/// Total amount of space available for inline / fixed slots.
	std::size_t instanceSlotSize;
	/// The CellKind this ShapeTree describes. Are the instances Shapes, Objects, or Arrays?
	CellKind instanceKind;
};

/// A description of a cell's layout. The Shape is like a Cell's class. Shapes may be
/// shared by Cells. The instanceKind can be examined to tell what kind of thing the
/// cell is.
///
/// Since Shapes are Cell's, they too have a Shape describing their layout.
class Shape
{
public:
	/// Calculate the allocation size
	static constexpr std::size_t calculateAllocSize(std::size_t slotCount)
	{
		return sizeof(Shape) + sizeof(SlotAttr) * slotCount;
	}

	/// Convert this object to a cell.
	CellHeader &header() { return header_; }

	const CellHeader &header() const { return header_; }

	/// Return the shape that describes _this_ Shape.
	Shape *layout() const { return header().layout(); }

	/// Set the layout of _this_ Shape.
	Shape &layout(Shape *shape)
	{
		header().layout(shape);
		return *this;
	}

	/// @group Instance information
	/// @{

	CellKind instanceKind() const
	{
		return shapeTreeData_->instanceKind;
	}

	/// Get the Span of `SlotAttr` described by this shape.
	Infra::Span<SlotAttr> instanceSlotAttrs() noexcept { return {instanceSlotAttrs_, instanceSlotCount_}; }

	Infra::Span<const SlotAttr> instanceSlotAttrs() const noexcept { return {instanceSlotAttrs_, instanceSlotCount_}; }

	/// Get the SlotDescriptors described by this shape.
	SlotDescriptorRange instanceSlotDescriptorRange() const noexcept
	{
		return SlotDescriptorRange(instanceSlotAttrs(), instanceSlotOffset(), instanceSlotWidth());
	}

	/// Number of slots described by this shape.
	std::size_t instanceSlotCount() const noexcept { return instanceSlotCount_; }

	/// The offset into the object, where the shape's slots actually begin.
	/// This is the total size of the slots represented by the parent hierachy of
	/// this shape.
	std::size_t instanceSlotOffset() const noexcept { return instanceSlotOffset_; }

	/// Total footprint of slots in an object.
	std::size_t instanceSlotWidth() const noexcept { return instanceSlotWidth_; }

	/// ObjectMaps form a heirachy of "inherited layouts."
	/// The parent object shape describes the slots immediately preceding
	/// If this `Shape` has no parent, this function returns `nullptr`.
	/// Having no parent implies the `slotOffset` is zero.
	Shape *parentLayout() const noexcept { return parentLayout_; }

	/// Common properties shared by all shapes in the shape-tree.
	ShapeTreeData *shapeTreeData() const noexcept { return shapeTreeData_; }

	/// @}

	/// @group GC support
	/// @{

	template <typename VisitorT>
	inline void
	visit(VisitorT &visitor)
	{
		visitHeader(header(), visitor);
		visitor.edge(this, BasicSlotHandle(&parentLayout_));
		transitions_.visit(visitor);
		visitor.edge(this, BasicSlotHandle(&shapeTreeData_));
	}

	/// Size of this GC cell.
	std::size_t allocSize() const { return calculateAllocSize(instanceSlotCount()); }

	/// @}

  protected:
	CellHeader header_;
	Shape *parentLayout_;
	TransitionSet transitions_;
	std::size_t instanceSlotOffset_;
	std::size_t instanceSlotWidth_;
	std::size_t instanceSlotCount_;
	ShapeTreeData *shapeTreeData_;
	SlotAttr instanceSlotAttrs_[0];
};

static_assert(std::is_standard_layout<Shape>::value, "Shape must be a StandardLayoutType.");

/// An offset table for the jit compilers.
struct ShapeOffsets
{
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_SHAPE_HPP_
