/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

class LocalPassByRefTest : public TRTest::JitTest {};

/*
 * Purpose of this test is to exercise the
 * optimizer when local stack variables (autos) are
 * passed by reference to functions, which may modify the
 * values. Thus the functin aliases the local stack values
 * and the optimizer needs to be aware of this aliasing.
 *
 * This requires two steps:
 *
 * The optimizer flag callsAsUses must be true. This flag
 * controls whether the optimizer will consider functions as
 * possibly aliasing local stack values.
 *
 * Secondly the language front end needs to set the aliasing
 * data whenever such aliasing can occur.
 *
 * The test below does neither of these things (as of now),
 * hence will fail.
 */

int foo(int*i, long long*l)
{
	*i = (int) *l;
	return 0;
}

TEST_F(LocalPassByRefTest, passByRefTest) { 
    char inputTrees[1024] = {0};
	const char format_string[] = 
		"(method return=Int32          "
		"  (block                      "
		"    (istore temp=\"a\"        "
		"      (iconst 5)              "
		"    )                         "
		"    (lstore temp=\"b\"        "
		"      (lconst 42)             "
		"    )                         "
		"    (treetop                  "
		"      (icall address=0x%jX args=[Address, Address] "
		"        (loadaddr temp=\"a\") "
		"        (loadaddr temp=\"b\") "
		"      )                       "
		"    )                         "
		"    (ireturn                  "
		"      (iadd                   "
		"        (l2i                  "
		"          (lload temp=\"b\")  "
		"        )                     "
		"        (iload temp=\"a\")    "
		"      )                       "
		"    )                         "
		"  )                           "
		")";
    std::snprintf(inputTrees, sizeof inputTrees, format_string, reinterpret_cast<uintmax_t>(&foo));
    auto trees = parseString(inputTrees);

    ASSERT_NOTNULL(trees) << "Trees failed to parse\n" << inputTrees;

    Tril::DefaultCompiler compiler{trees};
    ASSERT_EQ(0, compiler.compile()) << "Compilation failed unexpectedly\n" << "Input trees: " << inputTrees;

    auto entry_point = compiler.getEntryPoint<int32_t (*)()>();

    EXPECT_EQ(84, entry_point());
}
