#if !defined(OMR_OM_MEMVECTOR_HPP_)
#define OMR_OM_MEMVECTOR_HPP_

#include <OMR/Om/ArrayBuffer.hpp>
#include <OMR/Om/MemHandle.hpp>
#include <cstdint>

namespace OMR
{
namespace Om
{
/// A simple array API backed by a heap-allocated ArrayBuffer.
/// The array understands it's size, but has no notion of capacity, or unfilled
/// slots. MemArrays can be resized.
template <typename T>
class MemArray
{
public:
	static_assert(std::is_trivial<T>::value, "The MemArray only supports simple type storage.");

	using Size = ArrayBuffer::Size;

	static bool construct(Context& cx, const MemHandle<MemArray<T>> self, Size size)
	{
		self->buffer_ = ArrayBuffer::allocate(cx, size * sizeof(T));
		return self->buffer_ != nullptr;
	}

	static bool resize(Context& cx, const MemHandle<MemArray<T>> self, std::size_t size)
	{
		auto newBuffer = ArrayBuffer::allocate(cx, size * sizeof(T));

		if (newBuffer == nullptr) {
			return false;
		}

		auto oldBuffer = self->buffer_;
		auto copySize  = min(newBuffer->size(), oldBuffer->size());
		memcpy(oldBuffer->data(), newBuffer->data(), copySize);

		self->buffer_ = newBuffer;
		return true;
	}

	constexpr MemArray() : buffer_(nullptr) {}

	constexpr bool initialized() const noexcept { return buffer_ != nullptr; }

	constexpr bool empty() const noexcept { return buffer_ == nullptr; }

	T* data() noexcept { return reinterpret_cast<T*>(buffer_->data()); }

	const T* data() const noexcept { return reinterpret_cast<T*>(buffer_->data()); }

	/// Number of elements in this array.
	std::size_t size() const noexcept { return buffer_->size() / sizeof(T); }

	T& at(std::size_t index) noexcept { return data()[index]; }

	const T& at(std::size_t index) const noexcept { return data()[index]; }

	T& operator[](std::size_t index) noexcept { return at(index); }

	const T& operator[](std::size_t index) const noexcept { return at(index); }

	template <typename VisitorT>
	void visit(VisitorT visitor)
	{
		if (buffer_ != nullptr)
			visitor.edge(nullptr, (Cell*)buffer_);
	}

private:
	ArrayBuffer* buffer_;
};

static_assert(
	std::is_standard_layout<MemArray<int>>::value, "MemArray must be a StandardLayoutType.");

#if 0
/// A vector-API backed by a heap-allocated ArrayBuffer Cell. As a Member-type,
/// vectors are not self contained objects, but are meant to be embedded in
/// other Cell types, or rooted.
template <typename T>
struct MemVector {
  static std::size_t capacity(Context& cx, const MemVector<T>* self) {
    return self->buffer->size / sizeof(T);
  }

#if 0
  static std::size_t size(Context& cx, const MemVector<T>* self) {
    return self->size;
  }
#endif

  // TODO: Barrier::write(cx, handle.base(), &handle->buffer, newBuffer);
  static bool resize(Context& cx, const MemHandle<MemVector<T>>& self,
                     std::size_t size) {
    auto newBuffer = ArrayBuffer<T>::reallocate(cx, self->buffer, size);
    if (newBuffer != nullptr) {
      self->buffer = newBuffer;
      return true;
    }
    return false;
  }

  static T& get(Context& cx, const MemVector<T>* self, std::size_t index) {
    return ArrayBuffer<T>::get(cx, self->buffer, index);
  }

  // TODO: Barrier::store(cx, self, &Member::buffer, buffer);
  static void construct(Context& cx, const MemHandle<MemVector<T>>& self,
                        std::size_t size = 0) {
    self->buffer = ArrayBuffer<T>::allocate(cx, size);
    self->size = size;
  }
  ArrayBuffer* buffer;
};

static_assert(std::is_standard_layout<MemVector<char>>::value,
              "MemVector must be a StandardLayoutType.");
#endif

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MEMVECTOR_HPP_
