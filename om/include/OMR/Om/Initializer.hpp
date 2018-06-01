#if !defined(OMR_OM_INITIALIZER_HPP_)
#define OMR_OM_INITIALIZER_HPP_

namespace OMR {
namespace Om {
class Context;
class Cell;

/// Preliminary initialization functor. As an object is allocated, the mutator
/// may perform some concurrent GC work, "paying the GC tax". To do this work,
/// every object must be in a walkable, GC-able state. That includes our freshly
/// allocated object. An Initializer is used to perform this basic
/// initialization. This initialization happens immediately after an object's
/// memory is allocated, but before the allocation is "completed".
///
/// Note that Objects have a complex, multi-allocation layout. There are higher
/// level APIs for constructing them. This class is for low-level, per
/// allocation initialization. Initializers may not allocate.
class Initializer {
public:
	virtual Cell* operator()(Context& cx, Cell* cell) = 0;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_INITIALIZER_HPP_
