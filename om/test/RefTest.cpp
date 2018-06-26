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
	EXPECT_FALSE(ref);
}

TEST(RefTest, nullIntReferent) {
	Ref<int> ref(nullptr);
	EXPECT_EQ(ref, nullptr);
	EXPECT_EQ(ref, Ref<void>(nullptr));
	EXPECT_EQ(ref, ref);
	EXPECT_FALSE(ref);
}

TEST(RefTest, intReferent) {
	int a = 99;
	int b = 88;

	Ref<int> ra = &a;
	Ref<int> rb = &b;

	EXPECT_EQ(ra, &a);
	EXPECT_EQ(*ra, 99);
	EXPECT_NE(ra, nullptr);
	EXPECT_EQ(ra, ra);
	EXPECT_EQ(ra.get(), &a);
	EXPECT_EQ(ra, Ref<void>(&a));

	EXPECT_NE(ra, rb);
	EXPECT_EQ(*rb, b);
	EXPECT_NE(*ra, *rb);
}

class A {};

class B : public A {};

bool check(const Ref<A>& a) { return true; }

TEST(RefTest, referentInheritsFromType) {
	A a;
	B b;

	{
		Ref<A> ra = &a;
		Ref<B> rb = &b;
		EXPECT_NE(ra, rb);
		EXPECT_NE(rb, ra);
	}

	{
		Ref<A> ra = &a;
		Ref<A> rb = &b;
		EXPECT_NE(ra, rb);
		EXPECT_NE(rb, ra);
	}

	{
		Ref<A> rb1 = &b;
		Ref<B> rb2 = &b;
		EXPECT_EQ(rb1, rb2);
		EXPECT_EQ(rb2, rb1);
	}

	{
		Ref<B> r = &b;
		check(r);
	}
}

}  // namespace Test
}  // namespace Om
}  // namespace OMR
