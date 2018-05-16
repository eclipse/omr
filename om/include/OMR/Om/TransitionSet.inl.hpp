#if !defined(OMR_OM_TRANSITIONSET_INL_HPP_)
#define OMR_OM_TRANSITIONSET_INL_HPP_

#include <OMR/Om/Array.hpp>
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

inline Shape*
TransitionSet::lookup(Infra::Span<const SlotAttr> attributes, std::size_t hash) const
{
	std::size_t sz = size();
	for (std::size_t i = 0; i < sz; i++) {
		std::size_t idx = (hash + i) % sz;
		auto shape        = table_.at(idx).shape;
		if (shape == nullptr) {
			return nullptr;
		}
		if (attributes == shape->slotAttrs()) {
			return shape;
		}
	}
	return nullptr;
}

inline bool
TransitionSet::tryStore(Shape* shape, std::size_t hash)
{
	std::size_t sz = size();

	for (std::size_t i = 0; i < sz; i++) {
		std::size_t idx = (hash + i) % sz;
		if (table_[idx].shape == nullptr) {
			table_[idx] = Entry{shape};
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
		if (e.shape != nullptr) {
			visitor.edge((Cell*)this, (Cell*)e.shape);
		}
	}
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_TRANSITIONSET_INL_HPP_
