#if !defined(OMR_OBJECTMAP_INL_HPP_)
#define OMR_OBJECTMAP_INL_HPP_

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Shape.inl.hpp>
#include <OMR/Om/ObjectMap.hpp>
#include <OMR/Om/TransitionSet.inl.hpp>

namespace OMR
{
namespace Om
{
inline ObjectMap::ObjectMap(
	MetaShape* meta, ObjectMap* parent, const Infra::Span<const SlotAttr>& attributes)
	: baseMap_(meta, Shape::Kind::OBJECT_MAP)
	, parent_(parent)
	, transitions_()
	,
	// TODO: Find a clearer way to construct a map without a parent.
	slotOffset_(parent ? (parent->slotOffset() + parent->slotWidth()) : 0)
	, slotWidth_(0)
	, slotCount_(attributes.length())
{
	for (std::size_t i = 0; i < slotCount_; i++) {
		attributes_[i] = attributes[i];
		slotWidth_ += attributes[i].type().width();
	}
}

inline Cell*
ObjectMapInitializer::operator()(Context& cx, Cell* cell) noexcept
{
	auto meta = cx.globals().metaShape();
	new (cell) ObjectMap(meta, parent, attributes);
	return cell;
}

inline ObjectMap*
ObjectMap::allocate(Context& cx, Handle<ObjectMap> parent, Infra::Span<const SlotAttr> attributes)
{
	ObjectMapInitializer init;
	init.parent     = parent;
	init.attributes = attributes;

	std::size_t size  = calculateAllocSize(attributes.length());
	ObjectMap* result = BaseAllocator::allocate<ObjectMap>(cx, init, size);

	RootRef<ObjectMap> root(cx, result);
	bool ok = construct(cx, root);
	if (!ok) {
		return result = nullptr;
	} else {
		result = root.get();
	}

	return result;
}

inline ObjectMap*
ObjectMap::allocate(Context& cx)
{
	// TODO: Don't construct a bogus handle here, find a clearer way to do this.
	ObjectMap* parent = nullptr;
	return allocate(cx, Handle<ObjectMap>(parent), Infra::Span<const SlotAttr>(nullptr, 0));
}

inline bool
ObjectMap::construct(Context& cx, Handle<ObjectMap> self)
{
	return TransitionSet::construct(cx, {self, &ObjectMap::transitions_});
}

inline ObjectMap*
ObjectMap::lookUpTransition(
	Context& cx, const Infra::Span<const SlotAttr>& attributes, std::size_t hash)
{
	return transitions_.lookup(attributes, hash);
}

inline ObjectMap*
ObjectMap::derive(
	Context& cx, Handle<ObjectMap> base, const Infra::Span<const SlotAttr>& attributes,
	std::size_t hash)
{
	ObjectMap* derivation = ObjectMap::allocate(cx, base, attributes);
	base->transitions_.tryStore(derivation, hash);
	// TODO: Write barrier? the object map
	return derivation;
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OBJECTMAP_INL_HPP_
