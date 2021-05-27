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

#ifndef AOTINFRATEST_INCL
#define AOTINFRATEST_INCL

#include "TestDriver.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"

namespace TestCompiler
{

class AOTinfraTest;
typedef int32_t (RecursiveFibFunctionType)(int32_t);


class AOTinfraTest : public TestDriver
   {

   public:
   bool *traceEnabledLocation()    { return &_traceEnabled; }
   /**
    * Checks whether getting the variables, the constructors, and  
    * the serialization method works correctly for AOTMethodHeader.
    */
   void AOTMethodHeaderTest();
   /**
    * Checks whether storing and loading works correctly for AOTStorageInterface.
    */
   void AOTStorageTest();
   /**
    * Checks whether creating an AOTMethodHeader from Compiler in the context of creating a new method works correctly using AOTAdapter.
    */
   void AOTAdapterTest();

   protected:
   virtual void compileTestMethods();
   virtual void invokeTests() {}

   private:
   int32_t recursiveFib(int32_t n);
   static RecursiveFibFunctionType *_recursiveFibMethod;
   static bool _traceEnabled;
   };

class RecursiveFibMethod : public TR::MethodBuilder
   {
   public:
   RecursiveFibMethod(TR::TypeDictionary *types, AOTinfraTest *test);
   virtual bool buildIL();
   };

} // namespace TestCompiler

#endif // !defined(AOTINFRATEST_INCL)
