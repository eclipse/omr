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

class CallTest : public TRTest::JitTest {};

int32_t oracleBoracle(int32_t x) { return x + 1123; } // Randomish number to avoid accidental test passes.

TEST_F(CallTest, icallOracle) { 
    char inputTrees[200] = {0};
    const auto format_string = "(method return=Int32 args=[Int32] (block (ireturn (icall address=0x%jX args=[Int32] (iload parm=0)) )  ))";
    std::snprintf(inputTrees, 200, format_string, reinterpret_cast<uintmax_t>(&oracleBoracle));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    // Execution of this test is disabled on non-X86 platforms, as we 
    // do not have trampoline support, and so this call may be out of 
    // range for some architectures. 
#ifdef TR_TARGET_X86
    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();

    EXPECT_EQ(oracleBoracle(-1), entry_point(-1));
    EXPECT_EQ(oracleBoracle(-128), entry_point(-128));
    EXPECT_EQ(oracleBoracle(128), entry_point(128));
    EXPECT_EQ(oracleBoracle(1280), entry_point(1280));
#endif
}
