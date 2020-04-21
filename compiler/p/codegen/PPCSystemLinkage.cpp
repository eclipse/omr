/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "p/codegen/PPCSystemLinkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/Processors.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "runtime/Runtime.hpp"

uint32_t
mappedOffsetToFirstLocal(
      TR::CodeGenerator *cg,
      const TR::PPCLinkageProperties &linkage)
   {
   TR::Machine *machine = cg->machine();

   uint32_t min_arg_area = machine->getLinkRegisterKilled() ?
                               (cg->comp()->target().is64Bit() ? 8*8 : 8*4)
                               : 0;

   uint32_t offset = linkage.getOffsetToFirstLocal() +
      (cg->getLargestOutgoingArgSize() > min_arg_area ? cg->getLargestOutgoingArgSize() : min_arg_area);

   return offset;
   }


TR::PPCSystemLinkage::PPCSystemLinkage(TR::CodeGenerator *cg)
   : TR::Linkage(cg)
   {
   int i = 0;
   _properties._properties = IntegersInRegisters|FloatsInRegisters|RightToLeft;
   if (comp()->target().cpu.isBigEndian())
      _properties._properties |= SmallIntParmsAlignedRight;

   _properties._registerFlags[TR::RealRegister::NoReg] = 0;
   _properties._registerFlags[TR::RealRegister::gr0]   = 0;
   _properties._registerFlags[TR::RealRegister::gr1]   = Preserved|PPC_Reserved; // system sp
   _properties._registerFlags[TR::RealRegister::gr2]   = Preserved|PPC_Reserved; // TOC/system reserved
   _properties._registerFlags[TR::RealRegister::gr3]   = IntegerReturn|IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr4]   = IntegerReturn|IntegerArgument;

   _properties._registerFlags[TR::RealRegister::gr5]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr6]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr7]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr8]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr9]  = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr10] = IntegerArgument;
   _properties._registerFlags[TR::RealRegister::gr11] = 0;
   _properties._registerFlags[TR::RealRegister::gr12] = 0;
   if (cg->comp()->target().is64Bit())
      _properties._registerFlags[TR::RealRegister::gr13] = Preserved|PPC_Reserved; // system
   else
      _properties._registerFlags[TR::RealRegister::gr13] = Preserved;

   for (i = TR::RealRegister::gr14; i <= TR::RealRegister::LastGPR; i++)
      _properties._registerFlags[i]  = Preserved; // gr14 - gr31 preserved

   _properties._registerFlags[TR::RealRegister::fp0]   = 0;
   _properties._registerFlags[TR::RealRegister::fp1]   = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::fp2]   = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::fp3]   = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::fp4]   = FloatArgument|FloatReturn;
   _properties._registerFlags[TR::RealRegister::fp5]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp6]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp7]   = FloatArgument;
   _properties._registerFlags[TR::RealRegister::fp8]   = FloatArgument;

   if (cg->comp()->target().is64Bit() || cg->comp()->target().isAIX())
      {
      _properties._registerFlags[TR::RealRegister::fp9]   = FloatArgument;
      _properties._registerFlags[TR::RealRegister::fp10]  = FloatArgument;
      _properties._registerFlags[TR::RealRegister::fp11]  = FloatArgument;
      _properties._registerFlags[TR::RealRegister::fp12]  = FloatArgument;
      _properties._registerFlags[TR::RealRegister::fp13]  = FloatArgument;
      }
   else
      {
      _properties._registerFlags[TR::RealRegister::fp9]   = 0; //non-aix 32bit
      _properties._registerFlags[TR::RealRegister::fp10]  = 0;
      _properties._registerFlags[TR::RealRegister::fp11]  = 0;
      _properties._registerFlags[TR::RealRegister::fp12]  = 0;
      _properties._registerFlags[TR::RealRegister::fp13]  = 0;
      }

   for (i = TR::RealRegister::fp14; i <= TR::RealRegister::LastFPR; i++)
      _properties._registerFlags[i]  = Preserved; // fp14 - fp31 Preserved

   for (i = TR::RealRegister::vsr32; i <= TR::RealRegister::vsr51; i++)
      _properties._registerFlags[i]  = 0;         // vsr32 - vsr51 volatile

   for (i = TR::RealRegister::vsr52; i <= TR::RealRegister::LastVSR; i++)
      _properties._registerFlags[i]  = Preserved; // vsr52 - vsr63 Preserved


   _properties._registerFlags[TR::RealRegister::cr0]  = 0;
   _properties._registerFlags[TR::RealRegister::cr1]  = 0;
   _properties._registerFlags[TR::RealRegister::cr2]  = Preserved;
   _properties._registerFlags[TR::RealRegister::cr3]  = Preserved;
   _properties._registerFlags[TR::RealRegister::cr4]  = Preserved;
   _properties._registerFlags[TR::RealRegister::cr5]  = 0;
   _properties._registerFlags[TR::RealRegister::cr6]  = 0;
   _properties._registerFlags[TR::RealRegister::cr7]  = 0;

   _properties._numIntegerArgumentRegisters  = 8;
   _properties._firstIntegerArgumentRegister = 0;
   if (cg->comp()->target().is64Bit() || cg->comp()->target().isAIX())
      _properties._numFloatArgumentRegisters    = 13;
   else
      _properties._numFloatArgumentRegisters    = 8;

   _properties._firstFloatArgumentRegister   = 9;

   _properties._numVectorArgumentRegisters    = 1;  // TODO: finish vector linkage
   _properties._firstVectorArgumentRegister   = 22;

   _properties._argumentRegisters[0]  = TR::RealRegister::gr3;
   _properties._argumentRegisters[1]  = TR::RealRegister::gr4;
   _properties._argumentRegisters[2]  = TR::RealRegister::gr5;
   _properties._argumentRegisters[3]  = TR::RealRegister::gr6;
   _properties._argumentRegisters[4]  = TR::RealRegister::gr7;
   _properties._argumentRegisters[5]  = TR::RealRegister::gr8;
   _properties._argumentRegisters[6]  = TR::RealRegister::gr9;
   _properties._argumentRegisters[7]  = TR::RealRegister::gr10;
   // We use a somewhat hacky convention to set the linkageRegisterIndex
   // of the GPR containing the environment pointer....we set it
   // to just past the number of integer argument registers.  That's
   // why the following line is here....
   _properties._argumentRegisters[8]  = TR::RealRegister::gr11;
   _properties._argumentRegisters[9]  = TR::RealRegister::fp1;
   _properties._argumentRegisters[10]  = TR::RealRegister::fp2;
   _properties._argumentRegisters[11] = TR::RealRegister::fp3;
   _properties._argumentRegisters[12] = TR::RealRegister::fp4;
   _properties._argumentRegisters[13] = TR::RealRegister::fp5;
   _properties._argumentRegisters[14] = TR::RealRegister::fp6;
   _properties._argumentRegisters[15] = TR::RealRegister::fp7;
   _properties._argumentRegisters[16] = TR::RealRegister::fp8;

   if (cg->comp()->target().is64Bit() || cg->comp()->target().isAIX())
       {
       _properties._argumentRegisters[17] = TR::RealRegister::fp9;
       _properties._argumentRegisters[18] = TR::RealRegister::fp10;
       _properties._argumentRegisters[19] = TR::RealRegister::fp11;
       _properties._argumentRegisters[20] = TR::RealRegister::fp12;
       _properties._argumentRegisters[21] = TR::RealRegister::fp13;
       }
   _properties._argumentRegisters[22] = TR::RealRegister::vsr34;

   _properties._firstIntegerReturnRegister = 0;
   _properties._firstFloatReturnRegister = 2;
   _properties._firstVectorReturnRegister = 6;

   _properties._returnRegisters[0]  = TR::RealRegister::gr3;
   _properties._returnRegisters[1]  = TR::RealRegister::gr4;
   _properties._returnRegisters[2]  = TR::RealRegister::fp1;
   _properties._returnRegisters[3]  = TR::RealRegister::fp2;
   _properties._returnRegisters[4]  = TR::RealRegister::fp3;
   _properties._returnRegisters[5]  = TR::RealRegister::fp4;
   _properties._returnRegisters[6]  = TR::RealRegister::vsr34;

   if (cg->comp()->target().is64Bit())
      {
      _properties._numAllocatableIntegerRegisters          = 29; // 64
      _properties._firstAllocatableFloatArgumentRegister   = 42; // 64
      _properties._lastAllocatableFloatVolatileRegister    = 42; // 64
      }
   else
      {
      _properties._numAllocatableIntegerRegisters          = 30; // 32
      _properties._firstAllocatableFloatArgumentRegister   = 43; // 32
      _properties._lastAllocatableFloatVolatileRegister    = 43; // 32
      }

   if (cg->comp()->target().is32Bit() && cg->comp()->target().isAIX())
      _properties._firstAllocatableIntegerArgumentRegister = 9;  // aix 32 only
   else
      _properties._firstAllocatableIntegerArgumentRegister = 8;

   _properties._lastAllocatableIntegerVolatileRegister  = 10;
   _properties._numAllocatableFloatRegisters            = 32;
   _properties._numAllocatableCCRegisters               = 8;

   i = 0;
   _properties._allocationOrder[i++] = TR::RealRegister::gr12;

   if (cg->comp()->target().is32Bit() && cg->comp()->target().isAIX())
      _properties._allocationOrder[i++] = TR::RealRegister::gr11;

   _properties._allocationOrder[i++] = TR::RealRegister::gr10;
   _properties._allocationOrder[i++] = TR::RealRegister::gr9;
   _properties._allocationOrder[i++] = TR::RealRegister::gr8;
   _properties._allocationOrder[i++] = TR::RealRegister::gr7;
   _properties._allocationOrder[i++] = TR::RealRegister::gr6;
   _properties._allocationOrder[i++] = TR::RealRegister::gr5;
   _properties._allocationOrder[i++] = TR::RealRegister::gr4;
   _properties._allocationOrder[i++] = TR::RealRegister::gr3;

   if (cg->comp()->target().is64Bit() || !cg->comp()->target().isAIX())
      _properties._allocationOrder[i++] = TR::RealRegister::gr11;

   _properties._allocationOrder[i++] = TR::RealRegister::gr0;
   _properties._allocationOrder[i++] = TR::RealRegister::gr31;
   _properties._allocationOrder[i++] = TR::RealRegister::gr30;
   _properties._allocationOrder[i++] = TR::RealRegister::gr29;
   _properties._allocationOrder[i++] = TR::RealRegister::gr28;
   _properties._allocationOrder[i++] = TR::RealRegister::gr27;
   _properties._allocationOrder[i++] = TR::RealRegister::gr26;
   _properties._allocationOrder[i++] = TR::RealRegister::gr25;
   _properties._allocationOrder[i++] = TR::RealRegister::gr24;
   _properties._allocationOrder[i++] = TR::RealRegister::gr23;
   _properties._allocationOrder[i++] = TR::RealRegister::gr22;
   _properties._allocationOrder[i++] = TR::RealRegister::gr21;
   _properties._allocationOrder[i++] = TR::RealRegister::gr20;
   _properties._allocationOrder[i++] = TR::RealRegister::gr19;
   _properties._allocationOrder[i++] = TR::RealRegister::gr18;
   _properties._allocationOrder[i++] = TR::RealRegister::gr17;
   _properties._allocationOrder[i++] = TR::RealRegister::gr16;
   _properties._allocationOrder[i++] = TR::RealRegister::gr15;
   _properties._allocationOrder[i++] = TR::RealRegister::gr14;

   if (cg->comp()->target().is32Bit())
      _properties._allocationOrder[i++] = TR::RealRegister::gr13;

   _properties._allocationOrder[i++] = TR::RealRegister::fp0;
   _properties._allocationOrder[i++] = TR::RealRegister::fp13;
   _properties._allocationOrder[i++] = TR::RealRegister::fp12;
   _properties._allocationOrder[i++] = TR::RealRegister::fp11;
   _properties._allocationOrder[i++] = TR::RealRegister::fp10;
   _properties._allocationOrder[i++] = TR::RealRegister::fp9;
   _properties._allocationOrder[i++] = TR::RealRegister::fp8;
   _properties._allocationOrder[i++] = TR::RealRegister::fp7;
   _properties._allocationOrder[i++] = TR::RealRegister::fp6;
   _properties._allocationOrder[i++] = TR::RealRegister::fp5;
   _properties._allocationOrder[i++] = TR::RealRegister::fp4;
   _properties._allocationOrder[i++] = TR::RealRegister::fp3;
   _properties._allocationOrder[i++] = TR::RealRegister::fp2;
   _properties._allocationOrder[i++] = TR::RealRegister::fp1;
   _properties._allocationOrder[i++] = TR::RealRegister::fp31;
   _properties._allocationOrder[i++] = TR::RealRegister::fp30;
   _properties._allocationOrder[i++] = TR::RealRegister::fp29;
   _properties._allocationOrder[i++] = TR::RealRegister::fp28;
   _properties._allocationOrder[i++] = TR::RealRegister::fp27;
   _properties._allocationOrder[i++] = TR::RealRegister::fp26;
   _properties._allocationOrder[i++] = TR::RealRegister::fp25;
   _properties._allocationOrder[i++] = TR::RealRegister::fp24;
   _properties._allocationOrder[i++] = TR::RealRegister::fp23;
   _properties._allocationOrder[i++] = TR::RealRegister::fp22;
   _properties._allocationOrder[i++] = TR::RealRegister::fp21;
   _properties._allocationOrder[i++] = TR::RealRegister::fp20;
   _properties._allocationOrder[i++] = TR::RealRegister::fp19;
   _properties._allocationOrder[i++] = TR::RealRegister::fp18;
   _properties._allocationOrder[i++] = TR::RealRegister::fp17;
   _properties._allocationOrder[i++] = TR::RealRegister::fp16;
   _properties._allocationOrder[i++] = TR::RealRegister::fp15;
   _properties._allocationOrder[i++] = TR::RealRegister::fp14;
   _properties._allocationOrder[i++] = TR::RealRegister::cr7;
   _properties._allocationOrder[i++] = TR::RealRegister::cr6;
   _properties._allocationOrder[i++] = TR::RealRegister::cr5;
   _properties._allocationOrder[i++] = TR::RealRegister::cr1;
   _properties._allocationOrder[i++] = TR::RealRegister::cr0;
   _properties._allocationOrder[i++] = TR::RealRegister::cr2;
   _properties._allocationOrder[i++] = TR::RealRegister::cr3;
   _properties._allocationOrder[i++] = TR::RealRegister::cr4;

   _properties._preservedRegisterMapForGC     = 0x60007fff;
   _properties._methodMetaDataRegister        = TR::RealRegister::NoReg;
   _properties._normalStackPointerRegister    = TR::RealRegister::gr1;
   _properties._alternateStackPointerRegister = TR::RealRegister::gr31;
   _properties._TOCBaseRegister               = TR::RealRegister::gr2;
   _properties._vtableIndexArgumentRegister   = TR::RealRegister::NoReg;
   _properties._j9methodArgumentRegister      = TR::RealRegister::NoReg;

   // Note: FPRs 14-31 are preserved, however if you use them as vector registers you must
   // assume the additional 64 bits are volatile because the callee will only preserve
   // the FPR portion and the additional 64 bits will be undefined after the callee returns.
   if (cg->comp()->target().is64Bit())
      {
      bool isBE = cg->comp()->target().cpu.isBigEndian();

      // Volatile GPR (0,2-12) + FPR (0-13) + CCR (0-1,5-7) + VR (0-19) + FPR (14-31) if used as vector
      _properties._numberOfDependencyGPRegisters = 12 + 14 + 5 + 20 + 18;
      _properties._offsetToFirstParm             = isBE?48:32;
      _properties._offsetToFirstLocal            = isBE?48:32;
      }
   else
      {
      if (cg->comp()->target().isAIX())
         {
         // Volatile GPR (0,2-12) + FPR (0-13) + CCR (0-1,5-7) + VR (0-19) + FPR (14-31) if used as vector
         _properties._numberOfDependencyGPRegisters = 12 + 14 + 5 + 20 + 18;
         _properties._offsetToFirstParm             = 24;
         _properties._offsetToFirstLocal            = 24;
         }
      else
         {
         // Volatile GPR (0,3-12) + FPR (0-13) + CCR (0-1,5-7) + VR (0-19) + FPR (14-31) if used as vector
         _properties._numberOfDependencyGPRegisters = 11 + 14 + 5 + 20 + 18;
         _properties._offsetToFirstParm             = 8;
         _properties._offsetToFirstLocal            = 8;
         }
      }
   }


