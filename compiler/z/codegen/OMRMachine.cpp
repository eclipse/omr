/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRZMachine#C")
#pragma csect(STATIC,"OMRZMachine#S")
#pragma csect(TEST,"OMRZMachine#T")


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Machine_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "infra/Random.hpp"
#include "infra/Stack.hpp"
#include "codegen/SystemLinkage.hpp"
#include "ras/Debug.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"


// Register Association
#define S390_REGISTER_HEAVIEST_WEIGHT               0x0000ffff
#define S390_REGISTER_INTERFERENCE_WEIGHT           0x00008000
#define S390_REGISTER_INITIAL_PRESERVED_WEIGHT      0x00001000
#define S390_REGISTER_ASSOCIATED_WEIGHT             0x00000800
#define S390_REGISTER_PLACEHOLDER_WEIGHT            0x00000100
#define S390_REGISTER_LOW_PRIORITY_WEIGHT           0x000000C0
#define S390_REGISTER_BASIC_WEIGHT                  0x00000080
#define S390_REGISTER_PRESERVED_WEIGHT              0x00000002
#define S390_REGISTER_PAIR_SIBLING                  0x00008000

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine memeber functions
////////////////////////////////////////////////////////////////////////////////

TR_Debug *
OMR::Z::Machine::getDebug()
   {
   return self()->cg()->getDebug();
   }


static const char *
getRegisterName(TR::Register * reg, TR::CodeGenerator * cg)
   {
   if (cg->getDebug() && reg)
      return cg->getDebug()->getName(reg);
   else
      return "NULL";
   }

#define regName(reg)  getRegisterName(reg,cg())

///////////////////////////////////////////////////////////////////////////////
//  Copy Register by moving source register to target register
///////////////////////////////////////////////////////////////////////////////
TR::Instruction *
OMR::Z::Machine::registerCopy(TR::CodeGenerator* cg,
      TR_RegisterKinds rk,
      TR::RealRegister* targetReg,
      TR::RealRegister* sourceReg,
      TR::Instruction* precedingInstruction)
   {
   // TODO: In reality we should be validating that the the source is NULL and target is non-NULL however we cannot
   // currently do this because the registerCopy and registerExchange APIs are not updating the register states as
   // registers are shuffled around.
   //
   // A typical example of when this assert would fire is when attempting a register exchange (which uses this API)
   // via a spare reigster. We will generate 3 register shuffles but the register state is only updated later, thus
   // during the second shuffle the spare register will the the target in the above parameter and as such we would
   // assert since the spare register presumably did not have any virtual register associated with it.
   TR_ASSERT_FATAL(sourceReg->getAssignedRegister() != NULL || targetReg->getAssignedRegister() != NULL, "Attempting register copy with source (%s) and target (%s) real registers not corresponding to any virtual register", getRegisterName(sourceReg, cg), getRegisterName(targetReg, cg));

   TR::Node* node = precedingInstruction->getNode();
   TR::Instruction* cursor = NULL;

   switch (rk)
      {
      case TR_GPR:
         {
         // TODO: Once above is fixed we can always rely on targetReg->getAssignedRegister()
         auto nonNullAssignedReg = targetReg->getAssignedRegister() != NULL ?
            targetReg->getAssignedRegister() :
            sourceReg->getAssignedRegister();

         auto mnemonic = nonNullAssignedReg->is64BitReg() ?
            TR::InstOpCode::LGR :
            TR::InstOpCode::LR;

         cursor = generateRRInstruction(cg, mnemonic, node, targetReg, sourceReg, precedingInstruction);
         break;
         }
      case TR_FPR:
         cursor = generateRRInstruction(cg, TR::InstOpCode::LDR, node, targetReg, sourceReg, precedingInstruction);
         break;
      case TR_VRF:
         cursor = generateVRRaInstruction(cg, TR::InstOpCode::VLR, node, targetReg, sourceReg, precedingInstruction);
         break;
      }

   cg->traceRAInstruction(cursor);

   TR_Debug* debug = cg->getDebug();

   if (debug)
      {
      debug->addInstructionComment(cursor, "Register copy");
      }

   return cursor;
   }


/**
 * Exchange the contents of two registers
 */
TR::Instruction *
OMR::Z::Machine::registerExchange(TR::CodeGenerator* cg,
      TR_RegisterKinds rk,
      TR::RealRegister* targetReg,
      TR::RealRegister* sourceReg,
      TR::RealRegister* middleReg,
      TR::Instruction* precedingInstruction)
   {
   TR_ASSERT_FATAL(sourceReg->getAssignedRegister() != NULL, "Attempting register exchange with source real register (%s) not corresponding to any virtual register", getRegisterName(sourceReg, cg));
   TR_ASSERT_FATAL(targetReg->getAssignedRegister() != NULL, "Attempting register exchange with target real register (%s) not corresponding to any virtual register", getRegisterName(targetReg, cg));

   // middleReg is not used if rk==TR_GPR.
   TR::Compilation *comp = cg->comp();
   TR::Node * currentNode = precedingInstruction->getNode();
   TR::Instruction * currentInstruction = NULL;
   const char * REG_EXCHANGE = "LR=Reg_exchg";
   const char * REG_PAIR     = "LR=Reg_pair";
   TR_Debug * debugObj = cg->getDebug();
   TR::Machine *machine = cg->machine();

   // exchange floating point registers
   if (rk == TR_FPR)
      {
      if (middleReg != NULL)
         {
         middleReg->setHasBeenAssignedInMethod(true);

         currentInstruction = machine->registerCopy(cg, rk, sourceReg, middleReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(cg, rk, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(cg, rk, middleReg, targetReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         }
      else
         {
         TR::Instruction * currentInstruction = precedingInstruction;
         TR_BackingStore * location;
         location = cg->allocateSpill(8, false, NULL);
         TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), cg);
         location->getSymbolReference()->getSymbol()->setSpillTempLoaded();

         currentInstruction = generateRXInstruction(cg, TR::InstOpCode::STD, currentNode, targetReg, tempMR, currentInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = generateRRInstruction(cg, TR::InstOpCode::LDR, currentNode, targetReg, sourceReg, currentInstruction);
         cg->traceRAInstruction(currentInstruction);
         TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, 0, cg);
         currentInstruction = generateRXInstruction(cg, TR::InstOpCode::LD, currentNode, sourceReg, tempMR2, currentInstruction);
         cg->traceRAInstruction(currentInstruction);

         cg->freeSpill(location, 8, 0);
         }

      cg->generateDebugCounter("RegisterAllocator/Exchange/FPR", 1, TR::DebugCounter::Free);
      }
   // exchange vector registers
   else if (rk == TR_VRF)
      {
      // Uses naive temp move and if none available, uses memory.
      if (middleReg != NULL)
         {
         middleReg->setHasBeenAssignedInMethod(true);

         currentInstruction = machine->registerCopy(cg, rk, sourceReg, middleReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(cg, rk, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(cg, rk, middleReg, targetReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         }
      else
         {
         TR_BackingStore * location;
         location = cg->allocateSpill(16, false, NULL);
         TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), cg);
         location->getSymbolReference()->getSymbol()->setSpillTempLoaded();

         currentInstruction = generateVRXInstruction(cg, TR::InstOpCode::VST, currentNode, targetReg, tempMR, 0, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = generateVRRaInstruction(cg, TR::InstOpCode::VLR, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, 0, cg);
         currentInstruction = generateVRXInstruction(cg, TR::InstOpCode::VL, currentNode, sourceReg, tempMR2, 0, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);

         cg->freeSpill(location, 16, 0);
         }

      cg->generateDebugCounter("RegisterAllocator/Exchange/VRF", 1, TR::DebugCounter::Free);
      }
   else
      {
      TR::InstOpCode::Mnemonic opLoadReg = TR::InstOpCode::bad;
      TR::InstOpCode::Mnemonic opLoad = TR::InstOpCode::bad;
      TR::InstOpCode::Mnemonic opStore = TR::InstOpCode::bad;

      if (targetReg->getAssignedRegister()->is64BitReg() || sourceReg->getAssignedRegister()->is64BitReg())
         {
         opLoadReg = TR::InstOpCode::LGR;
         opLoad = TR::InstOpCode::LG;
         opStore = TR::InstOpCode::STG;
         }
      else
         {
         opLoadReg = TR::InstOpCode::LR;
         opLoad = TR::InstOpCode::L;
         opStore = TR::InstOpCode::ST;
         }

      // exchange general purpose registers
      //
      if (middleReg == NULL)
         {
         // Nasty OSC... should happen very rarely.  Only in cases where deps reach critical mass in reg table
         // and last dep resolution requires a regexch from 'blocked' state.
         //
         TR::Instruction * currentInstruction = precedingInstruction;
         TR_BackingStore * location;

         if (targetReg->getAssignedRegister()->is64BitReg() || sourceReg->getAssignedRegister()->is64BitReg())
            {
            location = cg->allocateSpill(8, false, NULL);
            }
         else
            {
            location = cg->allocateSpill(4, false, NULL);
            }
         TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), cg);
         location->getSymbolReference()->getSymbol()->setSpillTempLoaded();

         currentInstruction = generateRXInstruction(cg, opLoad, currentNode, sourceReg, tempMR, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = generateRRInstruction(cg, opLoadReg, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, 0, cg);
         currentInstruction = generateRXInstruction(cg, opStore, currentNode, targetReg, tempMR2, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);

         cg->freeSpill(location, TR::Compiler->om.sizeofReferenceAddress(), 0);
         }
      else
         {
         middleReg->setHasBeenAssignedInMethod(true);

         currentInstruction =
            generateRRInstruction(cg, opLoadReg, currentNode, sourceReg, middleReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
            }

         currentInstruction =
            generateRRInstruction(cg, opLoadReg, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
            }

         currentInstruction =
            generateRRInstruction(cg, opLoadReg, currentNode, middleReg, targetReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         if (debugObj)
            {
            debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
            }
         }

      cg->generateDebugCounter(precedingInstruction, "RegisterAllocator/Exchange/GPR", 1, TR::DebugCounter::Free);
      }

   if (middleReg)
      {
      cg->traceRegisterAssignment("registerExchange: %R and %R via %R", targetReg, sourceReg, middleReg);
      }
   else
      {
      cg->traceRegisterAssignment("registerExchange: %R and %R via memory", targetReg, sourceReg);
      }

   return currentInstruction;
   }

static bool
boundNext(TR::Instruction * currentInstruction, int32_t realNum, TR::Register * virtReg)
   {
   TR::Instruction * cursor = currentInstruction;
   TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum) realNum;
   TR::Node * nodeBBStart = NULL;

   while (cursor->getOpCodeValue() != TR::InstOpCode::proc)
      {
      TR::RegisterDependencyConditions * conditions;
      if ((conditions = cursor->getDependencyConditions()) != NULL)
         {
         TR::Register * boundReg = conditions->searchPostConditionRegister(realReg);
         if (boundReg == NULL)
            {
            boundReg = conditions->searchPreConditionRegister(realReg);
            }
         if (boundReg != NULL)
            {
            if (boundReg == virtReg)
               {
               return true;
               }
            return false;
            }
         }

      TR::Node * node = cursor->getNode();
      if (nodeBBStart != NULL && node != nodeBBStart)
         {
         return true;
         }
      if (node != NULL && node->getOpCodeValue() == TR::BBStart)
         {
         TR::Block * block = node->getBlock();
         if (!block->isExtensionOfPreviousBlock())
            {
            nodeBBStart = node;
            }
         }
      cursor = cursor->getPrev();
      // OOL entry label could cause this
      if (!cursor)
         return true;
      }

   return true;
   }

uint8_t
OMR::Z::Machine::getGPRSize()
   {
   return self()->cg()->comp()->target().is64Bit() ? 8 : 4;
   }

///////////////////////////////////////////////////////////////////////////////
//  Constructor

OMR::Z::Machine::Machine(TR::CodeGenerator * cg)
   : OMR::Machine(cg), _lastGlobalGPRRegisterNumber(-1), _last8BitGlobalGPRRegisterNumber(-1),
   _lastGlobalFPRRegisterNumber(-1), _lastGlobalCCRRegisterNumber(-1), _lastVolatileNonLinkGPR(-1), _lastLinkageGPR(-1),
     _lastVolatileNonLinkFPR(-1), _lastLinkageFPR(-1), _globalEnvironmentRegisterNumber(-1), _globalCAARegisterNumber(-1), _globalParentDSARegisterNumber(-1),
    _globalReturnAddressRegisterNumber(-1),_globalEntryPointRegisterNumber(-1)
   {
   self()->initializeRegisterFile();
   self()->initializeFPRegPairTable();
   self()->clearRegisterAssociations();
   }

/**
 *  This method implements a policy for finding the best swap register to be used
 *  when performing register exchange.
 *  It looks for blocked regs that are assigned dummy virt regs or for free regs.
 *  It is possible for this method to return NULL.
 */
TR::RealRegister *
OMR::Z::Machine::findBestSwapRegister(TR::Register* reg1, TR::Register* reg2)
   {
   TR_RegisterKinds rk = reg1->getKind();
   int32_t first, last;
   if (rk == TR_GPR)
      {
      first = TR::RealRegister::FirstGPR;
      last  = TR::RealRegister::LastAssignableGPR;
      }
   else if (rk == TR_FPR)
      {
      first = TR::RealRegister::FirstFPR;
      last  = TR::RealRegister::LastFPR;
      }
   else
      {
      first = TR::RealRegister::FirstVRF;
      last  = TR::RealRegister::LastVRF;
      }
   TR::RealRegister* realReg1 = toRealRegister(reg1->getAssignedRegister());
   TR::RealRegister* realReg2 = toRealRegister(reg2->getAssignedRegister());

   for (int32_t i = first; i <= last; i++)
      {
      if (
         _registerFile[i]->getState() == TR::RealRegister::Blocked     &&
         _registerFile[i]->getAssignedRegister() != NULL		      &&
         _registerFile[i]->getAssignedRegister()->isPlaceholderReg()  &&
         _registerFile[i] != realReg1                                 &&
         _registerFile[i] != realReg2
         )
         {
         return _registerFile[i];
         }
      }

   //  No luck.  No use-able dummy reg found.
   return NULL;
   }

/**
 * @return a bit map identifying assignable regs as 1's.
 */
uint32_t
OMR::Z::Machine::genBitMapOfAssignableGPRs()
   {
   uint32_t availRegMask = 0x0;

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; ++i)
      {
      if (_registerFile[i]->getState() != TR::RealRegister::Locked)
         {
         availRegMask |= _registerFile[i]->getRealRegisterMask();
         }
      }

   return availRegMask;
   }

/**
 * @return a bit map identifying assignable vector regs as 1's.
 */
uint32_t
OMR::Z::Machine::genBitMapOfAssignableVRFs()
   {
   uint32_t availRegMask = 0x0;

   for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastAssignableVRF; ++i)
      {
      if (_registerFile[i]->getState() != TR::RealRegister::Locked)
         {
         availRegMask |= _registerFile[i]->getRealRegisterMask();
         }
      }

   return availRegMask;
   }

/**
 * @return a bit vector identifying live regs pairs as 1's.
 */
uint8_t
OMR::Z::Machine::genBitVectOfLiveGPRPairs()
   {
   uint8_t liveRegMask = 0x0;

   for (int32_t i = TR::RealRegister::FirstGPR, j = 8; i <= TR::RealRegister::LastAssignableGPR; i += 2, --j)
      {
      if (_registerFile[i + 0]->getState() != TR::RealRegister::Free ||
          _registerFile[i + 1]->getState() != TR::RealRegister::Free)
         {
         // MSB = GPR0/GPR1, LSB = GPR14/GPR15
         liveRegMask |= _registerFile[j]->getRealRegisterMask();
         }
      }

   return liveRegMask;
   }
///////////////////////////////////////////////////////////////////////////////
// REGISTER PAIR assignment methods
///////////////////////////////////////////////////////////////////////////////

bool
OMR::Z::Machine::isLegalEvenOddPair(TR::RealRegister * evenReg, TR::RealRegister * oddReg, uint64_t availRegMask)
   {
   if (evenReg == NULL || oddReg == NULL)
      {
      return false;
      }

   else if (toRealRegister(evenReg)->getRegisterNumber() + 1 == toRealRegister(oddReg)->getRegisterNumber())
      {
      return self()->isLegalEvenRegister(evenReg, ALLOWBLOCKED, availRegMask) && self()->isLegalOddRegister(oddReg, ALLOWBLOCKED, availRegMask);
      }
   else
      {
      return false;
      }
   }

