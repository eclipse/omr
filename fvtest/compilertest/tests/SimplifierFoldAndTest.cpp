/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#include <limits.h>
#include <stdio.h>
#include "gtest/gtest.h"
#include "il/Node.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/MethodInfo.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "infra/ILWalk.hpp"
#include "OptTestDriver.hpp"
#include "ras/IlVerifier.hpp"

namespace TestCompiler
{

/* An IlInjector. This will be used to create the IL that will be compiled.
 * Note that this can be an IlBuilder, or anything else that acts like an
 * IlInjector.
 * Note that this is not an IlGenerator since ResolvedMethod requires an
 * IlInjector.
 */
class SimplifierFoldAndIlInjector : public TR::IlInjector
   {
   public:

   TR_ALLOC(TR_Memory::IlGenerator)

   SimplifierFoldAndIlInjector(TR::TypeDictionary *types, TestDriver *test)
   :
      TR::IlInjector(types, test)
      {
      }

   bool injectIL()
      {
      createBlocks(1);

      TR::SymbolReference* i = newTemp(Int64);

      // int64_t i = ((int64_t) parameter) & 0xFFFFFFFF00000000ll;
      storeToTemp(i,
            createWithoutSymRef(TR::land, 2,
                  iu2l
                       (parameter(0, Int32)),
                  lconst(0xFFFFFFFF00000000ll)));

      // return i;
      returnValue(loadTemp(i));

      return true;
      }
   };

/* Information about the ILInjector. This describes its
 * signature (name and parameters) and declares the IlInjector.
 */
class SimplifierFoldAndInfo : public TestCompiler::MethodInfo
   {
   public:
   SimplifierFoldAndInfo(TestDriver *test)
   :
      _ilInjector(&_types, test)
      {
      // TODO: make it easier to access Int32/Int64/etc.
      TR::IlType* Int32 = _types.PrimitiveType(TR::Int32);
      TR::IlType* Int64 = _types.PrimitiveType(TR::Int64);
      _args[0] = Int32;
      DefineFunction(__FILE__, LINETOSTR(__LINE__), "simplifierFoldAnd", 1, _args, Int64);
      DefineILInjector(&_ilInjector);
      }

   typedef int64_t (*MethodType)(int32_t);

   private:
   TR::TypeDictionary _types;
   TestCompiler::SimplifierFoldAndIlInjector _ilInjector;
   TR::IlType *_args[1];
   };

/* Verify the optimization was correctly applied.
 * A test failure is triggered by using `EXPECT_*`, and
 * compilation is stopped by returning a non-zero return code.
 * `ADD_FAILURE` is equivalent to expecting something false.
 *
 * While anything can be done in `verify`, this test
 * is implemented by traversing the IL, and checking if
 * there are AND operations.
 */
class SimplifierFoldAndIlVerifier : public TR::IlVerifier
   {
   public:
   int32_t verifyNode(TR::Node *node)
      {
      switch(node->getOpCodeValue())
         {
         case TR::land:
         case TR::iand:
         case TR::band:
         case TR::sand:
            ADD_FAILURE() << "Found an AND operation: " << node->getOpCodeValue();
            return 1;
         default:
            break;
         }
      return 0;
      }

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
   };

class SimplifierFoldAndTest : public OptTestDriver
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

   void invokeTests()
      {
      // Get the compiled method, cast to `SimplifierFoldAndInfo::MethodType`.
      auto testCompiledMethod = getCompiledMethod<SimplifierFoldAndInfo::MethodType>();

      // Invoke the compiled method, and assert the output is correct.
      ASSERT_EQ(0ll, testCompiledMethod(0));
      ASSERT_EQ(0ll, testCompiledMethod(1));
      ASSERT_EQ(0ll, testCompiledMethod(-1));
      ASSERT_EQ(0ll, testCompiledMethod(-9));
      ASSERT_EQ(0ll, testCompiledMethod(2147483647));
      }
   };

/* A test case.
 *
 * Note that this is a test fixture, since the macro starts with `TEST_F`.
 * This means its first parameter must be a class (which inherits from
 * `::testing::Test`, in this case through `OptTestDriver`). It must also
 * be defined within the same namespace as the class.
 */
TEST_F(SimplifierFoldAndTest, SimplifierFoldAndTest)
   {
   // Set the MethodInfo. This contains the IlInjector and method signature.
   SimplifierFoldAndInfo info(this);
   setMethodInfo(&info);

   /* Set the Verifier. Multiple verifiers can be run using `AllIlVerifier`.
    * Other helpers are in `IlVerifierHelpers.hpp`.
    * This is called after optimizations are applied on the generated IL,
    * but before codegen takes place.
    */
   SimplifierFoldAndIlVerifier ilVer;
   setIlVerifier(&ilVer);

   /* To run the test, either use `Verify()` or `VerifyAndInvoke()`.
    * The former generates IL, runs the selected optimizations,
    * verifies the optimized IL, then exits. The latter continues
    * to codegen, then runs the tests in `invokeTests`.
    */
   VerifyAndInvoke();
   }

}
