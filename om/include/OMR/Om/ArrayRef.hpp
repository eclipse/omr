#if !defined(OMR_OM_ARRAY_HPP_)
#define OMR_OM_ARRAYREF_HPP_

#include <OMR/Om/Array.hpp>

namespace OMR
{
namespace Om
{

/// A typed array reference. Experimental.
template <typename T>
class ArrayRef
{
public:

	ArrayRef(Array* buffer) { buffer_ = buffer }

	const T& at(std::size_t index) const { return data()[index]; }

	T& at(std::size_t index) const { return data()[index]; }

	Array* buffer() { return buffer_; }

	const Array* buffer() const { return buffer_; }

	T* data() { return reinterpret_cast<T*>(buffer_->data()); }

	template <typename Visitor>
	visit(Context& cx, Visitor& visitor)
	{
		buffer_->visit(cx, visitor);
	}

	template <typename Visitor>

private:
	Array* buffer_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ARRAYREF_HPP_
