/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#ifndef OPERANDARRAYTESTS_INCL
#define OPERANDARRAYTESTS_INCL

#include "ilgen/MethodBuilder.hpp"

#define ARRAYVALUEILTYPE Int32
#define ARRAYVALUETYPE   int32_t

namespace TR { class BytecodeBuilder; }

class OperandArrayTestMethod : public TR::MethodBuilder
   {
   public:
   OperandArrayTestMethod(TR::TypeDictionary *);
   virtual bool buildIL();

   static void verify(const char *step, int32_t max, int32_t num, ...);
   static bool verifyUntouched(int32_t maxTouched);

   ARRAYVALUETYPE **getArrayPtr() { return &_realArray; }

   protected:
   bool testArray(TR::BytecodeBuilder *b, bool useEqual);

   TR::IlType                      *_valueType;

   static ARRAYVALUETYPE           *_realArray;
   static int32_t                   _realArrayLength;
   
   static void createArray();
   static ARRAYVALUETYPE *moveArray();
   static void freeArray();
   };
   
class OperandArrayTestUsingFalseMethod : public OperandArrayTestMethod
   {
   public:
   OperandArrayTestUsingFalseMethod(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(OPERANDARRAYTESTS_INCL)
