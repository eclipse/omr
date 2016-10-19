/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRZMachine#C")
#pragma csect(STATIC,"OMRZMachine#S")
#pragma csect(TEST,"OMRZMachine#T")


#include <stdint.h>                                // for uint32_t, int32_t, etc
#include <stdio.h>                                 // for NULL, printf, etc
#include <string.h>                                // for memset
#include "codegen/BackingStore.hpp"                // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator
#include "codegen/FrontEnd.hpp"                    // for feGetEnv, etc
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction
#include "codegen/Linkage.hpp"                     // for Linkage
#include "codegen/Machine.hpp"                     // for MachineBase, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "compile/Compilation.hpp"                 // for Compilation, comp
#include "compile/ResolvedMethod.hpp"              // for TR_ResolvedMethod
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                     // for ObjectModel
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                          // for uintptrj_t
#include "il/Block.hpp"                            // for Block
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/Flags.hpp"                         // for flags32_t
#include "infra/List.hpp"                          // for List, etc
#include "infra/Random.hpp"
#include "infra/Stack.hpp"                         // for TR_Stack
#include "codegen/TRSystemLinkage.hpp"
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"           // for etc


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
   return _cg->getDebug();
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
OMR::Z::Machine::registerCopy(TR::Instruction *precedingInstruction,
             TR_RegisterKinds rk,
             TR::Register *targetReg,
             TR::Register *sourceReg,
             TR::CodeGenerator    *cg,
             flags32_t            instFlags)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node * currentNode = precedingInstruction->getNode();
   TR::Instruction * currentInstruction = NULL;
   TR::RealRegister      *targetRealReg = toRealRegister(targetReg->getRealRegister());
   TR::RealRegister      *sourceRealReg = toRealRegister(sourceReg->getRealRegister());
   char * REG_MOVE = "LR=Reg_move";
   char * REG_PAIR = "LR=Reg_pair";
   char * VREG_MOVE = "VLR=VReg_move";

   TR_Debug * debugObj = cg->getDebug();

   bool enableHighWordRA = cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;

   switch (rk)
      {
      case TR_GPR64:
         currentInstruction =
            generateRRInstruction(cg, TR::InstOpCode::LGR, currentNode, targetReg, sourceReg, precedingInstruction);

         if(sourceRealReg) sourceRealReg->setAssignedHigh(true);
         if(targetRealReg) targetRealReg->setAssignedHigh(true);

         cg->traceRAInstruction(currentInstruction);
#ifdef DEBUG
         if (debug("traceMsg90GPR") || debug("traceGPRStats"))
            {
            cg->incTotalRegisterMoves();
            }
#endif
         break;
      case TR_GPR:
         currentInstruction =
            generateRRInstruction(cg, (comp->getOption(TR_ForceLargeRAMoves)) ? TR::InstOpCode::LGR : TR::InstOpCode::getLoadRegOpCode(), currentNode, targetReg, sourceReg, precedingInstruction);
         if (enableHighWordRA && targetRealReg && sourceRealReg)
            TR_ASSERT( !targetRealReg->isHighWordRegister() && !sourceRealReg->isHighWordRegister(), "\nREG COPY: LR with HPR?\n");
         cg->traceRAInstruction(currentInstruction);
#ifdef DEBUG
         if (debug("traceMsg90GPR") || debug("traceGPRStats"))
            {
            cg->incTotalRegisterMoves();
            }
#endif
         break;
      case TR_HPR:
         //TR_ASSERTC( "Highword RA is disabled,comp, enableHighWordRA, but REG COPY is working on HPR??");
         //TR_ASSERTC(comp, sourceReg->isHighWordRegister() && targetReg->isHighWordRegister(),
         //"REG COPY HPR: both target and source Regs have to be HPR!");
         if (sourceRealReg && sourceRealReg->isLowWordRegister())
            currentInstruction = generateExtendedHighWordInstruction(currentNode, cg, TR::InstOpCode::LHLR, targetReg, sourceReg, 0, precedingInstruction);
         else
            currentInstruction = generateExtendedHighWordInstruction(currentNode, cg, TR::InstOpCode::LHHR, targetReg, sourceReg, 0, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
#ifdef DEBUG
         if (debug("traceMsg90GPR") || debug("traceGPRStats"))
            {
            cg->incTotalRegisterMoves();
            }
#endif
         break;
      case TR_GPRL:
         //TR_ASSERTC( "Highword RA is disabled,comp, enableHighWordRA, but REG COPY is working on GPR Low word??");
         //TR_ASSERTC(comp, sourceReg->isLowWordRegister() && targetReg->isLowWordRegister(),
         //"REG COPY HPR: both target and source Regs have to be Low word GPR!");
         if (sourceRealReg && sourceRealReg->isLowWordRegister())
            currentInstruction = generateRRInstruction(cg, TR::InstOpCode::LR, currentNode, targetReg, sourceReg, precedingInstruction);
         else
            currentInstruction = generateExtendedHighWordInstruction(currentNode, cg, TR::InstOpCode::LLHFR, targetReg, sourceReg, 0, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
#ifdef DEBUG
         if (debug("traceMsg90GPR") || debug("traceGPRStats"))
            {
            cg->incTotalRegisterMoves();
            }
#endif
         break;
      case TR_FPR:
         currentInstruction = generateRRInstruction(cg, TR::InstOpCode::LDR, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         break;
      case TR_AR:
         currentInstruction = generateRRInstruction(cg, TR::InstOpCode::CPYA, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         break;
      case TR_VRF:
         currentInstruction = generateVRRaInstruction(cg, TR::InstOpCode::VLR, currentNode, targetReg, sourceReg, precedingInstruction);
         cg->traceRAInstruction(currentInstruction);
         break;
      }

   if (debugObj)
      {
      if (rk == TR_VRF)
         debugObj->addInstructionComment(toS390VRRaInstruction(currentInstruction), VREG_MOVE);
      else
         debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_MOVE);
      }

   if (debugObj && instFlags.testAny(PAIRREG) && rk != TR_VRF)
      {
      debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_PAIR);
      }

   return currentInstruction;
   }


/**
 * Exchange the contents of two registers
 */
TR::Instruction *
OMR::Z::Machine::registerExchange(TR::Instruction      *precedingInstruction,
                 TR_RegisterKinds     rk,
                 TR::RealRegister *targetReg,
                 TR::RealRegister *sourceReg,
                 TR::RealRegister *middleReg,
                 TR::CodeGenerator    *cg,
                 flags32_t            instFlags)
   {
   // middleReg is not used if rk==TR_GPR.
   TR::Compilation *comp = cg->comp();
   TR::Node * currentNode = precedingInstruction->getNode();
   TR::Instruction * currentInstruction = NULL;
   char * REG_EXCHANGE = "LR=Reg_exchg";
   char * REG_PAIR     = "LR=Reg_pair";
   TR_Debug * debugObj = cg->getDebug();
   bool enableHighWordRA = cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;
   TR::Machine *machine = cg->machine();

   // exchange floating point registers
   if (rk == TR_FPR)
      {
      if (middleReg != NULL)
         {
         middleReg->setHasBeenAssignedInMethod(true);

         currentInstruction = machine->registerCopy(precedingInstruction, rk, sourceReg, middleReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, targetReg, sourceReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, middleReg, targetReg, cg, instFlags);
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

         currentInstruction = machine->registerCopy(precedingInstruction, rk, sourceReg, middleReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, targetReg, sourceReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, middleReg, targetReg, cg, instFlags);
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
   // exchange access registers
   else if (rk == TR_AR)
      {
      if (middleReg != NULL)
         {
         middleReg->setHasBeenAssignedInMethod(true);

         currentInstruction = machine->registerCopy(precedingInstruction, rk, sourceReg, middleReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, targetReg, sourceReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = machine->registerCopy(precedingInstruction, rk, middleReg, targetReg, cg, instFlags);
         cg->traceRAInstruction(currentInstruction);
         }
      else
         {
         TR::Instruction * currentInstruction = precedingInstruction;
         TR_BackingStore * location;
         location = cg->allocateSpill(4, false, NULL);
         TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), cg);
         location->getSymbolReference()->getSymbol()->setSpillTempLoaded();

         currentInstruction = generateRSInstruction(cg, TR::InstOpCode::STAM, currentNode, targetReg, targetReg, tempMR, currentInstruction);
         cg->traceRAInstruction(currentInstruction);
         currentInstruction = generateRRInstruction(cg, TR::InstOpCode::CPYA, currentNode, targetReg, sourceReg, currentInstruction);
         cg->traceRAInstruction(currentInstruction);
         TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, 0, cg);
         currentInstruction = generateRSInstruction(cg, TR::InstOpCode::LAM, currentNode, sourceReg, sourceReg, tempMR2, currentInstruction);
         cg->traceRAInstruction(currentInstruction);

         cg->freeSpill(location, 4, 0);
         }

      cg->generateDebugCounter("RegisterAllocator/Exchange/AR", 1, TR::DebugCounter::Free);
      }
   else
      {
      TR::InstOpCode::Mnemonic opLoadReg = TR::InstOpCode::getLoadRegOpCode();
      TR::InstOpCode::Mnemonic opLoad    = TR::InstOpCode::getLoadOpCode();
      TR::InstOpCode::Mnemonic opStore   = TR::InstOpCode::getStoreOpCode();

      if (comp->getOption(TR_ForceLargeRAMoves))
         rk = TR_GPR64;

      bool srcRegIsHPR = sourceReg->isHighWordRegister();
      bool tgtRegIsHPR = targetReg->isHighWordRegister();

      if (enableHighWordRA)
         {
         if (srcRegIsHPR)
            {
            opLoad  = TR::InstOpCode::LFH;
            }
         if (tgtRegIsHPR)
            {
            opStore = TR::InstOpCode::STFH;
            }

         if (srcRegIsHPR != tgtRegIsHPR)
            {
            opLoadReg = tgtRegIsHPR? TR::InstOpCode::LHLR:InstOpCode::LLHFR;
            }
         else
            {
            opLoadReg = tgtRegIsHPR? TR::InstOpCode::LHHR:InstOpCode::LR;
            }
         }

      if (rk == TR_GPR64)
         {
         opLoadReg = TR::InstOpCode::LGR;
         opLoad    = TR::InstOpCode::LG;
         opStore   = TR::InstOpCode::STG;
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

         if (rk == TR_GPR64)
            {
            location = cg->allocateSpill(8, false, NULL);      // No chance of a gcpoint
            }
         else
            {
            location = cg->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), false, NULL); // No chance of a gcpoint
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

         if (rk == TR_GPR64)
            {
            middleReg->setAssignedHigh(true);
            }

         bool middleRegIsHPR = middleReg->isHighWordRegister();

         if (enableHighWordRA && rk != TR_GPR64)
            {
            if (srcRegIsHPR != middleRegIsHPR)
               {
               currentInstruction =
                  generateRRInstruction(cg, srcRegIsHPR? TR::InstOpCode::LHLR:InstOpCode::LLHFR, currentNode, sourceReg, middleReg, precedingInstruction);
               }
            else
               {
               currentInstruction =
                  generateRRInstruction(cg, srcRegIsHPR? TR::InstOpCode::LHHR:InstOpCode::LR, currentNode, sourceReg, middleReg, precedingInstruction);
               }
            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }

            if (srcRegIsHPR != tgtRegIsHPR)
               {
               currentInstruction =
                  generateRRInstruction(cg, tgtRegIsHPR? TR::InstOpCode::LHLR:InstOpCode::LLHFR, currentNode, targetReg, sourceReg, precedingInstruction);
               }
            else
               {
               currentInstruction =
                  generateRRInstruction(cg, tgtRegIsHPR? TR::InstOpCode::LHHR:InstOpCode::LR, currentNode, targetReg, sourceReg, precedingInstruction);
               }
            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }

            if (middleRegIsHPR != tgtRegIsHPR)
               {
               currentInstruction =
                  generateRRInstruction(cg, middleRegIsHPR? TR::InstOpCode::LHLR:InstOpCode::LLHFR, currentNode, middleReg, targetReg, precedingInstruction);
               }
            else
               {
               currentInstruction =
                  generateRRInstruction(cg, middleRegIsHPR? TR::InstOpCode::LHHR:InstOpCode::LR, currentNode, middleReg, targetReg, precedingInstruction);
               }

            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }
            }
         else
            {
            currentInstruction =
               generateRRInstruction(cg, opLoadReg, currentNode, sourceReg, middleReg, precedingInstruction);
            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }
            if (debugObj && instFlags.testAny(PAIRREG))
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_PAIR);
               }

            currentInstruction =
               generateRRInstruction(cg, opLoadReg, currentNode, targetReg, sourceReg, precedingInstruction);
            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }
            if (debugObj && instFlags.testAny(PAIRREG))
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_PAIR);
               }

            currentInstruction =
               generateRRInstruction(cg, opLoadReg, currentNode, middleReg, targetReg, precedingInstruction);
            cg->traceRAInstruction(currentInstruction);
            if (debugObj)
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_EXCHANGE);
               }
            if (debugObj && instFlags.testAny(PAIRREG))
               {
               debugObj->addInstructionComment(toS390RRInstruction(currentInstruction), REG_PAIR);
               }
            }
         }
#ifdef DEBUG
      if (debug("traceMsg90GPR") || debug("traceGPRStats"))
         {
         cg->incTotalRegisterXfers();
         }
#endif

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

/**
 * Check if a given virtual register lives across any OOL
 * @return true if this register interference with an OOL
 */
static bool
checkOOLInterference(TR::Instruction * currentInstruction, TR::Register * virtReg)
   {
   TR::Instruction * cursor = currentInstruction;

   if (cursor->cg()->isOutOfLineColdPath() || cursor->cg()->isOutOfLineHotPath())
      return true;

   // walk the live range of virtReg
   while (cursor != virtReg->getStartOfRange())
      {
      // look for OOL return label
      if (cursor->isLabel() &&
          toS390LabelInstruction(cursor)->getLabelSymbol()->isEndOfColdInstructionStream())
         {
         cursor->cg()->traceRegisterAssignment("Found %R used in OOL [0x%p]: will not spill to HPR", virtReg, cursor);
         return true;
         }

      // This shouldn't be necessary, however if for any reason startOfRange
      // is not setup properly, we do not want to end up in an infinite loop
      if (cursor->getNode() != NULL &&
          cursor->getNode()->getOpCodeValue() == TR::BBStart &&
          !cursor->getNode()->getBlock()->isExtensionOfPreviousBlock())
         {
         // we reached a new BB, virtRegs do not live across BB
         return false;
         }

      cursor = cursor->getPrev();
      }

   return false;
   }

static bool
boundNext(TR::Instruction * currentInstruction, int32_t realNum, TR::Register * virtReg)
   {
   TR::Instruction * cursor = currentInstruction;
   TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum) realNum;
   TR::Node * nodeBBStart = NULL;

   while (cursor->getOpCodeValue() != TR::InstOpCode::PROC)
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

/*
///////////////////////////////////////////////////////////////////////////////
// updateInterference - For Highword RA, not all instructions interfere with
//                      both words of a register
///////////////////////////////////////////////////////////////////////////////
static uint32_t
updateInterference (TR::Instruction * currentInstruction, TR::Register * virtualReg,
                    uint32_t interference, bool updateHW)
   {
   TR::Instruction * cursor = currentInstruction;
   TR::Node * nodeBBStart = NULL;
   uint32_t checkRegList = 0;
   int32_t maskI = TR::RealRegister::FirstGPR;
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last = TR::RealRegister::LastAssignableGPR;

   while (cursor->getOpCodeValue() != TR::InstOpCode::PROC)
      {
      TR::RegisterDependencyConditions * conditions;
      if ((conditions = cursor->getDependencyConditions()) != NULL)
         {
         for (int32_t i=first; i<=last; i++)
            {
            if ((interference & (1 << (i - maskI))) && !(checkRegList & (1 << (i - maskI))))
               {
               TR::RealRegister::RegNum realReg = (TR::RealRegister::RegNum) i;
               TR::Register * interferedVReg = conditions->searchPostConditionRegister(realReg);
               if (interferedVReg == NULL)
                  {
                  interferedVReg = conditions->searchPreConditionRegister(realReg);
                  }
               if (interferedVReg != NULL)
                  {
                  // updateHW == true: remove interferences that do not clobber HW
                  //            false: remove interferences that do not clobber LW
                  if (interferedVReg->assignToHPR() && updateHW)
                     {
                     // if the highWord is free, this is not an interference
                     interference &= ~(1 << (i - maskI));
                     traceMsg(cursor->cg()->comp(),"removed GPR%d, does not interfere HW\n", i - maskI);
                     }
                  else if (interferedVReg->assignToGPR() && !updateHW)
                     {
                     // if the lowWord is free, this is not an interference
                     interference &= ~(1 << (i - maskI));
                     traceMsg(cursor->cg()->comp(),"removed GPR%d, does not interfere LW\n", i - maskI);
                     }
                  // only check this real reg once
                  checkRegList |= (1 << (i - maskI));
                  }
               }
            }
         }

      // we reached the beginning of live range
      if (currentInstruction == virtualReg->getStartOfRange())
         return interference;

      TR::Node * node = cursor->getNode();
      if (nodeBBStart != NULL && node != nodeBBStart)
         {
         return interference;
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
         return interference;
      }

   return interference;
   }

*/

uint8_t
OMR::Z::Machine::getGPRSize()
   {
   return TR::Compiler->target.is64Bit() ? 8 : 4;
   }

///////////////////////////////////////////////////////////////////////////////
//  Constructor