bool
OMR::Z::Machine::isLegalEvenRegister(TR::RealRegister * reg, bool allowBlocked, uint64_t availRegMask, bool allowLocked)
   {
   // Is the register assigned
   if (reg == NULL)
      {
      return false;
      }

   // The reg num types are actually +1 off real reg due to NoReg taking
   // up slot 0 in enumeration
   int32_t regNum = toRealRegister(reg)->getRegisterNumber() - 1;

   // Not a legal Even reg if 1) not even or 2) reg+1 is locked/blocked with non-sibling 3) using GPR0 when we shouldn't
   if (regNum %
      2 !=
      0 ||
      (toRealRegister(reg)->getRealRegisterMask() & availRegMask) ==
      0 ||
      _registerFile[toRealRegister(reg)->getRegisterNumber() + 1]->getState() ==
      TR::RealRegister::Locked && allowLocked == false ||
      (_registerFile[toRealRegister(reg)->getRegisterNumber() + 1]->getState() == TR::RealRegister::Blocked &&
      allowBlocked == false))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

bool
OMR::Z::Machine::isLegalFPPair(TR::RealRegister * firstReg, TR::RealRegister * secondReg, uint64_t availRegMask)
   {
   if (firstReg == NULL || secondReg == NULL)
      {
      return false;
      }
   else if (toRealRegister(firstReg)->getRegisterNumber() + 2 == toRealRegister(secondReg)->getRegisterNumber())
      {
      return self()->isLegalFirstOfFPRegister(firstReg, ALLOWBLOCKED, availRegMask) && self()->isLegalSecondOfFPRegister(secondReg, ALLOWBLOCKED, availRegMask);
      }
   else
      {
      return false;
      }
   }

bool
OMR::Z::Machine::isLegalFirstOfFPRegister(TR::RealRegister * reg, bool allowBlocked, uint64_t availRegMask, bool allowLocked)
   {
   // Is the register assigned
   if (reg == NULL)
      {
      return false;
      }

   // The reg num types are actually +1 off real reg due to NoReg taking
   // up slot 0 in enumeration
   int32_t regNum = toRealRegister(reg)->getRegisterNumber() - 1;

   // 1) must be one of FPR0, FPR4, FPR8, FPR12, FPR1, FPR5, FPR9, FPR13
   // 2) its sibling FP Reg is NOT locked/blocked
   if ((regNum % 4 != 0 && regNum % 4 != 1 ) ||
      (toRealRegister(reg)->getRealRegisterMask() & availRegMask) == 0 ||
      _registerFile[toRealRegister(reg)->getRegisterNumber() + 2]->getState() ==
      TR::RealRegister::Locked && allowLocked == false ||
      (_registerFile[toRealRegister(reg)->getRegisterNumber() + 2]->getState() == TR::RealRegister::Blocked &&
      allowBlocked == false))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

bool
OMR::Z::Machine::isLegalSecondOfFPRegister(TR::RealRegister * reg, bool allowBlocked, uint64_t availRegMask, bool allowLocked)
   {
   // Is the register assigned
   if (reg == NULL)
      {
      return false;
      }

   // The reg num types are actually +1 off real reg due to NoReg taking
   // up slot 0 in enumeration
   int32_t regNum = toRealRegister(reg)->getRegisterNumber() - 1;

   // Not a legal Even reg if 1) not even or 2) reg+1 is locked/blocked with non-sibling 3) using GPR0 when we shouldn't
   // 1) must be one of FPR2, FPR6, FPR10, FPR14, FPR3, FPR7, FPR11, FPR15
   // 2) its sibling FP Reg is NOT locked/blocked
   if ((regNum % 4 != 2 && regNum % 4 != 3 ) ||
      (toRealRegister(reg)->getRealRegisterMask() & availRegMask) == 0 ||
      _registerFile[toRealRegister(reg)->getRegisterNumber() - 2]->getState() ==
      TR::RealRegister::Locked && allowLocked == false ||
      (_registerFile[toRealRegister(reg)->getRegisterNumber() - 2]->getState() == TR::RealRegister::Blocked &&
      allowBlocked == false))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

/**
 * Try to grab an ODD register who's EVEN pred is unlocked and free.
 * If this fails, return the last ODD reg who's EVEN pred is not locked.
 */
TR::RealRegister *
OMR::Z::Machine::findBestLegalOddRegister(uint64_t availRegMask)
   {
   uint32_t bestWeightSoFar = 0xFFFFFFFF;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * lastOddReg = NULL;
   bool foundFreePair = false;

   for (int32_t i = TR::RealRegister::FirstGPR + 1; i <= TR::RealRegister::LastAssignableGPR; i += 2)
      {
      // Don't consider registers that can't be assigned
      if (_registerFile[i - 1]->getState() == TR::RealRegister::Locked ||
          _registerFile[i - 0]->getState() == TR::RealRegister::Locked ||
          _registerFile[i - 1]->getState() == TR::RealRegister::Blocked ||
          _registerFile[i - 0]->getState() == TR::RealRegister::Blocked ||
         (_registerFile[i - 0]->getRealRegisterMask() & availRegMask) == 0 ||
         (_registerFile[i - 1]->getRealRegisterMask() & availRegMask) == 0)
         {
         continue;
         }

      lastOddReg = _registerFile[i];

      if ((_registerFile[i - 0]->getState() == TR::RealRegister::Free || _registerFile[i - 0]->getState() == TR::RealRegister::Unlatched) &&
          (_registerFile[i - 1]->getState() == TR::RealRegister::Free || _registerFile[i - 1]->getState() == TR::RealRegister::Unlatched) &&
           _registerFile[i - 0]->getWeight() < bestWeightSoFar)
         {
         // Always prioritize a free pair
         foundFreePair = true;
         freeRegister = _registerFile[i];
         bestWeightSoFar = _registerFile[i]->getWeight();
         }
      else if (foundFreePair == false &&
         (_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
          _registerFile[i]->getWeight() < bestWeightSoFar)
         {
         freeRegister = _registerFile[i];
         bestWeightSoFar = freeRegister->getWeight();
         }
      }

   if (freeRegister == NULL)
      {
      if (lastOddReg)
         self()->cg()->traceRegisterAssignment("BEST LEGAL ODD: %R", lastOddReg);
      return lastOddReg;
      }
   else
      {
      self()->cg()->traceRegisterAssignment("BEST LEGAL ODD: %R", freeRegister);
      return freeRegister;
      }
   }

bool
OMR::Z::Machine::isLegalOddRegister(TR::RealRegister * reg, bool allowBlocked, uint64_t availRegMask, bool allowLocked)
   {
   // Is the register assigned
   if (reg == NULL)
      {
      return false;
      }

   // The reg num types are actually +1 off real reg due to NoReg taking
   // up slot 0 in enumeration
   int32_t regNum = toRealRegister(reg)->getRegisterNumber() - 1;

   // Not a legal Even reg if 1) not odd or 2) reg+1 is locked
   if (regNum % 2 != 1 ||
      (toRealRegister(reg)->getRealRegisterMask() & availRegMask) == 0 ||
      _registerFile[toRealRegister(reg)->getRegisterNumber() - 1]->getState() == TR::RealRegister::Locked  && allowLocked == false ||
      (_registerFile[toRealRegister(reg)->getRegisterNumber() - 1]->getState() == TR::RealRegister::Blocked &&
      allowBlocked == false))
      {
      return false;
      }
   else
      {
      return true;
      }
   }

/**
 * Try to grab an EVEN register who's ODD succ is unlocked and free.
 * If this fails, return the last EVEN reg who's ODD pred is not locked.
 */
TR::RealRegister *
OMR::Z::Machine::findBestLegalEvenRegister(uint64_t availRegMask)
   {
   uint32_t bestWeightSoFar = 0xFFFFFFFF;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * lastEvenReg = NULL;
   bool foundFreePair = false;

   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i += 2)
      {
      // Don't consider registers that can't be assigned.
      if (_registerFile[i + 0]->getState() == TR::RealRegister::Locked ||
          _registerFile[i + 1]->getState() == TR::RealRegister::Locked ||
          _registerFile[i + 0]->getState() == TR::RealRegister::Blocked ||
          _registerFile[i + 1]->getState() == TR::RealRegister::Blocked ||
         (_registerFile[i + 0]->getRealRegisterMask() & availRegMask) == 0 ||
         (_registerFile[i + 1]->getRealRegisterMask() & availRegMask) == 0)
         {
         continue;
         }

      lastEvenReg = _registerFile[i];
      //self()->cg()->traceRegWeight(lastEvenReg, lastEvenReg->getWeight());

      if ((_registerFile[i + 0]->getState() == TR::RealRegister::Free || _registerFile[i + 0]->getState() == TR::RealRegister::Unlatched) &&
          (_registerFile[i + 1]->getState() == TR::RealRegister::Free || _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
           _registerFile[i + 0]->getWeight() < bestWeightSoFar)
         {
         // Always prioritize a free pair
         foundFreePair = true;
         freeRegister = _registerFile[i];
         bestWeightSoFar = _registerFile[i]->getWeight();
         }
      else if (foundFreePair == false &&
         (_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
          _registerFile[i]->getWeight() < bestWeightSoFar)
         {
         freeRegister = _registerFile[i];
         bestWeightSoFar = freeRegister->getWeight();
         }
      }

   if (freeRegister == NULL)
      {
      if (lastEvenReg)
         self()->cg()->traceRegisterAssignment("BEST LAST LEGAL EVEN: %R", lastEvenReg);
      return lastEvenReg;
      }
   else
      {
      self()->cg()->traceRegisterAssignment("BEST LEGAL EVEN: %R", freeRegister);
      return freeRegister;
      }
   }

/**
 * Try to grab a legal high/low FP register who's sibling is unlocked and free.
 * If this fails, return the last high/low reg whose sibling pred is not locked.
 */
TR::RealRegister *
OMR::Z::Machine::findBestLegalSiblingFPRegister(bool isFirst, uint64_t availRegMask)
   {
   uint32_t bestWeightSoFar = 0xFFFFFFFF;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * lastFirstOfPair = NULL;
   TR::RealRegister * lastSecondOfPair = NULL;
   TR::RealRegister * lastSiblingFPReg = NULL;
   bool foundFreePair = false;

   for (int32_t i = 0; i < NUM_S390_FPR_PAIRS; i++)
      {
      TR::RealRegister::RegNum firstReg = _S390FirstOfFPRegisterPairs[i];
      TR::RealRegister::RegNum secondReg = _S390SecondOfFPRegisterPairs[i];
      // Don't consider registers that can't be assigned.
      if (_registerFile[firstReg]->getState() == TR::RealRegister::Locked
         || _registerFile[secondReg]->getState() == TR::RealRegister::Locked
         || _registerFile[firstReg]->getState() == TR::RealRegister::Blocked
         || _registerFile[secondReg]->getState() == TR::RealRegister::Blocked
         || (_registerFile[firstReg]->getRealRegisterMask() & availRegMask) == 0
         || (_registerFile[secondReg]->getRealRegisterMask() & availRegMask) == 0)
         {
         continue;
         }

      lastFirstOfPair = _registerFile[firstReg];
      lastSecondOfPair = _registerFile[secondReg];

      lastSiblingFPReg = isFirst ? lastFirstOfPair : lastSecondOfPair;

      //self()->cg()->traceRegWeight(lastSiblingFPReg, lastSiblingFPReg->getWeight());

      if ((lastFirstOfPair->getState() == TR::RealRegister::Free || lastFirstOfPair->getState() == TR::RealRegister::Unlatched)
         && (lastSecondOfPair->getState() == TR::RealRegister::Free || lastSecondOfPair->getState() == TR::RealRegister::Unlatched)
         && (lastSiblingFPReg->getWeight() < bestWeightSoFar))
         {
         foundFreePair = true;                   // Always prioritize a free pair
         freeRegister = lastSiblingFPReg;
         bestWeightSoFar = lastSiblingFPReg->getWeight();
         }
      else if (foundFreePair == false &&
         (lastSiblingFPReg->getState() == TR::RealRegister::Free || lastSiblingFPReg->getState() == TR::RealRegister::Unlatched)
         && lastSiblingFPReg->getWeight() < bestWeightSoFar)
         {
         freeRegister = lastSiblingFPReg;
         bestWeightSoFar = freeRegister->getWeight();
         }
      }

   if (freeRegister == NULL)
      {
      self()->cg()->traceRegisterAssignment("BEST LEGAL sibling FP Reg: %R", lastSiblingFPReg);
      return lastSiblingFPReg;
      }
   else
      {
      self()->cg()->traceRegisterAssignment("BEST LEGAL sibling FP Reg: %R", freeRegister);
      return freeRegister;
      }
   }

/**
 *  Register Assignment Interface
 *
 *  We assign any type of register (pair).
 *  Assign the best register to the virt target register
 *  @return the real reg that gets assigned. For a pair, it returns a new placeholder for
 *  the pair.  The real regs are double linked to associated virt regs.
 */
TR::Register *
OMR::Z::Machine::assignBestRegister(TR::Register    *targetRegister,
                                   TR::Instruction *currInst,
                                   bool            doBookKeeping,
                                   uint64_t        availRegMask)
   {
   TR::Register * returnRegister = 0;
   TR::RegisterPair *rp=targetRegister->getRegisterPair();

   if (rp)
      {
      returnRegister = self()->assignBestRegisterPair(targetRegister, currInst, doBookKeeping, availRegMask);
      }
   else
      {
      returnRegister = self()->assignBestRegisterSingle(targetRegister, currInst, doBookKeeping, availRegMask);
      }

   return returnRegister;
   }

TR::RealRegister *
OMR::Z::Machine::findBestRegisterForShuffle(TR::Instruction *currentInstruction,
                                           TR::Register        *currentAssignedRegister,
                                           uint64_t            availRegMask)
   {
   TR_ASSERT(currentAssignedRegister, "Register shuffling at instruction %P, but the register is not currently assigned?\n",currentInstruction);

   TR_RegisterKinds kindOfRegister = currentAssignedRegister->getKind();
   TR::RealRegister *newRegister = NULL;

   // we need to make sure we do not use same register that is assigned to this instruction for shuffling
   // Usually, other real registers assigned to the current instruction are block, except in situation where
   // we have exact one, non-pair target register. So temporary block it if it is not blocked already
   bool blockingRegister = false;
   TR::Register * targetRegister = currentInstruction->getRegisterOperand(1);
   if (targetRegister &&
       targetRegister->getRealRegister() &&
       targetRegister->getRealRegister()->getState() != TR::RealRegister::Blocked)
      {
      targetRegister->getRealRegister()->block();
      blockingRegister = true;
      }

   // find a free register
   if ((newRegister = self()->findBestFreeRegister(currentInstruction, kindOfRegister, currentAssignedRegister, availRegMask)) == NULL)
      {
      newRegister = self()->freeBestRegister(currentInstruction, currentAssignedRegister, kindOfRegister);
      }
   // unblock if we blocked target reg of the instruction
   if (blockingRegister)
      targetRegister->getRealRegister()->unblock();

   self()->cg()->traceRegisterAssignment("%R is assigned to invalid register in this instruction. Shuffling %R => %R...",
                                currentAssignedRegister, currentAssignedRegister->getAssignedRegister(), newRegister);
   return newRegister;
   }

/**
  * try to shuffle the virt reg to another real reg or spill that virtual register
  @return if the virtual reg was shuffled, the real reg that it was shuffled to, otherwise NULL
*/
TR::RealRegister *
OMR::Z::Machine::shuffleOrSpillRegister(TR::Instruction *currInst,
                                        TR::Register *toFreeReg,
                                        uint64_t availRegMask)
   {
   TR::RealRegister * assignedRegister = toFreeReg->getAssignedRealRegister();
   if (toFreeReg->isUsedInMemRef())
      {
      availRegMask &= ~TR::RealRegister::GPR0Mask;
      }

   TR::RealRegister * bestRegister = self()->findBestFreeRegister(currInst, toFreeReg->getKind(), toFreeReg, availRegMask);
   if (bestRegister == NULL)
      self()->spillRegister(currInst, toFreeReg);
   else
      {
      TR::Instruction * cursor = self()->registerCopy(self()->cg(), toFreeReg->getKind(), assignedRegister, bestRegister, currInst);
      toFreeReg->setAssignedRegister(bestRegister);
      bestRegister->setAssignedRegister(toFreeReg);
      bestRegister->setState(TR::RealRegister::Assigned);
      assignedRegister->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Unlatched);
      }

   return bestRegister;
   }

/**
 * Assign the best register to the virt target register
 * @return the real reg that gets assigned
 */
TR::Register *
OMR::Z::Machine::assignBestRegisterSingle(TR::Register    *targetRegister,
                                         TR::Instruction *currInst,
                                         bool            doBookKeeping,
                                         uint64_t        availRegMask)
   {
   TR_RegisterKinds kindOfRegister = targetRegister->getKind();
   TR::RealRegister * assignedRegister = targetRegister->getAssignedRealRegister();
   TR::Compilation *comp = self()->cg()->comp();

   bool reverseSpilled = false;

   bool defsRegister=currInst->defsRegister(targetRegister);
   if (assignedRegister == NULL)
      {
      if (self()->cg()->insideInternalControlFlow())
         {
         TR_ASSERT_FATAL(false, "Attempting to assign a register (%s) inside ICF", getRegisterName(targetRegister, self()->cg()));
         }
      }
   if (kindOfRegister != TR_FPR && kindOfRegister != TR_VRF && assignedRegister != NULL)
      {
      if ((assignedRegister->getRealRegisterMask() & availRegMask) == 0)
         {
         // Oh no.. targetRegister is assigned something it shouldn't be assigned to. Do some shuffling
         // find a new register to shuffle to
         TR::RealRegister * newAssignedRegister = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
         TR::Instruction *cursor = self()->registerCopy(self()->cg(), kindOfRegister, toRealRegister(assignedRegister), newAssignedRegister, currInst);
         targetRegister->setAssignedRegister(newAssignedRegister);
         newAssignedRegister->setAssignedRegister(targetRegister);
         newAssignedRegister->setState(TR::RealRegister::Assigned);
         assignedRegister->setAssignedRegister(NULL);
         assignedRegister->setState(TR::RealRegister::Free);
         assignedRegister = newAssignedRegister;
         }
      }
   else if (assignedRegister != NULL && (assignedRegister->getRealRegisterMask() & availRegMask) == 0)
      {
      // Oh no.. targetRegister is assigned something it shouldn't be assigned to
      // Do some shuffling
      // find a new register to shuffle to
      assignedRegister->block();
      TR::RealRegister * newAssignedRegister = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
      assignedRegister->unblock();
      TR::Instruction *cursor=self()->registerCopy(self()->cg(), kindOfRegister, assignedRegister, newAssignedRegister, currInst);
      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
      self()->cg()->traceRegAssigned(targetRegister,assignedRegister);
      targetRegister->setAssignedRegister(newAssignedRegister);
      newAssignedRegister->setAssignedRegister(targetRegister);
      newAssignedRegister->setState(TR::RealRegister::Assigned);
      assignedRegister->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Free);
      assignedRegister = newAssignedRegister;
      }

   // Have we already assigned a real register
   if (assignedRegister == NULL)
      {
      // These values are only equal upon the first assignment.  Hence, if they aren't
      // the same, and there is no assigned reg, we must have spilled the reg, so invert
      // the spill state to get the reg back
      if(targetRegister->getTotalUseCount() != targetRegister->getFutureUseCount())
         {
         assignedRegister = self()->reverseSpillState(currInst, targetRegister);
         reverseSpilled = true;
         }
      else
         {
         // New reg assignment, find a free reg.
         // If no free reg available,  free one up
         if ((assignedRegister = self()->findBestFreeRegister(currInst, kindOfRegister, targetRegister, availRegMask)) == NULL)
            {
            assignedRegister = self()->freeBestRegister(currInst, targetRegister, kindOfRegister);
            }
         }

      //  We double link the virt and real regs as such:
      //   targetRegister->_assignedRegister() -> assignedRegister
      //   targetRegister                      <- assignedRegister->_assignedRegister()
      targetRegister->setAssignedRegister(assignedRegister);
      assignedRegister->setAssignedRegister(targetRegister);
      assignedRegister->setState(TR::RealRegister::Assigned);

      self()->cg()->traceRegAssigned(targetRegister, assignedRegister);
      self()->cg()->clearRegisterAssignmentFlags();
      }

   // Bookkeeping to update the future use count
      if (doBookKeeping && (assignedRegister->getState() != TR::RealRegister::Locked))
      {
      targetRegister->decFutureUseCount();
      targetRegister->setIsLive();

      TR_ASSERT(targetRegister->getFutureUseCount() >= 0,
               "\nRegister assignment: register [%s] futureUseCount should not be negative (for node [%s], ref count=%d) !\n",
               self()->cg()->getDebug()->getName(targetRegister),
               self()->cg()->getDebug()->getName(currInst->getNode()),
               currInst->getNode()->getReferenceCount());

      // If we aren't re-using this reg anymore, kill assignment
      // OOL: if a register is first defined (became live) in the hot path, no matter how many futureUseCount left (in the code path)
      // the register is considered as dead now in the hot path, so GC map contains the correct list of live registers

      bool killOOLReg = false;
      if (self()->cg()->isOutOfLineHotPath() &&
          targetRegister->getStartOfRange() == currInst)
         {
         killOOLReg = true;
         TR::Instruction *currS390Inst = currInst;
         // this is to prevent problems with instructions such as XR virtRegx,virtRegx
         // We assign target registers first, but in this case we do not want to kill the virtual register
         // because the source register (which has the same virtual reg) is not yet assigned
         if (currS390Inst->getRegisterOperand(2) == currS390Inst->getRegisterOperand(1) &&
             currS390Inst->getRegisterOperand(2) == targetRegister)
            {
            killOOLReg = false;
            }
         }

      if ((targetRegister->getFutureUseCount() == 0) || killOOLReg)
         {
         self()->cg()->traceRegFreed(targetRegister, assignedRegister);
         targetRegister->resetIsLive();

         if (assignedRegister->getState() == TR::RealRegister::Locked )
            {
            assignedRegister->setAssignedRegister(NULL);
            }
         else
            {
            targetRegister->setAssignedRegister(NULL);
            assignedRegister->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Free);
            }
         }
      }

   return assignedRegister;
   }

/**
 * Assign the best even/odd pair of consecutive registers to the regPair object passed in.
 * If both virt regs are not assigned, assign a consecutive free pair (curr we do not handle
 * the case where a free cons pair is not available).
 * If either reg is already assigned, coerce assignment of respective even/odd register.
 *
 * Register order and naming convention for S390 Big Endian register pairs
 *    A register pair is made up of an EVEN and an ODD register
 *            [EVEN |ODD  ]
 *      bits  0      32   63
 *    where ODD=EVEN+1
 *
 *    When representing binary integers,
 *            [8000|0000] = -2^31
 *            [ffff|ffff] = -1
 *            [7fff|ffff] =  2^31-1
 *            [0000|0001] =  1
 *
 *    Hence, we denote the HIGHORDER word as bits [0 -31]
 *                 and the LOWORDER  word as bits [32-63], or
 *            [HIGH |LOW  ]
 *            0      32   63
 *
 *    When executing a LM or a STM to handle a register pair
 *    we must use
 *             LM  EVEN,ODD,memRef
 *             STM EVEN,ODD,memRef
 *
 *    as such we denote the EVEN,ODD pair as
 *            [FIRST|LAST ]
 *            0      32   63
 */
TR::Register *
OMR::Z::Machine::assignBestRegisterPair(TR::Register    *regPair,
                                       TR::Instruction *currInst,
                                       bool            doBookKeeping,
                                       uint64_t        availRegMask)
   {
   TR_ASSERT(regPair->getRegisterPair() != NULL,
      "OMR::Z::Machine::assignBestRegisterPair: Attempting to assign a real pair to a non-pair virtual\n");

   TR::Compilation *comp = self()->cg()->comp();

   TR::Register * firstReg = regPair->getHighOrder();
   TR::Register * lastReg = regPair->getLowOrder();

   TR::RealRegister * freeRegisterHigh = firstReg->getAssignedRealRegister();
   TR::RealRegister * freeRegisterLow = lastReg->getAssignedRealRegister();
   TR_RegisterKinds regPairKind = regPair->getKind();

  self()->cg()->traceRegisterAssignment("attempt to assign components of register pair ( %R %R )", firstReg, lastReg);

   if (firstReg->is64BitReg())
      self()->cg()->traceRegisterAssignment("%R is64BitReg", firstReg);
   if (lastReg->is64BitReg())
      self()->cg()->traceRegisterAssignment("%R is64BitReg", lastReg);

   if (freeRegisterHigh == NULL || freeRegisterLow == NULL)
      {
      if (self()->cg()->insideInternalControlFlow())
         {
         TR_ASSERT(0, "ASSERTION assignBestRegisterPair inside Internal Control Flow for inst %p.\n"
                      "Ensure all registers within ICF have a dependency anchored at the end-ICF label\n",currInst);
         }
      }

   // Disable GPR0 for this assignment if it is used in a memref
   if (firstReg->isUsedInMemRef())
      {
      availRegMask &= ~TR::RealRegister::GPR0Mask;
      availRegMask &= ~TR::RealRegister::GPR1Mask;
      }

   // We need a new placeholder for the pair that will stay with the instruction, leaving the
   // input pair free to change under further allocation.
   TR::RegisterPair * assignedRegPair = new (self()->cg()->trHeapMemory(), TR_MemoryBase::RegisterPair) TR::RegisterPair(lastReg, firstReg);
   if (regPair->getKind() == TR_FPR)
     assignedRegPair->setKind(TR_FPR);

   // In case we need to unspill both sibling regs, we do so concurrently to avoid runspill of siblings
   // triping over each other.
   if (freeRegisterLow  == NULL && (lastReg->getTotalUseCount() != lastReg->getFutureUseCount()) &&
       freeRegisterHigh == NULL && (firstReg->getTotalUseCount() != firstReg->getFutureUseCount()))
      {
      TR::RealRegister * tfreeRegisterHigh = NULL;
      TR::RealRegister * tfreeRegisterLow  = NULL;

      // Assign a real register pair for
      if (!self()->findBestFreeRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPairKind, currInst, availRegMask))
         {
         self()->freeBestRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPairKind, currInst, availRegMask);
         }
      freeRegisterLow = self()->reverseSpillState(currInst, lastReg, toRealRegister(tfreeRegisterLow));
      freeRegisterHigh = self()->reverseSpillState(currInst, firstReg, toRealRegister(tfreeRegisterHigh));
      }

   // TotalUse & FutureUse are only equal upon the first assignment.  Hence, if they aren't
   // the same, and there is no assigned reg, we must have spilled the reg, so invert
   // the spill state to get the reg back
   if (freeRegisterLow == NULL && (lastReg->getTotalUseCount() != lastReg->getFutureUseCount()))
      {
      if (freeRegisterHigh)
         {
         freeRegisterHigh->block();
         }

      freeRegisterLow = self()->reverseSpillState(currInst, lastReg);

      if (freeRegisterHigh)
         {
         freeRegisterHigh->unblock();
         }
      }

   // TotalUse & FutureUse are only equal upon the first assignment.  Hence, if they aren't
   // the same, and there is no assigned reg, we must have spilled the reg, so invert
   // the spill state to get the reg back
   if (freeRegisterHigh == NULL && (firstReg->getTotalUseCount() != firstReg->getFutureUseCount()))
      {
      if (freeRegisterLow)
         {
         freeRegisterLow->block();
         }

      freeRegisterHigh = self()->reverseSpillState(currInst, firstReg);

      if (freeRegisterLow)
         {
         freeRegisterLow->unblock();
         }
      }

   if ( ((regPair->getKind() != TR_FPR) && (regPair->getKind() != TR_VRF) &&
          self()->isLegalEvenOddPair(freeRegisterHigh, freeRegisterLow, availRegMask))
        || ((regPair->getKind() == TR_FPR) && self()->isLegalFPPair(freeRegisterHigh, freeRegisterLow, availRegMask)))
      {
      // already correctly assigned. Do nothing.
      }
   else if (freeRegisterLow == NULL && freeRegisterHigh == NULL)
      {
      // Assign a real register pair for
      //
      if (!self()->findBestFreeRegisterPair(&freeRegisterHigh, &freeRegisterLow, regPairKind, currInst, availRegMask))
         {
         self()->freeBestRegisterPair(&freeRegisterHigh, &freeRegisterLow, regPairKind, currInst, availRegMask);
         }

      firstReg->setAssignedRegister(freeRegisterHigh);
      lastReg->setAssignedRegister(freeRegisterLow);

      freeRegisterHigh->setAssignedRegister(firstReg);
      freeRegisterLow->setAssignedRegister(lastReg);

      freeRegisterHigh->setState(TR::RealRegister::Assigned);
      freeRegisterLow->setState(TR::RealRegister::Assigned);
      }
   else if ( TR_FPR != regPair->getKind() )
      { //GPR pair case, real reg assigned but not a legal even-odd pair
      if (!self()->isLegalEvenRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask))
         {
         // Let's make sure the ODD register is legal
         //
         if (!self()->isLegalOddRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
            {
            // Neither virtual register is in a legal real register.
            // We are going to move both of them, so unblock them in case
            // we are working with very restricted registers
            firstReg->unblock();
            lastReg->unblock();
            TR::RealRegister * newOddReg = self()->findBestLegalOddRegister(availRegMask);
            TR_ASSERT(newOddReg, "OMR::Z::Machine::assignBestRegisterPair: newOddReg is NULL!\n");
            self()->coerceRegisterAssignment(currInst, lastReg, newOddReg->getRegisterNumber());
            freeRegisterLow = newOddReg;
            }
         if (lastReg)
            {
            lastReg->block(); // Make sure we don't move this anymore when coercing firstReg
            }

         self()->coerceRegisterAssignment(currInst, firstReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
         freeRegisterHigh = self()->getRealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
         }
      else if (!self()->isLegalOddRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
         {
         // No need to check that the EVEN register is legal,
         // because otherwise we would have gone into the other branch.

         if (firstReg)
            {
            firstReg->block();  // Make sure we don't move this anymore when coercing lastReg
            }

         self()->coerceRegisterAssignment(currInst, lastReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
         freeRegisterLow = self()->getRealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
         }
      else if (!self()->isLegalEvenOddPair(freeRegisterHigh, freeRegisterLow, availRegMask))
         {
         // We could probably does something a little more  fancy to avoid
         // moving both regs in cases where one has a legal sibling that
         // is free.
         //

         // if one of them is a legal register, then we can just force the assignment for the sibling
         if (self()->isLegalEvenRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask))
            {
            if (firstReg)
               {
               firstReg->block();  // Make sure we don't move this anymore when coercing lastReg
               }

            self()->coerceRegisterAssignment(currInst, lastReg,
                                     (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
            freeRegisterLow = self()->getRealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
            }
         else if (self()->isLegalOddRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
            {
            if (lastReg)
               {
               lastReg->block();  // Make sure we don't move this anymore when coercing firstReg
               }

            self()->coerceRegisterAssignment(currInst, firstReg,
                                     (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
            freeRegisterHigh = self()->getRealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
            }
         else
            {
            TR::RealRegister * tfreeRegisterHigh = NULL;
            TR::RealRegister * tfreeRegisterLow  = NULL;

            // Assign a real register pair for
            //
            if (!self()->findBestFreeRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPairKind, currInst, availRegMask))
               {
               self()->freeBestRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPairKind, currInst, availRegMask);
               }

            self()->coerceRegisterAssignment(currInst, firstReg, (toRealRegister(tfreeRegisterHigh)->getRegisterNumber()));
            self()->coerceRegisterAssignment(currInst, lastReg, (toRealRegister(tfreeRegisterLow)->getRegisterNumber()));

            freeRegisterLow = tfreeRegisterLow;
            freeRegisterHigh = tfreeRegisterHigh;
            }
         }
      } //...GPR pair case, real reg assigned but not a legal even-odd pair
   else if ( TR_FPR == regPair->getKind() ) { //FP pair case, real reg assigned but not a legal fp reg pair
      if (!self()->isLegalFirstOfFPRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask))
         {
         // make sure the low FP register is legal
         //
         if (!self()->isLegalSecondOfFPRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
            {
            TR::RealRegister * newLowFPReg = self()->findBestLegalSiblingFPRegister(false,availRegMask);
            self()->coerceRegisterAssignment(currInst, lastReg, newLowFPReg->getRegisterNumber());
            freeRegisterLow = newLowFPReg;
            }
         if (lastReg)
            {
            lastReg->block(); // Make sure we don't move this anymore when coercing firstReg
            }
         self()->coerceRegisterAssignment(currInst, firstReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow->getSiblingRegister())->getRegisterNumber()));
         freeRegisterHigh = toRealRegister(freeRegisterLow->getSiblingRegister());
         }
      else if (!self()->isLegalSecondOfFPRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
         {
         // make sure the low FP register is legal
         //
         if (!self()->isLegalFirstOfFPRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask))
            {
            TR::RealRegister * newHighFPReg = self()->findBestLegalSiblingFPRegister(true,availRegMask);
            self()->coerceRegisterAssignment(currInst, firstReg, newHighFPReg->getRegisterNumber());
            freeRegisterHigh = newHighFPReg;
            }
         if (firstReg)
            {
            firstReg->block();  // Make sure we don't move this anymore when coercing lastReg
            }

         self()->coerceRegisterAssignment(currInst, lastReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh->getSiblingRegister())->getRegisterNumber()));
         freeRegisterLow = toRealRegister(freeRegisterHigh->getSiblingRegister());
         }
      else if (!self()->isLegalFPPair(freeRegisterHigh, freeRegisterLow, availRegMask))
         {
         // We could probably does something a little more  fancy to avoid
         // moving both regs in cases where one has a legal sibling that
         // is free.
         //
         TR::RealRegister * tfreeRegisterHigh = NULL;
         TR::RealRegister * tfreeRegisterLow  = NULL;

         // Assign a real register pair for
         //
         if (!self()->findBestFreeRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPair->getKind(), currInst, availRegMask))
            {
            self()->freeBestRegisterPair(&tfreeRegisterHigh, &tfreeRegisterLow, regPair->getKind(), currInst, availRegMask);
            }

         self()->coerceRegisterAssignment(currInst, firstReg, (toRealRegister(tfreeRegisterHigh)->getRegisterNumber()));
         self()->coerceRegisterAssignment(currInst, lastReg, (toRealRegister(tfreeRegisterLow)->getRegisterNumber()));

         freeRegisterLow = tfreeRegisterLow;
         freeRegisterHigh = tfreeRegisterHigh;
         }
      } // ...FP Pair
   // Do bookkeeping of virt reg
   //   Decrement the future use count as we are, by defn of trying to assign a
   //   a real reg, using it again.
   // Check to see if either register can be freed if ref count is 0
   //

   // OOL: if a register is first defined (became live) in the hot path, no matter how many futureUseCount left (in the code path)
   // the register is considered as dead now in the hot path, so GC map contains the correct list of live registers
   if (doBookKeeping)
      {
      firstReg->setIsLive();
      if (((firstReg->decFutureUseCount() == 0) ||
           (self()->cg()->isOutOfLineHotPath() && firstReg->getStartOfRange() == currInst)) &&
           (freeRegisterHigh->getState() != TR::RealRegister::Locked))
         {
         firstReg->resetIsLive();
         firstReg->setAssignedRegister(NULL);

         freeRegisterHigh->setAssignedRegister(NULL);
         if (freeRegisterHigh->getState() != TR::RealRegister::Locked)
            freeRegisterHigh->setState(TR::RealRegister::Free);
         }
      lastReg->setIsLive();
      if (((lastReg->decFutureUseCount() == 0) ||
           (self()->cg()->isOutOfLineHotPath() && lastReg->getStartOfRange() == currInst)) &&
          (freeRegisterLow->getState() != TR::RealRegister::Locked))
         {
         lastReg->resetIsLive();
         lastReg->setAssignedRegister(NULL);

         freeRegisterLow->setAssignedRegister(NULL);
         if (freeRegisterLow->getState() != TR::RealRegister::Locked)
            freeRegisterLow->setState(TR::RealRegister::Free);
         }
      }

   assignedRegPair->setHighOrder(freeRegisterHigh, self()->cg());
   assignedRegPair->setLowOrder(freeRegisterLow, self()->cg());

   return assignedRegPair;
   }

