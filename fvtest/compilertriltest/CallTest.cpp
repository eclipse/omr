/*******************************************************************************
 * Copyright IBM Corp. and others 2017
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "JitTest.hpp"
#include "default_compiler.hpp"

class CallTest : public TRTest::JitTest {};

int32_t oracleBoracle(int32_t x) { return x + 1123; } // Randomish number to avoid accidental test passes.

TEST_F(CallTest, icallOracle) { 

	SKIP_ON_PPC(MissingImplementation) << "Test is skipped on POWER because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64(MissingImplementation) << "Test is skipped on POWER 64 because calls are not currently supported (see issue #1645)";
    SKIP_ON_PPC64LE(MissingImplementation) << "Test is skipped on POWER 64le because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390_LINUX(MissingImplementation) << "Test is skipped on S390 Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_S390X_LINUX(MissingImplementation) << "Test is skipped on S390x Linux because calls are not currently supported (see issue #1645)";
    SKIP_ON_ARM(MissingImplementation) << "Test is skipped on ARM because calls are not currently supported (see issue #1645)";
    SKIP_ON_AARCH64(MissingImplementation) << "Test is skipped on AArch64 because calls are not currently supported (see issue #1645)";

    char inputTrees[200] = {0};
    const auto format_string = "(method return=Int32 args=[Int32] (block (ireturn (icall address=0x%X args=[Int32] (iload parm=0)) )  ))";
    Tril::format(inputTrees, 200, format_string, &oracleBoracle);
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler(trees);
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)(int32_t)>();

    EXPECT_EQ(oracleBoracle(-1), entry_point(-1));
    EXPECT_EQ(oracleBoracle(-128), entry_point(-128));
    EXPECT_EQ(oracleBoracle(128), entry_point(128));
    EXPECT_EQ(oracleBoracle(1280), entry_point(1280));
}
