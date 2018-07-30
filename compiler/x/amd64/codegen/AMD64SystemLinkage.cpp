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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "x/amd64/codegen/AMD64SystemLinkage.hpp"

#include <stdio.h>                                       // for printf
#include <string.h>                                      // for memset, etc
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"                       // for Instruction
#include "codegen/Machine.hpp"                           // for Machine, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "compile/Compilation.hpp"                       // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"                                // for uintptrj_t
#include "il/ILOps.hpp"                                  // for ILOpCode
#include "il/Node.hpp"                                   // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                                 // for Symbol
#include "il/SymbolReference.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                              // for TR_ASSERT
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                          // for ::CALLReg, etc
#include "x/codegen/X86SystemLinkage.hpp"

////////////////////////////////////////////////
//
// Helpful definitions
//
// These are only here to make the rest of the code below somewhat
// self-documenting.
//
enum
   {
   RETURN_ADDRESS_SIZE=8,
   GPR_REG_WIDTH=8,
   AMD64_STACK_SLOT_SIZE=8,
   AMD64_DEFAULT_STACK_ALIGNMENT=16,
   AMD64_ABI_NUM_INTEGER_LINKAGE_REGS=6,
   AMD64_ABI_NUM_FLOAT_LINKAGE_REGS=8,
   WIN64_FAST_ABI_NUM_INTEGER_LINKAGE_REGS=4,
   WIN64_FAST_ABI_NUM_FLOAT_LINKAGE_REGS=4
   };


