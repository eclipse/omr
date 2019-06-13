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

#ifndef JITTEST_HPP
#define JITTEST_HPP

#include <gtest/gtest.h>
#include <vector>
#include <stdexcept>
#include <iostream>
#include "control/Options.hpp"
#include "optimizer/Optimizer.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "omrport.h"
#include "Jit.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))
#define EXPECT_NULL(pointer) EXPECT_EQ(NULL, (pointer))
#define EXPECT_NOTNULL(pointer) EXPECT_TRUE(NULL != (pointer))

#define TRIL(code) #code

namespace TRTest
{

/**
 * @brief A test fixture that makes the port library available to its users
 *
 * This class makes it possible to make calls to the port library from within test cases.
 * Specifically, it makes it possible to use the port library macros without having to
 * write extra code in the test case body.
 *
 * The static methods `initPortLib()` and `shutdownPortLib()` must be called
 * externally (ideally from the global environment setup and teardown) to initialize
 * and shutdown the port library, respectively.
 */
class TestWithPortLib : public ::testing::Test
   {
   public:
   TestWithPortLib() : privateOmrPortLibrary(&PortLib) {}

   /**
    * @brief Exception for failed thread library initialization
    */
   class FailedThreadLibraryInit : public std::runtime_error
      {
      public:
      FailedThreadLibraryInit() : std::runtime_error("Failed to initialize the thread library.") {}
      };

   /**
    * @brief Exception for failing to attach current thread to thread library
    */
   class FailedCurrentThreadAttachment : public std::runtime_error
      {
      public:
      FailedCurrentThreadAttachment() : std::runtime_error("Failed to attach current thread to thread library.") {}
      };

   /**
    * @brief Exception for failed port library initialization
    */
   class FailedPortLibraryInit : public std::runtime_error
      {
      public:
      FailedPortLibraryInit() : std::runtime_error("Failed to initialize the port library.") {}
      };

   /**
    * @brief Initialize port library and thread library as a dependency
    *
    * Initialization should happen before any tests deriving from the JitTest
    * fixture are executed. If an error occures during one of the initialization
    * steps, an exception is thrown.
    */
   static void initPortLib()
      {
      if (0 != omrthread_init_library()) { throw FailedThreadLibraryInit(); }
      if (0 != omrthread_attach_ex(&current_thread, J9THREAD_ATTR_DEFAULT)) { throw FailedCurrentThreadAttachment(); }
      if (0 != omrport_init_library(&PortLib, sizeof(OMRPortLibrary))) { throw FailedPortLibraryInit(); }
      }

   /**
    * @brief Shutdown the port library and thread library
    *
    * Shutdown should only happen after all tests that derive from the JitTest
    * fixture have finished executing.
    */
   static void shutdownPortLib()
      {
      PortLib.port_shutdown_library(&PortLib);
      omrthread_shutdown_library();
      }

   protected:
   static OMRPortLibrary PortLib;         // global port library object for use in tests
   OMRPortLibrary *privateOmrPortLibrary; // pointer to object to be used by port library macro calls in tests