const TR::PPCLinkageProperties&
TR::PPCSystemLinkage::getProperties()
   {
   return _properties;
   }


void
TR::PPCSystemLinkage::initPPCRealRegisterLinkage()
   {
   // Each real register's weight is set to match this linkage convention
   TR::Machine *machine = cg()->machine();
   int icount;

   icount = TR::RealRegister::gr1;
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setState(TR::RealRegister::Locked);
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setAssignedRegister(machine->getRealRegister((TR::RealRegister::RegNum)icount));

   icount = TR::RealRegister::gr2;
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setState(TR::RealRegister::Locked);
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setAssignedRegister(machine->getRealRegister((TR::RealRegister::RegNum)icount));

   icount = TR::RealRegister::gr13;
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setState(TR::RealRegister::Locked);
   machine->getRealRegister((TR::RealRegister::RegNum)icount)->setAssignedRegister(machine->getRealRegister((TR::RealRegister::RegNum)icount));

   for (icount=TR::RealRegister::gr3; icount<=TR::RealRegister::gr12;
        icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);

   for (icount=TR::RealRegister::LastGPR;
        icount>=TR::RealRegister::gr14; icount--)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);

   for (icount=TR::RealRegister::FirstFPR;
        icount<=TR::RealRegister::fp13; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);

   for (icount=TR::RealRegister::LastFPR;
        icount>=TR::RealRegister::fp14; icount--)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);

   for (icount=TR::RealRegister::FirstVRF;
        icount<=TR::RealRegister::LastVRF; icount++)
      machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);

   for (icount=TR::RealRegister::FirstCCR;
        icount<=TR::RealRegister::LastCCR; icount++)
      if (icount>=TR::RealRegister::cr2 && icount<=TR::RealRegister::cr4)
         {
         machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(0xf000-icount);
         }
      else
         {
         machine->getRealRegister((TR::RealRegister::RegNum)icount)->setWeight(icount);
         }

   machine->setNumberOfLockedRegisters(TR_GPR, 3);
   machine->setNumberOfLockedRegisters(TR_FPR, 0);
   machine->setNumberOfLockedRegisters(TR_VRF, 0);
   }


