#if !defined(OMR_OM_CORETYPE_HPP_)
#define OMR_OM_CORETYPE_HPP_

#include <OMR/Om/Ref.hpp>
#include <OMR/Om/Value.hpp>
#include <cstdint>
#include <cstdlib>

namespace OMR {
namespace Om {

/// Fundamental, built in types.
/// At it's core, every SlotType is represented by one of these values.
/// Om only really cares about 3 categories of slots values
///   1. Naked GC pointer (REF)
///   2. Boxed GC pointer (VALUE)
///   3. Non-pointer
/// Beyond those 3 categories, we just need to know the width, so we can iterate
/// the object.
enum class CoreType {
	/// Binary data slots, of various sizes.
	INT8,
	INT16,
	INT32,
	INT64,
	FLOAT32,
	FLOAT64,
	VALUE, //< A polymorphic `Value`.
	REF,   //< A GC pointer
	PTR
};

/// Calculate the size of a CoreType. Fundamental types have a fixed size.
inline std::size_t coreTypeWidth(CoreType t) noexcept {
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

} // namespace Om
} // namespace OMR

#endif // OMR_OM_CORETYPE_HPP_
