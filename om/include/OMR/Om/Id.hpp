#if !defined(OMR_OM_ID_HPP_)
#define OMR_OM_ID_HPP_

#include <OMR/Infra/HashUtilities.hpp>
#include <cstddef>
#include <functional>

namespace OMR {
namespace Om {
using RawId = std::uint32_t;

class Id {
public:
	Id() = default;

	constexpr Id(const Id&) = default;

	constexpr explicit Id(RawId data) : data_(data) {}

	constexpr bool operator==(const Id& rhs) const noexcept { return data_ == rhs.data_; }

	constexpr bool operator!=(const Id& rhs) const noexcept { return data_ != rhs.data_; }

	constexpr bool isObject() const { return (data_ & TAG_MASK) == OBJECT_TAG; }

	constexpr bool isInteger() const { return (data_ & TAG_MASK) == FIXNUM_TAG; }

	constexpr RawId raw() const { return data_; }

	constexpr std::size_t hash() const { return Infra::Hash::hash(data_); }

private:
	static constexpr RawId HASH_SHIFT = 0x2;
	static constexpr RawId TAG_MASK = 0x3;
	static constexpr RawId FIXNUM_TAG = 0x0;
	static constexpr RawId OBJECT_TAG = 0x1;

	RawId data_;
};

class IdGenerator {
public:
	Id newId() { return Id(next_++); }

private:
	RawId next_ = 0;
};

} // namespace Om
} // namespace OMR

namespace std {
template<>
struct hash<OMR::Om::Id> {
	constexpr size_t operator()(const OMR::Om::Id& id) const noexcept { return id.raw(); }
};

} // namespace std

#endif // OMR_OM_ID_HPP_
