#if !defined(OMR_OM_CONTEXT_HPP_)
#define OMR_OM_CONTEXT_HPP_

#include <OMR/Om/MarkingFn.hpp>
#include <OMR/Om/MemoryManager.hpp>
#include <OMR/Om/RootRef.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <unordered_set>

class MM_EnvironmentBase;

namespace OMR
{
namespace Om
{
struct Cell;
struct Shape;
struct MetaShape;
struct ObjectMap;
struct Object;

class Globals;
class RefSeq;

/// TODO: Protect the Context constructor, force applications to use RunContext.

/// A GC context.
class Context
{
public:
	static constexpr const char* THREAD_NAME = "OMR::Om::Context";

	Context(MemoryManager& manager);

	Context(const Context& other) = delete;

	~Context() noexcept;

	MemoryManager& manager() const noexcept { return *manager_; }

	const Globals& globals() const noexcept { return manager().globals(); }

	OMR_VMThread* omrVmThread() const noexcept { return omrVmThread_; }

	MM_EnvironmentBase* omrGcThread() const noexcept;

	RootRefSeq& stackRoots() noexcept { return stackRoots_; }

	MarkingFnVector& userRoots() noexcept { return userRoots_; }

	const MarkingFnVector& userRoots() const noexcept { return userRoots_; }

private:
	MemoryManager* manager_;
	OMR_VMThread* omrVmThread_;
	RootRefSeq stackRoots_;
	MarkingFnVector userRoots_;
};

static_assert(
	std::is_standard_layout<Context>::value,
	"The Om context must be a StandardLayoutType for calculating JIT offsets.");

inline Context&
getContext(OMR_VMThread& omrVmThread)
{
	return *(Context*)omrVmThread._language_vmthread;
}

inline Context&
getContext(OMR_VMThread* omrVmThread)
{
	return getContext(*omrVmThread);
}

/// A special limited context that is only used during startup or shutdown.
class StartupContext : public Context
{
protected:
	friend class MemoryManager;
	StartupContext(MemoryManager& manager) : Context(manager) {}

	StartupContext(const StartupContext& other) = delete;
};

/// A full runtime context.
class RunContext : public Context
{
public:
	RunContext(MemoryManager& manager) : Context(manager) {}
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_CONTEXT_HPP_
