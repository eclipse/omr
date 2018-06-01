#include <OMR/Infra/BitUtilities.hpp>
#include <OMR/Infra/Double.hpp>
#include <gtest/gtest.h>

namespace OMR {
namespace Infra {
namespace Test {
TEST(DoubleTest, signBit) {
	EXPECT_TRUE(std::signbit(Double::fromRaw(Double::SIGN_MASK)));
	EXPECT_TRUE(std::signbit(Double::fromRaw(Double::toRaw(-0.1) & Double::SIGN_MASK)));
	EXPECT_FALSE(std::signbit(Double::fromRaw(Double::toRaw(0.45) & Double::SIGN_MASK)));
}

TEST(DoubleTest, Raw) {
	double d = 0.1;
	EXPECT_TRUE(Double::fromRaw(Double::toRaw(d)) == d);
}

TEST(DoubleTest, NaN) {
	// not NaN
	double d = 0.1;
	EXPECT_FALSE(Double::isNaN(Double::toRaw(d)));
	EXPECT_FALSE(Double::isSNaN(Double::toRaw(d)));
	EXPECT_FALSE(Double::isQNaN(Double::toRaw(d)));

	// qNaN
	EXPECT_TRUE(std::numeric_limits<double>::has_quiet_NaN);
	double qnan = std::numeric_limits<double>::quiet_NaN();
	EXPECT_TRUE((Double::toRaw(qnan) & Double::NAN_QUIET_TAG) == Double::NAN_QUIET_TAG);
	EXPECT_TRUE(Double::isNaN(Double::toRaw(qnan)));
	EXPECT_FALSE(Double::isSNaN(Double::toRaw(qnan)));
	EXPECT_TRUE(Double::isQNaN(Double::toRaw(qnan)));

	// sNaN
	EXPECT_TRUE(std::numeric_limits<double>::has_signaling_NaN);
	double snan = std::numeric_limits<double>::signaling_NaN();
	EXPECT_TRUE((Double::toRaw(snan) & Double::NAN_QUIET_TAG) == 0);
	EXPECT_TRUE(Double::isNaN(Double::toRaw(snan)));
	EXPECT_TRUE(Double::isSNaN(Double::toRaw(snan)));
	EXPECT_FALSE(Double::isQNaN(Double::toRaw(snan)));

	// infinity
	double inf = INFINITY;
	EXPECT_FALSE(Double::isNaN(Double::toRaw(inf)));
	EXPECT_FALSE(Double::isSNaN(Double::toRaw(inf)));
	EXPECT_FALSE(Double::isQNaN(Double::toRaw(inf)));
}

} // namespace Test
} // namespace Infra
} // namespace OMR
