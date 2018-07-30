/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"

#include <limits.h>                                 // for INT_MAX
#include <stddef.h>                                 // for NULL
#include <stdint.h>                                 // for int32_t, int64_t
#include "codegen/CodeGenerator.hpp"                // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                     // for feGetEnv
#include "codegen/Machine.hpp"                      // for Machine
#include "codegen/RealRegister.hpp"                 // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"            // for RecognizedMethod, etc
#include "codegen/Register.hpp"                     // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterPair.hpp"                 // for RegisterPair
#include "codegen/TreeEvaluator.hpp"                // for TreeEvaluator, etc
#include "compile/Compilation.hpp"                  // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"              // for TR::Options, etc
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for TR::DataType, etc
#include "il/ILOpCodes.hpp"                         // for ILOpCodes::d2l, etc
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node, vcount_t
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector, etc
#include "infra/Link.hpp"                           // for TR_LinkHead
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "x/codegen/IntegerMultiplyDecomposer.hpp"
#include "x/codegen/X86Evaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                     // for ::MOV4RegReg

OMR::X86::I386::CodeGenerator::CodeGenerator() :
   OMR::X86::CodeGenerator()
   {
   // Common X86 initialization
   //
   self()->initialize( self()->comp() );

   self()->setUsesRegisterPairsForLongs();

   if (debug("supportsArrayTranslateAndTest"))
      self()->setSupportsArrayTranslateAndTest();
   if (debug("supportsArrayCmp"))
      self()->setSupportsArrayCmp();

   self()->setSupportsDoubleWordCAS();
   self()->setSupportsDoubleWordSet();

   if (TR::Compiler->target.isWindows())
      {
      if (self()->comp()->getOptions()->getOption(TR_DisableTraps))
         {
         _numberBytesReadInaccessible = 0;
         _numberBytesWriteInaccessible = 0;
         }
      else
         {
         _numberBytesReadInaccessible = 4096;
         _numberBytesWriteInaccessible = 4096;
         self()->setHasResumableTrapHandler();
         self()->setEnableImplicitDivideCheck();
         }
      self()->setSupportsDivCheck();
      self()->setJNILinkageCalleeCleanup();

      // The default CTM behaviour is to do the conversion via X87 instructions.
      //
      static char *dontUseGPRsForWin32CTMConversion = feGetEnv("TR_DontUseGPRsForWin32CTMConversion");
      if (!dontUseGPRsForWin32CTMConversion)
         {
         self()->setUseGPRsForWin32CTMConversion();
         }

      static char *useLongDivideHelperForWin32CTMConversion = feGetEnv("TR_UseLongDivideHelperForWin32CTMConversion");
      if (useLongDivideHelperForWin32CTMConversion)
         {
         self()->setUseLongDivideHelperForWin32CTMConversion();
         }

      self()->setRealVMThreadRegister(self()->machine()->getX86RealRegister(TR::RealRegister::ebp));
      }
   else if (TR::Compiler->target.isLinux())
      {
      if (self()->comp()->getOptions()->getOption(TR_DisableTraps))
         {
         _numberBytesReadInaccessible = 0;
         _numberBytesWriteInaccessible = 0;
         }
      else
         {
         _numberBytesReadInaccessible = 4096;
         _numberBytesWriteInaccessible = 4096;
         self()->setHasResumableTrapHandler();
         self()->setEnableImplicitDivideCheck();
         }
      self()->setRealVMThreadRegister(self()->machine()->getX86RealRegister(TR::RealRegister::ebp));
      self()->setSupportsDivCheck();
      }
   else
      {
      TR_ASSERT(0, "unknown target");
      }

   self()->setSupportsInlinedAtomicLongVolatiles();

   static char *dontConsiderAllAutosForGRA = feGetEnv("TR_dontConsiderAllAutosForGRA");
   if (!dontConsiderAllAutosForGRA)
      self()->setConsiderAllAutosAsTacticalGlobalRegisterCandidates();

   }


TR::Register *
OMR::X86::I386::CodeGenerator::longClobberEvaluate(TR::Node *node)
   {
   TR_ASSERT(node->getOpCode().is8Byte(), "assertion failure");
   if (node->getReferenceCount() > 1)
      {
      TR::Register     *temp    = self()->evaluate(node);
      TR::Register     *lowReg  = self()->allocateRegister();
      TR::Register     *highReg = self()->allocateRegister();
      TR::RegisterPair *longReg = self()->allocateRegisterPair(lowReg, highReg);

      generateRegRegInstruction(MOV4RegReg, node, lowReg, temp->getLowOrder(), self());
      generateRegRegInstruction(MOV4RegReg, node, highReg, temp->getHighOrder(), self());
      return longReg;
      }
   else
      {
      return self()->evaluate(node);
      }
   }


