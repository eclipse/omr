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
#include "ilgen.hpp"

#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/CompileMethod.hpp"
#include "env/jittypes.h"
#include "gtest/gtest.h"
#include "il/DataTypes.hpp"
#include "ilgen.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

#define ASSERT_NULL(pointer) ASSERT_EQ(NULL, (pointer))
#define ASSERT_NOTNULL(pointer) ASSERT_TRUE(NULL != (pointer))

class IlGenTest : public Tril::Test::JitTest {};

// TODO (#4719): This test is currently broken on AIX, since it is not valid to use the return value of
// compileMethodFromDetails as a function pointer.
#if !defined(AIXPPC)
TEST_F(IlGenTest, Return3) {
    auto trees = parseString("(block (ireturn (iconst 3)))");

    TR::TypeDictionary types;
    auto Int32 = types.PrimitiveType(TR::Int32);
    TR::IlType* argTypes[] = { Int32 };

    Tril::GenericNodeConverter genericNodeConverter;
    Tril::CallConverter callConverter(&genericNodeConverter);

    Tril::TRLangBuilder injector(trees, &types, &callConverter);
    TR::ResolvedMethod compilee(__FILE__, LINETOSTR(__LINE__), "Return3InIL", sizeof(argTypes)/sizeof(TR::IlType*), argTypes, Int32, 0, &injector);
    TR::IlGeneratorMethodDetails methodDetails(&compilee);
    int32_t rc = 0;
    auto entry_point = compileMethodFromDetails(NULL, methodDetails, warm, rc);

    ASSERT_EQ(0, rc) << "Compilation failed";
    ASSERT_NOTNULL(entry_point) << "Entry point of compiled body cannot be null";

    auto entry = reinterpret_cast<int32_t(*)(void)>(entry_point);
    ASSERT_EQ(3, entry()) << "Compiled body did not return expected value";
}
#endif
