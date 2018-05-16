#if !defined(OMR_OM_ARRAYBUFFERINITIALIZER_HPP_)
#define OMR_OM_ARRAYBUFFERINITIALIZER_HPP_

#include <OMR/Om/Array.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Initializer.hpp>

namespace OMR
{
namespace Om
{

class ArrayInitializer : public Initializer
{
public:
	explicit ArrayInitializer(std::size_t size) : size_(size) {}

	virtual Cell* operator()(Context& cx, Cell* cell) override
	{
		auto array = reinterpret_cast<Array*>(cell);
		Shape* layout = cx.globals().arrayBufferShape();
		new (array) Array(layout, size_);
		return reinterpret_cast<Cell*>(array);
	}

private:
	std::size_t size_;  //< Size of buffer, not number of elements.
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_ARRAYBUFFERINITIALIZER_HPP_
