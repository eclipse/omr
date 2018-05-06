#if !defined(OMR_OM_ARRAY_HPP_)
#define OMR_OM_ARRAY_HPP_

#include <OMR/Om/ArrayBuffer.hpp>

namespace OMR
{
namespace Om
{
/// A typed vector backed by a gc'd buffer.
template <typename T>
class Array
{
public:
	Array(std::size_t capacity) { buffer_ = allocateA }

	const T& at(std::size_t index) const { return data()[index]; }

	T& at(std::size_t index) const { return data()[index]; }

	ArrayBuffer* buffer() { return buffer_; }

	const ArrayBuffer* buffer() const { return buffer_; }

	T* data() { return reinterpret_cast<T*>(buffer_->data()); }

	template <typename Visitor>
	visit(Context& cx, Visitor& visitor)
	{
		buffer_->visit(cx, visitor);
	}

private:
	ArrayBuffer* buffer_;
};

template <typename T>
class Box
{
	ArrayBuffer* buffer_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAY_HPP_
