#if !defined(OMR_OM_STARTUPERROR_HPP_)
#define OMR_OM_STARTUPERROR_HPP_

#include <stdexcept>

namespace OMR {
namespace Om {

class StartupError : public ::std::runtime_error
{
	using runtime_error::runtime_error;
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_STARTUPERROR_HPP_
