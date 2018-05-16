#if !defined(OMR_OM_MEMARRAYOPERATIONS_HPP_)
#define OMR_OM_MEMARRAYOPERATIONS_HPP_

#include <OMR/Om/MemArray.hpp>
#include <OMr/Om/ArrayOperations.hpp>
#include <OMR/Om/MemHandle.hpp>

namespace OMR {
namespace Om {

template <typename T>
inline bool constructMemArray(Context& cx, const MemHandle<MemArray<T>> self, std::size_t size)
{
	self->buffer_ = allocateArray(cx, size * sizeof(T));
	return self->buffer_ != nullptr;
}

template <typename T>
inline bool resizeMemArray(Context& cx, const MemHandle<MemArray<T>> self, std::size_t size)
{
	auto newBuffer = allocateArray(cx, size * sizeof(T));

	if (newBuffer == nullptr) {
		return false;
	}

	auto oldBuffer = self->buffer_;
	auto copySize  = min(newBuffer->size(), oldBuffer->size());
	memcpy(oldBuffer->data(), newBuffer->data(), copySize);

	self->buffer_ = newBuffer;
	return true;
}

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_MEMARRAYOPERATIONS_HPP_
