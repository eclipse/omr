#if !defined(OMR_OM_MEMREF_HPP_)
#define OMR_OM_MEMREF_HPP_

#include <OMR/Om/Ref.hpp>

#include <OMR/Infra/PointerUtilities.hpp>

#include <cstddef>

namespace OMR
{
namespace Om
{
class Cell;

/// An unsafe reference to a member.
/// Contrasted
template <typename T, typename C = Cell>
struct MemRef
{
public:
private:
	Ref<C> base;
	std::size_t offset;
};

template <typename T, typename M>
MemberRef
makeMembRef(T* cell, M* member)
{
	return MemberRef(cell, ptrdiff(cell, member));
}

template <typename T>
MemberRef
makeMemberRef(T* cell, std::size_t offset)
{
	return MemberRef(cell, offset);
}

template <typename T, typename M>
MemberRef<M>
makeMemberRef(T* cell, M T::*member)
{
	return MemberRef(cell, ptrdiff(cell, &cell->*member));
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MEMBERREF_HPP_