/**
 * Find an even-odd pair of free registers.
 * @return true if a free pair are found
 *         false if no free pair is found
 */
bool
OMR::Z::Machine::findBestFreeRegisterPair(TR::RealRegister ** firstRegister, TR::RealRegister ** lastRegister,
   TR_RegisterKinds rk, TR::Instruction * currInst, uint64_t availRegMask)
   {
   TR::RealRegister * freeRegisterLow = NULL;
   TR::RealRegister * freeRegisterHigh = NULL;

   uint64_t bestWeightSoFar = -1;

   if (rk != TR_FPR && rk != TR_VRF)
      {
      // Look at all reg pairs (starting with an even register)
      for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i += 2)
         {
         // Don't consider registers that can't be assigned.
         if (_registerFile[i + 0]->getState() == TR::RealRegister::Locked || (_registerFile[i + 0]->getRealRegisterMask() & availRegMask) == 0 ||
             _registerFile[i + 1]->getState() == TR::RealRegister::Locked || (_registerFile[i + 1]->getRealRegisterMask() & availRegMask) == 0)
            {
            continue;
            }

         // See if this pair is available, and better than the prev
         if ((_registerFile[i + 0]->getState() == TR::RealRegister::Free || _registerFile[i + 0]->getState() == TR::RealRegister::Unlatched) &&
             (_registerFile[i + 1]->getState() == TR::RealRegister::Free || _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
             (_registerFile[i + 0]->getWeight() + _registerFile[i + 1]->getWeight()) < bestWeightSoFar)
            {
            freeRegisterHigh = _registerFile[i];
            freeRegisterLow = _registerFile[i + 1];

            bestWeightSoFar = freeRegisterHigh->getWeight() + freeRegisterLow->getWeight();
            }
         }
      }
   else
      {
      // TODO: What about VRFs?
      // TODO: We look for _registerFile[i + 0] and _registerFile[i + 2] below, is this correct?
      // Look at all FP reg pairs
      for (int32_t k = 0; k < NUM_S390_FPR_PAIRS; k++)
         {
         int32_t i = _S390FirstOfFPRegisterPairs[k];
         // Don't consider registers that can't be assigned
         if (_registerFile[i + 0]->getState() == TR::RealRegister::Locked || (_registerFile[i + 0]->getRealRegisterMask() & availRegMask) == 0 ||
             _registerFile[i + 2]->getState() == TR::RealRegister::Locked || (_registerFile[i + 2]->getRealRegisterMask() & availRegMask) == 0)
            {
            continue;
            }

         // See if this pair is available and better than the previous
         if ((_registerFile[i + 0]->getState() == TR::RealRegister::Free || _registerFile[i + 0]->getState() == TR::RealRegister::Unlatched) &&
             (_registerFile[i + 2]->getState() == TR::RealRegister::Free || _registerFile[i + 2]->getState() == TR::RealRegister::Unlatched) &&
             (_registerFile[i + 0]->getWeight() + _registerFile[i + 2]->getWeight()) < bestWeightSoFar)
            {
            freeRegisterHigh = _registerFile[i];
            freeRegisterLow = _registerFile[i + 2];

            bestWeightSoFar = freeRegisterHigh->getWeight() + freeRegisterLow->getWeight();
            }
         }
      }

   // If unlatched, set it free
   if (freeRegisterHigh != NULL && freeRegisterHigh->getState() == TR::RealRegister::Unlatched)
      {
      freeRegisterHigh->setAssignedRegister(NULL);
      freeRegisterHigh->setState(TR::RealRegister::Free);
      }

   // If unlatched, set it free
   if (freeRegisterLow != NULL && freeRegisterLow->getState() == TR::RealRegister::Unlatched)
      {
      freeRegisterLow->setAssignedRegister(NULL);
      freeRegisterLow->setState(TR::RealRegister::Free);
      }

   // Did we find a pair, then update the register set structure.
   if (freeRegisterLow != NULL && freeRegisterHigh != NULL)
      {
      *firstRegister = freeRegisterHigh;
      *lastRegister = freeRegisterLow;

      self()->cg()->traceRegisterAssignment("BEST FREE PAIR: (%R, %R)", freeRegisterHigh, freeRegisterLow);
      return true;
      }
   else
      {
      return false;
      }
   }