TR_GlobalRegisterNumber
OMR::X86::I386::CodeGenerator::pickRegister(
      TR_RegisterCandidate *rc,
      TR::Block **allBlocks,
      TR_BitVector &availableRegisters,
      TR_GlobalRegisterNumber &highRegisterNumber,
      TR_LinkHead<TR_RegisterCandidate> *candidates)
   {
   if (!self()->comp()->getOptions()->getOption(TR_DisableRegisterPressureSimulation))
      {
      if (self()->comp()->getOption(TR_AssignEveryGlobalRegister))
         {
         // This is not really necessary except for testing purposes.
         // Conceptually, the common pickRegister code should be free to make
         // its choices based only on performance considerations, and shouldn't
         // need to worry about correctness.  When SupportsVMThreadGRA is not set,
         // it is incorrect to choose the VMThread register.  Therefore we mask
         // it out here.
         //
         // Having said that, the common code *does* already mask out the
         // VMThread register for convenience, so under normal circumstances,
         // this code is redundant.  It is only necessary when
         // TR_AssignEveryGlobalRegister is set.
         //
         availableRegisters -= *self()->getGlobalRegisters(TR_vmThreadSpill, self()->comp()->getMethodSymbol()->getLinkageConvention());
         }
      return OMR::CodeGenerator::pickRegister(rc, allBlocks, availableRegisters, highRegisterNumber, candidates);
      }

   if ((rc->getSymbol()->getDataType() == TR::Float) ||
       (rc->getSymbol()->getDataType() == TR::Double))
      {
      if (availableRegisters.get(7))
         return 7;
      if (availableRegisters.get(8))
         return 8;
      if (availableRegisters.get(9))
         return 9;
      if (availableRegisters.get(10))
         return 10;
      if (availableRegisters.get(11))
         return 11;
      if (availableRegisters.get(12))
         return 12;

      return -1;
      }


   if (!_assignedGlobalRegisters)
      _assignedGlobalRegisters = new (self()->trStackMemory()) TR_BitVector(self()->comp()->getSymRefCount(), self()->trMemory(), stackAlloc, growable);

   if (availableRegisters.get(5))
      return 5; // esi

   if (availableRegisters.get(2))
      return 2; // ecx

   static char *dontUseEBXasGPR = feGetEnv("dontUseEBXasGPR");
   if (!dontUseEBXasGPR && availableRegisters.get(1))
      return 1;

#ifdef J9_PROJECT_SPECIFIC
   TR::RecognizedMethod rm = self()->comp()->getMethodSymbol()->getRecognizedMethod();
   if (rm == TR::java_util_HashtableHashEnumerator_hasMoreElements)
      {
      if (availableRegisters.get(4))
         return 4; // edi
      if (availableRegisters.get(3))
         return 3; // edx
      }
   else
#endif
      {
      int32_t numExtraRegs = 0;
      int32_t maxRegisterPressure = 0;

      vcount_t visitCount = self()->comp()->incVisitCount();
      TR_BitVectorIterator bvi(rc->getBlocksLiveOnEntry());
      int32_t maxFrequency = 0;
      while (bvi.hasMoreElements())
         {
         int32_t liveBlockNum = bvi.getNextElement();
         TR::Block *block = allBlocks[liveBlockNum];
         if (block->getFrequency() > maxFrequency)
             maxFrequency = block->getFrequency();
         }

      int32_t maxStaticFrequency = 0;
      if (maxFrequency == 0)
         {
         bvi.setBitVector(rc->getBlocksLiveOnEntry());
         while (bvi.hasMoreElements())
            {
            int32_t liveBlockNum = bvi.getNextElement();
            TR::Block *block = allBlocks[liveBlockNum];
            TR_BlockStructure *blockStructure = block->getStructureOf();
            int32_t blockWeight = 1;
            if (blockStructure &&
                !block->isCold())
               {
               blockStructure->calculateFrequencyOfExecution(&blockWeight);
               if (blockWeight > maxStaticFrequency)
                  maxStaticFrequency = blockWeight;
               }
            }
         }

      bool assigningEDX = false;
      if (!availableRegisters.get(4) &&
          availableRegisters.get(3))
         assigningEDX = true;

      bool vmThreadUsed = false;

      bvi.setBitVector(rc->getBlocksLiveOnEntry());
      while (bvi.hasMoreElements())
         {
         int32_t liveBlockNum = bvi.getNextElement();
         TR::Block *block = allBlocks[liveBlockNum];

         _assignedGlobalRegisters->empty();
         int32_t numAssignedGlobalRegs = 0;
         TR_RegisterCandidate *prev;
         for (prev = candidates->getFirst(); prev; prev = prev->getNext())
            {
            bool gprCandidate = true;
            if ((prev->getSymbol()->getDataType() == TR::Float) ||
                (prev->getSymbol()->getDataType() == TR::Double))
               gprCandidate = false;
            if (gprCandidate && prev->getBlocksLiveOnEntry().get(liveBlockNum))
               {
               numAssignedGlobalRegs++;
               if (prev->getDataType() == TR::Int64)
                  numAssignedGlobalRegs++;
               _assignedGlobalRegisters->set(prev->getSymbolReference()->getReferenceNumber());
               }
            }

         maxRegisterPressure = self()->estimateRegisterPressure(block, visitCount, maxStaticFrequency, maxFrequency, vmThreadUsed, numAssignedGlobalRegs, _assignedGlobalRegisters, rc->getSymbolReference(), assigningEDX);

         if (maxRegisterPressure >= self()->comp()->cg()->getMaximumNumbersOfAssignableGPRs())
            break;
         }

      // Determine if we can spare any extra registers for this candidate without spilling
      // in any hot (critical) blocks
      //
      if (maxRegisterPressure < self()->comp()->cg()->getMaximumNumbersOfAssignableGPRs())
         numExtraRegs = self()->comp()->cg()->getMaximumNumbersOfAssignableGPRs() - maxRegisterPressure;

      //dumpOptDetails("For global register candidate %d reg pressure is %d maxRegs %d numExtraRegs %d\n", rc->getSymbolReference()->getReferenceNumber(), maxRegisterPressure, comp()->cg()->getMaximumNumbersOfAssignableGPRs(), numExtraRegs);

      if (numExtraRegs > 0)
         {
         if (availableRegisters.get(4))
            return 4; // edi

         if (availableRegisters.get(3))
            return 3; // edx
         }
      }

   return -1; // -1 ==> don't use a global register
   }

