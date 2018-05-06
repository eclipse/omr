#ifndef OMR_OM_MEMORYMANAGER_HPP_
#define OMR_OM_MEMORYMANAGER_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/MarkingFn.hpp>
#include <OMR/Om/MetaShape.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>

#include <set>
#include <stdexcept>

namespace OMR
{
namespace Om
{
struct Cell;
struct MetaShape;
struct ArrayBufferShape;

class Visitor;
class Context;
class RunContext;
class StartupContext;

class StartupError : public ::std::runtime_error
{
	using runtime_error::runtime_error;
};

/// A collection of singleton GC cells. The collection is initialized at startup
class Globals
{
public:
	MetaShape* metaShape() const noexcept { return metaMap_; }

	ArrayBufferShape* arrayBufferShape() const noexcept { return arrayBufferMap_; }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		visitor.rootEdge(this, (Cell*)metaMap_);
		visitor.rootEdge(this, (Cell*)arrayBufferMap_);
	}

protected:
	friend class MemoryManager;

	/// Allocate the globals. Throws StartupError if any allocation fails.
	void init(StartupContext& cx);

private:
	MetaShape* metaMap_               = nullptr;
	ArrayBufferShape* arrayBufferMap_ = nullptr;
};

using ContextSet = ::std::set<Context*>;

class MemoryManager
{
public:
	explicit MemoryManager(ProcessRuntime& runtime);

	MemoryManager(const MemoryManager&) = delete;

	~MemoryManager();

	OMR_VM& omrVm() { return omrVm_; }

	const OMR_VM& omrVm() const { return omrVm_; }

	ProcessRuntime& runtime() { return runtime_; }

	const Globals& globals() const { return globals_; }

	MarkingFnVector userRoots() noexcept { return userRoots_; }

	const MarkingFnVector userRoots() const noexcept { return userRoots_; }

	template <typename VisitorT>
	void visit(VisitorT& visitor)
	{
		globals_.visit(visitor);
		for (auto& fn : userRoots()) {
			fn(visitor);
		}
	}

	const ContextSet& contexts() const { return contexts_; }

	ContextSet& contexts() { return contexts_; }

protected:
	friend class Context;

private:
	void initOmrVm();

	void initOmrGc();

	ProcessRuntime& runtime_;
	OMR_VM omrVm_;
	Globals globals_;
	MarkingFnVector userRoots_;
	ContextSet contexts_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_MEMORYMANAGER_HPP_