void
OMR::Z::Machine::freeBestFPRegisterPair(TR::RealRegister ** firstReg, TR::RealRegister ** lastReg, TR::Instruction * currInst,
   uint64_t          availRegMask)
   {
   TR::Node * currentNode = currInst->getNode();
   TR::Compilation *comp = self()->cg()->comp();

   TR::Instruction * cursor = NULL;

   TR_BackingStore * locationLow;
   TR_BackingStore * locationHigh;

   // Definition of what the best candidate register pair is goes here.
   TR::RealRegister * bestCandidateHigh = NULL;
   TR::RealRegister * bestCandidateLow = NULL;

   TR_Debug * debugObj = self()->cg()->getDebug();

   // search through all FP reg pairs
   for (int32_t i = 0; i < NUM_S390_FPR_PAIRS; i ++)
      {
      TR::RealRegister::RegNum firstReg = _S390FirstOfFPRegisterPairs[i];
      TR::RealRegister::RegNum secondReg = _S390SecondOfFPRegisterPairs[i];
      // Don't consider registers that can't be assigned.
      if ((_registerFile[firstReg]->getState() == TR::RealRegister::Locked) ||
         (_registerFile[secondReg]->getState() == TR::RealRegister::Locked) ||
         (_registerFile[firstReg]->getState() == TR::RealRegister::Blocked) ||
         (_registerFile[secondReg]->getState() == TR::RealRegister::Blocked) ||
         (_registerFile[firstReg]->getRealRegisterMask() & availRegMask) == 0 ||
         (_registerFile[secondReg]->getRealRegisterMask() & availRegMask) == 0)
         {
         continue;
         }

      // Priority to assigned/unassigned pairs.
      if ((_registerFile[firstReg]->getState() == TR::RealRegister::Free || _registerFile[firstReg]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[secondReg]->getState() == TR::RealRegister::Assigned)
         {
         bestCandidateHigh = _registerFile[firstReg];
         bestCandidateLow = _registerFile[secondReg];
         }
      else if ((_registerFile[secondReg]->getState() == TR::RealRegister::Free ||
         _registerFile[secondReg]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[firstReg]->getState() == TR::RealRegister::Assigned)
         {
         bestCandidateHigh = _registerFile[firstReg];
         bestCandidateLow = _registerFile[secondReg];
         }
      // Both assigned
      else if (bestCandidateHigh == NULL && bestCandidateLow == NULL)
         {
         bestCandidateHigh = _registerFile[firstReg];
         bestCandidateLow = _registerFile[secondReg];
         }
      } // for all regs

   // Now that we've decided on a pair of real-regs to spill, we will need
   // the virtual regs to setup the spill information
   TR::Register * bestVirtCandidateHigh = bestCandidateHigh->getAssignedRegister();
   TR::Register * bestVirtCandidateLow = bestCandidateLow->getAssignedRegister();

   if (bestVirtCandidateLow != NULL)
      {
      locationLow = bestVirtCandidateLow->getBackingStorage();
      if(locationLow == NULL)
        locationLow = self()->cg()->allocateSpill(8, false, NULL, true);

      TR::MemoryReference * tempMRLow = generateS390MemoryReference(currentNode, locationLow->getSymbolReference(), self()->cg());
      locationLow->getSymbolReference()->getSymbol()->setSpillTempLoaded();
      bestVirtCandidateLow->setBackingStorage(locationLow);
      cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, currentNode, bestCandidateLow, tempMRLow, currInst);
      if ( !cursor->assignFreeRegBitVector() )
         {
         cursor->assignBestSpillRegister();
         }

      bestVirtCandidateLow->setAssignedRegister(NULL);

      if (!self()->cg()->isOutOfLineColdPath())
         {
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(bestVirtCandidateLow);

         // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
         // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
         // if we reverse spill this register inside the OOL cold/hot path
         if (!self()->cg()->isOutOfLineHotPath())
            {// main line
            locationLow->setMaxSpillDepth(1);
            }
         else
            {
            // hot path
            // do not overwrite main line spill depth
            if (locationLow->getMaxSpillDepth() != 1)
               locationLow->setMaxSpillDepth(2);
            }
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: adding reg pair low %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                          debugObj->getName(bestVirtCandidateLow), locationLow->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (locationLow->getMaxSpillDepth() != 1 &&
               locationLow->getMaxSpillDepth() != 2 )
            locationLow->setMaxSpillDepth(3);
         }
      } // if (bestVirtCandidateLow != NULL)

   if (bestVirtCandidateHigh != NULL)
      {
      locationHigh = bestVirtCandidateHigh->getBackingStorage();
      if(locationHigh == NULL)
        locationHigh = self()->cg()->allocateSpill(8, false, NULL, true);

      TR::MemoryReference * tempMRHigh = generateS390MemoryReference(currentNode, locationHigh->getSymbolReference(), self()->cg());
      locationHigh->getSymbolReference()->getSymbol()->setSpillTempLoaded();
      bestVirtCandidateHigh->setBackingStorage(locationHigh);
      cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LD, currentNode, bestCandidateHigh, tempMRHigh, currInst);
      if ( !cursor->assignFreeRegBitVector() )
         {
         cursor->assignBestSpillRegister();
         }

      bestVirtCandidateHigh->setAssignedRegister(NULL);

      if (!self()->cg()->isOutOfLineColdPath())
         {
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(bestVirtCandidateHigh);

         // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
         // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
         // if we reverse spill this register inside the OOL cold/hot path
         if (!self()->cg()->isOutOfLineHotPath())
            {// main line
            locationHigh->setMaxSpillDepth(1);
            }
         else
            {
            // hot path
            // do not overwrite main line spill depth
            if (locationHigh->getMaxSpillDepth() != 1)
               locationHigh->setMaxSpillDepth(2);
            }
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: adding reg pair high %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                          debugObj->getName(bestVirtCandidateHigh), locationHigh->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (locationHigh->getMaxSpillDepth() != 1 &&
               locationHigh->getMaxSpillDepth() != 2 )
            locationHigh->setMaxSpillDepth(3);
         }
      } //if (bestVirtCandidateHigh != NULL)

   // Mark associated real regs as free
   bestCandidateHigh->setState(TR::RealRegister::Free);
   bestCandidateLow->setState(TR::RealRegister::Free);
   bestCandidateHigh->setAssignedRegister(NULL);
   bestCandidateLow->setAssignedRegister(NULL);

   // Return free registers
   *firstReg = bestCandidateHigh;
   *lastReg = bestCandidateLow;
   }

void
OMR::Z::Machine::freeBestRegisterPair(TR::RealRegister ** firstReg, TR::RealRegister ** lastReg, TR_RegisterKinds rk, TR::Instruction * currInst,
   uint64_t          availRegMask)
   {
   self()->cg()->traceRegisterAssignment("FREE BEST REGISTER PAIR");
   if ( rk == TR_FPR )
     {
     self()->freeBestFPRegisterPair(firstReg,lastReg,currInst, availRegMask);
     return;
     }
   TR::Node * currentNode = currInst->getNode();

   TR::Instruction * cursor = NULL;
   TR::Compilation *comp = self()->cg()->comp();
   TR::Machine *machine = self()->cg()->machine();

   TR_BackingStore * locationLow;
   TR_BackingStore * locationHigh;

   // Definition of what the best candidate register pair is goes here.
   TR::RealRegister * bestCandidateHigh = NULL;
   TR::RealRegister * bestCandidateLow = NULL;

   TR_Debug * debugObj = self()->cg()->getDebug();

   // Look at all reg pairs (starting with an even reg)
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i += 2)
      {
      // Don't consider registers that can't be assigned.
      if ((_registerFile[i + 0]->getState() == TR::RealRegister::Locked) ||
          (_registerFile[i + 1]->getState() == TR::RealRegister::Locked) ||
          (_registerFile[i + 0]->getState() == TR::RealRegister::Blocked) ||
          (_registerFile[i + 1]->getState() == TR::RealRegister::Blocked) ||
          (_registerFile[i + 0]->getRealRegisterMask() & availRegMask) == 0 ||
          (_registerFile[i + 1]->getRealRegisterMask() & availRegMask) == 0)
         {
         continue;
         }

      // Priority to assigned/unassigned pairs.
      if ((_registerFile[i + 0]->getState() == TR::RealRegister::Free || _registerFile[i + 0]->getState() == TR::RealRegister::Unlatched) &&
           _registerFile[i + 1]->getState() == TR::RealRegister::Assigned)
         {
         bestCandidateHigh = _registerFile[i];
         bestCandidateLow = _registerFile[i + 1];
         }
      else if ((_registerFile[i + 1]->getState() == TR::RealRegister::Free || _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
                _registerFile[i + 0]->getState() == TR::RealRegister::Assigned)
         {
         bestCandidateHigh = _registerFile[i];
         bestCandidateLow = _registerFile[i + 1];
         }
      // Both assigned
      else if (bestCandidateHigh == NULL && bestCandidateLow == NULL)
         {
         bestCandidateHigh = _registerFile[i];
         bestCandidateLow = _registerFile[i + 1];
         }
      } // for all regs

   // Assert if no register pair was found
   if (bestCandidateHigh == NULL && bestCandidateLow == NULL)
      {
      if (self()->cg()->getDebug() != NULL)
         {
         self()->cg()->getDebug()->printGPRegisterStatus(comp->getOutFile(), machine);
         }

      TR_ASSERT_FATAL(0, "Ran out of register pairs to use as a pair on instruction [%p]", currInst);
      }

   // Now that we've decided on a pair of real-regs to spill, we will need
   // the virtual regs to setup the spill information
   TR::Register * bestVirtCandidateHigh = bestCandidateHigh->getAssignedRegister();
   TR::Register * bestVirtCandidateLow = bestCandidateLow->getAssignedRegister();

   if (bestVirtCandidateLow != NULL)
      {
      // Check to see if the value has already been spilled
      locationLow = bestVirtCandidateLow->getBackingStorage();

      if (locationLow == NULL)
         {
         if (bestVirtCandidateLow->containsInternalPointer())
            {
            locationLow = self()->cg()->allocateInternalPointerSpill(bestVirtCandidateLow->getPinningArrayPointer());
            }
         else
            {
            locationLow = bestVirtCandidateLow->is64BitReg() ?
               self()->cg()->allocateSpill(8, bestVirtCandidateLow->containsCollectedReference(), NULL, true) :
               self()->cg()->allocateSpill(4, bestVirtCandidateLow->containsCollectedReference(), NULL, true);
            }

         bestVirtCandidateLow->setBackingStorage(locationLow);
         }

      TR::MemoryReference * tempMRLow = generateS390MemoryReference(currentNode, locationLow->getSymbolReference(), self()->cg());
      locationLow->getSymbolReference()->getSymbol()->setSpillTempLoaded();

      if (bestVirtCandidateLow->is64BitReg())
         {
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LG, currentNode, bestCandidateLow, tempMRLow, currInst);
         }
      else
         {
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::L, currentNode, bestCandidateLow, tempMRLow, currInst);
         }

      self()->cg()->traceRAInstruction(cursor);
      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, "Load Spill : reg pair even");
         }

      if ( !cursor->assignFreeRegBitVector() )
         {
         cursor->assignBestSpillRegister();
         }

      bestVirtCandidateLow->setAssignedRegister(NULL);

      if (!self()->cg()->isOutOfLineColdPath())
         {
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(bestVirtCandidateLow);

         // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
         // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
         // if we reverse spill this register inside the OOL cold/hot path
         if (!self()->cg()->isOutOfLineHotPath())
            {// main line
            locationLow->setMaxSpillDepth(1);
            }
         else
            {
            // hot path
            // do not overwrite main line spill depth
            if (locationLow->getMaxSpillDepth() != 1)
               locationLow->setMaxSpillDepth(2);
            }
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: adding reg pair low %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                          debugObj->getName(bestVirtCandidateLow), locationLow->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (locationLow->getMaxSpillDepth() != 1 &&
               locationLow->getMaxSpillDepth() != 2 )
            locationLow->setMaxSpillDepth(3);
         }
      }

   if (bestVirtCandidateHigh != NULL)
      {
      // Check to see if the value has already been spilled
      locationHigh = bestVirtCandidateHigh->getBackingStorage();

      if (locationHigh == NULL)
         {
         if (bestVirtCandidateHigh->containsInternalPointer())
            {
            locationHigh = self()->cg()->allocateInternalPointerSpill(bestVirtCandidateHigh->getPinningArrayPointer());
            }
         else
            {
            locationHigh = bestVirtCandidateHigh->is64BitReg() ?
               self()->cg()->allocateSpill(8, bestVirtCandidateHigh->containsCollectedReference(), NULL, true) :
               self()->cg()->allocateSpill(4, bestVirtCandidateHigh->containsCollectedReference(), NULL, true);
            }

         bestVirtCandidateHigh->setBackingStorage(locationHigh);
         }

      TR::MemoryReference * tempMRHigh = generateS390MemoryReference(currentNode, locationHigh->getSymbolReference(), self()->cg());
      locationHigh->getSymbolReference()->getSymbol()->setSpillTempLoaded();

      if (bestVirtCandidateHigh->is64BitReg())
         {
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LG, currentNode, bestCandidateHigh, tempMRHigh, currInst);
         }
      else
         {
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::L, currentNode, bestCandidateHigh, tempMRHigh, currInst);
         }

      self()->cg()->traceRAInstruction(cursor);
      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, "Load Spill : reg pair odd");
         }

      if ( !cursor->assignFreeRegBitVector() )
         {
         cursor->assignBestSpillRegister();
         }

      bestVirtCandidateHigh->setAssignedRegister(NULL);

      if (!self()->cg()->isOutOfLineColdPath())
         {
         // the spilledRegisterList contains all registers that are spilled before entering
         // the OOL cold path, post dependencies will be generated using this list
         self()->cg()->getSpilledRegisterList()->push_front(bestVirtCandidateHigh);

         // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
         // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
         // if we reverse spill this register inside the OOL cold/hot path
         if (!self()->cg()->isOutOfLineHotPath())
            {// main line
            locationHigh->setMaxSpillDepth(1);
            }
         else
            {
            // hot path
            // do not overwrite main line spill depth
            if (locationHigh->getMaxSpillDepth() != 1)
               locationHigh->setMaxSpillDepth(2);
            }
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: adding reg pair high %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                          debugObj->getName(bestVirtCandidateHigh), locationHigh->getMaxSpillDepth());
         }
      else
         {
         // do not overwrite mainline and hot path spill depth
         // if this spill is inside OOL cold path, we do not need to protecting the spill slot
         // because the post condition at OOL entry does not expect this register to be spilled
         if (locationHigh->getMaxSpillDepth() != 1 &&
               locationHigh->getMaxSpillDepth() != 2 )
            locationHigh->setMaxSpillDepth(3);
         }
      }


   // Mark associated real regs as free
   bestCandidateHigh->setState(TR::RealRegister::Free);
   bestCandidateLow->setState(TR::RealRegister::Free);
   bestCandidateHigh->setAssignedRegister(NULL);
   bestCandidateLow->setAssignedRegister(NULL);

   // Return free registers
   *firstReg = bestCandidateHigh;
   *lastReg = bestCandidateLow;
   }

