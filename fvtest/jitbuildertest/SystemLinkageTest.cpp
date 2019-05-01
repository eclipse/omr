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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "JBTestUtil.hpp"

#include <cmath>

typedef int64_t (FooFunction)(int64_t, double, int32_t, double,
                              int64_t, double, int32_t, double);

DEFINE_BUILDER( FooBuilder,
                Int64,
                PARAM("arg0_int64", Int64),
                PARAM("arg1_double", Double),
                PARAM("arg2_int32", Int32 ),
                PARAM("arg3_double", Double),
                PARAM("arg4_int64", Int64),
                PARAM("arg5_double", Double),
                PARAM("arg6_int32", Int32),
                PARAM("arg7_double", Double))
   {
   auto comp0 = EqualTo(Load("arg0_int64"), Load("arg4_int64"));
   auto comp1 = EqualTo(Load("arg1_double"), Load("arg5_double"));
   auto comp2 = EqualTo(Load("arg2_int32"), Load("arg6_int32"));
   auto comp3 = EqualTo(Load("arg3_double"), Load("arg7_double"));
   auto comp4 = And(comp0,
                    And(comp1,
                        And(comp2,comp3)));
   Return(comp4);

   return true;
   }

class SystemLinkageTest : public JitBuilderTest {};

TEST_F(SystemLinkageTest, FooTest)
   {
   FooFunction *foo;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, FooBuilder, foo);

   ASSERT_TRUE(foo(200, 3.14159, -10, 6.67300 * pow(10, -11),
                   200, 3.14159, -10, 6.67300 * pow(10, -11)));
   }
