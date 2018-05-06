#if !defined(OMR_OM_ARRAYBUFFERINITIALIZER_HPP_)
#define OMR_OM_ARRAYBUFFERINITIALIZER_HPP_

#include <OMR/Om/ArrayBuffer.hpp>
#include <OMR/Om/Context.hpp>

namespace OMR
{
namespace Om
{

class ArrayBufferInitializer : public Initializer
{
public:
	explicit ArrayBufferInitializer(std::size_t size) : size_(size) {}

	virtual Cell* operator()(Context& cx, Cell* cell) override
	{
		auto buffer = reinterpret_cast<ArrayBuffer*>(cell);
		auto map    = cx.globals().arrayBufferShape();
		new (buffer) ArrayBuffer(map, size_);
		return &buffer->baseCell();
	}

private:
	std::size_t size_;  //< Size of buffer, not number of elements.
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_ARRAYBUFFERINITIALIZER_HPP_