uint32_t
TR::PPCSystemLinkage::getRightToLeft()
   {
   return getProperties().getRightToLeft();
   }


bool
TR::PPCSystemLinkage::hasToBeOnStack(TR::ParameterSymbol *parm)
   {
   return(parm->getAssignedGlobalRegisterIndex()>=0  && parm->isParmHasToBeOnStack());
   }


static bool
hasParmsPassedOnStack(List<TR::ParameterSymbol> &parmList)
   {
   ListIterator<TR::ParameterSymbol> parmIt(&parmList);
   TR::ParameterSymbol *parmCursor = parmIt.getFirst();

   while (parmCursor)
      {
      if (parmCursor->getLinkageRegisterIndex() < 0)
         return true;

      parmCursor = parmIt.getNext();
      }

   return false;
   }

void
TR::PPCSystemLinkage::mapParameters(
      TR::ResolvedMethodSymbol *method,
      List<TR::ParameterSymbol> &parmList)
   {

   uint32_t   stackIndex = method->getLocalMappingCursor();
   ListIterator<TR::ParameterSymbol> parameterIterator(&parmList);
   TR::ParameterSymbol              *parmCursor = parameterIterator.getFirst();
   const TR::PPCLinkageProperties&    linkage           = getProperties();
   int32_t                          offsetToFirstParm = linkage.getOffsetToFirstParm();
   int32_t offset_from_top = 0;
   int32_t slot_size = sizeof(uintptr_t);

   // The 64-bit ELF V2 ABI Specification makes having a parameter save area optional if all parameters can be passed in
   // registers. As a result, we cannot use the caller's parameter save area on 64-bit Linux if all parameters were
   // passed in registers. However, we *must* use the caller's parameter save area if one or more parameters were passed
   // on the stack to load those values correctly.
   bool saveParmsInLocalArea = comp()->target().isLinux() && comp()->target().is64Bit() && !hasParmsPassedOnStack(parmList);

   if (linkage.getRightToLeft())
      {
      while (parmCursor != NULL)
         {
         if (saveParmsInLocalArea)
            parmCursor->setParameterOffset(offset_from_top + stackIndex);
         else
            parmCursor->setParameterOffset(offset_from_top + offsetToFirstParm + stackIndex);

         if (linkage.getSmallIntParmsAlignedRight() && parmCursor->getType().isIntegral() && parmCursor->getSize() < slot_size)
            parmCursor->setParameterOffset(parmCursor->getParameterOffset() + slot_size - parmCursor->getSize());

         offset_from_top += (parmCursor->getSize() + slot_size - 1) & (~(slot_size - 1));
         parmCursor = parameterIterator.getNext();
         }
      if (saveParmsInLocalArea)
         method->setLocalMappingCursor(offset_from_top + stackIndex);
      }
   else
      {
      uint32_t sizeOfParameterArea = method->getNumParameterSlots() << 2;
      while (parmCursor != NULL)
         {
         parmCursor->setParameterOffset(sizeOfParameterArea -
                                        offset_from_top -
                                        parmCursor->getSize() + stackIndex +
                                        offsetToFirstParm);
         offset_from_top += (parmCursor->getSize() + slot_size - 1) & (~(slot_size - 1));
         parmCursor = parameterIterator.getNext();
         }
      }

   }


void
TR::PPCSystemLinkage::mapStack(TR::ResolvedMethodSymbol *method)
   {
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol *localCursor = automaticIterator.getFirst();
   const TR::PPCLinkageProperties& linkage = getProperties();
   TR::Machine *machine = cg()->machine();
   uint32_t stackIndex;
   int32_t lowGCOffset;
   TR::GCStackAtlas *atlas = cg()->getStackAtlas();

   // map all garbage collected references together so can concisely represent
   // stack maps. They must be mapped so that the GC map index in each local
   // symbol is honoured.

   stackIndex = mappedOffsetToFirstLocal(cg(), linkage);
   lowGCOffset = stackIndex;

   // Now map the rest of the locals
   //
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      mapSingleAutomatic(localCursor, stackIndex);
      localCursor = automaticIterator.getNext();
      }

   method->setScalarTempSlots((stackIndex-lowGCOffset) >> 2);

   TR::RealRegister::RegNum regIndex = TR::RealRegister::gr13;
   if (!method->isEHAware())
      {
      while (regIndex<=TR::RealRegister::LastGPR && !machine->getRealRegister(regIndex)->getHasBeenAssignedInMethod())
         regIndex = (TR::RealRegister::RegNum)((uint32_t)regIndex+1);
      }

   stackIndex += (TR::RealRegister::LastGPR-regIndex + 1) * TR::Compiler->om.sizeofReferenceAddress();

   // Align the stack frame (previously to 8 bytes, now to 16 for SIMD)
   //
   const uint32_t alignBytes = 16;
   stackIndex = (stackIndex + (alignBytes - 1)) & (~(alignBytes - 1));

   regIndex = TR::RealRegister::fp14;
   if (!method->isEHAware())
      {
      while (regIndex<=TR::RealRegister::LastFPR && !machine->getRealRegister(regIndex)->getHasBeenAssignedInMethod())
         regIndex = (TR::RealRegister::RegNum)((uint32_t)regIndex + 1);
      }

   stackIndex += (TR::RealRegister::LastFPR - regIndex + 1) * 8;

   method->setLocalMappingCursor(stackIndex);

   int32_t offsetToFirstParm = linkage.getOffsetToFirstParm();
   mapParameters(method, method->getParameterList());

#ifdef __LITTLE_ENDIAN__
   // XXX: See the unfortunate hack in mapParameters()
   stackIndex = method->getLocalMappingCursor();
#endif

   atlas->setParmBaseOffset(atlas->getParmBaseOffset() + offsetToFirstParm + stackIndex);
   }


void
TR::PPCSystemLinkage::mapSingleAutomatic(
      TR::AutomaticSymbol *p,
      uint32_t &stackIndex)
   {
   size_t align = 1;
   size_t size = p->getSize();

   if ((size & (size - 1)) == 0  && size <= 8) // if size is power of 2 and small
      {
      align = size;
      }
   else if (size > 8)
      {
      align = 8;
      }

   if (align > 1)
      stackIndex = (stackIndex + align - 1)&(~(align - 1));

   p->setOffset(stackIndex);
   stackIndex += size;
   }