OMR::Z::Machine::Machine(TR::CodeGenerator * cg)
   : OMR::Machine(NUM_S390_GPR, NUM_S390_FPR, NUM_S390_VRF), _cg(cg), _lastGlobalGPRRegisterNumber(-1), _last8BitGlobalGPRRegisterNumber(-1),
   _lastGlobalFPRRegisterNumber(-1), _lastGlobalCCRRegisterNumber(-1), _lastVolatileNonLinkGPR(-1), _lastLinkageGPR(-1),
     _lastVolatileNonLinkFPR(-1), _lastLinkageFPR(-1), _firstGlobalAccessRegisterNumber(-1), _lastGlobalAccessRegisterNumber(-1), _globalEnvironmentRegisterNumber(-1), _globalCAARegisterNumber(-1), _globalParentDSARegisterNumber(-1),
    _globalReturnAddressRegisterNumber(-1),_globalEntryPointRegisterNumber(-1)
   ,_lastGlobalHPRRegisterNumber(-1), _firstGlobalHPRRegisterNumber(-1)
   {
   self()->initialiseRegisterFile();
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
   if (rk == TR_GPR || rk == TR_GPR64)
      {
      first = TR::RealRegister::FirstGPR;
      last  = TR::RealRegister::LastAssignableGPR;
      }
   else if (rk == TR_AR)
      {
      first = TR::RealRegister::FirstAR;
      last = TR::RealRegister::LastAR;
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
 * @return a bit vector identifying assignable regs as 1's.
 */
uint32_t
OMR::Z::Machine::genBitVectOfAssignableGPRs()
   {
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last = TR::RealRegister::LastAssignableGPR;
   uint32_t bitVect = 0x0;

   for (int32_t i = first; i <= last; i++)
      {
      if (_registerFile[i]->getState() == TR::RealRegister::Locked)
         {
         continue;
         }

      bitVect |= TR::RealRegister::getBitMask(i);
      }

   return bitVect;
   }

/**
 * @return a bit vector identifying live regs pairs as 1's.
 */
uint8_t
OMR::Z::Machine::genBitVectOfLiveGPRPairs()
   {
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last = TR::RealRegister::LastAssignableGPR;
   uint8_t bitVect = 0x0;
   int32_t bit = 8;

   for (int32_t i = first; i <= last; i+=2)
      {
      if (_registerFile[i]->getState() != TR::RealRegister::Free ||
          _registerFile[i+1]->getState() != TR::RealRegister::Free)
         {
         // MSB = GPR0/1, LSB=GPR14/15
         bitVect |= TR::RealRegister::getBitMask(bit);
         }
      bit--;
      }

   return bitVect;
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
   if (toRealRegister(evenReg)->isHighWordRegister() || toRealRegister(oddReg)->isHighWordRegister())
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
OMR::Z::Machine::isLegalEvenOddRestrictedPair(TR::RealRegister * evenReg, TR::RealRegister * oddReg, uint64_t availRegMask)
   {
   if (evenReg == NULL || oddReg == NULL)
      {
      return false;
      }
   if (toRealRegister(evenReg)->isHighWordRegister() || toRealRegister(oddReg)->isHighWordRegister())
      {
      return false;
      }

   else if (toRealRegister(evenReg)->getRegisterNumber() + 1 == toRealRegister(oddReg)->getRegisterNumber())
      {
      return self()->isLegalEvenRegister(evenReg, ALLOWBLOCKED, availRegMask, ALLOWLOCKED) && self()->isLegalOddRegister(oddReg, ALLOWBLOCKED, availRegMask, ALLOWLOCKED);
      }
   else
      return false;
   }

bool
OMR::Z::Machine::isLegalEvenRegister(TR::RealRegister * reg, bool allowBlocked, uint64_t availRegMask, bool allowLocked)
   {
   // Is the register assigned
   if (reg == NULL)
      {
      return false;
      }

   if (toRealRegister(reg)->isHighWordRegister())
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
      (TR::RealRegister::getBitMask(toRealRegister(reg)->getRegisterNumber()) & availRegMask) ==
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
      (TR::RealRegister::getBitMask(toRealRegister(reg)->getRegisterNumber()) & availRegMask) == 0 ||
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
      (TR::RealRegister::getBitMask(toRealRegister(reg)->getRegisterNumber()) & availRegMask) == 0 ||
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
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last = TR::RealRegister::LastAssignableGPR;

   uint32_t bestWeightSoFar = 0xffffffff;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * lastOddReg = NULL;
   bool foundFreePair = false;

   for (int32_t i = first + 1; i <= last; i += 2)
      {
      // Don't consider registers that can't be assigned.
      if (_registerFile[i - 1]->getState() == TR::RealRegister::Locked ||
         _registerFile[i]->getState() == TR::RealRegister::Locked ||
         _registerFile[i - 1]->getState() == TR::RealRegister::Blocked ||
         _registerFile[i]->getState() == TR::RealRegister::Blocked ||
         (TR::RealRegister::getBitMask(i) & availRegMask) == 0 ||
         (TR::RealRegister::getBitMask(i - 1) & availRegMask) == 0)
         {
         continue;
         }

      lastOddReg = _registerFile[i];
      //_cg->traceRegWeight(lastOddReg, lastOddReg->getWeight());

      if ((_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
         (_registerFile[i - 1]->getState() == TR::RealRegister::Free ||
         _registerFile[i - 1]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[i]->getWeight() < bestWeightSoFar)
         {
         foundFreePair = true;                   // Always prioritize a free pair
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
         _cg->traceRegisterAssignment("BEST LEGAL ODD: %R", lastOddReg);
      return lastOddReg;
      }
   else
      {
      _cg->traceRegisterAssignment("BEST LEGAL ODD: %R", freeRegister);
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

   if (toRealRegister(reg)->isHighWordRegister())
      {
      return false;
      }


   // The reg num types are actually +1 off real reg due to NoReg taking
   // up slot 0 in enumeration
   int32_t regNum = toRealRegister(reg)->getRegisterNumber() - 1;

   // Not a legal Even reg if 1) not odd or 2) reg+1 is locked
   if (regNum % 2 != 1 ||
      (TR::RealRegister::getBitMask(toRealRegister(reg)->getRegisterNumber()) & availRegMask) == 0 ||
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
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last = TR::RealRegister::LastAssignableGPR;

   uint32_t bestWeightSoFar = 0xffffffff;
   TR::RealRegister * freeRegister = NULL;
   TR::RealRegister * lastEvenReg = NULL;
   bool foundFreePair = false;

   for (int32_t i = first; i <= last; i += 2)
      {
      // Don't consider registers that can't be assigned.
      if (_registerFile[i]->getState() == TR::RealRegister::Locked ||
         _registerFile[i + 1]->getState() == TR::RealRegister::Locked ||
         _registerFile[i]->getState() == TR::RealRegister::Blocked ||
         _registerFile[i + 1]->getState() == TR::RealRegister::Blocked ||
         (TR::RealRegister::getBitMask(i) & availRegMask) == 0 ||
         (TR::RealRegister::getBitMask(i + 1) & availRegMask) == 0)
         {
         continue;
         }

      lastEvenReg = _registerFile[i];
      //_cg->traceRegWeight(lastEvenReg, lastEvenReg->getWeight());

      if ((_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
         (_registerFile[i + 1]->getState() == TR::RealRegister::Free ||
         _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[i]->getWeight() <
         bestWeightSoFar)
         {
         foundFreePair = true;                   // Always prioritize a free pair
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
         _cg->traceRegisterAssignment("BEST LAST LEGAL EVEN: %R", lastEvenReg);
      return lastEvenReg;
      }
   else
      {
      _cg->traceRegisterAssignment("BEST LEGAL EVEN: %R", freeRegister);
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
   uint32_t bestWeightSoFar = 0xffffffff;
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
         || (TR::RealRegister::getBitMask(firstReg) & availRegMask) == 0
         || (TR::RealRegister::getBitMask(secondReg) & availRegMask) == 0)
         {
         continue;
         }

      lastFirstOfPair = _registerFile[firstReg];
      lastSecondOfPair = _registerFile[secondReg];

      lastSiblingFPReg = isFirst ? lastFirstOfPair : lastSecondOfPair;

      //_cg->traceRegWeight(lastSiblingFPReg, lastSiblingFPReg->getWeight());

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
      _cg->traceRegisterAssignment("BEST LEGAL sibling FP Reg: %R", lastSiblingFPReg);
      return lastSiblingFPReg;
      }
   else
      {
      _cg->traceRegisterAssignment("BEST LEGAL sibling FP Reg: %R", freeRegister);
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
      newRegister = self()->freeBestRegister(currentInstruction, currentAssignedRegister, kindOfRegister, availRegMask);
      }
   // unblock if we blocked target reg of the instruction
   if (blockingRegister)
      targetRegister->getRealRegister()->unblock();

   _cg->traceRegisterAssignment("%R is assigned to invalid register in this instruction. Shuffling %R => %R...",
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
      availRegMask &= ~(TR::RealRegister::getBitMask(TR::RealRegister::GPR0));

   TR::RealRegister * bestRegister = self()->findBestFreeRegister(currInst, toFreeReg->getKind(), toFreeReg, availRegMask);
   if (bestRegister == NULL)
      self()->spillRegister(currInst, toFreeReg);
   else
      {
      TR::Instruction * cursor = self()->registerCopy(currInst, toFreeReg->getKind(), assignedRegister, bestRegister, _cg, 0);
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
   TR::Compilation *comp = TR::comp();

   bool reverseSpilled = false;

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           kindOfRegister != TR_FPR && kindOfRegister != TR_VRF;
   // Return virtual AR in first pass of RA
   if (!_cg->getRAPassAR() && kindOfRegister == TR_AR)
      return targetRegister;

   bool defsRegister=currInst->defsRegister(targetRegister);
   if (assignedRegister == NULL)
      {
      if (_cg->insideInternalControlFlow())
         {
         TR_ASSERT(0, "ASSERTION assignBestRegisterSingle inside Internal Control Flow on currInst=%p.\n"
                      "Ensure all registers within ICF have a dependency anchored at the end-ICF label\n",currInst);
         }
      }
   if (enableHighWordRA && assignedRegister != NULL)
      {
      // In case we need a register shuffle, if the register we are assigning is a source register of the instruction,
      // the shuffling instruction should happen before this instruction
      TR::Instruction *appendInst = currInst;
      if (!defsRegister && currInst->usesRegister(targetRegister) )
         {
         appendInst = currInst->getPrev();
         }
      if (targetRegister->is64BitReg())
         {
         if ((TR::RealRegister::getBitMask(toRealRegister(assignedRegister)->getRegisterNumber()) & availRegMask) == 0)
           {
           // Oh no.. targetRegister is assigned something it shouldn't be assigned to. Do some shuffling
           // find a new register to shuffle to
           TR::RealRegister * newAssignedRegister = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
           TR::Instruction *cursor = self()->registerCopy(appendInst, kindOfRegister, toRealRegister(assignedRegister), newAssignedRegister, _cg, 0);
           assignedRegister->setAssignedRegister(NULL);
           assignedRegister->setState(TR::RealRegister::Free);
           assignedRegister = newAssignedRegister;
           }
         else if (toRealRegister(assignedRegister)->isLowWordRegister() &&
             toRealRegister(assignedRegister)->getHighWordRegister()->getAssignedRegister() != targetRegister)
            {
            _cg->traceRegisterAssignment("%R is 64bit but HPR is assigned to a different vreg, freeing up HPR", targetRegister);
            if (toRealRegister(assignedRegister)->getHighWordRegister()->getState() == TR::RealRegister::Assigned)
               {
               assignedRegister->block();
               TR::Instruction * cursor = self()->freeHighWordRegister(currInst, toRealRegister(assignedRegister)->getHighWordRegister(), 0);
               assignedRegister->unblock();
               _cg->traceRAInstruction(cursor);
               }
            else if (toRealRegister(assignedRegister)->getHighWordRegister()->getState() == TR::RealRegister::Blocked ||
                     toRealRegister(assignedRegister)->getHighWordRegister()->getState() == TR::RealRegister::Locked)
               {
               TR_ASSERT(0, "\n HPR RA: shouldn't get here, not supported yet!");
               }
            }
        else if (toRealRegister(assignedRegister)->isHighWordRegister())
            {
            _cg->traceRegisterAssignment("%R is 64bit but currently assigned to HPR, shuffling", targetRegister);

            // find a new 64-bit register and shuffle HPR there
            assignedRegister->block();
            TR::RealRegister * assignedRegister64 = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
            assignedRegister->unblock();
            TR::Instruction * cursor = generateExtendedHighWordInstruction(currInst->getNode(), _cg, TR::InstOpCode::LHLR, assignedRegister, assignedRegister64, 0, appendInst);
            _cg->traceRAInstruction(cursor);
            assignedRegister->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Free);
            assignedRegister = assignedRegister64;
            // todo: in regdepds make sure to do the other half: if the register comes in as 64-bit but requires an HPR, need to do shuffling
            }
         targetRegister->setAssignedRegister(assignedRegister);
         toRealRegister(assignedRegister)->getHighWordRegister()->setAssignedRegister(targetRegister);
         toRealRegister(assignedRegister)->getLowWordRegister()->setAssignedRegister(targetRegister);
         toRealRegister(assignedRegister)->getHighWordRegister()->setState(TR::RealRegister::Assigned);
         toRealRegister(assignedRegister)->getLowWordRegister()->setState(TR::RealRegister::Assigned);
         }
      else
         {
         // if the register we are assigning is a source register of the instruction,
         // the high-low word shuffling should happen before this instruction
         TR::Instruction *appendInst = currInst;
         if (!defsRegister && currInst->usesRegister(targetRegister) )
            {
            appendInst = currInst->getPrev();
            }
         //for 32-bit Registers, we need to make sure that the register is in the correct low/high word
         if ((toRealRegister(assignedRegister))->isLowWordRegister() && targetRegister->assignToHPR())
            {
            // need to find a free HPR and move assignedRegister there
            TR::RealRegister * assignedHighWordRegister = NULL;

            if ((assignedHighWordRegister = self()->findBestFreeRegister(currInst, kindOfRegister, targetRegister, availRegMask)) == NULL)
               {
               //assignedRegister->block();
               assignedHighWordRegister = self()->freeBestRegister(currInst, targetRegister, kindOfRegister, availRegMask);
               //assignedRegister->unblock();
               }

            TR::Instruction * cursor = generateExtendedHighWordInstruction(currInst->getNode(), _cg, TR::InstOpCode::LLHFR, assignedRegister, assignedHighWordRegister, 0, appendInst);

            self()->addToUpgradedBlockedList(assignedRegister) ? assignedRegister->setState(TR::RealRegister::Blocked) :
                                                         assignedRegister->setState(TR::RealRegister::Free);
            targetRegister->setAssignedRegister(assignedHighWordRegister);
            assignedHighWordRegister->setAssignedRegister(targetRegister);
            assignedHighWordRegister->setState(TR::RealRegister::Assigned);
            assignedRegister->setAssignedRegister(NULL);
            assignedRegister = assignedHighWordRegister;
            _cg->traceRAInstruction(cursor);
            }
         else if ((toRealRegister(assignedRegister))->isHighWordRegister() && targetRegister->assignToGPR())
            {
           // special case for RISBG, we can change the rotate amount to shuffle low word/ high word
            if (currInst->getOpCodeValue() == TR::InstOpCode::RISBG || currInst->getOpCodeValue() == TR::InstOpCode::RISBGN)
               {
               uint8_t rotateAmnt = ((TR::S390RIEInstruction* )currInst)->getSourceImmediate8();
               ((TR::S390RIEInstruction* )currInst)->setSourceImmediate8(rotateAmnt+32);
               }
            else
               {
               // need to find a free GPR and move assignedRegister there
               TR::RealRegister * assignedLowWordRegister = NULL;
               if ((assignedLowWordRegister = self()->findBestFreeRegister(currInst, kindOfRegister, targetRegister, availRegMask)) == NULL)
                  {
                  //assignedRegister->block();
                  assignedLowWordRegister = self()->freeBestRegister(currInst, targetRegister, kindOfRegister, availRegMask);
                  //assignedRegister->unblock();
                  }

               TR::Instruction * cursor = generateExtendedHighWordInstruction(currInst->getNode(), _cg, TR::InstOpCode::LHLR, assignedRegister, assignedLowWordRegister, 0, appendInst);

               self()->addToUpgradedBlockedList(assignedRegister) ? assignedRegister->setState(TR::RealRegister::Blocked):
                                                            assignedRegister->setState(TR::RealRegister::Free);
               targetRegister->setAssignedRegister(assignedLowWordRegister);
               assignedLowWordRegister->setAssignedRegister(targetRegister);
               assignedLowWordRegister->setState(TR::RealRegister::Assigned);
               assignedRegister->setAssignedRegister(NULL);
               assignedRegister = assignedLowWordRegister;
               _cg->traceRAInstruction(cursor);
               }
            }
         else if ((TR::RealRegister::getBitMask(toRealRegister(assignedRegister)->getRegisterNumber()) & availRegMask) == 0)
           {
           // Oh no.. targetRegister is assigned something it shouldn't be assigned to. Do some shuffling
           // find a new register to shuffle to
           TR::RealRegister * newAssignedRegister = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
           TR::Instruction *cursor = self()->registerCopy(appendInst, kindOfRegister, toRealRegister(assignedRegister), newAssignedRegister, _cg, 0);
           newAssignedRegister->setAssignedRegister(targetRegister);
           newAssignedRegister->setState(TR::RealRegister::Assigned);
           assignedRegister->setAssignedRegister(NULL);
           assignedRegister->setState(TR::RealRegister::Free);
           assignedRegister = newAssignedRegister;
           }
         }
      } // end if(enabledHighWordRA && assignedRegister != NULL)
   else if(assignedRegister != NULL && (TR::RealRegister::getBitMask(toRealRegister(assignedRegister)->getRegisterNumber()) & availRegMask) == 0)
      {
      // Oh no.. targetRegister is assigned something it shouldn't be assigned to
      // Do some shuffling
      // find a new register to shuffle to
      assignedRegister->block();
      TR::RealRegister * newAssignedRegister = self()->findBestRegisterForShuffle(currInst, targetRegister, availRegMask);
      assignedRegister->unblock();
      TR::Instruction *cursor=self()->registerCopy(currInst, kindOfRegister, toRealRegister(assignedRegister), newAssignedRegister, _cg, 0);
      _cg->setRegisterAssignmentFlag(TR_IndirectCoercion);
      _cg->traceRegAssigned(targetRegister,assignedRegister);
      targetRegister->setAssignedRegister(newAssignedRegister);
      newAssignedRegister->setAssignedRegister(targetRegister);
      newAssignedRegister->setState(TR::RealRegister::Assigned);
      assignedRegister->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Free);
      assignedRegister = newAssignedRegister;
      }

   // Make sure we only try to assign real regs to virt regs
   //   TR_ASSERTC(comp, targetRegister->getRealRegister()==NULL,
   //   "assignBestRegisterSingle: Trying to assign a real reg to a real reg in inst\n");

   // Have we already assigned a real register
   if (assignedRegister == NULL)
      {
      // True register model will mark register with a pending restoreSpillState and only when we see a def of this
      // register will we store to spill.
      if ((comp->getOption(TR_EnableTrueRegisterModel)) &&
          targetRegister->isLive() &&
          (targetRegister->isValueLiveOnExit() || !targetRegister->isNotUsedInThisBB()))
        targetRegister->setPendingSpillOnDef();

      // These values are only equal upon the first assignment.  Hence, if they arn't
      // the same, and there is no assigned reg, we must have spilled the reg, so invert
      // the spill state to get the reg back
      if(!comp->getOption(TR_EnableTrueRegisterModel) && targetRegister->getTotalUseCount() != targetRegister->getFutureUseCount())
         {
         assignedRegister = self()->reverseSpillState(currInst, targetRegister);
         reverseSpilled = true;
         }
      else
         {
         TR::Register *cr=targetRegister->getColouredRegister();
         // if nothing is using my coloured register use it
         if (cr && cr->getAssignedRegister() == NULL && cr->getRealRegister()->getState() != TR::RealRegister::Blocked)
           {
           assignedRegister = cr->getRealRegister();
           _cg->setRegisterAssignmentFlag(TR_ByColouring);
           }
         // New reg assignment, find a free reg.
         // If no free reg available,  free one up
         if (assignedRegister == NULL && (assignedRegister = self()->findBestFreeRegister(currInst, kindOfRegister, targetRegister, availRegMask)) == NULL)
            {
            if (enableHighWordRA && targetRegister->is64BitReg())
               {
               // do not highword spill to itself for 64bit reg
               //traceMsg(comp,"\nDo not spill to its own HPR!");
               assignedRegister = self()->freeBestRegister(currInst, targetRegister, kindOfRegister, availRegMask, false, true);
               }
            else
               {
               //traceMsg(comp,"\nCan spill to its own HPR!");
               assignedRegister = self()->freeBestRegister(currInst, targetRegister, kindOfRegister, availRegMask);
               }
            }
         }

      //  We double link the virt and real regs as such:
      //   targetRegister->_assignedRegister() -> assignedRegister
      //   targetRegister                      <- assignedRegister->_assignedRegister()
      targetRegister->setAssignedRegister(assignedRegister);
      assignedRegister->setAssignedRegister(targetRegister);
      assignedRegister->setState(TR::RealRegister::Assigned);

      // in addition, for 64bit assignment, assign HPR to targetRegister also
      // reverseSpill state already take cares of this itself
      if (enableHighWordRA && targetRegister->is64BitReg() && !reverseSpilled)
         {
         //TR_ASSERTC(comp, toRealRegister(assignedRegister)->getHighWordRegister()->getState() == TR::RealRegister::Free,
         //       "\nHW RA: assigning a 64bit virtual reg but HPR is not free!");
         toRealRegister(assignedRegister)->getHighWordRegister()->setAssignedRegister(targetRegister);
         toRealRegister(assignedRegister)->getHighWordRegister()->setState(TR::RealRegister::Assigned);
         }
      _cg->traceRegAssigned(targetRegister, assignedRegister);
      _cg->clearRegisterAssignmentFlags();
      }

   // Handle a definition that requires the register's spill location to be updated
   if(defsRegister &&
      targetRegister->isPendingSpillOnDef())
     {
     // traceMsg(cg()->comp(),"Handling ValueIsLiveOnExit() virtReg=%s assignedRegister=%s\n",cg()->getDebug()->getName(targetRegister),cg()->getDebug()->getName(assignedRegister));
     if(self()->cg()->insideInternalControlFlow())
       self()->reverseSpillState(self()->cg()->getInstructionAtEndInternalControlFlow(), targetRegister, toRealRegister(assignedRegister));
     else
       self()->reverseSpillState(currInst, targetRegister, toRealRegister(assignedRegister));
     }

   // Bookkeeping to update the future use count
      if (doBookKeeping &&
         (assignedRegister->getState() != TR::RealRegister::Locked ||
         self()->supportLockedRegisterAssignment()))
      {
      targetRegister->decFutureUseCount();
      targetRegister->setIsLive();

      TR_ASSERT(targetRegister->getFutureUseCount() >= 0,
               "\nRegister assignment: register [%s] futureUseCount should not be negative (for node [%s], ref count=%d) !\n",
               _cg->getDebug()->getName(targetRegister),
               _cg->getDebug()->getName(currInst->getNode()),
               currInst->getNode()->getReferenceCount());

      // If we arn't re-using this reg anymore, kill assignment
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

      if ((targetRegister->getFutureUseCount() == 0) ||
          killOOLReg ||
          ((comp->getOption(TR_EnableTrueRegisterModel)) && currInst->startOfLiveRange(targetRegister)))
         {
#if DEBUG
         if(comp->getOption(TR_EnableTrueRegisterModel) && currInst->startOfLiveRange(targetRegister) &&
            targetRegister->getFutureUseCount() > 0 && !currInst->getOpCode().isRegCopy() )
           {
           traceMsg(comp,
                    "Start of live range for a non global virtual yet its future count is not zero. Reg=%s Instr=[%p]\n",
                    _cg->getDebug()->getName(targetRegister),
                    currInst);
           }
#endif
         _cg->traceRegFreed(targetRegister, assignedRegister);
         targetRegister->resetIsLive();
         if (enableHighWordRA && targetRegister->is64BitReg())
            {
            toRealRegister(assignedRegister)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(assignedRegister)->getHighWordRegister()->setState(TR::RealRegister::Free);
            _cg->traceRegFreed(targetRegister, toRealRegister(assignedRegister)->getHighWordRegister());
            }
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


   // return the real reg
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

   TR::Register * firstVirtualBaseAR = NULL;
   TR::Register * lastVirtualBaseAR = NULL;
   TR::Compilation *comp = TR::comp();

   if (regPair->isArGprPair())
      {
      if (_cg->getRAPassAR())
         {
         regPair->getARofArGprPair()->decFutureUseCount();
         }
      else
      	 {
         TR::Register *targetReg = self()->assignBestRegisterSingle(regPair->getGPRofArGprPair(), currInst, doBookKeeping, availRegMask);
         currInst->addARDependencyCondition(regPair->getARofArGprPair(), targetReg);
         //regPair->getARofArGprPair()->decTotalUseCount();
         return targetReg;
   	     }
   	  }

   TR::Register * firstReg = regPair->getHighOrder();
   TR::Register * lastReg = regPair->getLowOrder();



   if (firstReg->isArGprPair())
      {
      firstVirtualBaseAR = firstReg->getARofArGprPair();
      firstReg->getGPRofArGprPair()->setAssociation(firstReg->getAssociation());
      firstReg = firstReg->getGPRofArGprPair();
      firstReg->setIsUsedInMemRef();
      }
   if (lastReg->isArGprPair())
      {
      lastVirtualBaseAR = lastReg->getARofArGprPair();
      lastReg->getGPRofArGprPair()->setAssociation(lastReg->getAssociation());
      lastReg = lastReg->getGPRofArGprPair();
      lastReg->setIsUsedInMemRef();
      }


   TR::RealRegister * freeRegisterHigh = firstReg->getAssignedRealRegister();
   TR::RealRegister * freeRegisterLow = lastReg->getAssignedRealRegister();
   TR_RegisterKinds regPairKind = regPair->getKind();

  _cg->traceRegisterAssignment("attempt to assign components of register pair ( %R %R )", firstReg, lastReg);

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           regPairKind != TR_FPR && regPairKind != TR_VRF;

   if (enableHighWordRA)
      {
      if (firstReg->is64BitReg())
         _cg->traceRegisterAssignment("%R is64BitReg", firstReg);
      if (lastReg->is64BitReg())
         _cg->traceRegisterAssignment("%R is64BitReg", lastReg);
      }

   if (freeRegisterHigh == NULL || freeRegisterLow == NULL)
      {
      if (_cg->insideInternalControlFlow())
         {
         TR_ASSERT(0, "ASSERTION assignBestRegisterPair inside Internal Control Flow for inst %p.\n"
                      "Ensure all registers within ICF have a dependency anchored at the end-ICF label\n",currInst);
         }
      }

   // Disable GPR0 for this assignment if it is used in a memref
   if (firstReg->isUsedInMemRef())
      {
      availRegMask &= ~(TR::RealRegister::getBitMask(TR::RealRegister::GPR0));
      availRegMask &= ~(TR::RealRegister::getBitMask(TR::RealRegister::GPR1));
      }

   if (enableHighWordRA && (lastReg->is64BitReg() || firstReg->is64BitReg()))
      {
      regPairKind = TR_GPR64;
      }

   // We need a new placeholder for the pair that will stay with the instruction, leaving the
   // input pair free to change under further allocation.
   TR::RegisterPair * assignedRegPair = new (_cg->trHeapMemory(), TR_MemoryBase::RegisterPair) TR::RegisterPair(lastReg, firstReg);
   if (regPair->getKind() == TR_FPR)
     assignedRegPair->setKind(TR_FPR);

   // In case we need to unspill both sibling regs, we do so concurrently to avoid runspill of siblings
   // triping over each other.
   if (!comp->getOption(TR_EnableTrueRegisterModel) &&
       freeRegisterLow  == NULL && (lastReg->getTotalUseCount() != lastReg->getFutureUseCount()) &&
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

   // True register model will mark register with a pending restoreSpillState and only when we see a def of this
   // register will we store to spill.
   if (freeRegisterLow == NULL &&
       (comp->getOption(TR_EnableTrueRegisterModel)) &&
       lastReg->isLive() &&
       (lastReg->isValueLiveOnExit() || !lastReg->isNotUsedInThisBB()))
     lastReg->setPendingSpillOnDef();
   if (freeRegisterHigh == NULL &&
       (comp->getOption(TR_EnableTrueRegisterModel)) &&
       firstReg->isLive() &&
       (firstReg->isValueLiveOnExit() || !firstReg->isNotUsedInThisBB()))
     firstReg->setPendingSpillOnDef();

   // TotalUse & FutureUse are only equal upon the first assignment.  Hence, if they arn't
   // the same, and there is no assigned reg, we must have spilled the reg, so invert
   // the spill state to get the reg back
   if (freeRegisterLow == NULL &&
       !comp->getOption(TR_EnableTrueRegisterModel) &&
       (lastReg->getTotalUseCount() != lastReg->getFutureUseCount()))
      {
      if (freeRegisterHigh)
         {
         freeRegisterHigh->block();
         }
      freeRegisterLow = self()->reverseSpillState(currInst, lastReg);
      freeRegisterLow->block();
      }

   // TotalUse & FutureUse are only equal upon the first assignment.  Hence, if they arn't
   // the same, and there is no assigned reg, we must have spilled the reg, so invert
   // the spill state to get the reg back
   if (freeRegisterHigh == NULL &&
       !comp->getOption(TR_EnableTrueRegisterModel) &&
       (firstReg->getTotalUseCount() != firstReg->getFutureUseCount()))
      {
      if (freeRegisterLow)
         {
         freeRegisterLow->block();
         }
      freeRegisterHigh = self()->reverseSpillState(currInst, firstReg);
      freeRegisterHigh->block();
      }

   // Propose coloured registers
   if(freeRegisterHigh == NULL)
     {
     TR::Register *firstCr=firstReg->getColouredRegister();
     // Lets check to see if partner is assigned to a legal register and partner's match is free
     bool colouredIsGood=true;
     if( freeRegisterLow && TR_FPR != regPair->getKind() && self()->isLegalOddRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask) &&
         toRealRegister(toRealRegister(freeRegisterLow)->getSiblingRegister())->getState() == TR::RealRegister::Free )
       colouredIsGood=false; // Default algorithm should pick a better register
     else if( freeRegisterLow && TR_FPR == regPair->getKind() && self()->isLegalSecondOfFPRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask) &&
              toRealRegister(toRealRegister(freeRegisterLow)->getSiblingRegister())->getState() == TR::RealRegister::Free )
       colouredIsGood=false; // Default algorithm should pick a better register
     // if nothing is using my coloured register use it
     if (colouredIsGood && firstCr && toRealRegister(firstCr)->getState() != TR::RealRegister::Blocked)
       {
       if(firstCr->getAssignedRegister() == NULL)
         {
         freeRegisterHigh = firstCr->getRealRegister();
         firstCr->setAssignedRegister(firstReg);
         firstReg->setAssignedRegister(firstCr);
         freeRegisterHigh->setState(TR::RealRegister::Assigned);
         _cg->setRegisterAssignmentFlag(TR_ByColouring);
         _cg->traceRegAssigned(firstReg,freeRegisterHigh);
         _cg->clearRegisterAssignmentFlags();
         }
       else if(firstCr->getAssignedRegister() != firstReg)
         {
         // wrong register in coloured
         freeRegisterHigh = firstCr->getRealRegister();
         // make sure to block low order reg so whoever is in my register doesn't get placed in my partner's reg
         freeRegisterHigh->getSiblingRegister()->block();
         self()->coerceRegisterAssignment(currInst, firstReg, toRealRegister(freeRegisterHigh)->getRegisterNumber(), PAIRREG);
         freeRegisterHigh->getSiblingRegister()->unblock();
         }
       }
     }
   if(freeRegisterLow == NULL)
     {
     TR::Register *lastCr=lastReg->getColouredRegister();
     // Lets check to see if partner is assigned to a legal register and partner's match is free
     bool colouredIsGood=true;
     if( freeRegisterHigh && TR_FPR != regPair->getKind() && self()->isLegalEvenRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask) &&
         toRealRegister(toRealRegister(freeRegisterHigh)->getSiblingRegister())->getState() == TR::RealRegister::Free )
       colouredIsGood=false; // Default algorithm should pick a better register
     else if( freeRegisterHigh && TR_FPR == regPair->getKind() && self()->isLegalFirstOfFPRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask) &&
              toRealRegister(toRealRegister(freeRegisterHigh)->getSiblingRegister())->getState() == TR::RealRegister::Free )
       colouredIsGood=false; // Default algorithm should pick a better register
     // if nothing is using my coloured register use it
     if (colouredIsGood && lastCr && toRealRegister(lastCr)->getState() != TR::RealRegister::Blocked)
       {
       if(lastCr->getAssignedRegister() == NULL)
         {
         freeRegisterLow = lastCr->getRealRegister();
         lastCr->setAssignedRegister(lastReg);
         lastReg->setAssignedRegister(lastCr);
         freeRegisterLow->setState(TR::RealRegister::Assigned);
         _cg->setRegisterAssignmentFlag(TR_ByColouring);
         _cg->traceRegAssigned(lastReg,freeRegisterLow);
         _cg->clearRegisterAssignmentFlags();
         }
       else if(lastCr->getAssignedRegister() != lastReg)
         {
         // wrong register in coloured
         freeRegisterLow = lastCr->getRealRegister();
         freeRegisterLow->getSiblingRegister()->block();
         self()->coerceRegisterAssignment(currInst, lastReg, toRealRegister(freeRegisterLow)->getRegisterNumber(), PAIRREG);
         freeRegisterLow->getSiblingRegister()->unblock();
         }
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

      if (enableHighWordRA && firstReg->is64BitReg())
         {
         toRealRegister(freeRegisterHigh)->getHighWordRegister()->setState(TR::RealRegister::Assigned);
         toRealRegister(freeRegisterHigh)->getHighWordRegister()->setAssignedRegister(firstReg);
         }

      if (enableHighWordRA && lastReg->is64BitReg())
         {
         toRealRegister(freeRegisterLow)->getHighWordRegister()->setState(TR::RealRegister::Assigned);
         toRealRegister(freeRegisterLow)->getHighWordRegister()->setAssignedRegister(lastReg);
         }
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
            self()->coerceRegisterAssignment(currInst, lastReg, newOddReg->getRegisterNumber(), PAIRREG);
            freeRegisterLow = newOddReg;
            }
         if (lastReg)
            {
            lastReg->block(); // Make sure we don't move this anymore when coercing firstReg
            }

         self()->coerceRegisterAssignment(currInst, firstReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1), PAIRREG);
         freeRegisterHigh = self()->getS390RealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
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
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1), PAIRREG);
         freeRegisterLow = self()->getS390RealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
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
                                     (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1), PAIRREG);
            freeRegisterLow = self()->getS390RealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh)->getRegisterNumber() + 1));
            }
         else if (self()->isLegalOddRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
            {
            if (lastReg)
               {
               lastReg->block();  // Make sure we don't move this anymore when coercing firstReg
               }

            self()->coerceRegisterAssignment(currInst, firstReg,
                                     (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1), PAIRREG);
            freeRegisterHigh = self()->getS390RealRegister((TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow)->getRegisterNumber() - 1));
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

            self()->coerceRegisterAssignment(currInst, firstReg, (toRealRegister(tfreeRegisterHigh)->getRegisterNumber()), PAIRREG);
            self()->coerceRegisterAssignment(currInst, lastReg, (toRealRegister(tfreeRegisterLow)->getRegisterNumber()), PAIRREG);

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
            self()->coerceRegisterAssignment(currInst, lastReg, newLowFPReg->getRegisterNumber(), PAIRREG);
            freeRegisterLow = newLowFPReg;
            }
         if (lastReg)
            {
            lastReg->block(); // Make sure we don't move this anymore when coercing firstReg
            }
         self()->coerceRegisterAssignment(currInst, firstReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterLow->getSiblingRegister())->getRegisterNumber()), PAIRREG);
         freeRegisterHigh = toRealRegister(freeRegisterLow->getSiblingRegister());
         }
      else if (!self()->isLegalSecondOfFPRegister(freeRegisterLow, DISALLOWBLOCKED, availRegMask))
         {
         // make sure the low FP register is legal
         //
         if (!self()->isLegalFirstOfFPRegister(freeRegisterHigh, DISALLOWBLOCKED, availRegMask))
            {
            TR::RealRegister * newHighFPReg = self()->findBestLegalSiblingFPRegister(true,availRegMask);
            self()->coerceRegisterAssignment(currInst, firstReg, newHighFPReg->getRegisterNumber(), PAIRREG);
            freeRegisterHigh = newHighFPReg;
            }
         if (firstReg)
            {
            firstReg->block();  // Make sure we don't move this anymore when coercing lastReg
            }

         self()->coerceRegisterAssignment(currInst, lastReg,
            (TR::RealRegister::RegNum) (toRealRegister(freeRegisterHigh->getSiblingRegister())->getRegisterNumber()), PAIRREG);
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

         self()->coerceRegisterAssignment(currInst, firstReg, (toRealRegister(tfreeRegisterHigh)->getRegisterNumber()), PAIRREG);
         self()->coerceRegisterAssignment(currInst, lastReg, (toRealRegister(tfreeRegisterLow)->getRegisterNumber()), PAIRREG);

         freeRegisterLow = tfreeRegisterLow;
         freeRegisterHigh = tfreeRegisterHigh;
         }
      } // ...FP Pair
   // Do bookkeeping of virt reg
   //   Decrement the future use count as we are, by defn of trying to assign a
   //   a real reg, using it again.
   // Check to see if either register can be freed if ref count is 0
   //

   // Handle a definition that requires the register's spill location to be updated
   if(currInst->defsAnyRegister(firstReg) &&
      firstReg->isPendingSpillOnDef())
     {
     if(self()->cg()->insideInternalControlFlow())
       self()->reverseSpillState(self()->cg()->getInstructionAtEndInternalControlFlow(), firstReg, toRealRegister(freeRegisterHigh));
     else
       self()->reverseSpillState(currInst, firstReg, toRealRegister(freeRegisterHigh));
     }
   if(currInst->defsAnyRegister(lastReg) &&
      lastReg->isPendingSpillOnDef())
     {
     if(self()->cg()->insideInternalControlFlow())
       self()->reverseSpillState(self()->cg()->getInstructionAtEndInternalControlFlow(), lastReg, toRealRegister(freeRegisterLow));
     else
       self()->reverseSpillState(currInst, lastReg, toRealRegister(freeRegisterLow));
     }

   // OOL: if a register is first defined (became live) in the hot path, no matter how many futureUseCount left (in the code path)
   // the register is considered as dead now in the hot path, so GC map contains the correct list of live registers
   if (doBookKeeping)
      {
      firstReg->setIsLive();
      if (((firstReg->decFutureUseCount() == 0) ||
           ((comp->getOption(TR_EnableTrueRegisterModel)) && currInst->startOfLiveRange(firstReg)) ||
           (self()->cg()->isOutOfLineHotPath() && firstReg->getStartOfRange() == currInst)) &&
           (freeRegisterHigh->getState() != TR::RealRegister::Locked || self()->supportLockedRegisterAssignment()))
         {
         firstReg->resetIsLive();
         firstReg->setAssignedRegister(NULL);
         if (enableHighWordRA && firstReg->is64BitReg())
            {
            toRealRegister(freeRegisterHigh)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(freeRegisterHigh)->getHighWordRegister()->setState(TR::RealRegister::Free);
            }
         freeRegisterHigh->setAssignedRegister(NULL);
         if (freeRegisterHigh->getState() != TR::RealRegister::Locked)
            freeRegisterHigh->setState(TR::RealRegister::Free);
         }
      lastReg->setIsLive();
      if (((lastReg->decFutureUseCount() == 0) ||
           ((comp->getOption(TR_EnableTrueRegisterModel)) && currInst->startOfLiveRange(lastReg)) ||
           (self()->cg()->isOutOfLineHotPath() && lastReg->getStartOfRange() == currInst)) &&
          (freeRegisterLow->getState() != TR::RealRegister::Locked || self()->supportLockedRegisterAssignment()))
         {
         lastReg->resetIsLive();
         lastReg->setAssignedRegister(NULL);
         if (enableHighWordRA && lastReg->is64BitReg())
            {
            toRealRegister(freeRegisterLow)->getHighWordRegister()->setAssignedRegister(NULL);
            toRealRegister(freeRegisterLow)->getHighWordRegister()->setState(TR::RealRegister::Free);
            }
         freeRegisterLow->setAssignedRegister(NULL);
         if (freeRegisterLow->getState() != TR::RealRegister::Locked)
            freeRegisterLow->setState(TR::RealRegister::Free);
         }
      }

   assignedRegPair->setHighOrder(freeRegisterHigh, self()->cg());
   assignedRegPair->setLowOrder(freeRegisterLow, self()->cg());

   if (firstVirtualBaseAR!=NULL)
      {
      currInst->addARDependencyCondition(firstVirtualBaseAR, freeRegisterHigh);
      }
   if (lastVirtualBaseAR!=NULL)
      {
      currInst->addARDependencyCondition(lastVirtualBaseAR, freeRegisterLow);
      }
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
   // *this    swipeable for debugging purposes
   uint32_t interference = 0;
   int32_t first, maskI;
   int32_t last;
   TR::Compilation *comp = TR::comp();

   TR::RealRegister * freeRegisterLow = NULL;
   TR::RealRegister * freeRegisterHigh = NULL;

   uint64_t bestWeightSoFar = (uint64_t) (-1);
   int32_t iOld = 0, iNew;

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;
   bool highWordPairIsFree = true;

   if ( rk != TR_FPR  && rk != TR_VRF)
      {
      maskI = first = TR::RealRegister::FirstGPR;
      last = TR::RealRegister::LastAssignableGPR;
      // Look at all reg pairs (starting with an even reg)
      for (int32_t i = first; i <= last; i += 2)
         {
         // Don't consider registers that can't be assigned.
         if ((_registerFile[i]->getState() == TR::RealRegister::Locked) ||
            (_registerFile[i + 1]->getState() == TR::RealRegister::Locked) ||
            (TR::RealRegister::getBitMask(i) & availRegMask) == 0 ||
            (TR::RealRegister::getBitMask(i + 1) & availRegMask) == 0)
            {
            continue;
            }

         //_cg->traceRegWeight(_registerFile[i], _registerFile[i]->getWeight());
         //_cg->traceRegWeight(_registerFile[i + 1], _registerFile[i + 1]->getWeight());

         if (enableHighWordRA && rk == TR_GPR64)
            {
            // for 64 bit reg pair, need to check high word pairs also
            highWordPairIsFree = false;
            if ((_registerFile[i]->getHighWordRegister()->getState() == TR::RealRegister::Free ||
                 _registerFile[i]->getHighWordRegister()->getState() == TR::RealRegister::Unlatched) &&
                (_registerFile[i + 1]->getHighWordRegister()->getState() == TR::RealRegister::Free ||
                 _registerFile[i + 1]->getHighWordRegister()->getState() == TR::RealRegister::Unlatched))
               highWordPairIsFree = true;
            }

         // See if this pair is available, and better than the prev
         if ((_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
            (_registerFile[i + 1]->getState() == TR::RealRegister::Free ||
            _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
             highWordPairIsFree &&
            ((_registerFile[i]->getWeight() + _registerFile[i + 1]->getWeight()) < bestWeightSoFar))
            {
            freeRegisterHigh = _registerFile[i];
            freeRegisterLow = _registerFile[i + 1];

            bestWeightSoFar = freeRegisterHigh->getWeight() + freeRegisterLow->getWeight();
            }
         }
      }
   else // if FP Reg Pair
      {
      // Look at all FP reg pairs
      for (int32_t k = 0; k < NUM_S390_FPR_PAIRS; k++)
         {
         int32_t i = _S390FirstOfFPRegisterPairs[k];
         // Don't consider registers that can't be assigned.
         if ((_registerFile[i]->getState() == TR::RealRegister::Locked) ||
            (_registerFile[i + 2]->getState() == TR::RealRegister::Locked) ||
            (TR::RealRegister::getBitMask(i) & availRegMask) == 0 ||
            (TR::RealRegister::getBitMask(i + 2) & availRegMask) == 0)
            {
            continue;
            }

         //_cg->traceRegWeight(_registerFile[i], _registerFile[i]->getWeight());
         //_cg->traceRegWeight(_registerFile[i + 2], _registerFile[i + 2]->getWeight());

         // See if this pair is available, and better than the prev
         if ((_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
            (_registerFile[i + 2]->getState() == TR::RealRegister::Free ||
            _registerFile[i + 2]->getState() == TR::RealRegister::Unlatched) &&
            ((_registerFile[i]->getWeight() + _registerFile[i + 2]->getWeight()) < bestWeightSoFar))
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

   if (enableHighWordRA && rk == TR_GPR64)
      {
      // If unlatched, set it free
      if (freeRegisterHigh != NULL && freeRegisterHigh->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
         {
         freeRegisterHigh->getHighWordRegister()->setAssignedRegister(NULL);
         freeRegisterHigh->getHighWordRegister()->setState(TR::RealRegister::Free);
         }

      // If unlatched, set it free
      if (freeRegisterLow != NULL && freeRegisterLow->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
         {
         freeRegisterLow->getHighWordRegister()->setAssignedRegister(NULL);
         freeRegisterLow->getHighWordRegister()->setState(TR::RealRegister::Free);
         }
      }

   // Did we find a pair, then update the register set structure.
   if (freeRegisterLow != NULL && freeRegisterHigh != NULL)
      {
      *firstRegister = freeRegisterHigh;
      *lastRegister = freeRegisterLow;

      _cg->traceRegisterAssignment("BEST FREE PAIR: (%R, %R)", freeRegisterHigh, freeRegisterLow);
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
   TR::Compilation *comp = TR::comp();
   int32_t first, maskI;
   int32_t last;
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
         (TR::RealRegister::getBitMask(firstReg) & availRegMask) == 0 ||
         (TR::RealRegister::getBitMask(secondReg) & availRegMask) == 0)
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
      // True register model will mark register with a pending restoreSpillState and only when we see a def of this
      // register will we store to spill.
      if ((comp->getOption(TR_EnableTrueRegisterModel)) &&
          (bestVirtCandidateLow->isValueLiveOnExit() || !bestVirtCandidateLow->isNotUsedInThisBB()))
        bestVirtCandidateLow->setPendingSpillOnDef();

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

      if (!comp->getOption(TR_DisableOOL))
         {
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
      } // if (bestVirtCandidateLow != NULL)

   if (bestVirtCandidateHigh != NULL)
      {
      // True register model will mark register with a pending restoreSpillState and only when we see a def of this
      // register will we store to spill.
      if ((comp->getOption(TR_EnableTrueRegisterModel)) &&
          (bestVirtCandidateHigh->isValueLiveOnExit() || !bestVirtCandidateHigh->isNotUsedInThisBB()))
        bestVirtCandidateHigh->setPendingSpillOnDef();

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

      if (!comp->getOption(TR_DisableOOL))
         {
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
   _cg->traceRegisterAssignment("FREE BEST REGISTER PAIR");
   if ( rk == TR_FPR )
     {
     self()->freeBestFPRegisterPair(firstReg,lastReg,currInst, availRegMask);
     return;
     }
   TR::Node * currentNode = currInst->getNode();
   int32_t first, maskI;
   int32_t last;
   TR::Instruction * cursor = NULL;
   TR::Compilation *comp = TR::comp();
   TR::Machine *machine = self()->cg()->machine();

   TR_BackingStore * locationLow;
   TR_BackingStore * locationHigh;

   // Definition of what the best candidate register pair is goes here.
   TR::RealRegister * bestCandidateHigh = NULL;
   TR::RealRegister * bestCandidateLow = NULL;

   TR_Debug * debugObj = self()->cg()->getDebug();

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR &&  rk != TR_VRF;
   maskI = first = TR::RealRegister::FirstGPR;
   last = TR::RealRegister::LastAssignableGPR;

   // Look at all reg pairs (starting with an even reg)
   for (int32_t i = first; i <= last; i += 2)
      {
      // Don't consider registers that can't be assigned.
      if ((_registerFile[i]->getState() == TR::RealRegister::Locked) ||
         (_registerFile[i + 1]->getState() == TR::RealRegister::Locked) ||
         (_registerFile[i]->getState() == TR::RealRegister::Blocked) ||
         (_registerFile[i + 1]->getState() == TR::RealRegister::Blocked) ||
         (TR::RealRegister::getBitMask(i) & availRegMask) == 0 ||
         (TR::RealRegister::getBitMask(i + 1) & availRegMask) == 0)
         {
         continue;
         }

      // Priority to assigned/unassigned pairs.
      if ((_registerFile[i]->getState() == TR::RealRegister::Free || _registerFile[i]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[i + 1]->getState() == TR::RealRegister::Assigned)
         {
         bestCandidateHigh = _registerFile[i];
         bestCandidateLow = _registerFile[i + 1];
         }
      else if ((_registerFile[i + 1]->getState() == TR::RealRegister::Free ||
         _registerFile[i + 1]->getState() == TR::RealRegister::Unlatched) &&
         _registerFile[i]->getState() ==
         TR::RealRegister::Assigned)
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

   //Assert if no register pair was found
   if(bestCandidateHigh == NULL && bestCandidateLow == NULL)
      {
      self()->cg()->getDebug()->printGPRegisterStatus(comp->getOutFile(), machine);
      traceMsg(comp, "OMR::Z::Machine::freeBestRegisterPair -- Ran out of regs to use as pair on Inst %p.\n",currInst);

      TR_ASSERT(0,"OMR::Z::Machine::freeBestRegisterPair -- Ran out of regs to use as a pair on Inst %p.\n",currInst);
      }

   // Now that we've decided on a pair of real-regs to spill, we will need
   // the virtual regs to setup the spill information
   TR::Register * bestVirtCandidateHigh = bestCandidateHigh->getAssignedRegister();
   TR::Register * bestVirtCandidateLow = bestCandidateLow->getAssignedRegister();

   if (enableHighWordRA && rk== TR_GPR64)
      {
      // this is to prevent us from spilling into the high word of candidates
      _cg->setAvailableHPRSpillMask(availRegMask);
      TR::RealRegister::RegNum hprHigh = bestCandidateHigh->getHighWordRegister()->getRegisterNumber();
      TR::RealRegister::RegNum hprLow = bestCandidateLow->getHighWordRegister()->getRegisterNumber();
      _cg->maskAvailableHPRSpillMask(TR::RealRegister::getBitMask(hprHigh));
      _cg->maskAvailableHPRSpillMask(TR::RealRegister::getBitMask(hprLow));
      }
   if (bestVirtCandidateLow != NULL)
      {
        // True register model will mark register with a pending restoreSpillState and only when we see a def of this
        // register will we store to spill.
        if ((comp->getOption(TR_EnableTrueRegisterModel)) &&
            (bestVirtCandidateLow->isValueLiveOnExit() || !bestVirtCandidateLow->isNotUsedInThisBB()))
          bestVirtCandidateLow->setPendingSpillOnDef();

         TR::InstOpCode::Mnemonic opCodeLow;
         if (comp->getOption(TR_ForceLargeRAMoves) ||
             bestVirtCandidateLow->is64BitReg() ||
             bestVirtCandidateLow->getKind() == TR_GPR64)
            {
            opCodeLow = TR::InstOpCode::LG;
            }
         else
            {
            opCodeLow = TR::InstOpCode::getLoadOpCode();
            }

         //if we selected the VM Thread Register to be freed, check to see if the value has already been spilled
         locationLow = bestVirtCandidateLow->getBackingStorage();
         if (_cg->needsVMThreadDependency() && bestVirtCandidateLow == _cg->getVMThreadRegister())
            {
            traceMsg(comp, "\ns390machine: freeBestRegisterPair - low reg is GPR13\n");
            if (bestVirtCandidateLow->getBackingStorage() == NULL)
               {
               traceMsg(comp, "\ns390machine: allocateVMThreadSpill called\n");
               locationLow = _cg->allocateVMThreadSpill();
               traceMsg(comp, "\ns390machine: allocateVMThreadSpill call completed\n");
               }
            }
         else if (locationLow == NULL && !bestVirtCandidateLow->containsInternalPointer())
            {
            if (bestVirtCandidateLow->getKind() == TR_GPR64)
               {
               locationLow = self()->cg()->allocateSpill(8, bestVirtCandidateLow->containsCollectedReference(), NULL, true);
               opCodeLow = TR::InstOpCode::LG;
               }
            else
               {
               if (enableHighWordRA)
                  {
                  if (bestVirtCandidateLow->is64BitReg())
                     {
                     locationLow = self()->cg()->allocateSpill(8,bestVirtCandidateLow->containsCollectedReference(), NULL, true);
                     opCodeLow = TR::InstOpCode::LG;
                     }
                  else
                     {
                     locationLow = self()->cg()->allocateSpill(4,bestVirtCandidateLow->containsCollectedReference(), NULL, true);
                     opCodeLow = TR::InstOpCode::L;
                     }
                  }
               else
                  {
                  locationLow = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), bestVirtCandidateLow->containsCollectedReference(), NULL, true);
                  }
               }
            }
         else if(locationLow == NULL)
            {
            locationLow = self()->cg()->allocateInternalPointerSpill(bestVirtCandidateLow->getPinningArrayPointer());
            }

         if (enableHighWordRA)
            {
            if (rk == TR_GPR64 && bestCandidateLow->getHighWordRegister()->getAssignedRegister() != NULL)
               {
               // if the candidate is assigned on both GPR and HPR, need to spill HPR
               if (bestCandidateLow->getHighWordRegister()->getAssignedRegister() != bestVirtCandidateLow)
                  {
                  self()->cg()->traceRegisterAssignment("spilling HPR %R for reg pair low",
                                                bestCandidateLow->getHighWordRegister()->getAssignedRegister());
                  currInst = self()->freeHighWordRegister(currInst, bestCandidateLow->getHighWordRegister(),0);
                  }
               }

            //TODO: change the logic here so that we don't generate 2 separate Loads to spill a 64-bit reg
            /*
            if (bestVirtCandidateLow->is64BitReg())
               opCodeLow = TR::InstOpCode::LG;
            else
               opCodeLow = TR::InstOpCode::L;
            */
            }

         TR::MemoryReference * tempMRLow = generateS390MemoryReference(currentNode, locationLow->getSymbolReference(), self()->cg());
         locationLow->getSymbolReference()->getSymbol()->setSpillTempLoaded();
         bestVirtCandidateLow->setBackingStorage(locationLow);
         cursor = generateRXInstruction(self()->cg(), opCodeLow, currentNode, bestCandidateLow, tempMRLow, currInst);

         if (enableHighWordRA && bestVirtCandidateLow->is64BitReg())
            {
            bestCandidateLow->getHighWordRegister()->setState(TR::RealRegister::Free);
            bestCandidateLow->getHighWordRegister()->setAssignedRegister(NULL);
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

         if (!comp->getOption(TR_DisableOOL))
            {
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
      } // if bestVirtCandidateLow != NULL
   else
      {
      if (enableHighWordRA && rk == TR_GPR64 &&
          bestCandidateLow->getHighWordRegister()->getAssignedRegister() != NULL)
         {
         // if the candidate is assigned on both GPR and HPR, need to spill HPR
         self()->cg()->traceRegisterAssignment("spilling HPR %R for reg pair low",
                                       bestCandidateLow->getHighWordRegister()->getAssignedRegister());
         currInst = self()->freeHighWordRegister(currInst, bestCandidateLow->getHighWordRegister(),0);
         }
      }

   if (bestVirtCandidateHigh != NULL)
      {
        // True register model will mark register with a pending restoreSpillState and only when we see a def of this
        // register will we store to spill.
        if ((comp->getOption(TR_EnableTrueRegisterModel)) &&
            (bestVirtCandidateHigh->isValueLiveOnExit() || !bestVirtCandidateHigh->isNotUsedInThisBB()))
          bestVirtCandidateHigh->setPendingSpillOnDef();

         TR::InstOpCode::Mnemonic opCodeHigh;
         if (comp->getOption(TR_ForceLargeRAMoves) ||
             bestVirtCandidateHigh->is64BitReg() ||
             bestVirtCandidateHigh->getKind() == TR_GPR64)
            {
            opCodeHigh = TR::InstOpCode::LG;
            }
         else
            {
            opCodeHigh = TR::InstOpCode::getLoadOpCode();
            }

         //if we selected the VM Thread Register to be freed, check to see if the value has already been spilled
         locationHigh = bestVirtCandidateHigh->getBackingStorage();
         if (_cg->needsVMThreadDependency() && bestVirtCandidateHigh == _cg->getVMThreadRegister())
            {
            traceMsg(comp, "\ns390machine: freeBestRegisterPair - high reg is GPR13\n");
            if (bestVirtCandidateHigh->getBackingStorage() == NULL)
               {
               traceMsg(comp, "\ns390machine: allocateVMThreadSpill called\n");
               locationHigh = _cg->allocateVMThreadSpill();
               traceMsg(comp, "\ns390machine: allocateVMThreadSpill call completed\n");
               }
            }
         else if (locationHigh == NULL && !bestVirtCandidateHigh->containsInternalPointer())
            {
            if (bestVirtCandidateHigh->getKind() == TR_GPR64)
               {
               locationHigh = self()->cg()->allocateSpill(8, bestVirtCandidateHigh->containsCollectedReference(), NULL, true);
               opCodeHigh = TR::InstOpCode::LG;
               }
            else
               {
               if (enableHighWordRA)
                  {
                  if (bestVirtCandidateHigh->is64BitReg())
                     {
                     locationHigh = self()->cg()->allocateSpill(8,bestVirtCandidateHigh->containsCollectedReference(), NULL, true);
                     opCodeHigh = TR::InstOpCode::LG;
                     }
                  else
                     {
                     locationHigh = self()->cg()->allocateSpill(4,bestVirtCandidateHigh->containsCollectedReference(), NULL, true);
                     opCodeHigh = TR::InstOpCode::L;
                     }
                  }
               else
                  {
                  locationHigh = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), bestVirtCandidateHigh->containsCollectedReference(), NULL, true);
                  }
               }
            }
         else if(locationHigh == NULL)
            {
            locationHigh = self()->cg()->allocateInternalPointerSpill(bestVirtCandidateHigh->getPinningArrayPointer());
            }

         if (enableHighWordRA)
            {
            if (rk == TR_GPR64 && bestCandidateHigh->getHighWordRegister()->getAssignedRegister() != NULL)
               {
               // if the candidate is assigned on both GPR and HPR, need to spill HPR
               if (bestCandidateHigh->getHighWordRegister()->getAssignedRegister() != bestVirtCandidateHigh)
                  {
                  self()->cg()->traceRegisterAssignment("spilling HPR %R for reg pair high",
                                                bestCandidateHigh->getHighWordRegister()->getAssignedRegister());
                  // todo: inst flags?
                  currInst = self()->freeHighWordRegister(currInst, bestCandidateHigh->getHighWordRegister(),0);
                  }
               }
            //TODO: change the logic here so that we don't generate 2 separate Loads to spill a 64-bit reg
            /*
            if (bestVirtCandidateHigh->is64BitReg())
               opCodeHigh = TR::InstOpCode::LG;
            else
               opCodeHigh = TR::InstOpCode::L;
            */
            }

         TR::MemoryReference * tempMRHigh = generateS390MemoryReference(currentNode, locationHigh->getSymbolReference(), self()->cg());
         locationHigh->getSymbolReference()->getSymbol()->setSpillTempLoaded();
         bestVirtCandidateHigh->setBackingStorage(locationHigh);
         cursor = generateRXInstruction(self()->cg(), opCodeHigh, currentNode, bestCandidateHigh, tempMRHigh, currInst);

         if (enableHighWordRA && bestVirtCandidateHigh->is64BitReg())
            {
            bestCandidateHigh->getHighWordRegister()->setState(TR::RealRegister::Free);
            bestCandidateHigh->getHighWordRegister()->setAssignedRegister(NULL);
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

         if (!comp->getOption(TR_DisableOOL))
            {
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
      } // if (bestVirtCandidateHigh != NULL)
   else
      {
      if (enableHighWordRA && rk == TR_GPR64 &&
          bestCandidateHigh->getHighWordRegister()->getAssignedRegister() != NULL)
         {
         // if the candidate is assigned on both GPR and HPR, need to spill HPR
         self()->cg()->traceRegisterAssignment("spilling HPR %R for reg pair high",
                                       bestCandidateHigh->getHighWordRegister()->getAssignedRegister());
         currInst = self()->freeHighWordRegister(currInst, bestCandidateHigh->getHighWordRegister(),0);
         }
      }


   // Mark associated real regs as free
   bestCandidateHigh->setState(TR::RealRegister::Free);
   bestCandidateLow->setState(TR::RealRegister::Free);
   bestCandidateHigh->setAssignedRegister(NULL);
   bestCandidateLow->setAssignedRegister(NULL);

   if (enableHighWordRA && rk == TR_GPR64)
      {
      bestCandidateHigh->getHighWordRegister()->setState(TR::RealRegister::Free);
      bestCandidateLow->getHighWordRegister()->setState(TR::RealRegister::Free);
      bestCandidateHigh->getHighWordRegister()->setAssignedRegister(NULL);
      bestCandidateLow->getHighWordRegister()->setAssignedRegister(NULL);
      }

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
                                     uint64_t          availRegMask,
                                     bool              needsHighWord)
   {
   // *this    swipeable for debugging purposes
   uint32_t interference = 0;
   int32_t first, maskI, last;
   uint32_t randomInterference;
   int32_t randomWeight;
   uint32_t randomPreference;
   TR::Compilation *comp = TR::comp();

   uint32_t preference = 0;
   if(virtualReg != NULL)
     {
     TR::Register *cr=virtualReg->getColouredRegister();
     if(cr)
       preference = toRealRegister(cr)->getRegisterNumber();
     else
       preference = virtualReg->getAssociation();
     }
   bool useGPR0 = (virtualReg == NULL) ? false : (virtualReg->isUsedInMemRef() == false);
   bool liveRegOn = (_cg->getLiveRegisters(rk) != NULL);
   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                          (rk == TR_GPR || rk == TR_GPR64);

   if (comp->getOption(TR_Randomize))
      {
      randomPreference = preference;
      if (TR::RealRegister::isHPR((TR::RealRegister::RegNum)preference))
         {
         randomPreference = _cg->randomizer.randomInt(TR::RealRegister::FirstHPR,TR::RealRegister::LastHPR);
         }
      else if (TR::RealRegister::isFPR((TR::RealRegister::RegNum)preference))
         {
         randomPreference = _cg->randomizer.randomInt(TR::RealRegister::FirstFPR,TR::RealRegister::LastFPR);
         }
      else if (TR::RealRegister::isVRF((TR::RealRegister::RegNum)preference))
         {
         randomPreference = _cg->randomizer.randomInt(TR::RealRegister::FirstVRF,TR::RealRegister::LastVRF);
         }
      else if (TR::RealRegister::isGPR((TR::RealRegister::RegNum)preference))
         {
         if (useGPR0)
            {
            randomPreference = _cg->randomizer.randomInt(TR::RealRegister::GPR0,TR::RealRegister::LastGPR);
            }
         else
            {
            randomPreference = _cg->randomizer.randomInt(TR::RealRegister::GPR1,TR::RealRegister::LastGPR);
            }
         }
       if (preference != randomPreference && performTransformation(comp,"O^O Random Codegen - Randomizing Preference from: %d to: %d\n", preference, randomPreference))
         {
         preference = randomPreference;
         }
      }

   if (needsHighWord && preference !=0 && !TR::RealRegister::isHPR((TR::RealRegister::RegNum)preference))
      {
      preference = 0;
      }

   uint64_t prefRegMask = TR::RealRegister::getBitMask(preference);

   if (liveRegOn && virtualReg != NULL)
      {
      interference = virtualReg->getInterference();
      if (comp->getOption(TR_Randomize))
         {
         randomInterference = _cg->randomizer.randomInt(0, 65535);
         if (performTransformation(comp , "O^O Random Codegen - Randomizing Interference for %s: Original=%x Random=%x\n" , _cg->getDebug()->getName(virtualReg) , interference , randomInterference))
            {
            interference = randomInterference;
            }
         }
      }

   if (!liveRegOn && useGPR0)
      {
      interference |= 1;
      }

   // Check to see if we exclude GPR0
   if (!useGPR0)
      {
      availRegMask &= (~TR::RealRegister::getBitMask(TR::RealRegister::GPR0));
      availRegMask &= (~TR::RealRegister::getBitMask(TR::RealRegister::HPR0));
      }

   // We can't use FPRs for vector registers when current instruction is a call
   if(virtualReg->getKind() == TR_VRF && self()->cg()->getSupportsVectorRegisters() && currentInstruction->isCall())
     {
     for(int32_t i=TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
       {
       availRegMask &= (~TR::RealRegister::getBitMask(i));
       }
     }

   if (rk == TR_GPR || rk == TR_GPR64)
      {
      maskI = first = TR::RealRegister::FirstGPR;
      last = TR::RealRegister::LastAssignableGPR;
      }
   else if (rk == TR_AR)
      {
      maskI = first = TR::RealRegister::FirstAR;
      last = TR::RealRegister::LastAR;
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

   if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalEvenOfPair && !needsHighWord)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalOddRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = TR::RealRegister::getBitMask(toRealRegister(realSibling)->getRegisterNumber() - 1);
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
       *                   Preference                 needsHighWord?
       *    1. TR::RealRegister::LegalEvenOfPair        No
       *    2. TR::RealRegister::LegalOddOfPair         No
       *    3. TR::RealRegister::LegalFirstOfFPPair     N/A
       *    4. TR::RealRegister::LegalSecondOfFPPair    N/A
       * 5. None of the above
       */
      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && virtualReg->is64BitReg())
         {
         if (bestRegister != NULL &&
             (bestRegister->getState() == TR::RealRegister::Free ||
              bestRegister->getState() == TR::RealRegister::Unlatched) &&
             (bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Free ||
              bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched))
            {
            if (bestRegister->getState() == TR::RealRegister::Unlatched)
               {
               bestRegister->setAssignedRegister(NULL);
               bestRegister->setState(TR::RealRegister::Free);
               }
            if (bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
               {
               bestRegister->getHighWordRegister()->setAssignedRegister(NULL);
               bestRegister->getHighWordRegister()->setState(TR::RealRegister::Free);
               }
            return bestRegister;
            }
         bestRegister = NULL;
         }
      else // Pre zG+ machine don't support Highword facility
         {
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
      }
   else if (self()->cg()->enableRegisterPairAssociation() && preference == TR::RealRegister::LegalOddOfPair && !needsHighWord)
      {
      // Check to see if there is a sibling already assigned
      if ((virtualReg->getSiblingRegister()) &&
         (realSibling = virtualReg->getSiblingRegister()->getAssignedRegister()) &&
         self()->isLegalEvenRegister((TR::RealRegister *)
            realSibling,
            ALLOWBLOCKED,
            availRegMask))
         {
         uint64_t sibRegMask = TR::RealRegister::getBitMask(toRealRegister(realSibling)->getRegisterNumber() + 1);

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
      if (self()->cg()->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && virtualReg->is64BitReg())
         {
         if (bestRegister != NULL &&
             (bestRegister->getState() == TR::RealRegister::Free ||
              bestRegister->getState() == TR::RealRegister::Unlatched) &&
             (bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Free ||
              bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched))
            {
            if (bestRegister->getState() == TR::RealRegister::Unlatched)
               {
               bestRegister->setAssignedRegister(NULL);
               bestRegister->setState(TR::RealRegister::Free);
               }
            if (bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
               {
               bestRegister->getHighWordRegister()->setAssignedRegister(NULL);
               bestRegister->getHighWordRegister()->setState(TR::RealRegister::Free);
               }
            return bestRegister;
            }
         bestRegister = NULL;
         }
      else
         {
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
         uint64_t sibRegMask = TR::RealRegister::getBitMask(toRealRegister(realSibling)->getRegisterNumber() - 2);
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
         uint64_t sibRegMask = TR::RealRegister::getBitMask(toRealRegister(realSibling)->getRegisterNumber() + 2);
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
         prefRegMask = TR::RealRegister::getBitMask(preference);
         }

      if (!enableHighWordRA)
         {
         if ((prefRegMask & availRegMask) && _registerFile[preference] != NULL &&
             ((_registerFile[preference]->getState() == TR::RealRegister::Free) ||
              (_registerFile[preference]->getState() == TR::RealRegister::Unlatched)))
            {
            bestWeightSoFar = 0x0fffffff;

            bestRegister = _registerFile[preference];
            if (bestRegister != NULL && bestRegister->getState() == TR::RealRegister::Unlatched)
               {
               bestRegister->setAssignedRegister(NULL);
               bestRegister->setState(TR::RealRegister::Free);
               }
            _cg->setRegisterAssignmentFlag(TR_ByAssociation);
            return bestRegister;
            }
         }
      // HighWord RA stuff
      else
         {
         if (virtualReg->is64BitReg() && !needsHighWord)
            {
            bool candidateLWFree = true;
            bool candidateHWFree = true;

            // if we have a preferred association
            if (preference != 0 && (prefRegMask & availRegMask) && _registerFile[preference] != NULL)
               {
               candidateLWFree =
                  (_registerFile[preference]->getState() == TR::RealRegister::Free) ||
                  (_registerFile[preference]->getState() == TR::RealRegister::Unlatched);
               candidateHWFree =
                  (_registerFile[preference]->getHighWordRegister()->getState() == TR::RealRegister::Free) ||
                  (_registerFile[preference]->getHighWordRegister()->getState() == TR::RealRegister::Unlatched);
               }

            // check if the preferred Full size reg is free
            if ((prefRegMask & availRegMask) && candidateLWFree && candidateHWFree && _registerFile[preference] != NULL)
               {
               bestWeightSoFar = 0x0fffffff;
               bestRegister = _registerFile[preference];
               if (bestRegister != NULL && bestRegister->getState() == TR::RealRegister::Unlatched)
                  {
                  bestRegister->setAssignedRegister(NULL);
                  bestRegister->setState(TR::RealRegister::Free);
                  }
               if (bestRegister != NULL &&
                   bestRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
                  {
                  bestRegister->getHighWordRegister()->setAssignedRegister(NULL);
                  bestRegister->getHighWordRegister()->setState(TR::RealRegister::Free);
                  }
               _cg->setRegisterAssignmentFlag(TR_ByAssociation);


               if (bestRegister != NULL)
                  _cg->traceRegisterAssignment("BEST FREE REG by pref for %R is %R", virtualReg, bestRegister);
               else
                  _cg->traceRegisterAssignment("BEST FREE REG by pref for %R is NULL", virtualReg);

               return bestRegister;
               }
            }
         else
            {
            // Only need LW or HW
            TR::RealRegister * candidate = NULL;
            if (preference != 0 && (prefRegMask & availRegMask) && _registerFile[preference] != NULL)
               {
               if (virtualReg->assignToHPR() || needsHighWord)
                  candidate = _registerFile[preference]->getHighWordRegister();
               else
                  candidate = _registerFile[preference]->getLowWordRegister();
               }
            if (candidate != NULL &&
                (prefRegMask & availRegMask) &&
                ((candidate->getState() == TR::RealRegister::Free) ||
                 (candidate->getState() == TR::RealRegister::Unlatched)))
               {
               bestWeightSoFar = 0x0fffffff;
               bestRegister = candidate;
               if (bestRegister != NULL && bestRegister->getState() == TR::RealRegister::Unlatched)
                  {
                  bestRegister->setAssignedRegister(NULL);
                  bestRegister->setState(TR::RealRegister::Free);
                  }
               _cg->setRegisterAssignmentFlag(TR_ByAssociation);


               if (bestRegister != NULL)
                  _cg->traceRegisterAssignment("BEST FREE REG by pref for %R is %R", virtualReg, bestRegister);
               else
                  _cg->traceRegisterAssignment("BEST FREE REG by pref for %R is NULL", virtualReg);

               return bestRegister;
               }
            }
         }
      }

   /*******************************************************************************************************************/
   /*            STEP 2                         Good 'ol linear search                                              */
   /****************************************************************************************************************/
   // If no association or assoc fails, find any other free register
   for (int32_t i = first; i <= last; i++)
      {
      uint64_t tRegMask = TR::RealRegister::getBitMask(i);

      if(!enableHighWordRA)
         {
         // Don't consider registers that can't be assigned.
         if ((_registerFile[i]->getState() == TR::RealRegister::Locked) || ((tRegMask & availRegMask) == 0))
            {
            continue;
            }
         //_cg->traceRegWeight(_registerFile[i], _registerFile[i]->getWeight());

         iNew = interference & (1 << (i - maskI));
         if ((_registerFile[i]->getState() == TR::RealRegister::Free || (_registerFile[i]->getState() == TR::RealRegister::Unlatched)) &&
             (freeRegister == NULL || (iOld && !iNew) || ((iOld || !iNew) && _registerFile[i]->getWeight() < bestWeightSoFar)))
            {
            iOld = iNew;

            freeRegister = _registerFile[i];
            bestWeightSoFar = freeRegister->getWeight();
            if (comp->getOption(TR_Randomize))
               {
               randomWeight = _cg->randomizer.randomInt(0, 0xFFF);
               if (performTransformation(comp, "O^O Random Codegen - Randomizing Weight for %s, Original bestWeightSoFar: %x randomized to: %x\n", _cg->getDebug()->getName(_registerFile[i]), bestWeightSoFar, randomWeight))
                  {
                  bestWeightSoFar =  randomWeight;
                  }
               }

            }
         }
      else
         {
         TR::RealRegister * candidate = _registerFile[i];
         if (candidate->getHighWordRegister()->getState() == TR::RealRegister::Locked)
            {
            if (candidate->getState() == TR::RealRegister::Free)
               {
               candidate->getHighWordRegister()->resetState(TR::RealRegister::Free);
               }
            }

         if (virtualReg->is64BitReg() && !needsHighWord)
            {
            bool candidateLWFree =
               (candidate->getState() == TR::RealRegister::Free) ||
               (candidate->getState() == TR::RealRegister::Unlatched);
            bool candidateHWFree =
               (candidate->getHighWordRegister()->getState() == TR::RealRegister::Free) ||
               (candidate->getHighWordRegister()->getState() == TR::RealRegister::Unlatched);


            // Don't consider registers that can't be assigned.
            if ((candidate->getState() == TR::RealRegister::Locked) ||
                (candidate->getHighWordRegister()->getState() == TR::RealRegister::Locked) ||
                ((tRegMask & availRegMask) == 0))
               {
               continue;
               }
            //_cg->traceRegWeight(candidate, candidate->getWeight());

            iNew = interference & (1 << (i - maskI));
            if (candidateLWFree && candidateHWFree &&
                (freeRegister == NULL || (iOld && !iNew) || ((iOld || !iNew) && candidate->getWeight() < bestWeightSoFar)))
               {
               iOld = iNew;

               freeRegister = candidate;
               bestWeightSoFar = freeRegister->getWeight();
               if (comp->getOption(TR_Randomize))
                  {
                  randomWeight = _cg->randomizer.randomInt(0, 0xFFF);
                  if (performTransformation(comp, "O^O Random Codegen - Randomizing Weight for %s, Original bestWeightSoFar: %x randomized to: %x\n", _cg->getDebug()->getName(_registerFile[i]), bestWeightSoFar, randomWeight))
                     {
                     bestWeightSoFar =  randomWeight;
                     }
                  }
               }
            }
         else
            {
            if (virtualReg->assignToHPR() || needsHighWord)
               {
               candidate = _registerFile[i]->getHighWordRegister();
               tRegMask = TR::RealRegister::getBitMask(candidate->getRegisterNumber());
               }
            else
               {
               candidate = _registerFile[i]->getLowWordRegister();
               }

            //_cg->traceRegWeight(candidate, candidate->getWeight());
            // Don't consider registers that can't be assigned.
            if ((candidate->getState() == TR::RealRegister::Locked) || ((tRegMask & availRegMask) == 0))
               {
               continue;
               }

            iNew = interference & (1 << (i - maskI));
            if ((candidate->getState() == TR::RealRegister::Free || (candidate->getState() == TR::RealRegister::Unlatched)) &&
                (freeRegister == NULL || (iOld && !iNew) || ((iOld || !iNew) && candidate->getWeight() < bestWeightSoFar)))
               {
               iOld = iNew;

               freeRegister = candidate;
               bestWeightSoFar = freeRegister->getWeight();
               if (comp->getOption(TR_Randomize))
                  {
                  randomWeight = _cg->randomizer.randomInt(0, 0xFFF);
                  if (performTransformation(comp, "O^O Random Codegen - Randomizing Weight for %s, Original bestWeightSoFar: %x randomized to: %x\n", _cg->getDebug()->getName(_registerFile[i]), bestWeightSoFar, randomWeight))
                     {
                     bestWeightSoFar =  randomWeight;
                     }
                  }
               }
            }
         }
      }

   if (freeRegister != NULL && freeRegister->getState() == TR::RealRegister::Unlatched)
      {
      freeRegister->setAssignedRegister(NULL);
      freeRegister->setState(TR::RealRegister::Free);
      }

   if (enableHighWordRA && virtualReg->is64BitReg())
      {
      // need to update HW for full size regs
      if (freeRegister != NULL && freeRegister->getHighWordRegister()->getState() == TR::RealRegister::Unlatched)
         {
         freeRegister->getHighWordRegister()->setAssignedRegister(NULL);
         freeRegister->getHighWordRegister()->setState(TR::RealRegister::Free);
         }
      }


   if (freeRegister != NULL)
      _cg->traceRegisterAssignment("BEST FREE REG for %R is %R", virtualReg, freeRegister);
   else
      _cg->traceRegisterAssignment("BEST FREE REG for %R is NULL (could not find one)", virtualReg);


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
   TR::Linkage * linkage = _cg->getS390Linkage();
   int32_t first = TR::RealRegister::FirstGPR + 1;  // skip GPR0
   int32_t last  = TR::RealRegister::LastAssignableGPR;
   uint64_t vector = 0;
   int32_t cnt         = 1;

   for (int32_t i=first; i<=last; i++)
      {
      TR::RealRegister * realReg = self()->getS390RealRegister(cnt+1);

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
      TR::RealRegister * realReg = self()->getS390RealRegister((TR::RealRegister::RegNum) i);
      if ( realReg->getState() != TR::RealRegister::Locked && !currentInstruction->usesRegister(realReg))
         {
         spill = realReg;
         }
      }

   TR_ASSERT( spill != NULL, "OMR::Z::Machine::findRegNotUsedInInstruction -- A spill reg should always be found.");

   return spill;
   }


/**
 * Look for the virtual reg in the highword register table.
 * @return the real reg if found, NULL if not found
 */
TR::RealRegister *
OMR::Z::Machine::findVirtRegInHighWordRegister(TR::Register *virtReg)
   {
   int32_t first = TR::RealRegister::FirstHPR;
   int32_t last  = TR::RealRegister::LastHPR;
   TR::Machine *machine = self()->cg()->machine();

   for (int32_t i = first; i <= last; i++)
      {
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);

      if (realReg->getAssignedRegister() && virtReg == realReg->getAssignedRegister() )
         {
         return realReg;
         }
      }

   return NULL;
   }


void
OMR::Z::Machine::allocateUpgradedBlockedList(TR_Stack<TR::RealRegister*> *mem)
   {
   _blockedUpgradedRegList = mem;
   }

bool
OMR::Z::Machine::addToUpgradedBlockedList(TR::RealRegister * reg)
   {
   if (reg == NULL)
      return false;
   self()->cg()->traceRegisterAssignment("Adding %s (0x%p) to blocked reg list", getRegisterName(reg,self()->cg()), reg);
   _blockedUpgradedRegList->push(reg);
   return true;
   }

TR::RealRegister *
OMR::Z::Machine::getNextRegFromUpgradedBlockedList()
   {
   if (_blockedUpgradedRegList->isEmpty())
      return NULL;
   TR::RealRegister * reg = _blockedUpgradedRegList->pop();
   TR_ASSERT(reg->getAssignedRegister() == NULL,
         "Register %s (0x%p) from Blocked-list has assigned reg %p", getRegisterName(reg,self()->cg()), reg, reg->getAssignedRegister());
   TR_ASSERT(reg->getState() == TR::RealRegister::Blocked,
         "Register %s (0x%p) from Blocked-list is not in Blocked state", getRegisterName(reg,self()->cg()), reg);
   return reg;
   }

// Spill all volatile access registers by either
//   1) moving them to a non-volatile access register
//   2) creating a true spill
//

/**
 * Block all volatile access registers
 */
void
OMR::Z::Machine::blockVolatileAccessRegisters()
   {
   int32_t first = TR::RealRegister::FirstAR;
   int32_t last  = TR::RealRegister::LastAR;
   TR::Machine *machine = self()->cg()->machine();
   TR::Linkage * linkage = _cg->getS390Linkage();

   for (int32_t i = first; i <= last; i++)
      {
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);

      bool volatileReg = !linkage->getPreserved((TR::RealRegister::RegNum) i);

      if ( volatileReg )
         {
         TR_ASSERT(realReg->getState() == TR::RealRegister::Free || realReg->getState() == TR::RealRegister::Locked,
            "OMR::Z::Machine::blockVolatileAccessRegister -- all volatile access regs should be free.");
         realReg->setState(TR::RealRegister::Blocked);
         }
      }
   }

/**
 * Unblock all volatile access registers
 */
void
OMR::Z::Machine::unblockVolatileAccessRegisters()
   {
   int32_t first = TR::RealRegister::FirstAR;
   int32_t last  = TR::RealRegister::LastAR;
   TR::Machine *machine = self()->cg()->machine();
   TR::Linkage * linkage = _cg->getS390Linkage();

   for (int32_t i = first; i <= last; i++)
      {
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);
      bool volatileReg = !linkage->getPreserved((TR::RealRegister::RegNum) i);

      if ( volatileReg )
         {
         realReg->setState(TR::RealRegister::Free);
         }
      }

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
   TR::Linkage * linkage = _cg->getS390Linkage();
   TR::Node * node = currentInstruction->getNode();

   for (int32_t i = first; i <= last; i++)
      {
      TR::Register * virtReg = NULL;
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);
      bool volatileReg = true;     // All high regs are volatile
                                   // !linkage->getPreserved((TR::RealRegister::RegNum) i);

      if (volatileReg                                      &&
          realReg->getState() == TR::RealRegister::Assigned &&
          (virtReg = realReg->getAssignedRegister())       &&
          virtReg->getKind() == TR_GPR64
         )
         {
         self()->spillRegister(currentInstruction, virtReg);
         }
      }
   }

/**
 * Block all volatile high registers
 */
void
OMR::Z::Machine::blockVolatileHighRegisters()
   {
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last  = TR::RealRegister::LastGPR;
   TR::Machine *machine = self()->cg()->machine();
   TR::Linkage * linkage = _cg->getS390Linkage();

   for (int32_t i = first; i <= last; i++)
      {
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);

      bool volatileReg = true; // All high regs are volatile
                               //!linkage->getPreserved((TR::RealRegister::RegNum) i);

      if (volatileReg)
         {
         TR_ASSERT(realReg->getState() == TR::RealRegister::Free || realReg->getState() == TR::RealRegister::Locked,
            "OMR::Z::Machine::blockVolatileAccessRegister -- all volatile access regs should be free.");
         realReg->setState(TR::RealRegister::Blocked);
         }
      }
   }

/**
 * Unblock all volatile high registers
 */
void
OMR::Z::Machine::unblockVolatileHighRegisters()
   {
   int32_t first = TR::RealRegister::FirstGPR;
   int32_t last  = TR::RealRegister::LastGPR;
   TR::Machine *machine = self()->cg()->machine();
   TR::Linkage * linkage = _cg->getS390Linkage();

   for (int32_t i = first; i <= last; i++)
      {
      TR::RealRegister * realReg =
         machine->getS390RealRegister((TR::RealRegister::RegNum) i);
      bool volatileReg = true; // All high regs are volatile
                               //!linkage->getPreserved((TR::RealRegister::RegNum) i);
      if (volatileReg)
         {
         realReg->setState(TR::RealRegister::Free);
         }
      }

   }


////////////////////////////////////////////////////

TR::RealRegister *
OMR::Z::Machine::freeBestRegister(TR::Instruction * currentInstruction, TR::Register * virtReg, TR_RegisterKinds rk,
                                 uint64_t availRegMask, bool allowNullReturn, bool doNotSpillToSiblingHPR)
   {
   _cg->traceRegisterAssignment("FREE BEST REGISTER FOR %R", virtReg);
   TR::Compilation *comp = TR::comp();

   if (virtReg->containsCollectedReference())
      _cg->traceRegisterAssignment("%R contains collected", virtReg);
   // *this    swipeable for debugging purposes
   int32_t numCandidates = 0, interference = 0, first, last, maskI;
   TR::Register * candidates[TR::RealRegister::LastVRF];
   TR::Machine *machine = self()->cg()->machine();
   bool useGPR0 = (virtReg == NULL) ? false : (virtReg->isUsedInMemRef() == false);

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;

   switch (rk)
      {
      case TR_GPR64:
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
      case TR_AR:
         maskI = first = TR::RealRegister::FirstAR;
         last = TR::RealRegister::LastAR;
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
      TR::RealRegister * realReg = machine->getS390RealRegister((TR::RealRegister::RegNum) i);

      if (!enableHighWordRA)
         {
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register * associatedVirtual = realReg->getAssignedRegister();
            bool          usedInMemRef      = associatedVirtual->isUsedInMemRef();

            if (currentInstruction->getDependencyConditions() &&
                currentInstruction->getDependencyConditions()->searchPostConditionRegister(associatedVirtual))
               {
               // we just assigned this virtual in the reg deps, do not free it
               traceMsg(_cg, "  Reg[@%d] associatedVirtual[%s] was excluded from spill target because of reg dependency\n",
                       realReg->getRegisterNumber()-1, _cg->getDebug()->getName(associatedVirtual));
               continue;
               }

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
      // HighWord RA stuff
      else
         {
         TR::RealRegister * realRegHW = realReg->getHighWordRegister();

         if (virtReg->assignToHPR())
            {
            if (realRegHW->getState() == TR::RealRegister::Assigned)
               {
               TR::Register * associatedVirtual = realRegHW->getAssignedRegister();

               if ((!iInterfere && i==preference && pref_favored) || (realReg->getState() == TR::RealRegister::Free))
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
         else if (virtReg->assignToGPR())
            {
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
         else if (virtReg->is64BitReg())
            {
            TR::Register * associatedVirtual = NULL;
            bool          usedInMemRef = false;
            bool          assignedToTwoRegs = false; // LW and HW are assigned to 2 different virtRegs, least preferred candidate
            if (realReg->getState() == TR::RealRegister::Assigned && realRegHW->getState() == TR::RealRegister::Assigned)
               {
               if (realReg->getAssignedRegister() != realRegHW->getAssignedRegister())
                  {
                  // prefer to spill a single Vreg
                  assignedToTwoRegs = true;
                  }
               }

            if (realRegHW->getState() == TR::RealRegister::Assigned)
               {
               associatedVirtual = realRegHW->getAssignedRegister();
               }
            if (realReg->getState() == TR::RealRegister::Assigned)
               {
               // candidate is LW's virtReg in case if both LW and HW need to be spilled
               associatedVirtual = realReg->getAssignedRegister();
               usedInMemRef      = associatedVirtual->isUsedInMemRef();
               }

            bool doNotSpillHPR = realRegHW->getAssignedRegister() == virtReg && realReg->getAssignedRegister() != virtReg;

            /*
            // do not to spill an HPR that contains a compressed ref when shift !=0.
            // We have to first decompress the pointer, which means we need an extra 64-bit register or at least the low word register.
            // for shift = 0 case, we can load LFH from stack directly
            if (realRegHW->getAssignedRegister() &&
                realRegHW->getAssignedRegister()->isSpilledToHPR() &&
                realRegHW->getAssignedRegister()->containsCollectedReference())
               {
               //doNotSpillHPR = true;
               }
            */

            if ((realReg->getState() == TR::RealRegister::Assigned || realReg->getState() == TR::RealRegister::Free) &&
                (realRegHW->getState() == TR::RealRegister::Assigned || realRegHW->getState() == TR::RealRegister::Free) &&
                !doNotSpillHPR)
               {
               if ((!iInterfere && i==preference && pref_favored) || (!usedInMemRef && !assignedToTwoRegs))
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
         }
      }

   if (numCandidates == 0)
      {
      if (!allowNullReturn)
         {
         if (self()->cg()->getDebug() != NULL)
            {
            self()->cg()->getDebug()->printGPRegisterStatus(comp->getOutFile(), machine);
            if (!useGPR0)
               traceMsg(comp, "GPR0 is not allowed for %s\n", self()->cg()->getDebug()->getName(virtReg));
            }
         traceMsg(comp, "OMR::Z::Machine::freeBestRegister -- Ran out of regs on Inst %p.\n",currentInstruction);

         TR_ASSERT(0,"OMR::Z::Machine::freeBestRegister -- Ran out of regs on Inst %p.\n",currentInstruction);
         }
      return NULL;
      }

   TR::Instruction * cursor = currentInstruction->getPrev();
   while (numCandidates > 1 && cursor != NULL && cursor->getOpCodeValue() != TR::InstOpCode::LABEL && cursor->getOpCodeValue() != TR::InstOpCode::PROC)
      {
      for (int32_t i = 0; i < numCandidates; i++)
         {
         if (cursor->refsRegister(candidates[i]))
            {
            candidates[i] = candidates[--numCandidates];
            i--; // on continue repeat test for candidate[i] as candidate[i] is now new.
            }
         // also need to check HW. if it's assigned, push the candidate back
         else if (enableHighWordRA && virtReg->is64BitReg() && candidates[i]->getAssignedRegister())
            {
            TR::RealRegister *candidateHW =
               toRealRegister(candidates[i]->getAssignedRegister())->getHighWordRegister();
            if (candidateHW->getState() == TR::RealRegister::Assigned &&
                candidateHW->getAssignedRegister())
               {
               if (cursor->refsRegister(candidateHW->getAssignedRegister()))
                  {
                  candidates[i] = candidates[--numCandidates];
                  i--; // on continue repeat test for candidate[i] as candidate[i] is now new.
                  }
               }
            }
         }
      cursor = cursor->getPrev();
      }

   TR::RealRegister * best = NULL;

   if (candidates[0]->getAssignedRegister())
      {
      best = toRealRegister(candidates[0]->getAssignedRegister());
      }
   else
      {
      TR_ASSERT( enableHighWordRA, "freeBestRegister:a virtual reg is assigned to NULL?");
      // if the best candidate is a previously spilled HPR
      best = toRealRegister(self()->findVirtRegInHighWordRegister(candidates[0]));
      }

   // Shortcut the selection when a virtReg is specified, and it is already assigned a real, we spill
   // that real
   if ( virtReg && virtReg->getRealRegister() )
      {
      candidates[0] = virtReg;
      }

   // check if we need to spill both words
   // todo fix this
   else if (enableHighWordRA &&
            virtReg->is64BitReg() &&
            best->isLowWordRegister() &&
            best->getHighWordRegister()->getAssignedRegister() &&
            best->getHighWordRegister()->getAssignedRegister() != candidates[0])
      {
      // todo, merge the two spills to 1 load
      // bug can't spill 2ice yet
      _cg->traceRegisterAssignment("HW RA: freeBestReg Spill %R for fullsize reg: %R ", best->getHighWordRegister(), virtReg);
      self()->spillRegister(currentInstruction, best->getHighWordRegister()->getAssignedRegister());
      }
   else if (enableHighWordRA && virtReg->assignToHPR())
      {
      best = best->getHighWordRegister();
      }
   else if (enableHighWordRA && (virtReg->is64BitReg() || virtReg->assignToGPR()))
      {
      best = best->getLowWordRegister();
      }

   // If we spill into highword, make sure to not to spill it into the one that will clobber fullsize reg
   if (enableHighWordRA && (virtReg->is64BitReg() || doNotSpillToSiblingHPR))
      {
      uint32_t availHighWordRegMap = ~(TR::RealRegister::getBitMask(toRealRegister(best->getHighWordRegister())->getRegisterNumber())) & availRegMask & 0xffff0000;
      self()->spillRegister(currentInstruction, candidates[0], availHighWordRegMap);
      }
   else
      {
      self()->spillRegister(currentInstruction, candidates[0]);
      }
   return best;
   }

/**
 * Free up a real register
 */
void OMR::Z::Machine::freeRealRegister(TR::Instruction *currentInstruction, TR::RealRegister *targetReal, bool is64BitReg)
  {
  TR::Compilation *comp = TR::comp();
  TR::Register *virtReg=targetReal->getAssignedRegister();
  bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                          targetReal->getKind() != TR_FPR && targetReal->getKind() != TR_VRF;

  if(virtReg)
    {
    if (enableHighWordRA && is64BitReg)
      {
      uint32_t availHighWordRegMap = ~(TR::RealRegister::getBitMask(toRealRegister(targetReal->getHighWordRegister())->getRegisterNumber())) & 0xffff0000;
      self()->spillRegister(currentInstruction, virtReg, availHighWordRegMap);
      }
    else
      {
      self()->spillRegister(currentInstruction, virtReg);
      }
    }
  else if(targetReal->getState() == TR::RealRegister::Assigned)
    {
    // Something is spilled to this register's high word
    TR_ASSERT(false,"freeRealRegister needs to free its high word");
    }
  }

TR::Instruction*
OMR::Z::Machine::freeHighWordRegister(TR::Instruction *currentInstruction, TR::RealRegister * targetRegisterHW, flags32_t instFlags)
   {
   TR::Register * virtRegHW = targetRegisterHW->getAssignedRegister();
   TR::RealRegister * spareReg = NULL;
   TR_RegisterKinds rk = virtRegHW->getKind();
   TR::Instruction * cursor = currentInstruction;

   _cg->traceRegisterAssignment(" freeHighWordRegister: %R:%R ", virtRegHW, targetRegisterHW);

   TR_ASSERT(targetRegisterHW != NULL, "freeHighWordRegister called but targetReg HW is null?");
   TR_ASSERT(targetRegisterHW->isHighWordRegister(), "freeHighWordRegister called for non HPR?");

   // if we are trying to free up the HPR that was used for a 32-bit GPR spill
   if (virtRegHW->getAssignedRegister() == NULL && targetRegisterHW->getState() == TR::RealRegister::Assigned)
      {
      _cg->traceRegisterAssignment(" HW RA %R was spilled to %R, now need to spill again", virtRegHW, targetRegisterHW);

      // try to find another free HPR
      spareReg = self()->findBestFreeRegister(currentInstruction, rk, virtRegHW, self()->cg()->getAvailableHPRSpillMask(), true);

      if (spareReg)
         {
         cursor = self()->registerCopy(currentInstruction, TR_HPR, targetRegisterHW, spareReg, self()->cg(), instFlags);
         targetRegisterHW->setState(TR::RealRegister::Unlatched);
         targetRegisterHW->setAssignedRegister(NULL);
         spareReg->setState(TR::RealRegister::Assigned);
         spareReg->setAssignedRegister(virtRegHW);
         }
      else
         {
         // if no more free regs, spill this one to stack, do not try to HPR spill again
         self()->spillRegister(currentInstruction, virtRegHW, 0);
         }

      return cursor;
      }

   spareReg = self()->findBestFreeRegister(currentInstruction, rk, virtRegHW, self()->cg()->getAvailableHPRSpillMask(), true);

   if (targetRegisterHW->getState() == TR::RealRegister::Blocked)
      {
      if (spareReg == NULL)
         {
         virtRegHW->block();
         spareReg = self()->freeBestRegister(currentInstruction, virtRegHW, rk, self()->cg()->getAvailableHPRSpillMask());
         virtRegHW->unblock();
         }
      TR_ASSERT(spareReg != NULL, "freeHighWordRegister: blocked, must find a spareReg");

      cursor = self()->registerCopy(currentInstruction, TR_HPR, targetRegisterHW, spareReg, self()->cg(), instFlags);
      //TR_ASSERTC( spareReg->isHighWordRegister(),_cg->comp(), "\nfreeHighWordRegister: spareReg is not an HPR?\n");
      spareReg->setAssignedRegister(virtRegHW);
      spareReg->setState(TR::RealRegister::Assigned);
      virtRegHW->setAssignedRegister(spareReg);
      }
   else
      {
      if (spareReg)
         {
         cursor = self()->registerCopy(currentInstruction, TR_HPR, targetRegisterHW, spareReg, self()->cg(), instFlags);
         //TR_ASSERTC( spareReg->isHighWordRegister(),_cg->comp(), "\nfreeHighWordRegister: spareReg is not an HPR?\n");
         spareReg->setAssignedRegister(virtRegHW);
         spareReg->setState(TR::RealRegister::Assigned);
         virtRegHW->setAssignedRegister(spareReg);
         }
      else
         {
         self()->spillRegister(currentInstruction, virtRegHW);
         }
      }

   targetRegisterHW->setAssignedRegister(NULL);
   targetRegisterHW->setState(TR::RealRegister::Free);
   return cursor;
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
OMR::Z::Machine::spillRegister(TR::Instruction * currentInstruction, TR::Register* virtReg, uint32_t availHighWordRegMap)
   {
   TR::Compilation *comp = TR::comp();
   TR::InstOpCode::Mnemonic opCode;
   bool containsInternalPointer = false;
   bool containsCollectedReg    = false;
   TR_RegisterKinds rk = virtReg->getKind();
   TR_BackingStore * location = NULL;
   TR::Node * currentNode = currentInstruction->getNode();
   TR::Instruction * cursor = NULL;
   bool allowCollectRefsInAccessReg = false;
   TR::RealRegister * best = NULL;
   TR_Debug * debugObj = self()->cg()->getDebug();

   if(comp->getOption(TR_EnableTrueRegisterModel))
     {
     virtReg->setValueLiveOnExit();
     }

   // Highword RA flags
   // check: what if virtReg is actually a real Reg??
   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;
   TR::RealRegister * freeHighWordReg = NULL;
   bool alreadySpilledToHPR = false;
   bool alreadySpilledToHPRCollectible = false;

   // If virtReg was already spilled into an HPR, need to find which one it is
   if (enableHighWordRA && virtReg->getAssignedRegister() == NULL)
      {
      best = self()->findVirtRegInHighWordRegister(virtReg);
      // do not attempt HPR spill anymore
      alreadySpilledToHPR = true;
      if (virtReg->containsCollectedReference())
        {
        // in this case, we must either find another free HPR for virtReg or
        // decompress this virtReg and spill it onto the stack
        alreadySpilledToHPRCollectible = true;
        }
      TR_ASSERT(best != NULL, "HW RA: spillRegister cannot find real target reg\n");
      }
   else
      {
      best =  toRealRegister(virtReg->getAssignedRegister());
      if (enableHighWordRA && best->isHighWordRegister())
         {
         alreadySpilledToHPR = true;
         if (virtReg->containsCollectedReference())
            {
            // in this case, we must either find another free HPR for virtReg or
            // decompress this virtReg and spill it onto the stack
            alreadySpilledToHPRCollectible = true;
            }
         }
      }

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

   static char * debugHPR= feGetEnv("TR_debugHPR");

   bool canSpillPointerToHPR = false;

   if (virtReg->containsCollectedReference() && TR::Compiler->target.is64Bit() &&
       comp->useCompressedPointers() && TR::Compiler->vm.heapBaseAddress() == 0 &&
       TR::Compiler->om.compressedReferenceShift() == 0 && !checkOOLInterference(currentInstruction, virtReg))
      {
      canSpillPointerToHPR = true;
      }

   // try to spill to highword
   if (enableHighWordRA && !comp->getOption(TR_DisableHPRSpill) &&
       ((!virtReg->is64BitReg() && !virtReg->containsCollectedReference()) || canSpillPointerToHPR) &&
       best->isLowWordRegister() &&
       !virtReg->containsInternalPointer() &&
       !self()->cg()->isOutOfLineColdPath())
      {
      // try to find a free HW reg
      availHighWordRegMap &= self()->cg()->getAvailableHPRSpillMask();
      freeHighWordReg = self()->findBestFreeRegister(currentInstruction, rk, virtReg, availHighWordRegMap, true);

      if (freeHighWordReg)
         {
         if (debugHPR)
            {
            printf ("\nHW RA: spilling into HPR ");   fflush(stdout);
            }
         self()->cg()->traceRegisterAssignment("\nHW RA: HW spill: %R(%R) into %R\n", virtReg, best, freeHighWordReg);

         // spill to HW
         if (alreadySpilledToHPR)
            {
            cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHHR, best, freeHighWordReg, 0, currentInstruction);
            }
         else
            {
            if (virtReg->containsCollectedReference())
               {
               if (debugHPR)
                  {
                  printf ("(collected) "); fflush(stdout);
                  }
               uint32_t compressShift = TR::Compiler->om.compressedReferenceShift();
               // need to assert heapbase, offset = 0
               if (compressShift == 0)
                  {
                  cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LLHFR, best, freeHighWordReg, 0, currentInstruction);
                  self()->cg()->traceRAInstruction(cursor);
                  cursor = generateRILInstruction(_cg, TR::InstOpCode::IIHF, currentNode, best, (uintptrj_t)0, cursor);
                  self()->cg()->traceRAInstruction(cursor);
                  }
               else
                  {
                  cursor = generateRIEInstruction(_cg, TR::InstOpCode::RISBLG, currentNode, best, freeHighWordReg, 0, 31+0x80-compressShift, 32+compressShift, currentInstruction);
                  self()->cg()->traceRAInstruction(cursor);
                  cursor = generateRIEInstruction(_cg, TR::InstOpCode::RISBHG, currentNode, best, freeHighWordReg, 32-compressShift, 31+0x80, 32+compressShift, currentInstruction);
                  self()->cg()->traceRAInstruction(cursor);
                  }
               }
            else
               {
               cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LLHFR, best, freeHighWordReg, 0, currentInstruction);
               }
            }
         if (debugHPR)
            {
            printf ("in %s ,0x%x, %s", comp->signature(), availHighWordRegMap, comp->getHotnessName(comp->getMethodHotness()));fflush(stdout);
            }

         if (debugObj)
            {
            debugObj->addInstructionComment(cursor, "Load Highword Spill");
            }
         self()->cg()->traceRAInstruction(cursor);

         best->setAssignedRegister(NULL);
         best->setState(TR::RealRegister::Free);
         if (virtReg->containsCollectedReference() && TR::Compiler->target.is64Bit())
            {
            best->getHighWordRegister()->setAssignedRegister(NULL);
            best->getHighWordRegister()->setState(TR::RealRegister::Free);
            }

         freeHighWordReg->setAssignedRegister(virtReg);
         freeHighWordReg->setState(TR::RealRegister::Assigned);

         virtReg->setAssignedRegister(NULL);  // NULL to force reverseSpillState
         virtReg->setSpilledToHPR(true);
         return;
         }
      }

   {
   location = virtReg->getBackingStorage();
   switch (rk)
     {
     case TR_GPR64:
       if (!comp->getOption(TR_DisableOOL) &&
           (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) &&
           virtReg->getBackingStorage())
         {
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       else if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
         {
         location = self()->cg()->allocateSpill(8, virtReg->containsCollectedReference(), NULL, true);
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling %s to (%p)\n", debugObj->getName(virtReg),location);
         }
       opCode = TR::InstOpCode::LG;
       break;
     case TR_GPR:
       if (!comp->getOption(TR_DisableOOL) &&
           (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) &&
           virtReg->getBackingStorage())
         {
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       //if we selected the VM Thread Register to be freed, check to see if the value has already been spilled
       else if (comp->getOption(TR_DisableOOL) &&
                _cg->needsVMThreadDependency() && virtReg == _cg->getVMThreadRegister())
         {
         if (virtReg->getBackingStorage() == NULL)
           {
           traceMsg(comp, "\ns390machine: allocateVMThreadSpill called\n");
           location = _cg->allocateVMThreadSpill();
           traceMsg(comp, "\ns390machine: allocateVMThreadSpill call completed\n");
           }
         else
           {
           traceMsg(comp, "\ns390machine: backing storage already assigned, re-use.\n");
           location = virtReg->getBackingStorage();
           }
         }
       else if (!containsInternalPointer)
         {
         if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
           {
           if ((enableHighWordRA && virtReg->is64BitReg()) || comp->getOption(TR_ForceLargeRAMoves))
             {
             location = self()->cg()->allocateSpill(8,virtReg->containsCollectedReference(), NULL, true);
             }
           else
             {
             location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), virtReg->containsCollectedReference(), NULL, true);
             }
           if (debugObj)
             self()->cg()->traceRegisterAssignment("\nSpilling %s to (%p)\n",debugObj->getName(virtReg),location);
           }
         }
       else if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
         {
         location = self()->cg()->allocateInternalPointerSpill(virtReg->getPinningArrayPointer());
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling internal pointer %s to (%p)\n", debugObj->getName(virtReg),location);
         }

       if (comp->getOption(TR_ForceLargeRAMoves))
         {
         opCode = TR::InstOpCode::LG;
         }
       else
         {
         opCode = TR::InstOpCode::getLoadOpCode();
         }

       if (enableHighWordRA)
         {
         if (best->isHighWordRegister())
           {
           opCode = TR::InstOpCode::LFH;
           }
         else if (best->isLowWordRegister() && best->getHighWordRegister()->getAssignedRegister() != virtReg)
           {
           opCode = TR::InstOpCode::L;
           }
         else
           opCode = TR::InstOpCode::LG;
         //TR_ASSERTC( TR::Compiler->target.is64Bit(),comp, "\nallocateSpill has incorrect spill slot size");
         //this assume kicks in for SLLG, MGHI etc on 31bit
         if (debugObj)
           self()->cg()->traceRegisterAssignment(" HW RA: spilling %R:%R", virtReg, best);
         }
       break;
     case TR_FPR:
       if (!comp->getOption(TR_DisableOOL) &&
           (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) &&
           virtReg->getBackingStorage())
         {
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       else if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
         {
         location = self()->cg()->allocateSpill(8, false, NULL, true); // TODO: Use 4 for single-precision values
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling FPR %s to (%p)\n", debugObj->getName(virtReg),location);
         }
       opCode = TR::InstOpCode::LD;
       break;
     case TR_AR:
       if (!comp->getOption(TR_DisableOOL) &&
           (self()->cg()->isOutOfLineColdPath() || self()->cg()->isOutOfLineHotPath()) &&
           virtReg->getBackingStorage())
         {
         location = virtReg->getBackingStorage();
         // reuse the spill slot
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nOOL: Reuse backing store (%p) for %s inside OOL\n",
                                         location,debugObj->getName(virtReg));
         }
       else if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
         {
         location = self()->cg()->allocateSpill(4, false, NULL, true);
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling AR %s to (%p)\n", debugObj->getName(virtReg),location);
         }
       opCode = TR::InstOpCode::LAM;
       break;
     case TR_VRF:
       // Spill of size 16 has never been done before. The call hierarchy seems to support it but this should be watched closely.
       if((!comp->getOption(TR_EnableTrueRegisterModel)) || location==NULL)
         {
         location = self()->cg()->allocateSpill(16, false, NULL, true);
         if (debugObj)
           self()->cg()->traceRegisterAssignment("\nSpilling VRF %s to (%p)\n", debugObj->getName(virtReg), location);
         }
       opCode = TR::InstOpCode::VL;
       break;
     }

   TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), self()->cg());
   location->getSymbolReference()->getSymbol()->setSpillTempLoaded();
   virtReg->setBackingStorage(location);
   if (enableHighWordRA && alreadySpilledToHPRCollectible)
     {
     // load compressed refs low word into highword
     TR::MemoryReference * mr = generateS390MemoryReference(*tempMR, 4, self()->cg());
     cursor = generateRXYInstruction(self()->cg(), TR::InstOpCode::LFH, currentNode, best, mr, currentInstruction);
     virtReg->setSpilledToHPR(false);
     }
   else
     {
     if (opCode == TR::InstOpCode::LAM)
       cursor = generateRSInstruction(self()->cg(), opCode, currentNode, best, best, tempMR, currentInstruction);
     else if (opCode == TR::InstOpCode::VL)
       cursor = generateVRXInstruction(self()->cg(), opCode, currentNode, best, tempMR, 0, currentInstruction);
     else
       cursor = generateRXInstruction(self()->cg(), opCode, currentNode, best, tempMR, currentInstruction);
     }
   }

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

   if (!comp->getOption(TR_DisableOOL) && location != NULL)
      {
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
      }
   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);
   if (enableHighWordRA && virtReg->is64BitReg())
      {
      best->getHighWordRegister()->setAssignedRegister(NULL);
      best->getHighWordRegister()->setState(TR::RealRegister::Free);
      }
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
   // *this    swipeable for debugging purposes
   TR_BackingStore * location = spilledRegister->getBackingStorage();
   TR::Node * currentNode = currentInstruction->getNode();
   TR_RegisterKinds rk = spilledRegister->getKind();

   TR::InstOpCode::Mnemonic opCode;
   int32_t dataSize;
   TR::Instruction * cursor = NULL;
   TR::RealRegister * freeHighWordReg = NULL;
   TR_Debug * debugObj = self()->cg()->getDebug();
   TR::Compilation *comp = TR::comp();
   //This may not actually need to be reversed if
   //this is a dummy register used for OOL dependencies

   _cg->traceRegisterAssignment("REVERSE SPILL STATE FOR %R", spilledRegister);

   spilledRegister->resetValueLiveOnExit();
   spilledRegister->resetPendingSpillOnDef();

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;

   if (spilledRegister->isPlaceholderReg())
      {
      return NULL;
      }

#ifdef DEBUG
   if (rk == TR_GPR && (debug("traceMsg90GPR") || debug("traceGPRStats")))
      {
      self()->cg()->incTotalSpills();
      }
#endif

   if (enableHighWordRA)
      {
      //TR_ASSERTC( !location,_cg->comp(), "\nHW RA: reg spilled to both HW and stack??\n");
      freeHighWordReg = self()->findVirtRegInHighWordRegister(spilledRegister);
      if (freeHighWordReg)
         _cg->traceRegisterAssignment("reverseSpillState: found GPR spilled to %R", freeHighWordReg);
      }

   // no real reg is assigned to targetRegister yet
   if (targetRegister == NULL)
      {
      // find a free register and assign
      targetRegister = self()->findBestFreeRegister(currentInstruction, spilledRegister->getKind(), spilledRegister);
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestRegister(currentInstruction, spilledRegister, spilledRegister->getKind());
         }
      }

   if (_cg->isOutOfLineColdPath())
      {
      // the future and total use count might not always reflect register spill state
      // for example a new register assignment in the hot path would cause FC != TC
      // in this case, assign a new register and return
      if (!freeHighWordReg)
         {
      if (!location)
         {
         if (debugObj)
            self()->cg()->traceRegisterAssignment("OOL: Not generating reverse spill for (%s)\n",
                                          debugObj->getName(spilledRegister));
         if (enableHighWordRA && spilledRegister->is64BitReg())
            {
            targetRegister->getHighWordRegister()->setState(TR::RealRegister::Assigned);
            targetRegister->getHighWordRegister()->setAssignedRegister(spilledRegister);
            }
         return targetRegister;
         }
         }
      }

   if (location == NULL && freeHighWordReg == NULL)
      {
      if (rk == TR_GPR)
         {
         if (enableHighWordRA && spilledRegister->is64BitReg())
            location = self()->cg()->allocateSpill(8, spilledRegister->containsCollectedReference(), NULL, true);
         else
            location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), spilledRegister->containsCollectedReference(), NULL, true);
         }
      else if (rk == TR_GPR64)
         {
         location = self()->cg()->allocateSpill(8, spilledRegister->containsCollectedReference(), NULL, true);
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

   if (comp->getOption(TR_Enable390FreeVMThreadReg) && spilledRegister == self()->cg()->getVMThreadRegister())
      {
      traceMsg(comp, "\ns390machine: setVMThreadSpillInstruction\n");
      _cg->setVMThreadSpillInstruction(currentInstruction);
      }
   else
      {
      targetRegister->setState(TR::RealRegister::Assigned);
      targetRegister->setAssignedRegister(spilledRegister);
      spilledRegister->setAssignedRegister(targetRegister);

      if (enableHighWordRA && spilledRegister->is64BitReg())
         {
         targetRegister->getHighWordRegister()->setState(TR::RealRegister::Assigned);
         targetRegister->getHighWordRegister()->setAssignedRegister(spilledRegister);
         }

      // the register was spilled to a HW reg
      if (freeHighWordReg)
         {
         if (targetRegister->isHighWordRegister())
            {
            cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHHR, freeHighWordReg, targetRegister, 0, currentInstruction);
            }
         else
            {
            if (spilledRegister->containsCollectedReference())
               {
               uint32_t compressShift = TR::Compiler->om.compressedReferenceShift();
               // need to assert heapbase, offset = 0
               if (compressShift == 0)
                  {
                  cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHLR, freeHighWordReg, targetRegister, 0, currentInstruction);
                  }
               else
                  {
                  cursor = generateRIEInstruction(_cg, TR::InstOpCode::RISBHG, currentNode, freeHighWordReg, targetRegister, 0, 31+0x80, 32-compressShift, currentInstruction);
                  }
               }
            else
               {
               cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHLR, freeHighWordReg, targetRegister, 0, currentInstruction);
               }
            }
         if (debugObj)
            {
            debugObj->addInstructionComment(cursor, "Reverse spill Highword");
            }
         self()->cg()->traceRAInstruction(cursor);
         spilledRegister->setSpilledToHPR(false);
         freeHighWordReg->setAssignedRegister(NULL);
         freeHighWordReg->setState(TR::RealRegister::Free);
         return targetRegister;
         }

      TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), self()->cg());

      bool needMVHI = false;

      switch (rk)
         {
         case TR_GPR:
            dataSize = TR::Compiler->om.sizeofReferenceAddress();
            opCode = TR::InstOpCode::getStoreOpCode();
            if (comp->getOption(TR_ForceLargeRAMoves))
               {
               dataSize = 8;
               opCode = TR::InstOpCode::STG;
               }
            if (enableHighWordRA)
               {
               if (spilledRegister->assignToHPR() || targetRegister->isHighWordRegister())
                  {
                  //dataSize = 4;
                  opCode = TR::InstOpCode::STFH;

                  if (spilledRegister->containsCollectedReference())
                     {
                     // decompressing: store into the lower bytes in memory
                     // need to zero out the higher bytes later too
                     needMVHI = true;
                     }
                  else
                     {
                     TR_ASSERT(!spilledRegister->is64BitReg(), "ReverseSpill: HPR cannot be 64 bit!\n");
                     }
                  }
               if (spilledRegister->assignToGPR() || targetRegister->isLowWordRegister())
                  {
                  // dont want to involve halfslot spills yet
                  //dataSize = 4;
                  opCode = TR::InstOpCode::ST;
                  }
               if (spilledRegister->is64BitReg())
                  {
                  dataSize = 8;
                  opCode = TR::InstOpCode::STG;
                  }
               }
            break;
         case TR_GPR64:
            dataSize = 8;
            opCode = TR::InstOpCode::STG;
            break;
         case TR_FPR:
            dataSize = 8;
            opCode = TR::InstOpCode::STD;
            break;
         case TR_AR:
            dataSize = 4;
            opCode = TR::InstOpCode::STAM;
            break;
         case TR_VRF:
            dataSize = 16;
            opCode = TR::InstOpCode::VST;
            break;
         }

      if(true) // Check to see if we should free spill location
        {
        if (comp->getOption(TR_DisableOOL))
          {
          self()->cg()->freeSpill(location, dataSize, 0);
          }
        else
          {
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
          }
        } // Need to free the spill location

      if (needMVHI)
         {
         cursor = generateSILInstruction(self()->cg(), TR::InstOpCode::MVHI, currentNode, tempMR, 0, currentInstruction);
         self()->cg()->traceRAInstruction(cursor);
         cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::STFH, currentNode, targetRegister, generateS390MemoryReference(*tempMR, 4, self()->cg()), cursor);
         self()->cg()->traceRAInstruction(cursor);
         spilledRegister->setSpilledToHPR(true);
         }
      else
         {
         if (opCode == TR::InstOpCode::STAM)
            cursor = generateRSInstruction(self()->cg(), opCode, currentNode, targetRegister, targetRegister, tempMR, currentInstruction);
         else if (opCode == TR::InstOpCode::VST)
            cursor = generateVRXInstruction(self()->cg(), opCode, currentNode, targetRegister, tempMR, 0, currentInstruction);
         else
            cursor = generateRXInstruction(self()->cg(), opCode, currentNode, targetRegister, tempMR, currentInstruction);

         self()->cg()->traceRAInstruction(cursor);
         }

      if (debugObj)
         {
         debugObj->addInstructionComment(cursor, "Spill");
         }
      if ( !cursor->assignFreeRegBitVector() )
         {
         cursor->assignBestSpillRegister();
         }
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
   if (virtReg->isUsedInMemRef() && realReg == _registerFile[TR::RealRegister::GPR0])
      {
      return false;
      }
   else
      {
      if (_cg->supportsHighWordFacility() && !TR::comp()->getOption(TR_DisableHighWordRA) &&
          virtReg->getKind() != TR_FPR && virtReg->getKind() != TR_VRF)
         {
         if ((virtReg->is64BitReg() && realReg->getLowWordRegister()->getAssignedRegister() == realReg->getHighWordRegister()->getAssignedRegister()) ||
             (!virtReg->is64BitReg() && realReg->getLowWordRegister()->getAssignedRegister() != realReg->getHighWordRegister()->getAssignedRegister()))
            {
            return true;
            }
         else
            {
            return false;
            }
         }
      return true;
      }
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
                                         TR::RealRegister::RegNum  registerNumber,
                                         flags32_t                                  instFlags)
   {
   // *this    swipeable for debugging purposes
   TR::RealRegister * targetRegister = _registerFile[registerNumber];
   TR::RealRegister * realReg = virtualRegister->getAssignedRealRegister();
   if(virtualRegister->isArGprPair()) realReg = virtualRegister->getGPRofArGprPair()->getAssignedRealRegister();
   TR::RealRegister * currentAssignedRegister = (realReg == NULL) ? NULL : toRealRegister(realReg);
   TR::RealRegister * spareReg = NULL;
   TR::Register * currentTargetVirtual = NULL;
   TR_RegisterKinds rk = virtualRegister->getKind();

   TR_RegisterKinds currentAssignedRegisterRK = rk; // used for register Copy, todo: use it for freeBestReg and findFreeReg
   TR_RegisterKinds currentTargetVirtualRK = rk;
   TR::Instruction * cursor = NULL;
   TR::Node * currentNode = currentInstruction->getNode();
   bool doNotRegCopy = false;
   TR::Compilation *comp = TR::comp();

   if(virtualRegister->isArGprPair())
     virtualRegister->getGPRofArGprPair()->setIsLive();
   else
     virtualRegister->setIsLive();

   if (!_cg->getRAPassAR() && (rk == TR_AR || targetRegister->getKind() == TR_AR))
      TR_ASSERT(false, "Should only process AR dependencies in AR register allocator pass");

   bool enableHighWordRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                           rk != TR_FPR && rk != TR_VRF;
   uint32_t availHighWordRegMap;
   if (enableHighWordRA)
      {
      availHighWordRegMap = ~(TR::RealRegister::getBitMask(toRealRegister(targetRegister)->getHighWordRegister()->getRegisterNumber()));
      }
    _cg->traceRegisterAssignment("COERCE %R into %R", virtualRegister, targetRegister);

   // If either the virtual we are coercing or the assigned reg we are
   //
   //
   if (targetRegister->getAssignedRegister() && targetRegister->getAssignedRegister()->getKind() == TR_GPR64)
      {
      rk = TR_GPR64;
      currentTargetVirtualRK = TR_GPR64;
      currentAssignedRegisterRK = TR_GPR64;
      }

   if (enableHighWordRA && currentAssignedRegister)
      {
      if (currentAssignedRegister->isHighWordRegister())
         currentAssignedRegisterRK = TR_HPR;
      else
         {
         if (!virtualRegister->is64BitReg())
            currentAssignedRegisterRK = TR_GPRL;
         else
            currentAssignedRegisterRK = TR_GPR64;
         }
      }

   // in addition to the 4 GPR's involved, we have 4 more registers for high words:
   //                      REAL                    VIRTUAL
   // Source       currentTargetVirtualHW <-> virtualRegisterHW
   // Target       targetRegisterHW       <-> currentTargetVirtualHW

   TR::RealRegister * targetRegisterHW = NULL;
   TR::Register * currentTargetVirtualHW = NULL;
   TR::RealRegister * currentAssignedRegisterHW = NULL;
   TR::Register * virtualRegisterHW = NULL;
   TR::RealRegister * spareRegHW = NULL;

   if (enableHighWordRA)
      {
      targetRegisterHW = targetRegister->getHighWordRegister();
      currentTargetVirtualHW = targetRegisterHW->getAssignedRegister();

      if (currentAssignedRegister != NULL)
         {
         currentAssignedRegisterHW = currentAssignedRegister->getHighWordRegister();
         virtualRegisterHW = currentAssignedRegisterHW->getAssignedRegister();
         }

      if (virtualRegister->is64BitReg())
         {
         _cg->traceRegisterAssignment(" HW RA coerceRA: %R needs 64 bit reg ", virtualRegister);
         }
      else
         {
         _cg->traceRegisterAssignment(" HW RA coerceRA: %R needs 32 bit reg ", virtualRegister);
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
      if(virtualRegister->isPlaceholderReg())
        targetRegister->setIsAssignedMoreThanOnce(); // Register is killed invalidate it for moving spill out of loop
      _cg->traceRegisterAssignment("target %R is free", targetRegister);
      if (enableHighWordRA && virtualRegister->is64BitReg())
         {
         if (targetRegister->isHighWordRegister() && !virtualRegister->isPlaceholderReg())
            {
            // this could happen for OOL, when the reg deps on the top of slow path dictates that a collectible register must be
            // spilled to a specific HPR
            TR_ASSERT( virtualRegister->containsCollectedReference(), " OOL HPR spill: spilling a 64 bit scalar into HPR");

            TR::RealRegister  * currentHighWordReg = self()->findVirtRegInHighWordRegister(virtualRegister);

            if (virtualRegister->getAssignedRegister() == NULL && currentHighWordReg)
               {
               // already spilled to HPR, so simply move
               cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHHR, currentHighWordReg, targetRegister, 0, currentInstruction);
               _cg->traceRAInstruction(cursor);

               //fix up states
               currentHighWordReg->setAssignedRegister(NULL);
               currentHighWordReg->setState(TR::RealRegister::Free);
               }
            else
               {
               if (currentAssignedRegister == NULL)
                  {
                  if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount() &&
                      virtualRegister->getBackingStorage() != NULL)
                     {
                     // the virtual register is currently spilled to stack, now we need to spill it onto HPR
                     // load it back from the stack into HPR with STFH
                     // since we are working with compressed refs shift = 0, simply load 32-bit value into HPR.
                     TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, virtualRegister->getBackingStorage()->getSymbolReference(), _cg);

                     // is the offset correct?  +4 big endian?
                     TR::MemoryReference * mr = generateS390MemoryReference(*tempMR, 4, _cg);

                     cursor = generateSILInstruction(self()->cg(), TR::InstOpCode::MVHI, currentNode, tempMR, 0, currentInstruction);
                     _cg->traceRAInstruction(cursor);
                     cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::STFH, currentNode, targetRegister, mr, cursor);
                     _cg->traceRAInstruction(cursor);

                     // fix up states
                     // don't need to worry about protecting backing storage because we are leaving cold path OOL now
                     _cg->freeSpill(virtualRegister->getBackingStorage(), 8, 0);
                     virtualRegister->setBackingStorage(NULL);
                     virtualRegister->setSpilledToHPR(true);
                     }
                  else
                     {
                     TR_ASSERT(comp, " OOL HPR spill: currentAssignedRegister is NULL but virtual reg is not spilled?");
                     }
                  }
               else
                  {
                  // the virtual register is currently assigned to a 64 bit real reg
                  // simply spill it to HPR and decompress
                  TR_ASSERT(currentAssignedRegister->isLowWordRegister(), " OOL HPR spill: 64-bit reg assigned to HPR and is not spilled to HPR");
                  cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LLHFR, currentAssignedRegister, targetRegister, 0, currentInstruction);
                  _cg->traceRAInstruction(cursor);
                  cursor = generateRILInstruction(_cg, TR::InstOpCode::IIHF, currentNode, currentAssignedRegister, (uintptrj_t)0, cursor);
                  _cg->traceRAInstruction(cursor);

                  // fix up states
                  currentAssignedRegister->setAssignedRegister(NULL);
                  currentAssignedRegister->setState(TR::RealRegister::Free);
                  currentAssignedRegisterHW->setAssignedRegister(NULL);
                  currentAssignedRegisterHW->setState(TR::RealRegister::Free);
                  }
               }
            // fix up the states
            virtualRegister->setAssignedRegister(NULL);
            virtualRegister->setSpilledToHPR(true);
            targetRegister->setAssignedRegister(virtualRegister);
            return cursor;
            }

         if (targetRegisterHW->getState() != TR::RealRegister::Free && targetRegisterHW->getState() != TR::RealRegister::Unlatched)
            {
            virtualRegister->block();
            targetRegister->setState(TR::RealRegister::Blocked);
            // free up this highword register by either spilling it or moving it
            cursor = self()->freeHighWordRegister(currentInstruction, targetRegisterHW, instFlags);
            virtualRegister->unblock();
            targetRegister->setState(TR::RealRegister::Free);
            }
         }
      // the virtual register haven't be assigned to any real register yet
      if (currentAssignedRegister == NULL)
         {
         // Vector registers coercion will most likely take this path

         // get the value of virtual register back from spill state if not first use
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            _cg->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (!comp->getOption(TR_DisableOOL) && self()->cg()->isOutOfLineColdPath())
               {
               _cg->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
               }
            }
         }
      else
         {
         // virtual register is currently assigned to a different register,
         // override it with the target reg

         // todo: HW copy, LW copy or 64bit?
         // if targetReg is HW, copy need to be HW
         cursor = self()->registerCopy(currentInstruction, currentAssignedRegisterRK, currentAssignedRegister, targetRegister, self()->cg(), instFlags);
         if (enableHighWordRA && virtualRegister->is64BitReg())
            {
            currentAssignedRegisterHW->setState(TR::RealRegister::Free);
            currentAssignedRegisterHW->setAssignedRegister(NULL);
            }
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      }
   // the target reg is blocked so we need to guarantee it stays in a reg
   else if (targetRegister->getState() == TR::RealRegister::Blocked)
      {
      currentTargetVirtual = targetRegister->getAssignedRegister();
      _cg->traceRegisterAssignment("target %R is blocked, assigned to %R", targetRegister, currentTargetVirtual);

      if (enableHighWordRA && currentTargetVirtual)
         {
         if (targetRegister->isHighWordRegister() &&
               targetRegister->getLowWordRegister()->getAssignedRegister() != currentTargetVirtual)
            {
            currentTargetVirtualRK = TR_HPR;
            }
         else
            {
         if (!currentTargetVirtual->is64BitReg())
            currentTargetVirtualRK = TR_GPRL;
         else
            currentTargetVirtualRK = TR_GPR64;
            }
         }

      // in this case we cannot use the HPR of target reg as spareReg
      if (enableHighWordRA && virtualRegister->is64BitReg())
         spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual, availHighWordRegMap);
      else
         spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual);
      _cg->setRegisterAssignmentFlag(TR_IndirectCoercion);

      // We may need spare reg no matter what
      if ( spareReg == NULL )
         {
         _cg->setRegisterAssignmentFlag(TR_RegisterSpilled);
         virtualRegister->block();
         currentTargetVirtual->block();

         if (enableHighWordRA && virtualRegister->is64BitReg())
            {
            // if we end up spilling currentTargetVirtual to HPR, make sure to not pick the HPR of targetRegister since it will not
            // free up the full 64 bit register
            spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), availHighWordRegMap, true, true);
            }
         else
            {
            // we can allow freeBestRegister() to return NULL here, as long as we can perform register exchange later via memory
            // and take a bad OSC penalty
            // todo: fix HW RA to enable register exchange later
            spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), 0xffff, true);
            }
         virtualRegister->unblock();
         currentTargetVirtual->unblock();
         }

      // if currentTargetVirtual is low word only, spare Reg will be low word
      // but virtual Reg could require full size, in this case, need to free the HW of target Reg
      if (enableHighWordRA && virtualRegister->is64BitReg())
         {

         // There is an extra check at the end to avoid the scenario in which virtualRegister is assigned to
         // HPRx and we are attempting to coerce it to GPRx. In this case we do not need to spill HPRx, rather just copy it
         if (targetRegisterHW->getState() != TR::RealRegister::Free &&
             targetRegisterHW->getState() != TR::RealRegister::Unlatched &&
             targetRegisterHW->getAssignedRegister() != virtualRegister)
            {
            if (targetRegisterHW->getAssignedRegister() &&
                targetRegisterHW->getAssignedRegister() != targetRegister->getAssignedRegister())
               {
               //TR_ASSERTC( currentTargetVirtual->isLowWordOnly(),_cg->comp(), "currentTargetVirtual is not LWOnly but HW is clobbered by another vreg?");
               virtualRegister->block();
               currentTargetVirtual->block();
               spareReg->block(); //to do: is this necessary? only need to block HPR?
               // free up this highword register by either spilling it or moving it
               cursor = self()->freeHighWordRegister(currentInstruction, targetRegisterHW, instFlags);
               spareReg->unblock();
               currentTargetVirtual->unblock();
               virtualRegister->unblock();
               }
            }
         }
      // find a free register if the virtual register hasn't been assigned to any real register
      // or it is a FPR for later use
      // virtual register is currently assigned to a different register,
      if (currentAssignedRegister != NULL)
         {

         // todo :HW fix
         if (!self()->isAssignable(currentTargetVirtual, currentAssignedRegister))
            {
            if (enableHighWordRA && spareReg == NULL && currentTargetVirtualRK != currentAssignedRegisterRK)
               {
               // register kinds mismatch and we do not have a free spare reg, take the OSC penalty...
               TR_BackingStore * location;

               location = self()->cg()->allocateSpill(8, false, NULL, true);      // No chance of a gcpoint
               TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, location->getSymbolReference(), self()->cg());

               // swapping currentAssignedRegister <-> targetRegister
               //          virtualRegister         <-> currentTargetVirtual

               cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::LG, currentNode, currentAssignedRegister, tempMR, currentInstruction);
               self()->cg()->traceRAInstruction(cursor);
               cursor = generateRRInstruction(self()->cg(), TR::InstOpCode::LGR, currentNode, targetRegister, currentAssignedRegister, currentInstruction);
               self()->cg()->traceRAInstruction(cursor);
               TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, 0, self()->cg());
               cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::STG, currentNode, targetRegister, tempMR2, currentInstruction);
               self()->cg()->traceRAInstruction(cursor);

               self()->cg()->freeSpill(location, 8, 0);

               // fix up the low word states
               currentAssignedRegister->setState(TR::RealRegister::Blocked);
               currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(currentAssignedRegister);

               // fix up the high word states
               // store old states in a temp
               TR::RealRegister::RegState tempState = currentAssignedRegisterHW->getState();
               TR::Register * tempAssignedReg = currentAssignedRegisterHW->getAssignedRegister();

               currentAssignedRegisterHW->setState(targetRegisterHW->getState());
               currentAssignedRegisterHW->setAssignedRegister(targetRegisterHW->getAssignedRegister());

               // If the target register contained a 64-bit value then the Low-Word and High-Word real registers
               // would point to the same virtual register. As such the following is true:
               //
               // currentTargetVirtual == targetRegisterHW->getAssignedRegister()
               //
               // hence the statement in the if block below would set currentTargetVirtual to point to the High-Word
               // of the currentAssignedRegister which is not what we want (since it is a 64-bit value).

               if (targetRegisterHW->getAssignedRegister() && !targetRegisterHW->getAssignedRegister()->is64BitReg())
                  {
                  targetRegisterHW->getAssignedRegister()->setAssignedRegister(currentAssignedRegisterHW);
                  }

               if (!virtualRegister->is64BitReg())
                  {
                  targetRegisterHW->setState(tempState);
                  targetRegisterHW->setAssignedRegister(tempAssignedReg);
                  if (tempAssignedReg)
                     {
                     tempAssignedReg->setAssignedRegister(targetRegisterHW);
                     }
                  }
               }
            else
               {
               TR_ASSERT(spareReg!=NULL, "coerce reg - blocked, sparereg cannot be NULL.");
               _cg->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(currentInstruction, currentTargetVirtualRK, targetRegister, spareReg, self()->cg(), instFlags);
               cursor = self()->registerCopy(currentInstruction, currentAssignedRegisterRK, currentAssignedRegister, targetRegister, self()->cg(), instFlags);

               spareReg->setState(TR::RealRegister::Assigned);
               currentTargetVirtual->setAssignedRegister(spareReg);
               spareReg->setAssignedRegister(currentTargetVirtual);

               if (enableHighWordRA && currentTargetVirtual->is64BitReg())
                  {
                  targetRegister->getLowWordRegister()->setState(TR::RealRegister::Unlatched);
                  targetRegister->getLowWordRegister()->setAssignedRegister(NULL);
                  targetRegisterHW->setState(TR::RealRegister::Unlatched);
                  targetRegisterHW->setAssignedRegister(NULL);

                  spareReg->getHighWordRegister()->setState(TR::RealRegister::Assigned); //no need to block the HW half, we already blocked the Lw
                  spareReg->getHighWordRegister()->setAssignedRegister(currentTargetVirtual);
                  }
               currentAssignedRegister->setState(TR::RealRegister::Unlatched);
               currentAssignedRegister->setAssignedRegister(NULL);
               if (enableHighWordRA && virtualRegister->is64BitReg())
                  {
                  currentAssignedRegisterHW->setState(TR::RealRegister::Unlatched);
                  currentAssignedRegisterHW->setAssignedRegister(NULL);
                  }
               }
            }
         else
            {
            _cg->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
            if (enableHighWordRA)
               {
               cursor = self()->registerExchange(currentInstruction, currentAssignedRegisterRK, targetRegister, currentAssignedRegister, spareReg, self()->cg(), instFlags);
               if (currentTargetVirtual->is64BitReg())
                  {
                  //currentAssignedRegister->getHighWordRegister()->setState(TR::RealRegister::Blocked);  //not necessary
                  currentAssignedRegister->getHighWordRegister()->setAssignedRegister(currentTargetVirtual);
                  }
               }
            else
               {
               // Vector coercion most likely to take this path.
            cursor = self()->registerExchange(currentInstruction, rk, targetRegister, currentAssignedRegister, spareReg, self()->cg(), instFlags);
               }
            currentAssignedRegister->setState(TR::RealRegister::Blocked);
            currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(currentAssignedRegister);

            }
         }
      else
         {
         _cg->traceRegAssigned(currentTargetVirtual, spareReg);

         // virtual register is not assigned yet, copy register
         cursor = self()->registerCopy(currentInstruction, currentTargetVirtualRK, targetRegister, spareReg, self()->cg(), instFlags);

         spareReg->setState(TR::RealRegister::Assigned);
         spareReg->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(spareReg);

         if (enableHighWordRA && currentTargetVirtual->is64BitReg())
            {
            //TR_ASSERT(targetRegisterHW->getState() == TR::RealRegister::Blocked,
            //        "currentTargetVirtual is blocked and is fullsize, but the HW is not blocked?");

            spareReg->getHighWordRegister()->setState(TR::RealRegister::Assigned);
            spareReg->getHighWordRegister()->setAssignedRegister(currentTargetVirtual);
            targetRegister->getLowWordRegister()->setState(TR::RealRegister::Unlatched);
            targetRegister->getLowWordRegister()->setAssignedRegister(NULL);
            targetRegisterHW->setState(TR::RealRegister::Unlatched);
            targetRegisterHW->setAssignedRegister(NULL);
            }
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            _cg->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (!comp->getOption(TR_DisableOOL) && self()->cg()->isOutOfLineColdPath())
               {
               _cg->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
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
      _cg->traceRegisterAssignment("target %R is assigned, assigned to %R", targetRegister, currentTargetVirtual);

      if (enableHighWordRA && currentTargetVirtual)
         {
         // this happens for OOL HPR spill, simply return
         if (currentTargetVirtual == virtualRegister)
            {
            virtualRegister->setAssignedRegister(targetRegister);
            _cg->traceRegAssigned(virtualRegister, targetRegister);
            _cg->clearRegisterAssignmentFlags();
            return cursor;
            }
         if (targetRegister->isHighWordRegister() &&
               targetRegister->getLowWordRegister()->getAssignedRegister() != currentTargetVirtual)
            {
            currentTargetVirtualRK = TR_HPR;
            }
         else
            {
         if (!currentTargetVirtual->is64BitReg())
            currentTargetVirtualRK = TR_GPRL;
         else
            currentTargetVirtualRK = TR_GPR64;
            }
         }

      // If the target HPR was a spill slot
      if (enableHighWordRA &&
          currentTargetVirtual->getAssignedRegister() == NULL &&
          targetRegister->isHighWordRegister())
         {
         _cg->traceRegisterAssignment(" HW RA %R was spilled to %R, now need to spill again", currentTargetVirtual, targetRegister);

         // free this spill slot up first
         // block virtual reg?
         cursor = self()->freeHighWordRegister(currentInstruction, targetRegister, instFlags);

         if (virtualRegister->is64BitReg() && !virtualRegister->isPlaceholderReg())
            {
            // this could happen for OOL, when the reg deps on the top of slow path dictates that a collectible register must be
            // spilled to a specific HPR
            TR_ASSERT( virtualRegister->containsCollectedReference(), " OOL HPR spill: spilling a 64 bit scalar into HPR");

            TR::RealRegister  * currentHighWordReg = self()->findVirtRegInHighWordRegister(virtualRegister);

            if (virtualRegister->getAssignedRegister() == NULL && currentHighWordReg)
               {
               // already spilled to HPR, so simply move
               cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHHR, currentHighWordReg, targetRegister, 0, currentInstruction);
               _cg->traceRAInstruction(cursor);

               //fix up states
               currentHighWordReg->setAssignedRegister(NULL);
               currentHighWordReg->setState(TR::RealRegister::Free);
               }
            else
               {
               if (currentAssignedRegister == NULL)
                  {
                  if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount() &&
                      virtualRegister->getBackingStorage() != NULL)
                     {
                     // the virtual register is currently spilled to stack, now we need to spill it onto HPR
                     // load it back from the stack into HPR with STFH
                     // since we are working with compressed refs shift = 0, simply load 32-bit value into HPR.
                     TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, virtualRegister->getBackingStorage()->getSymbolReference(), _cg);

                     // is the offset correct?  +4 big endian?
                     TR::MemoryReference * mr = generateS390MemoryReference(*tempMR, 4, _cg);

                     cursor = generateSILInstruction(self()->cg(), TR::InstOpCode::MVHI, currentNode, tempMR, 0, currentInstruction);
                     _cg->traceRAInstruction(cursor);
                     cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::STFH, currentNode, targetRegister, mr, cursor);
                     _cg->traceRAInstruction(cursor);

                     // fix up states
                     // don't need to worry about protecting backing storage because we are leaving cold path OOL now
                     _cg->freeSpill(virtualRegister->getBackingStorage(), 8, 0);
                     virtualRegister->setBackingStorage(NULL);
                     virtualRegister->setSpilledToHPR(true);
                     }
                  else
                     {
                     TR_ASSERT(comp, " OOL HPR spill: currentAssignedRegister is NULL but virtual reg is not spilled?");
                     }
                  }
               else
                  {
                  // the virtual register is currently assigned to a 64 bit real reg
                  // simply spill it to HPR and decompress
                  TR_ASSERT(currentAssignedRegister->isLowWordRegister(), " OOL HPR spill: 64-bit reg assigned to HPR and is not spilled to HPR");
                  cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LLHFR, currentAssignedRegister, targetRegister, 0, currentInstruction);
                  _cg->traceRAInstruction(cursor);
                  cursor = generateRILInstruction(_cg, TR::InstOpCode::IIHF, currentNode, currentAssignedRegister, (uintptrj_t)0, cursor);
                  _cg->traceRAInstruction(cursor);

                  // fix up states
                  currentAssignedRegister->setAssignedRegister(NULL);
                  currentAssignedRegister->setState(TR::RealRegister::Free);
                  currentAssignedRegisterHW->setAssignedRegister(NULL);
                  currentAssignedRegisterHW->setState(TR::RealRegister::Free);
                  }
               }
            // fix up the states
            virtualRegister->setAssignedRegister(NULL);
            virtualRegister->setSpilledToHPR(true);
            targetRegister->setAssignedRegister(virtualRegister);
            }
         else
            {
            // we need to either assign or spill a 32-bit virtual register into a HPR
            // the target HPR is now free
            if (currentAssignedRegister == NULL)
               {
               // the 32-bit virtual register is spilled
               TR::RealRegister  * currentHighWordReg = self()->findVirtRegInHighWordRegister(virtualRegister);
               if (currentHighWordReg)
                  {
                  // if it is spilled to HPR, simply move it
                  cursor = generateExtendedHighWordInstruction(currentNode, _cg, TR::InstOpCode::LHHR, currentHighWordReg, targetRegister, 0, currentInstruction);
                  _cg->traceRAInstruction(cursor);

                  //fix up states
                  currentHighWordReg->setState(TR::RealRegister::Free);
                  currentHighWordReg->setAssignedRegister(NULL);
                  virtualRegister->setSpilledToHPR(false);
                  }
               else if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
                  {
                  // if it is spilled to stack, load it back into HPR
                  TR_ASSERT(virtualRegister->getBackingStorage(), " OOL HPR spill: virtual reg is not spilled to stack nor HPR");
                  TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, virtualRegister->getBackingStorage()->getSymbolReference(), _cg);

                  TR::MemoryReference * mr = generateS390MemoryReference(*tempMR, 4, self()->cg());

                  cursor = generateSILInstruction(self()->cg(), TR::InstOpCode::MVHI, currentNode, tempMR, 0, currentInstruction);
                  _cg->traceRAInstruction(cursor);
                  cursor = generateRXInstruction(self()->cg(), TR::InstOpCode::STFH, currentNode, targetRegister, mr, cursor);
                  _cg->traceRAInstruction(cursor);

                  // fix up states
                  // don't need to worry about protecting backing storage because we are leaving cold path OOL now
                  _cg->freeSpill(virtualRegister->getBackingStorage(), 8, 0);
                  virtualRegister->setBackingStorage(NULL);
                  virtualRegister->setSpilledToHPR(true);
                  }
               // and do nothing in case virtual reg is a placeholder
               }
            else
               {
               // the 32-bit virtual register is currently assigned to a real register
               // do register copy
               cursor = self()->registerCopy(currentInstruction, currentAssignedRegisterRK, currentAssignedRegister, targetRegister, self()->cg(), instFlags);
               currentAssignedRegister->setState(TR::RealRegister::Free);
               currentAssignedRegister->setAssignedRegister(NULL);
               }

            // fix up states
            virtualRegister->setAssignedRegister(targetRegister);
            targetRegister->setState(TR::RealRegister::Assigned);
            targetRegister->setAssignedRegister(virtualRegister);
            }
         return cursor;
         }
      else
         {
         // in this case we cannot use the HPR of target reg as spareReg
         if (enableHighWordRA && virtualRegister->is64BitReg())
            spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual, availHighWordRegMap);
         else
            // Look for a free reg in case we need a spare.
            spareReg = self()->findBestFreeRegister(currentInstruction, rk, currentTargetVirtual);
         }
      _cg->setRegisterAssignmentFlag(TR_IndirectCoercion);

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
         //  reg exchange with HW not implemented yet
         if (!self()->isAssignable(currentTargetVirtual, currentAssignedRegister)
             || enableHighWordRA
             )
            {
            // There is an alternative to blindly spilling because:
            //   1. there was a FREE reg
            //   2. freeBestReg found a better choice to be spilled
            if (spareReg == NULL)
               {
               _cg->setRegisterAssignmentFlag(TR_RegisterSpilled);

               //  The current source reg's assignment is automatically blocked out
               virtualRegister->block();
               if (enableHighWordRA && virtualRegister->is64BitReg())
                  {
                  // if we end up spilling currentTargetVirtual to HPR, make sure to not pick the HPR of targetRegister since it will not
                  // free up the full 64 bit register
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), availHighWordRegMap, false, true);
                  }
               else
                  {
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), availHighWordRegMap, true);
                  }

               // For some reason (blocked/locked regs etc), we couldn't find a spare reg so spill the virtual in the target and use it for coercion
               if (spareReg == NULL)
               {
               self()->spillRegister(currentInstruction, currentTargetVirtual);
               targetRegister->setAssignedRegister(virtualRegister);
               virtualRegister->setAssignedRegister(targetRegister);
               targetRegister->setState(TR::RealRegister::Assigned);
               }

               // freeBestRegister could spill currentTargetVirtual directly to free up targetRegister.
               // If currentTargetVirtual is 64-bit, spareReg will be a GPR (targetRegister's low word)
               // however, targetRegister could be an HPR because we might only need its high word. In this case
               // we must not generate register move of LGR targetRegister,spareReg because they will be the same register
               // and we must leave currentTargetVirtual spilled instead of assigning it to spareReg.
               if (targetRegister->isHighWordRegister() && currentTargetVirtual->is64BitReg() &&
                   targetRegister->getRegisterNumber() == spareReg->getHighWordRegister()->getRegisterNumber())
                  doNotRegCopy = true;
               virtualRegister->unblock();
               }

            // if currentTargetVirtual is low word only, spare Reg will be low word
            // but virtual Reg could require full size, in this case, need to free the HW of target Reg
            if (enableHighWordRA && virtualRegister->is64BitReg())
               {
               if (targetRegisterHW->getState() != TR::RealRegister::Free &&
                   targetRegisterHW->getState() != TR::RealRegister::Unlatched)
                  {
                  // if the targetReg and targetRegHW are assigned to two different virtual regs
                  if (targetRegisterHW->getAssignedRegister() &&
                      targetRegisterHW->getAssignedRegister() != targetRegister->getAssignedRegister())
                     {
                     //TR_ASSERTC( currentTargetVirtual->isLowWordOnly(),_cg->comp(), "currentTargetVirtual is not LWOnly but HW is clobbered by another vreg?");
                     virtualRegister->block();
                     currentTargetVirtual->block();
                     spareReg->block(); //to do: is this necessary? only need to block HPR?
                     // free up this highword register by either spilling it or moving it
                     cursor = self()->freeHighWordRegister(currentInstruction, targetRegisterHW, instFlags);
                     spareReg->unblock();
                     currentTargetVirtual->unblock();
                     virtualRegister->unblock();
                     }
                  }
               }

            // Spill policy decided the best reg to spill was not the targetReg, so move target
            // to the spareReg, and move the source reg to the target.
            if (targetRegister->getRegisterNumber() != spareReg->getRegisterNumber() && !doNotRegCopy)
               {
               _cg->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(currentInstruction, currentTargetVirtualRK, targetRegister, spareReg, self()->cg(), instFlags);
               if (enableHighWordRA && currentTargetVirtual->is64BitReg())
                  {
                  spareReg->getHighWordRegister()->setState(TR::RealRegister::Assigned);
                  spareReg->getHighWordRegister()->setAssignedRegister(currentTargetVirtual);
                  targetRegister->getLowWordRegister()->setState(TR::RealRegister::Unlatched);
                  targetRegister->getLowWordRegister()->setAssignedRegister(NULL);
                  targetRegisterHW->setState(TR::RealRegister::Unlatched);
                  targetRegisterHW->setAssignedRegister(NULL);
                  }
               spareReg->setState(TR::RealRegister::Assigned);
               spareReg->setAssignedRegister(currentTargetVirtual);
               currentTargetVirtual->setAssignedRegister(spareReg);
               }

            cursor = self()->registerCopy(currentInstruction, currentAssignedRegisterRK, currentAssignedRegister, targetRegister, self()->cg(), instFlags);
            currentAssignedRegister->setState(TR::RealRegister::Unlatched);
            currentAssignedRegister->setAssignedRegister(NULL);
            if (enableHighWordRA && virtualRegister->is64BitReg())
               {
               currentAssignedRegisterHW->setState(TR::RealRegister::Unlatched);
               currentAssignedRegisterHW->setAssignedRegister(NULL);
               }
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

               _cg->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
               _cg->setRegisterAssignmentFlag(TR_RegisterSpilled);

               cursor = self()->registerCopy(currentInstruction, rk, currentAssignedRegister, targetRegister, self()->cg(), instFlags);
               currentAssignedRegister->setState(TR::RealRegister::Unlatched);
               currentAssignedRegister->setAssignedRegister(NULL);
               }
            else
               {
               cursor = self()->registerExchange(currentInstruction, rk, targetRegister, currentAssignedRegister, spareReg, self()->cg(), instFlags);
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
            int32_t availHWRegs = -1;
            bool spillTargetHWReg = false;
            if (enableHighWordRA && virtualRegister->is64BitReg())
               {
               // for 64-bit, do not spill to the sibling HPR
               availHWRegs = availHighWordRegMap;
               if (targetRegisterHW->getState() != TR::RealRegister::Free &&
                   targetRegisterHW->getAssignedRegister() != NULL &&
                   targetRegisterHW->getAssignedRegister() != targetRegister->getAssignedRegister())
                  {
                  spillTargetHWReg = true;
                  }
               }
            // In zLinux, GPR6 may be used for param passing, in this case, we can't spill to it
            if (TR::Compiler->target.isLinux())
               {
               // HPR7-12 are available
               self()->spillRegister(currentInstruction, currentTargetVirtual, availHWRegs & 0x1F800000);
               }
            else
               {
               // can only spill to preserved registers (HPR6-HPR12)
               self()->spillRegister(currentInstruction, currentTargetVirtual, availHWRegs & 0x1FC00000);
               //spillRegister(currentInstruction, currentTargetVirtual, 0x1FC00000);
               }
            targetRegister->setState(TR::RealRegister::Unlatched);
            targetRegister->setAssignedRegister(NULL);

            if (spillTargetHWReg)
               {
               self()->spillRegister(currentInstruction, targetRegisterHW->getAssignedRegister(), 0);
               }

            if (enableHighWordRA && currentTargetVirtual->is64BitReg())
               {
               targetRegister->getLowWordRegister()->setState(TR::RealRegister::Unlatched);
               targetRegister->getLowWordRegister()->setAssignedRegister(NULL);
               targetRegisterHW->setState(TR::RealRegister::Unlatched);
               targetRegisterHW->setAssignedRegister(NULL);
               }
            }
         else
            {
            // If we didn't find a FREE reg, choose the best one to spill.
            // The worst case situation is that only the target is left to spill.
            if (spareReg == NULL)
               {
               _cg->setRegisterAssignmentFlag(TR_RegisterSpilled);

               virtualRegister->block();
               if (enableHighWordRA && virtualRegister->is64BitReg())
                  {
                  // if we end up spilling currentTargetVirtual to HPR, make sure to not pick the HPR of targetRegister since it will not
                  // free up the full 64 bit register
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), availHighWordRegMap, false, true);
                  }
               else
                  {
                  spareReg = self()->freeBestRegister(currentInstruction, currentTargetVirtual, currentTargetVirtual->getKind(), availHighWordRegMap, true);
                  }

               // For some reason (blocked/locked regs etc), we couldn't find a spare reg so spill the virtual in the target and use it for coercion
               if (spareReg == NULL)
               {
               self()->spillRegister(currentInstruction, currentTargetVirtual);
               targetRegister->setAssignedRegister(virtualRegister);
               virtualRegister->setAssignedRegister(targetRegister);
               targetRegister->setState(TR::RealRegister::Assigned);
               }

               // freeBestRegister could spill currentTargetVirtual directly to free up targetRegister.
               // If currentTargetVirtual is 64-bit, spareReg will be a GPR (targetRegister's low word)
               // however, targetRegister could be an HPR because we might only need its high word. In this case
               // we must not generate register move of LGR targetRegister,spareReg because they are the same register
               // and we must leave currentTargetVirtual spilled instead of assigning it to spareReg.
               if (targetRegister->isHighWordRegister() && currentTargetVirtual->is64BitReg() &&
                   targetRegister->getRegisterNumber() == spareReg->getHighWordRegister()->getRegisterNumber())
                  doNotRegCopy = true;
               virtualRegister->unblock();
               }

            // if currentTargetVirtual is low word only, spare Reg will be low word
            // but virtual Reg could require full size, in this case, need to free the HW of target Reg
            if (enableHighWordRA && virtualRegister->is64BitReg())
               {
               if (targetRegisterHW->getState() != TR::RealRegister::Free &&
                   targetRegisterHW->getState() != TR::RealRegister::Unlatched)
                  {
                  if (targetRegisterHW->getAssignedRegister() &&
                      targetRegisterHW->getAssignedRegister() != targetRegister->getAssignedRegister())
                     {
                     //TR_ASSERTC( currentTargetVirtual->isLowWordOnly(),_cg->comp(), "currentTargetVirtual is not LWOnly but HW is clobbered by another vreg?");
                     virtualRegister->block();
                     currentTargetVirtual->block();
                     spareReg->block(); //to do: is this necessary? only need to block HPR?
                     // free up this highword register by either spilling it or moving it
                     cursor = self()->freeHighWordRegister(currentInstruction, targetRegisterHW, instFlags);
                     spareReg->unblock();
                     currentTargetVirtual->unblock();
                     virtualRegister->unblock();
                     }
                  }
               }

            //  If we chose to spill a reg that wasn't the target, we use the new space
            //  to free up the target.
            if (targetRegister->getRegisterNumber() != spareReg->getRegisterNumber() && !doNotRegCopy)
               {
               _cg->resetRegisterAssignmentFlag(TR_RegisterSpilled);
               _cg->traceRegAssigned(currentTargetVirtual, spareReg);

               cursor = self()->registerCopy(currentInstruction, currentTargetVirtualRK, targetRegister, spareReg, self()->cg(), instFlags);
               spareReg->setState(TR::RealRegister::Assigned);
               spareReg->setAssignedRegister(currentTargetVirtual);

               if (enableHighWordRA && currentTargetVirtual->is64BitReg())
                  {
                  spareReg->getHighWordRegister()->setState(TR::RealRegister::Assigned);
                  spareReg->getHighWordRegister()->setAssignedRegister(currentTargetVirtual);
                  targetRegister->getLowWordRegister()->setState(TR::RealRegister::Unlatched);
                  targetRegister->getLowWordRegister()->setAssignedRegister(NULL);
                  targetRegisterHW->setState(TR::RealRegister::Unlatched);
                  targetRegisterHW->setAssignedRegister(NULL);
                  }
               currentTargetVirtual->setAssignedRegister(spareReg);
               self()->cg()->recordRegisterAssignment(spareReg,currentTargetVirtual);
               }

            //  Draw the source reg back out of SPILL state
            if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
               {
               _cg->setRegisterAssignmentFlag(TR_RegisterReloaded);
               self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
               }
            else
               {
               if (!comp->getOption(TR_DisableOOL) && self()->cg()->isOutOfLineColdPath())
                  {
                  _cg->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
                  }
               }
            }
         }

      _cg->resetRegisterAssignmentFlag(TR_IndirectCoercion);
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
                                         getRegisterName(targetRegister,_cg),
                                         getRegisterName(virtualRegister,_cg));
      traceMsg(comp, "             This assignment is equivalent to using a hard coded real register.\n");
      if (_cg->getRAPassAR() && virtualRegister->getAssignedRegister() && virtualRegister->getAssignedRegister() != targetRegister)
         TR_ASSERT(false, "shouldn't re-assign to a locked register\n");


      // the virtual register haven't be assigned to any real register yet
      if (currentAssignedRegister == NULL)
         {
         // get the value of virtual register back from spill state if not first use
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            _cg->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         else
            {
            if (!comp->getOption(TR_DisableOOL) && self()->cg()->isOutOfLineColdPath())
               {
               _cg->getFirstTimeLiveOOLRegisterList()->push_front(virtualRegister);
               }
            }
         }
      else
         {
         // virtual register is currently assigned to a different register,
         // override it with the target reg

         // todo: HW copy, LW copy or 64bit?
         // if targetReg is HW, copy need to be HW
         cursor = self()->registerCopy(currentInstruction, currentAssignedRegisterRK, currentAssignedRegister, targetRegister, self()->cg(), instFlags);
         if (enableHighWordRA && virtualRegister->is64BitReg())
            {
            currentAssignedRegisterHW->setState(TR::RealRegister::Free);
            currentAssignedRegisterHW->setAssignedRegister(NULL);
            }
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }

      if (self()->supportLockedRegisterAssignment())
      	 {
      	 // the AR check is to avoid false errors for biit call
      	 // like "LAM(R1,R1,...)" and needs to be re-visited

      	 if (!_cg->getRAPassAR() &&
      	 	   targetRegister->getAssignedRegister() != NULL &&
      	 	   targetRegister->getAssignedRegister() != targetRegister)
      	 	   {
      	     TR::Register * toFreeRegister = targetRegister->getAssignedRegister();
             // register is locked but assigned to a VR, need to re-assign VR to another reg via reg copy or spill it
             _cg->traceRegisterAssignment(" Freeing locked register %R ", targetRegister);
             uint64_t availRegMask = 0xffffffff;
             if (toFreeRegister->isUsedInMemRef())
                availRegMask &= ~(TR::RealRegister::getBitMask(TR::RealRegister::GPR0));

             TR::RealRegister * bestRegister = NULL;
             if ((bestRegister = self()->findBestFreeRegister(currentInstruction, toFreeRegister->getKind(), toFreeRegister, availRegMask)) == NULL)
                {
                bestRegister = self()->freeBestRegister(currentInstruction, toFreeRegister, toFreeRegister->getKind(), availRegMask);
                }
             self()->registerCopy(currentInstruction, toFreeRegister->getKind(), toRealRegister(targetRegister), bestRegister, _cg, 0);
             toFreeRegister->setAssignedRegister(bestRegister);
             bestRegister->setAssignedRegister(toFreeRegister);
             bestRegister->setState(TR::RealRegister::Assigned);

             }
      	 targetRegister->setAssignedRegister(virtualRegister);
         }
      }

   virtualRegister->setAssignedRegister(targetRegister);
   if (enableHighWordRA && virtualRegister->is64BitReg())
      {
      targetRegisterHW->setAssignedRegister(virtualRegister);
      targetRegisterHW->setState(TR::RealRegister::Assigned);
      }
   _cg->traceRegAssigned(virtualRegister, targetRegister);

   _cg->clearRegisterAssignmentFlags();
   return cursor;
   }