////////////////////////////////////////////////////////////////////////////////
// SINGLE REGISTER assignment methods
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine::findBestFreeRegister
////////////////////////////////////////////////////////////////////////////////
TR::RealRegister *
OMR::Z::Machine::findBestFreeRegister(TR::Instruction   *currentInstruction,
                                     TR_RegisterKinds  rk,
                                     TR::Register      *virtualReg,
                                     uint64_t          availRegMask)
   {
   uint32_t interference = 0;
   int32_t first, maskI, last;
   uint32_t randomInterference;
   int32_t randomWeight;
   uint32_t randomPreference;
   TR::Compilation *comp = self()->cg()->comp();

   uint32_t preference = (virtualReg != NULL) ? virtualReg->getAssociation() : 0;

   bool useGPR0 = (virtualReg == NULL) ? false : (availRegMask & TR::RealRegister::GPR0Mask);
   bool liveRegOn = (self()->cg()->getLiveRegisters(rk) != NULL);

   if (comp->getOption(TR_Randomize))
      {
      randomPreference = preference;

      if (TR::RealRegister::isFPR((TR::RealRegister::RegNum)preference))
         {
         randomPreference = self()->cg()->randomizer.randomInt(TR::RealRegister::FirstFPR,TR::RealRegister::LastFPR);
         }
      else if (TR::RealRegister::isVRF((TR::RealRegister::RegNum)preference))
         {
         randomPreference = self()->cg()->randomizer.randomInt(TR::RealRegister::FirstVRF,TR::RealRegister::LastVRF);
         }
      else if (TR::RealRegister::isGPR((TR::RealRegister::RegNum)preference))
         {
         if (useGPR0)
            {
            randomPreference = self()->cg()->randomizer.randomInt(TR::RealRegister::GPR0,TR::RealRegister::LastGPR);
            }
         else
            {
            randomPreference = self()->cg()->randomizer.randomInt(TR::RealRegister::GPR1,TR::RealRegister::LastGPR);
            }
         }
       if (preference != randomPreference && performTransformation(comp,"O^O Random Codegen - Randomizing Preference from: %d to: %d\n", preference, randomPreference))
         {
         preference = randomPreference;
         }
      }

   uint64_t prefRegMask = TR::RealRegister::isRealReg((TR::RealRegister::RegNum)preference) ? _registerFile[preference]->getRealRegisterMask() : 0;

   if (liveRegOn && virtualReg != NULL)
      {
      interference = virtualReg->getInterference();
      if (comp->getOption(TR_Randomize))
         {
         randomInterference = self()->cg()->randomizer.randomInt(0, 65535);
         if (performTransformation(comp , "O^O Random Codegen - Randomizing Interference for %s: Original=%x Random=%x\n" , self()->cg()->getDebug()->getName(virtualReg) , interference , randomInterference))
            {
            interference = randomInterference;
            }
         }
      }

   if (!liveRegOn && useGPR0)
      {
      interference |= 1;
      }

   // We can't use FPRs for vector registers when current instruction is a call
   if(virtualReg->getKind() == TR_VRF && self()->cg()->getSupportsVectorRegisters() && currentInstruction->isCall())
     {
     for (int32_t i = TR::RealRegister::FPR0Mask; i <= TR::RealRegister::FPR15Mask; ++i)
       {
       availRegMask &= ~i;
       }
     }

   if (rk == TR_GPR)
      {
      maskI = first = TR::RealRegister::FirstGPR;
      last = TR::RealRegister::LastAssignableGPR;
      }
   else if (rk == TR_FPR)
      {
      maskI = first = TR::RealRegister::FirstFPR;
      last = TR::RealRegister::LastFPR;
      }
   else // VRF
      {
      maskI = first = TR::RealRegister::FirstVRF; // Partial VRF RA
      last = TR::RealRegister::LastVRF;
      }

   uint32_t bestWeightSoFar = 0xffffffff;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * bestRegister = NULL;
   TR::Register * realSibling = NULL;
   int32_t iOld = 0, iNew;

   /****************************************************************************************************************/
   /*            STEP 1                         Register Associations                                              */
   /****************************************************************************************************************/
   // Register Associations are best effort. If you really need to map a virtual to a real, use register pre/post dependency conditions.

   if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalEvenOfPair)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalOddRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = toRealRegister(toRealRegister(realSibling)->getSiblingRegister())->getRealRegisterMask();
         // If the sibling is assigned we choose the even register next to it
         if (availRegMask & sibRegMask)
            {
            bestRegister = _registerFile[toRealRegister(realSibling)->getRegisterNumber() - 1];
            }
         }
      else
         {
         if (bestRegister = self()->findBestLegalEvenRegister(availRegMask))
            // Set the pair's sibling to a high weight so that assignment to this real reg is unlikely
            {
            _registerFile[toRealRegister(bestRegister)->getRegisterNumber() + 1]->setWeight(S390_REGISTER_PAIR_SIBLING);
            }
         }

      /*
       * If the desired register is indeed free use it.
       * If not, default to standard search which is broken into 4 categories:
       * If register pair associations are enabled:
       *                   Preference
       *    1. TR::RealRegister::LegalEvenOfPair
       *    2. TR::RealRegister::LegalOddOfPair
       *    3. TR::RealRegister::LegalFirstOfFPPair
       *    4. TR::RealRegister::LegalSecondOfFPPair
       *    5. None of the above
       */
      if (bestRegister != NULL &&
            (bestRegister->getState() == TR::RealRegister::Free || bestRegister->getState() == TR::RealRegister::Unlatched))
         {
         if (bestRegister->getState() == TR::RealRegister::Unlatched)
            {
            bestRegister->setAssignedRegister(NULL);
            bestRegister->setState(TR::RealRegister::Free);
            }
         return bestRegister;
         }
      }
   else if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalOddOfPair)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalEvenRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = toRealRegister(toRealRegister(realSibling)->getSiblingRegister())->getRealRegisterMask();

         // If the sibling is assigned we choose the even register next to it
         if (availRegMask & sibRegMask)
            {
            bestRegister = _registerFile[toRealRegister(realSibling)->getRegisterNumber() + 1];
            }
         }
      else
         {
         if (bestRegister = self()->findBestLegalOddRegister(availRegMask))
            // Set the pair's sibling to a high weight so that assignment to this real reg is unlikely
            {
            _registerFile[toRealRegister(bestRegister)->getRegisterNumber() - 1]->setWeight(S390_REGISTER_PAIR_SIBLING);
            }
         }

      // If the desired register is indeed free use it.
      // If not, default to standard search
      if (bestRegister != NULL &&
            (bestRegister->getState() == TR::RealRegister::Free || bestRegister->getState() == TR::RealRegister::Unlatched))
         {
         if (bestRegister->getState() == TR::RealRegister::Unlatched)
            {
            bestRegister->setAssignedRegister(NULL);
            bestRegister->setState(TR::RealRegister::Free);
            }
         return bestRegister;
         }
      }
   else if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalFirstOfFPPair)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalSecondOfFPRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = toRealRegister(toRealRegister(realSibling)->getSiblingRegister())->getRealRegisterMask();
         // If the sibling is assigned we choose the even register next to it
         if (availRegMask & sibRegMask)
            {
            bestRegister = _registerFile[toRealRegister(realSibling)->getRegisterNumber() - 2];
            }
         }
      else
         {
         if (bestRegister = self()->findBestLegalSiblingFPRegister(true,availRegMask))
            // Set the pair's sibling to a high weight so that assignment to this real reg is unlikely
            {
            _registerFile[toRealRegister(bestRegister)->getRegisterNumber() + 2]->setWeight(S390_REGISTER_PAIR_SIBLING);
            }
         }

      // If the desired register is indeed free use it.
      // If not, default to standard search
      if (bestRegister != NULL &&
         (bestRegister->getState() == TR::RealRegister::Free || bestRegister->getState() == TR::RealRegister::Unlatched))
         {
         if (bestRegister->getState() == TR::RealRegister::Unlatched)
            {
            bestRegister->setAssignedRegister(NULL);
            bestRegister->setState(TR::RealRegister::Free);
            }
         return bestRegister;
         }
      }
   else if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalSecondOfFPPair)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalFirstOfFPRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = toRealRegister(toRealRegister(realSibling)->getSiblingRegister())->getRealRegisterMask();
         // If the sibling is assigned we choose the even register next to it
         if (availRegMask & sibRegMask)
            {
            bestRegister = _registerFile[toRealRegister(realSibling)->getRegisterNumber() + 2];
            }
         }
      else
         {
         if (bestRegister = self()->findBestLegalSiblingFPRegister(false,availRegMask))
            // Set the pair's sibling to a high weight so that assignment to this real reg is unlikely
            {
            _registerFile[toRealRegister(bestRegister)->getRegisterNumber() - 2]->setWeight(S390_REGISTER_PAIR_SIBLING);
            }
         }

      // If the desired register is indeed free use it.
      // If not, default to standard search
      if (bestRegister != NULL &&
         (bestRegister->getState() == TR::RealRegister::Free || bestRegister->getState() == TR::RealRegister::Unlatched))
         {
         if (bestRegister->getState() == TR::RealRegister::Unlatched)
            {
            bestRegister->setAssignedRegister(NULL);
            bestRegister->setState(TR::RealRegister::Free);
            }
         return bestRegister;
         }
      }
   else
      {
      // If preference register is also marked as an interference at some point, check to see if the interference still applies.
      if (liveRegOn && preference != 0 && (interference & (1 << (preference - maskI))))
         {
         if (!boundNext(currentInstruction, preference, virtualReg))
            {
            preference = 0;
            }
         prefRegMask = TR::RealRegister::isRealReg((TR::RealRegister::RegNum)preference) ? _registerFile[preference]->getRealRegisterMask() : 0;
         }

      // Check if the preferred register is free
      if ((prefRegMask & availRegMask) && _registerFile[preference] != NULL &&
            (_registerFile[preference]->getState() == TR::RealRegister::Free ||
               _registerFile[preference]->getState() == TR::RealRegister::Unlatched))
         {
         bestWeightSoFar = 0x0fffffff;
         bestRegister = _registerFile[preference];

         if (bestRegister->getState() == TR::RealRegister::Unlatched)
            {
            bestRegister->setAssignedRegister(NULL);
            bestRegister->setState(TR::RealRegister::Free);
            }

         self()->cg()->setRegisterAssignmentFlag(TR_ByAssociation);

         if (bestRegister != NULL)
            self()->cg()->traceRegisterAssignment("BEST FREE REG by pref for %R is %R", virtualReg, bestRegister);
         else
            self()->cg()->traceRegisterAssignment("BEST FREE REG by pref for %R is NULL", virtualReg);

         return bestRegister;
         }
      }

   /*******************************************************************************************************************/
   /*            STEP 2                         Good 'ol linear search                                              */
   /****************************************************************************************************************/
   // If no association or assoc fails, find any other free register
   for (int32_t i = first; i <= last; i++)
      {
      uint64_t tRegMask = _registerFile[i]->getRealRegisterMask();

      // Don't consider registers that can't be assigned.
      if ((_registerFile[i]->getState() == TR::RealRegister::Locked) || ((tRegMask & availRegMask) == 0))
         {
         continue;
         }
      //self()->cg()->traceRegWeight(_registerFile[i], _registerFile[i]->getWeight());

      iNew = interference & (1 << (i - maskI));
      if ((_registerFile[i]->getState() == TR::RealRegister::Free || (_registerFile[i]->getState() == TR::RealRegister::Unlatched)) &&
            (freeRegister == NULL || (iOld && !iNew) || ((iOld || !iNew) && _registerFile[i]->getWeight() < bestWeightSoFar)))
         {
         iOld = iNew;

         freeRegister = _registerFile[i];
         bestWeightSoFar = freeRegister->getWeight();
         if (comp->getOption(TR_Randomize))
            {
            randomWeight = self()->cg()->randomizer.randomInt(0, 0xFFF);
            if (performTransformation(comp, "O^O Random Codegen - Randomizing Weight for %s, Original bestWeightSoFar: %x randomized to: %x\n", self()->cg()->getDebug()->getName(_registerFile[i]), bestWeightSoFar, randomWeight))
               {
               bestWeightSoFar =  randomWeight;
               }
            }

         }
      }

   if (freeRegister != NULL && freeRegister->getState() == TR::RealRegister::Unlatched)
      {
      freeRegister->setAssignedRegister(NULL);
      freeRegister->setState(TR::RealRegister::Free);
      }

   if (freeRegister != NULL)
      self()->cg()->traceRegisterAssignment("BEST FREE REG for %R is %R", virtualReg, freeRegister);
   else
      self()->cg()->traceRegisterAssignment("BEST FREE REG for %R is NULL (could not find one)", virtualReg);


   return freeRegister;
   }

////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine::freeBestRegister
////////////////////////////////////////////////////////////////////////////////

/**
 * Construct a bit vector of all regs that are volatile or that have already been
 * assigned and are also FREE.
 */
uint64_t
OMR::Z::Machine::constructFreeRegBitVector(TR::Instruction  *currentInstruction)
   {
   TR::Linkage * linkage = self()->cg()->getS390Linkage();
   int32_t first = TR::RealRegister::FirstGPR + 1;  // skip GPR0
   int32_t last  = TR::RealRegister::LastAssignableGPR;
   uint64_t vector = 0;
   int32_t cnt         = 1;

   for (int32_t i=first; i<=last; i++)
      {
      TR::RealRegister * realReg = self()->getRealRegister(cnt+1);

      if ( realReg->getState() == TR::RealRegister::Free &&
           !currentInstruction->usesRegister(realReg)   &&
            ( realReg->getHasBeenAssignedInMethod() ||
              !linkage->getPreserved((TR::RealRegister::RegNum) (cnt+1)) )
         )
         {
         vector |= (0x1<<cnt);
         }
      cnt++;
      }

   return vector;
   }

/**
 * Find a register not used in the current instruction that is not LOCKED
 */
TR::RealRegister *
OMR::Z::Machine::findRegNotUsedInInstruction(TR::Instruction  *currentInstruction)
   {
   int32_t first = TR::RealRegister::FirstGPR + 1;
   int32_t last  = TR::RealRegister::LastAssignableGPR;
   TR::RealRegister * spill = NULL;

   for (int32_t i = first; i <= last, spill==NULL; i++)
      {
      TR::RealRegister * realReg = self()->getRealRegister((TR::RealRegister::RegNum) i);
      if ( realReg->getState() != TR::RealRegister::Locked && !currentInstruction->usesRegister(realReg))
         {
         spill = realReg;
         }
      }

   TR_ASSERT( spill != NULL, "OMR::Z::Machine::findRegNotUsedInInstruction -- A spill reg should always be found.");

   return spill;
   }

/**
 * Spill all high regs on calls
 */
void
OMR::Z::Machine::spillAllVolatileHighRegisters(TR::Instruction *currentInstruction)
   {
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last  = TR::RealRegister::LastGPR;
   TR::Machine *machine = self()->cg()->machine();
   TR::Linkage * linkage = self()->cg()->getS390Linkage();
   TR::Node * node = currentInstruction->getNode();

   for (int32_t i = first; i <= last; i++)
      {
      TR::Register * virtReg = NULL;
      TR::RealRegister * realReg =
         machine->getRealRegister((TR::RealRegister::RegNum) i);
      bool volatileReg = true;     // All high regs are volatile
                                   // !linkage->getPreserved((TR::RealRegister::RegNum) i);

      if (volatileReg                                      &&
          realReg->getState() == TR::RealRegister::Assigned &&
          (virtReg = realReg->getAssignedRegister())       &&
          virtReg->is64BitReg()
         )
         {
         self()->spillRegister(currentInstruction, virtReg);
         }
      }
   }

////////////////////////////////////////////////////

TR::RealRegister *
OMR::Z::Machine::freeBestRegister(TR::Instruction * currentInstruction, TR::Register * virtReg, TR_RegisterKinds rk, bool allowNullReturn)
   {
   self()->cg()->traceRegisterAssignment("FREE BEST REGISTER FOR %R", virtReg);
   TR::Compilation *comp = self()->cg()->comp();

   if (virtReg->containsCollectedReference())
      self()->cg()->traceRegisterAssignment("%R contains collected", virtReg);
   int32_t numCandidates = 0, interference = 0, first, last, maskI;
   TR::Register * candidates[TR::RealRegister::LastVRF];
   TR::Machine *machine = self()->cg()->machine();
   bool useGPR0 = (virtReg == NULL) ? false : (virtReg->isUsedInMemRef() == false);

   switch (rk)
      {
      case TR_GPR:
         maskI = first = TR::RealRegister::FirstGPR;
         last = TR::RealRegister::LastAssignableGPR;
         if (useGPR0 == false)
            {
            maskI++;
            first++;
            }
         break;
      case TR_FPR:
         maskI = first = TR::RealRegister::FirstFPR;
         last = TR::RealRegister::LastFPR;
         break;
      case TR_VRF:
         maskI = first = TR::RealRegister::FirstVRF;
         last = TR::RealRegister::LastVRF;
         break;
      }

   int32_t preference = 0, pref_favored = 0;
   if (self()->cg()->getLiveRegisters(rk) != NULL && virtReg != NULL)
      {
      interference = virtReg->getInterference();
      // interference might not be acurate beacuse not all vRegs clobber high words
      // todo: merge with boundnext
      preference = virtReg->getAssociation();

      // Consider yielding
      if (preference != 0 && boundNext(currentInstruction, preference, virtReg))
         {
         pref_favored = 1;
         interference &= ~(1 << (preference - maskI));
         }
      }

   TR::Register *tempReg = NULL;
   for (int32_t i = first; i <= last; i++)
      {
      int32_t iInterfere = interference & (1 << (i - maskI));
      TR::RealRegister * realReg = machine->getRealRegister((TR::RealRegister::RegNum) i);

      // TODO: This assert was added as it existed in a path which was guarded by is64BitReg. I'm fairly certain
      // this assert should never fire because otherwise we would be trying to free the best register when a free
      // register already exists, meaning that the caller is to blame for not checking if a free register was
      // available. An alternative would be to just return the free register right away? In either event we'll
      // leave this assert here for a little while and we can remove it once it has had time to bake.
      TR_ASSERT_FATAL(realReg->getState() != TR::RealRegister::Free, "Attempting to free best register for virtual register (%s) when a free register (%s) already exists",
         getRegisterName(virtReg, self()->cg()),
         getRegisterName(realReg, self()->cg()));

      if (realReg->getState() == TR::RealRegister::Assigned)
         {
         TR::Register * associatedVirtual = realReg->getAssignedRegister();
         bool          usedInMemRef      = associatedVirtual->isUsedInMemRef();

         if ((!iInterfere && i==preference && pref_favored) || !usedInMemRef)
            {
            if (numCandidates == 0)
               {
               candidates[0] = associatedVirtual;
               }
            else
               {
               tempReg       = candidates[0];
               candidates[0] = associatedVirtual;
               candidates[numCandidates] = tempReg;
               }
            }
         else
            {
            candidates[numCandidates] = associatedVirtual;
            }
         numCandidates++;
         }
      }

   if (numCandidates == 0)
      {
      if (!allowNullReturn)
         {
         if (self()->cg()->getDebug() != NULL)
            {
            self()->cg()->getDebug()->printGPRegisterStatus(comp->getOutFile(), machine);
            }

         TR_ASSERT_FATAL(false, "Ran out of register candidates to free on instruction [%p]", currentInstruction);
         }

      return NULL;
      }

   TR::Instruction * cursor = currentInstruction->getPrev();
   while (numCandidates > 1 && cursor != NULL && cursor->getOpCodeValue() != TR::InstOpCode::label && cursor->getOpCodeValue() != TR::InstOpCode::proc)
      {
      for (int32_t i = 0; i < numCandidates; i++)
         {
         if (cursor->refsRegister(candidates[i]))
            {
            candidates[i] = candidates[--numCandidates];
            i--; // on continue repeat test for candidate[i] as candidate[i] is now new.
            }
         }
      cursor = cursor->getPrev();
      }

   TR::RealRegister * best = toRealRegister(candidates[0]->getAssignedRegister());

   self()->spillRegister(currentInstruction, candidates[0]);

   return best;
   }

/**
 *  Spill in a virt register, which frees up the assigned real reg, as we do
 *  assignment in reverse-order of execution.
 *
 *  ...   GPRX is now free
 *  L GPRX, #MEMREF   // Undo spill of virtual reg
 *  A GPRX, ....      // use GPRX
 */
void
OMR::Z::Machine::spillRegister(TR::Instruction * currentInstruction, TR::Register* virtReg)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR::InstOpCode::Mnemonic opCode;
   bool containsInternalPointer = false;
   bool containsCollectedReg    = false;
   TR_RegisterKinds rk = virtReg->getKind();
   TR_BackingStore * location = NULL;
   TR::Node * currentNode = currentInstruction->getNode();
   TR::Instruction * cursor = NULL;
   TR::RealRegister * best = NULL;
   TR_Debug * debugObj = self()->cg()->getDebug();

   best = toRealRegister(virtReg->getAssignedRegister());

   if (virtReg->containsInternalPointer())
      {
      containsInternalPointer = true;
      }

   if (virtReg->containsCollectedReference())
      {
      containsCollectedReg = true;
      }
   if (debugObj)
      {
      self()->cg()->traceRegisterAssignment("SPILLING REGISTER %R", virtReg);
      if (virtReg->is64BitReg())
         self()->cg()->traceRegisterAssignment("%R is 64 bit", virtReg);
      else
         self()->cg()->traceRegisterAssignment("%R is 32 bit", virtReg);
      if (containsCollectedReg)
         self()->cg()->traceRegisterAssignment("%R contains collected", virtReg);
      }

   location = virtReg->getBackingStorage();
   switch (rk)
     {
     case TR_GPR:
       if ((self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) && virtReg->getBackingStorage())
         {
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       else if (!containsInternalPointer)
         {
         location = virtReg->is64BitReg() ?
            self()->cg()->allocateSpill(8, virtReg->containsCollectedReference(), NULL, true) :
            self()->cg()->allocateSpill(4, virtReg->containsCollectedReference(), NULL, true);

         if (debugObj)
             self()->cg()->traceRegisterAssignment("\nSpilling %s to (%p)\n",debugObj->getName(virtReg),location);
         }
       else
         {
         location = self()->cg()->allocateInternalPointerSpill(virtReg->getPinningArrayPointer());
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling internal pointer %s to (%p)\n", debugObj->getName(virtReg),location);
         }

       opCode = virtReg->is64BitReg() ?
          TR::InstOpCode::LG :
          TR::InstOpCode::L;

       break;
     case TR_FPR:
       if ((self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) && virtReg->getBackingStorage())
         {
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       else
         {
         location = self()->cg()->allocateSpill(8, false, NULL, true); // TODO: Use 4 for single-precision values
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling FPR %s to (%p)\n", debugObj->getName(virtReg),location);
         }
       opCode = TR::InstOpCode::LD;
       break;
     case TR_VRF:
       // Spill of size 16 has never been done before. The call hierarchy seems to support it but this should be watched closely.
       location = self()->cg()->allocateSpill(16, false, NULL, true);
       if (debugObj)
          self()->cg()->traceRegisterAssignment("\nSpilling VRF %s to (%p)\n", debugObj->getName(virtReg), location);

       opCode = TR::InstOpCode::VL;
       break;
     }

   TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), self()->cg());
   location->getSymbolReference()->getSymbol()->setSpillTempLoaded();
   virtReg->setBackingStorage(location);

   if (opCode == TR::InstOpCode::VL)
      cursor = generateVRXInstruction(self()->cg(), opCode, currentNode, best, tempMR, 0, currentInstruction);
   else
      cursor = generateRXInstruction(self()->cg(), opCode, currentNode, best, tempMR, currentInstruction);

   self()->cg()->traceRAInstruction(cursor);
   if (debugObj)
      {
      debugObj->addInstructionComment(cursor, "Load Spill");
      }

   //  Local local allocation
   if ( !cursor->assignFreeRegBitVector() )
      {
      cursor->assignBestSpillRegister();
      }

   if (!self()->cg()->isOutOfLineColdPath())
      {
      // the spilledRegisterList contains all registers that are spilled before entering
      // the OOL cold path, post dependencies will be generated using this list
      self()->cg()->getSpilledRegisterList()->push_front(virtReg);

      // OOL cold path: depth = 3, hot path: depth =2,  main line: depth = 1
      // if the spill is outside of the OOL cold/hot path, we need to protect the spill slot
      // if we reverse spill this register inside the OOL cold/hot path
      if (!self()->cg()->isOutOfLineHotPath())
         {// main line
         location->setMaxSpillDepth(1);
         }
      else
         {
         // hot path
         // do not overwrite main line spill depth
         if (location->getMaxSpillDepth() != 1)
            location->setMaxSpillDepth(2);
         }
      if (debugObj)
         self()->cg()->traceRegisterAssignment("OOL: adding %s to the spilledRegisterList, maxSpillDepth = %d\n",
                                       debugObj->getName(virtReg), location->getMaxSpillDepth());
      }
   else
      {
      // do not overwrite mainline and hot path spill depth
      // if this spill is inside OOL cold path, we do not need to protecting the spill slot
      // because the post condition at OOL entry does not expect this register to be spilled
      if (location->getMaxSpillDepth() != 1 &&
            location->getMaxSpillDepth() != 2 )
         location->setMaxSpillDepth(3);
      }

   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);

   virtReg->setAssignedRegister(NULL);
   }