TR::AMD64Win64FastCallLinkage::AMD64Win64FastCallLinkage(TR::CodeGenerator *cg)
   : TR::AMD64SystemLinkage(cg)
   {
   uint8_t r, p;

   // For SysV/Win64 ABI, the outgoing argument regions need to be 16/32 bytes
   // aligned, we will reserve outgoing arguments size in prologue to get the
   // alignment properly handled, that is the reason why
   // ReservesOutgoingArgsInPrologue is needed.
   //
   _properties._properties =
      EightBytePointers   |
      EightByteParmSlots  |
      CallerCleanup       |
      IntegersInRegisters |
      LongsInRegisters    |
      FloatsInRegisters   |
      LinkageRegistersAssignedByCardinalPosition |
      CallerFrameAllocatesSpaceForLinkageRegs |
      ReservesOutgoingArgsInPrologue
      ;

   // Integer arguments
   //
   p=0;
   _properties._firstIntegerArgumentRegister = p;
   _properties._argumentRegisters[p++] = TR::RealRegister::ecx;
   _properties._argumentRegisters[p++] = TR::RealRegister::edx;
   _properties._argumentRegisters[p++] = TR::RealRegister::r8;
   _properties._argumentRegisters[p++] = TR::RealRegister::r9;
   _properties._numIntegerArgumentRegisters = p;

   // Float arguments
   //
   _properties._firstFloatArgumentRegister = p;
   for(r=0; r<=3; r++)
      _properties._argumentRegisters[p++] = TR::RealRegister::xmmIndex(r);
   _properties._numFloatArgumentRegisters = p - _properties._numIntegerArgumentRegisters;

   // Preserved registers.
   //
   p = 0;
   _properties._preservedRegisters[p++] = TR::RealRegister::edi;
   _properties._preservedRegisters[p++] = TR::RealRegister::esi;
   _properties._preservedRegisters[p++] = TR::RealRegister::ebx;
   _properties._preservedRegisters[p++] = TR::RealRegister::r12;
   _properties._preservedRegisters[p++] = TR::RealRegister::r13;
   _properties._preservedRegisters[p++] = TR::RealRegister::r14;
   _properties._preservedRegisters[p++] = TR::RealRegister::r15;

   _properties._numberOfPreservedGPRegisters = p;

   for (r=6; r<=15; r++)
      _properties._preservedRegisters[p++] = TR::RealRegister::xmmIndex(r);

   _properties._numberOfPreservedXMMRegisters = p - _properties._numberOfPreservedGPRegisters;

   _properties._maxRegistersPreservedInPrologue = p;
   _properties._numPreservedRegisters = p;

   // Volatile registers.
   //
   p = 0;
   _properties._volatileRegisters[p++] = TR::RealRegister::eax;
   _properties._volatileRegisters[p++] = TR::RealRegister::ecx;
   _properties._volatileRegisters[p++] = TR::RealRegister::edx;
   _properties._volatileRegisters[p++] = TR::RealRegister::r8;
   _properties._volatileRegisters[p++] = TR::RealRegister::r9;
   _properties._volatileRegisters[p++] = TR::RealRegister::r10;
   _properties._volatileRegisters[p++] = TR::RealRegister::r11;
   _properties._numberOfVolatileGPRegisters = p;

   for(r=0; r<=5; r++)
      _properties._volatileRegisters[p++] = TR::RealRegister::xmmIndex(r);
   _properties._numberOfVolatileXMMRegisters = p - _properties._numberOfVolatileGPRegisters;
   _properties._numVolatileRegisters = p;

   // Return registers.
   //
   _properties._returnRegisters[0] = TR::RealRegister::eax;
   _properties._returnRegisters[1] = TR::RealRegister::xmm0;
   _properties._returnRegisters[2] = TR::RealRegister::NoReg;

   // Scratch registers.
   //
   _properties._scratchRegisters[0] = TR::RealRegister::r10;
   _properties._scratchRegisters[1] = TR::RealRegister::r11;
   _properties._scratchRegisters[2] = TR::RealRegister::eax;
   _properties._numScratchRegisters = 3;

   _properties._preservedRegisterMapForGC = 0;

   _properties._framePointerRegister = TR::RealRegister::ebp;
   _properties._methodMetaDataRegister = TR::RealRegister::NoReg;
   _properties._offsetToFirstParm = RETURN_ADDRESS_SIZE;
   _properties._offsetToFirstLocal = _properties.getAlwaysDedicateFramePointerRegister() ? -GPR_REG_WIDTH : 0;

   memset(_properties._registerFlags, 0, sizeof(_properties._registerFlags));

   // Integer arguments/return
   //
   _properties._registerFlags[TR::RealRegister::ecx] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::edx] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::r8] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::r9] = IntegerArgument;

   _properties._registerFlags[TR::RealRegister::eax] = IntegerReturn;

   // Float arguments/return
   //
   _properties._registerFlags[TR::RealRegister::xmm0] = FloatArgument | FloatReturn;
   for (r=1; r <= 3; r++)
      _properties._registerFlags[TR::RealRegister::xmmIndex(r)] = FloatArgument;

   // Preserved
   //
   _properties._registerFlags[TR::RealRegister::edi] = Preserved;
   _properties._registerFlags[TR::RealRegister::esi] = Preserved;
   _properties._registerFlags[TR::RealRegister::ebx] = Preserved;
   _properties._registerFlags[TR::RealRegister::ebp] = Preserved;
   _properties._registerFlags[TR::RealRegister::esp] = Preserved;
   for (r=12; r <= 15; r++)
      _properties._registerFlags[TR::RealRegister::rIndex(r)] = Preserved;

   p = 0;

   // Volatiles that aren't linkage regs
   //
   if (OMR::X86::AMD64::Machine::enableNewPickRegister())
      {
      if (OMR::X86::AMD64::Machine::numGPRRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::eax;
         _properties._allocationOrder[p++] = TR::RealRegister::r10;
         }
      else
         TR_ASSERT(OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
      }
   _properties._allocationOrder[p++] = TR::RealRegister::r11;

   // Linkage regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::ecx;
   _properties._allocationOrder[p++] = TR::RealRegister::edx;
   _properties._allocationOrder[p++] = TR::RealRegister::r8;
   _properties._allocationOrder[p++] = TR::RealRegister::r9;

   // Preserved regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::edi;
   _properties._allocationOrder[p++] = TR::RealRegister::esi;
   _properties._allocationOrder[p++] = TR::RealRegister::ebx;
   _properties._allocationOrder[p++] = TR::RealRegister::r12;
   _properties._allocationOrder[p++] = TR::RealRegister::r13;
   _properties._allocationOrder[p++] = TR::RealRegister::r14;
   _properties._allocationOrder[p++] = TR::RealRegister::r15;

   TR_ASSERT(p == machine()->getNumGlobalGPRs(), "assertion failure");

   // Linkage FP regs
   //
   if (OMR::X86::AMD64::Machine::enableNewPickRegister())
      {
      if (OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::xmm0;
         _properties._allocationOrder[p++] = TR::RealRegister::xmm1;
         }
      else
         TR_ASSERT(OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
      }
   _properties._allocationOrder[p++] = TR::RealRegister::xmm2;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm3;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm4;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm5;

   // Other volatile FP regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::xmm6;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm7;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm8;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm9;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm10;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm11;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm12;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm13;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm14;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm15;

   _properties._OutgoingArgAlignment = AMD64_DEFAULT_STACK_ALIGNMENT;

   TR_ASSERT(p == (machine()->getNumGlobalGPRs() + machine()->_numGlobalFPRs), "assertion failure");
   }


TR::AMD64ABILinkage::AMD64ABILinkage(TR::CodeGenerator *cg)
   : TR::AMD64SystemLinkage(cg)
   {
   uint8_t r, p;

   // For SysV/Win64 ABI, the outgoing argument regions need to be 16/32 bytes
   // aligned, we will reserve outgoing arguments size in prologue to get the
   // alignment properly handled, that is the reason why
   // ReservesOutgoingArgsInPrologue is needed.
   //
   _properties._properties =
      EightBytePointers   |
      EightByteParmSlots  |
      CallerCleanup       |
      IntegersInRegisters |
      LongsInRegisters    |
      FloatsInRegisters   |
      ReservesOutgoingArgsInPrologue;

   if (!cg->comp()->getOption(TR_OmitFramePointer))
      _properties._properties |= AlwaysDedicateFramePointerRegister;

   // Integer arguments
   //
   p=0;
   _properties._firstIntegerArgumentRegister = p;
   _properties._argumentRegisters[p++] = TR::RealRegister::edi;
   _properties._argumentRegisters[p++] = TR::RealRegister::esi;
   _properties._argumentRegisters[p++] = TR::RealRegister::edx;
   _properties._argumentRegisters[p++] = TR::RealRegister::ecx;
   _properties._argumentRegisters[p++] = TR::RealRegister::r8;
   _properties._argumentRegisters[p++] = TR::RealRegister::r9;
   _properties._numIntegerArgumentRegisters = p;

   // Float arguments
   //
   _properties._firstFloatArgumentRegister = p;
   for(r=0; r<=7; r++)
      _properties._argumentRegisters[p++] = TR::RealRegister::xmmIndex(r);
   _properties._numFloatArgumentRegisters = p - _properties._numIntegerArgumentRegisters;

   // Preserved registers.
   //
   p = 0;
   _properties._preservedRegisters[p++] = TR::RealRegister::ebx;
   _properties._preservedRegisterMapForGC = TR::RealRegister::ebxMask;
   for (r=12; r<=15; r++)
      {
      _properties._preservedRegisters[p++] = TR::RealRegister::rIndex(r);
      _properties._preservedRegisterMapForGC |= TR::RealRegister::gprMask(TR::RealRegister::rIndex(r));
      }

   _properties._numberOfPreservedGPRegisters = p;
   _properties._numberOfPreservedXMMRegisters = 0;

   _properties._maxRegistersPreservedInPrologue = p;
   _properties._numPreservedRegisters = p;

   // Volatile registers.
   //
   p = 0;
   _properties._volatileRegisters[p++] = TR::RealRegister::eax;
   _properties._volatileRegisters[p++] = TR::RealRegister::ecx;
   _properties._volatileRegisters[p++] = TR::RealRegister::edx;
   _properties._volatileRegisters[p++] = TR::RealRegister::esi;
   _properties._volatileRegisters[p++] = TR::RealRegister::edi;
   _properties._volatileRegisters[p++] = TR::RealRegister::r8;
   _properties._volatileRegisters[p++] = TR::RealRegister::r9;
   _properties._volatileRegisters[p++] = TR::RealRegister::r10;
   _properties._volatileRegisters[p++] = TR::RealRegister::r11;
   _properties._numberOfVolatileGPRegisters = p;

   for (r=0; r<=15; r++)
      _properties._volatileRegisters[p++] = TR::RealRegister::xmmIndex(r);

   _properties._numberOfVolatileXMMRegisters = p - _properties._numberOfVolatileGPRegisters;
   _properties._numVolatileRegisters = p;

   // Return registers.
   //
   _properties._returnRegisters[0] = TR::RealRegister::eax;
   _properties._returnRegisters[1] = TR::RealRegister::xmm0;
   _properties._returnRegisters[2] = TR::RealRegister::NoReg;

   // Scratch registers.
   //
   _properties._scratchRegisters[0] = TR::RealRegister::r10;
   _properties._scratchRegisters[1] = TR::RealRegister::r11;
   _properties._scratchRegisters[2] = TR::RealRegister::eax;
   _properties._numScratchRegisters = 3;

   _properties._framePointerRegister = TR::RealRegister::ebp;
   _properties._methodMetaDataRegister = TR::RealRegister::NoReg;
   _properties._offsetToFirstParm = RETURN_ADDRESS_SIZE;
   _properties._offsetToFirstLocal = _properties.getAlwaysDedicateFramePointerRegister() ? -GPR_REG_WIDTH : 0;

   memset(_properties._registerFlags, 0, sizeof(_properties._registerFlags));

   // Integer arguments/return
   //
   _properties._registerFlags[TR::RealRegister::edi] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::esi] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::edx] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::ecx] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::r8] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::r9] = IntegerArgument;

   _properties._registerFlags[TR::RealRegister::eax] = IntegerReturn;

   // Float arguments/return
   //
   _properties._registerFlags[TR::RealRegister::xmm0] = FloatArgument | FloatReturn;
   for (r=1; r <= 7; r++)
      _properties._registerFlags[TR::RealRegister::xmmIndex(r)] = FloatArgument;

   // Preserved
   //
   _properties._registerFlags[TR::RealRegister::ebx] = Preserved;
   _properties._registerFlags[TR::RealRegister::ebp] = Preserved;
   _properties._registerFlags[TR::RealRegister::esp] = Preserved;
   for (r=12; r <= 15; r++)
      _properties._registerFlags[TR::RealRegister::rIndex(r)] = Preserved;

   p = 0;

   // Volatiles that aren't linkage regs
   //
   if (OMR::X86::AMD64::Machine::enableNewPickRegister())
      {
      if (OMR::X86::AMD64::Machine::numGPRRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::eax;
         _properties._allocationOrder[p++] = TR::RealRegister::r10;
         }
      else
         TR_ASSERT(OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
      }
   _properties._allocationOrder[p++] = TR::RealRegister::r11;

   // Linkage regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::edi;
   _properties._allocationOrder[p++] = TR::RealRegister::esi;
   _properties._allocationOrder[p++] = TR::RealRegister::edx;
   _properties._allocationOrder[p++] = TR::RealRegister::ecx;
   _properties._allocationOrder[p++] = TR::RealRegister::r8;
   _properties._allocationOrder[p++] = TR::RealRegister::r9;

   // Preserved regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::ebx;
   _properties._allocationOrder[p++] = TR::RealRegister::r12;
   _properties._allocationOrder[p++] = TR::RealRegister::r13;
   _properties._allocationOrder[p++] = TR::RealRegister::r14;
   _properties._allocationOrder[p++] = TR::RealRegister::r15;

   TR_ASSERT(p == machine()->getNumGlobalGPRs(), "assertion failure");

   // Linkage FP regs
   //
   if (OMR::X86::AMD64::Machine::enableNewPickRegister())
      {
      if (OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 0)
         {
         _properties._allocationOrder[p++] = TR::RealRegister::xmm0;
         _properties._allocationOrder[p++] = TR::RealRegister::xmm1;
         }
      else
         TR_ASSERT(OMR::X86::AMD64::Machine::numRegsWithheld(cg) == 2, "numRegsWithheld: only 0 and 2 currently supported");
      }
   _properties._allocationOrder[p++] = TR::RealRegister::xmm2;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm3;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm4;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm5;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm6;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm7;

   // Other volatile FP regs
   //
   _properties._allocationOrder[p++] = TR::RealRegister::xmm8;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm9;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm10;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm11;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm12;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm13;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm14;
   _properties._allocationOrder[p++] = TR::RealRegister::xmm15;

   _properties.setOutgoingArgAlignment(AMD64_DEFAULT_STACK_ALIGNMENT);

   TR_ASSERT(p == (machine()->getNumGlobalGPRs() + machine()->_numGlobalFPRs), "assertion failure");
   }