uint64_t OMR::Z::Machine::filterColouredRegisterConflicts(TR::Register *targetRegister, TR::Register *siblingRegister,
                                                             TR::Instruction *currInst)
  {
  uint64_t mask=0xffffffff;
  TR::Compilation *comp = TR::comp();
  TR::list<TR::Register *> conflictRegs(getTypedAllocator<TR::Register*>(comp->allocator()));

  if(currInst->defsAnyRegister(targetRegister))
    {
    currInst->getDefinedRegisters(conflictRegs);
    for(auto reg = conflictRegs.begin(); reg != conflictRegs.end(); ++reg)
      {
      TR::Register *cr=(*reg)->getRealRegister() ? NULL : (*reg)->getAssignedRegister();
      if(cr==NULL)
        cr=(*reg)->getRealRegister() ? NULL : (*reg)->getColouredRegister();
      if(cr && targetRegister != (*reg) && (*reg)->getAssignedRegister() != targetRegister &&
         (siblingRegister == NULL || (*reg) != siblingRegister))
        mask &= ~TR::RealRegister::getBitMask(toRealRegister(cr)->getRegisterNumber());
      }
    }

  currInst->getUsedRegisters(conflictRegs);
  for(auto reg = conflictRegs.begin(); reg != conflictRegs.end(); ++reg)
    {
    TR::Register *cr=(*reg)->getRealRegister() ? NULL : (*reg)->getAssignedRegister();
    if(cr==NULL)
      cr=(*reg)->getRealRegister() ? NULL : (*reg)->getColouredRegister();
    if(cr && targetRegister != (*reg) && (*reg)->getAssignedRegister() != targetRegister &&
       (siblingRegister == NULL || (*reg) != siblingRegister))
      mask &= ~TR::RealRegister::getBitMask(toRealRegister(cr)->getRegisterNumber());
    }

  return mask;

  }


