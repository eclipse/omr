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

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
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
#include "ras/Debug.hpp"
#include "ras/Delimiter.hpp"
#include "z/codegen/S390Evaluator.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/SystemLinkage.hpp"

TR::SystemLinkage::SystemLinkage(TR::CodeGenerator* cg, TR_S390LinkageConventions elc, TR_LinkageConventions lc)
   :
      TR::Linkage(cg, elc,lc),
      _GPRSaveMask(0),
      _FPRSaveMask(0)
   {
   }

TR::SystemLinkage *
TR::SystemLinkage::self()
   {
   return static_cast<TR::SystemLinkage *>(this);
   }

void
TR::SystemLinkage::initS390RealRegisterLinkage()
   {
   int32_t icount = 0, ret_count=0;

   TR::RealRegister * spReal  = getStackPointerRealRegister();
   spReal->setState(TR::RealRegister::Locked);
   spReal->setAssignedRegister(spReal);
   spReal->setHasBeenAssignedInMethod(true);

   // set register weight
   for (icount = TR::RealRegister::FirstGPR; icount >= TR::RealRegister::LastAssignableGPR; icount++)
      {
      int32_t weight;
      if (getIntegerReturn((TR::RealRegister::RegNum) icount))
         {
         weight = ++ret_count;
         }
      else if (getPreserved((TR::RealRegister::RegNum) icount))
         {
         weight = 0xf000 + icount;
         }
      else
         {
         weight = icount;
         }
      cg()->machine()->getRealRegister((TR::RealRegister::RegNum) icount)->setWeight(weight);
      }
   }


// Non-Java use
void
TR::SystemLinkage::initParamOffset(TR::ResolvedMethodSymbol * method, int32_t stackIndex, List<TR::ParameterSymbol> *parameterList)
{
   ListIterator<TR::ParameterSymbol> parameterIterator((parameterList != NULL)? parameterList : &method->getParameterList());
   parameterIterator.reset();
   TR::ParameterSymbol * parmCursor = parameterIterator.getFirst();
   int8_t gprSize = cg()->machine()->getGPRSize();

   int32_t paramNum = -1;
   setIncomingParmAreaBeginOffset(stackIndex);

   while (parmCursor != NULL)
      {
      paramNum++;

      int8_t lri = parmCursor->getLinkageRegisterIndex();
      int32_t parmSize = parmCursor->getSize();
      if (isXPLinkLinkageType())
         {
         parmCursor->setParameterOffset(stackIndex);
         // parameters aligned on GPR sized boundaries - set for next
         int32_t alignedSize = parmCursor->getSize();
         alignedSize = ((alignedSize + gprSize - 1)/gprSize)*gprSize;
         stackIndex += alignedSize;
         }
      else if (lri == -1)
         {
         parmCursor->setParameterOffset(stackIndex);
         stackIndex += std::max<int32_t>(parmCursor->getSize(), cg()->machine()->getGPRSize());
         }
      else
         { // linkage parm registers reserved in caller save area
         TR::RealRegister::RegNum argRegNum;
         if (parmCursor->getType().isFloatingPoint())
             argRegNum = getFloatArgumentRegister(lri);
         else
             argRegNum = getIntegerArgumentRegister(lri);
         parmCursor->setParameterOffset(getStackFrameSize() + getRegisterSaveOffset(REGNUM(argRegNum)));
         }

      if (isSmallIntParmsAlignedRight() && parmCursor->getType().isIntegral() && (gprSize > parmCursor->getSize())) // WCode motivated
         {
         // linkage related
         parmCursor->setParameterOffset(parmCursor->getParameterOffset() + gprSize - parmCursor->getSize());
         }

      parmCursor = parameterIterator.getNext();
      }
   setIncomingParmAreaEndOffset(stackIndex);
}

/**
 * General utility
 * Reverse the bit ordering of the save mask
 */
uint16_t
TR::SystemLinkage::flipBitsRegisterSaveMask(uint16_t mask)
   {
   TR_ASSERT(NUM_S390_GPR == NUM_S390_FPR, "need special flip bits for FPR");
   uint16_t i, outputMask;

   outputMask = 0;
   for (i = 0; i < NUM_S390_GPR; i++)
       {
       if (mask & (1 << i))
          {
          outputMask |= 1 << (NUM_S390_GPR-1-i);
          }
       }
   return outputMask;
   }

/**
 * Front-end customization of callNativeFunction
 */
void
TR::SystemLinkage::generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   TR_ASSERT(0,"Different system types should have their own implementation.");
   }

/**
 * Call System routine
 * @return return value will be return value from system routine copied to private linkage return reg
 */
TR::Register *
TR::SystemLinkage::callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptr_t targetAddress,
      TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
      TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint)
   {
   return 0;
   }

