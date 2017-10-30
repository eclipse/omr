/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "codegen/X86FastCallLinkage.hpp"

#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/RealRegisterManager.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "x/codegen/X86Instruction.hpp"

#ifdef TR_TARGET_64BIT
#include "x/amd64/codegen/AMD64FastCallConfig.hpp"
#else
#include "x/i386/codegen/IA32FastCallConfig.hpp"
#endif

const size_t TR::X86FastCallCallSite::NumberOfIntParamRegisters   = sizeof(IntParamRegisters)   / sizeof(IntParamRegisters[0]);
const size_t TR::X86FastCallCallSite::NumberOfFloatParamRegisters = sizeof(FloatParamRegisters) / sizeof(FloatParamRegisters[0]);
const size_t TR::X86FastCallCallSite::StackIndexAdjustment        = RegisterParameterShadowOnStack ? NumberOfIntParamRegisters : 0;

// Calculate preserved register map.
// The map is a constant that the C++ compiler *should* be able to determine at compile time
static uint32_t X86FastCallCallSiteCalculatePreservedRegisterMapForGC()
   {
   uint32_t ret = 0;
   for (size_t i = 0;
        i < sizeof(TR::X86FastCallCallSite::CalleeSavedRegisters) / sizeof(TR::X86FastCallCallSite::CalleeSavedRegisters[0]);
        i++)
      {
      ret |= TR::RealRegister::gprMask(TR::X86FastCallCallSite::CalleeSavedRegisters[i]);
      }
   return ret;
   }
const uint32_t TR::X86FastCallCallSite::PreservedRegisterMapForGC = X86FastCallCallSiteCalculatePreservedRegisterMapForGC();

TR::Register* TR::X86FastCallCallSite::BuildCall()
   {
   TR_ASSERT(CalleeCleanup || cg()->getLinkage()->getProperties().getReservesOutgoingArgsInPrologue(),
             "Caller must reserve outgoing args in prologue unless callee cleans up stack");

   if (cg()->comp()->getOption(TR_TraceCG))
      {
      traceMsg(cg()->comp(), "X86 FastCall: [%04d] %s\n", _SymRef->getReferenceNumber(), cg()->getDebug()->getName(_SymRef));
      }
   RealRegisterManager RealRegisters(cg());
   TR::RealRegister*   ESP = cg()->machine()->getX86RealRegister(TR::RealRegister::esp);

   // Preserve caller saved registers
   for (size_t i = 0;
        i < sizeof(CallerSavedRegisters) / sizeof(CallerSavedRegisters[0]);
        i++)
      {
      RealRegisters.Use(CallerSavedRegisters[i]);
      }
   // Find the return register, EAX/RAX/XMM0
   TR::Register* EAX = RealRegisters.Use(TR::RealRegister::eax);
   TR::Register* XMM0 = RealRegisters.Use(TR::RealRegister::xmm0);

   // Build parameters, parameters in _Params are Right-to-Left (RTL)
   TR::Register* IntParams[NumberOfIntParamRegisters] = {};
   TR::Register* FloatParams[NumberOfFloatParamRegisters] = {};
   TR_Stack<TR::Register*> StackParams(cg()->trMemory());

   size_t IntParamIndex = 0;
#ifndef WINDOWS
   size_t FloatParamIndex = 0;
#else
   auto& FloatParamIndex = IntParamIndex;
#endif

   for (size_t i = _Params.size(); i > 0; i--)
      {
      auto param = _Params[i-1];
      switch(param->getKind())
         {
         case TR_GPR:
            if (IntParamIndex < NumberOfIntParamRegisters)
               {
               IntParams[IntParamIndex++] = param;
               }
            else
               {
               StackParams.push(param);
               }
            break;
         case TR_FPR:
            if (FloatParamIndex < NumberOfFloatParamRegisters)
               {
               FloatParams[FloatParamIndex++] = param;
               }
            else
               {
               StackParams.push(param);
               if(TR::Compiler->target.is32Bit() && !param->isSinglePrecision())
                  {
                  StackParams.push(NULL);
                  }
               }
            break;
         default:
            TR_ASSERT(false, "Unsupported call param data type: #%d", (int)param->getKind());
         }
      }

   // Reserve stack for parameters
   auto SizeOfParamsOnStack = (StackParams.size() + StackIndexAdjustment)*StackSlotSize;
   if (CalleeCleanup && SizeOfParamsOnStack > 0)
      {
      generateRegImmInstruction(SUBRegImms(), _Node, ESP, SizeOfParamsOnStack, cg());
      }
   // Parameters passed by stack
   while(!StackParams.isEmpty())
      {
      auto param = StackParams.pop();
      if(param)
         {
         auto opcode = param->getKind() == TR_GPR ? SMemReg() :
                       param->isSinglePrecision() ? MOVSSMemReg : MOVSDMemReg;
         generateMemRegInstruction(opcode,
                                   _Node,
                                   generateX86MemoryReference(ESP, StackParams.size()*StackSlotSize, cg()),
                                   param,
                                   cg());
         }
      }
   // Parameters passed by FPR
#if defined(WINDOWS) && defined(TR_TARGET_32BIT)
   TR_ASSERT(NumberOfFloatParamRegisters == 0, "FastCallLinkage with float parameters is not supported on 32-bit Windows!");
#endif
   for (size_t i = NumberOfFloatParamRegisters; i > 0; i--)
      {
      auto param = FloatParams[i-1];
      if(param)
         {
         generateRegRegInstruction(MOVDQURegReg,
                                   _Node,
                                   RealRegisters.Use(FloatParamRegisters[i-1]),
                                   param,
                                   cg());
         }
      }
   // Parameters passed by GPR
   for (size_t i = NumberOfIntParamRegisters; i > 0; i--)
      {
      auto param = IntParams[i-1];
      if(param)
         {
         generateRegRegInstruction(MOVRegReg(),
                                   _Node,
                                   RealRegisters.Use(IntParamRegisters[i-1]),
                                   param,
                                   cg());
         }
      }

   // Call helper
   TR::X86ImmInstruction* instr = generateImmSymInstruction(CALLImm4,
                                                            _Node,
                                                            (uintptrj_t)_SymRef->getMethodAddress(),
                                                            _SymRef,
                                                            RealRegisters.BuildRegisterDependencyConditions(),
                                                            cg());
   instr->setNeedsGCMap(PreservedRegisterMapForGC);

   // Stack adjustment
   if (CalleeCleanup)
      {
      instr->setAdjustsFramePointerBy(-SizeOfParamsOnStack);
      }
   else
      {
      if (SizeOfParamsOnStack > cg()->getLargestOutgoingArgSize())
         {
         cg()->setLargestOutgoingArgSize(SizeOfParamsOnStack);
         }
      }

   // Return value
   TR::Register* ret = NULL;
   switch (_ReturnType)
      {
      case TR::NoType:
         break;
      case TR::Address:
         ret = cg()->allocateCollectedReferenceRegister();
         generateRegRegInstruction(MOVRegReg(), _Node, ret, EAX, cg());
         break;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
#ifdef TR_TARGET_64BIT
      case TR::Int64:
#endif
         ret = cg()->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), _Node, ret, EAX, cg());
         break;
      case TR::Double:
         ret = cg()->allocateRegister(TR_FPR);
         generateRegRegInstruction(MOVDQURegReg, _Node, ret, XMM0, cg());
         break;
      default:
         TR_ASSERT(false, "Unsupported call return data type: #%d", (int)_ReturnType);
         break;
      }
   return ret;
}