////////////////////////////////////////////////////////////////////////////////
// OMR::Z::Machine::initialiseRegisterFile
////////////////////////////////////////////////////////////////////////////////
void
OMR::Z::Machine::initialiseRegisterFile()
   {
   // *this    swipeable for debugging purposes

   // Initialize GPRs
   _registerFile[TR::RealRegister::NoReg] = NULL;
   _registerFile[TR::RealRegister::SpilledReg] = NULL;

   _registerFile[TR::RealRegister::GPR0] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR0, TR::RealRegister::GPR0Mask, _cg);

   _registerFile[TR::RealRegister::GPR1] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR1, TR::RealRegister::GPR1Mask, _cg);

   _registerFile[TR::RealRegister::GPR2] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR2, TR::RealRegister::GPR2Mask, _cg);

   _registerFile[TR::RealRegister::GPR3] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR3, TR::RealRegister::GPR3Mask, _cg);

   _registerFile[TR::RealRegister::GPR4] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR4, TR::RealRegister::GPR4Mask, _cg);

   _registerFile[TR::RealRegister::GPR5] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR5, TR::RealRegister::GPR5Mask, _cg);

   _registerFile[TR::RealRegister::GPR6] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR6, TR::RealRegister::GPR6Mask, _cg);

   _registerFile[TR::RealRegister::GPR7] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR7, TR::RealRegister::GPR7Mask, _cg);

   _registerFile[TR::RealRegister::GPR8] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR8, TR::RealRegister::GPR8Mask, _cg);

   _registerFile[TR::RealRegister::GPR9] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::GPR9, TR::RealRegister::GPR9Mask, _cg);

   _registerFile[TR::RealRegister::GPR10] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR10, TR::RealRegister::GPR10Mask, _cg);

   _registerFile[TR::RealRegister::GPR11] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR11, TR::RealRegister::GPR11Mask, _cg);

   _registerFile[TR::RealRegister::GPR12] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR12, TR::RealRegister::GPR12Mask, _cg);

   _registerFile[TR::RealRegister::GPR13] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR13, TR::RealRegister::GPR13Mask, _cg);

   _registerFile[TR::RealRegister::GPR14] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR14, TR::RealRegister::GPR14Mask, _cg);

   _registerFile[TR::RealRegister::GPR15] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::GPR15, TR::RealRegister::GPR15Mask, _cg);

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

   _registerFile[TR::RealRegister::FPR0] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR0, TR::RealRegister::FPR0Mask, _cg);

   _registerFile[TR::RealRegister::FPR1] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR1, TR::RealRegister::FPR1Mask, _cg);

   _registerFile[TR::RealRegister::FPR2] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR2, TR::RealRegister::FPR2Mask, _cg);

   _registerFile[TR::RealRegister::FPR3] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR3, TR::RealRegister::FPR3Mask, _cg);

   _registerFile[TR::RealRegister::FPR4] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR4, TR::RealRegister::FPR4Mask, _cg);

   _registerFile[TR::RealRegister::FPR5] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR5, TR::RealRegister::FPR5Mask, _cg);

   _registerFile[TR::RealRegister::FPR6] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR6, TR::RealRegister::FPR6Mask, _cg);

   _registerFile[TR::RealRegister::FPR7] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR7, TR::RealRegister::FPR7Mask, _cg);

   _registerFile[TR::RealRegister::FPR8] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR8, TR::RealRegister::FPR8Mask, _cg);

   _registerFile[TR::RealRegister::FPR9] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::FPR9, TR::RealRegister::FPR9Mask, _cg);

   _registerFile[TR::RealRegister::FPR10] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR10, TR::RealRegister::FPR10Mask, _cg);

   _registerFile[TR::RealRegister::FPR11] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR11, TR::RealRegister::FPR11Mask, _cg);

   _registerFile[TR::RealRegister::FPR12] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR12, TR::RealRegister::FPR12Mask, _cg);

   _registerFile[TR::RealRegister::FPR13] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR13, TR::RealRegister::FPR13Mask, _cg);

   _registerFile[TR::RealRegister::FPR14] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR14, TR::RealRegister::FPR14Mask, _cg);

   _registerFile[TR::RealRegister::FPR15] = new (_cg->trHeapMemory()) TR::RealRegister(TR_FPR, 0, TR::RealRegister::Free,
                                                      TR::RealRegister::FPR15, TR::RealRegister::FPR15Mask, _cg);

   // Initialize Access Regs
   _registerFile[TR::RealRegister::AR0] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR0, TR::RealRegister::AR0Mask, _cg);

   _registerFile[TR::RealRegister::AR1] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR1, TR::RealRegister::AR1Mask, _cg);

   _registerFile[TR::RealRegister::AR2] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR2, TR::RealRegister::AR2Mask, _cg);

   _registerFile[TR::RealRegister::AR3] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR3, TR::RealRegister::AR3Mask, _cg);

   _registerFile[TR::RealRegister::AR4] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR4, TR::RealRegister::AR4Mask, _cg);

   _registerFile[TR::RealRegister::AR5] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR5, TR::RealRegister::AR5Mask, _cg);

   _registerFile[TR::RealRegister::AR6] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR6, TR::RealRegister::AR6Mask, _cg);

   _registerFile[TR::RealRegister::AR7] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR7, TR::RealRegister::AR7Mask, _cg);

   _registerFile[TR::RealRegister::AR8] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR8, TR::RealRegister::AR8Mask, _cg);

   _registerFile[TR::RealRegister::AR9] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR9, TR::RealRegister::AR9Mask, _cg);

   _registerFile[TR::RealRegister::AR10] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR10, TR::RealRegister::AR10Mask, _cg);

   _registerFile[TR::RealRegister::AR11] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR11, TR::RealRegister::AR11Mask, _cg);

   _registerFile[TR::RealRegister::AR12] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR12, TR::RealRegister::AR12Mask, _cg);

   _registerFile[TR::RealRegister::AR13] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR13, TR::RealRegister::AR13Mask, _cg);

   _registerFile[TR::RealRegister::AR14] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR14, TR::RealRegister::AR14Mask, _cg);

   _registerFile[TR::RealRegister::AR15] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::AR15, TR::RealRegister::AR15Mask, _cg);

   // Initialize High Regs
   _registerFile[TR::RealRegister::HPR0] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR0, TR::RealRegister::HPR0Mask, _cg);

   _registerFile[TR::RealRegister::HPR1] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR1, TR::RealRegister::HPR1Mask, _cg);

   _registerFile[TR::RealRegister::HPR2] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR2, TR::RealRegister::HPR2Mask, _cg);

   _registerFile[TR::RealRegister::HPR3] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR3, TR::RealRegister::HPR3Mask, _cg);

   _registerFile[TR::RealRegister::HPR4] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR4, TR::RealRegister::HPR4Mask, _cg);

   _registerFile[TR::RealRegister::HPR5] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR5, TR::RealRegister::HPR5Mask, _cg);

   _registerFile[TR::RealRegister::HPR6] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR6, TR::RealRegister::HPR6Mask, _cg);

   _registerFile[TR::RealRegister::HPR7] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR7, TR::RealRegister::HPR7Mask, _cg);

   _registerFile[TR::RealRegister::HPR8] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR8, TR::RealRegister::HPR8Mask, _cg);

   _registerFile[TR::RealRegister::HPR9] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR9, TR::RealRegister::HPR9Mask, _cg);

   _registerFile[TR::RealRegister::HPR10] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR10, TR::RealRegister::HPR10Mask, _cg);

   _registerFile[TR::RealRegister::HPR11] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR11, TR::RealRegister::HPR11Mask, _cg);

   _registerFile[TR::RealRegister::HPR12] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR12, TR::RealRegister::HPR12Mask, _cg);

   _registerFile[TR::RealRegister::HPR13] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR13, TR::RealRegister::HPR13Mask, _cg);

   _registerFile[TR::RealRegister::HPR14] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR14, TR::RealRegister::HPR14Mask, _cg);

   _registerFile[TR::RealRegister::HPR15] = new (_cg->trHeapMemory()) TR::RealRegister(TR_GPR, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::HPR15, TR::RealRegister::HPR15Mask, _cg);

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

   _registerFile[TR::RealRegister::VRF16] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF16, TR::RealRegister::VRF16Mask, _cg);

   _registerFile[TR::RealRegister::VRF17] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF17, TR::RealRegister::VRF17Mask, _cg);

   _registerFile[TR::RealRegister::VRF18] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF18, TR::RealRegister::VRF18Mask, _cg);

   _registerFile[TR::RealRegister::VRF19] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF19, TR::RealRegister::VRF19Mask, _cg);

   _registerFile[TR::RealRegister::VRF20] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF20, TR::RealRegister::VRF20Mask, _cg);

   _registerFile[TR::RealRegister::VRF21] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF21, TR::RealRegister::VRF21Mask, _cg);

   _registerFile[TR::RealRegister::VRF22] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF22, TR::RealRegister::VRF22Mask, _cg);

   _registerFile[TR::RealRegister::VRF23] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF23, TR::RealRegister::VRF23Mask, _cg);

   _registerFile[TR::RealRegister::VRF24] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF24, TR::RealRegister::VRF24Mask, _cg);

   _registerFile[TR::RealRegister::VRF25] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF25, TR::RealRegister::VRF25Mask, _cg);

   _registerFile[TR::RealRegister::VRF26] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF26, TR::RealRegister::VRF26Mask, _cg);

   _registerFile[TR::RealRegister::VRF27] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF27, TR::RealRegister::VRF27Mask, _cg);

   _registerFile[TR::RealRegister::VRF28] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF28, TR::RealRegister::VRF28Mask, _cg);

  _registerFile[TR::RealRegister::VRF29] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF29, TR::RealRegister::VRF29Mask, _cg);

  _registerFile[TR::RealRegister::VRF30] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF30, TR::RealRegister::VRF30Mask, _cg);

  _registerFile[TR::RealRegister::VRF31] = new (_cg->trHeapMemory()) TR::RealRegister(TR_VRF, 0, TR::RealRegister::Free,
                                                     TR::RealRegister::VRF31, TR::RealRegister::VRF31Mask, _cg);
   _registerFile[TR::RealRegister::HPR0]->setLowWordRegister(_registerFile[TR::RealRegister::GPR0]);
   _registerFile[TR::RealRegister::HPR1]->setLowWordRegister(_registerFile[TR::RealRegister::GPR1]);
   _registerFile[TR::RealRegister::HPR2]->setLowWordRegister(_registerFile[TR::RealRegister::GPR2]);
   _registerFile[TR::RealRegister::HPR3]->setLowWordRegister(_registerFile[TR::RealRegister::GPR3]);
   _registerFile[TR::RealRegister::HPR4]->setLowWordRegister(_registerFile[TR::RealRegister::GPR4]);
   _registerFile[TR::RealRegister::HPR5]->setLowWordRegister(_registerFile[TR::RealRegister::GPR5]);
   _registerFile[TR::RealRegister::HPR6]->setLowWordRegister(_registerFile[TR::RealRegister::GPR6]);
   _registerFile[TR::RealRegister::HPR7]->setLowWordRegister(_registerFile[TR::RealRegister::GPR7]);
   _registerFile[TR::RealRegister::HPR8]->setLowWordRegister(_registerFile[TR::RealRegister::GPR8]);
   _registerFile[TR::RealRegister::HPR9]->setLowWordRegister(_registerFile[TR::RealRegister::GPR9]);
   _registerFile[TR::RealRegister::HPR10]->setLowWordRegister(_registerFile[TR::RealRegister::GPR10]);
   _registerFile[TR::RealRegister::HPR11]->setLowWordRegister(_registerFile[TR::RealRegister::GPR11]);
   _registerFile[TR::RealRegister::HPR12]->setLowWordRegister(_registerFile[TR::RealRegister::GPR12]);
   _registerFile[TR::RealRegister::HPR13]->setLowWordRegister(_registerFile[TR::RealRegister::GPR13]);
   _registerFile[TR::RealRegister::HPR14]->setLowWordRegister(_registerFile[TR::RealRegister::GPR14]);
   _registerFile[TR::RealRegister::HPR15]->setLowWordRegister(_registerFile[TR::RealRegister::GPR15]);

   _registerFile[TR::RealRegister::GPR0]->setHighWordRegister(_registerFile[TR::RealRegister::HPR0]);
   _registerFile[TR::RealRegister::GPR1]->setHighWordRegister(_registerFile[TR::RealRegister::HPR1]);
   _registerFile[TR::RealRegister::GPR2]->setHighWordRegister(_registerFile[TR::RealRegister::HPR2]);
   _registerFile[TR::RealRegister::GPR3]->setHighWordRegister(_registerFile[TR::RealRegister::HPR3]);
   _registerFile[TR::RealRegister::GPR4]->setHighWordRegister(_registerFile[TR::RealRegister::HPR4]);
   _registerFile[TR::RealRegister::GPR5]->setHighWordRegister(_registerFile[TR::RealRegister::HPR5]);
   _registerFile[TR::RealRegister::GPR6]->setHighWordRegister(_registerFile[TR::RealRegister::HPR6]);
   _registerFile[TR::RealRegister::GPR7]->setHighWordRegister(_registerFile[TR::RealRegister::HPR7]);
   _registerFile[TR::RealRegister::GPR8]->setHighWordRegister(_registerFile[TR::RealRegister::HPR8]);
   _registerFile[TR::RealRegister::GPR9]->setHighWordRegister(_registerFile[TR::RealRegister::HPR9]);
   _registerFile[TR::RealRegister::GPR10]->setHighWordRegister(_registerFile[TR::RealRegister::HPR10]);
   _registerFile[TR::RealRegister::GPR11]->setHighWordRegister(_registerFile[TR::RealRegister::HPR11]);
   _registerFile[TR::RealRegister::GPR12]->setHighWordRegister(_registerFile[TR::RealRegister::HPR12]);
   _registerFile[TR::RealRegister::GPR13]->setHighWordRegister(_registerFile[TR::RealRegister::HPR13]);
   _registerFile[TR::RealRegister::GPR14]->setHighWordRegister(_registerFile[TR::RealRegister::HPR14]);
   _registerFile[TR::RealRegister::GPR15]->setHighWordRegister(_registerFile[TR::RealRegister::HPR15]);
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
   if (OMR::Z::Machine::isRestrictedReg(reg))
      return tableIndex;
   if (self()->getS390RealRegister(reg)->getState() == TR::RealRegister::Locked)
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
   if (OMR::Z::Machine::isRestrictedReg(reg))
      return tableIndex;
   if (self()->getS390RealRegister(reg)->getState() == TR::RealRegister::Locked)
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
   TR::Compilation *comp = TR::comp();

   if (!comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      int32_t p = 0;
      static char *dontInitializeGlobalRegisterTableFromLinkage = feGetEnv("TR_dontInitializeGlobalRegisterTableFromLinkage");
      bool enableHighWordGRA = _cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA);
      if (dontInitializeGlobalRegisterTableFromLinkage)
         {
         self()->setFirstGlobalGPRRegisterNumber(0);

         // Volatiles that aren't linkage regs
         //
        // p = addGlobalReg(TR::RealRegister::GPR0, p); // Local register assigner can't handle virtuals assigned to GPR0 appearing in memrefs

         // Linkage regs in reverse order
         //
         p = self()->addGlobalReg(TR::RealRegister::GPR3, p);
         p = self()->addGlobalReg(TR::RealRegister::GPR2, p);
         p = self()->addGlobalReg(TR::RealRegister::GPR1, p);
         self()->setLastLinkageGPR(p-1);

         // Preserved regs, vmthread last
         //
         p = self()->addGlobalReg(TR::RealRegister::GPR6, p); // NOTE: GPR6 must be avoided if on-demand literal pool opt isn't run
         static char * noGraFIX= feGetEnv("TR_NOGRAFIX");
         // Exclude GPR7 if we are not on Freeway+ hardware
         if (  !noGraFIX
            && !comp->getOption(TR_DisableLongDispStackSlot)
            && _cg->getExtCodeBaseRegisterIsFree()
            )
            {
            p = self()->addGlobalReg(TR::RealRegister::GPR7, p);
            }
         p = self()->addGlobalReg(TR::RealRegister::GPR8,  p);
         p = self()->addGlobalReg(TR::RealRegister::GPR9,  p);
         p = self()->addGlobalReg(TR::RealRegister::GPR10, p);
         p = self()->addGlobalReg(TR::RealRegister::GPR11, p);
         p = self()->addGlobalReg(TR::RealRegister::GPR12, p);
         p = self()->addGlobalReg(TR::RealRegister::GPR13, p); // vmthread

         if (enableHighWordGRA)
            {
            // HPR
            // this is a bit tricky, we consider Global HPRs part of Global GPRs
            self()->setFirstGlobalHPRRegisterNumber(p);
            // volatile HPRs
            // might use HPR4 on 31-bit zLinux
            p = self()->addGlobalReg(TR::RealRegister::HPR3, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR2, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR1, p);
            // for preserved regs, we can only use HPR6-12 because VM only saves/restores those
            if (TR::Compiler->target.is32Bit())
               {
               // might use GPR6 on 64-bit for lit pool reg
               p = self()->addGlobalReg(TR::RealRegister::HPR6, p);
               }
            p = self()->addGlobalReg(TR::RealRegister::HPR7, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR8, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR9, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR10, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR11, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR12, p);
            self()->setLastGlobalHPRRegisterNumber(p-1);
            // might use HPR15 on 31-bit zOS
            }
         // Access regs
         //
         if (comp->getOption(TR_Enable390AccessRegs))
            {
            for (int32_t i = TR::RealRegister::FirstAR; i <= TR::RealRegister::LastAR; i++)
               {
               p = self()->addGlobalReg((TR::RealRegister::RegNum)i, p);
               }
            }

         self()->setLastGlobalGPRRegisterNumber(p-1);

         // Volatiles that aren't linkage regs
         //
         p = self()->addGlobalReg(TR::RealRegister::FPR1, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR3, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR5, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR7, p);
   #if !defined(ENABLE_PRESERVED_FPRS)
         p = self()->addGlobalReg(TR::RealRegister::FPR15, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR14, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR13, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR12, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR11, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR10, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR9,  p);
         p = self()->addGlobalReg(TR::RealRegister::FPR8,  p);
   #endif

         // Linkage regs in reverse order
         //
         p = self()->addGlobalReg(TR::RealRegister::FPR6, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR4, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR2, p);
         p = self()->addGlobalReg(TR::RealRegister::FPR0, p);
         self()->setLastLinkageFPR(p-1);

         // Preserved regs
         //
   #if defined(ENABLE_PRESERVED_FPRS)
         p = addGlobalReg(TR::RealRegister::FPR15, p);
         p = addGlobalReg(TR::RealRegister::FPR14, p);
         p = addGlobalReg(TR::RealRegister::FPR13, p);
         p = addGlobalReg(TR::RealRegister::FPR12, p);
         p = addGlobalReg(TR::RealRegister::FPR11, p);
         p = addGlobalReg(TR::RealRegister::FPR10, p);
         p = addGlobalReg(TR::RealRegister::FPR9,  p);
         p = addGlobalReg(TR::RealRegister::FPR8,  p);
   #endif
         }
      else
         {
      TR::Linkage *linkage = _cg->getS390Linkage();
         int32_t i;
         TR::RealRegister::RegNum reg;
         self()->setFirstGlobalGPRRegisterNumber(0);

         if (linkage->isZLinuxLinkageType())
            p = self()->addGlobalReg(TR::RealRegister::GPR1, p); // WOOHOO!

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
            for (i = linkage->getNumIntegerArgumentRegisters(); i >= 0; i--)
               {
               if (!linkage->getPreserved(linkage->getIntegerArgumentRegister(i)))
                  p = self()->addGlobalReg(linkage->getIntegerArgumentRegister(i), p);
               }
            }


         for (i = linkage->getNumIntegerArgumentRegisters(); i >= 0; i--)
            p = self()->addGlobalReg(linkage->getIntegerArgumentRegister(i), p);

         self()->setLastLinkageGPR(p-1);

         if ( (_cg->isLiteralPoolOnDemandOn() && !linkage->isZLinuxLinkageType()) || (_cg->isLiteralPoolOnDemandOn() && !linkage->getPreserved(linkage->getLitPoolRegister())) )
            p = self()->addGlobalReg(linkage->getLitPoolRegister(), p);
         if (!_cg->isGlobalStaticBaseRegisterOn())
            p = self()->addGlobalReg(linkage->getStaticBaseRegister(), p);
         if (!_cg->isGlobalPrivateStaticBaseRegisterOn())
            p = self()->addGlobalReg(linkage->getPrivateStaticBaseRegister(), p);
         for (i = linkage->getNumSpecialArgumentRegisters(); i >= 0; i--)
            p = self()->addGlobalReg(linkage->getSpecialArgumentRegister(i), p);
         p = self()->addGlobalReg(linkage->getIntegerReturnRegister(), p);
         p = self()->addGlobalReg(linkage->getLongReturnRegister(), p);
         p = self()->addGlobalReg(linkage->getLongLowReturnRegister(), p);
         p = self()->addGlobalReg(linkage->getLongHighReturnRegister(), p);

         int32_t eReg = 0;

         // Preserved regs in descending order to encourage stmg with gpr15 and
         // gpr14, which are commonly preserved in zLinux system linkage
         //
         if (linkage->isZLinuxLinkageType()) // ordering seems to make crashes on zos.
            {
            for (i = TR::RealRegister::LastAssignableGPR; i >= TR::RealRegister::FirstGPR; --i)
               {
               reg = (TR::RealRegister::RegNum)i;

               if (eReg && reg == eReg) continue;

               if (linkage->getPreserved(reg))
                  {
                     // Dangling else above
                     if (reg != linkage->getStaticBaseRegister() &&
                           reg != linkage->getPrivateStaticBaseRegister() &&
                           reg != linkage->getStackPointerRegister())
                        p = self()->addGlobalReg(reg, p);
                  }
               }
            }
         else
            {
            // Preserved regs, with special heavily-used regs last
            //
            for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
               {
               reg = (TR::RealRegister::RegNum)i;

               if (eReg && reg == eReg) continue;

               if (linkage->getPreserved(reg))
                  {
                     // Dangling else above
                     if (reg != linkage->getLitPoolRegister() &&
                           reg != linkage->getStaticBaseRegister() &&
                           reg != linkage->getPrivateStaticBaseRegister() &&
                           reg != linkage->getStackPointerRegister())
                        p = self()->addGlobalReg(reg, p);
                  }
               }
            }

         p = self()->addGlobalRegLater(linkage->getMethodMetaDataRegister(), p);
         if (TR::Compiler->target.isZOS())
            {
            p = self()->addGlobalRegLater(_cg->getS390Linkage()->getStackPointerRegister(), p);
            }

         // Special regs that add to prologue cost
         //
         p = self()->addGlobalRegLater(linkage->getEnvironmentPointerRegister(), p);

         //p = addGlobalRegLater(linkage->getLitPoolRegister(), p); // zOS private linkage might want this here?

         if (linkage->isXPLinkLinkageType())
            p = self()->addGlobalRegLater(TR::RealRegister::GPR7, p);

         if (enableHighWordGRA)
            {
            // HPR
            // this is a bit tricky, we consider Global HPRs part of Global GPRs
            self()->setFirstGlobalHPRRegisterNumber(p);
            // volatile HPRs
            // might use HPR4 on 31-bit zLinux
            p = self()->addGlobalReg(TR::RealRegister::HPR3, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR2, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR1, p);
            // for preserved regs, we can only use HPR6-12 because VM only saves/restores those
            if (TR::Compiler->target.is32Bit())
               {
               // might use GPR6 on 64-bit for lit pool reg
               p = self()->addGlobalReg(TR::RealRegister::HPR6, p);
               }
            p = self()->addGlobalReg(TR::RealRegister::HPR7, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR8, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR9, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR10, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR11, p);
            p = self()->addGlobalReg(TR::RealRegister::HPR12, p);
            self()->setLastGlobalHPRRegisterNumber(p-1);
            // might use HPR15 on 31-bit zOS
            }

         self()->setLastGlobalGPRRegisterNumber(p-1);

         if (_cg->globalAccessRegistersSupported())
            {
            self()->setFirstGlobalAccessRegisterNumber(p);

            //add the same access registers as GPRs
            for (i = self()->getFirstGlobalGPRRegisterNumber(); i <= self()->getLastGlobalGPRRegisterNumber(); i++)
               {
               reg = (TR::RealRegister::RegNum) (_globalRegisterNumberToRealRegisterMap[i] - TR::RealRegister::FirstGPR + TR::RealRegister::FirstAR);
               p = self()->addGlobalReg(reg, p);
               }
            self()->setLastGlobalAccessRegisterNumber(p-1);
            }

          // Volatiles that aren't linkage regs
          //
          self()->setFirstGlobalFPRRegisterNumber(p);
          for (i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
             {
             reg = (TR::RealRegister::RegNum)i;
             if (!linkage->getPreserved(reg) && !linkage->getFloatArgument(reg))
                p = self()->addGlobalReg(reg, p);
             }

          // Linkage regs in reverse order
          //
          for (i = linkage->getNumFloatArgumentRegisters(); i >= 0; i--)
             p = self()->addGlobalReg(linkage->getFloatArgumentRegister(i), p);
          self()->setLastLinkageFPR(p-1);

          // Preserved regs, vmthread last
          //
          for (i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
             {
             reg = (TR::RealRegister::RegNum)i;
             if (linkage->getPreserved(reg))
                p = self()->addGlobalReg(reg, p);
             }

           self()->setLastGlobalFPRRegisterNumber(p-1);

           // initGlobalVectorRegisterMap sets first/last global grns and overlapped grns
           if (_cg->getSupportsVectorRegisters())
              p = self()->initGlobalVectorRegisterMap(p);

          self()->setLastGlobalVRFRegisterNumber(p-1);

          for (int32_t i = 0; i < p; i++)
             {
             if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getCAAPointerRegister())
                self()->setGlobalCAARegisterNumber(i);
             if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getEnvironmentPointerRegister())
                self()->setGlobalEnvironmentRegisterNumber(i);
             if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getParentDSAPointerRegister())
                self()->setGlobalParentDSARegisterNumber(i);
             if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getEntryPointRegister())
                self()->setGlobalEntryPointRegisterNumber(i);
             if (_globalRegisterNumberToRealRegisterMap[i] == linkage->getReturnAddressRegister())
                self()->setGlobalReturnAddressRegisterNumber(i);
             }
          }

      self()->setLastRealRegisterGlobalRegisterNumber(p-1);
      self()->setLastGlobalCCRRegisterNumber(p-1);

      return _globalRegisterNumberToRealRegisterMap;
      }
   // Initialize the array
   //   _globalRegisterNumberToRealRegisterMap = new uint32_t[NUM_S390_GPR+NUM_S390_FPR]; // Make room for max num GPRs + FPRs

   // GPRs

   self()->setLastVolatileNonLinkGPR(0);

   _globalRegisterNumberToRealRegisterMap[0] = TR::RealRegister::GPR3;     // volatile and 3rd param
   _globalRegisterNumberToRealRegisterMap[1] = TR::RealRegister::GPR2;     // volatile and 3rd param
   _globalRegisterNumberToRealRegisterMap[2] = TR::RealRegister::GPR1;     // volatile and 3rd param
   self()->setLastLinkageGPR(2);
   self()->setFirstGlobalGPRRegisterNumber(3);                                         // Index of first global GPR

   // Global register 3 will be assigned to GPR6 if dynamic litpool was run
   // in OMR::Z::Machine::releaseLiteralPoolRegister()
   _globalRegisterNumberToRealRegisterMap[GLOBAL_REG_FOR_LITPOOL] = (uint32_t) (-1);            // preserved

   static char * noGraFIX= feGetEnv("TR_NOGRAFIX");
   // Exclude GPR7 if we are not on Freeway+ hardware
   if ( !noGraFIX                                                     &&
        !comp->getOption(TR_DisableLongDispStackSlot)          &&
        _cg->getExtCodeBaseRegisterIsFree()
      )
      {
      _globalRegisterNumberToRealRegisterMap[4] = TR::RealRegister::GPR7;  // preserved
      }
   else
      {
      _globalRegisterNumberToRealRegisterMap[4] = (uint32_t) (-1);            // preserved
      }

   _globalRegisterNumberToRealRegisterMap[5] = TR::RealRegister::GPR8;     // preserved
   _globalRegisterNumberToRealRegisterMap[6] = TR::RealRegister::GPR9;     // preserved
   _globalRegisterNumberToRealRegisterMap[7] = TR::RealRegister::GPR10;    // preserved
   _globalRegisterNumberToRealRegisterMap[8] = TR::RealRegister::GPR11;    // preserved -- non-Java may lock
   _globalRegisterNumberToRealRegisterMap[9] = TR::RealRegister::GPR12;    // preserved -- non-Java may lock

   // Access Registers
   self()->setFirstGlobalAccessRegisterNumber(10);
   _globalRegisterNumberToRealRegisterMap[10] = (uint32_t) (-1);              // TR::RealRegister::AR0; locked on zLinux
   _globalRegisterNumberToRealRegisterMap[11] = (uint32_t) (-1);              // TR::RealRegister::AR1; locked on zLinux
   // /// /// Disable and test on only on ZOS where the other ARs are protected by system linkage
   _globalRegisterNumberToRealRegisterMap[12] = (uint32_t) (-1);              // TR::RealRegister::AR2;
   _globalRegisterNumberToRealRegisterMap[13] = (uint32_t) (-1);              // TR::RealRegister::AR3;
   _globalRegisterNumberToRealRegisterMap[14] = (uint32_t) (-1);              // TR::RealRegister::AR4;
   _globalRegisterNumberToRealRegisterMap[15] = (uint32_t) (-1);              // TR::RealRegister::AR5;
   _globalRegisterNumberToRealRegisterMap[16] = (uint32_t) (-1);              // TR::RealRegister::AR6;
   _globalRegisterNumberToRealRegisterMap[17] = (uint32_t) (-1);              // TR::RealRegister::AR7;
   // /// ///

   _globalRegisterNumberToRealRegisterMap[18] = TR::RealRegister::AR8;      // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[19] = (uint32_t) (-1);               // TR::RealRegister::AR9;      // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[20] = (uint32_t) (-1);               // TR::RealRegister::AR10;     // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[21] = (uint32_t) (-1);               // TR::RealRegister::AR11;     // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[22] = (uint32_t) (-1);               // TR::RealRegister::AR12;     // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[23] = (uint32_t) (-1);               // TR::RealRegister::AR13;     // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[24] = (uint32_t) (-1);               // TR::RealRegister::AR14;     // preserved ZOS
   _globalRegisterNumberToRealRegisterMap[25] = (uint32_t) (-1);               // TR::RealRegister::AR15;     // preserved ZOS
   self()->setLastGlobalAccessRegisterNumber(25);

   self()->setLastGlobalGPRRegisterNumber(25);        // Index of last global GPR
   self()->setLast8BitGlobalGPRRegisterNumber(25);    // Index of last global 8bit Reg

   // additional (forced) restricted regs.
   // Similar code in TR_S390PrivateLinkage::initS390RealRegisterLinkage() for RA

   for (int32_t i = self()->getFirstGlobalGPRRegisterNumber(); i < self()->getLastGlobalGPRRegisterNumber(); ++i)
      {
      uint32_t regReal = _globalRegisterNumberToRealRegisterMap[i];
      if (self()->isRestrictedReg((TR::RealRegister::RegNum) regReal))
         {
         _globalRegisterNumberToRealRegisterMap[i] = (uint32_t) (-1);
         }
      }

   // Disable GRA Access Regs
   //
   if (
         TR::Compiler->target.is64Bit()                          ||
        !comp->getOption(TR_Enable390AccessRegs)
      )
      {
      for (int32_t i = self()->getFirstGlobalAccessRegisterNumber(); i <= self()->getLastGlobalAccessRegisterNumber(); ++i)
         {
         _globalRegisterNumberToRealRegisterMap[i] = (uint32_t) (-1);
         }
      }

   // FPRs
   _globalRegisterNumberToRealRegisterMap[26] = TR::RealRegister::FPR7;  // volatile float
   _globalRegisterNumberToRealRegisterMap[27] = TR::RealRegister::FPR5;  // volatile float
   _globalRegisterNumberToRealRegisterMap[28] = TR::RealRegister::FPR3;  // volatile float
   _globalRegisterNumberToRealRegisterMap[29] = TR::RealRegister::FPR1;  // volatile float
   self()->setLastVolatileNonLinkFPR(29);

