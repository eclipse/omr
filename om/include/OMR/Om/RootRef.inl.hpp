#if !defined(OMR_OM_ROOTREF_INL_HPP_)
#define OMR_OM_ROOTREF_INL_HPP_

#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/RootRef.hpp>

namespace OMR
{
namespace Om
{
template <typename T>
inline RootRef<T>::RootRef(Context& cx, T* ptr) noexcept : RootRef(cx.stackRoots(), ptr)
{
}

template <typename T>
inline RootRef<T>::RootRef(RootRefSeq& seq, T* ptr) noexcept
	: seq_(seq), node_(reinterpret_cast<Cell*>(ptr), seq.head())
{
	seq_.head(&node_);
}

template <typename T>
template <typename U>
inline RootRef<T>::RootRef(const RootRef<U>& other) noexcept : RootRef(other.seq(), get<T*>(other))
{
}

template <typename T>
template <typename U>
inline RootRef<T>::RootRef(RootRef<U>&& other) noexcept
	: seq_(other.seq()), node_(get<T*>(other), other.tail())
{
	assert(other.isHead());
	other.node_.first  = nullptr;
	other.node_.second = nullptr;
	seq_.head(&node_);
}

template <typename T>
inline RootRef<T>::~RootRef() noexcept
{
	std::cerr << "RootRef<T>::~RootRef(this=" << this << ")" << std::endl;
	assert(isHead());
	seq_.head(node_.second);
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ROOTREF_INL_HPP_
