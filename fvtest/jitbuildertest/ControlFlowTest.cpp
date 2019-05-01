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

typedef void (*DoubleReturnFunction)(void);

DEFINE_BUILDER( DoubleReturn,
                NoType )
   {
   auto firstBuilder = OrphanBytecodeBuilder(0, (char *)"return");
   AppendBuilder(firstBuilder);
   firstBuilder->Return();

   auto secondBuilder = OrphanBytecodeBuilder(1, (char *)"return"); // should be ignored and cleaned up
   secondBuilder->Return();

   return true;
   }

class ControlfFlowTest : public JitBuilderTest {};

TEST_F(ControlfFlowTest, DoubleReturnTest)
   {
   DoubleReturnFunction doubleReturn;
   ASSERT_COMPILE(OMR::JitBuilder::TypeDictionary, DoubleReturn, doubleReturn);
   ASSERT_NO_FATAL_FAILURE(doubleReturn());
   }
