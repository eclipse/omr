#if !defined(OMR_OM_TRANSITIONSET_INL_HPP_)
#define OMR_OM_TRANSITIONSET_INL_HPP_

#include <OMR/Om/ArrayBuffer.inl.hpp>
#include <OMR/Om/TransitionSet.hpp>

namespace OMR
{
namespace Om
{
inline bool
TransitionSet::construct(Context& cx, MemHandle<TransitionSet> self)
{
	return MemArray<Entry>::construct(
		cx, MemHandle<MemArray<Entry>>(self, &TransitionSet::table_), 32);
}

inline ObjectMap*
TransitionSet::lookup(Infra::Span<const SlotAttr> attributes, std::size_t hash) const
{
	std::size_t sz = size();
	for (std::size_t i = 0; i < sz; i++) {
		std::size_t idx = (hash + i) % sz;
		auto map        = table_.at(idx).map;
		if (map == nullptr) {
			return nullptr;
		}
		if (attributes == map->slotAttrs()) {
			return map;
		}
	}
	return nullptr;
}

inline bool
TransitionSet::tryStore(ObjectMap* map, std::size_t hash)
{
	std::size_t sz = size();

	for (std::size_t i = 0; i < sz; i++) {
		std::size_t idx = (hash + i) % sz;
		if (table_[idx].map == nullptr) {
			table_[idx] = Entry{map};
			return true;
		}
	}
	return false;
}

template <typename VisitorT>
inline void
TransitionSet::visit(VisitorT& visitor)
{
	if (!table_.initialized()) {
		return;
	}

	// note that this visit will not walk the contents, just the buffer itself.
	table_.visit(visitor);

	for (std::size_t i = 0; i < size(); i++) {
		const Entry& e = table_[i];
		if (e.map != nullptr) {
			visitor.edge((Cell*)this, (Cell*)e.map);
		}
	}
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_TRANSITIONSET_INL_HPP_
