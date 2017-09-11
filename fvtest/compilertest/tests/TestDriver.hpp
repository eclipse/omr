/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef TESTDRIVER_HPP
#define TESTDRIVER_HPP

#include <stdint.h>
#include "compile/CompilationTypes.hpp"

namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class MethodBuilder; }

extern "C" uint8_t *compileMethod(TR::IlGeneratorMethodDetails &, TR_Hotness, int32_t &);

namespace TestCompiler
{
#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

class TestDriver
   {
   public:
   void RunTest();
   virtual void compileTestMethods() = 0;
   virtual void invokeTests() = 0;

   int32_t compileMethodBuilder(TR::MethodBuilder *m, uint8_t **entry);
   };

}// namespace TestCompiler

#endif
