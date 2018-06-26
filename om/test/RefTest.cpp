#include <OMR/Om/Ref.hpp>

#include <gtest/gtest.h>

namespace OMR {
namespace Om {
namespace Test {

TEST(RefTest, nullVoidReferent) {
	Ref<void> ref(nullptr);
	EXPECT_EQ(ref, nullptr);
	EXPECT_EQ(ref, Ref<int>(nullptr));
	EXPECT_EQ(ref, ref);
}

TEST(RefTest, nullIntReferent) {
	Ref<int> ref(nullptr);
	EXPECT_EQ(ref, nullptr);
	EXPECT_EQ(ref, Ref<void>(nullptr));
	EXPECT_EQ(ref, ref);
}

TEST(RefTest, intReferent) {
	int referent = 99;
	Ref<int> ref = &referent;

	EXPECT_EQ(ref, &referent);
	EXPECT_EQ(*ref, 99);
	EXPECT_NE(ref, nullptr);
	EXPECT_EQ(ref, ref);
	EXPECT_EQ(ref.get(), &referent);

	int referent2;
	auto ref2 = makeRef(&referent2);
	EXPECT_NE(ref, ref2);
}

}  // namespace Test
}  // namespace Om
}  // namespace OMR