TR::Register *
TR::AMD64SystemLinkage::buildVolatileAndReturnDependencies(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *deps)
   {

   if (callNode->getOpCode().isIndirect())
      {
      TR::Node *vftChild = callNode->getFirstChild();
      if (vftChild->getRegister() && (vftChild->getReferenceCount() > 1))
         {
         }
      else
         {
         // VFT child dies here; decrement it early so it doesn't interfere with dummy regs.
         cg()->recursivelyDecReferenceCount(vftChild);
         }
      }

   TR_ASSERT(deps != NULL, "expected register dependencies");

   // Figure out which is the return register.
   //
   TR::RealRegister::RegNum returnRegIndex;
   TR_RegisterKinds returnKind;

   switch (callNode->getDataType())
      {
      case TR::NoType:
         returnRegIndex = TR::RealRegister::NoReg;
         returnKind = TR_NoRegister;
         break;

      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
      case TR::Address:
         returnRegIndex = getProperties().getIntegerReturnRegister();
         returnKind = TR_GPR;
         break;

      case TR::Float:
      case TR::Double:
         returnRegIndex = getProperties().getFloatReturnRegister();
         returnKind = TR_FPR;
         break;

      case TR::Aggregate:
      default:
         TR_ASSERT(false, "Unrecognized call node data type: #%d", (int)callNode->getDataType());
         break;
      }

   // Kill all non-preserved int and float regs besides the return register.
   //
   int32_t i;
   TR::RealRegister::RegNum scratchIndex = getProperties().getIntegerScratchRegister(1);
   for (i=0; i<getProperties().getNumVolatileRegisters(); i++)
      {
      TR::RealRegister::RegNum regIndex = getProperties()._volatileRegisters[i];

      if (regIndex != returnRegIndex)
         {
         TR_RegisterKinds rk = (i < getProperties()._numberOfVolatileGPRegisters) ? TR_GPR : TR_FPR;
         TR::Register *dummy = cg()->allocateRegister(rk);
         deps->addPostCondition(dummy, regIndex, cg());

         // Note that we don't setPlaceholderReg here.  If this volatile reg is also volatile
         // in the caller's linkage, then that flag doesn't matter much anyway.  If it's preserved
         // in the caller's linkage, then we don't want to set that flag because we want this
         // use of the register to count as a "real" use, thereby motivating the prologue to
         // preserve the register.

         // A scratch register is necessary to call the native without a trampoline.
         //
         if (callNode->getOpCode().isIndirect() || (regIndex != scratchIndex))
            cg()->stopUsingRegister(dummy);
         }
      }

#if defined (PYTHON) && 0
   // Evict the preserved registers across the call
   //
   for (i=0; i<getProperties().getNumberOfPreservedGPRegisters(); i++)
      {
      TR::RealRegister::RegNum regIndex = getProperties()._preservedRegisters[i];

      TR::Register *dummy = cg()->allocateRegister(TR_GPR);
      deps->addPostCondition(dummy, regIndex, cg());

      // Note that we don't setPlaceholderReg here.  If this volatile reg is also volatile
      // in the caller's linkage, then that flag doesn't matter much anyway.  If it's preserved
      // in the caller's linkage, then we don't want to set that flag because we want this
      // use of the register to count as a "real" use, thereby motivating the prologue to
      // preserve the register.

      // A scratch register is necessary to call the native without a trampoline.
      //
      if (callNode->getOpCode().isIndirect() || (regIndex != scratchIndex))
         cg()->stopUsingRegister(dummy);
      }
#endif

   if (callNode->getOpCode().isIndirect())
      {
      TR::Node *vftChild = callNode->getFirstChild();
      if (vftChild->getRegister() && (vftChild->getReferenceCount() > 1))
         {
         // VFT child survives the call, so we must include it in the postconditions.
         deps->addPostCondition(vftChild->getRegister(), TR::RealRegister::NoReg, cg());
         cg()->recursivelyDecReferenceCount(vftChild);
         }
      }

   // Now that everything is dead, we can allocate the return register without
   // interference
   //
   TR::Register *returnRegister;
   if (returnRegIndex)
      {
      TR_ASSERT(returnKind != TR_NoRegister, "assertion failure");

      if (callNode->getDataType() == TR::Address)
         returnRegister = cg()->allocateCollectedReferenceRegister();
      else
         {
         returnRegister = cg()->allocateRegister(returnKind);
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         }

      deps->addPostCondition(returnRegister, returnRegIndex, cg());
      }
   else
      returnRegister = NULL;


 // The reg dependency is left open intentionally, and need to be closed by
 // the caller. The reason is because, child class might call this method, while
 // adding more register dependecies;  if we close the reg dependency here,
 // the child class won't be able to add more register dependencies.

   return returnRegister;
   }