void
TR::PPCSystemLinkage::createPrologue(TR::Instruction *cursor)
   {
   TR::Node *firstNode = comp()->getStartTree()->getNode();

   if (comp()->getOption(TR_EntryBreakPoints))
      {
      cursor = generateInstruction(cg(), TR::InstOpCode::bad, firstNode, cursor);
      }

   createPrologue(cursor, comp()->getJittedMethodSymbol()->getParameterList());
   }


void
TR::PPCSystemLinkage::createPrologue(
      TR::Instruction *cursor,
      List<TR::ParameterSymbol> &parmList)
   {
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties &properties = getProperties();
   TR::ResolvedMethodSymbol        *bodySymbol = comp()->getJittedMethodSymbol();
   // Stack Pointer Register may currently be set to "alternate"
   cg()->setStackPointerRegister(machine->getRealRegister(properties.getNormalStackPointerRegister()));
   TR::RealRegister        *sp = cg()->getStackPointerRegister();
   TR::RealRegister        *metaBase = cg()->getMethodMetaDataRegister();
   TR::RealRegister        *gr2 = machine->getRealRegister(TR::RealRegister::gr2);
   TR::RealRegister        *gr0 = machine->getRealRegister(TR::RealRegister::gr0);
   TR::RealRegister        *gr11 = machine->getRealRegister(TR::RealRegister::gr11);
   TR::RealRegister        *cr0 = machine->getRealRegister(TR::RealRegister::cr0);
   TR::Node                   *firstNode = comp()->getStartTree()->getNode();
   TR::RealRegister::RegNum regIndex;
   int32_t                    size = bodySymbol->getLocalMappingCursor();
   int32_t                    argSize;


#ifdef __LITTLE_ENDIAN__
   //Move TOCBase into r2
   cursor = loadConstant(cg(), firstNode, (int64_t)(cg()->getTOCBase()), gr2, cursor, false, false);
#endif

   if (machine->getLinkRegisterKilled())
      {
      cursor = generateTrg1Instruction(cg(), TR::InstOpCode::mflr, firstNode, gr0, cursor);
      }

   if (machine->getLinkRegisterKilled())
      cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, 2*TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr0, cursor);

   // Save in-register arguments to the caller frame: this will not be
   // necessary if this linkage can be accounted for in the code-gen.
   // Force standard saveArguments behaviour by setting fsd=false and saveOnly=false
   //cursor = saveArguments(cursor, false, false);

   argSize = 0;
   TR::RealRegister::RegNum savedFirst=TR::RealRegister::fp14;
   if (!bodySymbol->isEHAware())
      {
      while (savedFirst<=TR::RealRegister::LastFPR && !machine->getRealRegister(savedFirst)->getHasBeenAssignedInMethod())
         savedFirst=(TR::RealRegister::RegNum)((uint32_t)savedFirst+1);
      }
   for (regIndex=TR::RealRegister::LastFPR; regIndex>=savedFirst; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex-1))
      {
      argSize = argSize - 8;
      cursor = generateMemSrc1Instruction(cg(), TR::InstOpCode::stfd, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, argSize, 8, cg()), machine->getRealRegister(regIndex), cursor);
      }

   savedFirst = TR::RealRegister::gr13;
   if (!bodySymbol->isEHAware())
      {
      while (savedFirst<=TR::RealRegister::LastGPR && !machine->getRealRegister(savedFirst)->getHasBeenAssignedInMethod())
         savedFirst=(TR::RealRegister::RegNum)((uint32_t)savedFirst+1);
      }

   if (savedFirst <= TR::RealRegister::LastGPR)
      {
      if (cg()->comp()->target().cpu.id() == TR_PPCgp || cg()->comp()->target().is64Bit() ||
          (!comp()->getOption(TR_OptimizeForSpace) &&
           TR::RealRegister::LastGPR - savedFirst <= 3))
         for (regIndex=TR::RealRegister::LastGPR; regIndex>=savedFirst; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex-1))
            {
            argSize = argSize - TR::Compiler->om.sizeofReferenceAddress();
            cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, argSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), machine->getRealRegister(regIndex), cursor);
            }
      else
         {
         argSize = argSize - (TR::RealRegister::LastGPR - savedFirst + 1) * 4;
         cursor = generateMemSrc1Instruction(cg(), (savedFirst==TR::RealRegister::LastGPR)?TR::InstOpCode::stw:TR::InstOpCode::stmw, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, argSize, 4*(TR::RealRegister::LastGPR-savedFirst+1), cg()), machine->getRealRegister(savedFirst), cursor);
         }
      }

   if ( bodySymbol->isEHAware() ||
        machine->getRealRegister(TR::RealRegister::cr2)->getHasBeenAssignedInMethod() ||
        machine->getRealRegister(TR::RealRegister::cr3)->getHasBeenAssignedInMethod() ||
        machine->getRealRegister(TR::RealRegister::cr4)->getHasBeenAssignedInMethod() )
      {
      cursor = generateTrg1Instruction(cg(), TR::InstOpCode::mfcr, firstNode, gr0, cursor);
      cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_st, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), gr0, cursor);
      }

   // If only the link area is needed, a stack frame is unnecessary.
   // Some opcodes write into temps above stack, therefore need to allocate
   // stack frame if there are any saved non-volatiles
   //

   if ( size > properties.getOffsetToFirstParm()  - argSize)
      {
      if (size > (-LOWER_IMMED))
         {
         cursor = loadConstant(cg(), firstNode, -size, gr11, cursor);
         cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stux, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, gr11, TR::Compiler->om.sizeofReferenceAddress(), cg()), sp, cursor);
         }
      else
         {
         cursor = generateMemSrc1Instruction(cg(),TR::InstOpCode::Op_stu, firstNode, new (trHeapMemory()) TR::MemoryReference(sp, -size, TR::Compiler->om.sizeofReferenceAddress(), cg()), sp, cursor);
         }
      }
   else   // leaf routine
      {
      ListIterator<TR::ParameterSymbol> parameterIterator(&parmList);
      TR::ParameterSymbol              *parmCursor = parameterIterator.getFirst();
      while (parmCursor != NULL)
         {
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() - size);
         parmCursor = parameterIterator.getNext();
         }
      }

   // Force standard saveArguments behaviour by setting fsd=false and saveOnly=false
   cursor = saveArguments(cursor, false, false, parmList);

   //Preserve JIT pseudo-TOC as required by FrontEnd.
   if(cg()->hasCall())
      {
      cg()->setAppendInstruction(cursor);
      TR::TreeEvaluator::preserveTOCRegister(firstNode, cg(), NULL);
      }

   }