////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine::reverseSpillState
////////////////////////////////////////////////////////////////////////////////
TR::RealRegister *
OMR::Z::Machine::reverseSpillState(TR::Instruction      *currentInstruction,
                                  TR::Register         *spilledRegister,
                                  TR::RealRegister *targetRegister)
   {
   TR_ASSERT_FATAL(spilledRegister->getAssignedRegister() == NULL, "Attempting to fill an already assigned virtual register (%s)", getRegisterName(spilledRegister, self()->cg()));

   TR_BackingStore * location = spilledRegister->getBackingStorage();
   TR::Node * currentNode = currentInstruction->getNode();
   TR_RegisterKinds rk = spilledRegister->getKind();

   TR::InstOpCode::Mnemonic opCode;
   int32_t dataSize;
   TR::Instruction * cursor = NULL;
   TR_Debug * debugObj = self()->cg()->getDebug();
   TR::Compilation *comp = self()->cg()->comp();
   //This may not actually need to be reversed if
   //this is a dummy register used for OOL dependencies

   self()->cg()->traceRegisterAssignment("REVERSE SPILL STATE FOR %R", spilledRegister);

   if (spilledRegister->isPlaceholderReg())
      {
      return NULL;
      }

   // no real reg is assigned to targetRegister yet
   if (targetRegister == NULL)
      {
      // find a free register and assign
      uint64_t regMask = 0xffffffff;
      if (spilledRegister->isUsedInMemRef())
         regMask = ~TR::RealRegister::GPR0Mask;

      targetRegister = self()->findBestFreeRegister(currentInstruction, spilledRegister->getKind(), spilledRegister, regMask);
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestRegister(currentInstruction, spilledRegister, spilledRegister->getKind());
         }
      }

   if (self()->cg()->isOutOfLineColdPath())
      {
      // the future and total use count might not always reflect register spill state
      // for example a new register assignment in the hot path would cause FC != TC
      // in this case, assign a new register and return
      if (location == NULL)
         {
         if (debugObj)
            {
            self()->cg()->traceRegisterAssignment("OOL: Not generating reverse spill for (%s)\n", debugObj->getName(spilledRegister));
            }

         targetRegister->setState(TR::RealRegister::Assigned);
         targetRegister->setAssignedRegister(spilledRegister);
         spilledRegister->setAssignedRegister(targetRegister);

         return targetRegister;
         }
      }

   if (location == NULL)
      {
      if (rk == TR_GPR)
         {
         location = spilledRegister->is64BitReg() ?
            self()->cg()->allocateSpill(8, spilledRegister->containsCollectedReference(), NULL, true) :
            self()->cg()->allocateSpill(4, spilledRegister->containsCollectedReference(), NULL, true);
         }
      else if (rk == TR_VRF)
         {
         location = self()->cg()->allocateSpill(16, false, NULL, true);
         }
      else
         {
         location = self()->cg()->allocateSpill(8, false, NULL, true); // TODO: Use 4 for single-precision values
         }
      spilledRegister->setBackingStorage(location);
      }

   targetRegister->setState(TR::RealRegister::Assigned);
   targetRegister->setAssignedRegister(spilledRegister);
   spilledRegister->setAssignedRegister(targetRegister);

   TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), self()->cg());

   switch (rk)
      {
      case TR_GPR:
         dataSize = TR::Compiler->om.sizeofReferenceAddress();
         opCode = TR::InstOpCode::ST;

         if (spilledRegister->is64BitReg())
            {
            dataSize = 8;
            opCode = TR::InstOpCode::STG;
            }
         break;
      case TR_FPR:
         dataSize = 8;
         opCode = TR::InstOpCode::STD;
         break;
      case TR_VRF:
         dataSize = 16;
         opCode = TR::InstOpCode::VST;
         break;
      }

   if (self()->cg()->isOutOfLineColdPath())
      {
      bool isOOLentryReverseSpill = false;
      if (currentInstruction->isLabel())
         {
         if (toS390LabelInstruction(currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream())
            {
            // indicates that we are at OOL entry point post conditions. Since
            // we are now exiting the OOL cold path (going reverse order)
            // and we called reverseSpillState(), the main line path
            // expects the Virt reg to be assigned to a real register
            // we can now safely unlock the protected backing storage
            // This prevents locking backing storage for future OOL blocks
            isOOLentryReverseSpill = true;
            }
         }

      // OOL: only free the spill slot if the register was spilled in the same or less dominant path
      // ex: spilled in cold path, reverse spill in hot path or main line
      // we have to spill this register again when we reach OOL entry point due to post
      // conditions. We want to guarantee that the same spill slot will be protected and reused.
      // maxSpillDepth: 3:cold path, 2:hot path, 1:main line
      if (location->getMaxSpillDepth() == 0 || location->getMaxSpillDepth() == 3 || isOOLentryReverseSpill)
         {
         location->setMaxSpillDepth(0);
         self()->cg()->freeSpill(location, dataSize, 0);
         spilledRegister->setBackingStorage(NULL);
         }
      else
         {
         if (debugObj)
            self()->cg()->traceRegisterAssignment("\nOOL: reverse spill %s in less dominant path (%d / 3), protect spill slot (%p)\n",
                                          debugObj->getName(spilledRegister), location->getMaxSpillDepth(), location);
         }
      }
   else if (self()->cg()->isOutOfLineHotPath())
      {
      // the spilledRegisterList contains all registers that are spilled before entering
      // the OOL path (in backwards RA). Post dependencies will be generated using this list.
      // Any registers reverse spilled before entering OOL should be removed from the spilled list
      if (debugObj)
         self()->cg()->traceRegisterAssignment("\nOOL: removing %s from the spilledRegisterList)\n", debugObj->getName(spilledRegister));
      self()->cg()->getSpilledRegisterList()->remove(spilledRegister);
      if (location->getMaxSpillDepth() == 2)
         {
         location->setMaxSpillDepth(0);
         self()->cg()->freeSpill(location, dataSize, 0);
         spilledRegister->setBackingStorage(NULL);
         }
      else
         {
         if (debugObj)
            self()->cg()->traceRegisterAssignment("\nOOL: reverse spilling %s in less dominant path (%d / 2), protect spill slot (%p)\n",
                                          debugObj->getName(spilledRegister), location->getMaxSpillDepth(), location);
         location->setMaxSpillDepth(0);
         }
      }
   else // main line
      {
      if (debugObj)
         self()->cg()->traceRegisterAssignment("\nOOL: removing %s from the spilledRegisterList)\n", debugObj->getName(spilledRegister));
      self()->cg()->getSpilledRegisterList()->remove(spilledRegister);
      location->setMaxSpillDepth(0);
      self()->cg()->freeSpill(location, dataSize, 0);
      spilledRegister->setBackingStorage(NULL);
      }

   if (opCode == TR::InstOpCode::VST)
      cursor = generateVRXInstruction(self()->cg(), opCode, currentNode, targetRegister, tempMR, 0, currentInstruction);
   else
      cursor = generateRXInstruction(self()->cg(), opCode, currentNode, targetRegister, tempMR, currentInstruction);

   self()->cg()->traceRAInstruction(cursor);

   if (debugObj)
      {
      debugObj->addInstructionComment(cursor, "Spill");
      }
   if ( !cursor->assignFreeRegBitVector() )
      {
      cursor->assignBestSpillRegister();
      }

   return targetRegister;
   }

/**
 * We check if the virtual can be assigned to the given real
 * Encode rules for illegal assignment.
 */
bool
OMR::Z::Machine::isAssignable(TR::Register * virtReg, TR::RealRegister * realReg)
   {
   // virtReg used in memory reference can not be assigned to GPR0.
   return !(virtReg->isUsedInMemRef() && realReg == _registerFile[TR::RealRegister::GPR0]);
   }

/**
 * Assign a specific real register to a virtual register
 *
 *                       REAL                  VIRTUAL
 *     Source       currentAssignedRegister  virtualRegister
 *     Target       targetRegister           currentTargetVirtual
 */
TR::Instruction *
OMR::Z::Machine::coerceRegisterAssignment(TR::Instruction                            *currentInstruction,
                                         TR::Register                               *virtualRegister,
                                         TR::RealRegister::RegNum  registerNumber)
   {
   TR::RealRegister * targetRegister = _registerFile[registerNumber];
   TR::RealRegister * realReg = virtualRegister->getAssignedRealRegister();
   TR::RealRegister * currentAssignedRegister = (realReg == NULL) ? NULL : toRealRegister(realReg);
   TR::RealRegister * spareReg = NULL;
   TR::Register * currentTargetVirtual = NULL;
   TR_RegisterKinds rk = virtualRegister->getKind();

   TR::Instruction * cursor = NULL;
   TR::Node * currentNode = currentInstruction->getNode();
   bool doNotRegCopy = false;
   TR::Compilation *comp = self()->cg()->comp();

   virtualRegister->setIsLive();

   self()->cg()->traceRegisterAssignment("COERCE %R into %R", virtualRegister, targetRegister);

   if (rk != TR_FPR && rk != TR_VRF)
      {
      if (virtualRegister->is64BitReg())
         {
         self()->cg()->traceRegisterAssignment(" coerceRA: %R needs 64 bit reg ", virtualRegister);
         }
      else
         {
         self()->cg()->traceRegisterAssignment(" coerceRA: %R needs 32 bit reg ", virtualRegister);
         }
      }

   // already assign with the one we want
   if (currentAssignedRegister == targetRegister)
      {
      // All is good, so do nothing except set all pointers
      // and state flags appropriately.
      }
   // the target reg is free
   else if (targetRegister->getState() == TR::RealRegister::Free || targetRegister->getState() == TR::RealRegister::Unlatched)
      {
      if (virtualRegister->isPlaceholderReg())
        targetRegister->setIsAssignedMoreThanOnce(); // Register is killed invalidate it for moving spill out of loop
      self()->cg()->traceRegisterAssignment("target %R is free", targetRegister);

      // the virtual register haven't be assigned to any real register yet
      if (currentAssignedRegister == NULL)
         {
         // Vector registers coercion will most likely take this path

         // get the value of virtual register back from spill state if not first use
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (self()->cg()->isOutOfLineColdPath())
               {
               self()->cg()->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
               }
            }
         }
      else
         {
         // virtual register is currently assigned to a different register,
         // override it with the target reg
         cursor = self()->registerCopy(self()->cg(), rk, currentAssignedRegister, targetRegister, currentInstruction);

         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      }
   // the target reg is blocked so we need to guarantee it stays in a reg
   else if (targetRegister->getState() == TR::RealRegister::Blocked)
      {
      currentTargetVirtual = targetRegister->getAssignedRegister();
      self()->cg()->traceRegisterAssignment("target %R is blocked, assigned to %R", targetRegister, currentTargetVirtual);
      uint64_t regMask = 0xffffffff;
      if (currentTargetVirtual->isUsedInMemRef())
         regMask = ~TR::RealRegister::GPR0Mask;
      spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual, regMask);

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);

      // We may need spare reg no matter what
      if ( spareReg == NULL )
         {
         self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
         virtualRegister->block();
         currentTargetVirtual->block();

         spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), true);

         virtualRegister->unblock();
         currentTargetVirtual->unblock();
         }

      // find a free register if the virtual register hasn't been assigned to any real register
      // or it is a FPR for later use
      // virtual register is currently assigned to a different register,
      if (currentAssignedRegister != NULL)
         {
         if (!self()->isAssignable(currentTargetVirtual, currentAssignedRegister))
            {
               {
               /**
                * Register exchange API takes care of exchanging between registers.
                * In rare case, where virtual register to which real target register
                * is assigned to is used in memory reference and currentAssignedRegister
                * is GPR0, we can not do register exchange and in that case we would need
                * spareReg.
                * Currently we do not have any solution when we hit this scenario so
                * instead of failing the JVM with Assert, fail the compilation.
                */
               if (spareReg == NULL)
                  {
                  comp->failCompilation<TR::CompilationException>("Abort compilation as we can not find a spareReg for blocked target real register");
                  }
               self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(self()->cg(), rk, targetRegister, spareReg, currentInstruction);
               cursor = self()->registerCopy(self()->cg(), rk, currentAssignedRegister, targetRegister, currentInstruction);

               spareReg->setState(TR::RealRegister::Assigned);
               currentTargetVirtual->setAssignedRegister(spareReg);
               spareReg->setAssignedRegister(currentTargetVirtual);

               targetRegister->setState(TR::RealRegister::Unlatched);
               targetRegister->setAssignedRegister(NULL);

               currentAssignedRegister->setState(TR::RealRegister::Unlatched);
               currentAssignedRegister->setAssignedRegister(NULL);
               }
            }
         else
            {
            self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);

            cursor = self()->registerExchange(self()->cg(), rk, targetRegister, currentAssignedRegister, spareReg, currentInstruction);

            currentAssignedRegister->setState(TR::RealRegister::Blocked);
            currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
            }
         }
      else
         {
         self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);

         // virtual register is not assigned yet, copy register
         cursor = self()->registerCopy(self()->cg(), rk, targetRegister, spareReg, currentInstruction);

         spareReg->setState(TR::RealRegister::Assigned);
         spareReg->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(spareReg);

         targetRegister->setState(TR::RealRegister::Unlatched);
         targetRegister->setAssignedRegister(NULL);

         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (self()->cg()->isOutOfLineColdPath())
               {
               self()->cg()->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
               }
            }
         // spareReg is assigned.
         }
      }
   // the target reg is assigned
   else if (targetRegister->getState() == TR::RealRegister::Assigned)
      {
      //  Since target is assigned, it must have a virtReg associated to it
      currentTargetVirtual = targetRegister->getAssignedRegister();
      self()->cg()->traceRegisterAssignment("target %R is assigned, assigned to %R", targetRegister, currentTargetVirtual);

      if (rk != TR_FPR && rk != TR_VRF && currentTargetVirtual)
         {
         // this happens for OOL spill, simply return
         if (currentTargetVirtual == virtualRegister)
            {
            virtualRegister->setAssignedRegister(targetRegister);
            self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
            self()->cg()->clearRegisterAssignmentFlags();
            return cursor;
            }
         }
      uint64_t regMask = 0xffffffff;
      if (currentTargetVirtual->isUsedInMemRef())
         regMask = ~TR::RealRegister::GPR0Mask;
      spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual, regMask);

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);

      // If the source register is already assigned a realReg, we will try and
      // keep both source and target in real regs by:
      //   1. exchanging the two realRegs
      //   2. moving the target to a different real, called spareReg.
      // In case we do need to spill, we let the spill policy defined in freeBestReg
      // choose the best spill candidate.
      if (currentAssignedRegister != NULL)
         {
         //  We may not be able to do an exchange as the target virtReg is not
         //  allowed to be assigned to the source's realReg (e.g. GPR0).
         if (!self()->isAssignable(currentTargetVirtual, currentAssignedRegister) || (rk != TR_FPR && rk != TR_VRF))
            {
            // There is an alternative to blindly spilling because:
            //   1. there was a FREE reg
            //   2. freeBestReg found a better choice to be spilled
            if (spareReg == NULL)
               {
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);

               //  The current source reg's assignment is automatically blocked out
               virtualRegister->block();
               if (virtualRegister->is64BitReg())
                  {
                  // TODO: Can we allow a null return here? Why are the two paths different?
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind());
                  }
               else
                  {
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), true);
                  }

               // For some reason (blocked/locked regs etc), we couldn't find a spare reg so spill the virtual in the target and use it for coercion
               if (spareReg == NULL)
                  {
                  self()->spillRegister(currentInstruction, currentTargetVirtual);
                  targetRegister->setAssignedRegister(virtualRegister);
                  virtualRegister->setAssignedRegister(targetRegister);
                  targetRegister->setState(TR::RealRegister::Assigned);
                  }

               virtualRegister->unblock();
               }

            // Spill policy decided the best reg to spill was not the targetReg, so move target
            // to the spareReg, and move the source reg to the target.
            if (targetRegister->getRegisterNumber() != spareReg->getRegisterNumber() && !doNotRegCopy)
               {
               self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(self()->cg(), rk, targetRegister, spareReg, currentInstruction);

               targetRegister->setState(TR::RealRegister::Unlatched);
               targetRegister->setAssignedRegister(NULL);

               spareReg->setState(TR::RealRegister::Assigned);
               spareReg->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(spareReg);
               }

            cursor = self()->registerCopy(self()->cg(), rk, currentAssignedRegister, targetRegister, currentInstruction);
            currentAssignedRegister->setState(TR::RealRegister::Unlatched);
            currentAssignedRegister->setAssignedRegister(NULL);
            }
         else
            {
            // Look through reg list and pick the best dummy reg as a swap reg.
            // A dummy reg is better than a free reg, so clobber spareReg if succesfull.
            TR::RealRegister* reg = NULL;
            if (targetRegister->getAssignedRegister() == currentTargetVirtual &&
                currentTargetVirtual->getAssignedRegister() == NULL)
               // to prevent exception in findBestSwapRegister
               currentTargetVirtual->setAssignedRegister(targetRegister);

            if (reg = self()->findBestSwapRegister(virtualRegister, currentTargetVirtual))
               {
               spareReg = reg;
               }

            // If we were unable to find a spareReg,  just spill the target
            if (spareReg == NULL)
               {
               self()->spillRegister(currentInstruction, currentTargetVirtual);

               self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);

               cursor = self()->registerCopy(self()->cg(), rk, currentAssignedRegister, targetRegister, currentInstruction);
               currentAssignedRegister->setState(TR::RealRegister::Unlatched);
               currentAssignedRegister->setAssignedRegister(NULL);
               }
            else
               {
               cursor = self()->registerExchange(self()->cg(), rk, targetRegister, currentAssignedRegister, spareReg, currentInstruction);
               currentAssignedRegister->setState(TR::RealRegister::Assigned);
               currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(currentAssignedRegister);

               }
            }
         }
      else
         {

         // If there wasn't a free reg caught by spareReg and we are calling out,
         // and we are coercing to get a dummy reg, it is likely better to just
         // spill the reg than try and move the expression to a non-volatile
         // register and then spill it.
         //
         if (spareReg == NULL && virtualRegister->isPlaceholderReg())
            {
            self()->spillRegister(currentInstruction, currentTargetVirtual);

            targetRegister->setState(TR::RealRegister::Unlatched);
            targetRegister->setAssignedRegister(NULL);
            }
         else
            {
            // If we didn't find a FREE reg, choose the best one to spill.
            // The worst case situation is that only the target is left to spill.
            if (spareReg == NULL)
               {
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);

               virtualRegister->block();
               if (virtualRegister->is64BitReg())
                  {
                  // TODO: Can we allow a null return here? Why are the two paths different? There is a similar case
                  // above.
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind());
                  }
               else
                  {
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), true);
                  }

               // For some reason (blocked/locked regs etc), we couldn't find a spare reg so spill the virtual in the target and use it for coercion
               if (spareReg == NULL)
               {
               self()->spillRegister(currentInstruction, currentTargetVirtual);
               targetRegister->setAssignedRegister(virtualRegister);
               virtualRegister->setAssignedRegister(targetRegister);
               targetRegister->setState(TR::RealRegister::Assigned);
               }

               virtualRegister->unblock();
               }

            //  If we chose to spill a reg that wasn't the target, we use the new space
            //  to free up the target.
            if (targetRegister->getRegisterNumber() != spareReg->getRegisterNumber() && !doNotRegCopy)
               {
               self()->cg()->resetRegisterAssignmentFlag(TR_RegisterSpilled);
               self()->cg()->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(self()->cg(), rk, targetRegister, spareReg, currentInstruction);

               spareReg->setState(TR::RealRegister::Assigned);
               spareReg->setAssignedRegister(currentTargetVirtual);

               targetRegister->setState(TR::RealRegister::Unlatched);
               targetRegister->setAssignedRegister(NULL);

               currentTargetVirtual->setAssignedRegister(spareReg);
               self()->cg()->recordRegisterAssignment(spareReg,currentTargetVirtual);
               }

            //  Draw the source reg back out of SPILL state
            if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
               {
               self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
               self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
               }
            else
               {
               if (self()->cg()->isOutOfLineColdPath())
                  {
                  self()->cg()->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
                  }
               }
            }
         }

      self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
      }

   //  We will allow Locked regs to be pointed to, but do not allow
   //  the locked reg to point back to the source.  This is specifically
   //  implemented for the ext code base, which may or may not be locked.
   //  In the case that it is locked, we hard code the virtual to GPR7
   //  using the singly linked assignement.  In the case that it isn't
   //  locked,  things are treaded as usual.
   if (targetRegister->getState() != TR::RealRegister::Locked)
      {
      targetRegister->setState(TR::RealRegister::Assigned);
      targetRegister->setAssignedRegister(virtualRegister);
      }
   else
      {
      traceMsg(comp, "    WARNING: Assigning a Locked register %s to %s\n",
                                         getRegisterName(targetRegister,self()->cg()),
                                         getRegisterName(virtualRegister,self()->cg()));
      traceMsg(comp, "             This assignment is equivalent to using a hard coded real register.\n");

      // the virtual register haven't be assigned to any real register yet
      if (currentAssignedRegister == NULL)
         {
         // get the value of virtual register back from spill state if not first use
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (self()->cg()->isOutOfLineColdPath())
               {
               self()->cg()->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
               }
            }
         }
      else
         {
         // virtual register is currently assigned to a different register,
         // override it with the target reg
         cursor = self()->registerCopy(self()->cg(), rk, currentAssignedRegister, targetRegister, currentInstruction);

         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      }

   virtualRegister->setAssignedRegister(targetRegister);

   self()->cg()->traceRegAssigned(virtualRegister, targetRegister);

   self()->cg()->clearRegisterAssignmentFlags();
   return cursor;
   }


