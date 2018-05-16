#ifndef OMR_OM_BOX_HPP_
#define OMR_OM_BOX_HPP_

#include <OMR/Om/Cell.hpp>

namespace OMR
{
namespace Om
{
/// A box around a native data type, T. BoxCells provide a way to allocate
/// native data types on the managed heap. The box wraps the object in minimal
/// GC facilities. Note that, as a DataCell, there is no way to safetly store
/// GC references inside a BoxCell.
template <typename T>
class Box : public Array
{
public:
	template <typename... Args>
	BoxCell(std::in_place_t, Args...&& args) : DataCell(sizeof(T))
	{
		new (data()) T(std::forward<Args>(args)...);
	}

	T& unbox() { return *(T*)data(); }

	const T& unbox() const { return *(T*)data(); }
};

template <typename T>
class Box
{
};

class BoxRef
{
};

class BoxRootRef
{
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_BOX_HPP_