void
TR::PPCSystemLinkage::createEpilogue(TR::Instruction *cursor)
   {
   int32_t blockNumber = cursor->getNext()->getBlockIndex();
   TR::Machine *machine = cg()->machine();
   const TR::PPCLinkageProperties &properties = getProperties();
   TR::ResolvedMethodSymbol *bodySymbol = comp()->getJittedMethodSymbol();
   TR::RealRegister *sp = cg()->getStackPointerRegister();
   TR::RealRegister *sp2 = sp;
   TR::RealRegister *gr0 = machine->getRealRegister(TR::RealRegister::gr0);
   TR::RealRegister *gr11 = machine->getRealRegister(TR::RealRegister::gr11);
   TR::Node *currentNode = cursor->getNode();
   int32_t size = bodySymbol->getLocalMappingCursor();
   int32_t saveSize = 0;
   TR::Instruction *tempCursor = cursor;
   TR::RealRegister::RegNum savedFirst=TR::RealRegister::fp14;
   TR::RealRegister::RegNum regIndex;

   if (!bodySymbol->isEHAware())
      {
      while (savedFirst<=TR::RealRegister::LastFPR && !machine->getRealRegister(savedFirst)->getHasBeenAssignedInMethod())
         savedFirst=(TR::RealRegister::RegNum)((uint32_t)savedFirst+1);
      }
   for (regIndex=TR::RealRegister::LastFPR; regIndex>=savedFirst; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex-1))
      {
      saveSize = saveSize - 8;
      cursor = generateTrg1MemInstruction(cg(), TR::InstOpCode::lfd, currentNode, machine->getRealRegister(regIndex), new (trHeapMemory()) TR::MemoryReference(sp, saveSize, 8, cg()), cursor);
      }

   savedFirst = TR::RealRegister::gr13;
   if (!bodySymbol->isEHAware())
      {
      while (savedFirst<=TR::RealRegister::LastGPR && !machine->getRealRegister(savedFirst)->getHasBeenAssignedInMethod())
         savedFirst=(TR::RealRegister::RegNum)((uint32_t)savedFirst+1);
      }

   if (savedFirst <= TR::RealRegister::LastGPR)
      {
      if (cg()->comp()->target().cpu.id() == TR_PPCgp || cg()->comp()->target().is64Bit() ||
          (!comp()->getOption(TR_OptimizeForSpace) &&
           TR::RealRegister::LastGPR - savedFirst <= 3))
         for (regIndex=TR::RealRegister::LastGPR; regIndex>=savedFirst; regIndex=(TR::RealRegister::RegNum)((uint32_t)regIndex-1))
            {
            saveSize = saveSize - TR::Compiler->om.sizeofReferenceAddress();
            cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, machine->getRealRegister(regIndex), new (trHeapMemory()) TR::MemoryReference(sp, saveSize, TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
            }
      else
         {
         saveSize = saveSize - (TR::RealRegister::LastGPR - savedFirst + 1) * 4;
         cursor = generateTrg1MemInstruction(cg(), (savedFirst==TR::RealRegister::LastGPR)?TR::InstOpCode::lwz:TR::InstOpCode::lmw, currentNode, machine->getRealRegister(savedFirst), new (trHeapMemory()) TR::MemoryReference(sp, saveSize, 4*(TR::RealRegister::LastGPR-savedFirst+1), cg()), cursor);
         }
      }

   cursor = tempCursor;

   // If only the link area is needed, a stack frame is unnecessary.
   // Some opcodes write into temps above stack, therefore need to allocate
   // stack frame if there are any saved non-volatiles
   //
   if (size > properties.getOffsetToFirstParm() - saveSize)
      {
      if (size > UPPER_IMMED)
         {
         cursor = loadConstant(cg(), currentNode, size, gr11, cursor);
         cursor = generateTrg1Src2Instruction(cg(), TR::InstOpCode::add, currentNode, sp, sp2, gr11, cursor);
         }
      else
         {
         cursor = generateTrg1Src1ImmInstruction(cg(), TR::InstOpCode::addi2, currentNode, sp, sp2, size, cursor);
         }
      }

   if (machine->getLinkRegisterKilled())
      {
      cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, gr0, new (trHeapMemory()) TR::MemoryReference(sp, 2*TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
      cursor = generateSrc1Instruction(cg(), TR::InstOpCode::mtlr, currentNode, gr0, 0, cursor);
      }

   if ( bodySymbol->isEHAware() ||
        machine->getRealRegister(TR::RealRegister::cr2)->getHasBeenAssignedInMethod() ||
        machine->getRealRegister(TR::RealRegister::cr3)->getHasBeenAssignedInMethod() ||
        machine->getRealRegister(TR::RealRegister::cr4)->getHasBeenAssignedInMethod() )
      {
      cursor = generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, currentNode, gr0, new (trHeapMemory()) TR::MemoryReference(sp, TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()), cursor);
      cursor = generateSrc1Instruction(cg(), TR::InstOpCode::mtcr, currentNode, gr0, 0, cursor);
      }

   }

