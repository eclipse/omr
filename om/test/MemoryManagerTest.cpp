

#include <OMR/Infra/Span.hpp>
#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Array.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/MemoryManager.hpp>
#include <OMR/Om/Object.inl.hpp>
#include <OMR/Om/ShapeOperations.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>

#include <omrgc.h>

#include <gtest/gtest.h>

namespace OMR
{
namespace Om
{
namespace Test
{
ProcessRuntime runtime;

TEST(MemoryManagerTest, StartUpAMemoryManager)
{
	MemoryManager manager(runtime);
	EXPECT_NE(manager.globals().metaShape(), nullptr);
	EXPECT_NE(manager.globals().arrayBufferShape(), nullptr);
}

TEST(MemoryManagerTest, StartUpAContext)
{
	MemoryManager manager(runtime);
	Context cx(manager);
	EXPECT_NE(cx.globals().metaShape(), nullptr);
	EXPECT_NE(cx.globals().arrayBufferShape(), nullptr);
}

TEST(MemoryManagerTest, loseAnObjects)
{
	MemoryManager manager(runtime);
	Context cx(manager);

	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}));
	Object* object = allocateObject(cx, shape);

	EXPECT_NE(object->layout(), nullptr);
	// EXPECT_EQ(object->layout(), shape);
	OMR_GC_SystemCollect(cx.omrVmThread(), 0);
	// EXPECT_EQ(object->layout(), (Shape*)0x5e5e5e5e5e5e5e5eul);
}

TEST(MemoryManagerTest, keepAnObject)
{
	MemoryManager manager(runtime);
	Context cx(manager);

	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}));
	RootRef<Object> object(cx, allocateObject(cx, shape));
	EXPECT_EQ(object->layout(), shape.get());
	OMR_GC_SystemCollect(cx.omrVmThread(), 0);
	EXPECT_EQ(object->layout(), shape.get());
}

TEST(MemoryManagerTest, objectTransition)
{
	MemoryManager manager(runtime);
	Context cx(manager);

	RootRef<Shape> emptyObjectMap(cx, allocateRootObjectLayout(cx, {}));
	RootRef<Object> obj1(cx, allocateObject(cx, emptyObjectMap));

	const std::array<const SlotAttr, 1> attributes{
		{SlotAttr(SlotType(Id(0), CoreType::VALUE), Id(0))}};

	const Infra::Span<const Om::SlotAttr> span(attributes);
	Object::transition(cx, obj1, span, Om::hash(span));

	// check
	{
		auto m = obj1->layout();
		EXPECT_EQ(m->layout(), cx.globals().metaShape());
		EXPECT_EQ(m->instanceSlotAttrs(), span);
		EXPECT_EQ(m->instanceSlotCount(), 1);
		EXPECT_EQ(m->parentLayout()->instanceSlotOffset(), 0);
	}

	// second object
	{
		RootRef<Object> obj2(cx, allocateObject(cx, emptyObjectMap));
		auto m = obj2->takeExistingTransition(cx, attributes, Om::hash(attributes));
		EXPECT_NE(m, nullptr);
		EXPECT_EQ(m, obj1->layout());
		EXPECT_EQ(m, obj2->layout());
	}
}

TEST(MemoryManagerTest, objectTransitionReuse)
{
	MemoryManager manager(runtime);
	Context cx(manager);
}

}  // namespace Test
}  // namespace Om
}  // namespace OMR
