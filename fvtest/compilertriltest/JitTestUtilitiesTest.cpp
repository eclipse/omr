/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "JitTest.hpp"
#include "gtest/gtest-spi.h"

TEST(PtrTest, AssertNullWithNullValue)
   {
   ASSERT_NULL(nullptr) << "This should always pass.";
   }

TEST(PtrTest, AssertNotNullWithNonNullValue)
   {
   ASSERT_NOTNULL(reinterpret_cast<void*>(0x1)) << "This should always pass.";
   }

TEST(PtrTest, AssertNullWithNonNullValue)
   {
   EXPECT_FATAL_FAILURE(ASSERT_NULL(reinterpret_cast<void*>(0x1)), "");
   }

TEST(PtrTest, AssertNotNullWithNullValue)
   {
   EXPECT_FATAL_FAILURE(ASSERT_NOTNULL(nullptr), "");
   }

TEST(PtrTest, ExpectNullWithNullValue)
   {
   EXPECT_NULL(nullptr) << "This should always pass.";
   }

TEST(PtrTest, ExpectNotNullWithNonNullValue)
   {
   EXPECT_NOTNULL(reinterpret_cast<void*>(0x1)) << "This should always pass.";
   }

TEST(PtrTest, ExpectNullWithNonNullValue)
   {
   EXPECT_NONFATAL_FAILURE(EXPECT_NULL(reinterpret_cast<void*>(0x1)), "");
   }

TEST(PtrTest, ExpectNotNullWithNullValue)
   {
   EXPECT_NONFATAL_FAILURE(EXPECT_NOTNULL(nullptr), "");
   }

TEST(TRTestCombineVectorTest, CombineEmptyVectorsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine(vector<int>{}, vector<int>{});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty vectors should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine(vector<long>{}, vector<char>{});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty vectors should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyAndNonEmptyVectorsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine(vector<int>{}, vector<int>{{1, 2, 3}});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineEmptyAndNonEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine(vector<long>{}, vector<char>{'a', 'b', 'c'});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyAndEmptyVectorsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine(vector<int>{{1, 2, 3}}, vector<int>{});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyAndEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine(vector<long>{{1, 2, 3}}, vector<char>{});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any vector with an empty vector should always result in another empty vector.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyVectorsOfSameType)
   {
   using namespace std;
   auto v1 = vector<int>{{1, 2, 3}};
   auto v2 = vector<int>{{4, 5}};
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_EQ(v1.size() * v2.size(), v.size())
         << "Size of combined vectors should be the product of the sizes of the two individual vectors.";
   }

TEST(TRTestCombineVectorTest, CombineNonEmptyVectorsOfDifferentTypes)
   {
   using namespace std;
   auto v1 = vector<long>{{1, 2, 3}};
   auto v2 = vector<char>{'a', 'b'};
   auto v = TRTest::combine(v1, v2);
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_EQ(v1.size() * v2.size(), v.size())
         << "Size of combined vectors should be the product of the sizes of the two individual vectors.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyListsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine<int, int>({}, {});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty lists should always result in an empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyListsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine<long, char>({}, {});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining two empty lists should always result in an empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyAndNonEmptyListsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine<int, int>({}, {1, 2, 3});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineEmptyAndNonEmptyListsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine<long, char>({}, {'a', 'b', 'c'});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyAndEmptyListsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine<int, int>({1, 2, 3}, {});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyAndEmptyListsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine<long, char>({1, 2, 3}, {});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_TRUE(v.empty())
         << "Combining any list with an empty list should always result in another empty vector.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyListsOfSameType)
   {
   using namespace std;
   auto v = TRTest::combine<int, int>({1, 2, 3}, {4, 5});
   ::testing::StaticAssertTypeEq<vector<tuple<int,int>>, decltype (v)>();
   ASSERT_EQ(3*2, v.size())
         << "Size of combined lists should be the product of the sizes of the two individual lists.";
   }

TEST(TRTestCombineBraceInitTest, CombineNonEmptyListsOfDifferentTypes)
   {
   using namespace std;
   auto v = TRTest::combine<long, char>({1, 2, 3}, {'a', 'b'});
   ::testing::StaticAssertTypeEq<vector<tuple<long,char>>, decltype (v)>();
   ASSERT_EQ(3*2, v.size())
         << "Size of combined lists should be the product of the sizes of the two individual lists.";
   }

TEST(TRTestFilter, FilterNothingFromEmptyVector)
   {
   auto v_in = std::vector<char>{};
   auto v_out = TRTest::filter(v_in, [](char c){ return false; }); // should filter nothing
   ASSERT_TRUE(v_out.empty()) << "Filtering an empty vector should result in another empty vector.";
   }

TEST(TRTestFilter, FilterEverythingFromEmptyVector)
   {
   auto v_in = std::vector<char>{};
   auto v_out = TRTest::filter(v_in, [](char c){ return true; }); // should filter everything
   ASSERT_TRUE(v_out.empty()) << "Filtering an empty vector should result in another empty vector.";
   }

TEST(TRTestFilter, FilterNothingFromVector)
   {
   auto v_in = std::vector<char>{'a', 'b', 'c'};
   auto v_out = TRTest::filter(v_in, [](char c){ return false; }); // should filter nothing
   ASSERT_EQ(v_in, v_out) << "Filtering nothing should just return the vector unchanged.";
   }

TEST(TRTestFilter, FilterEverythingFromVector)
   {
   auto v_in = std::vector<char>{'a', 'b', 'c'};
   auto v_out = TRTest::filter(v_in, [](char c){ return true; }); // should filter everything
   ASSERT_TRUE(v_out.empty()) << "Filtering everything from vector should result in an empty vector.";
   }

TEST(TRTestFilter, FilterVectorWithNoOccurrences)
   {
   auto pred = [](char c){ return c == 'c'; };
   auto v_in = std::vector<char>{'a', 'b', 'd', 'e'};
   auto v_out = TRTest::filter(v_in, pred);
   ASSERT_EQ(v_in, v_out)
         << "Filtering a vector that doesn't contain elements matching the predicate should just return the same vector.";
   ASSERT_EQ(0, std::count_if(v_out.cbegin(), v_out.cend(), pred))
         << "Filtering should leave no elements matching the filter predicate.";
   }

TEST(TRTestFilter, FilterVectorWithOneOccurrence)
   {
   auto pred = [](char c){ return c == 'c'; };
   auto v_in = std::vector<char>{'a', 'b', 'c', 'd', 'e'};
   auto v_out = TRTest::filter(v_in, pred);
   ASSERT_EQ(0, std::count_if(v_out.cbegin(), v_out.cend(), pred))
         << "Filtering should leave no elements matching the filter predicate.";
   }

TEST(TRTestFilter, FilterVectorWithManyOccurrences)
   {
   auto pred = [](char c){ return c == 'c'; };
   auto v_in = std::vector<char>{'a', 'c', 'b', 'c', 'c', 'd', 'c', 'e', 'c'};
   auto v_out = TRTest::filter(v_in, pred);
   ASSERT_EQ(0, std::count_if(v_out.cbegin(), v_out.cend(), pred))
         << "Filtering should leave no elements matching the filter predicate.";
   }
