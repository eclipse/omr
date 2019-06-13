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
#include "ras/IlVerifier.hpp"
#include "ras/IlVerifierHelpers.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))

class CompileTest : public Tril::Test::JitTest {};

TEST_F(CompileTest, Return3) {
    auto trees = parseString("(method return=\"Int32\" (block (ireturn (iconst 3))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(3, entry()) << "Compiled body did not return expected value";
}


TEST_F(CompileTest, NoCodeGen) {
    auto trees = parseString("(method return=\"Int32\" (block (ireturn (iconst 3))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler(trees);
    TR::NoCodegenVerifier verifier(NULL); 

    EXPECT_NE(0, compiler.compileWithVerifier(&verifier)) << "Compilation succeeded";

    EXPECT_TRUE(verifier.hasRun());
}
