/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "JitTest.hpp"
#include "gtest/gtest-spi.h"

TEST(PtrTest, AssertNullWithNullValue)
   {
   ASSERT_NULL(NULL) << "This should always pass.";
   }

TEST(PtrTest, AssertNotNullWithNonNullValue)
   {
   ASSERT_NOTNULL(reinterpret_cast<void*>(0x1)) << "This should always pass.";
   }

TEST(PtrTest, AssertNotNullWithNullValue)
   {
   EXPECT_FATAL_FAILURE(ASSERT_NOTNULL(NULL), "");
   }

TEST(PtrTest, ExpectNullWithNullValue)
   {
   EXPECT_NULL(NULL) << "This should always pass.";
   }

TEST(PtrTest, ExpectNotNullWithNonNullValue)
   {
   EXPECT_NOTNULL(reinterpret_cast<void*>(0x1)) << "This should always pass.";
   }

TEST(PtrTest, ExpectNotNullWithNullValue)
   {
   EXPECT_NONFATAL_FAILURE(EXPECT_NOTNULL(NULL), "");
   }


TEST(TRTestCombineVectorTest, CombineEmptyVectorsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine(vector<int>(), vector<int>());
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty vectors should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine(vector<long>(), vector<char>());
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty vectors should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyAndNonEmptyVectorsOfSameType)
   {
   using namespace std;
   int test_array[3] = {1, 2 ,3};
   auto v = TRTest::combine(vector<int>(), vector<int> (test_array, test_array + 3));
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyAndNonEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   char test_array[3] = {'a', 'b', 'c'};
   auto v = TRTest::combine(vector<long>(), vector<char>(test_array, test_array + 3));
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyAndEmptyVectorsOfSameType)
   {
   using namespace std;
   int test_array[] = {1, 2, 3};
   auto v = TRTest::combine(vector<int> (test_array, test_array + 3), vector<int>());
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyAndEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   long test_array[] = {1, 2 ,3};
   auto v = TRTest::combine(vector<long>(test_array, test_array + 3), vector<char>());
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyVectorsOfSameType)
   {
   using namespace std;
   int test_array1[] = {1, 2, 3};
   int test_array2[] = {4, 5};
   auto v1 = vector<int> (test_array1, test_array1 + 3);
   auto v2 = vector<int> (test_array2, test_array2 + 2);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_EQ(v1.size() * v2.size(), v.size())
         << "Size of combined vectors should be the product of the sizes of the two individual vectors.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   long test_array1[3] = {1, 2, 3};
   char test_array2[2] = {'a', 'b'};
   auto v1 = vector<long> (test_array1, test_array1 + 3);
   auto v2 = vector<char> (test_array2, test_array2 + 2);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_EQ(v1.size() * v2.size(), v.size())
         << "Size of combined vectors should be the product of the sizes of the two individual vectors.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyListsOfSameType)
   {
   using namespace std;
   auto v1 = vector<int> ();
   auto v2 = vector<int> ();
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty lists should always result in an empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyListsOfDifferentTypes)
   {
   using namespace std;
   auto v1 = vector<long> ();
   auto v2 = vector<char> ();
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty lists should always result in an empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyAndNonEmptyListsOfSameType)
   {
   using namespace std;
   int test_array2[3] = {1, 2, 3};
   auto v1 = vector<int> ();
   auto v2 = vector<int> (test_array2, test_array2 + 3);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyAndNonEmptyListsOfDifferentTypes)
   {
   using namespace std;
   char test_array2[3] = {'a', 'b', 'c'};
   auto v1 = vector<long> ();
   auto v2 = vector<char> (test_array2, test_array2 + 3);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyAndEmptyListsOfSameType)
   {
   using namespace std;
   int test_array1[3] = {1, 2, 3};
   auto v1 = vector<int> (test_array1, test_array1 + 3);
   auto v2 = vector<int> ();
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyAndEmptyListsOfDifferentTypes)
   {
   using namespace std;
   long test_array1[3] = {1, 2, 3};
   auto v1 = vector<long> (test_array1, test_array1 + 3);
   auto v2 = vector<char> ();
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyListsOfSameType)
   {
   using namespace std;
   int test_array1[] = {1, 2, 3};
   int test_array2[] = {4, 5};
   auto v1 = vector<int> (test_array1, test_array1 + 3);
   auto v2 = vector<int> (test_array2, test_array2 + 2);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_EQ(3*2, v.size())
         << "Size of combined lists should be the product of the sizes of the two individual lists.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyListsOfDifferentTypes)
   {
   using namespace std;
   long test_array1[3] = {1, 2, 3};
   char test_array2[2] = {'a', 'b'};
   auto v1 = vector<long> (test_array1, test_array1 + 3);
   auto v2 = vector<char> (test_array2, test_array2 + 2);
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_EQ(3*2, v.size())
         << "Size of combined lists should be the product of the sizes of the two individual lists.";
   }


bool returnFalse(char c) {
    return false;
}

bool returnTrue(char c) {
    return true;
}

TEST(TRTestFilter, FilterNothingFromEmptyVector)
   {
   auto v_in = std::vector<char>();
   auto v_out = TRTest::filter(v_in, returnFalse); // should filter nothing
   ASSERT_TRUE(v_out.empty()) << "Filtering an empty vector should result in another empty vector.";
   }

TEST(TRTestFilter, FilterEverythingFromEmptyVector)
   {
   auto v_in = std::vector<char>();
   auto v_out = TRTest::filter(v_in, returnTrue); // should filter everything
   ASSERT_TRUE(v_out.empty()) << "Filtering an empty vector should result in another empty vector.";
   }

TEST(TRTestFilter, FilterNothingFromVector)
   {
   char test_array[3] = {'a', 'b', 'c'};
   auto v_in = std::vector<char>(test_array, test_array + 3);
   auto v_out = TRTest::filter(v_in, returnFalse); // should filter nothing
   ASSERT_EQ(v_in, v_out) << "Filtering nothing should just return the vector unchanged.";
   }

TEST(TRTestFilter, FilterEverythingFromVector)
   {
   char test_array[3] = {'a', 'b', 'c'};
   auto v_in = std::vector<char>(test_array, test_array + 3);
   auto v_out = TRTest::filter(v_in, returnTrue); // should filter everything
   ASSERT_TRUE(v_out.empty()) << "Filtering everything from vector should result in an empty vector.";
   }

bool isChar_c(char l) {
     return l=='c';
}

TEST(TRTestFilter, FilterVectorWithNoOccurrences)
   {
   char test_array[4] = {'a', 'b', 'd', 'e'};
   auto v_in = std::vector<char>(test_array, test_array + 4);
   auto v_out = TRTest::filter(v_in, isChar_c);
   ASSERT_EQ(v_in, v_out)
         << "Filtering a vector that doesn't contain elements matching the predicate should just return the same vector.";
   ASSERT_EQ(0, std::count_if(v_out.begin(), v_out.end(), isChar_c))
         << "Filtering should leave no elements matching the filter predicate.";
   }

TEST(TRTestFilter, FilterVectorWithOneOccurrence)
   {
   char test_array[5] = {'a', 'b', 'c', 'd', 'e'};
   auto v_in = std::vector<char>(test_array, test_array + 5);
   auto v_out = TRTest::filter(v_in, isChar_c);
   ASSERT_EQ(0, std::count_if(v_out.begin(), v_out.end(), isChar_c))
         << "Filtering should leave no elements matching the filter predicate.";
   }

TEST(TRTestFilter, FilterVectorWithManyOccurrences)
   {
   char test_array[9] = {'a', 'c', 'b', 'c', 'c', 'd', 'c', 'e', 'c'};
   auto v_in = std::vector<char>(test_array, test_array + 9);
   auto v_out = TRTest::filter(v_in, isChar_c);
   ASSERT_EQ(0, std::count_if(v_out.begin(), v_out.end(), isChar_c))
         << "Filtering should leave no elements matching the filter predicate.";
   }

TEST(SkipTest, SkipIfTrue)
   {
   SKIP_IF(true, UnsupportedFeature) << "Test is intentionally skipped to verify that skipping works";
   FAIL() << "SKIP_IF did not skip this test";
   }

TEST(SkipTest, SkipIfFalse)
   {
   EXPECT_FATAL_FAILURE(
      {
         SKIP_IF(false, KnownBug) << "Test should not have been skipped! SKIP_IF must have a bug.";
         FAIL() << "This test should not be skipped by SKIP_IF";
      },
      "This test should not be skipped by SKIP_IF");
   }

class TestWithPortLib : public TRTest::TestWithPortLib {};

TEST_F(TestWithPortLib, CheckCurrentPlatform)
   {
   ASSERT_STRNE("unknown", omrsysinfo_get_CPU_architecture())
      << "The host architecture should be known";
   }