void
TR::SystemLinkage::mapStack(TR::ResolvedMethodSymbol * method)
  	{
		{
     	mapStack(method,0);
  		}
  	}

/**
 * This function will intitialize all the offsets from frame pointer
 * i.e. stackPointer value before buying stack for method being jitted
 * The offsets will be fixed up (at the end /in createPrologue) when the stackFrameSize is known
 */
void
TR::SystemLinkage::mapStack(TR::ResolvedMethodSymbol * method, uint32_t stackIndex)
   {
   int32_t i;

// ====== COMPLEXITY ALERT:  (TODO: fix this nonsense: map stack positively)
// This method is called twice - once with a 0 value of stackIndex and secondly with the stack size.
// This adds complexities:
//     -  alignment: ensuring things are aligned properly
//     -  item layout is done in a reverse order : those things mapped first here are at higher addresses
//        end offsets are really begin offsets and vice versa.
// ====== COMPLEXITY ALERT

   setStackSizeCheckNeeded(true);


   // process incomming parameters
   if (!isZLinuxLinkageType())
       {
       initParamOffset(method,getOutgoingParmAreaBeginOffset()+ stackIndex);
       }

   // Now map the locals
   //
   setLocalsAreaBeginOffset(stackIndex);
   ListIterator<TR::AutomaticSymbol> automaticIterator(&method->getAutomaticList());
   TR::AutomaticSymbol * localCursor = automaticIterator.getFirst();
   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   while (localCursor != NULL)
      {
      if (!TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }

   automaticIterator.reset();
   localCursor = automaticIterator.getFirst();

   // align double - but there is more to do to align the stack in general as double.
   while (localCursor != NULL)
      {
      if (TR::Linkage::needsAlignment(localCursor->getDataType(), cg()))
         {
         mapSingleAutomatic(localCursor, stackIndex);
         }
      localCursor = automaticIterator.getNext();
      }


   // Force the stack size to be increased by...
   stackIndex -= (comp()->getOptions()->get390StackBufferSize()/4)*4;
   stackIndex -= (stackIndex & 0x4) ? 4 : 0;

   if (true)
      {
      //Save used Preserved FPRs
      setFPRSaveAreaBeginOffset(stackIndex);
      int16_t FPRSaveMask = 0;
      for (i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; ++i)
         {
         if (getPreserved(REGNUM(i)) && ((getRealRegister(REGNUM(i)))->getHasBeenAssignedInMethod()))
            {
            FPRSaveMask |= 1 << (i - TR::RealRegister::FirstFPR);
            stackIndex -= cg()->machine()->getFPRSize();
            }
         }

      setFPRSaveMask(FPRSaveMask);

      if (FPRSaveMask != 0 && isOSLinkageType())
         {
         #define DELTA_ALIGN(x, align) ((x & (align-1)) ? (align -((x)&(align-1))) : 0)
         stackIndex -= DELTA_ALIGN(stackIndex, 16);
         }
      setFPRSaveAreaEndOffset(stackIndex);
      }

   // Map slot for long displacement
   if (comp()->target().isLinux())
      {
      // Linux on Z has a special reserved slot in the linkage convention
      setOffsetToLongDispSlot(comp()->target().is64Bit() ? 8 : 4);
      }
   else
      {
      setOffsetToLongDispSlot(stackIndex -= 16);
      }

   if (comp()->getOption(TR_TraceCG))
      traceMsg(comp(), "\n\nOffsetToLongDispSlot = %d\n", getOffsetToLongDispSlot());

   if (isZLinuxLinkageType())
      {
      initParamOffset(method,getOutgoingParmAreaBeginOffset()+ stackIndex);
      }

   method->setLocalMappingCursor(stackIndex);

   stackIndex -= (stackIndex & 0x4) ? 4 : 0;
   setLocalsAreaEndOffset(stackIndex);
   }

void TR::SystemLinkage::mapSingleAutomatic(TR::AutomaticSymbol * p, uint32_t & stackIndex)
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

   align = (align == 0) ? 1 : align;

   // we map in a backwards fashion
   int32_t roundup = align - 1;
   stackIndex = (stackIndex-size) & (~roundup);
   p->setOffset(stackIndex);
   }

bool TR::SystemLinkage::hasToBeOnStack(TR::ParameterSymbol * parm)
   {
   return parm->getAssignedGlobalRegisterIndex() >=  0 &&  parm->isParmHasToBeOnStack();
   }

intptr_t TR::SystemLinkage::entryPointFromCompiledMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }

intptr_t TR::SystemLinkage::entryPointFromInterpretedMethod()
   {
   return reinterpret_cast<intptr_t>(cg()->getCodeStart());
   }
