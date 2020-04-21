/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

typedef int64_t (FooFunction)(int64_t, double, int32_t, double, int64_t, double, int32_t, double,
                              int64_t, double, int32_t, double, int64_t, double, int32_t, double);

DEFINE_BUILDER( FooBuilder,
                Int64,
                PARAM("arg0_int64", Int64),
                PARAM("arg1_double", Double),
                PARAM("arg2_int32", Int32 ),
                PARAM("arg3_double", Double),
                PARAM("arg4_int64", Int64),
                PARAM("arg5_double", Double),
                PARAM("arg6_int32", Int32),
                PARAM("arg7_double", Double),
                PARAM("arg8_int64", Int64),
                PARAM("arg9_double", Double),
                PARAM("arg10_int32", Int32 ),
                PARAM("arg11_double", Double),
                PARAM("arg12_int64", Int64),
                PARAM("arg13_double", Double),
                PARAM("arg14_int32", Int32),
                PARAM("arg15_double", Double))
   {
   auto comp0 = EqualTo(Load("arg0_int64"), Load("arg8_int64"));
   auto comp1 = EqualTo(Load("arg1_double"), Load("arg9_double"));
   auto comp2 = EqualTo(Load("arg2_int32"), Load("arg10_int32"));
   auto comp3 = EqualTo(Load("arg3_double"), Load("arg11_double"));
   auto comp4 = EqualTo(Load("arg4_int64"), Load("arg12_int64"));
   auto comp5 = EqualTo(Load("arg5_double"), Load("arg13_double"));
   auto comp6 = EqualTo(Load("arg6_int32"), Load("arg14_int32"));
   auto comp7 = EqualTo(Load("arg7_double"), Load("arg15_double"));
   auto comp8 = And(comp0,
                    And(comp1,
                        And(comp2,
                           And(comp3,
                              And(comp4,
                                 And(comp5,
                                    And(comp6, comp7)))))));
   Return(comp8);

   return true;
   }

class SystemLinkageTest : public JitBuilderTest {};

TEST_F(SystemLinkageTest, FooTest)
   {
   FooFunction *foo;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, FooBuilder, foo);

   ASSERT_TRUE(foo(200, 3.14159, -10, 6.67300 * pow(10, -11), -123, 0.9876, 999, 1.4142,
                   200, 3.14159, -10, 6.67300 * pow(10, -11), -123, 0.9876, 999, 1.4142));
   }
