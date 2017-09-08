/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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


#ifndef OPERANDSTACKTESTS_INCL
#define OPERANDSTACKTESTS_INCL

#include "ilgen/MethodBuilder.hpp"

#define STACKVALUEILTYPE	Int32
#define	STACKVALUETYPE		int32_t

//#define STACKVALUEILTYPE	Int64
//#define STACKVALUETYPE	int64_t

namespace TR { class BytecodeBuilder; }

class OperandStackTestMethod : public TR::MethodBuilder
   {
   public:
   OperandStackTestMethod(TR::TypeDictionary *);
   virtual bool buildIL();

   static void verifyStack(const char *step, int32_t max, int32_t num, ...);
   static bool verifyUntouched(int32_t maxTouched);

   STACKVALUETYPE **getSPPtr() { return &_realStackTop; }

   protected:
   bool testStack(TR::BytecodeBuilder *b, bool useEqual);

   TR::IlType                      * _valueType;

   static STACKVALUETYPE           * _realStack;
   static STACKVALUETYPE           * _realStackTop;
   static int32_t                    _realStackSize;

   static void createStack();
   static STACKVALUETYPE *moveStack();
   static void freeStack();
   };

class OperandStackTestUsingStructMethod : public OperandStackTestMethod
   {
   public:
   OperandStackTestUsingStructMethod(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(OPERANDSTACKTESTS_INCL)
