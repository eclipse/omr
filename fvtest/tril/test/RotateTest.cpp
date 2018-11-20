/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

class RotateTest : public Tril::Test::JitTest {};

TEST_F(RotateTest, irol1) {
    auto trees = parseString("(method name=\"irol1\" return=\"Int32\" args=[\"Int32\", \"Int32\"] (block (ireturn (irol (iload parm=0) (iload parm=1)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int32_t (*)(int32_t, int32_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0xABCDEF01, entry(0xABCDEF01, 0)) << "Compiled body did not return expected value";
    ASSERT_EQ(0xDEF01ABC, entry(0xABCDEF01, 12)) << "Compiled body did not return expected value";
    ASSERT_EQ(0x1ABCDEF0, entry(0xABCDEF01, 92)) << "Compiled body did not return expected value";
}

TEST_F(RotateTest, irol2) {
    auto trees = parseString("(method name=\"irol2\" return=\"Int32\" args=[\"Int32\"] (block (ireturn (irol (iload parm=0) (iconst 20)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0xF01ABCDE, entry(0xABCDEF01)) << "Compiled body did not return expected value";
}

TEST_F(RotateTest, irol3) {
    auto trees = parseString("(method name=\"irol2\" return=\"Int32\" args=[\"Int32\"] (block (ireturn (irol (iload parm=0) (iconst 34)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int32_t (*)(int32_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0xAF37BC06, entry(0xABCDEF01)) << "Compiled body did not return expected value";
}

TEST_F(RotateTest, lrol1) {
    auto trees = parseString("(method name=\"lrol1\" return=\"Int64\" args=[\"Int64\", \"Int32\"] (block (lreturn (lrol (lload parm=0) (iload parm=1)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int64_t (*)(int64_t, int32_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0xABCDEF0123456789LL, entry(0xABCDEF0123456789LL, 0)) << "Compiled body did not return expected value";
    ASSERT_EQ(0xDEF0123456789ABCLL, entry(0xABCDEF0123456789LL, 12)) << "Compiled body did not return expected value";
    ASSERT_EQ(0x123456789ABCDEF0LL, entry(0xABCDEF0123456789LL, 92)) << "Compiled body did not return expected value";
}

TEST_F(RotateTest, lrol2) {
    auto trees = parseString("(method name=\"lrol2\" return=\"Int64\" args=[\"Int64\"] (block (lreturn (lrol (lload parm=0) (iconst 20)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0xF0123456789ABCDELL, entry(0xABCDEF0123456789LL)) << "Compiled body did not return expected value";
}

TEST_F(RotateTest, lrol3) {
    auto trees = parseString("(method name=\"lrol3\" return=\"Int64\" args=[\"Int64\"] (block (lreturn (lrol (lload parm=0) (iconst 93)))))");

    ASSERT_NOTNULL(trees);

    Tril::DefaultCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int64_t (*)(int64_t)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(0x2468ACF13579BDE0LL, entry(0xABCDEF0123456789LL)) << "Compiled body did not return expected value";
}
