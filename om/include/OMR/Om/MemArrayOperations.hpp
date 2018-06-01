#if !defined(OMR_OM_MEMARRAYOPERATIONS_HPP_)
#define OMR_OM_MEMARRAYOPERATIONS_HPP_

#include <OMR/Om/ArrayOperations.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/MemArray.hpp>
#include <OMR/Om/MemHandle.hpp>

namespace OMR {
namespace Om {

struct MemArrayOps {

	template<typename T>
	static inline bool
	construct(Context& cx, const MemHandle<MemArray<T>> self, std::size_t size) {
		if (self->initialized()) {
			return true;
		}

		self->buffer_ = allocateArray(cx, size * sizeof(T));
		return self->buffer_ != nullptr;
	}

	template<typename T>
	static inline bool
	resize(Context& cx, const MemHandle<MemArray<T>> self, std::size_t size) {
		if (!self.initialized()) {
			return construct(cx, self, size);
		}

		auto newBuffer = allocateArray(cx, size * sizeof(T));

		if (newBuffer == nullptr) {
			return false;
		}

		auto oldBuffer = self->buffer_;
		auto copySize = min(newBuffer->dataSize(), oldBuffer->dataSize());
		memcpy(oldBuffer->data(), newBuffer->data(), copySize);

		self->buffer_ = newBuffer;
		return true;
	}
};

template<typename T>
bool initializeMemArray(Context& cx, MemHandle<MemArray<T>> self, std::size_t size) {
	return MemArrayOps::construct<T>(cx, self, size);
}

} // namespace Om
} // namespace OMR

#endif // OMR_OM_MEMARRAYOPERATIONS_HPP_