#if defined(ENABLE_PRESERVED_FPRS)
   setLastVolatileNonLinkFPR(29);

   _globalRegisterNumberToRealRegisterMap[30] = TR::RealRegister::FPR6;  // volatile and 4th param float
   _globalRegisterNumberToRealRegisterMap[31] = TR::RealRegister::FPR4;  // volatile and 3rd param float
   _globalRegisterNumberToRealRegisterMap[32] = TR::RealRegister::FPR2;  // volatile and 2nd param float
   _globalRegisterNumberToRealRegisterMap[33] = TR::RealRegister::FPR0;  // volatile and 1st param float
   setLastLinkageFPR(33);

   _globalRegisterNumberToRealRegisterMap[34] = TR::RealRegister::FPR15;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[35] = TR::RealRegister::FPR14;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[36] = TR::RealRegister::FPR13;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[37] = TR::RealRegister::FPR12;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[38] = TR::RealRegister::FPR11;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[39] = TR::RealRegister::FPR10;  // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[40] = TR::RealRegister::FPR9;   // preserved float or vector
   _globalRegisterNumberToRealRegisterMap[41] = TR::RealRegister::FPR8;   // preserved float or vector
#else
   _globalRegisterNumberToRealRegisterMap[30] = TR::RealRegister::FPR15;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[31] = TR::RealRegister::FPR14;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[32] = TR::RealRegister::FPR13;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[33] = TR::RealRegister::FPR12;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[34] = TR::RealRegister::FPR11;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[35] = TR::RealRegister::FPR10;  // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[36] = TR::RealRegister::FPR9;   // volatile float or vector
   _globalRegisterNumberToRealRegisterMap[37] = TR::RealRegister::FPR8;   // volatile float or vector

   self()->setLastVolatileNonLinkFPR(37);

   _globalRegisterNumberToRealRegisterMap[38] = TR::RealRegister::FPR6;  // volatile and 4th param float
   _globalRegisterNumberToRealRegisterMap[39] = TR::RealRegister::FPR4;  // volatile and 3rd param float
   _globalRegisterNumberToRealRegisterMap[40] = TR::RealRegister::FPR2;  // volatile and 2nd param float
   _globalRegisterNumberToRealRegisterMap[41] = TR::RealRegister::FPR0;  // volatile and 1st param float
   self()->setLastLinkageFPR(41);