int32_t TR::PPCSystemLinkage::buildArgs(TR::Node *callNode,
                                       TR::RegisterDependencyConditions *dependencies)

   {
   const TR::PPCLinkageProperties &properties = getProperties();
   TR::PPCMemoryArgument *pushToMemory = NULL;
   TR::Register       *tempRegister;
   int32_t   argIndex = 0, memArgs = 0, i;
   int32_t   argSize = 0;
   uint32_t  numIntegerArgs = 0;
   uint32_t  numFloatArgs = 0;
   uint32_t  numVectorArgs = 0;

   TR::Node  *child;
   void     *smark;
   TR::DataType resType = callNode->getType();
   TR_Array<TR::Register *> &tempLongRegisters = cg()->getTransientLongRegisters();
   TR::Symbol * callSymbol = callNode->getSymbolReference()->getSymbol();

   uint32_t firstArgumentChild = callNode->getFirstArgumentIndex();
   bool aix_style_linkage = (cg()->comp()->target().isAIX() || (cg()->comp()->target().is64Bit() && cg()->comp()->target().isLinux()));

   /* Step 1 - figure out how many arguments are going to be spilled to memory i.e. not in registers */
   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntegerArgs >= properties.getNumIntArgRegs())
               memArgs++;
            numIntegerArgs++;
            break;
         case TR::Int64:
            if (cg()->comp()->target().is64Bit())
               {
               if (numIntegerArgs >= properties.getNumIntArgRegs())
                  memArgs++;
               numIntegerArgs++;
               }
            else
               {
               if (aix_style_linkage)
                  {
                  if (numIntegerArgs == properties.getNumIntArgRegs()-1)
                     memArgs++;
                  else if (numIntegerArgs > properties.getNumIntArgRegs()-1)
                     memArgs += 2;
                  }
               else
                  {
                  if (numIntegerArgs & 1)
                     numIntegerArgs++;
                  if (numIntegerArgs >= properties.getNumIntArgRegs())
                     memArgs += 2;
                  }
               numIntegerArgs += 2;
               }
            break;
         case TR::Float:
            if (aix_style_linkage)
               {
               if (numIntegerArgs >= properties.getNumIntArgRegs())
                  memArgs++;
               numIntegerArgs++;
               }
            else
               {
               if (numFloatArgs >= properties.getNumFloatArgRegs())
                  memArgs++;
               }
            numFloatArgs++;
            break;
         case TR::Double:
            if (aix_style_linkage)
               {
               if (cg()->comp()->target().is64Bit())
                  {
                  if (numIntegerArgs >= properties.getNumIntArgRegs())
                     memArgs++;
                  numIntegerArgs++;
                  }
               else
                  {
                  if (numIntegerArgs >= properties.getNumIntArgRegs()-1)
                     memArgs++;
                  numIntegerArgs += 2;
                  }
               }
            else
               {
               if (numFloatArgs >= properties.getNumFloatArgRegs())
                  memArgs++;
               }
            numFloatArgs++;
            break;

         case TR::VectorDouble:
            // TODO : finish implementation
            numVectorArgs++;
            break;
         case TR::Aggregate:
            {
            size_t size = child->getSymbolReference()->getSymbol()->getSize();
            size = (size + sizeof(uintptr_t) - 1) & (~(sizeof(uintptr_t) - 1)); // round up the size
            size_t slots = size / sizeof(uintptr_t);

            if (numIntegerArgs >= properties.getNumIntArgRegs())
               memArgs += slots;
            else
               memArgs += (properties.getNumIntArgRegs() - numIntegerArgs) > slots ? 0: slots - (properties.getNumIntArgRegs() - numIntegerArgs);
            numIntegerArgs += slots;
            }
            break;
         default:
            TR_ASSERT(false, "Argument type %s is not supported\n", child->getDataType().toString());
         }
      }

   // From here, down, any new stack allocations will expire / die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   /* End result of Step 1 - determined number of memory arguments! */
   if (memArgs > 0)
      {
      pushToMemory = new (trStackMemory()) TR::PPCMemoryArgument[memArgs];
      }

   numIntegerArgs = 0;
   numFloatArgs = 0;
   numVectorArgs = 0;

   for (i = firstArgumentChild; i < callNode->getNumChildren(); i++)
      {
      TR::MemoryReference *mref=NULL;
      TR::Register        *argRegister;
      bool                 checkSplit = true;

      child = callNode->getChild(i);
      TR::DataType childType = child->getDataType();

      switch (childType)
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address: // have to do something for GC maps here
               if (childType == TR::Address)
                  argRegister = pushAddressArg(child);
               else
                  argRegister = pushIntegerWordArg(child);

            if (numIntegerArgs < properties.getNumIntArgRegs())
               {

               if (checkSplit && !cg()->canClobberNodesRegister(child, 0))
                  {
                  if (argRegister->containsCollectedReference())
                     tempRegister = cg()->allocateCollectedReferenceRegister();
                  else
                     tempRegister = cg()->allocateRegister();
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                  argRegister = tempRegister;
                  }
               if (numIntegerArgs == 0 &&
                  (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr3);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                  }
               else if (cg()->comp()->target().is32Bit() && numIntegerArgs == 1 && resType.isInt64())
                  {
                  TR::Register *resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr4);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                  }
               else
                  {
                  TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               }
            else // numIntegerArgs >= properties.getNumIntArgRegs()
               {
               mref = getOutgoingArgumentMemRef(argSize, argRegister,TR::InstOpCode::Op_st, pushToMemory[argIndex++], TR::Compiler->om.sizeofReferenceAddress());
               if (!aix_style_linkage)
                  argSize += TR::Compiler->om.sizeofReferenceAddress();
               }
            numIntegerArgs++;
            if (aix_style_linkage)
               argSize += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            argRegister = pushLongArg(child);
            if (!aix_style_linkage)
               {
               if (numIntegerArgs & 1)
                  {
                  if (numIntegerArgs < properties.getNumIntArgRegs())
                     TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  numIntegerArgs++;
                  }
               }
            if (numIntegerArgs < properties.getNumIntArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  if (cg()->comp()->target().is64Bit())
                     {
                     tempRegister = cg()->allocateRegister();
                     generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister);
                     argRegister = tempRegister;
                     }
                  else
                     {
                     tempRegister = cg()->allocateRegister();
                     generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, argRegister->getRegisterPair()->getHighOrder());
                     argRegister = cg()->allocateRegisterPair(argRegister->getRegisterPair()->getLowOrder(), tempRegister);
                     tempLongRegisters.add(argRegister);
                     }
                  }
               if (numIntegerArgs == 0 &&
                   (resType.isAddress() || resType.isInt32() || resType.isInt64()))
                  {
                  TR::Register *resultReg;
                  if (resType.isAddress())
                     resultReg = cg()->allocateCollectedReferenceRegister();
                  else
                     resultReg = cg()->allocateRegister();
                  if (cg()->comp()->target().is64Bit())
                     dependencies->addPreCondition(argRegister, TR::RealRegister::gr3);
                  else
                     dependencies->addPreCondition(argRegister->getRegisterPair()->getHighOrder(), TR::RealRegister::gr3);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr3);
                  }
               else if (cg()->comp()->target().is32Bit() && numIntegerArgs == 1 && resType.isInt64())
                  {
                  TR::Register *resultReg = cg()->allocateRegister();
                  dependencies->addPreCondition(argRegister, TR::RealRegister::gr4);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                  }
               else
                  {
                  if (cg()->comp()->target().is64Bit())
                     TR::addDependency(dependencies, argRegister, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  else
                     TR::addDependency(dependencies, argRegister->getRegisterPair()->getHighOrder(), properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               if (cg()->comp()->target().is32Bit())
                  {
                  if (numIntegerArgs < properties.getNumIntArgRegs()-1)
                     {
                     if (!cg()->canClobberNodesRegister(child, 0))
                        {
                        TR::Register *over_lowReg = argRegister->getRegisterPair()->getLowOrder();
                        tempRegister = cg()->allocateRegister();
                        generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, tempRegister, over_lowReg);
                        argRegister->getRegisterPair()->setLowOrder(tempRegister, cg());
                        }
                     if (numIntegerArgs == 0 && resType.isInt64())
                        {
                        TR::Register *resultReg = cg()->allocateRegister();
                        dependencies->addPreCondition(argRegister->getRegisterPair()->getLowOrder(), TR::RealRegister::gr4);
                        dependencies->addPostCondition(resultReg, TR::RealRegister::gr4);
                        }
                     else
                        TR::addDependency(dependencies, argRegister->getRegisterPair()->getLowOrder(), properties.getIntegerArgumentRegister(numIntegerArgs+1), TR_GPR, cg());
                     }
                  else // numIntegerArgs == properties.getNumIntArgRegs()-1
                     {
                     mref = getOutgoingArgumentMemRef(argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                     }
                  numIntegerArgs ++;
                  }
               }
            else // numIntegerArgs >= properties.getNumIntArgRegs()
               {
               if (cg()->comp()->target().is64Bit())
                  {
                  mref = getOutgoingArgumentMemRef(argSize, argRegister, TR::InstOpCode::std, pushToMemory[argIndex++], TR::Compiler->om.sizeofReferenceAddress());
                  }
               else
                  {
                  if (!aix_style_linkage)
                     argSize = (argSize + 4) & (~7);
                  mref = getOutgoingArgumentMemRef(argSize, argRegister->getRegisterPair()->getHighOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                  mref = getOutgoingArgumentMemRef(argSize+4, argRegister->getRegisterPair()->getLowOrder(), TR::InstOpCode::stw, pushToMemory[argIndex++], 4);
                  numIntegerArgs ++;
                  if (!aix_style_linkage)
                     argSize += 8;
                  }
               }
            numIntegerArgs ++;
            if (aix_style_linkage)
               argSize += 8;
            break;

         case TR::Float:
            argRegister = pushFloatArg(child);
            for (int r = 0; r < ((childType == TR::Float) ? 1: 2); r++)
            {
            TR::Register * argReg;
            if (childType == TR::Float)
               argReg = argRegister;
            else
               argReg = (r == 0) ? argRegister->getHighOrder() : argRegister->getLowOrder();

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argReg);
                  argReg = tempRegister;
                  }
               if ((numFloatArgs == 0 && resType.isFloatingPoint()))
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);

                  dependencies->addPreCondition(argReg, (r == 0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  dependencies->addPostCondition(resultReg, (r == 0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  }
               else
                  TR::addDependency(dependencies, argReg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else if (!aix_style_linkage)
               // numFloatArgs >= properties.getNumFloatArgRegs()
               {
               mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4);
               argSize += 4;
               }

            if (aix_style_linkage)
               {
               if (numIntegerArgs < properties.getNumIntArgRegs())
                  {
                  if (numIntegerArgs==0 && resType.isAddress())
                     {
                     TR::Register *aReg = cg()->allocateRegister();
                     TR::Register *bReg = cg()->allocateCollectedReferenceRegister();
                     dependencies->addPreCondition(aReg, TR::RealRegister::gr3);
                     dependencies->addPostCondition(bReg, TR::RealRegister::gr3);
                     }
                  else
                     TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
                  }
               else // numIntegerArgs >= properties.getNumIntArgRegs()
                  {
                  if (cg()->comp()->target().is64Bit() && cg()->comp()->target().isLinux())
                     {
                     mref = getOutgoingArgumentMemRef(argSize+4, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4);
                     }
                  else
                     {
                     mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfs, pushToMemory[argIndex++], 4);
                     }
                  }

               numIntegerArgs++;
               }
            numFloatArgs++;
            if (aix_style_linkage)
               argSize += TR::Compiler->om.sizeofReferenceAddress();

            }  // for loop
            break;

         case TR::Double:
            argRegister = pushDoubleArg(child);
            for (int r = 0; r < ((childType == TR::Double) ? 1: 2); r++)
            {
            TR::Register * argReg;
            if (childType == TR::Double)
               argReg = argRegister;
            else
               argReg = (r == 0) ? argRegister->getHighOrder() : argRegister->getLowOrder();

            if (numFloatArgs < properties.getNumFloatArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_FPR);
                  generateTrg1Src1Instruction(cg(), TR::InstOpCode::fmr, callNode, tempRegister, argReg);
                  argReg = tempRegister;
                  }
               if (numFloatArgs == 0 && resType.isFloatingPoint())
                  {
                  TR::Register *resultReg;
                  if (resType.getDataType() == TR::Float)
                     resultReg = cg()->allocateSinglePrecisionRegister();
                  else
                     resultReg = cg()->allocateRegister(TR_FPR);
                  dependencies->addPreCondition(argReg, (r==0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  dependencies->addPostCondition(resultReg, (r==0) ? TR::RealRegister::fp1 : TR::RealRegister::fp2);
                  }
               else
                  TR::addDependency(dependencies, argReg, properties.getFloatArgumentRegister(numFloatArgs), TR_FPR, cg());
               }
            else if (!aix_style_linkage)
               // numFloatArgs >= properties.getNumFloatArgRegs()
               {
               argSize = (argSize + 4) & (~7);
               mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8);
               argSize += 8;
               }

            if (aix_style_linkage)
               {
               if (numIntegerArgs < properties.getNumIntArgRegs())
                  {
                  TR::MemoryReference *tempMR;

                  if (numIntegerArgs==0 && resType.isAddress())
                     {
                     TR::Register *aReg = cg()->allocateRegister();
                     TR::Register *bReg = cg()->allocateCollectedReferenceRegister();
                     dependencies->addPreCondition(aReg, TR::RealRegister::gr3);
                     dependencies->addPostCondition(bReg, TR::RealRegister::gr3);
                     }
                  else
                     TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());

                  if (cg()->comp()->target().is32Bit())
                     {
                     if ((numIntegerArgs+1) < properties.getNumIntArgRegs())
                        TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs+1), TR_GPR, cg());
                     else
                        {
                        mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8);
                        }
                     }
                  }
               else // numIntegerArgs >= properties.getNumIntArgRegs()
                  {
                  mref = getOutgoingArgumentMemRef(argSize, argReg, TR::InstOpCode::stfd, pushToMemory[argIndex++], 8);
                  }

               numIntegerArgs += cg()->comp()->target().is64Bit()?1:2;
               }
            numFloatArgs++;
            if (aix_style_linkage)
               argSize += 8;

            }    // end of for loop
            break;
         case TR::VectorDouble:
            argRegister = pushThis(child);
            TR::Register * argReg;
            argReg = argRegister;

            if (numVectorArgs < properties.getNumVectorArgRegs())
               {
               if (!cg()->canClobberNodesRegister(child, 0))
                  {
                  tempRegister = cg()->allocateRegister(TR_VSX_VECTOR);
                  generateTrg1Src2Instruction(cg(), TR::InstOpCode::xxlor, callNode, tempRegister, argReg, argReg);
                  argReg = tempRegister;
                  }
               if (numVectorArgs == 0)
                  {
                  TR::Register *resultReg = cg()->allocateRegister(TR_VSX_VECTOR);
                  dependencies->addPreCondition(argReg, TR::RealRegister::vsr34);
                  dependencies->addPostCondition(resultReg, TR::RealRegister::vsr34);
                  }
               else
                  TR::addDependency(dependencies, argReg, properties.getVectorArgumentRegister(numVectorArgs), TR_VRF, cg());
               }
            else
               {
               TR_ASSERT(false, "not handling in memory vector arguments yet\n");
               }
            numVectorArgs++;
            if (aix_style_linkage)
               argSize += 16;

            break;
         }
      }

   while (numIntegerArgs < properties.getNumIntArgRegs())
      {
      if (numIntegerArgs == 0 && resType.isAddress())
         {
         dependencies->addPreCondition(cg()->allocateRegister(), properties.getIntegerArgumentRegister(0));
         dependencies->addPostCondition(cg()->allocateCollectedReferenceRegister(), properties.getIntegerArgumentRegister(0));
         }
      else
         {
         TR::addDependency(dependencies, NULL, properties.getIntegerArgumentRegister(numIntegerArgs), TR_GPR, cg());
         }
      numIntegerArgs++;
      }

   TR_LiveRegisters *liveRegs;
   bool liveVSXScalar, liveVSXVector, liveVMX;

   liveRegs = cg()->getLiveRegisters(TR_VSX_SCALAR);
   liveVSXScalar = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);
   liveRegs = cg()->getLiveRegisters(TR_VSX_VECTOR);
   liveVSXVector = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);
   liveRegs = cg()->getLiveRegisters(TR_VRF);
   liveVMX = (!liveRegs || liveRegs->getNumberOfLiveRegisters() > 0);

   TR::addDependency(dependencies, NULL, TR::RealRegister::fp0, TR_FPR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr12, TR_GPR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr0, TR_CCR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr1, TR_CCR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr5, TR_CCR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr6, TR_CCR, cg());
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr7, TR_CCR, cg());