// Build arguments for system linkage dispatch.
//
int32_t TR::AMD64SystemLinkage::buildArgs(
      TR::Node *callNode,
      TR::RegisterDependencyConditions *deps)
   {
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::RealRegister::RegNum noReg = TR::RealRegister::NoReg;
   TR::RealRegister *espReal = machine()->getX86RealRegister(TR::RealRegister::esp);
   int32_t firstNodeArgument = callNode->getFirstArgumentIndex();
   int32_t lastNodeArgument = callNode->getNumChildren() - 1;
   int32_t offset = 0;
   int32_t sizeOfOutGoingArgs= 0;
   uint16_t numIntArgs = 0,
            numFloatArgs = 0;
   int32_t first, last, direction;
   int32_t numCopiedRegs = 0;
   TR::Register *copiedRegs[TR::X86LinkageProperties::MaxArgumentRegisters];

   if (getProperties().passArgsRightToLeft())
      {
      first = lastNodeArgument;
      last  = firstNodeArgument - 1;
      direction = -1;
      }
   else
      {
      first = firstNodeArgument;
      last  = lastNodeArgument + 1;
      direction = 1;
      }

   // If the dispatch is indirect we must add the VFT register to the preconditions
   // so that it gets register assigned with the other preconditions to the call.
   //
   if (callNode->getOpCode().isIndirect())
      {
      TR::Node *vftChild = callNode->getFirstChild();
      TR_ASSERT(vftChild->getRegister(), "expecting VFT child to be evaluated");
      TR::RealRegister::RegNum scratchRegIndex = getProperties().getIntegerScratchRegister(1);
      deps->addPreCondition(vftChild->getRegister(), scratchRegIndex, cg());
      }

   int32_t i;
   for (i = first; i != last; i += direction)
      {
      TR::parmLayoutResult layoutResult;
      TR::RealRegister::RegNum rregIndex = noReg;
      TR::Node *child = callNode->getChild(i);

      layoutParm(child, sizeOfOutGoingArgs, numIntArgs, numFloatArgs, layoutResult);

      if (layoutResult.abstract & TR::parmLayoutResult::IN_LINKAGE_REG_PAIR)
         {
         // TODO: AMD64 SysV ABI might put a struct into a pair of linkage registerr
         TR_ASSERT(false, "haven't support linkage_reg_pair yet.\n");
         }
      else if (layoutResult.abstract & TR::parmLayoutResult::IN_LINKAGE_REG)
         {
         TR_RegisterKinds regKind = layoutResult.regs[0].regKind;
         uint32_t regIndex = layoutResult.regs[0].regIndex;
         TR_ASSERT(regKind == TR_GPR || regKind == TR_FPR, "linkage registers includes TR_GPR and TR_FPR\n");
         rregIndex = (regKind == TR_FPR) ? getProperties().getFloatArgumentRegister(regIndex): getProperties().getIntegerArgumentRegister(regIndex);
         }
      else
         {
         offset = layoutResult.offset;
         }

      TR::Register *vreg;
      vreg = cg()->evaluate(child);

      bool needsStackOffsetUpdate = false;
      if (rregIndex != noReg)
         {
         // For NULL JNI reference parameters, it is possible that the NULL value will be evaluated into
         // a different register than the child.  In that case it is not necessary to copy the temporary scratch
         // register across the call.
         //
         if ((child->getReferenceCount() > 1) &&
             (vreg == child->getRegister()))
            {
            TR::Register *argReg = cg()->allocateRegister();
            if (vreg->containsCollectedReference())
               argReg->setContainsCollectedReference();
            generateRegRegInstruction(TR::Linkage::movOpcodes(RegReg, movType(child->getDataType())), child, argReg, vreg, cg());
            vreg = argReg;
            copiedRegs[numCopiedRegs++] = vreg;
            }

         deps->addPreCondition(vreg, rregIndex, cg());
         }
      else
         {
         // Ideally, we would like to push rather than move
         generateMemRegInstruction(TR::Linkage::movOpcodes(MemReg, fullRegisterMovType(vreg)),
                                   child,
                                   generateX86MemoryReference(espReal, offset, cg()),
                                   vreg,
                                   cg());
         }

      cg()->decReferenceCount(child);
      }

   // Now that we're finished making the preconditions, all the interferences
   // are established and we can kill these regs.
   //
   for (i = 0; i < numCopiedRegs; i++)
      cg()->stopUsingRegister(copiedRegs[i]);

   deps->stopAddingPreConditions();

   return sizeOfOutGoingArgs;
   }