////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine::initializeRegisterFile
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::Machine::initializeRegisterFile()
   {

   // Initialize GPRs
   _registerFile[TR::RealRegister::NoReg] = NULL;

   _registerFile[TR::RealRegister::GPR0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR0, TR::RealRegister::GPR0Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR1, TR::RealRegister::GPR1Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR2, TR::RealRegister::GPR2Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR3, TR::RealRegister::GPR3Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR4, TR::RealRegister::GPR4Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR5, TR::RealRegister::GPR5Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR6, TR::RealRegister::GPR6Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR7, TR::RealRegister::GPR7Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR8, TR::RealRegister::GPR8Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR9, TR::RealRegister::GPR9Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR10, TR::RealRegister::GPR10Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR11, TR::RealRegister::GPR11Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR12, TR::RealRegister::GPR12Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR13, TR::RealRegister::GPR13Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR14, TR::RealRegister::GPR14Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR15, TR::RealRegister::GPR15Mask, self()->cg());

   _registerFile[TR::RealRegister::GPR0]->setSiblingRegister(_registerFile[TR::RealRegister::GPR1]);
   _registerFile[TR::RealRegister::GPR1]->setSiblingRegister(_registerFile[TR::RealRegister::GPR0]);
   _registerFile[TR::RealRegister::GPR2]->setSiblingRegister(_registerFile[TR::RealRegister::GPR3]);
   _registerFile[TR::RealRegister::GPR3]->setSiblingRegister(_registerFile[TR::RealRegister::GPR2]);
   _registerFile[TR::RealRegister::GPR4]->setSiblingRegister(_registerFile[TR::RealRegister::GPR5]);
   _registerFile[TR::RealRegister::GPR5]->setSiblingRegister(_registerFile[TR::RealRegister::GPR4]);
   _registerFile[TR::RealRegister::GPR6]->setSiblingRegister(_registerFile[TR::RealRegister::GPR7]);
   _registerFile[TR::RealRegister::GPR7]->setSiblingRegister(_registerFile[TR::RealRegister::GPR6]);
   _registerFile[TR::RealRegister::GPR8]->setSiblingRegister(_registerFile[TR::RealRegister::GPR9]);
   _registerFile[TR::RealRegister::GPR9]->setSiblingRegister(_registerFile[TR::RealRegister::GPR8]);
   _registerFile[TR::RealRegister::GPR10]->setSiblingRegister(_registerFile[TR::RealRegister::GPR11]);
   _registerFile[TR::RealRegister::GPR11]->setSiblingRegister(_registerFile[TR::RealRegister::GPR10]);
   _registerFile[TR::RealRegister::GPR12]->setSiblingRegister(_registerFile[TR::RealRegister::GPR13]);
   _registerFile[TR::RealRegister::GPR13]->setSiblingRegister(_registerFile[TR::RealRegister::GPR12]);
   _registerFile[TR::RealRegister::GPR14]->setSiblingRegister(_registerFile[TR::RealRegister::GPR15]);
   _registerFile[TR::RealRegister::GPR15]->setSiblingRegister(_registerFile[TR::RealRegister::GPR14]);

   // Initialize FPRs

   _registerFile[TR::RealRegister::FPR0] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR0, TR::RealRegister::FPR0Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR1] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR1, TR::RealRegister::FPR1Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR2] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR2, TR::RealRegister::FPR2Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR3] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR3, TR::RealRegister::FPR3Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR4] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR4, TR::RealRegister::FPR4Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR5] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR5, TR::RealRegister::FPR5Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR6] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR6, TR::RealRegister::FPR6Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR7] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR7, TR::RealRegister::FPR7Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR8] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR8, TR::RealRegister::FPR8Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR9] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR9, TR::RealRegister::FPR9Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR10] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR10, TR::RealRegister::FPR10Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR11] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR11, TR::RealRegister::FPR11Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR12] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR12, TR::RealRegister::FPR12Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR13] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR13, TR::RealRegister::FPR13Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR14] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR14, TR::RealRegister::FPR14Mask, self()->cg());

   _registerFile[TR::RealRegister::FPR15] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR15, TR::RealRegister::FPR15Mask, self()->cg());

   // Initialize Vector Regs
   // first 16 overlaps with FPRs
   _registerFile[TR::RealRegister::VRF0]  = _registerFile[TR::RealRegister::FPR0];
   _registerFile[TR::RealRegister::VRF1]  = _registerFile[TR::RealRegister::FPR1];
   _registerFile[TR::RealRegister::VRF2]  = _registerFile[TR::RealRegister::FPR2];
   _registerFile[TR::RealRegister::VRF3]  = _registerFile[TR::RealRegister::FPR3];
   _registerFile[TR::RealRegister::VRF4]  = _registerFile[TR::RealRegister::FPR4];
   _registerFile[TR::RealRegister::VRF5]  = _registerFile[TR::RealRegister::FPR5];
   _registerFile[TR::RealRegister::VRF6]  = _registerFile[TR::RealRegister::FPR6];
   _registerFile[TR::RealRegister::VRF7]  = _registerFile[TR::RealRegister::FPR7];
   _registerFile[TR::RealRegister::VRF8]  = _registerFile[TR::RealRegister::FPR8];
   _registerFile[TR::RealRegister::VRF9]  = _registerFile[TR::RealRegister::FPR9];
   _registerFile[TR::RealRegister::VRF10] = _registerFile[TR::RealRegister::FPR10];
   _registerFile[TR::RealRegister::VRF11] = _registerFile[TR::RealRegister::FPR11];
   _registerFile[TR::RealRegister::VRF12] = _registerFile[TR::RealRegister::FPR12];
   _registerFile[TR::RealRegister::VRF13] = _registerFile[TR::RealRegister::FPR13];
   _registerFile[TR::RealRegister::VRF14] = _registerFile[TR::RealRegister::FPR14];
   _registerFile[TR::RealRegister::VRF15] = _registerFile[TR::RealRegister::FPR15];

   _registerFile[TR::RealRegister::VRF16] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF16, TR::RealRegister::VRF16Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF17] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF17, TR::RealRegister::VRF17Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF18] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF18, TR::RealRegister::VRF18Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF19] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF19, TR::RealRegister::VRF19Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF20] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF20, TR::RealRegister::VRF20Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF21] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF21, TR::RealRegister::VRF21Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF22] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF22, TR::RealRegister::VRF22Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF23] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF23, TR::RealRegister::VRF23Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF24] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF24, TR::RealRegister::VRF24Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF25] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF25, TR::RealRegister::VRF25Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF26] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF26, TR::RealRegister::VRF26Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF27] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF27, TR::RealRegister::VRF27Mask, self()->cg());

   _registerFile[TR::RealRegister::VRF28] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF28, TR::RealRegister::VRF28Mask, self()->cg());

  _registerFile[TR::RealRegister::VRF29] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF29, TR::RealRegister::VRF29Mask, self()->cg());

  _registerFile[TR::RealRegister::VRF30] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF30, TR::RealRegister::VRF30Mask, self()->cg());

  _registerFile[TR::RealRegister::VRF31] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF31, TR::RealRegister::VRF31Mask, self()->cg());

   // set siblings for Floating Point register pairs used by long doubles
   _registerFile[TR::RealRegister::FPR0]->setSiblingRegister(_registerFile[TR::RealRegister::FPR2]);
   _registerFile[TR::RealRegister::FPR1]->setSiblingRegister(_registerFile[TR::RealRegister::FPR3]);
   _registerFile[TR::RealRegister::FPR2]->setSiblingRegister(_registerFile[TR::RealRegister::FPR0]);
   _registerFile[TR::RealRegister::FPR3]->setSiblingRegister(_registerFile[TR::RealRegister::FPR1]);
   _registerFile[TR::RealRegister::FPR4]->setSiblingRegister(_registerFile[TR::RealRegister::FPR6]);
   _registerFile[TR::RealRegister::FPR5]->setSiblingRegister(_registerFile[TR::RealRegister::FPR7]);
   _registerFile[TR::RealRegister::FPR6]->setSiblingRegister(_registerFile[TR::RealRegister::FPR4]);
   _registerFile[TR::RealRegister::FPR7]->setSiblingRegister(_registerFile[TR::RealRegister::FPR5]);
   _registerFile[TR::RealRegister::FPR8]->setSiblingRegister(_registerFile[TR::RealRegister::FPR10]);
   _registerFile[TR::RealRegister::FPR9]->setSiblingRegister(_registerFile[TR::RealRegister::FPR11]);
   _registerFile[TR::RealRegister::FPR10]->setSiblingRegister(_registerFile[TR::RealRegister::FPR8]);
   _registerFile[TR::RealRegister::FPR11]->setSiblingRegister(_registerFile[TR::RealRegister::FPR9]);
   _registerFile[TR::RealRegister::FPR12]->setSiblingRegister(_registerFile[TR::RealRegister::FPR14]);
   _registerFile[TR::RealRegister::FPR13]->setSiblingRegister(_registerFile[TR::RealRegister::FPR15]);
   _registerFile[TR::RealRegister::FPR14]->setSiblingRegister(_registerFile[TR::RealRegister::FPR12]);
   _registerFile[TR::RealRegister::FPR15]->setSiblingRegister(_registerFile[TR::RealRegister::FPR13]);

   }

// GRA register table ///////////////////////////

int32_t OMR::Z::Machine::addGlobalReg(TR::RealRegister::RegNum reg, int32_t tableIndex)
   {
   if (reg == TR::RealRegister::NoReg)
      return tableIndex;
   if (self()->getRealRegister(reg)->getState() == TR::RealRegister::Locked)
      return tableIndex;
   for (int32_t i = 0; i < tableIndex; i++)
      if (_globalRegisterNumberToRealRegisterMap[i] == reg)
         return tableIndex; // Already in the table, so don't add it
   _globalRegisterNumberToRealRegisterMap[tableIndex++] = reg;
   return tableIndex;
   }

int32_t OMR::Z::Machine::getGlobalReg(TR::RealRegister::RegNum reg)
   {
   for (int32_t i = 0; i <= self()->getLastGlobalVRFRegisterNumber(); i++)
      {
      if (_globalRegisterNumberToRealRegisterMap[i] == reg)
         return i;
      }
   return -1;
   }

int32_t OMR::Z::Machine::addGlobalRegLater(TR::RealRegister::RegNum reg, int32_t tableIndex)
   {
   if (reg == TR::RealRegister::NoReg)
      return tableIndex;
   if (self()->getRealRegister(reg)->getState() == TR::RealRegister::Locked)
      return tableIndex;
   for (int32_t i = 0; i < tableIndex; i++)
      if (_globalRegisterNumberToRealRegisterMap[i] == reg)
         {
         // It's already in the table.  Delete it from where it is, and then we'll put it at the end.
         tableIndex--;
         for (; i < tableIndex; i++)
            _globalRegisterNumberToRealRegisterMap[i] = _globalRegisterNumberToRealRegisterMap[i+1];
         break;
         }
   _globalRegisterNumberToRealRegisterMap[tableIndex++] = reg;
   return tableIndex;
   }

void
OMR::Z::Machine::initializeFPRegPairTable()
   {
      _S390FirstOfFPRegisterPairs[0] = TR::RealRegister::FPR0;  // pair FPR0 & FPR2
      _S390FirstOfFPRegisterPairs[1] = TR::RealRegister::FPR4;  // pair FPR4 & FPR6
      _S390FirstOfFPRegisterPairs[2] = TR::RealRegister::FPR8;  // pair FPR8 & FPR10
      _S390FirstOfFPRegisterPairs[3] = TR::RealRegister::FPR12; // pair FPR12 & FPR14
      _S390FirstOfFPRegisterPairs[4] = TR::RealRegister::FPR1;  // pair FPR1 & FPR3
      _S390FirstOfFPRegisterPairs[5] = TR::RealRegister::FPR5;  // pair FPR5 & FPR7
      _S390FirstOfFPRegisterPairs[6] = TR::RealRegister::FPR9;  // pair FPR9 & FPR11
      _S390FirstOfFPRegisterPairs[7] = TR::RealRegister::FPR13;  // pair FPR13 & FPR15

      _S390SecondOfFPRegisterPairs[0] = TR::RealRegister::FPR2;  // pair FPR0 & FPR2
      _S390SecondOfFPRegisterPairs[1] = TR::RealRegister::FPR6;  // pair FPR4 & FPR6
      _S390SecondOfFPRegisterPairs[2] = TR::RealRegister::FPR10;  // pair FPR8 & FPR10
      _S390SecondOfFPRegisterPairs[3] = TR::RealRegister::FPR14;  // pair FPR12 & FPR14
      _S390SecondOfFPRegisterPairs[4] = TR::RealRegister::FPR3;  // pair FPR1 & FPR3
      _S390SecondOfFPRegisterPairs[5] = TR::RealRegister::FPR7;  // pair FPR5 & FPR7
      _S390SecondOfFPRegisterPairs[6] = TR::RealRegister::FPR11;  // pair FPR9 & FPR11
      _S390SecondOfFPRegisterPairs[7] = TR::RealRegister::FPR15;  // pair FPR13 & FPR15
   }