   private:
   static omrthread_t current_thread;  // handle for current thread; needed to initialize thread library
   };

/**
 * @brief The JitTest class is a basic test fixture for OMR compiler test cases.
 *
 * The fixture does the following for OMR compiler tests that use it:
 *
 * - initialize JIT just before the test starts
 * - shutdown JIT just after the test finishes executing
 * - makes port library macros available for use in test cases
 *
 * Example use:
 *
 *    class MyTestCase : public TRTest::JitTest {};
 */
class JitTest : public TestWithPortLib
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
   auto v = std::vector<std::tuple<typename L::value_type, typename R::value_type>>();
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
std::vector<std::tuple<L, R>> combine(std::vector<L> l, std::vector<R> r)
   {
   auto v = std::vector<std::tuple<L, R>>();
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
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<int64_t> const_values<int64_t>()
   {
   int64_t inputArray[] = { zero_value<int64_t>(),
                      one_value<int64_t>(),
                      negative_one_value<int64_t>(),
                      positive_value<int64_t>(),
                      negative_value<int64_t>(),
                      std::numeric_limits<int64_t>::min(),
                      std::numeric_limits<int64_t>::max(),
                      static_cast<int64_t>(std::numeric_limits<int64_t>::min() + 1),
                      static_cast<int64_t>(std::numeric_limits<int64_t>::max() - 1),
                      0x000000000000005FL,
                      0x0000000000000088L,
                      0x0000000080000000L,
                      0x7FFFFFFF7FFFFFFFL,
                      0x00000000FFFF0FF0L,
                      static_cast<int64_t>(0x800000007FFFFFFFL),
                      static_cast<int64_t>(0xFFFFFFF00FFFFFFFL),
                      static_cast<int64_t>(0x08000FFFFFFFFFFFL),
                    };

   return std::vector<int64_t>(inputArray, inputArray + sizeof(inputArray) / sizeof(int64_t));
   }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<uint64_t> const_values<uint64_t>()
   {
   uint64_t inputArray[] = { zero_value<uint64_t>(),
                      one_value<uint64_t>(),
                      negative_one_value<uint64_t>(),
                      positive_value<uint64_t>(),
                      negative_value<uint64_t>(),
                      std::numeric_limits<uint64_t>::min(),
                      std::numeric_limits<uint64_t>::max(),
                      static_cast<uint64_t>(std::numeric_limits<uint64_t>::min() + 1),
                      static_cast<uint64_t>(std::numeric_limits<uint64_t>::max() - 1),
                      0x000000000000005FL,
                      0x0000000000000088L,
                      0x0000000080000000L,
                      0x7FFFFFFF7FFFFFFFL,
                      0x00000000FFFF0FF0L,
                      0x800000007FFFFFFFL,
                      0xFFFFFFF00FFFFFFFL,
                      0x08000FFFFFFFFFFFL,
                      static_cast<uint64_t>(std::numeric_limits<int64_t>::min()),
                      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()),
                      static_cast<uint64_t>(std::numeric_limits<int64_t>::min() + 1),
                      static_cast<uint64_t>(std::numeric_limits<int64_t>::max() - 1),
                    };

   return std::vector<uint64_t>(inputArray, inputArray + sizeof(inputArray) / sizeof(uint64_t));
   }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<int32_t> const_values<int32_t>()
   {
   int32_t inputArray[] = { zero_value<int32_t>(),
                      one_value<int32_t>(),
                      negative_one_value<int32_t>(),
                      positive_value<int32_t>(),
                      negative_value<int32_t>(),
                      std::numeric_limits<int32_t>::min(),
                      std::numeric_limits<int32_t>::max(),
                      static_cast<int32_t>(std::numeric_limits<int32_t>::min() + 1),
                      static_cast<int32_t>(std::numeric_limits<int32_t>::max() - 1),
                      0x0000005F,
                      0x00000088,
                      static_cast<int32_t>(0x80FF0FF0),
                      static_cast<int32_t>(0x80000000),
                      static_cast<int32_t>(0xFF000FFF),
                      static_cast<int32_t>(0xFFFFFF0F)
                    };

   return std::vector<int32_t>(inputArray, inputArray + sizeof(inputArray) / sizeof(int32_t));
   }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<uint32_t> const_values<uint32_t>()
   {
   uint32_t inputArray[] = { zero_value<uint32_t>(),
                      one_value<uint32_t>(),
                      negative_one_value<uint32_t>(),
                      positive_value<uint32_t>(),
                      negative_value<uint32_t>(),
                      std::numeric_limits<uint32_t>::min(),
                      std::numeric_limits<uint32_t>::max(),
                      static_cast<uint32_t>(std::numeric_limits<uint32_t>::min() + 1),
                      static_cast<uint32_t>(std::numeric_limits<uint32_t>::max() - 1),
                      0x0000005F,
                      0x00000088,
                      0x80FF0FF0,
                      0x80000000,
                      0xFF000FFF,
                      0xFFFFFF0F,
                      static_cast<uint32_t>(std::numeric_limits<int32_t>::min()),
                      static_cast<uint32_t>(std::numeric_limits<int32_t>::max()),
                      static_cast<uint32_t>(std::numeric_limits<int32_t>::min() + 1),
                      static_cast<uint32_t>(std::numeric_limits<int32_t>::max() - 1),
                    };

   return std::vector<uint32_t>(inputArray, inputArray + sizeof(inputArray) / sizeof(uint32_t));
   }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<float> const_values<float>()
   {
   float inputArray[] = {
      zero_value<float>(),
      one_value<float>(),
      negative_one_value<float>(),
      positive_value<float>(),
      negative_value<float>(),
      std::numeric_limits<float>::min(),
      std::numeric_limits<float>::max(),
      static_cast<float>(std::numeric_limits<float>::min() + 1),
      static_cast<float>(std::numeric_limits<float>::max() - 1),
      0x0000005F,
      0x00000088,
      static_cast<float>(0x80FF0FF0),
      static_cast<float>(0x80000000),
      static_cast<float>(0xFF000FFF),
      static_cast<float>(0xFFFFFF0F),
      0.01f,
      0.1f
   };

   return std::vector<float>(inputArray, inputArray + sizeof(inputArray) / sizeof(float));
   }

/**
 * @brief Convenience function returning possible test inputs of the specified type
 */
template <>
inline std::vector<double> const_values<double>()
   {
   double inputArray[] = {
      zero_value<double>(),
      one_value<double>(),
      negative_one_value<double>(),
      positive_value<double>(),
      negative_value<double>(),
      std::numeric_limits<double>::min(),
      std::numeric_limits<double>::max(),
      static_cast<double>(std::numeric_limits<double>::min() + 1),
      static_cast<double>(std::numeric_limits<double>::max() - 1),
      0x0000005F,
      0x00000088,
      static_cast<double>(0x80FF0FF0),
      static_cast<double>(0x80000000),
      static_cast<double>(0xFF000FFF),
      static_cast<double>(0xFFFFFF0F),
      0.01,
      0.1
   };

   return std::vector<double>(inputArray, inputArray + sizeof(inputArray) / sizeof(double));
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

/**
 * @brief Enum values representing the reason for skipping a test
 *
 * These values are intended to be short descriptions of why a test is skipped
 * and are mostly useful for logging and reporting purposes. Additional explanations
 * should be specified in the skip message.
 */
enum SkipReason {
   KnownBug,               // test is skipped because of a known bug
   MissingImplementation,  // feature under test is not implemented
   UnsupportedFeature,     // feature under test is not supported
   NumSkipReasons_,        // DO NOT USE IN USER CODE
};

/**
 * @brief Stringification of SkipReason enum values
 */
static const char * const skipReasonStrings[] = {
   "Known Bug",
   "Missing Implementation",
   "Unsupported Feature",
};

static_assert(SkipReason::NumSkipReasons_ == sizeof(skipReasonStrings)/sizeof(char*),
             "SkipReason and skipReasonStrings do not have the same number of elements");

/**
 * @breif allow SkipReason instances to be streamed
 */
inline std::ostream& operator << (std::ostream& os, SkipReason reason)
   {
   return os <<  skipReasonStrings[static_cast<int>(reason)];
   }

/**
 * @brief A helper class to allow streaming messages to the SKIP_IF macro
 *
 * The strategy used to allow message streaming is based on the technique
 * used to implement message streaming in Google Tests ASSERT*() macros.
 *
 * This class serves a similar purpose as the `AssertHelper` class in
 * Google Test.
 */
class SkipHelper
   {
   public:

   explicit SkipHelper(SkipReason reason)
      : reason_(reason)
      {}

   /**
    * @brief Hack to allow a message to be streamed to the SKIP_IF macro
    *
    * The overload for this operator implements the "side effects" the SKIP_IF
    * macro performs when a test is skipped:
    *
    * - prints to stdout:
    *   - the reason for skipping
    *   - the name of the skipped test
    *   - the skip message
    * - records the skip reason as a property of the current test in Google Test generated logs
    * - emits a "success" event with the skip message
    *
    * @param stream object that represents the messages for a SKIP_IF invocation
    * @return void so that the result of an assignment can be used
    *         as the "return value" of a void retuning function
    */
   void operator = (const ::testing::Message& message) const
      {
      const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
      ::testing::Test::RecordProperty("skipped", skipReasonStrings[static_cast<int>(reason_)]);
      std::cout << reason_ << ": Skipping test: " << test_info->name() << "\n    " << message << "\n";
      SUCCEED() << message;
      }

   private:

   SkipReason reason_;
   };

/**
 * @brief A macro to allow a test to be conditionally skipped
 *
 * This macro allows a test to be conditionally skipped without failing the test.
 * Multiple invocations can be specified per test. While the macro can can be used
 * anywhere within the scope of a test, it is best to only use at at the beginning,
 * before the main body of a test.
 *
 * To skip a test, a condition aswell as a "reason" for skipping must be specified.
 * The condition may be any boolean expression. The "reason" must be a value from
 * the `SkipReason` enum (see its documentation for further details). Optionally,
 * a more detailed message can also be specified using the `<<` stream operator;
 * as is done with `ASSERT*()` macros. When the test suite is executed, skipped
 * tests will print the (short) reason for skipping, the test name, and the skip
 * message to stdout.
 *
 * The basic syntax for using this macro is:
 *
 *    SKIP_IF(<condition>, <reason>) << <message>;
 */
#define SKIP_IF(condition, reason) \
   switch (0) case 0: default: /* guard against ambiguous else */ \
   if (!(condition)) { /* allow test to proceed normally */ } \
   else \
      return SkipHelper(SkipReason::reason) = ::testing::Message()

#endif // JITTEST_HPP
