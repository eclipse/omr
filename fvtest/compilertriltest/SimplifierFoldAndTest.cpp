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
#include "default_compiler.hpp"
#include "il/Node.hpp"
#include "infra/ILWalk.hpp"
#include "ras/IlVerifier.hpp"
#include "ras/IlVerifierHelpers.hpp"
#include "SharedVerifiers.hpp" //for NoAndIlVerifier


/**
 * Test Fixture for SimplifierFoldAndTest that 
 * selects only the relevant opts for the test case
 */
class SimplifierFoldAndTest : public TRTest::JitOptTest
   {

   public:
   SimplifierFoldAndTest()
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

/*
 * method(int32_t parameter) 
 *   int64_t i = ((int64_t) parameter) & 0xFFFFFFFF00000000ll;
 *   return i;
 *
 * Note that the combo of the mask and width of the parameter means
 * the expression should always fold to zero. 
 */
TEST_F(SimplifierFoldAndTest, FoldHappens) {
    auto* inputTrees = "(method return=Int64 args=[Int32]  "
                       " (block                            "
                       "  (lreturn                         "
                       "   (land                           " 
                       "    (lconst 0xFFFFFFFF00000000)    "
                       "    (iu2l (iload parm=0))))))      "; 


    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    NoAndIlVerifier verifier;  

    ASSERT_EQ(0, compiler.compileWithVerifier(&verifier)) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int64_t (*)(int32_t)>();
    // Invoke the compiled method, and assert the output is correct.
    EXPECT_EQ(0ll, entry_point(0));
    EXPECT_EQ(0ll, entry_point(1));
    EXPECT_EQ(0ll, entry_point(-1));
    EXPECT_EQ(0ll, entry_point(-9));
    EXPECT_EQ(0ll, entry_point(2147483647));
}

