#if !defined(OMR_OM_OBJECTMAP_HPP_)
#define OMR_OM_OBJECTMAP_HPP_

#include <OMR/Infra/Span.hpp>
#include <OMR/Om/ArrayBuffer.hpp>
#include <OMR/Om/Initializer.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/SlotAttr.hpp>
#include <OMR/Om/TransitionSet.hpp>

namespace OMR
{
namespace Om
{
class Context;

using Index = std::size_t;

/// An index into an object. A SlotIndex can be used to find the address of a
/// slot inside an object. Turning an index into a useful value also requires
/// that you know the type of the slot.
/// TODO: Make SlotIndex only constructible by ObjectMaps and their iterators.
class SlotIndex
{
public:
	SlotIndex() = default;

	SlotIndex(const SlotIndex&) = default;

	SlotIndex(std::size_t offset) : offset_(offset) {}

	bool operator==(const SlotIndex& rhs) const noexcept { return offset_ == rhs.offset_; }

	bool operator!=(const SlotIndex& rhs) const noexcept { return offset_ != rhs.offset_; }

protected:
	friend struct ObjectMap;
	friend struct Object;

	std::size_t offset() const noexcept { return offset_; }

private:
	std::size_t offset_;
};

/// Describes a slot's index and attributes. The SlotAtt is unowned data,
/// typically stored in an ObjectMap, on the heap, so it's not safe to hold a
/// slot index across a GC safepoint.
class SlotDescriptor : public SlotIndex
{
public:
	SlotDescriptor() = default;

	SlotDescriptor(const SlotDescriptor&) = default;

	SlotDescriptor(SlotIndex index, const SlotAttr* attr) : SlotIndex(index), attr_(attr) {}

	const SlotAttr& attr() const noexcept { return *attr_; }

private:
	const SlotAttr* attr_;
};

/// Like `SlotLookup`, but the located SlotAttrs are immutable.

/// An iterator that also tracks slot offsets using the width of the
/// slot's type. Results in `SlotDescriptors`, fancy handles that describe a
/// slot's type and offset.
class SlotDescriptorIterator
{
public:
	SlotDescriptorIterator(const SlotAttr* current, std::size_t offset = 0)
		: current_(current), offset_(offset)
	{
	}

	SlotDescriptorIterator operator++(int)
	{
		SlotDescriptorIterator copy(*this);
		offset_ += current_->type().width();
		++current_;
		return copy;
	}

	SlotDescriptorIterator& operator++()
	{
		offset_ += current_->type().width();
		current_++;
		return *this;
	}

	bool operator==(const SlotDescriptorIterator& rhs) const
	{
		return (current_ == rhs.current_);
	}

	bool operator!=(const SlotDescriptorIterator& rhs) const { return !(*this == rhs); }

	SlotDescriptor operator*() const { return SlotDescriptor{SlotIndex{offset_}, current_}; }

private:
	const SlotAttr* current_;
	std::size_t offset_;
};

/// An iterable view of the slot described by an ObjectMap.
/// The Slot Attributes are unowned.
class SlotDescriptorRange
{
public:
	SlotDescriptorIterator begin() const
	{
		return SlotDescriptorIterator(attributes_.begin(), offset_);
	}

	SlotDescriptorIterator end() const
	{
		return SlotDescriptorIterator(attributes_.end(), offset_ + width_);
	}

	Infra::Span<const SlotAttr> attributes() const { return attributes_; }

protected:
	friend struct ObjectMap;

	SlotDescriptorRange(
		Infra::Span<const SlotAttr> attributes, std::size_t offset, std::size_t width)
		: attributes_(attributes), offset_(offset), width_(width)
	{
	}

private:
	const Infra::Span<const SlotAttr> attributes_;
	std::size_t offset_;
	std::size_t width_;
};

/// A map that describes the layout of an object.
struct ObjectMap
{
public:
	/// @{
	/// @group High level API

	/// Allocate an object map that describes one slot.
	static ObjectMap*
	allocate(Context& cx, Handle<ObjectMap> parent, Infra::Span<const SlotAttr> attributes);

	/// Allocate an object map that describes no slots
	static ObjectMap* allocate(Context& cx);

	/// Part two of initialization. @see-also ObjectMapInitializer
	static bool construct(Context& cx, Handle<ObjectMap> self);

	/// Create a slot map that derives base. Add the new slot map to the set of
	/// known transistions from base.
	static ObjectMap*
	derive(Context& cx, Handle<ObjectMap> base, const Infra::Span<const SlotAttr>& attr,
	       std::size_t hash);

	/// Look up a transition to a derived shape.
	ObjectMap*
	lookUpTransition(Context& cx, const Infra::Span<const SlotAttr>& attr, std::size_t hash);

	/// @}