#ifdef J9_PROJECT_SPECIFIC
   TR::TreeEvaluator::buildArgsProcessFEDependencies(callNode, cg(), dependencies);
#endif

   int32_t floatRegsUsed = (numFloatArgs>properties.getNumFloatArgRegs())?properties.getNumFloatArgRegs():numFloatArgs;


   bool isHelper = false;
   if (callNode->getSymbolReference()->getReferenceNumber() == TR_PPCVectorLogDouble)
      {
      isHelper = true;
      }

   if (liveVMX || liveVSXScalar || liveVSXVector)
      {
      for (i=(TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::LastFPR+1); i<=TR::RealRegister::LastVSR; i++)
         {
         if (numVectorArgs > 0)
            {
            TR_ASSERT(numVectorArgs == 1, "Handling only one vector argument for now\n");
            if (i == TR::RealRegister::vsr34)
               continue;
            }

         if (!properties.getPreserved((TR::RealRegister::RegNum)i) || !isHelper)
            {
            TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_VSX_SCALAR, cg());
            }

         }
      }

   for (i=(TR::RealRegister::RegNum)((uint32_t)TR::RealRegister::fp0+floatRegsUsed+1); i<=TR::RealRegister::LastFPR; i++)
      {
      if (!properties.getPreserved((TR::RealRegister::RegNum)i) || liveVSXVector)
         {
         TR::addDependency(dependencies, NULL, (TR::RealRegister::RegNum)i, TR_FPR, cg());
         }
      }

   if (memArgs > 0)
      {
      for (argIndex=0; argIndex<memArgs; argIndex++)
         {
         TR::Register *aReg = pushToMemory[argIndex].argRegister;
         generateMemSrc1Instruction(cg(), pushToMemory[argIndex].opCode, callNode, pushToMemory[argIndex].argMemory, aReg);
         cg()->stopUsingRegister(aReg);
         }
      }

   return argSize;
   }

void TR::PPCSystemLinkage::buildDirectCall(TR::Node *callNode,
                                           TR::SymbolReference *callSymRef,
                                           TR::RegisterDependencyConditions *dependencies,
                                           const TR::PPCLinkageProperties &pp,
                                           int32_t argSize)
   {
   TR::Instruction             *gcPoint;
   TR::MethodSymbol               *callSymbol = callSymRef->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol        *sym = callSymbol->getResolvedMethodSymbol();
   bool aix_style_linkage = (cg()->comp()->target().isAIX() || (cg()->comp()->target().is64Bit() && cg()->comp()->target().isLinux()));
   int32_t                        refNum = callSymRef->getReferenceNumber();

   //This is not JIT pseudo TOC, but jit-module system TOC.

   if(aix_style_linkage)
         {
         //Implies TOC needs to be restored (ie not 32bit ppc-linux-be)
         //This is wrong with regard to where system TOC is restored from, but happens to be right for JIT
         //for the time being.
         if(cg()->comp()->target().cpu.isBigEndian())
            {
#if !defined(JITTEST)
            TR::TreeEvaluator::restoreTOCRegister(callNode, cg(), dependencies);
#endif
            }
         else
            {
            //For Little Endian, load target's Global Entry Point into r12, TOC will be restored at branch target.
            TR::Register *geReg = dependencies->searchPreConditionRegister(TR::RealRegister::gr12);
            generateTrg1MemInstruction( cg(),TR::InstOpCode::Op_load, callNode, geReg,
                  new (cg()->trHeapMemory()) TR::MemoryReference(cg()->getTOCBaseRegister(),
                        (refNum-1)*TR::Compiler->om.sizeofReferenceAddress(),
                        TR::Compiler->om.sizeofReferenceAddress(), cg()));
            }
         }

   generateDepImmSymInstruction(cg(), TR::InstOpCode::bl, callNode,
         (uintptr_t)callSymbol->getMethodAddress(),
         dependencies, callSymRef?callSymRef:callNode->getSymbolReference());

   //Bug fix: JIT doesn't need to restore caller's system TOC since there is no infrastructure to generate
   //         ABI-conforming module of dynamic code.  Plus, linux32 has no system TOC.
#if defined(JITTEST)
   TR::TreeEvaluator::restoreTOCRegister(callNode, cg(), dependencies);
#endif
   }


TR::Register *TR::PPCSystemLinkage::buildDirectDispatch(TR::Node *callNode)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();

   const TR::PPCLinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t           argSize = buildArgs(callNode, dependencies);
   TR::Register       *returnRegister;
   buildDirectCall(callNode, callSymRef, dependencies,pp,argSize);

   cg()->machine()->setLinkRegisterKilled(true);
   cg()->setHasCall();

   TR::Register *lowReg=NULL, *highReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icall:
      case TR::acall:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getIntegerReturnRegister());
         break;
      case TR::lcall:
         {
         if (cg()->comp()->target().is64Bit())
            returnRegister = dependencies->searchPostConditionRegister(
                                pp.getLongReturnRegister());
         else
            {
            lowReg  = dependencies->searchPostConditionRegister(
                         pp.getLongLowReturnRegister());
            highReg = dependencies->searchPostConditionRegister(
                         pp.getLongHighReturnRegister());

            returnRegister = cg()->allocateRegisterPair(lowReg, highReg);
            }
         }
         break;
      case TR::fcall:
      case TR::dcall:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getFloatReturnRegister());
         break;
      case TR::vcall:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getVectorReturnRegister());
         break;
      case TR::call:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown direct call Opcode.");
      }

   callNode->setRegister(returnRegister);

   cg()->freeAndResetTransientLongs();
   dependencies->stopUsingDepRegs(cg(), lowReg==NULL?returnRegister:highReg, lowReg);
   return(returnRegister);
   }


