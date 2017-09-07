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

#ifndef JITTEST_HPP
#define JITTEST_HPP

#include <gtest/gtest.h>
#include "Jit.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(nullptr, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(nullptr != (pointer))
#define EXPECT_NULL(pointer) EXPECT_EQ(nullptr, (pointer))
#define EXPECT_NOTNULL(pointer) EXPECT_TRUE(nullptr != (pointer))

#define TRIL(code) #code

extern "C" bool initializeJitWithOptions(char *options);

namespace TRTest
{

/**
 * @brief The JitBuilderTest class is a basic test fixture for JitBuilder test cases.
 *
 * Most JitBuilder test case fixtures should publically inherit from this class.
 *
 * Example use:
 *
 *    class MyTestCase : public JitBuilderTest {};
 */
class JitTest : public ::testing::Test
   {
   public:

   static void SetUpTestCase()
      {
      auto initSuccess = initializeJitWithOptions((char*)"-Xjit:acceptHugeMethods,enableBasicBlockHoisting,omitFramePointer,useIlValidator,paranoidoptcheck");
      ASSERT_TRUE(initSuccess) << "Failed to initialize the JIT.";
      }

   static void TearDownTestCase()
      {
      shutdownJit();
      }
   };

/**
 * @brief Returns the Cartesian product of two standard-conforming containers
 * @tparam L is the type of the first input standard-conforming container
 * @tparam R is the type of the second input standard-conforming container
 * @param l is the first input standard-conforming container
 * @param r is the second input standard-conforming container
 * @return the Cartesian product of the input containers as a std::vector of std::tuple
 *
 * Example:
 *
 *    combine(std::vector<int>{1, 2, 3}, std::list<float>{4.0, 5.0, 6.0})
 */
template <typename L, typename R>
std::vector<std::tuple<typename L::value_type, typename R::value_type>> combine(L l, R r)
   {
   auto v = std::vector<std::tuple<typename L::value_type, typename R::value_type>>{};
   v.reserve((l.end() - l.begin())*(r.end() - r.begin()));
   for (auto i = l.begin(); i != l.end(); ++i)
      for (auto j = r.begin(); j != r.end(); ++j)
         v.push_back(std::make_tuple(*i, *j));
   return v;
   }

/**
 * @brief Returns the Cartesian product of two initializer lists
 * @tparam L is the type of elements in the first initializer list
 * @tparam R is the type of elements in the second initializer list
 * @param l is the first initializer list
 * @param r is the second initializer list
 * @return the Cartesian product of the two input lists
 *
 * Because of the rules surrounding type-deduction of initializer lists, the
 * types of the elements of the two lists must be explicitly specified when
 * calling this function.
 *
 * Example:
 *
 *    combine<int, float>({1, 2, 3}, {4.0, 5.0, 6.0})
 *
 */
template <typename L, typename R>
std::vector<std::tuple<L, R>> combine(std::initializer_list<L> l, std::initializer_list<R> r)
   {
   auto v = std::vector<std::tuple<L, R>>{};
   v.reserve((l.end() - l.begin())*(r.end() - r.begin()));
   for (auto i = l.begin(); i != l.end(); ++i)
      for (auto j = r.begin(); j != r.end(); ++j)
         v.push_back(std::make_tuple(*i, *j));
   return v;
   }

/**
 * @brief Given standard container and a predicate, returns a copy of the
 *    container with the elements matching the predicate removed
 */
template <typename C, typename Predicate>
C filter(C range, Predicate pred) {
    auto end = std::remove_if(range.begin(), range.end(), pred);
    range.erase(end, range.end());
    return range;
}

/**
 * @brief A family of functions returning constants of the specified type
 */
template <typename T> constexpr T zero_value() { return static_cast<T>(0); }
template <typename T> constexpr T one_value() { return static_cast<T>(1); }
template <typename T> constexpr T negative_one_value() { return static_cast<T>(-1); }
template <typename T> constexpr T positive_value() { return static_cast<T>(42); }
template <typename T> constexpr T negative_value() { return static_cast<T>(-42); }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <typename T>
std::vector<T> const_values()
   {
   return std::vector<T>{ zero_value<T>(),
                          one_value<T>(),
                          negative_one_value<T>(),
                          positive_value<T>(),
                          negative_value<T>(),
                          std::numeric_limits<T>::min(),
                          std::numeric_limits<T>::max(),
                          std::numeric_limits<T>::min() + 1,
                          std::numeric_limits<T>::max() - 1
                        };
   }

/**
 * @brief Convenience function returning pairs of possible test inputs of the specified types
 */
template <typename L, typename R>
std::vector<std::tuple<L,R>> const_value_pairs()
   {
   return TRTest::combine(const_values<L>(), const_values<R>());
   }

} // namespace CompTest

#endif // JITTEST_HPP
