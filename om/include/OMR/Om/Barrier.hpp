#if !defined(OMR_OM_BARRIER_HPP_)
#define OMR_OM_BARRIER_HPP_

namespace OMR
{
namespace Om
{
namespace Barrier
{
using WriteBarrier = ::MM_StandardWriteBarrier;

class WriteBarrier
{
};

class PreBarrier
{
};

class PostBarrier
{
};

template <typename T>
struct WriteBarrier
{
};

template <typename T>
struct ReadBarrier
{
};

template <typename T>
struct Barrier
{
public:
	T& raw() { return value_; }

	T value_;
};

template <typename T>
struct BarrierRef
{
};

template <typename C = Cell, typename Member = Value>
prebarrier(C* cell, Member){

};

template <typename Container, typename Member, typename Value>
Value
write(Context& cx, Container* cell, Member* member, Value value)
{
	prebarrier(cell, member);
	*member = value;
	postbarrier(cell, member);
	return value;
}

}  // namespace Barrier
}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_BARRIER_HPP_