int32_t
OMR::X86::I386::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *node)
   {
   // TODO: Currently, lookupEvaluator doesn't deal properly with different
   // glRegDeps on different cases of a lookupswitch.
   //
   static const char *enableLookupswitch = feGetEnv("TR_enableGRAAcrossLookupSwitch");
   if (!enableLookupswitch && node->getOpCode().getOpCodeValue()==TR::lookup)
      return 1;

   if (node->getOpCode().getOpCodeValue()==TR::table)
      {
      // 1 for jump table base reg, which is not apparent in the trees
      // 1 for ebp when it is needed for the VMThread
      //
      return self()->getNumberOfGlobalGPRs() - 2;
      }

   if (node->getOpCode().isIf())
      {
      // we run out of all but one/two registers in these cases
      //
      if (node->getFirstChild()->getType().isInt64())
         {
         if (node->getOpCode().isBranch())
            {
            TR::Node *firstChild = node->getFirstChild();
            TR::Node *secondChild = node->getSecondChild();
            int extraRegsAvailable = 0;


            if(firstChild->getOpCodeValue() == TR::d2l ||
               secondChild->getOpCodeValue() == TR::d2l)
               {
               return 1;
               }

            if ((firstChild->getReferenceCount() == 1 &&
                firstChild->getOpCode().isLoadVarDirect()) ||
                (secondChild->getReferenceCount() == 1 &&
                 firstChild->getOpCode().isLoadVarDirect()))
               extraRegsAvailable += 0; // TODO: put it back to 2 when looking at GRA, GRA pushes allocation of 8 registers

            return 2 + extraRegsAvailable;
            }
         else
            {
            // TR_lcmpXX opcodes take up 5 regs
            //
            return 1;
            }
         }

      // we run out of all but one register in these cases....last time I tried....
      //

      if (node->getFirstChild()->getOpCodeValue() == TR::instanceof)
         {
         if (!TR::TreeEvaluator::instanceOfOrCheckCastNeedSuperTest(node->getFirstChild(), self())  &&
             TR::TreeEvaluator::instanceOfOrCheckCastNeedEqualityTest(node->getFirstChild(), self()))
            return self()->getNumberOfGlobalGPRs() - 4; // ebp plus three other regs if vft masking is enabled
         else
            return 0;
         }

      // All other conditional branches, we usually need one reg for the compare and possibly one for the vmthread
      //return getNumberOfGlobalGPRs() - 1 - (node->isVMThreadRequired()? 1 : 0);
      // vmThread required might be set on a node after GRA has ran
      return self()->getNumberOfGlobalGPRs() - 2;
      }

   return INT_MAX;
   }


bool
OMR::X86::I386::CodeGenerator::codegenMulDecomposition(int64_t multiplier)
   {
   bool iMulDecomposeReport = self()->comp()->getOptions()->getTraceSimplifier(TR_TraceMulDecomposition);
   bool answer = false;
   if (iMulDecomposeReport)
      diagnostic("\nCodegen was queried for the value of %d, ", (int32_t)multiplier);
   answer = TR_X86IntegerMultiplyDecomposer::hasDecomposition (multiplier);

   if (iMulDecomposeReport)
      diagnostic("codegen said [%c]\n", (answer)?'Y':'N');

   return answer;
   }
