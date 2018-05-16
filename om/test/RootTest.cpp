
// #include <OMR/Om/Ref.hpp>

#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/MemHandle.hpp>
#include <OMR/Om/MemoryManager.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>
#include <OMR/Om/TransitionSet.hpp>

#include <gtest/gtest.h>

namespace OMR
{
namespace Om
{
namespace Test
{
ProcessRuntime runtime;

TEST(RootTest, basic)
{
	MemoryManager manager(runtime);
	Context cx(manager);
	RootRef<Object> root(cx, Object::allocate(cx));
	Handle<Object> handle(root);
	// MemHandle<Object::Base> member(handle, &Object::base);
}

}  // namespace Test
}  // namespace Om
}  // namespace OMR