#endif

   self()->setLastRealRegisterGlobalRegisterNumber(41);

   self()->setLastGlobalFPRRegisterNumber(41);        // Index of last global FPR
   self()->setLastGlobalCCRRegisterNumber(41);        // Index of last global CCR

   // reserved
   _globalRegisterNumberToRealRegisterMap[42] = (uint32_t) (-1);   // reserved
   _globalRegisterNumberToRealRegisterMap[43] = (uint32_t) (-1);   // reserved
   _globalRegisterNumberToRealRegisterMap[44] = (uint32_t) (-1);   // reserved
   _globalRegisterNumberToRealRegisterMap[45] = (uint32_t) (-1);   // reserved
   _globalRegisterNumberToRealRegisterMap[46] = (uint32_t) (-1);   // reserved
   _globalRegisterNumberToRealRegisterMap[47] = (uint32_t) (-1);   // reserved

   uint32_t vectorOffset = 48;
   self()->initGlobalVectorRegisterMap(vectorOffset);

   return 0;
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
   if (!_cg->getSupportsVectorRegisters() && !TR::comp()->getOption(TR_DisableVectorRegGRA))
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
   const bool useEntireRegFile = ((hideLowerHalf == NULL) && _cg->getSupportsVectorRegisters());

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

   // Similar to HPR/GPR overlap, FPR and VRF will have same 'lastOverlapping' grn.
   self()->setLastGlobalFPRRegisterNumber(self()->getLastOverlappedGlobalVRFRegisterNumber());

   if (traceVectorGRN)
      {
      printf("Java func: %s func: %s\n", TR::comp()->getCurrentMethod()->nameChars(), __FUNCTION__);
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


void
OMR::Z::Machine::lockGlobalRegister(int32_t globalRegisterTableIndex)
   {
   TR::Compilation *comp = TR::comp();
   if (comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      _globalRegisterNumberToRealRegisterMap[globalRegisterTableIndex] = (uint32_t) (-1);
      }
   else
      {
      // TODO: make sure this method is not called without TR_DisableRegisterPressureSimulation
      // TR_ASSERTC( false,comp, "lockGlobalRegister() does not work with new pickRegister\n");
      }
   }

void
OMR::Z::Machine::releaseGlobalRegister(int32_t globalRegisterTableIndex, TR::RealRegister::RegNum gReg)
   {
   _globalRegisterNumberToRealRegisterMap[globalRegisterTableIndex] = gReg;
   }

int
OMR::Z::Machine::findGlobalRegisterIndex(TR::RealRegister::RegNum gReg)
   {
   int32_t index = -1;
   int32_t last = self()->getLastGlobalCCRRegisterNumber();
   for (int32_t i = 0; i < last; i++)
      {
      if (_globalRegisterNumberToRealRegisterMap[i] == gReg)
         {
         index = i;
         break;
         }
      }
   return index;
   }

// call this if optimizer run TR_DynamicLiteralPool pass
void
OMR::Z::Machine::releaseLiteralPoolRegister()
   {
   TR::Compilation *comp = TR::comp();
   if (comp->getOption(TR_DisableRegisterPressureSimulation))
      {
      _globalRegisterNumberToRealRegisterMap[GLOBAL_REG_FOR_LITPOOL] = TR::RealRegister::GPR6;
      }
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalAccessRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstGlobalRegisterNumber(TR_AR,reg);
   return _firstGlobalAccessRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalAccessRegisterNumber(TR_GlobalRegisterNumber reg)
    {
    self()->setLastGlobalRegisterNumber(TR_AR,reg);
    return _lastGlobalAccessRegisterNumber = reg;
    }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastGlobalRegisterNumber(TR_GPR,reg);
   return _lastGlobalGPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalHPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastGlobalRegisterNumber(TR_HPR,reg);
   return _lastGlobalHPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalGPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstGlobalRegisterNumber(TR_GPR,reg);
   return _firstGlobalGPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalHPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstGlobalRegisterNumber(TR_HPR,reg);
   return _firstGlobalHPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstGlobalRegisterNumber(TR_FPR, reg);
   self()->setFirstOverlappedGlobalFPRRegisterNumber(reg);
   return _firstGlobalFPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalFPRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastGlobalRegisterNumber(TR_FPR,reg);
   self()->setLastOverlappedGlobalFPRRegisterNumber(reg);
   return _lastGlobalFPRRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setFirstGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setFirstGlobalRegisterNumber(TR_VRF,reg);
   return _firstGlobalVRFRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalVRFRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastGlobalRegisterNumber(TR_VRF,reg);
   return _lastGlobalVRFRegisterNumber = reg;
   }

TR_GlobalRegisterNumber
OMR::Z::Machine::setLastGlobalCCRRegisterNumber(TR_GlobalRegisterNumber reg)
   {
   self()->setLastGlobalRegisterNumber(TR_CCR,reg);
   return _lastGlobalCCRRegisterNumber=reg;
   }

TR::Register *
OMR::Z::Machine::getAccessRegisterFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg)
  {
  if (
       reg >= self()->getFirstGlobalAccessRegisterNumber()      &&
       reg <= self()->getLastGlobalAccessRegisterNumber()       &&
       _globalRegisterNumberToRealRegisterMap[reg] >= 0
     )
     {
     return _registerFile[_globalRegisterNumberToRealRegisterMap[reg]];
     }

  return NULL;
  }