uint32_t *
OMR::Z::Machine::initializeGlobalRegisterTable()
   {
   TR::Compilation *comp = self()->cg()->comp();

   int32_t p = 0;

   TR::Linkage *linkage = self()->cg()->getS390Linkage();
   self()->setFirstGlobalGPRRegisterNumber(0);

   if (linkage->isZLinuxLinkageType())
      p = self()->addGlobalReg(TR::RealRegister::GPR1, p);

   // Linkage regs in reverse order
   //
   // Note: the existence of getLastLinkageGPR unfortunately means
   // linkage registers have to be in a contiguous chunk.  This never
   // mattered as long as they were all volatile, because we'd want to
   // put them together anyway, but with preserved linkage registers, it
   // makes sense to separate them.  However, we can't do so until we
   // eliminate getLastLinkageGPR etc.
   //
   // The best we can do is to add the volatile ones first, then the
   // preserved ones.
   //

   if (linkage->isZLinuxLinkageType()) // ordering seems to make crashes on zos.
      {
      for (int32_t i = linkage->getNumIntegerArgumentRegisters(); i >= 0; i--)
         {
         if (!linkage->getPreserved(linkage->getIntegerArgumentRegister(i)))
            p = self()->addGlobalReg(linkage->getIntegerArgumentRegister(i), p);
         }
      }


   for (int32_t i = linkage->getNumIntegerArgumentRegisters() - 1; i >= 0; i--)
      p = self()->addGlobalReg(linkage->getIntegerArgumentRegister(i), p);

   self()->setLastLinkageGPR(p-1);

   if ( (self()->cg()->isLiteralPoolOnDemandOn() && !linkage->isZLinuxLinkageType()) || (self()->cg()->isLiteralPoolOnDemandOn() && !linkage->getPreserved(linkage->getLitPoolRegister())) )
      p = self()->addGlobalReg(linkage->getLitPoolRegister(), p);
   if (!self()->cg()->isGlobalStaticBaseRegisterOn())
      p = self()->addGlobalReg(linkage->getStaticBaseRegister(), p);
   if (!self()->cg()->isGlobalPrivateStaticBaseRegisterOn())
      p = self()->addGlobalReg(linkage->getPrivateStaticBaseRegister(), p);
   p = self()->addGlobalReg(linkage->getIntegerReturnRegister(), p);
   p = self()->addGlobalReg(linkage->getLongReturnRegister(), p);
   p = self()->addGlobalReg(linkage->getLongLowReturnRegister(), p);
   p = self()->addGlobalReg(linkage->getLongHighReturnRegister(), p);

   // Preserved regs in descending order to encourage stmg with gpr15 and
   // gpr14, which are commonly preserved in zLinux system linkage
   //
   if (linkage->isZLinuxLinkageType()) // ordering seems to make crashes on zos.
      {
      for (int32_t i = TR::RealRegister::LastAssignableGPR; i >= TR::RealRegister::FirstGPR; --i)
         {
         auto regNum = static_cast<TR::RealRegister::RegNum>(i);

         if (linkage->getPreserved(regNum))
            {
            if (regNum != linkage->getStaticBaseRegister() &&
                regNum != linkage->getPrivateStaticBaseRegister() &&
                regNum != linkage->getStackPointerRegister())
               {
               p = self()->addGlobalReg(regNum, p);
               }
            }
         }
      }
   else
      {
      // Preserved regs, with special heavily-used regs last
      //
      for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
         {
         auto regNum = static_cast<TR::RealRegister::RegNum>(i);

         if (linkage->getPreserved(regNum))
            {
            if (regNum != linkage->getLitPoolRegister() &&
                regNum != linkage->getStaticBaseRegister() &&
                regNum != linkage->getPrivateStaticBaseRegister() &&
                regNum != linkage->getStackPointerRegister())
               {
               p = self()->addGlobalReg(regNum, p);
               }
            }
         }
      }

   p = self()->addGlobalRegLater(linkage->getMethodMetaDataRegister(), p);
   if (self()->cg()->comp()->target().isZOS())
      {
      p = self()->addGlobalRegLater(self()->cg()->getS390Linkage()->getStackPointerRegister(), p);
      }

   // Special regs that add to prologue cost
   //
   p = self()->addGlobalRegLater(linkage->getENVPointerRegister(), p);

   //p = addGlobalRegLater(linkage->getLitPoolRegister(), p); // zOS private linkage might want this here?

   if (linkage->isXPLinkLinkageType())
      p = self()->addGlobalRegLater(TR::RealRegister::GPR7, p);

   self()->setLastGlobalGPRRegisterNumber(p-1);

   // Volatiles that aren't linkage regs
   //
   self()->setFirstGlobalFPRRegisterNumber(p);
   for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
      {
      auto regNum = static_cast<TR::RealRegister::RegNum>(i);

      if (!linkage->getPreserved(regNum) && !linkage->getFloatArgument(regNum))
         {
         p = self()->addGlobalReg(regNum, p);
         }
      }

   // Linkage regs in reverse order
   //
   for (int32_t i = linkage->getNumFloatArgumentRegisters(); i >= 0; i--)
      {
      p = self()->addGlobalReg(linkage->getFloatArgumentRegister(i), p);
      }

   self()->setLastLinkageFPR(p-1);

   // Preserved regs, vmthread last
   //
   for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
      {
      auto regNum = static_cast<TR::RealRegister::RegNum>(i);

      if (linkage->getPreserved(regNum))
         {
         p = self()->addGlobalReg(regNum, p);
         }
      }

   self()->setLastGlobalFPRRegisterNumber(p-1);

   // initGlobalVectorRegisterMap sets first/last global grns and overlapped grns
   if (self()->cg()->getSupportsVectorRegisters())
      p = self()->initGlobalVectorRegisterMap(p);

   self()->setLastGlobalVRFRegisterNumber(p-1);

   for (int32_t i = 0; i < p; i++)
      {
      if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getCAAPointerRegister())
         self()->setGlobalCAARegisterNumber(i);
      if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getENVPointerRegister())
         self()->setGlobalEnvironmentRegisterNumber(i);
      if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getParentDSAPointerRegister())
         self()->setGlobalParentDSARegisterNumber(i);
      if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getEntryPointRegister())
         self()->setGlobalEntryPointRegisterNumber(i);
      if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getReturnAddressRegister())
         self()->setGlobalReturnAddressRegisterNumber(i);
      }

   self()->setLastGlobalCCRRegisterNumber(p-1);

   return _globalRegisterNumberToRealRegisterMap;
   }

/**
 * Initialize global vectors in a common location since all configurations (zOS/zLinux 31/64) have the same linkage conventions
 * Each register is explicitly set to stay consistent with a majority of the global register number setup procedures across z-supported
 * platforms and configurations
 *
 * It is a no-op for platforms/products that do not support vector registers
 */
uint32_t
OMR::Z::Machine::initGlobalVectorRegisterMap(uint32_t vectorOffset)
   {
   if (!self()->cg()->getSupportsVectorRegisters() && !self()->cg()->comp()->getOption(TR_DisableVectorRegGRA))
      {
      self()->setFirstGlobalVRFRegisterNumber(-1);
      self()->setLastGlobalVRFRegisterNumber(-1);
      self()->setFirstOverlappedGlobalVRFRegisterNumber(-1);
      self()->setLastOverlappedGlobalVRFRegisterNumber(-1);
      return vectorOffset;
      }

   // Since Vector/FPR overlapping GRA is not yet complete, enable entire vector reg map use only if auto-SIMD
   // or SIMD library are enabled. Eventually, this will be the default case.
   //
   // This flag prevents the low reg file (VRF0-15 from being part of GRA). This ensures no overlap.

   static const char * hideLowerHalf = feGetEnv("TR_hideOverlappingVecRegsFromGRA");
   const bool useEntireRegFile = ((hideLowerHalf == NULL) && self()->cg()->getSupportsVectorRegisters());

   TR_GlobalRegisterNumber firstOverlappingVecOffset = -1;
   TR_GlobalRegisterNumber lastOverlappingVecOffset  = -1;

   // FPR/VRF Overlap section. We simply copy the FPR ordering here.
   // For other machines with peculiar overlapping btwn FPR/VRF, highly recommend overriding initGlobalVectorRegisterMap
   //         .--------.
   // [GPR | FPR | (FPR/VRF) | VRF | ... ]          _globalRegisterNumberToRealRegisterMap
   //
   // For ILGRA the plan is to usef fp0-fp15,v16-v31
   // This means just reuse the FP registers as they are.
   // Colouring will make sure to map global virtual FPRs only to the first 16 and vector global virtuals can map to all 32 registers.

   if (useEntireRegFile)
      {
      TR_GlobalRegisterNumber fof = self()->getFirstGlobalFPRRegisterNumber();
      TR_GlobalRegisterNumber lof = self()->getLastGlobalFPRRegisterNumber();
      TR_GlobalRegisterNumber dictIdx = fof;
      firstOverlappingVecOffset = static_cast<TR_GlobalRegisterNumber>(vectorOffset);
      int32_t numOverlappingVRF = lof - fof + 1;
      // match FPR global reg numbering
      for (int i=0; i<numOverlappingVRF; i++)
         {
         _globalRegisterNumberToRealRegisterMap[vectorOffset++] = _globalRegisterNumberToRealRegisterMap[dictIdx++];
         }
      lastOverlappingVecOffset = static_cast<TR_GlobalRegisterNumber>(vectorOffset - 1);
      }

   // Add non-overlapping VRFs
#define addToGRAMap(regNum) _globalRegisterNumberToRealRegisterMap[vectorOffset++] = TR::RealRegister::regNum
   // (x-preserved, now volatile) locals.
   addToGRAMap(VRF16);
   addToGRAMap(VRF17);
   addToGRAMap(VRF18);
   addToGRAMap(VRF19);
   addToGRAMap(VRF20);
   addToGRAMap(VRF21);
   addToGRAMap(VRF22);
   addToGRAMap(VRF23);

   // (volatiles) param passing
   addToGRAMap(VRF29);
   addToGRAMap(VRF31);
   addToGRAMap(VRF29);
   addToGRAMap(VRF27);
   addToGRAMap(VRF25);
   addToGRAMap(VRF30);
   addToGRAMap(VRF28);
   addToGRAMap(VRF26);

   // (volatiles) param passing + rv
   addToGRAMap(VRF24);

   static const char * traceVectorGRN = feGetEnv("TR_traceVectorGRA");

   // We need to ensure fpr's grn's are part of vrf grn's since available reg bit vector in GRA depends on it.
   // This greatly simplifies GRA.
   // By setting first VRF reg num to first FPR, we'll essentially see 16 + 32 grn's for vectors BUT gra takes care
   // to ensure we do things right i.e if any of the 16 fpr grn's are assigned, remove the corresponding reg in the 32.
   self()->setFirstGlobalVRFRegisterNumber(self()->getFirstGlobalFPRRegisterNumber());   // equiv to FPR0
   self()->setLastGlobalVRFRegisterNumber(vectorOffset - 1);                     // equiv to VRF31

   self()->setFirstOverlappedGlobalVRFRegisterNumber(firstOverlappingVecOffset); // equiv to VRF0
   self()->setLastOverlappedGlobalVRFRegisterNumber(lastOverlappingVecOffset);   // equiv to VRF16

   // Similar FPR and VRF will have same 'lastOverlapping' grn.
   self()->setLastGlobalFPRRegisterNumber(self()->getLastOverlappedGlobalVRFRegisterNumber());

   if (traceVectorGRN)
      {
      printf("Java func: %s func: %s\n", self()->cg()->comp()->getCurrentMethod()->nameChars(), __FUNCTION__);
      printf("ff %d\t", self()->getFirstGlobalFPRRegisterNumber());
      printf("lf %d\t", self()->getLastGlobalFPRRegisterNumber());
      printf("fof %d\t", self()->getFirstOverlappedGlobalFPRRegisterNumber());
      printf("lof %d\t", self()->getLastOverlappedGlobalFPRRegisterNumber());
      printf("fv  %d\t", self()->getFirstGlobalVRFRegisterNumber());
      printf("lv  %d\t", self()->getLastGlobalVRFRegisterNumber());
      printf("fov %d\t", self()->getFirstOverlappedGlobalVRFRegisterNumber());
      printf("lov %d\t", self()->getLastOverlappedGlobalVRFRegisterNumber());
      printf("\n--------------------\n");
      }

   return vectorOffset;
#undef addToGRAMap
   }

// call this if optimizer run TR_DynamicLiteralPool pass
void
OMR::Z::Machine::releaseLiteralPoolRegister()
   {
   TR::Compilation *comp = self()->cg()->comp();
   if (comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      _globalRegisterNumberToRealRegisterMap[GLOBAL_REG_FOR_LITPOOL] = TR::RealRegister::GPR6;
      }
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   return _lastGlobalGPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   return _firstGlobalGPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstOverlappedGlobalFPRRegisterNumber(reg);
   return _firstGlobalFPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastOverlappedGlobalFPRRegisterNumber(reg);
   return _lastGlobalFPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   return _firstGlobalVRFRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   return _lastGlobalVRFRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalCCRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   return _lastGlobalCCRRegisterNumber=reg;
   }

// Register Association ////////////////////////////////////////////
void
OMR::Z::Machine::setRegisterWeightsFromAssociations()
   {
   TR::Linkage * linkage = self()->cg()->getS390Linkage();
   int32_t first = TR::RealRegister::FirstGPR;
   TR::Compilation *comp = self()->cg()->comp();
   int32_t last = TR::RealRegister::LastAssignableVRF;

   for (int32_t i = first; i <= last; ++i)
      {
      TR::Register * assocReg = _registerAssociations[i];
      if (linkage->getPreserved((TR::RealRegister::RegNum) i) && _registerFile[i]->getHasBeenAssignedInMethod() == false)
         {
         if (assocReg)
            {
            assocReg->setAssociation(i);
            }
         _registerFile[i]->setWeight(S390_REGISTER_INITIAL_PRESERVED_WEIGHT);
         }
      else if (assocReg == NULL)
         {
         if (i >= TR::RealRegister::FirstOverlapVRF && i <= TR::RealRegister::LastOverlapVRF)
            _registerFile[i]->setWeight(S390_REGISTER_LOW_PRIORITY_WEIGHT);
         else
            _registerFile[i]->setWeight(S390_REGISTER_BASIC_WEIGHT);
         }
      else
         {
         assocReg->setAssociation(i);
         if (assocReg->isPlaceholderReg())
            {
            // placeholder register and is only needed at the specific dependency
            // site (usually a killed register on a call)
            // so defer this register's weight to that of registers
            // where the associated register has a longer life
            _registerFile[i]->setWeight(S390_REGISTER_PLACEHOLDER_WEIGHT);
            }
         else
            {
            _registerFile[i]->setWeight(S390_REGISTER_ASSOCIATED_WEIGHT);
            }
         }
      }
   }

void
OMR::Z::Machine::createRegisterAssociationDirective(TR::Instruction * cursor)
   {
   TR::Compilation *comp = self()->cg()->comp();
   int32_t last = TR::RealRegister::LastAssignableVRF;
   TR::RegisterDependencyConditions * associations  = new (self()->cg()->trHeapMemory(), TR_MemoryBase::RegisterDependencyConditions) TR::RegisterDependencyConditions(0, last, self()->cg());

   // Go through the current associations held in the machine and put a copy of
   // that state out into the stream after the cursor
   // so that when the register assigner goes backwards through this point
   // it updates the machine and register association states properly
   //
   for (int32_t i = 0; i < last; i++)
      {
      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum) (i + 1);
      associations->addPostCondition(self()->getVirtualAssociatedWithReal(regNum), regNum);
      }

   TR::Instruction *cursor1 = new (self()->cg()->trHeapMemory(), TR_MemoryBase::S390Instruction) TR::Instruction(cursor, TR::InstOpCode::assocreg, associations, self()->cg());

   if (cursor == self()->cg()->getAppendInstruction())
      {
      self()->cg()->setAppendInstruction(cursor->getNext());
      }
   }

TR::Register *
OMR::Z::Machine::setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register * virtReg)
   {
   if ((regNum == TR::RealRegister::EvenOddPair) ||
      (regNum == TR::RealRegister::LegalEvenOfPair) ||
      (regNum == TR::RealRegister::LegalOddOfPair))
      {
      virtReg->setAssociation(regNum);
      return NULL;
      }
   else if (regNum >= TR::RealRegister::NumRegisters || _registerFile[regNum]->getRealRegisterMask() == 0)
      {
      virtReg->setAssociation(TR::RealRegister::NoReg);
      return NULL;
      }

   return _registerAssociations[regNum] = virtReg;
   }

void
OMR::Z::Machine::takeRegisterStateSnapShot()
   {
   for (int32_t i=TR::RealRegister::FirstGPR;i<TR::RealRegister::NumRegisters;i++)
      {
      _registerFlagsSnapShot[i] = _registerFile[i]->getFlags();
      _registerStatesSnapShot[i] = _registerFile[i]->getState();
      _registerAssociationsSnapShot[i] = self()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)(i));
      _assignedRegisterSnapShot[i] = _registerFile[i]->getAssignedRegister();
      _registerAssignedSnapShot[i] = _registerFile[i]->getHasBeenAssignedInMethod();
      _registerWeightSnapShot[i] = _registerFile[i]->getWeight();
      }
   }

void
OMR::Z::Machine::restoreRegisterStateFromSnapShot()
   {
   for (int32_t i=TR::RealRegister::FirstGPR;i<TR::RealRegister::NumRegisters;i++)
      {
      _registerFile[i]->setWeight(_registerWeightSnapShot[i]);
      _registerFile[i]->setFlags(_registerFlagsSnapShot[i]);
      _registerFile[i]->setState(_registerStatesSnapShot[i]);
      if (_registerAssociationsSnapShot[i])
         self()->setVirtualAssociatedWithReal((TR::RealRegister::RegNum)(i), _registerAssociationsSnapShot[i]);
      if (_registerFile[i]->getState() == TR::RealRegister::Free)
         {
         if (_registerFile[i]->getAssignedRegister() != NULL)
            {
            if (_registerFile[i]->getAssignedRegister()->getAssignedRegister() == _registerFile[i])
               {
               // clear the Virt -> Real reg assignment if we restored the Real reg state to FREE
               _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
               }
            }
         }
      else if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         if (_registerFile[i]->getAssignedRegister() != NULL &&
             _registerFile[i]->getAssignedRegister() != _assignedRegisterSnapShot[i])
            {
            // If the virtual register associated with the _registerFile[i] is not assigned to the current real register
            // it must have been updated by prior _registerFile.... Do NOT Clear.
            //   Ex:
            //     RegFile starts as:
            //       _registerFile[12] -> GPR_3555
            //       _registerFile[15] -> GPR_3545
            //     SnapShot:
            //       _registerFile[12] -> GPR_3545
            //       _registerFile[15] -> GPR_3562
            //  When we handled _registerFile[12], we would have updated the assignment of GPR_3545 (currently to GPR15) to GPR12.
            //  When we subsequently handle _registerFile[15], we cannot blindly reset GPR_3545's assigned register to NULL,
            //  as that will incorrectly break the assignment to GPR12.
            if (_registerFile[i]->getAssignedRegister()->getAssignedRegister() == _registerFile[i])
               {
               // clear the Virt -> Real reg assignment for any newly assigned virtual register (due to spills) in the hot path
               _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
               }
            }
         }
      _registerFile[i]->setAssignedRegister(_assignedRegisterSnapShot[i]);
      //_registerFile[i]->setHasBeenAssignedInMethod(_registerAssignedSnapShot[i]);
      // make sure to double link virt - real reg if assigned
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         //self()->cg()->traceRegisterAssignment("\nOOL: restoring %R : %R", _registerFile[i], _registerFile[i]->getAssignedRegister());
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(_registerFile[i]);
         }

      // If a register is only created and used in multiple OOL hot paths (created in one OOL hot path
      // the dead in a later OOL hot path) and has never been used in any OOL cold path or main line, then we need
      // to make sure to not include this register in the snapshot once the register is dead, otherwise
      // we will end up having a virtual register with futureUseCount=0 assigned to a real reg dangling in the
      // register state table. (Bug exposed by PPC Tarok)
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned &&
          _registerFile[i]->getAssignedRegister()->getFutureUseCount() == 0)
         {
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(NULL);
         _registerFile[i]->setAssignedRegister(NULL);
         _registerFile[i]->setState(TR::RealRegister::Free);
         }
      }
   }

TR::RegisterDependencyConditions * OMR::Z::Machine::createCondForLiveAndSpilledGPRs(TR::list<TR::Register*> *spilledRegisterList)
   {
   int32_t i, c=0;
   // Calculate number of register dependencies required. This step is not really necessary, but
   // it is space conscious
   //
   TR::Compilation *comp = self()->cg()->comp();
   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastVRF; i = ((i == TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstVRF : i+1) )
      {
      TR::RealRegister *realReg = self()->getRealRegister(i);

      TR_ASSERT(realReg->getState() == TR::RealRegister::Assigned ||
              realReg->getState() == TR::RealRegister::Free ||
              realReg->getState() == TR::RealRegister::Locked,
              "cannot handle realReg state %d, (block state is %d)\n",realReg->getState(),TR::RealRegister::Blocked);

      if (realReg->getState() == TR::RealRegister::Assigned)
         c++;
      }

   c += spilledRegisterList ? spilledRegisterList->size() : 0;

   TR::RegisterDependencyConditions *deps = NULL;

   if (c)
      {
      deps = new (self()->cg()->trHeapMemory(), TR_MemoryBase::RegisterDependencyConditions) TR::RegisterDependencyConditions(0, c, self()->cg());
      for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastVRF; i = ((i==TR::RealRegister::LastAssignableGPR)? TR::RealRegister::FirstVRF : i+1))
         {
         TR::RealRegister *realReg = self()->getRealRegister(i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register *virtReg = realReg->getAssignedRegister();
            deps->addPostCondition(virtReg, realReg->getRegisterNumber());
            virtReg->incFutureUseCount();
            }
         }
      }

    if (spilledRegisterList)
       {
       for (auto li = spilledRegisterList->begin(); li != spilledRegisterList->end(); ++li)
          {
    	  TR::Register* virtReg = *li;
          deps->addPostCondition(virtReg, TR::RealRegister::SpilledReg);
          }
       }


   return deps;
   }
