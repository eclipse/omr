#if !defined(OMR_OM_ADDRESS_HPP_)
#define OMR_OM_ADDRESS_HPP_

#include <cstdint>

namespace OMR {
namespace Om {

/// Unsigned integer type capable of storing the address of any GC cell. In a compressed references
/// scheme, the size of this type is smaller.
using Address = std::uintptr_t;

} // namespace Om
} // namespace OMR

#endif // OMR_OM_ADDRESS_HPP_