	/// @{
	/// @group Base Accessors

	Shape& baseShape() noexcept { return baseMap_; }

	const Shape& baseShape() const noexcept { return baseMap_; }

	Cell& baseCell() noexcept { return baseShape().baseCell(); }

	const Cell& baseCell() const noexcept { return baseShape().baseCell(); }

	/// @}

	/// @group Slot Queries
	/// @{

	/// Get the Span of `SlotAttr` stored in the ObjectMap.
	Infra::Span<SlotAttr> slotAttrs() noexcept { return {attributes_, slotCount_}; }

	Infra::Span<const SlotAttr> slotAttrs() const noexcept { return {attributes_, slotCount_}; }

	/// Get the SlotDescriptors described by this map.
	SlotDescriptorRange slotDescriptors() const noexcept
	{
		return SlotDescriptorRange(slotAttrs(), slotOffset_, slotWidth_);
	}

	/// Number of slots described by this map.
	std::size_t slotCount() const noexcept { return slotCount_; }

	/// The offset into the object, where the map's slots actually begin.
	/// This is the total size of the slots represented by the parent hierachy of
	/// this map.
	std::size_t slotOffset() const noexcept { return slotOffset_; }

	/// Total footprint of slots in an object.
	std::size_t slotWidth() const noexcept { return slotWidth_; }

	/// ObjectMaps form a heirachy of "inherited layouts."
	/// The parent object map describes the slots immediately preceding
	/// If this `ObjectMap` has no parent, this function returns `nullptr`.
	/// Having no parent implies the `slotOffset` is zero.
	inline ObjectMap* parent() const noexcept { return parent_; }

	/// @group GC Support
	/// @{

	template <typename VisitorT>
	inline void
	visit(VisitorT& visitor)
	{
		baseShape().visit(visitor);
		visitor.edge((Cell*)this, (Cell*)parent());
		transitions_.visit(visitor);
	}

	std::size_t allocSize() const { return calculateAllocSize(slotCount()); }

	/// @}

protected:
	friend struct ObjectMapInitializer;
	friend struct ObjectMapOffsets;

	/// Calculate the allocation size
	static constexpr std::size_t calculateAllocSize(std::size_t slotCount)
	{
		return sizeof(ObjectMap) + sizeof(SlotAttr) * slotCount;
	}

	/// Initialize an object map that describes a slot.
	ObjectMap(MetaShape* meta, ObjectMap* parent, const Infra::Span<const SlotAttr>& slots);

	Shape baseMap_;
	ObjectMap* parent_;
	TransitionSet transitions_;

	/// @group Instance Properties
	/// @{
	std::size_t slotOffset_;
	std::size_t slotWidth_;
	std::size_t slotCount_;
	/// @}

	/// Off the end of the SlotMap is storage for it's slot attributes.
	/// We overallocate the slot map structure to provide storage for the array.
	/// `calculateAllocSize` returns
	SlotAttr attributes_[0];
};

static_assert(std::is_standard_layout<ObjectMap>::value, "ObjectMap must be a StandardLayoutType");

/// An offset table for the jit compilers.
struct ObjectMapOffsets
{
	static constexpr std::size_t baseShape = offsetof(ObjectMap, baseMap_);
	static constexpr std::size_t parent  = offsetof(ObjectMap, parent_);
	/// TODO: Flush out the offsets table
};

/// A functor that performs basic initialization of an ObjectMap.
struct ObjectMapInitializer : public Initializer
{
	virtual Cell* operator()(Context& cx, Cell* cell) noexcept override;

	Handle<ObjectMap> parent;
	Infra::Span<const SlotAttr> attributes;
};

/// @group Conversion Functions
/// Conversions between an ObjectMap and it's base structures.
/// @{

inline Shape*
asMap(ObjectMap* objectMap)
{
	return &objectMap->baseShape();
}

inline const Shape*
asMap(const ObjectMap* objectMap)
{
	return &objectMap->baseShape();
}

inline Cell*
asCell(ObjectMap* objectMap)
{
	return &objectMap->baseCell();
}

inline const Cell*
asCell(const ObjectMap* objectMap)
{
	return &objectMap->baseCell();
}

inline ObjectMap*
asObjectMap(Cell* cell)
{
	return reinterpret_cast<ObjectMap*>(cell);
}

inline const ObjectMap*
asObjectMap(const Cell* cell)
{
	return reinterpret_cast<const ObjectMap*>(cell);
}

inline ObjectMap*
asObjectMap(Shape* map)
{
	return reinterpret_cast<ObjectMap*>(map);
}

inline const ObjectMap*
asObjectMap(const Shape* map)
{
	return reinterpret_cast<const ObjectMap*>(map);
}

/// @}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_OBJECTMAP_HPP_
