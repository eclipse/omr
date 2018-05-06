#if !defined(OMR_OM_SLOTDESCRIPTOR_HPP_)
#define OMR_OM_SLOTDESCRIPTOR_HPP_

#include <OMR/Infra/HashUtilities.hpp>
#include <OMR/Infra/Span.hpp>
#include <OMR/Om/Id.hpp>
#include <OMR/Om/Value.hpp>

#include <cstddef>
#include <functional>

namespace OMR
{
namespace Om
{
/// TODO: Move Ref somewhere else.
using Ref = std::uintptr_t;

/// Fundamental, built in types.
/// At it's core, every SlotType is represented by one of these values.
/// Om only really cares about 3 categories of slots values
///   1. Naked GC pointer (REF)
///   2. Boxed GC pointer (VALUE)
///   3. Non-pointer
/// Beyond those 3 categories, we just need to know the width, so we can iterate
/// the object.
enum class CoreType
{
	/// Binary data slots, of various sizes.
	INT8,
	INT16,
	INT32,
	INT64,
	FLOAT32,
	FLOAT64,
	VALUE,  //< A polymorphic `Value`.
	REF     //< A GC pointer.
};

/// Calculate the size of a CoreType. Fundamental types have a fixed size.
inline std::size_t
width(CoreType t) noexcept
{
	switch (t) {
	case CoreType::INT8: return 1;
	case CoreType::INT16: return 2;
	case CoreType::INT32: return 4;
	case CoreType::INT64: return 8;
	case CoreType::FLOAT32: return 4;
	case CoreType::FLOAT64: return 8;
	case CoreType::VALUE: return sizeof(Value);
	case CoreType::REF: return sizeof(Ref);
	default: break;
	}
	return -1;
}

class SlotType
{
public:
	constexpr SlotType(const SlotType&) = default;

	constexpr SlotType(Id id, CoreType coreType) noexcept : id_(id), coreType_(coreType) {}

	constexpr CoreType coreType() const noexcept { return coreType_; }

	constexpr Id id() const noexcept { return id_; }

	SlotType& id(Id id) noexcept
	{
		id_ = id;
		return *this;
	}

	constexpr std::size_t hash() const noexcept
	{
		return Infra::Hash::mix(id_.hash(), std::size_t(coreType()));
	}

	constexpr bool operator==(const SlotType& rhs) const noexcept { return id_ == rhs.id_; }

	constexpr bool operator!=(const SlotType& rhs) const noexcept { return id_ != rhs.id_; }

	std::size_t width() const noexcept { return ::OMR::Om::width(coreType()); }

private:
	Id id_;
	CoreType coreType_;
};

class SlotAttr
{
public:
	/// Allocate a SlotAttr, where the type is {id: 0, coreType: VALUE}
	constexpr explicit SlotAttr(Id id) noexcept : type_(Id(0), CoreType::VALUE), id_(id) {}

	constexpr SlotAttr(const SlotAttr&) noexcept = default;

	constexpr SlotAttr(const SlotType& type, const Id& id) noexcept : type_(type), id_(id) {}

	constexpr Id id() const noexcept { return id_; }

	SlotAttr& id(Id id)
	{
		id_ = id;
		return *this;
	}

	SlotType& type() noexcept { return type_; }

	constexpr const SlotType& type() const noexcept { return type_; }

	SlotAttr& type(const SlotType& type) noexcept
	{
		type_ = type;
		return *this;
	}

	constexpr std::size_t hash() const noexcept
	{
		return Infra::Hash::mix(id().hash(), type().hash());
	}

	constexpr bool operator==(const SlotAttr& rhs) const noexcept
	{
		return (id_ == rhs.id_) && (type_ == rhs.type_);
	}

	constexpr bool operator!=(const SlotAttr& rhs) const noexcept
	{
		return (id_ != rhs.id_) || (type_ != rhs.type_);
	}

	/// Width of the slot's value.
	constexpr std::size_t width() const noexcept { return type_.width(); }

	/// The fundamental type of the slot.
	constexpr CoreType coreType() const noexcept { return type_.coreType(); }

private:
	SlotType type_;
	Id id_;
};

static_assert(
	sizeof(SlotAttr) == sizeof(SlotType) + sizeof(Id),
	"SlotAttr must have no padding--needed for memcmp.");

static_assert(
	std::is_standard_layout<SlotAttr>::value,
	"Slot Descriptors are heap allocated, so must be StandardLayoutType.");

/// @group Span operations
/// @{

/// Deep hash of a Span of SlotAttrs.
inline std::size_t
hash(const Infra::Span<const SlotAttr>& ds)
{
	std::size_t h = 7;
	for (const SlotAttr& d : ds) {
		Infra::Hash::mix(h, d.hash());
	}
	return h;
}

/// Deep compare of two spans of SlotAttrs.
/// The two spans must have the same length and data.
inline bool
operator==(const Infra::Span<const SlotAttr>& lhs, const Infra::Span<const SlotAttr>& rhs)
{
	if (lhs.length() != rhs.length()) {
		return true;
	}

	if (lhs.value() == rhs.value()) {
		return true;
	}

	// TODO: if SlotAttr padding is zero, this could be memcmp'd.
	for (std::size_t i = 0; i < lhs.length(); i++) {
		if (lhs[i] != rhs[i]) {
			return false;
		}
	}

	return true;
}

inline bool
operator!=(const Infra::Span<const SlotAttr>& lhs, const Infra::Span<const SlotAttr>& rhs)
{
	return !(lhs == rhs);
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_SLOTDESCRIPTOR_HPP_