TR::Register *
TR::AMD64SystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR_ASSERT(methodSymRef->getSymbol()->castToMethodSymbol()->isComputed(), "system linkage only supports computed indirect call for now %p\n", callNode);

   // Evaluate VFT
   //
   TR::Register *vftRegister;
   TR::Node *vftNode = callNode->getFirstChild();
   if (vftNode->getRegister())
      {
      vftRegister = vftNode->getRegister();
      }
   else
      {
      vftRegister = cg()->evaluate(vftNode);
      }

   // Allocate adequate register dependencies.
   //
   // pre = number of argument registers + 1 for VFT register
   // post = number of volatile + VMThread + return register
   //
   uint32_t pre = getProperties().getNumIntegerArgumentRegisters() + getProperties().getNumFloatArgumentRegisters() + 1;
   uint32_t post = getProperties().getNumVolatileRegisters() + 1 + (callNode->getDataType() == TR::NoType ? 0 : 1);

#if defined (PYTHON) && 0
   // Treat all preserved GP regs as volatile until register map support available.
   //
   post += getProperties().getNumberOfPreservedGPRegisters();
#endif

   TR::RegisterDependencyConditions *callDeps = generateRegisterDependencyConditions(pre, 1, cg());

   TR::RealRegister::RegNum scratchRegIndex = getProperties().getIntegerScratchRegister(1);
   callDeps->addPostCondition(vftRegister, scratchRegIndex, cg());
   callDeps->stopAddingPostConditions();

   // Evaluate outgoing arguments on the system stack and build pre-conditions.
   //
   int32_t memoryArgSize = buildArgs(callNode, callDeps);

   // Dispatch
   //
   generateRegInstruction(CALLReg, callNode, vftRegister, callDeps, cg());
   cg()->resetIsLeafMethod();

   // Build label post-conditions
   //
   TR::RegisterDependencyConditions *postDeps = generateRegisterDependencyConditions(0, post, cg());
   TR::Register *returnReg = buildVolatileAndReturnDependencies(callNode, postDeps);
   postDeps->stopAddingPostConditions();

   TR::LabelSymbol *postDepLabel = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, callNode, postDepLabel, postDeps, cg());

   return returnReg;
   }