TR::Register *
OMR::Z::Machine::getGPRFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      if (
          reg >= self()->getFirstGlobalGPRRegisterNumber() &&
          reg <= self()->getFirstGlobalHPRRegisterNumber() &&
          _globalRegisterNumberToRealRegisterMap[reg] >= 0
          )
         {
         return _registerFile[_globalRegisterNumberToRealRegisterMap[reg]];
         }

      return NULL;
      }

TR::Register *
OMR::Z::Machine::getHPRFromGlobalRegisterNumber(TR_GlobalRegisterNumber reg)
      {
      if (
          reg >= self()->getFirstGlobalHPRRegisterNumber() &&
          reg <= self()->getLastGlobalGPRRegisterNumber() &&
          _globalRegisterNumberToRealRegisterMap[reg] >= 0
          )
         {
         return _registerFile[_globalRegisterNumberToRealRegisterMap[reg]];
         }

      return NULL;
      }

// Register Association ////////////////////////////////////////////
void
OMR::Z::Machine::setRegisterWeightsFromAssociations()
   {
   // *this    swipeable for debugging purposes
   TR::Linkage * linkage = _cg->getS390Linkage();
   int32_t first = TR::RealRegister::FirstGPR;
   TR::Compilation *comp = TR::comp();
   int32_t last = TR::RealRegister::LastAssignableVRF;
   if (_cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      last = TR::RealRegister::LastHPR;

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
   // *this    swipeable for debugging purposes
   TR::Compilation *comp = TR::comp();
   int32_t last = TR::RealRegister::LastAssignableVRF;
   TR::RegisterDependencyConditions * associations;

   if (_cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      int32_t lastHPR = last + TR::RealRegister::LastHPR - TR::RealRegister::FirstHPR;
      associations = new (_cg->trHeapMemory(), TR_MemoryBase::RegisterDependencyConditions) TR::RegisterDependencyConditions(0, lastHPR, _cg);
      }
   else
      {
      associations = new (_cg->trHeapMemory(), TR_MemoryBase::RegisterDependencyConditions) TR::RegisterDependencyConditions(0, last, _cg);
      }
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

   if (_cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      for (int32_t i = TR::RealRegister::FirstHPR; i < TR::RealRegister::LastHPR+1; i++)
         {
         TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum) (i);
         associations->addPostCondition(self()->getVirtualAssociatedWithReal(regNum), regNum);
         }
      }


   TR::Instruction *cursor1 = new (_cg->trHeapMemory(), TR_MemoryBase::S390Instruction) TR::Instruction(cursor, TR::InstOpCode::ASSOCREGS, associations, self()->cg());

   if (cursor == comp->getAppendInstruction())
      {
      comp->setAppendInstruction(cursor->getNext());
      }
   }

