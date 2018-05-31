#ifndef OMR_OM_MEMORYMANAGER_HPP_
#define OMR_OM_MEMORYMANAGER_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/MarkingFn.hpp>
#include <OMR/Om/Runtime.hpp>
#include <OMR/Om/Globals.hpp>
#include <OMR/Om/StartupError.hpp>

#include <set>

namespace OMR
{
namespace Om
{
class Cell;
class Shape;

class Visitor;
class Context;
class RunContext;
class StartupContext;


using ContextSet = ::std::set<Context*>;

void attachVmContext(OMR_VM& vm, Context& cx);

void detachVmContext(OMR_VM& vm, Context& cx);

class MemorySystem
{
public:
	explicit MemorySystem(ProcessRuntime& runtime);

	MemorySystem(const MemorySystem&) = delete;

	~MemorySystem();

	OMR_VM& vm() { return omrVm_; }

	const OMR_VM& vm() const { return omrVm_; }

	ProcessRuntime& runtime() { return runtime_; }

	const Globals& globals() const { return globals_; }

	MarkingFnVector userRoots() noexcept { return userRoots_; }

	const MarkingFnVector userRoots() const noexcept { return userRoots_; }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		globals_.visit(visitor);
		/// TODO: Reimplement user root callback function.
		// for (auto& fn : userRoots()) {
		// 	fn(visitor);
		// }
	}

	const ContextSet& contexts() const { return contexts_; }

	ContextSet& contexts() { return contexts_; }

protected:
	friend class Context;

private:
	void initOmrVm();

	void initOmrGc();

	void initOmrGcSlaveThreads(StartupContext& cx);

	ProcessRuntime& runtime_;
	OMR_VM omrVm_;
	Globals globals_;
	MarkingFnVector userRoots_;
	ContextSet contexts_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MEMORYMANAGER_HPP_
