#if !defined(OMR_OM_ANYCELL_HPP_)
#define OMR_OM_ANYCELL_HPP_

#include <OMR/Om/ArrayBuffer.hpp>
#include <OMR/Om/ArrayBufferShape.hpp>
#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/MetaShape.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/ObjectMap.hpp>

namespace OMR
{
namespace Om
{
union AnyObjectMap
{
	Cell cell;
	Shape map;
	ObjectMap objectMap;
};

union AnyMap
{
	Cell cell;
	Shape map;
	ObjectMap objectMap;
	MetaShape metaShape;
	ArrayBufferShape arrayBufferShape;
};

union AnyCell
{
	Cell cell;
	Object object;
	Shape map;
	ObjectMap objectMap;
	MetaShape metaShape;
	ArrayBufferShape;
	ArrayBuffer;
};

namespace To
{
template <typename T>
Cell&
cell(T& x)
{
	return x.base().cell;
}

template <typename T>
Cell*
cell<T*>(T* p)
{
	return &cell(*T);
}

template <typename T>

}  // namespace To

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ANYCELL_HPP_
