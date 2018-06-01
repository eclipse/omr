#if !defined(OMR_OM_MEMTRANSITIONSET_HPP_)
#define OMR_OM_MEMTRANSITIONSET_HPP_

#include <OMR/Infra/Span.hpp>
#include <OMR/Om/BasicSlotHandle.hpp>
#include <OMR/Om/Id.hpp>
#include <OMR/Om/MemArray.hpp>
#include <OMR/Om/MemHandle.hpp>
#include <OMR/Om/SlotAttr.hpp>
#include <cstddef>
#include <type_traits>

namespace OMR {
namespace Om {

class Shape;
class Context;

struct TransitionSetEntry {
	Shape* shape;
};

/// The TransitionSet is a collection of Maps for the purpose of tracking known
/// object transitions. As objects grow slots, a chain of maps is built up. The
/// transition table tells us the existing derivations of a given shape. When an
/// object grows a slot, we look to see if this layout transition has been done
/// before, so we can reuse the shape. The Transition set is embedded in other
/// native objects.
class TransitionSet {
public:
	TransitionSet() = default;

	std::size_t size() const { return table.size(); }

	Shape* lookup(Infra::Span<const SlotAttr> desc, std::size_t hash) const;

	// try to store object in the table. if the table is full, fail.
	bool tryStore(Shape* shape, std::size_t hash);

	bool initialized() const noexcept { return table.initialized(); }

	template<typename VisitorT>
	void visit(VisitorT& visitor) {
		if (!initialized()) {
			return;
		}

		// note that this visit will not walk the contents, just the buffer itself.
		table.visit(visitor);

		for (std::size_t i = 0; i < size(); i++) {
			TransitionSetEntry& e = table[i];
			if (e.shape != nullptr) {
				visitor.edge(this, BasicSlotHandle(&e.shape));
			}
		}
	}

	MemArray<TransitionSetEntry> table;
};

static_assert(std::is_standard_layout<TransitionSet>::value,
              "TransitionSet must be a StandardLayoutType.");

static_assert(std::is_standard_layout<TransitionSetEntry>::value,
              "TransitionSet must be a StandardLayoutType.");

} // namespace Om
} // namespace OMR

#endif // OMR_OM_MEMTRANSITIONSET_HPP_
