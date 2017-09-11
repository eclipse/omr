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

#ifndef X86_EVALUATOR_INCL
#define X86_EVALUATOR_INCL



#include <stddef.h>                         // for NULL
#include <stdint.h>                         // for uint8_t
#include "x/codegen/X86Ops.hpp"             // for TR_X86OpCodes
#include "codegen/TreeEvaluator.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }


extern void constLengthArrayCopy(
   TR::Node *node,
   TR::CodeGenerator *cg,
   TR::Register *byteSrcReg,
   TR::Register *byteDstReg,
   TR::Node *byteLenNode,
   bool preserveSrcPointer,
   bool preserveDstPointer);

extern void genCodeToPerformLeftToRightAndBlockConcurrentOpIfNeeded(
   TR::Node *node,
   TR::MemoryReference *memRef,
   TR::Register *valueReg,
   TR::Register *tempReg,
   TR::Register *tempReg1,
   TR::Register *tempReg2,
   TR::LabelSymbol *nonLockedOpLabel,
   TR::LabelSymbol *&opDoneLabel,
   TR::RegisterDependencyConditions *&deps,
   uint8_t size,
   TR::CodeGenerator *cg,
   bool isLoad,
   bool genOutOfline,
   bool keepValueRegAlive = false,
   TR::LabelSymbol *startControlFlowLabel = NULL);



class TR_X86ComputeCC : public TR::TreeEvaluator
   {
   public:

   static void bitwise32(TR::Node *node, TR::Register *ccReg, TR::Register *target, TR::CodeGenerator *cg);
   static bool setCarryBorrow(TR::Node *flagNode, bool invertValue, TR::CodeGenerator *cg);

   };

#endif
