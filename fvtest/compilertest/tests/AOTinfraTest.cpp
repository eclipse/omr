/**********************************************************************
* Copyright (c) 2020, 2020 IBM Corp. and others
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
**********************************************************************/

#include "gtest/gtest.h"
#include "tests/AOTinfraTest.hpp"
#include "env/AOTMethodHeader.hpp"

namespace TestCompiler
{

void
AOTinfraTest::AOTMethodHeaderTest()
   {

   uint32_t firstCompiledCodeSize = 64;
   uint8_t* firstCompiledCodeStart = reinterpret_cast<uint8_t*>(&firstCompiledCodeSize);
   uint32_t firstRelocationsSize = 128;
   uint8_t* firstRelocationsStart = reinterpret_cast<uint8_t*>(&firstRelocationsSize);

   TR::AOTMethodHeader firstInstance(firstCompiledCodeStart, firstCompiledCodeSize, firstRelocationsStart, firstRelocationsSize);

   uint32_t firstCompiledCodeSizeOutput = firstInstance.self()->compiledCodeSize;
   uint8_t* firstCompiledCodeStartOutput = firstInstance.self()->compiledCodeStart;
   uint32_t firstRelocationsSizeOutput = firstInstance.self()->relocationsSize;
   uint8_t* firstRelocationsStartOutput = firstInstance.self()->relocationsStart;

   uint8_t* rawData = reinterpret_cast<uint8_t*>(firstInstance.self()->serialize());
   TR::AOTMethodHeader secondInstance(rawData);
   uint8_t* returnedData = reinterpret_cast<uint8_t*>(secondInstance.self()->serialize());
  
   uint32_t secondCompiledCodeSize = secondInstance.self()->compiledCodeSize;
   uint8_t* secondCompiledCodeStart = secondInstance.self()->compiledCodeStart;
   uint32_t secondRelocationsSize = secondInstance.self()->relocationsSize;
   uint8_t* secondRelocationsStart = secondInstance.self()->relocationsStart;

   EXPECT_EQ(*rawData,*returnedData);

   EXPECT_EQ(firstCompiledCodeSize,firstCompiledCodeSizeOutput);
   EXPECT_EQ(firstCompiledCodeStart,firstCompiledCodeStartOutput);
   EXPECT_EQ(firstRelocationsSize,firstRelocationsSizeOutput);
   EXPECT_EQ(firstRelocationsStart,firstRelocationsStartOutput);

   /**
    * After serialization the pointer is no longer the same
    */

   EXPECT_EQ(firstCompiledCodeSize,secondCompiledCodeSize);
   EXPECT_EQ(*firstCompiledCodeStart,*secondCompiledCodeStart);
   EXPECT_EQ(firstRelocationsSize,secondRelocationsSize);
   EXPECT_EQ(*firstRelocationsStart,*secondRelocationsStart);
   }

}// namespace TestCompiler


TEST(JITILBuilderTest, AOTMethodHeaderTest)
   {
   TestCompiler::AOTinfraTest _aotinfraTest;
   _aotinfraTest.AOTMethodHeaderTest();
   }