void TR::PPCSystemLinkage::buildVirtualDispatch(TR::Node                            *callNode,
                                               TR::RegisterDependencyConditions *dependencies,
                                               uint32_t                            sizeOfArguments)
   {
   bool aix_style_linkage = (cg()->comp()->target().isAIX() || (cg()->comp()->target().is64Bit() && cg()->comp()->target().isLinux()));
   TR_ASSERT(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isComputed(), "system linkage only supports computed indirect call for now %p\n", callNode);
   //We do not support Linux 32bit.
   if(!aix_style_linkage)
      {
      TR_ASSERT(false,"Don't know how to dispatch to 32-bit Linux SystemLinkage\n");
      return;
      }

   //Here we use aix_style_linkage only.

#if defined(DEBUG)
   //Ensure we set dependencies on the following registers when building the arguments to the call.
   //Result register
   TR::Register        *gr3=dependencies->searchPreConditionRegister(TR::RealRegister::gr3);
   TR_ASSERT((gr3 != NULL), "TR::PPCSystemLinkage::buildVirtualDispatch no dependence set on return register gr3.");
#endif

   const TR::PPCLinkageProperties &properties = getProperties();

   //Scratch register.
   TR::Register        *gr0=dependencies->searchPreConditionRegister(TR::RealRegister::gr0);
   //Library TOC Register
   TR::Register        *grTOCReg=TR::TreeEvaluator::retrieveTOCRegister(callNode, cg(), dependencies);
   //Native Stack Pointer (C Stack)
   TR::RealRegister *grSysStackReg=cg()->machine()->getRealRegister(properties.getNormalStackPointerRegister());

   TR_ASSERT((gr0 != NULL),            "TR::PPCSystemLinkage::buildVirtualDispatch no dependence set on scratch register gr0.");
   TR_ASSERT((grTOCReg != NULL),       "TR::PPCSystemLinkage::buildVirtualDispatch no dependence set on Library TOC register gr2.");
   TR_ASSERT((grSysStackReg != NULL),  "TR::PPCSystemLinkage::buildVirtualDispatch no dependence set on Native C Stack register gr1.");

   cg()->evaluate(callNode->getChild(0));
   cg()->decReferenceCount(callNode->getChild(0));

   int32_t callerSaveTOCOffset = (cg()->comp()->target().cpu.isBigEndian() ? 5 : 3) *  TR::Compiler->om.sizeofReferenceAddress();

   TR::Register *targetRegister;
   if (cg()->comp()->target().cpu.isBigEndian())
      {
      // load target from FD
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, gr0, new (trHeapMemory()) TR::MemoryReference(callNode->getChild(0)->getRegister(), 0, TR::Compiler->om.sizeofReferenceAddress(), cg()));

      targetRegister = gr0;
      }
   else
      {
      TR::Register *gr12=dependencies->searchPreConditionRegister(TR::RealRegister::gr12);
      generateTrg1Src1Instruction(cg(), TR::InstOpCode::mr, callNode, gr12, callNode->getChild(0)->getRegister());
      targetRegister = gr12;
      }

   generateSrc1Instruction(cg(), TR::InstOpCode::mtctr, callNode, targetRegister, 0);

   if (cg()->comp()->target().cpu.isBigEndian())
      generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, grTOCReg, new (trHeapMemory()) TR::MemoryReference(callNode->getChild(0)->getRegister(), TR::Compiler->om.sizeofReferenceAddress(), TR::Compiler->om.sizeofReferenceAddress(), cg()));

   generateDepInstruction(cg(), TR::InstOpCode::bctrl, callNode, dependencies);

   // load JIT TOC back
   generateTrg1MemInstruction(cg(),TR::InstOpCode::Op_load, callNode, grTOCReg, new (trHeapMemory()) TR::MemoryReference(grSysStackReg, callerSaveTOCOffset, TR::Compiler->om.sizeofReferenceAddress(), cg()));
   }


TR::Register *TR::PPCSystemLinkage::buildIndirectDispatch(TR::Node *callNode)
   {
   const TR::PPCLinkageProperties &pp = getProperties();
   TR::RegisterDependencyConditions *dependencies =
      new (trHeapMemory()) TR::RegisterDependencyConditions(
         pp.getNumberOfDependencyGPRegisters(),
         pp.getNumberOfDependencyGPRegisters(), trMemory());

   int32_t             argSize = buildArgs(callNode, dependencies);
   TR::Register        *returnRegister;

   buildVirtualDispatch(callNode, dependencies, argSize);
   cg()->machine()->setLinkRegisterKilled(true);
   cg()->setHasCall();

   TR::Register *lowReg=NULL, *highReg;
   switch(callNode->getOpCodeValue())
      {
      case TR::icalli:
      case TR::acalli:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getIntegerReturnRegister());
         break;
      case TR::lcalli:
         {
         if (cg()->comp()->target().is64Bit())
            returnRegister = dependencies->searchPostConditionRegister(
                                pp.getLongReturnRegister());
         else
            {
            lowReg  = dependencies->searchPostConditionRegister(
                         pp.getLongLowReturnRegister());
            highReg = dependencies->searchPostConditionRegister(
                         pp.getLongHighReturnRegister());

            returnRegister = cg()->allocateRegisterPair(lowReg, highReg);
            }
         }
         break;
      case TR::fcalli:
      case TR::dcalli:
         returnRegister = dependencies->searchPostConditionRegister(
                             pp.getFloatReturnRegister());
         if (callNode->getDataType() == TR::Float)
            returnRegister->setIsSinglePrecision();
         break;
      case TR::calli:
         returnRegister = NULL;
         break;
      default:
         returnRegister = NULL;
         TR_ASSERT(0, "Unknown indirect call Opcode.");
      }

   callNode->setRegister(returnRegister);

   cg()->freeAndResetTransientLongs();
   dependencies->stopUsingDepRegs(cg(), lowReg==NULL?returnRegister:highReg, lowReg);
   return(returnRegister);
   }

void TR::PPCSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
   {
   setParameterLinkageRegisterIndex(method, method->getParameterList());
   }

void TR::PPCSystemLinkage::setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parmList)
   {
   ListIterator<TR::ParameterSymbol>   paramIterator(&parmList);
   TR::ParameterSymbol      *paramCursor = paramIterator.getFirst();
   int32_t                  numIntArgs = 0, numFloatArgs = 0;
   const TR::PPCLinkageProperties& properties = getProperties();

   while ( (paramCursor!=NULL) &&
           ( (numIntArgs < properties.getNumIntArgRegs()) ||
             (numFloatArgs < properties.getNumFloatArgRegs()) ) )
      {
      int32_t index = -1;

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            numIntArgs++;
            break;
         case TR::Int64:
            if (numIntArgs<properties.getNumIntArgRegs())
               {
               index = numIntArgs;
               }
            if (cg()->comp()->target().is64Bit())
               numIntArgs ++;
            else
               numIntArgs += 2;
            break;
         case TR::Float:
         case TR::Double:
            if (numFloatArgs<properties.getNumFloatArgRegs())
               {
               index = numFloatArgs;
               }
            numFloatArgs++;
            numIntArgs++;
            break;
         case TR::Aggregate:
            {
            size_t slot = TR::Compiler->om.sizeofReferenceAddress();
            size_t size = (paramCursor->getSize() + slot - 1) & (~(slot - 1));
            size_t slots = size / slot;
            numIntArgs += slots;
            }
            break;
         default:
            TR_ASSERT(false, "assertion failure");
            break;
         }
      paramCursor->setLinkageRegisterIndex(index);
      paramCursor = paramIterator.getNext();
      }

   }

intptr_t TR::PPCSystemLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

intptr_t TR::PPCSystemLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

