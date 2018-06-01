#if !defined OMR_OM_SLOTTYPE_HPP_
#define OMR_OM_SLOTTYPE_HPP_

#include <OMR/Om/CoreType.hpp>
#include <OMR/Om/Id.hpp>
#include <cstdlib>

namespace OMR {
namespace Om {

class SlotType {
public:
	constexpr SlotType(const SlotType&) = default;

	constexpr SlotType(Id id, CoreType coreType) noexcept : id_(id), coreType_(coreType) {}

	constexpr CoreType coreType() const noexcept { return coreType_; }

	constexpr Id id() const noexcept { return id_; }

	SlotType& id(Id id) noexcept {
		id_ = id;
		return *this;
	}

	constexpr std::size_t hash() const noexcept {
		return Infra::Hash::mix(id_.hash(), std::size_t(coreType()));
	}

	constexpr bool operator==(const SlotType& rhs) const noexcept { return id_ == rhs.id_; }

	constexpr bool operator!=(const SlotType& rhs) const noexcept { return id_ != rhs.id_; }

	std::size_t width() const noexcept { return coreTypeWidth(coreType()); }

private:
	Id id_;
	CoreType coreType_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_SLOTTYPE_HPP_