TR::Register *
OMR::Z::Machine::setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register * virtReg)
   {
   if ((regNum == TR::RealRegister::ArGprPair) ||
      (regNum == TR::RealRegister::ArOfArGprPair) ||
      (regNum == TR::RealRegister::GprOfArGprPair))
      {
      virtReg->setAssociation(regNum);
      return NULL;
      }

   if ((regNum == TR::RealRegister::EvenOddPair) ||
      (regNum == TR::RealRegister::LegalEvenOfPair) ||
      (regNum == TR::RealRegister::LegalOddOfPair))
      {
      virtReg->setAssociation(regNum);
      return NULL;
      }
   else if (TR::RealRegister::getBitMask(regNum) == 0)
      {
      virtReg->setAssociation(TR::RealRegister::NoReg);
      return NULL;
      }

   return _registerAssociations[regNum] = virtReg;
   }

/**
 * Longer term, once we clean up lit pool / extended lit pool regs and
 * arbitrary usage of other regs, we can integrate this better, but for now,
 * it is just a simple list of regs that are known to be 'safe'
 */
bool
OMR::Z::Machine::isRestrictedReg(TR::RealRegister::RegNum reg)
   {
   static const TR::RealRegister::RegNum regList[] =
      {
      TR::RealRegister::GPR9,
      TR::RealRegister::GPR10,
      TR::RealRegister::GPR11,
      TR::RealRegister::GPR12,
      };
   TR::Compilation *comp = TR::comp();
   static const int32_t regListSize = (sizeof(regList) / sizeof(TR::RealRegister::RegNum));

   int32_t numRestrictedRegs = comp->getOptions()->getNumRestrictedGPRs();
   if (numRestrictedRegs < 0 || numRestrictedRegs > regListSize)
      {
      static bool printed = false;
      #ifdef DEBUG
      if (!printed)
         {
         fprintf(stderr, "Invalid value for numRestrictedRegs or on-demand lit pool is disabled. Needs to range from 0 to %d\n",
            regListSize);
         printed = true;
         }
      #endif
      return false;
      }

   for (int32_t i = 0; i < numRestrictedRegs; ++i)
      {
      if (regList[i] == reg)
         {
         return true;
         }
      }

   return false;
   }

bool
OMR::Z::Machine::supportLockedRegisterAssignment()
   {
   return false; // TODO : Identity needs folding
   }

TR::RealRegister *
OMR::Z::Machine::getRegisterFile(int32_t i)
   {
   return  _registerFile[i];
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
      _registerAssignedHighSnapShot[i] = _registerFile[i]->getAssignedHigh();
      _registerWeightSnapShot[i] = _registerFile[i]->getWeight();
      _containsHPRSpillSnapShot[i] = false;
      if (_assignedRegisterSnapShot[i] && _assignedRegisterSnapShot[i]->getAssignedRegister() == NULL)
         {
         _cg->traceRegisterAssignment("\nOOL: %R : %R", _registerFile[i], _assignedRegisterSnapShot[i]);
         TR_ASSERT(_registerFile[i]->isHighWordRegister(), "OOL: HPR spill?? %d", i);
         _containsHPRSpillSnapShot[i] = true;
         }
      }
   /* this should not change
   for (int32_t i=0;i<(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR);i++)
      {
      _globalRegisterNumberToRealRegisterMapSnapShot[i] = _globalRegisterNumberToRealRegisterMap[i];
      }
   */
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
      //_registerFile[i]->setAssignedHigh(_registerAssignedHighSnapShot[i]);
      // make sure to double link virt - real reg if assigned
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned && !_containsHPRSpillSnapShot[i])
         {
         //_cg->traceRegisterAssignment("\nOOL: restoring %R : %R", _registerFile[i], _registerFile[i]->getAssignedRegister());
         if (!_registerFile[i]->getAssignedRegister()->is64BitReg() || !_registerFile[i]->isHighWordRegister())
            {
            _registerFile[i]->getAssignedRegister()->setAssignedRegister(_registerFile[i]);
            }
         }

      if (_containsHPRSpillSnapShot[i])
         {
         _registerFile[i]->getAssignedRegister()->setSpilledToHPR(true);
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
   /*
   for (int32_t i=0;i<(NUM_S390_GPR+NUM_S390_FPR+NUM_S390_AR);i++)
      {
      _globalRegisterNumberToRealRegisterMap[i]=_globalRegisterNumberToRealRegisterMapSnapShot[i];
      }
   */
   }

TR::RegisterDependencyConditions * OMR::Z::Machine::createDepCondForLiveGPRs(TR::list<TR::Register*> *spilledRegisterList)
   {
   int32_t i, c=0;
   // Calculate number of register dependencies required. This step is not really necessary, but
   // it is space conscious
   //
   TR::Compilation *comp = TR::comp();
   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastVRF; i = ((i == TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstVRF : i+1) )
      {
      TR::RealRegister *realReg = self()->getS390RealRegister(i);

      TR_ASSERT(realReg->getState() == TR::RealRegister::Assigned ||
              realReg->getState() == TR::RealRegister::Free ||
              realReg->getState() == TR::RealRegister::Locked,
              "cannot handle realReg state %d, (block state is %d)\n",realReg->getState(),TR::RealRegister::Blocked);

      if (realReg->getState() == TR::RealRegister::Assigned)
         c++;
      }

   c += spilledRegisterList ? spilledRegisterList->size() : 0;

   if (_cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
      {
      for (i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; i++)
         {
         if (self()->getS390RealRegister(i)->getState() == TR::RealRegister::Assigned && self()->getS390RealRegister(i)->getAssignedRegister())
            {
            if (self()->getS390RealRegister(i)->getAssignedRegister() != self()->getS390RealRegister(i)->getLowWordRegister()->getAssignedRegister())
               c++;
            // if a HPR is assigned to a virtReg but a virtReg is not assigned to HPR, we must have spilled the virtReg to HPR
            if (!self()->getS390RealRegister(i)->getAssignedRegister()->getAssignedRegister())
               c++;
            }
         }
      }

   TR::RegisterDependencyConditions *deps = NULL;

   if (c)
      {
      deps = new (_cg->trHeapMemory(), TR_MemoryBase::RegisterDependencyConditions) TR::RegisterDependencyConditions(0, c, _cg);
      for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastVRF; i = ((i==TR::RealRegister::LastAssignableGPR)? TR::RealRegister::FirstVRF : i+1))
         {
         TR::RealRegister *realReg = self()->getS390RealRegister(i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register *virtReg = realReg->getAssignedRegister();
            //if (!spilledRegisterList || !spilledRegisterList->find(virtReg))
            //{
                //TR_ASSERTC(comp, !spilledRegisterList || !spilledRegisterList->find(virtReg),
                // "a register should not be in both an assigned state and in the spilled list\n");
            deps->addPostCondition(virtReg, realReg->getRegisterNumber());
               //}

            //virtReg->incTotalUseCount();
            virtReg->incFutureUseCount();
            }
         }
      if (_cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         {
         for (i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; i++)
            {
            if (self()->getS390RealRegister(i)->getState() == TR::RealRegister::Assigned && self()->getS390RealRegister(i)->getAssignedRegister())
               {
               if(self()->getS390RealRegister(i)->getAssignedRegister() != self()->getS390RealRegister(i)->getLowWordRegister()->getAssignedRegister())
                  {
                  deps->addPostCondition(self()->getS390RealRegister(i)->getAssignedRegister(),self()->getS390RealRegister(i)->getRegisterNumber());
                  self()->getS390RealRegister(i)->getAssignedRegister()->incFutureUseCount();
                  }
               // if a HPR is assigned to a virtReg but a virtReg is not assigned to HPR, we must have spilled the virtReg to HPR
               if(!self()->getS390RealRegister(i)->getAssignedRegister()->getAssignedRegister())
                  {
                  // this is not a conflicting dependency rather a directive to tell RA to mark the virt reg as a HPR spill
                  deps->addPostCondition(self()->getS390RealRegister(i), TR::RealRegister::SpilledReg);
                  }
               }
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
