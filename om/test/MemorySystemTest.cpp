

#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/AnySlotWalker.hpp>
#include <OMR/Om/Array.hpp>
#include <OMR/Om/Context.inl.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/ObjectOperations.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/ShapeOperations.hpp>

#include <OMR/Infra/Span.hpp>
#include <gtest/gtest.h>
#include <omrgc.h>

namespace OMR {
namespace Om {
namespace Test {

ProcessRuntime runtime;

TEST(MemorySystemTest, StartUpAMemorySystem) {
	MemorySystem system(runtime);
	EXPECT_NE(system.globals().metaShape(), nullptr);
	EXPECT_NE(system.globals().arrayBufferShape(), nullptr);
}

TEST(MemorySystemTest, StartUpAContext) {
	MemorySystem system(runtime);
	Context cx(system);
	EXPECT_NE(cx.globals().metaShape(), nullptr);
	EXPECT_NE(cx.globals().arrayBufferShape(), nullptr);
}

TEST(MemorySystemTest, loseAnObjects) {
	MemorySystem system(runtime);
	Context cx(system);

	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}));
	Object* object = allocateObject(cx, shape);

	EXPECT_NE(object->layout(), nullptr);
	// EXPECT_EQ(object->layout(), shape);
	OMR_GC_SystemCollect(cx.vmContext(), 0);
	// EXPECT_EQ(object->layout(), (Shape*)0x5e5e5e5e5e5e5e5eul);
}

TEST(MemorySystemTest, keepAnObject) {
	MemorySystem system(runtime);
	Context cx(system);

	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}));
	RootRef<Object> object(cx, allocateObject(cx, shape));
	EXPECT_EQ(object->layout(), shape.get());
	OMR_GC_SystemCollect(cx.vmContext(), 0);
	EXPECT_EQ(object->layout(), shape.get());
}

class PrintVisitor {
public:
	template<typename SlotHandleT>
	bool edge(void* cell, SlotHandleT slot) const {
		std::cerr << slot.readReference() << std::endl;
		return true; // continue
	}
};

TEST(MemorySystemTest, print) {
	MemorySystem system(runtime);
	Context cx(system);

	RootRef<Shape> shape(cx, allocateRootObjectLayout(cx, {}, DEFAULT_INLINE_DATA_SIZE));

	ASSERT_NE(shape->shapeTreeData()->instanceInlineSlotsSize, 0);

	RootRef<Object> object(cx, allocateObject(cx, shape));

	Om::Id id(1);
	Om::SlotType type(Om::Id(0), Om::CoreType::VALUE);

	auto map = Om::transitionLayout(cx, object, {{type, id}});
	assert(map != nullptr);

	Om::SlotDescriptor descriptor;
	Om::lookupSlot(cx, object, id, descriptor);

	Om::setValue(cx, object.get(), descriptor, {AS_REF, object.get()});

	AnySlotWalker walker(object.get<Cell>());
	walker.traverse(PrintVisitor());

	// EXPECT_EQ(object->layout(), shape.get());
	// OMR_GC_SystemCollect(cx.vmContext(), 0);
	// EXPECT_EQ(object->layout(), shape.get());
}

TEST(MemorySystemTest, objectTransition) {
	MemorySystem system(runtime);
	Context cx(system);

	RootRef<Shape> emptyObjectMap(cx, allocateRootObjectLayout(cx, {}));
	RootRef<Object> obj1(cx, allocateObject(cx, emptyObjectMap));

	const std::array<const SlotAttr, 1> attributes{
	        {SlotAttr(SlotType(Id(0), CoreType::VALUE), Id(0))}};

	const Infra::Span<const Om::SlotAttr> span(attributes);
	transitionLayout(cx, obj1, span);

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
		auto m = takeExistingTransition(cx, obj2, attributes);
		EXPECT_NE(m, nullptr);
		EXPECT_EQ(m, obj1->layout());
		EXPECT_EQ(m, obj2->layout());
	}
}

TEST(MemorySystemTest, objectTransitionReuse) {
	MemorySystem system(runtime);
	Context cx(system);
}

} // namespace Test
} // namespace Om
} // namespace OMR
