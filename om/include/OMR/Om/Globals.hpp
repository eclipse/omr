#if !defined(OMR_OM_GLOBALS_HPP_)
#define OMR_OM_GLOBALS_HPP_

#include <OMR/Om/Shape.hpp>
#include <OMR/Om/StartupError.hpp>

namespace OMR {
namespace Om {

class StartupContext;
class Cell;

/// A collection of singleton GC cells. The collection is initialized at startup.
class Globals
{
public:
	Shape* metaShape() const noexcept { return metaShape_; }

	Shape* arrayBufferShape() const noexcept { return arrayBufferShape_; }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		if (metaShape_ != nullptr)
			visitor.edge(this, BasicSlotHandle((Cell**)&metaShape_));
	
		if (arrayBufferShape_ != nullptr)
			visitor.edge(this, BasicSlotHandle((Cell**)&arrayBufferShape_));
	}

protected:
	friend class MemorySystem;

	/// Allocate the globals. Throws StartupError if any allocation fails.
	void init(StartupContext& cx);

private:
	Shape* metaShape_        = nullptr;
	Shape* arrayBufferShape_ = nullptr;
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_GLOBALS_HPP_