TR::Register *TR::AMD64SystemLinkage::buildDirectDispatch(
      TR::Node *callNode,
      bool spillFPRegs)
   {
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   TR::Register *returnReg;

   // Allocate adequate register dependencies.
   //
   // pre = number of argument registers
   // post = number of volatile + return register
   //
   uint32_t pre = getProperties().getNumIntegerArgumentRegisters() + getProperties().getNumFloatArgumentRegisters();
   uint32_t post = getProperties().getNumVolatileRegisters() + (callNode->getDataType() == TR::NoType ? 0 : 1);

#if defined (PYTHON) && 0
   // Treat all preserved GP regs as volatile until register map support available.
   //
   post += getProperties().getNumberOfPreservedGPRegisters();
#endif

   TR::RegisterDependencyConditions *preDeps = generateRegisterDependencyConditions(pre, 0, cg());
   TR::RegisterDependencyConditions *postDeps = generateRegisterDependencyConditions(0, post, cg());

   // Evaluate outgoing arguments on the system stack and build pre-conditions.
   //
   int32_t memoryArgSize = buildArgs(callNode, preDeps);

   // Build post-conditions.
   //
   returnReg = buildVolatileAndReturnDependencies(callNode, postDeps);
   postDeps->stopAddingPostConditions();

   // Find the second scratch register in the post dependency list.
   //
   TR::Register *scratchReg = NULL;
   TR::RealRegister::RegNum scratchRegIndex = getProperties().getIntegerScratchRegister(1);
   for (int32_t i=0; i<post; i++)
      {
      if (postDeps->getPostConditions()->getRegisterDependency(i)->getRealRegister() == scratchRegIndex)
         {
         scratchReg = postDeps->getPostConditions()->getRegisterDependency(i)->getRegister();
         break;
         }
      }

#if defined(PYTHON) && 0
   // For Python, store the instruction that contains the GC map at this site into
   // the frame object.
   //
   TR::SymbolReference *frameObjectSymRef =
      comp()->getSymRefTab()->findOrCreateAutoSymbol(comp()->getMethodSymbol(), 0, TR::Address, true, false, true);

   TR::Register *frameObjectRegister = cg()->allocateRegister();
   generateRegMemInstruction(
         L8RegMem,
         callNode,
         frameObjectRegister,
         generateX86MemoryReference(frameObjectSymRef, cg()),
         cg());

   TR::RealRegister *espReal = cg()->machine()->getX86RealRegister(TR::RealRegister::esp);
   TR::Register *gcMapPCRegister = cg()->allocateRegister();

   generateRegMemInstruction(
         LEA8RegMem,
         callNode,
         gcMapPCRegister,
         generateX86MemoryReference(espReal, -8, cg()),
         cg());

   // Use "volatile" registers across the call.  Once proper register map support
   // is implemented, r14 and r15 will no longer be volatile and a different pair
   // should be chosen.
   //
   TR::RegisterDependencyConditions *gcMapDeps = generateRegisterDependencyConditions(0, 2, cg());
   gcMapDeps->addPostCondition(frameObjectRegister, TR::RealRegister::r14, cg());
   gcMapDeps->addPostCondition(gcMapPCRegister, TR::RealRegister::r15, cg());
   gcMapDeps->stopAddingPostConditions();

   generateMemRegInstruction(
         S8MemReg,
         callNode,
         generateX86MemoryReference(frameObjectRegister, fe()->getPythonGCMapPCOffsetInFrame(), cg()),
         gcMapPCRegister,
         gcMapDeps,
         cg());

   cg()->stopUsingRegister(frameObjectRegister);
   cg()->stopUsingRegister(gcMapPCRegister);
#endif

   TR::Instruction *instr;
   if (methodSymbol->getMethodAddress())
      {
      TR_ASSERT(scratchReg, "could not find second scratch register");
      auto LoadRegisterInstruction = generateRegImm64SymInstruction(
         MOV8RegImm64,
         callNode,
         scratchReg,
         (uintptr_t)methodSymbol->getMethodAddress(),
         methodSymRef,
         cg());

      if( TR::Options::getCmdLineOptions()->getOption(TR_EmitRelocatableELFFile) )
         {
         LoadRegisterInstruction->setReloKind(TR_NativeMethodAbsolute);
         }

      instr = generateRegInstruction(CALLReg, callNode, scratchReg, preDeps, cg());
      }
   else
      {
      instr = generateImmSymInstruction(CALLImm4, callNode, (uintptrj_t)methodSymbol->getMethodAddress(), methodSymRef, preDeps, cg());
      }

   cg()->resetIsLeafMethod();

   instr->setNeedsGCMap(getProperties().getPreservedRegisterMapForGC());

   cg()->stopUsingRegister(scratchReg);

   TR::LabelSymbol *postDepLabel = generateLabelSymbol(cg());
   generateLabelInstruction(LABEL, callNode, postDepLabel, postDeps, cg());

   return returnReg;
   }

