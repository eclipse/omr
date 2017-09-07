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

/*
 * This file defines some minimal sanity tests for the JitBuilder Test Utilities.
 */

#include "JBTestUtil.hpp"
#include "gtest/gtest-spi.h"

DEFINE_TYPES(NoTypes) {}

/*
 * `JustReturn` generates a function that simply returns.
 */

DECLARE_BUILDER(JustReturn);

typedef void (*JustReturnFunctionType)(void);

DEFINE_BUILDER_CTOR(JustReturn)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("JustReturn");
   DefineReturnType(NoType);
   }

DEFINE_BUILDIL(JustReturn)
   {
   Return();

   return true;
   }

/*
 * `BadBuilder` simply fails to generate any IL. This should lead to a failed compilation.
 */

DECLARE_BUILDER(BadBuilder);

typedef void (*BadBuilderFunctionType)(void);

DEFINE_BUILDER_CTOR(BadBuilder)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("BadBuilder");
   DefineReturnType(NoType);
   }

DEFINE_BUILDIL(BadBuilder)
   {
   return false;
   }

class selftest : public JitBuilderTest {};

/*
 * `JustReturnTest` compiles the `JustReturn` method and executes it. The test
 * passes if the method is successfully compiled and no fatal failure occurs
 * from executing the compiled body.
 */

TEST_F(selftest, JustReturnTest)
   {
   JustReturnFunctionType justReturn;
   ASSERT_COMPILE(NoTypes, JustReturn, justReturn);
   ASSERT_NO_FATAL_FAILURE(justReturn());
   }

/*
 * `BadBuilderTest` attempts to compile the `BadBuilder` method. The test passes
 * if compilation results in a fatal failure.
 *
 * Due to technical limitations of Google Test, the test body must be encapsulated
 * into a separate function so that `EXPECT_FATAL_FAILURE` can be used to catch
 * fatal failures (the expected result of this test).
 */

static void badBuilderRunner()
   {
   BadBuilderFunctionType badBuilder;
   ASSERT_COMPILE(NoTypes,BadBuilder,badBuilder);
   }

TEST_F(selftest, BadBuilderTest)
   {
   EXPECT_FATAL_FAILURE(badBuilderRunner(), "Failed to compile method ");
   }
