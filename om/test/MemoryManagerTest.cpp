

#include <OMR/Infra/Span.hpp>
#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Array.hpp>
#include <OMR/Om/ArrayBufferShape.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/MemoryManager.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/Shape.hpp>
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

	RootRef<Shape> shape(cx, Shape::allocate(cx));
	Object* object = Object::allocate(cx, shape);

	EXPECT_NE(object->shape(), nullptr);
	OMR_GC_SystemCollect(cx.omrVmThread(), 0);
	// EXPECT_EQ(object->shape(), (Shape*)0x5e5e5e5e5e5e5e5eul);
}

TEST(MemoryManagerTest, keepAnObject)
{
	MemoryManager manager(runtime);
	Context cx(manager);

	RootRef<Shape> shape(cx, Shape::allocate(cx));
	RootRef<Object> object(cx, Object::allocate(cx, shape));
	EXPECT_EQ(object->shape(), shape.get());
	OMR_GC_SystemCollect(cx.omrVmThread(), 0);
	EXPECT_EQ(object->shape(), shape.get());
}

TEST(MemoryManagerTest, objectTransition)
{
	MemoryManager manager(runtime);
	Context cx(manager);

	RootRef<Shape> emptyObjectMap(cx, Shape::allocate(cx));
	RootRef<Object> obj1(cx, Object::allocate(cx, emptyObjectMap));

	const std::array<const SlotAttr, 1> attributes{
		{SlotAttr(SlotType(Id(0), CoreType::VALUE), Id(0))}};

	const Infra::Span<const Om::SlotAttr> span(attributes);
	Object::transition(cx, obj1, span, Om::hash(span));

	// check
	{
		auto m = obj1->shape();
		EXPECT_EQ(m->baseCell().shape(), &cx.globals().metaShape()->baseShape());
		EXPECT_EQ(m->slotAttrs(), span);
		EXPECT_EQ(m->slotCount(), 1);
		EXPECT_EQ(m->parent()->slotOffset(), 0);
	}

	// second object
	{
		RootRef<Object> obj2(cx, Object::allocate(cx, emptyObjectMap));
		auto m = obj2->takeExistingTransition(cx, attributes, Om::hash(attributes));
		EXPECT_NE(m, nullptr);
		EXPECT_EQ(m, obj1->shape());
		EXPECT_EQ(m, obj2->shape());
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