static const TR::RealRegister::RegNum NOT_ASSIGNED = (TR::RealRegister::RegNum)-1;

uint32_t
TR::AMD64SystemLinkage::getAlignment(TR::DataType type)
   {
   switch(type)
      {
      case TR::Float:
         return 4;
      case TR::Double:
         return 8;
      case TR::Int8:
         return 1;
      case TR::Int16:
         return 2;
      case TR::Int32:
         return 4;
      case TR::Address:
      case TR::Int64:
         return 8;
      // TODO: struct/union support should be added as a case branch.
      case TR::Aggregate:
      default:
         TR_ASSERT(false, "types still not support in layout\n");
         return 0;
      }
   }


void
TR::AMD64ABILinkage::mapIncomingParms(
      TR::ResolvedMethodSymbol *method,
      uint32_t &stackIndex)
   {
   ListIterator<TR::ParameterSymbol> parameterIterator(&method->getParameterList());
   TR::ParameterSymbol *parmCursor = parameterIterator.getFirst();

   if (!parmCursor) return;

   if (parmCursor->getLinkageRegisterIndex() < 0)
      {
      copyLinkageInfoToParameterSymbols();
      }

   // 1st: handle parameters which are passed through stack
   //
   TR::X86SystemLinkage::mapIncomingParms(method);

   // 2nd: handle parameters which are passed through linkage registers, but are
   // not assigned any register after RA (or say, by their first usage point,
   // a MOV is needed to load it from stack to register).
   //
   // AMD64 SysV ABI says that: a parameter is placed either in registers or
   // pushed on the stack, but can't take both.  So, for parms passed through
   // linkage registers but don't have physical registers assigned after RA,
   // we will allocate stack space in local variable region.
   //
   for (parmCursor = parameterIterator.getFirst(); parmCursor; parmCursor = parameterIterator.getNext())
      {
      if ((parmCursor->getLinkageRegisterIndex() >= 0) && (parmCursor->getAllocatedIndex() < 0 || hasToBeOnStack(parmCursor)))
         {
         uint32_t align = getAlignment(parmCursor->getDataType());
         uint32_t alignMinus1 = (align <= AMD64_STACK_SLOT_SIZE) ? (AMD64_STACK_SLOT_SIZE - 1) : (align - 1);
         uint32_t pos = -stackIndex;
         pos += parmCursor->getSize();
         pos = (pos + alignMinus1) & (~alignMinus1);
         stackIndex = -pos;
         parmCursor->setParameterOffset(stackIndex);

         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "mapIncomingParms setParameterOffset %d for param symbol (reg param without home location) %p, hasToBeOnStack() %d\n", parmCursor->getParameterOffset(), parmCursor, hasToBeOnStack(parmCursor));
         }
      else if (parmCursor->getLinkageRegisterIndex() >=0 && parmCursor->getAllocatedIndex() >= 0)
         {
         //parmCursor->setDontHaveStackSlot(0); // this is a hack , so as we could print stack layout table in createPrologue
         if (comp()->getOption(TR_TraceCG))
            traceMsg(comp(), "mapIncomingParms no need to set parm %p, for it has got register %d assigned\n", parmCursor, parmCursor->getAllocatedIndex());
         }
      }
   }


bool
TR::AMD64SystemLinkage::layoutTypeInRegs(
      TR::DataType type,
      uint16_t &intArgs,
      uint16_t &floatArgs,
      TR::parmLayoutResult &layoutResult)
   {
   int oldIntArgs = intArgs, oldFloatArgs = floatArgs;

   const int32_t maxIntArgs   = getProperties().getNumIntegerArgumentRegisters();
   const int32_t maxFloatArgs = getProperties().getNumFloatArgumentRegisters();

   switch(type)
      {
      case TR::Float:
      case TR::Double:
         if (getProperties().getLinkageRegistersAssignedByCardinalPosition() && intArgs < maxIntArgs)
            intArgs++;
         if (floatArgs < maxFloatArgs)
            {
            layoutResult.regs[0].regIndex = floatArgs++;
            layoutResult.regs[0].regKind = TR_FPR;
            }
         else
            goto FAIL_TO_LAYOUT_IN_REGS;
         return true;
      case TR::Int8:
      case TR::Int16:
      case TR::Int32:
      case TR::Int64:
      case TR::Address:
         if (getProperties().getLinkageRegistersAssignedByCardinalPosition() && floatArgs < maxFloatArgs)
            floatArgs++;
         if (intArgs < maxIntArgs)
            {
            layoutResult.regs[0].regIndex = intArgs++;
            layoutResult.regs[0].regKind = TR_GPR;
            }
         else
            goto FAIL_TO_LAYOUT_IN_REGS;
         return true;
      // TODO: struct/union support should be added as a case branch.
      case TR::Aggregate:
      default:
         TR_ASSERT(false, "types still not support in layout\n");
         return false;
      }

FAIL_TO_LAYOUT_IN_REGS:
   // AMD64 SysV ABI:  If there are no registers available for any eightbyte of an argument, the whole argument is passed on the stack. If registers have already been assigned for some eightbytes of such an argument, the assignments get reverted.
   intArgs = oldIntArgs;
   floatArgs = oldFloatArgs;
   return false;
   }


