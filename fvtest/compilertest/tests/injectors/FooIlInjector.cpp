/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "compile/Compilation.hpp"
#include "env/FrontEnd.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "tests/FooBarTest.hpp"
#include "tests/injectors/FooIlInjector.hpp"

namespace TestCompiler
{

bool
FooIlInjector::injectIL()
   {
   FooBarTest *test = static_cast<FooBarTest *>(_test);
   createBlocks(4);

   // 4 blocks requested start at 2 (0 is entry, 1 is exit)
   // by default, generate to block 2

   // Block 2: blocks(0)
   // int32_t newIndex = _bar(index);
   // if (newIndex < 0) goto Block5;
   TR::SymbolReference *newIndexSymRef = newTemp(Int32);
   TR::ResolvedMethod *barMethod = test->barMethod();
   storeToTemp(newIndexSymRef, callFunction(barMethod, Int32, 1, indexParameter()));
   ifjump(TR::ificmplt, loadTemp(newIndexSymRef), iconst(0), 3);

   // Block 3: blocks(1)
   // if (newIndex >= 100) goto Block5;
   ifjump(TR::ificmpge, loadTemp(newIndexSymRef), iconst(test->dataArraySize()), 3);

   // Block 4: blocks(2)
   // return dataArray[newIndex];
   returnValue(arrayLoad(staticAddress(test->dataArray()), loadTemp(newIndexSymRef), Int32));

   // Block 5: blocks(3)
   // return -1;
   generateToBlock(3);
   returnValue(iconst(-1));

   return true;
   }

} // namespace TestCompiler
