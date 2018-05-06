#if !defined(OMR_INFRA_POINTERUTILITIES_HPP_)
#define OMR_INFRA_POINTERUTILITIES_HPP_

#include <cstddef>
#include <cstdint>

namespace OMR
{
namespace Infra
{
template <typename T, typename U>
std::uintptr_t
ptrdiff(T* base, U* p)
{
	return reinterpret_cast<const char*>(p) - reinterpret_cast<const char*>(base);
}

template <typename T, typename U = T>
U*
ptradd(T* base, std::uintptr_t offset)
{
	return (U*)(((char*)base) + offset);
}

}  // namespace Infra
}  // namespace OMR

#endif  // OMR_INFRA_POINTERUTILITIES_HPP_
