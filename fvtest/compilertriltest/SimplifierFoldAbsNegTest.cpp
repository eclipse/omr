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

#include <cmath>
#include "JitTest.hpp"
#include "default_compiler.hpp"
#include "il/Node.hpp"
#include "infra/ILWalk.hpp"
#include "ras/IlVerifier.hpp"
#include "ras/IlVerifierHelpers.hpp"
#include "SharedVerifiers.hpp" //for NoAndIlVerifier

template <typename T> char* prefixForType();
template <> char* prefixForType<int32_t>() { return "i"; }
template <> char* prefixForType<int64_t>() { return "l"; }
template <> char* prefixForType<  float>() { return "f"; }
template <> char* prefixForType< double>() { return "d"; }

template <typename T> char* nameForType();
template <> char* nameForType<int32_t>() { return "Int32" ; }
template <> char* nameForType<int64_t>() { return "Int64" ; }
template <> char* nameForType<  float>() { return "Float" ; }
template <> char* nameForType< double>() { return "Double"; }

template <typename T> std::vector<T> dataForType();

template <> std::vector<int32_t> dataForType<int32_t>()
   {
   static int32_t values[] = { 0, 1, 2, -1, -2, 99999, -99999, std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };

   return std::vector<int32_t>(values, values + sizeof(values) / sizeof(int32_t));
   }

template <> std::vector<int64_t> dataForType<int64_t>()
   {
   static int64_t values[] = { 0, 1, 2, -1, -2, 99999, -99999, std::numeric_limits<int64_t>::max(), std::numeric_limits<int64_t>::min() };

   return std::vector<int64_t>(values, values + sizeof(values) / sizeof(int64_t));
   }

template <> std::vector<float> dataForType<float>()
   {
   static float values[] = { 0, 1, 2, -1, -2, 3.14F, -3.14F, std::numeric_limits<float>::min(), std::numeric_limits<float>::min() };

   return std::vector<float>(values, values + sizeof(values) / sizeof(float));
   }

template <> std::vector<double> dataForType<double>()
   {
   static double values[] = { 0, 1, 2, -1, -2, 3.14F, -3.14F, std::numeric_limits<double>::min(), std::numeric_limits<double>::min() };

   return std::vector<double>(values, values + sizeof(values) / sizeof(double));
   }

class SimplifierFoldAbsNegTestIlVerifierBase : public TR::IlVerifier
   {
   public:
   int32_t verify(TR::ResolvedMethodSymbol *sym)
      {
      for(TR::PreorderNodeIterator iter(sym->getFirstTreeTop(), sym->comp()); iter.currentTree(); ++iter)
         {
         int32_t rtn = verifyNode(iter.currentNode());
         if(rtn)
            return rtn;
         }
      return 0;
      }
   protected:
      virtual int32_t verifyNode(TR::Node *node) = 0;
   };

/**
 * Test Fixture for SimplifierFoldAbsNegTest that
 * selects only the relevant opts for the test case
 */
template <typename T>
class SimplifierFoldAbsNegTest : public TRTest::JitOptTest
   {

   public:
   SimplifierFoldAbsNegTest()
      {
      /* Add an optimization.
       * You can add as many optimizations as you need, in order,
       * using `addOptimization`, or add a group using
       * `addOptimizations(omrCompilationStrategies[warm])`.
       * This could also be done in test cases themselves.
       */
      addOptimization(OMR::treeSimplification);
      }

   };

typedef ::testing::Types<int32_t, int64_t, float, double> Types;
TYPED_TEST_CASE(SimplifierFoldAbsNegTest, Types);


class NoAbsAbsIlVerifier : public SimplifierFoldAbsNegTestIlVerifierBase
   {
   protected:
   virtual int32_t verifyNode(TR::Node *node)
      {
      return node->getOpCode().isAbs() && node->getChild(0)->getOpCode().isAbs();
      }
   };

/*
 * method(T parameter)
 *   return abs(abs(parameter));
 */
TYPED_TEST(SimplifierFoldAbsNegTest, FoldAbsAbs) {
    char inputTrees[256];
    Tril::format(inputTrees, sizeof(inputTrees), "(method return=%s args=[%s]    "
                                                  " (block                        "
                                                  "  (%sreturn                    "
                                                  "   (%sabs                      "
                                                  "    (%sabs (%sload parm=0))))))",
                  nameForType<TypeParam>(), nameForType<TypeParam>(),
                  prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    NoAbsAbsIlVerifier verifier;

    ASSERT_EQ(0, compiler.compileWithVerifier(&verifier)) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam(*)(TypeParam)>();

    // Invoke the compiled method, and assert the output is correct.
    auto values = dataForType<TypeParam>();
    for (auto it = values.begin(); it != values.end(); ++it)
       {
       EXPECT_EQ(std::abs(*it), entry_point(*it));
       }
}


class NoAbsNegIlVerifier : public SimplifierFoldAbsNegTestIlVerifierBase
   {
   protected:
   virtual int32_t verifyNode(TR::Node *node)
      {
      return node->getOpCode().isAbs() && node->getChild(0)->getOpCode().isNeg();
      }
   };

/*
 * method(T parameter)
 *   return abs(neg(parameter));
 */
TYPED_TEST(SimplifierFoldAbsNegTest, FoldAbsNeg) {
    char inputTrees[256];
    Tril::format(inputTrees, sizeof(inputTrees), "(method return=%s args=[%s]    "
                                                  " (block                        "
                                                  "  (%sreturn                    "
                                                  "   (%sabs                      "
                                                  "    (%sneg (%sload parm=0))))))",
                  nameForType<TypeParam>(), nameForType<TypeParam>(),
                  prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    NoAbsNegIlVerifier verifier;

    ASSERT_EQ(0, compiler.compileWithVerifier(&verifier)) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam(*)(TypeParam)>();

    // Invoke the compiled method, and assert the output is correct.
    auto values = dataForType<TypeParam>();
    for (auto it = values.begin(); it != values.end(); ++it)
       {
       EXPECT_EQ(std::abs(*it), entry_point(*it));
       }
}


class NoNegNegIlVerifier : public SimplifierFoldAbsNegTestIlVerifierBase
   {
   protected:
   virtual int32_t verifyNode(TR::Node *node)
      {
      return node->getOpCode().isNeg() && node->getChild(0)->getOpCode().isNeg();
      }
   };

/*
 * method(T parameter)
 *   return neg(neg(parameter));
 */
TYPED_TEST(SimplifierFoldAbsNegTest, FoldNegNeg) {
    char inputTrees[256];
    Tril::format(inputTrees, sizeof(inputTrees), "(method return=%s args=[%s]    "
                                                  " (block                        "
                                                  "  (%sreturn                    "
                                                  "   (%sneg                      "
                                                  "    (%sneg (%sload parm=0))))))",
                  nameForType<TypeParam>(), nameForType<TypeParam>(),
                  prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>(), prefixForType<TypeParam>());

    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    NoNegNegIlVerifier verifier;

    ASSERT_EQ(0, compiler.compileWithVerifier(&verifier)) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<TypeParam(*)(TypeParam)>();

    // Invoke the compiled method, and assert the output is correct.
    auto values = dataForType<TypeParam>();
    for (auto it = values.begin(); it != values.end(); ++it)
       {
       EXPECT_EQ(*it, entry_point(*it));
       }
}
