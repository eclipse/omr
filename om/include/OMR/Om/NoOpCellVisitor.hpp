#if !defined(OMR_OM_NOOPCELLVISITOR_HPP_)
#define OMR_OM_NOOPCELLVISITOR_HPP_

namespace OMR {
namespace Om {

class CellVisitor {};

class NoOpCellVisitor : public CellVisitor {
public:
	template <typename SlotHandleT>
	void edge(void* cell, SlotHandleT slot) {}
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_NOOPCELLVISITOR_HPP_