int32_t
TR::AMD64SystemLinkage::layoutParm(
      TR::Node *parmNode,
      int32_t &dataCursor,
      uint16_t &intReg,
      uint16_t &floatReg,
      TR::parmLayoutResult &layoutResult)
   {
   //AMD64 SysV ABI:  if the size of an object is larger than four eightbytes, or it contains unaligned fields, it has class MEMORY.
   if (parmNode->getSize() > 4*AMD64_STACK_SLOT_SIZE) goto LAYOUT_ON_STACK;

   if (layoutTypeInRegs(parmNode->getDataType(), intReg, floatReg, layoutResult))
      {
      layoutResult.abstract |= TR::parmLayoutResult::IN_LINKAGE_REG;

      if (parmNode->getSize() > GPR_REG_WIDTH)
         layoutResult.abstract |= TR::parmLayoutResult::IN_LINKAGE_REG_PAIR;

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "layout param node %p in register\n", parmNode);

      if (!getProperties().getCallerFrameAllocatesSpaceForLinkageRegisters())
         return 0;
      }

LAYOUT_ON_STACK:
   layoutResult.abstract |= TR::parmLayoutResult::ON_STACK;
   int32_t align = layoutTypeOnStack(parmNode->getDataType(), dataCursor, layoutResult);
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "layout param node %p on stack\n", parmNode);
   return align;
   }


// TODO: Try to combine the 2 layoutParm functions.
int32_t
TR::AMD64SystemLinkage::layoutParm(
      TR::ParameterSymbol *parmSymbol,
      int32_t &dataCursor,
      uint16_t &intReg,
      uint16_t &floatReg,
      TR::parmLayoutResult &layoutResult)
   {
   //AMD64 SysV ABI:  if the size of an object is larger than four eightbytes, or it contains unaligned fields, it has class MEMORY.
   if (parmSymbol->getSize() > 4*AMD64_STACK_SLOT_SIZE) goto LAYOUT_ON_STACK;

   if (layoutTypeInRegs(parmSymbol->getDataType(), intReg, floatReg, layoutResult))
      {
      layoutResult.abstract |= TR::parmLayoutResult::IN_LINKAGE_REG;

      if (parmSymbol->getSize() > GPR_REG_WIDTH)
         layoutResult.abstract |= TR::parmLayoutResult::IN_LINKAGE_REG_PAIR;

      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "layout param symbol %p in register\n", parmSymbol);

      if (!getProperties().getCallerFrameAllocatesSpaceForLinkageRegisters())
         return 0;
      }

LAYOUT_ON_STACK:
   layoutResult.abstract |= TR::parmLayoutResult::ON_STACK;
   int32_t align = layoutTypeOnStack(parmSymbol->getDataType(), dataCursor, layoutResult);
   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "layout param symbol %p on stack\n", parmSymbol);
   return align;
   }


void
TR::AMD64SystemLinkage::setUpStackSizeForCallNode(TR::Node* node)
   {
   const TR::X86LinkageProperties     &properties = getProperties();
   uint16_t intReg = 0, floatReg = 0;
   // AMD64 SysV ABI: The end of the input argument area shall be aligned on a 16 (32, if __m256 is passed on stack) byte boundary. In other words, the value (%rsp + 8) is always a multiple of 16 (32) when control is transferred to the function entry point.
   int32_t alignment = AMD64_DEFAULT_STACK_ALIGNMENT;
   int32_t sizeOfOutGoingArgs = 0;

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "setUpStackSizeForCallNode for call node %p\n", node);

   for (int32_t i= node->getFirstArgumentIndex(); i<node->getNumChildren(); ++i)
      {
      TR::parmLayoutResult fakeParm;
      TR::Node *parmNode = node->getChild(i);
      int32_t parmAlign = layoutParm(parmNode, sizeOfOutGoingArgs, intReg, floatReg, fakeParm);
      if (parmAlign == 32)
         alignment = 32;
      }

   if (sizeOfOutGoingArgs > cg()->getLargestOutgoingArgSize())
      {
      cg()->setLargestOutgoingArgSize(sizeOfOutGoingArgs);
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "setUpStackSizeForCallNode setLargestOutgoingArgSize %d(for call node %p)\n", sizeOfOutGoingArgs, node);
      }

   if (alignment > _properties.getOutgoingArgAlignment())
      {
      _properties.setOutgoingArgAlignment(alignment);
      if (comp()->getOption(TR_TraceCG))
         traceMsg(comp(), "setUpStackSizeForCallNode setOutgoingArgAlignment %d(for call node %p)\n", alignment, node);
      }
   }
