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
#include <vector>
#include <stdexcept> 
#include "Jit.hpp"
#include "control/Options.hpp"
#include "optimizer/Optimizer.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))
#define EXPECT_NULL(pointer) EXPECT_EQ(NULL, (pointer))
#define EXPECT_NOTNULL(pointer) EXPECT_TRUE(NULL != (pointer))

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

   JitTest()
      {
      auto initSuccess = initializeJitWithOptions((char*)"-Xjit:acceptHugeMethods,enableBasicBlockHoisting,omitFramePointer,useILValidator,paranoidoptcheck");
      if (!initSuccess) 
         throw std::runtime_error("Failed to initialize jit");
      }

   ~JitTest()
      {
      shutdownJit();
      }
   };

/**
 * @brief A fixture for testing with a customized optimization strategy. 
 *
 * The design of this is such that it is expected sublasses will 
 * call addOptimization inside their constructor, so that SetUp will
 * know what opts to use.
 */
class JitOptTest : public JitTest 
   {
   public:

   JitOptTest() :
      JitTest(), _optimizations(), _strategy(NULL)
      {
      } 

   virtual void SetUp()
      {
      JitTest::SetUp();

      // This is an allocated pointer because the strategy needs to 
      // live as long as this fixture
      _strategy = new OptimizationStrategy[_optimizations.size() + 1];

      makeOptimizationStrategyArray(_strategy);
      TR::Optimizer::setMockStrategy(_strategy);
      }


   ~JitOptTest() 
      {
      TR::Optimizer::setMockStrategy(NULL);
      delete[] _strategy;
      }

   /**
    * Append a single optimization to the list of optimizations to perform.
    * The optimization is marked as `MustBeDone`.
    *
    * @param opt The optimization to perform.
    */
   void addOptimization(OMR::Optimizations opt)
      {
      OptimizationStrategy strategy = {opt, OMR::MustBeDone};
      _optimizations.push_back(strategy);
      }

   /**
    * Append an optimization strategy to the list of optimizations to perform.
    *
    * @param opts An array of optimizations to perform. The last item in this
    * array must be `endOpts` or `endGroup`.
    */
   void addOptimizations(const OptimizationStrategy *opts)
      {
      const OptimizationStrategy *end = opts;
      while(end->_num != OMR::endOpts && end->_num != OMR::endGroup)
         ++end;

      _optimizations.insert(_optimizations.end(), opts, end);
      }


   private:
   /**
    * Fill the array \p strategy with optimizations.
    *
    * @param[out] strategy An array with at least
    * `_optimization.size() + 1` elements.
    */
   void makeOptimizationStrategyArray(OptimizationStrategy *strat)
      {
      for(unsigned int i = 0; i < _optimizations.size(); ++i)
         {
         strat[i]._num = _optimizations[i]._num;
         strat[i]._options = _optimizations[i]._options;
         }

      strat[_optimizations.size()]._num = OMR::endOpts;
      strat[_optimizations.size()]._options = 0;
      }

   OptimizationStrategy*             _strategy;
   std::vector<OptimizationStrategy> _optimizations;
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
template <typename T> const T zero_value() { return static_cast<T>(0); }
template <typename T> const T one_value() { return static_cast<T>(1); }
template <typename T> const T negative_one_value() { return static_cast<T>(-1); }
template <typename T> const T positive_value() { return static_cast<T>(42); }
template <typename T> const T negative_value() { return static_cast<T>(-42); }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <typename T>
std::vector<T> const_values()
   {
   T inputArray[] = { zero_value<T>(),
                      one_value<T>(),
                      negative_one_value<T>(),
                      positive_value<T>(),
                      negative_value<T>(),
                      std::numeric_limits<T>::min(),
                      std::numeric_limits<T>::max(),
                      static_cast<T>(std::numeric_limits<T>::min() + 1),
                      static_cast<T>(std::numeric_limits<T>::max() - 1)
                    };
   
   return std::vector<T>(inputArray, inputArray + sizeof(inputArray) / sizeof(T));
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
