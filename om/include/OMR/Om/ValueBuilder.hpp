#if !defined(OMR_OM_VALUEBUILDER_HPP_)
#define OMR_OM_VALUEBUILDER_HPP_

#include <OMR/Om/Value.hpp>

#include <ilgen/IlBuilder.hpp>

namespace OMR {
namespace Om {
namespace ValueBuilder {

inline TR::IlValue* getInt48(TR::IlBuilder* b, TR::IlValue* v) {
	return b->And(v, b->ConstInt64(Om::Value::PAYLOAD_MASK));
}

inline TR::IlValue* fromInt48(TR::IlBuilder* b, TR::IlValue* i) {
	return b->Or(b->And(i, b->ConstInt64(Value::PAYLOAD_MASK)),
	             b->ConstInt64(Om::Value::Tag::INT48));
}

} // namespace ValueBuilder
} // namespace Om
} // namespace OMR

#endif // OMR_OM_VALUEBUILDER_HPP_
