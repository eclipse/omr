#if !defined(OMR_OM_CONTEXT_HPP_)
#define OMR_OM_CONTEXT_HPP_

#include <OMR/Om/MarkingFn.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/RootList.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <unordered_set>

class MM_EnvironmentBase;

namespace OMR {
namespace Om {

class Cell;
class Shape;
class Object;
class Globals;

/// TODO: Protect the Context constructor, force applications to use RunContext.

/// A GC context.
class Context {
public:
	static constexpr const char* THREAD_NAME = "OMR::Om::Context";

	Context(MemorySystem& system);

	Context(const Context& other) = delete;

	~Context() noexcept;

	MemorySystem& system() const noexcept { return *manager_; }

	const Globals& globals() const noexcept { return system().globals(); }

	OMR_VMThread* vmContext() const noexcept { return vmContext_; }

	MM_EnvironmentBase* gcContext() const noexcept;

	RootList& stackRoots() noexcept { return stackRoots_; }

	MarkingFnVector& userMarkingFns() noexcept { return userMarkingFns_; }

	const MarkingFnVector& userMarkingFns() const noexcept { return userMarkingFns_; }

private:
	static void attachVmContext(OMR_VM& vm, Context& cx);

	static void detachVmContext(OMR_VM& vm, Context& cx);

	Thread thread_;
	MemorySystem* manager_;
	OMR_VMThread* vmContext_;
	RootList stackRoots_;
	MarkingFnVector userMarkingFns_;
};

static_assert(std::is_standard_layout<Context>::value,
              "The Om context must be a StandardLayoutType for calculating JIT offsets.");

inline Context& getContext(OMR_VMThread& vmContext) {
	return *(Context*)vmContext._language_vmthread;
}

inline Context& getContext(OMR_VMThread* vmContext) { return getContext(*vmContext); }

/// A special limited context that is only used during startup or shutdown.
class StartupContext : public Context {
protected:
	friend class MemorySystem;
	StartupContext(MemorySystem& system) : Context(system) {}

	StartupContext(const StartupContext& other) = delete;
};

/// A full runtime context.
class RunContext : public Context {
public:
	RunContext(MemorySystem& system) : Context(system) {}
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_CONTEXT_HPP_
