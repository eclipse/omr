
#include <OMR/Om/Allocation.hpp>
#include <OMR/Om/Allocator.hpp>
#include <OMR/Om/Context.hpp>
#include <OMR/Om/MemorySystem.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/RootRef.hpp>
#include <OMR/Om/Runtime.hpp>

#include <gtest/gtest.h>
#include <omrgc.h>

namespace OMR {
namespace Om {
namespace Test {
// clang-format off
std::vector<std::int64_t> signedIntegers = {
  0l, 1l, -1l, 42l, -42l, 1l << 46,
  std::numeric_limits<std::int32_t>::max(),
  std::numeric_limits<std::int32_t>::min(),
};
// clang-format on

TEST(ValueTest, signedIntegerConstructorRoundTrip) {
	for (auto i : signedIntegers) {
		Value value(AS_INT48, i);
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isInt48());
		EXPECT_FALSE(value.isUint48());
		EXPECT_FALSE(value.isDouble());
		EXPECT_EQ(value.kind(), Value::Kind::INT48);
		EXPECT_EQ(i, value.getInt48());
	}
}

TEST(ValueTest, setSignedIntegerRoundTrip) {
	for (auto i : signedIntegers) {
		Value value;
		value.setInt48(i);
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isInt48());
		EXPECT_FALSE(value.isUint48());
		EXPECT_FALSE(value.isDouble());
		EXPECT_EQ(value.kind(), Value::Kind::INT48);
		EXPECT_EQ(i, value.getInt48());
	}
}

// clang-format off
std::vector<std::uint64_t> unsignedIntegers = {
  0ul, 1ul, 42l, 42l, 0x0000FFFFFFFFFFFFul, 1l << 47,
  std::numeric_limits<std::uint32_t>::max(),
  std::numeric_limits<std::uint32_t>::min(),
};
// clang-format on

TEST(ValueTest, unsignedIntegerConstructorRoundTrip) {
	for (auto i : unsignedIntegers) {
		Value value(AS_UINT48, i);
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isUint48());
		EXPECT_FALSE(value.isInt48());
		EXPECT_FALSE(value.isDouble());
		EXPECT_EQ(value.kind(), Value::Kind::UINT48);
		EXPECT_EQ(i, value.getUint48());
	}
}

TEST(ValueTest, setUnsignedIntegerRoundTrip) {
	for (auto i : unsignedIntegers) {
		Value value;
		value.setUint48(i);
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isUint48());
		EXPECT_FALSE(value.isInt48());
		EXPECT_FALSE(value.isDouble());
		EXPECT_EQ(value.kind(), Value::Kind::UINT48);
		EXPECT_EQ(i, value.getUint48());
	}
}

TEST(ValueTest, canonicalNan) {
	EXPECT_TRUE(std::isnan(Infra::Double::fromRaw(Value::CANONICAL_NAN)));
	EXPECT_FALSE(std::signbit(Infra::Double::fromRaw(Value::CANONICAL_NAN)));
	EXPECT_TRUE(Infra::Double::isNaN(Value::CANONICAL_NAN));
	EXPECT_TRUE(Infra::Double::isQNaN(Value::CANONICAL_NAN));
	EXPECT_FALSE(Infra::Double::isSNaN(Value::CANONICAL_NAN));
	EXPECT_NE(Infra::Double::fromRaw(Value::CANONICAL_NAN),
	          Infra::Double::fromRaw(Value::CANONICAL_NAN));
}

TEST(ValueTest, doubleRoundTrip) {
	const std::vector<double> doubles = {0.0,
	                                     1.0,
	                                     43.21,
	                                     std::numeric_limits<double>::infinity(),
	                                     std::numeric_limits<double>::max(),
	                                     std::numeric_limits<double>::min()};

	for (auto d : doubles) {
		for (auto sign : {+1.0, -1.0}) {
			auto x = d * sign;
			Value value(AS_DOUBLE, x);
			EXPECT_EQ(x, value.getDouble());
			EXPECT_FALSE(value.isBoxedValue());
			EXPECT_TRUE(value.isDouble());
			EXPECT_FALSE(value.isInt48());
		}
	}
}

TEST(ValueTest, signalingNanDouble) {
	Value value(AS_DOUBLE, std::numeric_limits<double>::signaling_NaN());
	EXPECT_TRUE(std::isnan(value.getDouble()));
	EXPECT_FALSE(value.isBoxedValue());
	EXPECT_NE(value.getDouble(), Infra::Double::fromRaw(Value::CANONICAL_NAN));
	EXPECT_EQ(value.raw(), Value::CANONICAL_NAN);
}

TEST(ValueTest, quietNanDouble) {
	Value value(AS_DOUBLE, std::numeric_limits<double>::quiet_NaN());
	EXPECT_TRUE(std::isnan(value.getDouble()));
	EXPECT_FALSE(value.isBoxedValue());
	EXPECT_NE(value.getDouble(), Infra::Double::fromRaw(Value::CANONICAL_NAN));
	EXPECT_EQ(value.raw(), Value::CANONICAL_NAN);
}

TEST(ValueTest, pointerRoundTrip) {
	for (void* p : {(void*)0, (void*)1, (void*)(-1 & Value::PAYLOAD_MASK)}) {
		Value value(AS_PTR, p);
		EXPECT_EQ(p, value.getPtr());
		EXPECT_TRUE(value.isBoxedValue());
		EXPECT_TRUE(value.isPtr());
	}
}

} // namespace Test
} // namespace Om
} // namespace OMR
