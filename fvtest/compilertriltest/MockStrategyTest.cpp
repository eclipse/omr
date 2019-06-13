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
 * Uses the mockStrategy support to select only an 
 * optimization that should have no effect on the 
 * existence of 'and' operations. 
 *
 * Coupling note: This is assuming the default optimization
 * strategy contains the simplifier. If it did not, this test
 * case would be invalid
 */
class MockStrategyTest : public TRTest::JitOptTest
   {

   public:
   MockStrategyTest()
      {
      /*
       * By adding a non-simplifier opt, we make sure the 
       * simplifier doesn't run. 
       */
      addOptimization(OMR::trivialDeadTreeRemoval);
      }

   };

/*
 * This tree should not fold, as the simplifier should be disabled. This
 * ensures that the mock optimizer support is working. 
 */
TEST_F(MockStrategyTest, FoldDoesntHappen) {
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

    ASSERT_NE(0, compiler.compileWithVerifier(&verifier)) 
       << "Simplifier simplified when it shouldn't have!";
}


