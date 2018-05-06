
#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/Runtime.hpp>

#include <OMR/Om/Object.inl.hpp>
#include <OMR/Om/RootRef.inl.hpp>
#include <OMR/Om/MemoryManager.inl.hpp>
#include <OMR/Om/Allocator.inl.hpp>

#include <omrgc.h>

#include <gtest/gtest.h>

namespace OMR
{
namespace Om
{
namespace Test
{
// clang-format off
std::vector<std::int32_t> integers = {
  0, 1, -1, 42, -42,
  std::numeric_limits<std::int32_t>::max(),
  std::numeric_limits<std::int32_t>::min()
};
// clang-format on

TEST(ValueTest, integerConstructorRoundTrip)
{
	for (auto i : integers) {
		Value value(i);

		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isInteger());
		EXPECT_FALSE(value.isDouble());

		auto i2 = value.getInteger();
		EXPECT_EQ(i, i2);
	}
}

TEST(ValueTest, setIntegerRoundTrip)
{
	for (auto i : integers) {
		Value value;
		value.setInteger(i);
		auto i2 = value.getInteger();
		EXPECT_EQ(i, i2);
	}
}

TEST(ValueTest, canonicalNan)
{
	EXPECT_TRUE(std::isnan(Infra::Double::fromRaw(CANONICAL_NAN)));
	EXPECT_FALSE(std::signbit(Infra::Double::fromRaw(CANONICAL_NAN)));
	EXPECT_TRUE(Infra::Double::isNaN(CANONICAL_NAN));
	EXPECT_TRUE(Infra::Double::isQNaN(CANONICAL_NAN));
	EXPECT_FALSE(Infra::Double::isSNaN(CANONICAL_NAN));
	EXPECT_NE(Infra::Double::fromRaw(CANONICAL_NAN), Infra::Double::fromRaw(CANONICAL_NAN));
}

TEST(ValueTest, doubleRoundTrip)
{
	const std::vector<double> doubles =  //
		{0.0,
		 1.0,
		 43.21,
		 std::numeric_limits<double>::infinity(),
		 std::numeric_limits<double>::max(),
		 std::numeric_limits<double>::min()};

	for (auto d : doubles) {
		for (auto sign : {+1.0, -1.0}) {
			d *= sign;
			Value value;
			value.setDouble(d);
			EXPECT_EQ(d, value.getDouble());
			EXPECT_FALSE(value.isBoxedValue());
			EXPECT_TRUE(value.isDouble());
		}
	}
}

TEST(ValueTest, signalingNanDouble)
{
	Value value;
	value.setDouble(std::numeric_limits<double>::signaling_NaN());
	EXPECT_TRUE(std::isnan(value.getDouble()));
	EXPECT_FALSE(value.isBoxedValue());
	EXPECT_NE(value.getDouble(), Infra::Double::fromRaw(CANONICAL_NAN));
	EXPECT_EQ(value.raw(), CANONICAL_NAN);
}

TEST(ValueTest, quietNanDouble)
{
	Value value;
	value.setDouble(std::numeric_limits<double>::quiet_NaN());
	EXPECT_TRUE(std::isnan(value.getDouble()));
	EXPECT_FALSE(value.isBoxedValue());
	EXPECT_NE(value.getDouble(), Infra::Double::fromRaw(CANONICAL_NAN));
	EXPECT_EQ(value.raw(), CANONICAL_NAN);
}

TEST(ValueTest, pointerRoundTrip)
{
	for (void* p : {(void*)0, (void*)1, (void*)(-1 & VALUE_MASK)}) {
		Value value;
		value.setPtr(p);
		EXPECT_EQ(p, value.getPtr());
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isPtr());
	}
}

}  // namespace Test
}  // namespace Om
}  // namespace OMR
