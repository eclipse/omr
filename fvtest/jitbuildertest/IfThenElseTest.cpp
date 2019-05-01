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

// Both path exist, no merge return needed
DEFINE_BUILDER(SubIfThenElseBothPathBuilder,
               Int64,
               PARAM("valueA", Int64),
               PARAM("valueB", Int64) )
   {
   OMR::JitBuilder::IlBuilder *elsePath = NULL;
   OMR::JitBuilder::IlBuilder *thenPath = NULL;

   IfThenElse(&thenPath, &elsePath, ConstInt64(0));
   thenPath->Return(
                   thenPath->Sub(
                   thenPath->Load("valueA"),
                   thenPath->Load("valueB")));
   elsePath->Return(
                   elsePath->Sub(
                   elsePath->Load("valueB"),
                   elsePath->Load("valueA")));
   return true;
   }

// one path exist and will be taken, still need hardcoded Return
DEFINE_BUILDER(SubIfThenNullBuilder,
               Int64,
               PARAM("valueA", Int64),
               PARAM("valueB", Int64) )
   {
   OMR::JitBuilder::IlBuilder *thenPath = NULL;

   IfThenElse(&thenPath, NULL, ConstInt64(1));
   thenPath->Return(
                   thenPath->Sub(
                   thenPath->Load("valueA"),
                   thenPath->Load("valueB")));

   Return(Sub(
              Load("valueB"),
              Load("valueA")));
   return true;
   }

// The NULL path will be taken, need hardcoded Return
DEFINE_BUILDER(SubIfNullElseBuilder,
               Int64,
               PARAM("valueA", Int64),
               PARAM("valueB", Int64) )
   {
   OMR::JitBuilder::IlBuilder *elsePath = NULL;

   IfThenElse(NULL, &elsePath, ConstInt64(1));
   elsePath->Return(
                   elsePath->Sub(
                   elsePath->Load("valueB"),
                   elsePath->Load("valueA")));

   Return(Sub(
              Load("valueA"),
              Load("valueB")));
   return true;
   }

typedef int64_t (*IfLongFunctionType)(int64_t, int64_t);
class IfThenElseTest : public JitBuilderTest {};

TEST_F(IfThenElseTest, TrueBothPath)
   {
   IfLongFunctionType typeIfFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, SubIfThenElseBothPathBuilder, typeIfFunction);
   ASSERT_EQ(typeIfFunction(10, 5), -5);
   }

TEST_F(IfThenElseTest, TrueElsePath)
   {
   IfLongFunctionType typeIfFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, SubIfThenNullBuilder, typeIfFunction);
   ASSERT_EQ(typeIfFunction(10, 5), 5);
   }

TEST_F(IfThenElseTest, FalseBothPath)
   {
   IfLongFunctionType typeIfFunction;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, SubIfNullElseBuilder, typeIfFunction);
   ASSERT_EQ(typeIfFunction(10, 5), 5);
   }
