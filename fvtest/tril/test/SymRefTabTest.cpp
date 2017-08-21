/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include "JitTest.hpp"
#include "jitbuilder_compiler.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(nullptr, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(nullptr != (pointer))

class SymRefTabTest : public Tril::Test::JitTest {};

TEST_F(SymRefTabTest, Return3) {
    auto trees = parseString("(method return=Int32"
                                 "(symreftab"
                                    "(symref offset=0 (auto name=foo type=Address size=8 setflags=[IsAutomatic]) )"
                                 ")"
                                 "(block (ireturn (iconst 3))))");

    ASSERT_NOTNULL(trees);

    Tril::JitBuilderCompiler compiler{trees};

    ASSERT_EQ(0, compiler.compile()) << "Compilation failed";

    auto entry = compiler.getEntryPoint<int32_t (*)(void)>();
    ASSERT_NOTNULL(entry) << "Entry point of compiled body cannot be null";
    ASSERT_EQ(3, entry()) << "Compiled body did not return expected value";
}
