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

#include <stdint.h>                                   // for int32_t, etc
#include <stdio.h>                                    // for NULL, printf, etc
#include <string.h>                                   // for memcpy, memset
#include <algorithm>                                  // for std::find
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"                  // for CodeGenerator
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                       // for TR::IO::fprintf, etc
#include "codegen/Instruction.hpp"                    // for Instruction
#include "codegen/Linkage.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                        // for MachineBase, etc
#include "codegen/Machine_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                   // for RealRegister, etc
#include "codegen/Register.hpp"                       // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/RegisterUsage.hpp"                  // for RegisterUsage
#include "codegen/TreeEvaluator.hpp"                  // for TreeEvaluator
#include "compile/Compilation.hpp"                    // for Compilation, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"                // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                                 // for IO
#include "env/ObjectModel.hpp"                        // for ObjectModel
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                               // for Block
#include "il/DataTypes.hpp"                           // for DataTypes, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                               // for ILOpCode
#include "il/Node.hpp"                                // for Node
#include "il/Symbol.hpp"                              // for Symbol
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "infra/List.hpp"                             // for List, etc
#include "ras/Debug.hpp"                              // for TR_DebugBase
#include "x/codegen/OutlinedInstructions.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                       // for TR_X86OpCodes, etc
#include "x/codegen/X86Register.hpp"

extern bool existsNextInstructionToTestFlags(TR::Instruction *startInstr,
                                             uint8_t         testMask);

#define IA32_REGISTER_HEAVIEST_WEIGHT               0x0000ffff
#define IA32_REGISTER_INTERFERENCE_WEIGHT           0x00008000
#define IA32_REGISTER_INITIAL_PRESERVED_WEIGHT      0x00001000
#define IA32_REGISTER_ASSOCIATED_WEIGHT             0x00000800
#define IA32_REGISTER_PLACEHOLDER_WEIGHT            0x00000100
#define IA32_REGISTER_BASIC_WEIGHT                  0x00000080

#define IA32_REGISTER_PRESERVED_WEIGHT              0x00000002
#define IA32_BYTE_REGISTER_INTERFERENCE_WEIGHT      0x00000004

// Distance (in number of instructions) from the current instruction to search
// while looking for a spill candidate.
//
#define FREE_BEST_REGISTER_SEARCH_DISTANCE 800

// Distance (in number of instructions) from the current instruction before a
// register will be given priority as a spill candidate.
//
#define FREE_BEST_REGISTER_MINIMUM_CANDIDATE_DISTANCE 4

// Candidate register types for freeBestGPRegister
//
enum FreeBestRegisterCandidateTypes
   {
   BestNonInterferingDiscardableConstant = 0,
   BestMayInterfereDiscardableConstant,
   BestInterferingDiscardableConstant,
   BestNonInterferingDiscardableMemory,
   BestMayInterfereDiscardableMemory,
   BestInterferingDiscardableMemory,
   NonInterferingTarget,
   InterferingTarget,
   BestNonInterfering,
   BestMayInterfere,
   BestInterfering,
   NumBestRegisters
   };

// Significant distances (in number of instructions) between candidate register
// types in freeBestGPRegister
//
static const int16_t freeBestRegisterSignificantDistances[NumBestRegisters][NumBestRegisters] =
   {
      { 0,  5, 10, 20, 20, 20, 40, 40, 60, 60, 60 }, // BestNonInterferingDiscardableConstant
      { 0,  0, 10, 20, 20, 20, 40, 40, 60, 60, 60 }, // BestMayInterfereDiscardableConstant
      { 0,  0,  0, 10, 20, 20, 30, 30, 50, 50, 50 }, // BestInterferingDiscardableConstant
      { 0,  0,  0,  0,  5, 10, 40, 40, 60, 60, 60 }, // BestNonInterferingDiscardableMemory
      { 0,  0,  0,  0,  0, 10, 40, 40, 60, 60, 60 }, // BestMayInterfereDiscardableMemory
      { 0,  0,  0,  0,  0,  0, 30, 30, 50, 50, 50 }, // BestInterferingDiscardableMemory
      { 0,  0,  0,  0,  0,  0,  0, 10, 40, 40, 90 }, // NonInterferingTarget
      { 0,  0,  0,  0,  0,  0,  0,  0, 10, 10, 90 }, // InterferingTarget
      { 0,  0,  0,  0,  0,  0,  0,  0,  0,  5, 20 }, // BestNonInterfering
      { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 20 }, // BestMayInterfere
      { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 }  // BestInterfering
   };


bool existsNextInstructionToTestFlags(TR::Instruction *startInstr,
                                      uint8_t         testMask)
   {
   if (!startInstr)
      return false;

   TR::Instruction  *cursor = startInstr;

   do
      {
      cursor = cursor->getNext();

      if (!cursor) break;

      // Cursor instruction tests an eflag that is set exclusively by our testMask.
      //
      if (cursor->getOpCode().getTestedEFlags() & testMask)
         {
         return true;
         }

      // Mask out those eflags that are modified by this instruction.
      //
      testMask = testMask & ~(cursor->getOpCode().getModifiedEFlags());
      }
   while (testMask &&
          cursor->getOpCodeValue() != LABEL   &&
          cursor->getOpCodeValue() != RET     &&
          cursor->getOpCodeValue() != RETImm2 &&
          cursor->getOpCodeValue() != ReturnMarker &&
          !cursor->getOpCode().isBranchOp());

   return false;
   }

static bool registersMayOverlap(TR::Register *reg1, TR::Register *reg2)
   {
   if (reg1->getStartOfRange() && reg2->getEndOfRange() &&
       reg1->getStartOfRange()->getIndex() >= reg2->getEndOfRange()->getIndex())
      return false;
   if (reg1->getEndOfRange() && reg2->getStartOfRange() &&
       reg1->getEndOfRange()->getIndex() <= reg2->getStartOfRange()->getIndex())
      return false;
   return true;
   }


OMR::X86::Machine::Machine
   (
   uint8_t numIntRegs,
   uint8_t numFPRegs,
   TR::CodeGenerator *cg,
   TR::RealRegister **registerFile,
   TR::Register **registerAssociations,
   uint8_t numGlobalGPRs,
   uint8_t numGlobal8BitGPRs,
   uint8_t numGlobalFPRs,
   TR::Register **xmmGlobalRegisters,
   uint32_t *globalRegisterNumberToRealRegisterMap
   )
   : OMR::Machine(cg),
   _registerFile(registerFile),
   _registerAssociations(registerAssociations),
   _numGlobalGPRs(numGlobalGPRs),
   _numGlobal8BitGPRs(numGlobal8BitGPRs),
   _numGlobalFPRs(numGlobalFPRs),
   _xmmGlobalRegisters(xmmGlobalRegisters),
   _globalRegisterNumberToRealRegisterMap(globalRegisterNumberToRealRegisterMap),
   _spilledRegistersList(NULL),
   _numGPRs(numIntRegs)
   {
   self()->initializeFPStackRegisterFile();
   _fpTopOfStack = TR_X86FPStackRegister::fpStackEmpty;
   self()->resetFPStackRegisters();
   self()->resetXMMGlobalRegisters();

   for (int i=0; i<TR::NumTypes; i++)
      {
      _dummyLocal[i] = NULL;
      }

   self()->clearRegisterAssociations();
   }

void OMR::X86::Machine::resetXMMGlobalRegisters()
   {
   for (int32_t i = 0; i < TR::RealRegister::NumXMMRegisters; i++)
      self()->setXMMGlobalRegister(i, NULL);
   }

TR_Debug *OMR::X86::Machine::getDebug()
   {
   return self()->cg()->getDebug();
   }

int32_t OMR::X86::Machine::getGlobalReg(TR::RealRegister::RegNum reg)
   {
   for (int32_t i = 0; i < self()->getLastGlobalFPRRegisterNumber(); i++)
      {
      if (_globalRegisterNumberToRealRegisterMap[i] == reg)
         return i;
      }
   return -1;
   }

TR::RealRegister *
OMR::X86::Machine::findBestFreeGPRegister(TR::Instruction   *currentInstruction,
                                      TR::Register      *virtReg,
                                      TR_RegisterSizes  requestedRegSize,
                                      bool              considerUnlatched)
   {
   int first, last, i;

   struct TR_Candidate
      {
      TR::Register            *_virtual;
      TR::RealRegister::RegNum _real;
      } candidates[16]; // TODO:AMD64: Should be max number of regs of any one kind
   int32_t      numCandidates = 0;

   TR_ASSERT( (virtReg && (virtReg->getKind() == TR_GPR || virtReg->getKind() == TR_FPR || virtReg->getKind() == TR_VRF)),
           "OMR::X86::Machine::findBestFreeGPRegister() ==> Unexpected register kind!" );

   bool useRegisterAssociations = self()->cg()->enableRegisterAssociations() ? true : false;
   bool useRegisterInterferences = self()->cg()->enableRegisterInterferences() ? true : false;
   bool useRegisterWeights = self()->cg()->enableRegisterWeights() ? true : false;

   if (useRegisterAssociations &&
       virtReg->getAssociation() != TR::RealRegister::NoReg &&
       self()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)virtReg->getAssociation()) == virtReg &&
       (_registerFile[virtReg->getAssociation()]->getAssignedRegister() == NULL ||
       (considerUnlatched && _registerFile[virtReg->getAssociation()]->getState() == TR::RealRegister::Unlatched)))
      {
      if (_registerFile[virtReg->getAssociation()]->getState() != TR::RealRegister::Locked)
         {
         if (requestedRegSize != TR_ByteReg || virtReg->getAssociation() <= TR::RealRegister::Last8BitGPR)
            {
            self()->cg()->setRegisterAssignmentFlag(TR_ByAssociation);
            return _registerFile[virtReg->getAssociation()];
            }
         }
      }

   switch (requestedRegSize)
      {
      case TR_ByteReg:
         first = TR::RealRegister::FirstGPR;
         last  = TR::RealRegister::Last8BitGPR;
         break;
      case TR_HalfWordReg:
      case TR_WordReg:
         first = TR::RealRegister::FirstGPR;
         last  = TR::RealRegister::LastAssignableGPR;
         break;
      case TR_DoubleWordReg:
         first = TR::RealRegister::FirstMMXR;
         last  = TR::RealRegister::LastMMXR;
         break;
      case TR_QuadWordReg:
         first = TR::RealRegister::FirstXMMR;
         last  = TR::RealRegister::LastXMMR;
         break;
      default:
         TR_ASSERT(0, "unknown register size requested\n");
      }

   uint32_t                        weight;
   uint32_t                        bestWeightSoFar   = IA32_REGISTER_HEAVIEST_WEIGHT ;
   TR_RegisterMask                 interference      = virtReg->getInterference();
   TR_RegisterMask                 byteRegisterInterference = interference & 0x80000000;
   TR::RealRegister             *freeRegister      = NULL;
   const TR::X86LinkageProperties &linkageProperties = self()->cg()->getProperties();

   for (i = first; i <= last; i++)
      {
      // Don't consider registers that can't be assigned.
      //
      if (_registerFile[i]->getState() == TR::RealRegister::Locked)
         {
         self()->cg()->traceRegWeight(_registerFile[i], 0xdead1);
         interference >>= 1;
         continue;
         }

      TR::Register *associatedRegister = useRegisterAssociations ? self()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)i) : NULL;

      if (useRegisterWeights)
         {
         if ((linkageProperties.isPreservedRegister((TR::RealRegister::RegNum)i) || linkageProperties.isCalleeVolatileRegister((TR::RealRegister::RegNum)i)) &&
             _registerFile[i]->getWeight() == IA32_REGISTER_INITIAL_PRESERVED_WEIGHT)
            {
            if (associatedRegister)
               _registerFile[i]->setWeight(IA32_REGISTER_ASSOCIATED_WEIGHT);
            else if (_registerFile[i]->getHasBeenAssignedInMethod())
               _registerFile[i]->setWeight(IA32_REGISTER_BASIC_WEIGHT);
            }

         // Get the weight of the real register.
         //
         weight = _registerFile[i]->getWeight();

         // If the real register has an association whose live range does not
         // overlap with this one, make it very popular.
         //
         // Increase the weight if the register interferes with the virtual register.
         //
         if (associatedRegister && !registersMayOverlap(virtReg, associatedRegister))
            weight &= ~(IA32_REGISTER_ASSOCIATED_WEIGHT | IA32_REGISTER_PLACEHOLDER_WEIGHT);
         else if ((interference & 1))
            weight += IA32_REGISTER_INTERFERENCE_WEIGHT;

         // Add tie-breaking weights for preserved registers and for interference
         // with byte registers.
         //
         if (!(interference & 1))
            {
            if (byteRegisterInterference && i <= TR::RealRegister::Last8BitGPR)
               weight += IA32_BYTE_REGISTER_INTERFERENCE_WEIGHT;
            if (linkageProperties.isPreservedRegister((TR::RealRegister::RegNum)i))
               weight += IA32_REGISTER_PRESERVED_WEIGHT;
            }
         }
      else
         {
         weight = _registerFile[i]->getWeight();
         }

      if (((_registerFile[i]->getAssignedRegister() == NULL &&
            _registerFile[i]->getState() != TR::RealRegister::Blocked) ||
           (considerUnlatched &&
            _registerFile[i]->getState() == TR::RealRegister::Unlatched)))
         {
         self()->cg()->traceRegWeight(_registerFile[i], weight);

         if (weight < bestWeightSoFar)
            {
            freeRegister    = _registerFile[i];
            bestWeightSoFar = weight;
            numCandidates   = 0;
            }
         else if (weight == bestWeightSoFar &&
                  useRegisterInterferences &&
                  (weight & IA32_REGISTER_INTERFERENCE_WEIGHT))
            {
            TR::Register *r = self()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)i);
            if (r && (r->getAssociation() != TR::RealRegister::NoReg) &&
                _registerFile[i]->getState() == TR::RealRegister::Free)
               {
               candidates[numCandidates]._virtual = r;
               candidates[numCandidates++]._real  = (TR::RealRegister::RegNum)i;
               }
            }
         }
      else if (_registerFile[i]->getAssignedRegister() == NULL)
         self()->cg()->traceRegWeight(_registerFile[i], 0xdead2);
      else
         self()->cg()->traceRegWeight(_registerFile[i], 0xdead3);

      interference >>= 1;
      }

   // If there is more than one interfering candidate, choose the one that
   // is used farthest away.
   //
   if (useRegisterInterferences && (numCandidates > 1))
      {
      TR::Instruction  *cursor;
      int32_t             distance = 0;
      for (cursor = currentInstruction->getPrev();
           cursor && numCandidates > 1;
           cursor = cursor->getPrev())
         {
         if (cursor->getOpCodeValue() == PROCENTRY)
            break;

         if (cursor->getOpCodeValue() == ASSOCREGS)
            continue;

         for (i = 0; i < numCandidates; i++)
            {
            if (cursor->refsRegister(candidates[i]._virtual))
               {
               self()->cg()->traceRegInterference(virtReg, candidates[i]._virtual, distance);
               candidates[i] = candidates[--numCandidates];
               }
            }
         distance++;
         }
      freeRegister = _registerFile[candidates[0]._real];
      }

   return freeRegister;
   }


TR::RealRegister *OMR::X86::Machine::freeBestGPRegister(TR::Instruction           *currentInstruction,
                                                        TR::Register              *virtReg,
                                                        TR_RegisterSizes           requestedRegSize,
                                                        TR::RealRegister::RegNum   targetRegister,
                                                        bool                       considerVirtAsSpillCandidate)
   {
   TR::Register        *candidates[16]; // TODO:AMD64: Should be max number of regs of any one kind
   int                 numCandidates = 0;
   int                 first, last;
   TR::Instruction  *cursor;
   TR::Register        *bestRegisters[NumBestRegisters];
   int32_t             bestDistances[NumBestRegisters];
   TR::Register        *bestRegister = NULL;
   int32_t             bestDistance = -1;
   int32_t             bestType = -1;
   int32_t             distance = 1;
   TR_RegisterMask     interference = virtReg->getInterference();
   TR_RegisterMask     byteRegisterInterference = interference & 0x80000000;
   int32_t             interferes;
   int32_t             i, j;
   TR::RealRegister *realReg;

   TR::Compilation *comp = self()->cg()->comp();

   bool useRegisterInterferences = self()->cg()->enableRegisterInterferences() ? true : false;
   bool enableRematerialisation = self()->cg()->enableRematerialisation() ? true : false;

   TR_ASSERT(virtReg && (virtReg->getKind() == TR_GPR || virtReg->getKind() == TR_FPR || virtReg->getKind() == TR_VRF),
          "OMR::X86::Machine::freeBestGPRegister() ==> expecting to free GPRs or XMMRs only!");

   switch (requestedRegSize)
      {
      case TR_ByteReg:
         first = TR::RealRegister::FirstGPR;
         last  = TR::RealRegister::Last8BitGPR;
         break;
      case TR_HalfWordReg:
      case TR_WordReg:
         first = TR::RealRegister::FirstGPR;
         last = TR::RealRegister::LastAssignableGPR;
         break;
      case TR_DoubleWordReg:
         first = TR::RealRegister::FirstMMXR;
         last  = TR::RealRegister::LastMMXR;
         break;
      case TR_QuadWordReg:
         first = TR::RealRegister::FirstXMMR;
         last  = TR::RealRegister::LastXMMR;
         break;
      default:
         TR_ASSERT(0, "unknown register size requested\n");
      }

   char *bestRegisterTypes[NumBestRegisters] =
      {
      "non-interfering discardable (from constant) register",
      "may interfere discardable (from constant) byte register",
      "interfering discardable (from constant) register",
      "non-interfering discardable (from memory) register",
      "may interfere discardable (from memory) byte register",
      "interfering discardable (from memory) register",
      "non-interfering target register",
      "interfering target register",
      "non-interfering register",
      "may interfere byte register",
      "interfering register"
      };

   if (targetRegister)
      self()->cg()->traceRegisterAssignment("Spill candidates for %R (target %R):", virtReg, _registerFile[targetRegister]);
   else
      self()->cg()->traceRegisterAssignment("Spill candidates for %R:", virtReg);

   for (i = 0; i < NumBestRegisters; i++)
      {
      bestRegisters[i] = NULL;
      bestDistances[i] = 0;
      }

   // Identify all spillable candidates of the appropriate register type.
   //
   for (i = first; i <= last; i++)
      {
      // Don't consider registers that can't be assigned.
      //
      if (_registerFile[i]->getState() == TR::RealRegister::Locked)
         {
         continue;
         }

      realReg = self()->getX86RealRegister((TR::RealRegister::RegNum)i);

      if (realReg->getState() == TR::RealRegister::Assigned)
         {
         candidates[numCandidates++] = realReg->getAssignedRegister();
         if (targetRegister == i)
            {
            if (useRegisterInterferences && (interference & (1 << (i-1)))) // TODO:AMD64: Use the proper mask value
               bestRegisters[InterferingTarget] = realReg->getAssignedRegister();
            else
               bestRegisters[NonInterferingTarget] = realReg->getAssignedRegister();
            }
         }
      }

   if (numCandidates == 0)
      {
      // If we are trying to find a suitable spill register for a virtual that currently occupies a
      // register required in a register dependency, there may not be any suitable spill
      // candidates if all available real registers are requested in the register dependency.  In
      // such a case, where our virtual need not be in a register across the dependency, choose the
      // virtual itself as a spill candidate.
      //
      TR_ASSERT(considerVirtAsSpillCandidate,
              "freeBestGPRegister(): could not find any GPR spill candidates for %s\n", self()->getDebug()->getName(virtReg));

      bestRegister = virtReg;
      }

   // From all the spillable candidates identified, choose the most appropriate based on
   // its rematerialisation value and its proximity to the current instruction.
   //
   TR::RealRegister::RegNum registerNumber;
   for (cursor = currentInstruction->getPrev();
        cursor;
        cursor = cursor->getPrev())
      {
      if (cursor->getOpCodeValue() == PROCENTRY)
         break;

      if (numCandidates == 0)
         break;

      if (distance > FREE_BEST_REGISTER_SEARCH_DISTANCE)
         break;

      if (cursor->getOpCodeValue() == FENCE)
         {
         // Don't walk past the start of the super (extended) block.
         // This is primarily for the non-linear register assigner because
         // the linear RA won't even have values live beyond the start of the
         // super block
         TR::Node *node = cursor->getNode();
         if (node->getOpCodeValue() == TR::BBStart &&
             !node->getBlock()->isExtensionOfPreviousBlock())
            break;
         }

      if (cursor->getOpCodeValue() == FENCE ||
          cursor->getOpCodeValue() == FPREGSPILL)
         continue;

      for (i = 0; i < numCandidates; i++)
         {
         registerNumber = toRealRegister(candidates[i]->getAssignedRegister())->getRegisterNumber();

         // Look for case where we come across the dependency reference for
         // the target register and the desired real register is still one
         // of the remaining candidates.
         //
         if (virtReg->getAssociation() != TR::RealRegister::NoReg &&
             cursor->dependencyRefsRegister(virtReg))
            {
            for (j = 0; j < numCandidates; ++j)
               {
               realReg = toRealRegister(candidates[j]->getAssignedRegister());
               if (realReg->getRegisterNumber() == virtReg->getAssociation())
                  {
                  bestRegister = candidates[j];
                  self()->cg()->setRegisterAssignmentFlag(TR_ByAssociation);
                  goto done;  // Rude exit
                  }
               }
            }

         interferes = interference & (1 << (registerNumber-1)); // TODO:AMD64: Use the proper mask value

         if (cursor->refsRegister(candidates[i]))
            {
            if (distance > FREE_BEST_REGISTER_MINIMUM_CANDIDATE_DISTANCE)
               {
               TR_RematerializationInfo *info = candidates[i]->isDiscardable() ? candidates[i]->getRematerializationInfo() : NULL;

               if (enableRematerialisation && info && info->isActive())
                  {
                  if (info->isRematerializableFromMemory() || info->isRematerializableFromAddress())
                     {
                     if (interferes)
                        j = BestInterferingDiscardableMemory;
                     else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
                        j = BestMayInterfereDiscardableMemory;
                     else
                        j = BestNonInterferingDiscardableMemory;
                     }
                  else
                     {
                     TR_ASSERT(candidates[i]->getRematerializationInfo()->isRematerializableFromConstant(),
                            "freeBestGPRegister => unknown rematerialisable register type!");
                     if (interferes)
                        j = BestInterferingDiscardableConstant;
                     else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
                        j = BestMayInterfereDiscardableConstant;
                     else
                        j = BestNonInterferingDiscardableConstant;
                     }

                  if (debug("dumpRemat"))
                     {
                     diagnostic("---> Identified %s rematerialisation spill "
                                 "candidate %s at instruction %s in %s\n",
                                 self()->getDebug()->toString(info), self()->getDebug()->getName(candidates[i]),
                                 self()->getDebug()->getName(currentInstruction), comp->signature());
                     }
                  }
               else
                  {
                  if (useRegisterInterferences)
                     {
                     if (interferes)
                        j = BestInterfering;
                     else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
                        j = BestMayInterfere;
                     else
                        j = BestNonInterfering;
                     }
                  else
                     {
                     j = BestNonInterfering;
                     }
                  }

               bestRegisters[j] = candidates[i];
               bestDistances[j] = distance;
               bestType         = j;
               if (useRegisterInterferences && (bestRegisters[j] == bestRegisters[InterferingTarget]))
                  bestDistances[InterferingTarget] = distance;
               else if (bestRegisters[j] == bestRegisters[NonInterferingTarget])
                  bestDistances[NonInterferingTarget] = distance;
               }

            bestRegister = candidates[i];
            bestDistance = distance;
            candidates[i] = candidates[--numCandidates];
            }
         }
      distance++;
      }

   // Sort out the remaining candidates
   //
   for (i = numCandidates-1; i >= 0; i--)
      {
      registerNumber = toRealRegister(candidates[i]->getAssignedRegister())->getRegisterNumber();
      interferes = interference & (1 << (registerNumber-1)); // TODO:AMD64: Use the proper mask value

      TR_RematerializationInfo *info =
         candidates[i]->isDiscardable() ? candidates[i]->getRematerializationInfo() : NULL;

      if (enableRematerialisation && info && info->isActive())
         {
         if (candidates[i]->getRematerializationInfo()->isRematerializableFromMemory() ||
             candidates[i]->getRematerializationInfo()->isRematerializableFromAddress())
            {
            if (interferes)
               j = BestInterferingDiscardableMemory;
            else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
               j = BestMayInterfereDiscardableMemory;
            else
               j = BestNonInterferingDiscardableMemory;
            }
         else
            {
            if (interferes)
               j = BestInterferingDiscardableConstant;
            else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
               j = BestMayInterfereDiscardableConstant;
            else
               j = BestNonInterferingDiscardableConstant;
            }
         }
      else
         {
         if (useRegisterInterferences)
            {
            if (interferes)
               j = BestInterfering;
            else if (byteRegisterInterference && registerNumber <= TR::RealRegister::Last8BitGPR)
               j = BestMayInterfere;
            else
               j = BestNonInterfering;
            }
         else
            {
            j = BestNonInterfering;
            }
         }

      bestRegister = bestRegisters[j] = candidates[i];
      bestDistance = bestDistances[j] = distance;
      bestType     = j;
      if (useRegisterInterferences && bestRegisters[j] == bestRegisters[InterferingTarget])
         bestDistances[InterferingTarget] = distance;
      else if (bestRegisters[j] == bestRegisters[NonInterferingTarget])
         bestDistances[NonInterferingTarget] = distance;
      }

   if (bestType >= 0)
      {
      for (i = 0; i < NumBestRegisters; i++)
         {
         if (bestRegisters[i])
            self()->cg()->traceRegisterAssignment("   (best %s %R at distance %d)", bestRegisters[i], bestRegisterTypes[i], bestDistances[i]);
         }

      // See if there is a candidate with higher priority close enough to the
      // farthest candidate to be better.
      // Ignore candidates that are less than half the distance to the
      // farthest candidate.
      //
      for (i = 0; i < bestType; i++)
         {
         if (bestDistances[i] &&
             (bestDistance - bestDistances[i]) < freeBestRegisterSignificantDistances[i][bestType] &&
             (bestDistance - bestDistances[i]) < bestDistances[i])
            {
            bestRegister = bestRegisters[i];
            bestDistance = bestDistances[i];
            bestType = i;
            break;
            }
         }
      }

   done:

   // Set bestDiscardableRegister if the chosen register is discardable.
   //
   TR::Register *bestDiscardableRegister =
      (bestRegister->isDiscardable() &&
       bestRegister->getRematerializationInfo()->isActive()) ? bestRegister : NULL;

   TR::RealRegister         *best = toRealRegister(bestRegister->getAssignedRegister());
   TR::Instruction             *instr = NULL;

   if (enableRematerialisation && bestDiscardableRegister)
      {
      TR_RematerializationInfo *info = bestDiscardableRegister->getRematerializationInfo();

      if (info->isRematerializableFromMemory())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(info->getSymbolReference(), self()->cg());

         if (info->isIndirect())
            {
            TR_ASSERT(info->getBaseRegister()->getAssignedRegister(),
                   "base register %s of dependent rematerialisable register %s must be assigned\n",
                   self()->getDebug()->getName(info->getBaseRegister()),
                   self()->getDebug()->getName(bestDiscardableRegister));

            tempMR->setBaseRegister(info->getBaseRegister()->getAssignedRegister());
#ifdef DEBUG
            self()->cg()->incNumRematerializedIndirects();
#endif
            }
#ifdef DEBUG
         else if (tempMR->getSymbolReference().getSymbol()->isStatic())
            self()->cg()->incNumRematerializedStatics();
         else
            self()->cg()->incNumRematerializedLocals();
#endif

         if (info->getDataType() == TR_RematerializableFloat)
            {
            instr = generateRegMemInstruction(currentInstruction, MOVSSRegMem, best, tempMR, self()->cg());
#ifdef DEBUG
            self()->cg()->incNumRematerializedXMMRs();
#endif
            }
         else
            {
            instr = TR::TreeEvaluator::insertLoadMemory(0, best, tempMR, info->getDataType(), self()->cg(), currentInstruction);
            }
         }
      else if (info->isRematerializableFromAddress())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(info->getSymbolReference(), self()->cg());
         instr = generateRegMemInstruction(currentInstruction, LEARegMem(), best, tempMR, self()->cg());
#ifdef DEBUG
         self()->cg()->incNumRematerializedAddresses();
#endif
         }
      else
         {
         if (info->getDataType() == TR_RematerializableFloat)
            {
            TR::IA32ConstantDataSnippet *cds = self()->cg()->findOrCreate4ByteConstant(currentInstruction->getNode(), info->getConstant());
            TR::MemoryReference  *tempMR = generateX86MemoryReference(cds, self()->cg());
            instr = generateRegMemInstruction(currentInstruction, MOVSSRegMem, best, tempMR, self()->cg());
#ifdef DEBUG
            self()->cg()->incNumRematerializedXMMRs();
#endif
            }
         else
            {
            instr = TR::TreeEvaluator::insertLoadConstant(0, best, info->getConstant(), info->getDataType(), self()->cg(), currentInstruction);
            }
#ifdef DEBUG
         self()->cg()->incNumRematerializedConstants();
#endif
         }

      self()->cg()->traceRAInstruction(instr);
      if (comp->getDebug())
         comp->getDebug()->addInstructionComment(instr, "$REMAT");

      info->setRematerialized();
      bestDiscardableRegister->setAssignedRegister(NULL);

      if (debug("dumpRemat"))
         {
         if (info->isIndirect())
            diagnostic("---> Spilling %s rematerialisable register %s (type %d, base %s)\n",
                        self()->getDebug()->toString(info),
                        self()->getDebug()->getName(bestDiscardableRegister),
                        info->getDataType(),
                        self()->getDebug()->getName(info->getBaseRegister()));
         else
            diagnostic("---> Spilling %s rematerialisable register %s (type %d)\n",
                        self()->getDebug()->toString(info),
                        self()->getDebug()->getName(bestDiscardableRegister),
                        info->getDataType());
         }
      }
   else
      {
      bool containsInternalPointer = false;
      if (bestRegister->containsInternalPointer())
         containsInternalPointer = true;

      TR_BackingStore *location = NULL;
      int32_t offset = 0;
      if ((bestRegister->getKind() == TR_FPR))
         {
         if (bestRegister->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = bestRegister->getBackingStorage();
            }
         else
            {
            location = self()->cg()->allocateSpill(bestRegister->isSinglePrecision()? 4 : 8, false, &offset);
            }
         }
      else if ((bestRegister->getKind() == TR_VRF))
         {
         if (bestRegister->getBackingStorage())
            {
            // If there is backing storage associated with a register, it means the
            // backing store wasn't returned to the free list and it can be used.
            //
            location = bestRegister->getBackingStorage();
            }
         else
            {
            location = self()->cg()->allocateSpill(16, false, &offset);
            }
         }
      else
         {
         if (containsInternalPointer)
            {
            if (bestRegister->getBackingStorage())
               {
               // If there is backing storage associated with a register, it means the
               // backing store wasn't returned to the free list and it can be used.
               //
               location = bestRegister->getBackingStorage();
               }
            else
               {
               location = self()->cg()->allocateInternalPointerSpill(bestRegister->getPinningArrayPointer());
               }
            }
         else
            {
            if (bestRegister->getBackingStorage())
               {
               // If there is backing storage associated with a register, it means the
               // backing store wasn't returned to the free list and it can be used.
               //
               location = bestRegister->getBackingStorage();
               location->setMaxSpillDepth(self()->cg()->getCurrentPathDepth());
               if (self()->cg()->getDebug())
                  self()->cg()->traceRegisterAssignment("find or create free backing store (%p) for %s with initial max spill depth of %d and adding to list\n",
                                                location,self()->cg()->getDebug()->getName(bestRegister),self()->cg()->getCurrentPathDepth());
               }
            else
               {
               location = self()->cg()->allocateSpill(TR::Compiler->om.sizeofReferenceAddress(), bestRegister->containsCollectedReference(), &offset);
               location->setMaxSpillDepth(self()->cg()->getCurrentPathDepth());
               if (self()->cg()->getDebug())
                  self()->cg()->traceRegisterAssignment("find or create free backing store (%p) for %s with initial max spill depth of %d and adding to list\n",
                                                location,self()->cg()->getDebug()->getName(bestRegister),self()->cg()->getCurrentPathDepth());
               }
            }
         }

      if (self()->cg()->getUseNonLinearRegisterAssigner())
         {
         TR_ASSERT(bestRegister, "did not find bestRegister");
         TR_ASSERT(self()->cg()->getSpilledRegisterList(), "no spilled reg list allocated");

         if (self()->getDebug())
            self()->cg()->traceRegisterAssignment("adding %s to the spilledRegisterList)\n", self()->getDebug()->getName(bestRegister));

         self()->cg()->getSpilledRegisterList()->push_front(bestRegister);
         }

      TR::MemoryReference *tempMR = generateX86MemoryReference(location->getSymbolReference(), offset, self()->cg());
      bestRegister->setBackingStorage(location);
      TR_ASSERT(offset == 0 || offset == 4, "assertion failure");
      bestRegister->setIsSpilledToSecondHalf(offset > 0);

      bestRegister->setAssignedRegister(NULL);
      self()->cg()->getSpilledIntRegisters().push_front(bestRegister);

      TR_X86OpCodes op;
      if (bestRegister->getKind() == TR_FPR)
         {
         op = (bestRegister->isSinglePrecision()) ? MOVSSRegMem : (self()->cg()->getXMMDoubleLoadOpCode());
         }
      else if (bestRegister->getKind() == TR_VRF)
         {
         op = MOVDQURegMem;
         }
      else
         {
         op = LRegMem();
         }
      instr = new (self()->cg()->trHeapMemory()) TR::X86RegMemInstruction(currentInstruction, op, best, tempMR, self()->cg());

      self()->cg()->traceRegFreed(bestRegister, best);
      self()->cg()->traceRAInstruction(instr);
#ifdef DEBUG
      self()->cg()->incNumSpilledRegisters();
#endif
      }

   if (enableRematerialisation)
      self()->cg()->deactivateDependentDiscardableRegisters(bestRegister);

   best->setAssignedRegister(NULL);
   best->setState(TR::RealRegister::Free);

   return best;
   }

#if defined(_MSC_VER) && !defined(DEBUG)
#pragma optimize("g", on)
#endif

TR::RealRegister *OMR::X86::Machine::reverseGPRSpillState(TR::Instruction     *currentInstruction,
                                                        TR::Register        *spilledRegister,
                                                        TR::RealRegister *targetRegister,
                                                        TR_RegisterSizes    requestedRegSize)
   {
   TR::Compilation *comp = self()->cg()->comp();
   if (targetRegister == NULL)
      {
      targetRegister = self()->findBestFreeGPRegister(currentInstruction, spilledRegister, requestedRegSize);
      if (targetRegister == NULL)
         {
         targetRegister = self()->freeBestGPRegister(currentInstruction, spilledRegister, requestedRegSize);
         }
      }

   TR_BackingStore *location = spilledRegister->getBackingStorage();

   // If the virtual register has better spill placement info, see if the spill
   // can be moved to later in the instruction stream
   //
   if (self()->cg()->enableBetterSpillPlacements())
      {
      if (spilledRegister->hasBetterSpillPlacement())
         {
         TR::Instruction *betterInstruction = self()->cg()->findBetterSpillPlacement(spilledRegister, targetRegister->getRegisterNumber());
         if (betterInstruction)
            {
            self()->cg()->setRegisterAssignmentFlag(TR_HasBetterSpillPlacement);
            currentInstruction = betterInstruction;
            }
         }

      self()->cg()->removeBetterSpillPlacementCandidate(targetRegister);
      }

   if (self()->cg()->getUseNonLinearRegisterAssigner())
      {
      self()->cg()->getSpilledRegisterList()->remove(spilledRegister);
      }

   self()->cg()->getSpilledIntRegisters().remove(spilledRegister);

   if (self()->cg()->enableRematerialisation())
      {
      self()->cg()->reactivateDependentDiscardableRegisters(spilledRegister);

      // A store is not necessary if the register can be rematerialised.
      //
      if (spilledRegister->getRematerializationInfo() &&
          spilledRegister->getRematerializationInfo()->isRematerialized())
         {
         if (debug("dumpRemat"))
            {
            diagnostic("---> Avoiding storing %s rematerializable register %s to memory in %s\n",
                        self()->getDebug()->toString(spilledRegister->getRematerializationInfo()),
                        self()->getDebug()->getName(spilledRegister),
                        comp->signature());
            }

         return targetRegister;
         }
      }

   TR::MemoryReference *tempMR = generateX86MemoryReference(location->getSymbolReference(), spilledRegister->isSpilledToSecondHalf()? 4 : 0, self()->cg());
   TR::Instruction        *instr  = NULL;

   if (spilledRegister->getKind() == TR_FPR)
      {
      instr = new (self()->cg()->trHeapMemory())
         TR::X86MemRegInstruction(
            currentInstruction,
            spilledRegister->isSinglePrecision() ? MOVSSMemReg : MOVSDMemReg,
            tempMR,
            targetRegister, self()->cg());

      // Do not add a freed spill slot back onto the free list if the list is locked.
      // This is to enforce re-use of the same spill slot for a virtual register
      // while assigning non-linear control flow regions.
      //
      self()->cg()->freeSpill(location, spilledRegister->isSinglePrecision()? 4:8, spilledRegister->isSpilledToSecondHalf()? 4:0);
      if (!self()->cg()->isFreeSpillListLocked())
         {
         spilledRegister->setBackingStorage(NULL);
         }
      }
   else if (spilledRegister->getKind() == TR_VRF)
      {
      instr = new (self()->cg()->trHeapMemory())
         TR::X86MemRegInstruction(
            currentInstruction,
            MOVDQUMemReg,
            tempMR,
            targetRegister, self()->cg());

      // Do not add a freed spill slot back onto the free list if the list is locked.
      // This is to enforce re-use of the same spill slot for a virtual register
      // while assigning non-linear control flow regions.
      //
      self()->cg()->freeSpill(location, 16, 0);
      if (!self()->cg()->isFreeSpillListLocked())
         {
         spilledRegister->setBackingStorage(NULL);
         }
      }
   else
      {
      instr = new (self()->cg()->trHeapMemory())
         TR::X86MemRegInstruction(
            currentInstruction,
            SMemReg(),
            tempMR,
            targetRegister, self()->cg());
      // Do not add a freed spill slot back onto the free list if the list is locked.
      // This is to enforce re-use of the same spill slot for a virtual register
      // while assigning non-linear control flow regions.
      //
      self()->cg()->freeSpill(location, TR::Compiler->om.sizeofReferenceAddress(), spilledRegister->isSpilledToSecondHalf()? 4:0);
      if (!self()->cg()->isFreeSpillListLocked())
         {
         spilledRegister->setBackingStorage(NULL);
         }
      }

   self()->cg()->traceRAInstruction(instr);

   return targetRegister;
   }

void OMR::X86::Machine::coerceGPRegisterAssignment(TR::Instruction          *currentInstruction,
                                               TR::Register             *virtualRegister,
                                               TR::RealRegister::RegNum  registerNumber,
                                               bool                     coerceToSatisfyRegDeps)
   {
   TR::RealRegister *targetRegister          = _registerFile[registerNumber];
   TR::RealRegister    *currentAssignedRegister = virtualRegister->getAssignedRealRegister();
   TR::Instruction     *instr                   = NULL;

   if (targetRegister->getState() == TR::RealRegister::Free)
      {
      if (currentAssignedRegister == NULL)
         {
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseGPRSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }
      else
         {
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                             MOVRegReg(),
                                             currentAssignedRegister,
                                             targetRegister, self()->cg());
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }

      if (self()->cg()->enableBetterSpillPlacements())
         self()->cg()->removeBetterSpillPlacementCandidate(targetRegister);

      self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
      if (instr)
         self()->cg()->traceRAInstruction(instr);
      }
   else if (targetRegister->getState() == TR::RealRegister::Blocked ||
            targetRegister->getState() == TR::RealRegister::Assigned)
      {
      TR::Register *currentTargetVirtual = targetRegister->getAssignedRegister();

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
      if (currentAssignedRegister != NULL)
         {
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                             XCHGRegReg(),
                                             currentAssignedRegister,
                                             targetRegister, self()->cg());

         if (targetRegister->getState() == TR::RealRegister::Assigned)
            currentAssignedRegister->setState(TR::RealRegister::Assigned, currentTargetVirtual->isPlaceholderReg());
         currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
         self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
         self()->cg()->traceRAInstruction(instr);
         }
      else
         {
         TR::RealRegister *candidate = self()->findBestFreeGPRegister(currentInstruction, currentTargetVirtual);

         if (candidate)
            {
            if (self()->cg()->enableBetterSpillPlacements())
               self()->cg()->removeBetterSpillPlacementCandidate(candidate);
            }
         else
            {
            // A spill is needed, either to satisfy the coercion, or to free up a second
            // register whence the virtual currently occupying the target may be moved,
            // i.e. an indirect coercion.
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            candidate = self()->freeBestGPRegister(currentInstruction, currentTargetVirtual, TR_WordReg, registerNumber, coerceToSatisfyRegDeps);
            }

         if ((targetRegister != candidate) && (candidate != currentTargetVirtual))
            {
            instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                                MOVRegReg(),
                                                targetRegister,
                                                candidate, self()->cg());
            currentTargetVirtual->setAssignedRegister(candidate);
            candidate->setAssignedRegister(currentTargetVirtual);
            candidate->setState(targetRegister->getState());
            self()->cg()->traceRegAssigned(currentTargetVirtual, candidate);
            self()->cg()->traceRAInstruction(instr);
            self()->cg()->resetRegisterAssignmentFlag(TR_RegisterSpilled); // the spill (if any) has been traced
            }

         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseGPRSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }

      if (targetRegister->getState() == TR::RealRegister::Blocked)
         {
         if (self()->cg()->enableBetterSpillPlacements())
            self()->cg()->removeBetterSpillPlacementCandidate(targetRegister);
         }

      self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
      self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
      }

   targetRegister->setState(TR::RealRegister::Assigned, virtualRegister->isPlaceholderReg());
   targetRegister->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(targetRegister);
   virtualRegister->setAssignedAsByteRegister(false);
   }

void OMR::X86::Machine::coerceXMMRegisterAssignment(TR::Instruction          *currentInstruction,
                                                TR::Register             *virtualRegister,
                                                TR::RealRegister::RegNum  regNum,
                                                bool                     coerceToSatisfyRegDeps)
   {
   TR::RealRegister *targetRegister          = _registerFile[regNum];
   TR::RealRegister    *currentAssignedRegister = virtualRegister->getAssignedRealRegister();
   TR::Instruction     *instr                   = NULL;

   if (targetRegister->getState() == TR::RealRegister::Free)
      {
      if (currentAssignedRegister == NULL)
         {
         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseGPRSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }
      else
         {
         if (virtualRegister->getKind() == TR_VRF)
            {
            instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                                MOVDQURegReg,
                                                currentAssignedRegister,
                                                targetRegister, self()->cg());
            }
         else if (virtualRegister->isSinglePrecision())
            {
            instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                                MOVAPSRegReg,
                                                currentAssignedRegister,
                                                targetRegister, self()->cg());
            }
         else
            {
            instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction,
                                                MOVAPDRegReg,
                                                currentAssignedRegister,
                                                targetRegister, self()->cg());
            }
         currentAssignedRegister->setState(TR::RealRegister::Free);
         currentAssignedRegister->setAssignedRegister(NULL);
         }
      self()->cg()->removeBetterSpillPlacementCandidate(targetRegister);

      self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
      if (instr)
         self()->cg()->traceRAInstruction(instr);
      }
   else if (targetRegister->getState() == TR::RealRegister::Blocked)
      {
      TR::Register *currentTargetVirtual = targetRegister->getAssignedRegister();

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
      if (currentAssignedRegister != NULL)
         {
         // Exchange the contents of two XMM registers without an XCHG instruction.
         //
         TR_X86OpCodes xchgOp = BADIA32Op;
         if (virtualRegister->getKind() == TR_FPR && virtualRegister->isSinglePrecision())
            {
            xchgOp = XORPSRegReg;
            }
         else //virtualRegister->getKind() == TR_VRF || (virtualRegister->getKind() == TR_FPR && !virtualRegister->isSinglePrecision())
            {
            xchgOp = XORPDRegReg;
            }

         self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, currentAssignedRegister, targetRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, targetRegister, currentAssignedRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, currentAssignedRegister, targetRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         currentAssignedRegister->setState(TR::RealRegister::Blocked);
         currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
         }
      else
         {
         TR::RealRegister *candidate = self()->findBestFreeGPRegister(currentInstruction, currentTargetVirtual, TR_QuadWordReg);

         if (candidate)
            {
            self()->cg()->removeBetterSpillPlacementCandidate(candidate);
            }
         else
            {
            // A spill is needed, either to satisfy the coercion, or to free up a second
            // register whence the virtual currently occupying the target may be moved,
            // i.e. an indirect coercion.
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            candidate = self()->freeBestGPRegister(currentInstruction, currentTargetVirtual, TR_QuadWordReg, regNum, coerceToSatisfyRegDeps);
            }

         if (targetRegister != candidate)
            {
            if (virtualRegister->getKind() == TR_VRF)
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVDQURegReg, targetRegister, candidate, self()->cg());
               }
            else if (currentTargetVirtual->isSinglePrecision())
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVAPSRegReg, targetRegister, candidate, self()->cg());
               }
            else
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVAPDRegReg, targetRegister, candidate, self()->cg());
               }
            candidate->setState(TR::RealRegister::Blocked);
            candidate->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(candidate);
            self()->cg()->traceRegAssigned(currentTargetVirtual, candidate);
            self()->cg()->traceRAInstruction(instr);
            self()->cg()->resetRegisterAssignmentFlag(TR_RegisterSpilled); // the spill (if any) has been traced
            }

         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseGPRSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }

      self()->cg()->removeBetterSpillPlacementCandidate(targetRegister);
      self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
      self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
      }
   else if (targetRegister->getState() == TR::RealRegister::Assigned)
      {
      TR::Register *currentTargetVirtual = targetRegister->getAssignedRegister();

      self()->cg()->setRegisterAssignmentFlag(TR_IndirectCoercion);
      if (currentAssignedRegister != NULL)
         {
         TR_X86OpCodes xchgOp = BADIA32Op;
         if (virtualRegister->getKind() == TR_FPR && virtualRegister->isSinglePrecision())
            {
            xchgOp = XORPSRegReg;
            }
         else //virtualRegister->getKind() == TR_VRF || (virtualRegister->getKind() == TR_FPR && !virtualRegister->isSinglePrecision())
            {
            xchgOp = XORPDRegReg;
            }

         self()->cg()->traceRegAssigned(currentTargetVirtual, currentAssignedRegister);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, currentAssignedRegister, targetRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, targetRegister, currentAssignedRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, xchgOp, currentAssignedRegister, targetRegister, self()->cg());
         self()->cg()->traceRAInstruction(instr);
         currentAssignedRegister->setState(TR::RealRegister::Assigned, currentTargetVirtual->isPlaceholderReg());
         currentAssignedRegister->setAssignedRegister(currentTargetVirtual);
         currentTargetVirtual->setAssignedRegister(currentAssignedRegister);
         }
      else
         {
         TR::RealRegister *candidate = self()->findBestFreeGPRegister(currentInstruction, currentTargetVirtual, TR_QuadWordReg);
         if (candidate)
            {
            self()->cg()->removeBetterSpillPlacementCandidate(candidate);
            }
         else
            {
            // A spill is needed, either to satisfy the coercion, or to free up a second
            // register whence the virtual currently occupying the target may be moved,
            // i.e. an indirect coercion.
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
            candidate = self()->freeBestGPRegister(currentInstruction, currentTargetVirtual, TR_QuadWordReg, regNum, coerceToSatisfyRegDeps);
            }

         if (targetRegister != candidate)
            {
            if (virtualRegister->getKind() == TR_VRF)
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVDQURegReg, targetRegister, candidate, self()->cg());
               }
            else if (currentTargetVirtual->isSinglePrecision())
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVAPSRegReg, targetRegister, candidate, self()->cg());
               }
            else
               {
               instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, MOVAPDRegReg, targetRegister, candidate, self()->cg());
               }
            candidate->setState(TR::RealRegister::Assigned, currentTargetVirtual->isPlaceholderReg());
            candidate->setAssignedRegister(currentTargetVirtual);
            currentTargetVirtual->setAssignedRegister(candidate);
            self()->cg()->traceRegAssigned(currentTargetVirtual, candidate);
            self()->cg()->traceRAInstruction(instr);
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled); // the spill (if any) has been traced
            }

         if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
            {
            self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
            self()->reverseGPRSpillState(currentInstruction, virtualRegister, targetRegister);
            }
         }

      self()->cg()->resetRegisterAssignmentFlag(TR_IndirectCoercion);
      self()->cg()->traceRegAssigned(virtualRegister, targetRegister);
      }

   targetRegister->setState(TR::RealRegister::Assigned, virtualRegister->isPlaceholderReg());
   targetRegister->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(targetRegister);
   virtualRegister->setAssignedAsByteRegister(false);
   }

void OMR::X86::Machine::swapGPRegisters(TR::Instruction          *currentInstruction,
                                    TR::RealRegister::RegNum  regNum1,
                                    TR::RealRegister::RegNum  regNum2)
   {
   TR::RealRegister *realReg1 = self()->getX86RealRegister(regNum1);
   TR::RealRegister *realReg2 = self()->getX86RealRegister(regNum2);
   TR::Instruction *instr = new (self()->cg()->trHeapMemory()) TR::X86RegRegInstruction(currentInstruction, XCHGRegReg(), realReg1, realReg2, self()->cg());

   TR::Register *virtReg1 = realReg1->getAssignedRegister();
   TR::Register *virtReg2 = realReg2->getAssignedRegister();

   virtReg1->setAssignedRegister(realReg2);
   virtReg2->setAssignedRegister(realReg1);
   realReg1->setAssignedRegister(virtReg2);
   realReg2->setAssignedRegister(virtReg1);

   self()->cg()->traceRegAssigned(virtReg1, realReg2);
   self()->cg()->traceRegAssigned(virtReg2, realReg1);
   self()->cg()->traceRAInstruction(instr);
   }

void OMR::X86::Machine::coerceGPRegisterAssignment(TR::Instruction   *currentInstruction,
                                               TR::Register      *virtualRegister,
                                               TR_RegisterSizes  requestedRegSize)
   {
   TR::RealRegister *candidate;
   if ((candidate = self()->findBestFreeGPRegister(currentInstruction, virtualRegister, requestedRegSize)) == NULL)
      {
      self()->cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
      candidate = self()->freeBestGPRegister(currentInstruction, virtualRegister, requestedRegSize);
      }

   if (virtualRegister->getTotalUseCount() != virtualRegister->getFutureUseCount())
      {
      self()->cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
      self()->reverseGPRSpillState(currentInstruction, virtualRegister, candidate);
      }

   if (self()->cg()->enableBetterSpillPlacements())
      self()->cg()->removeBetterSpillPlacementCandidate(candidate);

   candidate->setState(TR::RealRegister::Assigned, virtualRegister->isPlaceholderReg());
   candidate->setAssignedRegister(virtualRegister);
   virtualRegister->setAssignedRegister(candidate);
   virtualRegister->setAssignedAsByteRegister(false);

   self()->cg()->traceRegAssigned(virtualRegister, candidate);
   }


void OMR::X86::Machine::setGPRWeightsFromAssociations()
   {
   const TR::X86LinkageProperties &linkageProperties = self()->cg()->getProperties();

   for (int i = TR::RealRegister::FirstGPR;
        i <= TR::RealRegister::LastAssignableGPR;
        ++i)
      {
      // Skip non-assignable registers
      //
      if (self()->getX86RealRegister((TR::RealRegister::RegNum)i)->getState() == TR::RealRegister::Locked)
         continue;

      TR::Register *assocReg = _registerAssociations[i];
      if ((linkageProperties.isPreservedRegister((TR::RealRegister::RegNum)i) || linkageProperties.isCalleeVolatileRegister((TR::RealRegister::RegNum)i)) &&
          _registerFile[i]->getHasBeenAssignedInMethod() == false)
         {
         if (assocReg)
            {
            assocReg->setAssociation(i);
            }
         _registerFile[i]->setWeight(IA32_REGISTER_INITIAL_PRESERVED_WEIGHT);
         }
      else if (assocReg == NULL)
         {
         _registerFile[i]->setWeight(IA32_REGISTER_BASIC_WEIGHT);
         }
      else
         {
         assocReg->setAssociation(i);
         if (assocReg->isPlaceholderReg())
            { // placeholder register and is only needed at the specific dependency
              // site (usually a killed register on a call)
              // so defer this register's weight to that of registers
              // where the associated register has a longer life
            _registerFile[i]->setWeight(IA32_REGISTER_PLACEHOLDER_WEIGHT);
            }
         else
            {
            _registerFile[i]->setWeight(IA32_REGISTER_ASSOCIATED_WEIGHT);
            }
         }
      }
   }

void
OMR::X86::Machine::createRegisterAssociationDirective(TR::Instruction *cursor)
   {
   TR::Compilation *comp = self()->cg()->comp();
   TR::RegisterDependencyConditions  *associations =
      generateRegisterDependencyConditions((uint8_t)0, TR::RealRegister::LastAssignableGPR, self()->cg());

   // Go through the current associations held in the machine and put a copy of
   // that state out into the stream after the cursor
   // so that when the register assigner goes backwards through this point
   // it updates the machine and register association states properly
   //
   for (int i = 0; i < TR::RealRegister::LastAssignableGPR; i++)
      {
      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum)(i+1);

      // Skip non-assignable registers
      //
      if (self()->getX86RealRegister(regNum)->getState() == TR::RealRegister::Locked)
         continue;

      associations->addPostCondition(self()->getVirtualAssociatedWithReal(regNum),
                                     regNum,
                                     self()->cg(),
                                     0,
                                     true);
      }

   associations->stopAddingPostConditions();

   new (self()->cg()->trHeapMemory()) TR::Instruction(associations, ASSOCREGS, cursor, self()->cg());
   if (cursor == self()->cg()->getAppendInstruction())
      self()->cg()->setAppendInstruction(cursor->getNext());

   // There's no need to have a virtual appear in more than one ASSOCREGS after
   // its live range has ended.  One is enough.  So we clear out all the dead
   // registers from the associations.
   //
   for (int i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      TR::Register *virtReg = self()->getVirtualAssociatedWithReal((TR::RealRegister::RegNum)i);
      if (virtReg && !virtReg->isLive())
         self()->setVirtualAssociatedWithReal((TR::RealRegister::RegNum)i, NULL);
      }

   }

// TODO:AMD64: Confirm that this is what the weights mean
#define PRESERVED_WEIGHT    0xFF00
#define NONPRESERVED_WEIGHT 0
#define UNUSED_WEIGHT       0xFFFF

void
OMR::X86::Machine::initializeRegisterFile(const struct TR::X86LinkageProperties &properties)
   {
   int reg;

   // Special pseudo-registers
   //
   _registerFile[TR::RealRegister::NoReg] = NULL;
   _registerFile[TR::RealRegister::ByteReg] = NULL;
   _registerFile[TR::RealRegister::BestFreeReg] = NULL;

   // GPRs
   //
   _registerFile[TR::RealRegister::eax] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               properties.isPreservedRegister(TR::RealRegister::eax) ? PRESERVED_WEIGHT: NONPRESERVED_WEIGHT,
                                               TR::RealRegister::Free,
                                               TR::RealRegister::eax,
                                               TR::RealRegister::eaxMask, self()->cg());

   static char *dontUseEBXasGPR = feGetEnv("dontUseEBXasGPR");

   if (!dontUseEBXasGPR)
      {
      _registerFile[TR::RealRegister::ebx] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                  properties.isPreservedRegister(TR::RealRegister::ebx) ? PRESERVED_WEIGHT: NONPRESERVED_WEIGHT,
                                                  TR::RealRegister::Free,
                                                  TR::RealRegister::ebx,
                                                  TR::RealRegister::ebxMask, self()->cg());
      }
   else
      {
      _registerFile[TR::RealRegister::ebx] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                  UNUSED_WEIGHT,
                                                  TR::RealRegister::Locked,
                                                  TR::RealRegister::ebx,
                                                  TR::RealRegister::ebxMask, self()->cg());
      _registerFile[TR::RealRegister::ebx]->setAssignedRegister(_registerFile[TR::RealRegister::ebx]);
      }

   _registerFile[TR::RealRegister::ecx] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               properties.isPreservedRegister(TR::RealRegister::ecx) ? PRESERVED_WEIGHT: NONPRESERVED_WEIGHT,
                                               TR::RealRegister::Free,
                                               TR::RealRegister::ecx,
                                               TR::RealRegister::ecxMask, self()->cg());

   _registerFile[TR::RealRegister::edx] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               properties.isPreservedRegister(TR::RealRegister::edx) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                               TR::RealRegister::Free,
                                               TR::RealRegister::edx,
                                               TR::RealRegister::edxMask, self()->cg());

   _registerFile[TR::RealRegister::edi] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               properties.isPreservedRegister(TR::RealRegister::edi) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                               TR::RealRegister::Free,
                                               TR::RealRegister::edi,
                                               TR::RealRegister::ediMask, self()->cg());

   _registerFile[TR::RealRegister::esi] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               properties.isPreservedRegister(TR::RealRegister::esi) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                               TR::RealRegister::Free,
                                               TR::RealRegister::esi,
                                               TR::RealRegister::esiMask, self()->cg());

   // Platforms that are able to use ebp as a GPR can set the state to Free and
   // clear the assigned register in the CodeGen constructor.
   _registerFile[TR::RealRegister::ebp] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               UNUSED_WEIGHT,
                                               TR::RealRegister::Locked,
                                               TR::RealRegister::ebp,
                                               TR::RealRegister::ebpMask, self()->cg());
   _registerFile[TR::RealRegister::ebp]->setAssignedRegister(_registerFile[TR::RealRegister::ebp]);

   _registerFile[TR::RealRegister::esp] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               UNUSED_WEIGHT,
                                               TR::RealRegister::Locked,
                                               TR::RealRegister::esp,
                                               TR::RealRegister::espMask, self()->cg());
   _registerFile[TR::RealRegister::esp]->setAssignedRegister(_registerFile[TR::RealRegister::esp]);

   _registerFile[TR::RealRegister::vfp] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                               UNUSED_WEIGHT,
                                               TR::RealRegister::Locked,
                                               TR::RealRegister::vfp,
                                               TR::RealRegister::noRegMask, self()->cg());
   _registerFile[TR::RealRegister::vfp]->setAssignedRegister(_registerFile[TR::RealRegister::NoReg]);

#ifdef TR_TARGET_64BIT
   for(reg = TR::RealRegister::r8; reg <= TR::RealRegister::LastAssignableGPR; reg++)
      {
      _registerFile[reg] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                  properties.isPreservedRegister((TR::RealRegister::RegNum)reg) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                                  TR::RealRegister::Free,
                                                  (TR::RealRegister::RegNum)reg,
                                                  TR::RealRegister::gprMask((TR::RealRegister::RegNum)reg), self()->cg());
      }
#endif

   // Other register kinds
   //
   for(reg = TR::RealRegister::FirstFPR; reg <= TR::RealRegister::LastFPR; reg++)
      {
      _registerFile[reg] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_X87,
                                                  properties.isPreservedRegister((TR::RealRegister::RegNum)reg) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                                  TR::RealRegister::Free,
                                                  (TR::RealRegister::RegNum)reg,
                                                  TR::RealRegister::fprMask((TR::RealRegister::RegNum)reg), self()->cg());
      }

   for(reg = TR::RealRegister::FirstMMXR; reg <= TR::RealRegister::LastMMXR; reg++)
      {
      _registerFile[reg] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_GPR,
                                                  properties.isPreservedRegister((TR::RealRegister::RegNum)reg) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                                  TR::RealRegister::Free,
                                                  (TR::RealRegister::RegNum)reg,
                                                  TR::RealRegister::mmrMask((TR::RealRegister::RegNum)reg), self()->cg());
      }

   for(reg = TR::RealRegister::FirstXMMR; reg <= TR::RealRegister::LastXMMR; reg++)
      {
      _registerFile[reg] = new (self()->cg()->trHeapMemory()) TR::RealRegister(TR_FPR,
                                                  properties.isPreservedRegister((TR::RealRegister::RegNum)reg) ? PRESERVED_WEIGHT : NONPRESERVED_WEIGHT,
                                                  TR::RealRegister::Free,
                                                  (TR::RealRegister::RegNum)reg,
                                                  TR::RealRegister::xmmrMask((TR::RealRegister::RegNum)reg), self()->cg());
      }

   }

uint32_t*
OMR::X86::Machine::getGlobalRegisterTable(const struct TR::X86LinkageProperties& property)
   {
   uint32_t * allocationOrder = property.getRegisterAllocationOrder();
   for (int i = 0; i < self()->getNumGlobalGPRs() + _numGlobalFPRs; i++)
      {
      _globalRegisterNumberToRealRegisterMap[i] = allocationOrder[i];
      }
   return _globalRegisterNumberToRealRegisterMap;
   }

TR::RealRegister **
OMR::X86::Machine::cloneRegisterFile(TR::RealRegister **registerFile, TR_AllocationKind allocKind)
   {
   int32_t arraySize = sizeof(TR::RealRegister *)*TR_X86_REGISTER_FILE_SIZE;
   TR::RealRegister  **registerFileClone = (TR::RealRegister **)self()->cg()->trMemory()->allocateMemory(arraySize, allocKind);
   int32_t i = 0;
   int32_t endReg = TR::RealRegister::LastAssignableGPR;

   TR_LiveRegisters *liveFPRs = self()->cg()->getLiveRegisters(TR_FPR);
   TR_LiveRegisters *liveVRFs = self()->cg()->getLiveRegisters(TR_VRF);

   if ((liveFPRs && liveFPRs->getNumberOfLiveRegisters() > 0) || (liveVRFs && liveVRFs->getNumberOfLiveRegisters() > 0))
      endReg = TR::RealRegister::LastXMMR;

   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      registerFileClone[i] = (TR::RealRegister *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::RealRegister), allocKind);
      memcpy(registerFileClone[i], registerFile[i], sizeof(TR::RealRegister));
      }

   // the vfp entry is a real register and it must always point to the same _frameRegister pointer so the memRef assignRegisters check
   // if (_baseRegister == cg()->machine()->getX86RealRegister(TR::RealRegister::vfp)
   // works correctly
   registerFileClone[TR::RealRegister::vfp] = self()->cg()->getFrameRegister();

   // if there are no live FPRs then initialize the clone with the incoming registerFile values for the XMMRegs so
   // these pointers are not garbage values
   if (endReg == TR::RealRegister::LastAssignableGPR)
      {
      for (i = TR::RealRegister::FirstXMMR; i <= TR::RealRegister::LastXMMR; i++)
         registerFileClone[i] = registerFile[i];
      }

   return registerFileClone;
   }


TR::RegisterDependencyConditions * OMR::X86::Machine::createDepCondForLiveGPRs()
   {
   int32_t i, c=0;

   // Calculate number of register dependencies required.  This step is not really necessary, but
   // it is space conscious.
   //
   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastXMMR; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      TR::RealRegister *realReg = self()->getX86RealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Assigned ||
          realReg->getState() == TR::RealRegister::Free ||
          realReg->getState() == TR::RealRegister::Blocked)
         c++;
      }

   TR::RegisterDependencyConditions *deps = NULL;

   if (c)
      {
      deps = generateRegisterDependencyConditions(0, c, self()->cg());
      for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastXMMR; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
         {
         TR::RealRegister *realReg = self()->getX86RealRegister((TR::RealRegister::RegNum)i);
         if (realReg->getState() == TR::RealRegister::Assigned ||
             realReg->getState() == TR::RealRegister::Free ||
             realReg->getState() == TR::RealRegister::Blocked)
            {
            TR::Register *virtReg;
            if (realReg->getState() == TR::RealRegister::Free)
               {
               virtReg = self()->cg()->allocateRegister(i <= TR::RealRegister::LastAssignableGPR? TR_GPR : TR_FPR);
               virtReg->setPlaceholderReg();
               }
            else
               {
               virtReg = realReg->getAssignedRegister();
               }
            deps->addPostCondition(virtReg, realReg->getRegisterNumber(), self()->cg());
            if (virtReg->isPlaceholderReg())
               self()->cg()->stopUsingRegister(virtReg);
            virtReg->incTotalUseCount();
            virtReg->incFutureUseCount();
            }
         }

      }

   return deps;
   }

// if calledForDeps == false then this is being called to generate conditions for use in regAssocs
TR::RegisterDependencyConditions * OMR::X86::Machine::createCondForLiveAndSpilledGPRs(bool calledForDeps, TR::list<TR::Register*> *spilledRegisterList)
   {
   int32_t i, c=0;

   // Calculate number of register dependencies required.  This step is not really necessary, but
   // it is space conscious.
   //
   int32_t endReg = TR::RealRegister::LastAssignableGPR;
   TR_LiveRegisters *liveFPRs = self()->cg()->getLiveRegisters(TR_FPR);
   TR_LiveRegisters *liveVRFs = self()->cg()->getLiveRegisters(TR_VRF);
   if ((liveFPRs && liveFPRs->getNumberOfLiveRegisters() > 0) || (liveVRFs && liveVRFs->getNumberOfLiveRegisters() > 0))
      endReg = TR::RealRegister::LastXMMR;
   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      TR::RealRegister *realReg = self()->getX86RealRegister((TR::RealRegister::RegNum)i);
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
      deps = generateRegisterDependencyConditions(0, c, self()->cg());
      for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
         {
         TR::RealRegister *realReg = self()->getX86RealRegister((TR::RealRegister::RegNum)i);
         if (realReg->getState() == TR::RealRegister::Assigned)
            {
            TR::Register *virtReg = realReg->getAssignedRegister();
            TR_ASSERT(!spilledRegisterList || !(std::find(spilledRegisterList->begin(), spilledRegisterList->end(), virtReg) != spilledRegisterList->end())
            		,"a register should not be in both an assigned state and in the spilled list\n");
            deps->addPostCondition(virtReg, realReg->getRegisterNumber(), self()->cg());
            if (calledForDeps)
               {
               virtReg->incTotalUseCount();
               virtReg->incFutureUseCount();
               virtReg->setAssignedRegister(NULL);
               realReg->setAssignedRegister(NULL);
               realReg->setState(TR::RealRegister::Free);
               }
            }
         }

      if (spilledRegisterList)
         {
         for (auto i = spilledRegisterList->begin(); i != spilledRegisterList->end(); ++i)
         {
            deps->addPostCondition(*i, TR::RealRegister::SpilledReg, self()->cg());
         }
         }
      }

   return deps;
   }


// Shares many similarities with 'cloneRegisterFile'.  Consider merging.
//
TR::RealRegister **OMR::X86::Machine::captureRegisterFile()
   {
   int32_t arraySize = sizeof(TR::RealRegister *) * TR_X86_REGISTER_FILE_SIZE;

   TR::RealRegister **registerFileClone =
      (TR::RealRegister **)self()->cg()->trMemory()->allocateMemory(arraySize, heapAlloc);

   int32_t endReg = TR::RealRegister::LastXMMR;
   for (int32_t i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      registerFileClone[i] =
         (TR::RealRegister *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::RealRegister), heapAlloc);
      memcpy(registerFileClone[i], _registerFile[i], sizeof(TR::RealRegister));
      }

   registerFileClone[TR::RealRegister::vfp] =
      (TR::RealRegister *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::RealRegister), heapAlloc);
   memcpy(registerFileClone[TR::RealRegister::vfp], _registerFile[TR::RealRegister::vfp], sizeof(TR::RealRegister));

   return registerFileClone;
   }

void OMR::X86::Machine::installRegisterFile(TR::RealRegister **registerFileCopy)
   {
   int32_t endReg = TR::RealRegister::LastXMMR;

   // Unlink currently assigned registers from their virtuals.  This must be done
   // before the register file is installed to prevent unintended clobbering.
   //
   for (int32_t i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      TR::Register *assignedVirtReg = _registerFile[i]->getAssignedRegister();
      if (assignedVirtReg != NULL)
         {
         TR_ASSERT(_registerFile[i]->getState() == TR::RealRegister::Assigned ||
                _registerFile[i]->getState() == TR::RealRegister::Locked,
            "expecting register file register to be either assigned or locked");

         if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
            {
            assignedVirtReg->setAssignedRegister(NULL);
            }
         }
      }

   // Install register file.
   //
   for (int32_t i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      // Some real register fields are sticky and their contents must be preserved.
      //
      // TODO: this will have to be made smarter to work cross platform.
      //
      bool wasAssigned = _registerFile[i]->getHasBeenAssignedInMethod();

      memcpy(_registerFile[i], registerFileCopy[i], sizeof(TR::RealRegister));

      // Copy the sticky bits.
      //
      if (wasAssigned)
         {
         _registerFile[i]->setHasBeenAssignedInMethod(true);
         }

      // Link captured assigned registers to their virtuals.
      //
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         _registerFile[i]->getAssignedRegister()->setAssignedRegister(_registerFile[i]);
         }
      }

   memcpy(_registerFile[TR::RealRegister::vfp], registerFileCopy[TR::RealRegister::vfp], sizeof(TR::RealRegister));
   }

TR::Register **OMR::X86::Machine::captureRegisterAssociations()
   {
   int32_t arraySize = sizeof(TR::Register *) * TR_X86_REGISTER_FILE_SIZE;

   TR::Register **registerAssociationsClone =
      (TR::Register **)self()->cg()->trMemory()->allocateMemory(arraySize, heapAlloc);

   int32_t endReg = TR::RealRegister::LastXMMR;
   for (int32_t i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      if (_registerAssociations[i] != NULL)
         {
         registerAssociationsClone[i] =
            (TR::Register *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::Register), heapAlloc);
         memcpy(registerAssociationsClone[i], _registerAssociations[i], sizeof(TR::Register));
         }
      else
         {
         registerAssociationsClone[i] = NULL;
         }
      }

   if (_registerAssociations[TR::RealRegister::vfp] != NULL)
      {
      registerAssociationsClone[TR::RealRegister::vfp] =
         (TR::Register *)self()->cg()->trMemory()->allocateMemory(sizeof(TR::Register), heapAlloc);
      memcpy(registerAssociationsClone[TR::RealRegister::vfp], _registerAssociations[TR::RealRegister::vfp], sizeof(TR::Register));
      }
   else
      {
      registerAssociationsClone[TR::RealRegister::vfp] = NULL;
      }

   return registerAssociationsClone;
   }

TR::list<TR::Register*> *OMR::X86::Machine::captureSpilledRegistersList()
   {

   // TODO: hang a spilled registers list from the machine

   TR_ASSERT(self()->cg()->getSpilledRegisterList(), "expecting an existing spilledRegistersList before capture");

   TR::list<TR::Register*> *srl = new (self()->cg()->trHeapMemory()) TR::list<TR::Register*>(getTypedAllocator<TR::Register*>(self()->cg()->comp()->allocator()));
   for(auto iter = self()->cg()->getSpilledRegisterList()->begin(); iter != self()->cg()->getSpilledRegisterList()->end(); ++iter)
      {
      srl->push_front(*iter);
      }

   return srl;
   }

TR::RegisterDependencyConditions *
TR_RegisterAssignerState::createDependenciesFromRegisterState(TR_OutlinedInstructions *oi)
   {
   // Calculate the required number of dependencies.
   //
   TR::Compilation *comp = _machine->cg()->comp();
   int32_t numDeps = 0;
   int32_t i;
   int32_t endReg = TR::RealRegister::LastXMMR;
   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         numDeps++;
      }

   numDeps += _spilledRegistersList->size();

   if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
      {
      traceMsg(comp, "createDependenciesFromRegisterState : %d live registers: %d assigned, %d spilled\n",
         numDeps,
         numDeps-_spilledRegistersList->size(),
         _spilledRegistersList->size());
      }

   if (numDeps == 0)
      return NULL;

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(0, numDeps, _machine->cg());

   TR_RegisterAssignerState *rasAtMerge = oi->getRegisterAssignerStateAtMerge();
   TR_ASSERT(rasAtMerge, "Must have captured register assigner state at merge");

   // Populate assignments from the register file.
   //
   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      TR::RealRegister *realReg = _registerFile[i];
      if (realReg->getState() == TR::RealRegister::Assigned)
         {
         TR::Register *virtReg = realReg->getAssignedRegister();
         deps->addPostCondition(virtReg, realReg->getRegisterNumber(), _machine->cg());

#if 0
         if (!rasAtMerge->isLive(virtReg) &&
             !oi->findInRegisterUsageList(oi->getOutlinedPathRegisterUsageList(), virtReg))
            {
            TR::list<OMR::RegisterUsage*> *mainlineRUL = oi->getMainlinePathRegisterUsageList();

            if (mainlineRUL)
               {
               for(auto iter = mainlineRUL->begin(); iter != mainlineRUL->end(); ++iter)
                  {
                  if ((*iter)->virtReg == virtReg)
                     {
                     virtReg->decTotalUseCount((*iter)->useCount);

                     if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
                        {
                        traceMsg(comp, "Adjusting up register use counts of reg %p (fuc=%d:tuc=%d:mergeFuc=%d) by %d\n",
                        		(*iter)->virtReg, (*iter)->virtReg->getFutureUseCount(), (*iter)->virtReg->getTotalUseCount(), (*iter)->mergeFuc, (*iter)->useCount);
                        }
                     }
                  }
               }
            else
               {
               // skip it?
               }
            }
#endif

         // Because this happens during register assignment the future use needs to be adjusted
         // to account for this new use.  The total use count will be adjusted when this
         // dependency is hung off an instruction.
         //
         virtReg->incFutureUseCount();

         if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
            {
            traceMsg(comp, "   create ASSIGNED dependency: virtual %p -> %s\n",
               virtReg,
               _machine->getDebug()->getName(realReg));
            }
         bool found = (std::find(_spilledRegistersList->begin(), _spilledRegistersList->end(), virtReg) != _spilledRegistersList->end());
         TR_ASSERT(!_spilledRegistersList || !found,
               "a register should not be in both an assigned state and in the spilled list\n");
         }
      }

   // Create spill dependencies from the spilled registers list.
   //
   for(auto iter = _spilledRegistersList->begin(); iter != _spilledRegistersList->end(); ++iter)
      {
      deps->addPostCondition(*iter, TR::RealRegister::SpilledReg, _machine->cg());

      // Because this happens during register assignment the future use needs to be adjusted
      // to account for this new use.  The total use count will be adjusted when this
      // dependency is hung off an instruction.
      //
      (*iter)->incFutureUseCount();

      if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(comp, "   create SPILLED dependency: virtual %p -> backing storage %p\n",
        		 *iter,
        		 (*iter)->getBackingStorage());
         }
      }

   return deps;
   }

// Captures the current state of the register assigner from the machine.
//
void TR_RegisterAssignerState::capture()
   {
   _registerFile = _machine->captureRegisterFile();
   _registerAssociations = _machine->captureRegisterAssociations();
   _spilledRegistersList = _machine->captureSpilledRegistersList();
   }

// Installs a previously captured register assigner state into the machine.
//
void TR_RegisterAssignerState::install()
   {
   TR_ASSERT(_registerFile && _registerAssociations && _spilledRegistersList, "missing captured register assigner state");

   // The captured register file cannot simply replace the initial register
   // file in the machine because of the fact that pointers to real registers
   // (i.e., the elements of the register file itself) may be held at various
   // places.  It is not strictly safe to assume we can change the addresses
   // of the register file at will.
   //
   _machine->installRegisterFile(_registerFile);

   // Pointers internal to the following lists are never held and they can be
   // simply swapped in.
   //
   _machine->setRegisterAssociations(_registerAssociations);
   _machine->cg()->setSpilledRegisterList(_spilledRegistersList);
   }

// Determine whether a given virtual register is live within a captured
// register state.
//
bool TR_RegisterAssignerState::isLive(TR::Register *virtReg)
   {
   // Check the register file
   //
   int32_t i;
   int32_t endReg = TR::RealRegister::LastXMMR;
   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         if (_registerFile[i]->getAssignedRegister() == virtReg)
            {
            return true;
            }
         }
      }

   // Check the spilled register list
   //
   auto iter = _spilledRegistersList->begin();
   for (; (iter != _spilledRegistersList->end()) && (*iter) != virtReg; ++iter)
      {
      }

   return (iter != _spilledRegistersList->end()) ? true : false;

   }

void TR_RegisterAssignerState::dump()
   {
   TR::Compilation *comp = _machine->cg()->comp();

   if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
      {
      traceMsg(comp, "\nREGISTER ASSIGNER STATE\n=======================\n\nAssigned Live Registers:\n");

      int32_t i;
      int32_t endReg = TR::RealRegister::LastXMMR;
      for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
         {
         if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
            {
            traceMsg(comp, "         %s -> %s\n",
               comp->getDebug()->getName(_registerFile[i]->getAssignedRegister()),
               comp->getDebug()->getName(_registerFile[i]));
            }
         }

      traceMsg(comp, "\nSpilled Registers:\n");
      for(auto iter = _spilledRegistersList->begin(); iter != _spilledRegistersList->end(); ++iter)
         traceMsg(comp, "         %s\n", comp->getDebug()->getName(*iter));

      traceMsg(comp, "\n=======================\n");
      }
   }

void OMR::X86::Machine::adjustRegisterUseCountsUp(TR::list<OMR::RegisterUsage*> *rul, bool adjustFuture)
   {
   if (!rul)
      return;
   TR::Compilation *comp = self()->cg()->comp();
   for(auto iter = rul->begin(); iter != rul->end(); ++iter)
      {
      if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(comp, "Adjusting UP register use counts of reg %p (fuc=%d:tuc=%d:adjustFuture=%d) by %d -> ",
        		 (*iter)->virtReg, (*iter)->virtReg->getFutureUseCount(), (*iter)->virtReg->getTotalUseCount(), adjustFuture, (*iter)->useCount);
         }

      (*iter)->virtReg->incTotalUseCount((*iter)->useCount);

      if (adjustFuture)
    	  (*iter)->virtReg->incFutureUseCount((*iter)->useCount);

      if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(comp, "(fuc=%d:tuc=%d)\n", (*iter)->virtReg->getFutureUseCount(), (*iter)->virtReg->getTotalUseCount());
         }
      }
   }

void OMR::X86::Machine::adjustRegisterUseCountsDown(TR::list<OMR::RegisterUsage*> *rul, bool adjustFuture)
   {
   if (!rul)
      return;
   TR::Compilation *comp = self()->cg()->comp();

   for(auto iter = rul->begin(); iter != rul->end(); ++iter)
      {
      if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(comp, "Adjusting DOWN register use counts of reg %p (fuc=%d:tuc=%d:adjustFuture=%d) by %d -> ",
        		 (*iter)->virtReg, (*iter)->virtReg->getFutureUseCount(), (*iter)->virtReg->getTotalUseCount(), adjustFuture, (*iter)->useCount);
         }

      (*iter)->virtReg->decTotalUseCount((*iter)->useCount);

      if (adjustFuture)
    	  (*iter)->virtReg->decFutureUseCount((*iter)->useCount);

      if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
         {
         traceMsg(comp, "(fuc=%d:tuc=%d)\n", (*iter)->virtReg->getFutureUseCount(), (*iter)->virtReg->getTotalUseCount());
         }
      }
   }


void OMR::X86::Machine::disassociateUnspilledBackingStorage()
   {
   int32_t i;
   int32_t endReg = TR::RealRegister::LastXMMR;
   TR_BackingStore *location = NULL;
   TR::Compilation *comp = self()->cg()->comp();

   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         TR::Register *virtReg = _registerFile[i]->getAssignedRegister();
         location = virtReg->getBackingStorage();

         if (location)
            {
            int32_t size = -1;
            if (virtReg->getKind() == TR_FPR)
               {
               if (virtReg->isSinglePrecision())
                  {
                  size = 4;
                  }
               else
                  {
                  size = 8;
                  }
               }
            else if (virtReg->getKind() == TR_VRF)
               {
               size = 16;
               }
            else
               {
               size = TR::Compiler->om.sizeofReferenceAddress();
               }
            self()->cg()->freeSpill(location, size, virtReg->isSpilledToSecondHalf()? 4:0);
            virtReg->setBackingStorage(NULL);

            traceMsg(self()->cg()->comp(), "disassociating backing storage %p from assigned virtual %p\n", location, virtReg);
            }
         }
      }
   }


void OMR::X86::Machine::purgeDeadRegistersFromRegisterFile()
   {
   int32_t i;
   int32_t endReg = TR::RealRegister::LastXMMR;
   for (i = TR::RealRegister::FirstGPR; i <= endReg; i = ((i==TR::RealRegister::LastAssignableGPR) ? TR::RealRegister::FirstXMMR : i+1))
      {
      if (_registerFile[i]->getState() == TR::RealRegister::Assigned)
         {
         TR::Register *virtReg = _registerFile[i]->getAssignedRegister();
         if (virtReg->getFutureUseCount() == 0)
            {
            virtReg->setAssignedRegister(NULL);
            _registerFile[i]->setAssignedRegister(NULL);
            _registerFile[i]->setState(TR::RealRegister::Free);
            }
/*
            traceMsg(comp, "         %s -> %s\n",
               comp->getDebug()->getName(_registerFile[i]->getAssignedRegister()),
               comp->getDebug()->getName(_registerFile[i]));
*/
         }
      }

#if 0
   // TODO: Remove dead spilled registers.
   //
   for(auto iter = cg()->getSpilledRegisterList().begin(); iter != cg()->getSpilledRegisterList().end(); ++iter)
      {
      if ((*iter)->getFutureUseCount() == 0)
         {
         }
      }
#endif

   }


// Retrieve a memory reference handle to a dummy local variable of the specified type.
// If the dummy has not been allocated, allocate it and return the handle.
//
// These reusable temporaries are intended for use whenever a temporary store to a
// memory location is required.

TR::MemoryReference *OMR::X86::Machine::getDummyLocalMR(TR::DataType dt)
   {
   if (_dummyLocal[dt] == NULL)
      {
      _dummyLocal[dt] = self()->cg()->allocateLocalTemp(dt);
      }

   return generateX86MemoryReference(_dummyLocal[dt], self()->cg());
   }


TR::RealRegister *OMR::X86::Machine::fpMapToStackRelativeRegister(TR::Register *vreg)
   {
   TR_X86FPStackRegister *fpReg = toX86FPStackRegister(vreg->getAssignedRealRegister());
   return _registerFile[self()->getFPTopOfStack() - fpReg->getFPStackRegisterNumber() + TR::RealRegister::FirstFPR];
   }


void OMR::X86::Machine::resetFPStackRegisters()
   {
   int32_t i;
   for (i=0;i<TR_X86FPStackRegister::NumRegisters;i++)
      {
      self()->setFPStackRegister(i, NULL);
      self()->setCopiedFPStackRegister(i, NULL);
      self()->setFPStackRegisterNode(i, NULL);
      }
   }


void OMR::X86::Machine::initializeFPStackRegisterFile()
   {
   _fpStack[TR_X86FPStackRegister::fp0] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp0,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp1] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp1,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp2] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp2,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp3] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp3,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp4] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp4,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp5] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp5,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp6] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp6,
                                                                    TR::RealRegister::NoReg, self()->cg());
   _fpStack[TR_X86FPStackRegister::fp7] = new (self()->cg()->trHeapMemory()) TR_X86FPStackRegister(TR_X86FPStackRegister::Free,
                                                                    TR_X86FPStackRegister::fp7,
                                                                    TR::RealRegister::NoReg, self()->cg());
   }


// Does the specified virtual register occupy the top of stack?
//
bool OMR::X86::Machine::isFPRTopOfStack(TR::Register *virtReg)
   {
   TR_X86FPStackRegister *fpReg = toX86FPStackRegister(virtReg->getAssignedRegister());
   return (fpReg == self()->getFPTopOfStackPtr()) ? true : false;
   }


// Push a value onto the FP stack.
//
void OMR::X86::Machine::fpStackPush(TR::Register *virtReg)
   {

   _fpTopOfStack++;

   // Check for stack overflow.
   //
   TR_ASSERT( (_fpTopOfStack < TR_X86FPStackRegister::fpStackFull),
            "fpStackPush() ==> Floating point stack overflow!" );

   // Check if the register is already on the stack.
   //
   TR_ASSERT( (virtReg->getAssignedRegister() == NULL),
            "fpStackPush() ==> Virtual register is already on the stack!" );

   virtReg->setAssignedRegister(_fpStack[_fpTopOfStack]);
   _fpStack[_fpTopOfStack]->setAssignedRegister(virtReg);
   _fpStack[_fpTopOfStack]->setState(TR::RealRegister::Assigned);
   }


// Coerce a value onto the specified location on the FP stack; note
// that stack height is not affected by this method.
//
void OMR::X86::Machine::fpStackCoerce(TR::Register *virtReg, int32_t stackLocation)
   {
   //
   // Check for stack overflow.
   //
   TR_ASSERT( (stackLocation < TR_X86FPStackRegister::fpStackFull),
            "fpStackCoerce() ==> Floating point stack overflow!" );

   // Check for stack underflow.
   //
   TR_ASSERT( (stackLocation > TR_X86FPStackRegister::fpStackEmpty),
            "fpStackCoerce() ==> Floating point stack underflow!" );

   virtReg->setAssignedRegister(_fpStack[stackLocation]);
   _fpStack[stackLocation]->setAssignedRegister(virtReg);
   _fpStack[stackLocation]->setState(TR::RealRegister::Assigned);
   }


// Pop a value from the FP stack.
//
TR::Register *OMR::X86::Machine::fpStackPop()
   {

   // Check for stack underflow.
   //
   TR_ASSERT( (_fpTopOfStack > TR_X86FPStackRegister::fpStackEmpty),
            "fpStackPop() ==> Floating point stack underflow!" );

   _fpStack[_fpTopOfStack]->setState(TR::RealRegister::Free);
   TR::Register *virtReg = _fpStack[_fpTopOfStack]->getAssignedRegister();
   virtReg->setAssignedRegister(NULL);
   _fpStack[_fpTopOfStack]->setAssignedRegister(NULL);

   _fpTopOfStack--;
   return virtReg;
   }


// Pop every value off the FP stack.
//
void OMR::X86::Machine::popEntireStack()
   {
   int32_t topOfStack = self()->getFPTopOfStack();
   for (int32_t i = 0; i < topOfStack+1; i++)
      self()->fpStackPop();
   }


// Exchange a register with the top register on the FP stack.
// It is assumed that both registers of interest are on the FP stack.
//
// If 'generateCode' is true then a corresponding FXCH instruction will be generated in the
// instruction stream.
//
TR::Instruction  *OMR::X86::Machine::fpStackFXCH(TR::Instruction *prevInstruction,
                                                TR::Register    *vreg,
                                                bool            generateCode)
   {

   TR_X86FPStackRegister *fpReg   = toX86FPStackRegister(vreg->getAssignedRegister());
   int32_t                vregNum = (int32_t) fpReg->getFPStackRegisterNumber();
   TR_X86FPStackRegister *pTop    = _fpStack[_fpTopOfStack];
   TR::Instruction     *cursor  = NULL;

   // Assertions about stack shape.
   //
   TR_ASSERT( ((_fpTopOfStack > TR_X86FPStackRegister::fpStackEmpty) &&
            (_fpTopOfStack < TR_X86FPStackRegister::fpStackFull)),
           "fpStackFXCH() ==> Stack top register is not on the FP stack!" );

   TR_ASSERT( ((vregNum > TR_X86FPStackRegister::fpStackEmpty) &&
            (vregNum < TR_X86FPStackRegister::fpStackFull)),
           "fpStackFXCH() ==> Virtual register is not on the FP stack!" );

   // Generate code to exchange two FP stack registers.
   //
   if (generateCode)
      {
      cursor = prevInstruction;
      TR::RealRegister *realFPReg = self()->fpMapToStackRelativeRegister(vreg);
      cursor = new (self()->cg()->trHeapMemory()) TR::X86FPRegInstruction(cursor, FXCHReg, realFPReg, self()->cg());
      }

   _fpStack[_fpTopOfStack] = _fpStack[vregNum];
   _fpStack[vregNum] = pTop;

   _fpStack[_fpTopOfStack]->setFPStackRegisterNumber( (TR_X86FPStackRegister::TR_X86FPStackRegisters)_fpTopOfStack);
   _fpStack[vregNum]->setFPStackRegisterNumber((TR_X86FPStackRegister::TR_X86FPStackRegisters)vregNum);

   return cursor;
   }


// Exchange a real FP stack relative register with the top register on the FP stack
// and generate code.  It is assumed that both registers of interest are on the FP stack.
//
TR::Instruction  *OMR::X86::Machine::fpStackFXCH(TR::Instruction *prevInstruction,
                                                int32_t         stackReg)
   {
   TR_X86FPStackRegister *pTop = _fpStack[_fpTopOfStack];
   int32_t vRegNum             = _fpTopOfStack - stackReg;

   // Assertions about stack shape.
   //
   TR_ASSERT( ((_fpTopOfStack > TR_X86FPStackRegister::fpStackEmpty) &&
            (_fpTopOfStack < TR_X86FPStackRegister::fpStackFull)),
           "fpStackFXCH() ==> Stack top register is not on the FP stack!" );

   TR_ASSERT( ((vRegNum > TR_X86FPStackRegister::fpStackEmpty) &&
            (vRegNum < TR_X86FPStackRegister::fpStackFull)),
           "fpStackFXCH() ==> Virtual register is not on the FP stack!" );

   // Generate code to exchange two FP stack registers.
   //
   TR::Instruction   *cursor = prevInstruction;
   TR::RealRegister  *realFPReg = self()->fpMapToStackRelativeRegister(stackReg);
   cursor = new (self()->cg()->trHeapMemory()) TR::X86FPRegInstruction(cursor, FXCHReg, realFPReg, self()->cg());

   _fpStack[_fpTopOfStack] = _fpStack[vRegNum];
   _fpStack[vRegNum] = pTop;

   _fpStack[_fpTopOfStack]->setFPStackRegisterNumber( (TR_X86FPStackRegister::TR_X86FPStackRegisters)_fpTopOfStack );
   _fpStack[vRegNum]->setFPStackRegisterNumber( (TR_X86FPStackRegister::TR_X86FPStackRegisters)vRegNum );

   return cursor;
   }


// Coerce registers X and Y into FP stack positions ST0 and ST1 in as few stack exchanges
// as possible.
//
// If 'strict' is true, X and Y will be exchanged into positions ST0 and ST1, respectively.
//
// If 'strict' is false, either X or Y will occupy ST0 and the other register will occupy ST1.
// The orientation will be chosen based on whichever results in the fewest possible register
// FXCHs to achieve.
//
// It is assumed that both X and Y are on the stack already.
//
void OMR::X86::Machine::fpCoerceRegistersToTopOfStack(TR::Instruction *prevInstruction,
                                                   TR::Register    *X,
                                                   TR::Register    *Y,
                                                   bool            strict)
   {

   TR_X86FPStackRegister *xReg       = toX86FPStackRegister(X->getAssignedRegister());
   int32_t                 x         = (int32_t) xReg->getFPStackRegisterNumber();
   TR_X86FPStackRegister *yReg       = toX86FPStackRegister(Y->getAssignedRegister());
   int32_t                 y         = (int32_t) yReg->getFPStackRegisterNumber();
   TR::Instruction      *cursor;

   // Quick exchange if the registers are the same.
   //
   if (X == Y)
      {
      if (x != _fpTopOfStack)
         {
         (void) self()->fpStackFXCH(prevInstruction, X);
         }
      return;
      }

   uint8_t n = 0;
   n |= (x == _fpTopOfStack)   ? 0x08 : 0x00;
   n |= (x == _fpTopOfStack-1) ? 0x04 : 0x00;
   n |= (y == _fpTopOfStack)   ? 0x02 : 0x00;
   n |= (y == _fpTopOfStack-1) ? 0x01 : 0x00;

   switch(n)
      {
      case 0:
         cursor = self()->fpStackFXCH(prevInstruction, Y);
         cursor = self()->fpStackFXCH(cursor, 1);
         (void) self()->fpStackFXCH(cursor, X);
         break;

      case 1:
         (void) self()->fpStackFXCH(prevInstruction, X);
         break;

      case 2:
         cursor = self()->fpStackFXCH(prevInstruction, 1);
         (void) self()->fpStackFXCH(cursor, X);
         break;

      case 4:
         cursor = self()->fpStackFXCH(prevInstruction, Y);

         if (strict)
            {
            (void) self()->fpStackFXCH(cursor, 1);
            }
         break;

      case 6:
         if (strict)
            {
            (void) self()->fpStackFXCH(prevInstruction, 1);
            }
         break;

      case 8:
         cursor = self()->fpStackFXCH(prevInstruction, 1);
         cursor = self()->fpStackFXCH(cursor, Y);

         if (strict)
            {
            (void) self()->fpStackFXCH(cursor, 1);
            }
         break;

      case 9:
         // X and Y are already in desired positions.
         break;

      default:
         diagnostic("fpCoerceRegistersToTopOfStack() ==> Invalid stack configuration!");
         break;
      }
   }


// Determine the reverse form of an IA32 FP instruction.
//
TR_X86OpCodes OMR::X86::Machine::fpDetermineReverseOpCode(TR_X86OpCodes op)
   {

   switch (op)
      {
      case FADDRegReg:
      case DADDRegReg:
      case FMULRegReg:
      case DMULRegReg:
      case FCOMRegReg:
      case DCOMRegReg:
         // op is unchanged for commutative instructions
         break;

      case FDIVRegReg:  op = FDIVRRegReg; break;
      case DDIVRegReg:  op = DDIVRRegReg; break;
      case FDIVRRegReg: op = FDIVRegReg;  break;
      case DDIVRRegReg: op = DDIVRegReg;  break;

      case FSUBRegReg:  op = FSUBRRegReg; break;
      case DSUBRegReg:  op = DSUBRRegReg; break;
      case FSUBRRegReg: op = FSUBRegReg;  break;
      case DSUBRRegReg: op = DSUBRegReg;  break;

      default:
         // no reverse instruction, return original
         break;
      }

   return op;
   }


// Determine the pop form of an IA32 FP instruction.
//
TR_X86OpCodes OMR::X86::Machine::fpDeterminePopOpCode(TR_X86OpCodes op)
   {

   switch (op)
      {
      case FADDRegReg:
      case DADDRegReg:    op = FADDPReg;  break;
      case FDIVRegReg:
      case DDIVRegReg:    op = FDIVPReg;  break;
      case FDIVRRegReg:
      case DDIVRRegReg:   op = FDIVRPReg; break;
      case FISTMemReg:    op = FISTPMem;  break;
      case DISTMemReg:    op = DISTPMem;  break;
      case FSSTMemReg:    op = FSSTPMem;  break;
      case DSSTMemReg:    op = DSSTPMem;  break;
      case FMULRegReg:
      case DMULRegReg:    op = FMULPReg;  break;
      case FSTMemReg:     op = FSTPMemReg;   break;
      case DSTMemReg:     op = DSTPMemReg;   break;
      case FSUBRegReg:
      case DSUBRegReg:    op = FSUBPReg;  break;
      case FSUBRRegReg:
      case DSUBRRegReg:   op = FSUBRPReg; break;
      case FCOMRegReg:
      case DCOMRegReg:    op = FCOMPReg;  break;
      case FCOMRegMem:    op = FCOMPMem;  break;
      case DCOMRegMem:    op = DCOMPMem;  break;
      case FCOMPReg:      op = FCOMPP;    break;
      case FCOMIRegReg:
      case DCOMIRegReg:   op = FCOMIPReg; break;
      case FSTRegReg:     op = FSTPReg;   break;
      case DSTRegReg:     op = DSTPReg;   break;

      default:
         // no pop instruction, return original
         break;
      }

   return op;
   }


// Find the next available FP register.  Since values must enter and leave
// the stack via the top of stack, this register will be the new top of stack.
//
// If this function returns NULL, a free register could not be found.  A spill
// is necessary to free up a register.
//
// Note that the caller of this function is responsible for updating the top
// of stack pointer.
//
TR_X86FPStackRegister *OMR::X86::Machine::findFreeFPRegister()
   {

   TR_X86FPStackRegister *freeRegister = NULL;
   int32_t                nextFPReg = _fpTopOfStack + 1;

   // If the stack is full then a spill is required.
   //
   if (nextFPReg < TR_X86FPStackRegister::fpStackFull)
      {
      freeRegister = _fpStack[nextFPReg];
      }

   return freeRegister;
   }


// Select and spill a register from a full FP stack.
//
TR::Instruction *OMR::X86::Machine::freeBestFPRegister(TR::Instruction *prevInstruction)
   {

   TR::Register *candidates[TR_X86FPStackRegister::NumRegisters];
   int          numCandidates = 0;

   TR_ASSERT( (_fpTopOfStack+1 == TR_X86FPStackRegister::fpStackFull),
            "OMR::X86::Machine::freeBestFPRegister() ==> FP stack is not full!" );

   // All assigned registers on the FP stack are candidates for spilling.
   // A register should be 'blocked' to remove it from spill consideration.
   //
   for (int i=TR_X86FPStackRegister::fpFirstStackReg; i < TR_X86FPStackRegister::NumRegisters; i++)
      {
      TR_X86FPStackRegister *stackReg = _fpStack[i];
      if (stackReg->getState() == TR::RealRegister::Assigned)
         {
         candidates[numCandidates++] = stackReg->getAssignedRegister();
         }
      }

   // Of the candidates identified, choose the one that is used in the most distant future.
   // Start with the first instruction following the current instruction.
   //
   //                                             Previous   ->    Current -> Next
   TR::Instruction  *cursor = prevInstruction->getNext()->getNext();
   while (numCandidates > 1                   &&
          cursor != NULL                      &&
          cursor->getOpCodeValue() != LABEL   &&
          cursor->getOpCodeValue() != RET     &&
          cursor->getOpCodeValue() != RETImm2 &&
          cursor->getOpCodeValue() != ReturnMarker &&
          !cursor->getOpCode().isBranchOp())
      {
      for (int i = 0; i < numCandidates; i++)
         {
         if (cursor->refsRegister(candidates[i]))
            {
            candidates[i] = candidates[--numCandidates];
            }
         }
      cursor = cursor->getNext();
      }

   // Spill the best register to memory.
   //
   return self()->fpSpillFPR(prevInstruction, candidates[0]);
   }


// Spill a single FPR from the FP stack to storage.
//
TR::Instruction *OMR::X86::Machine::fpSpillFPR(TR::Instruction *prevInstruction,
                                           TR::Register    *vreg)
   {

   TR_X86FPStackRegister *fpReg = toX86FPStackRegister(vreg->getAssignedRegister());
   TR::Instruction        *cursor = prevInstruction;

   TR_ASSERT( (fpReg != NULL), "OMR::X86::Machine::fpSpillFPR ==> Attempting to spill a non-existant register!" );

   if (fpReg != NULL)
      {
      // Bring spill register to top of stack.
      //
      if (!self()->isFPRTopOfStack(vreg))
         {
         cursor = self()->fpStackFXCH(cursor, vreg);
         }

      bool isFloat = vreg->isSinglePrecision(); // && debug("floatSpills");
      int32_t offset = 0;

      TR_BackingStore *location = self()->cg()->allocateSpill(isFloat? 4:8, false, &offset);
      TR::MemoryReference  *tempMR = generateX86MemoryReference(location->getSymbolReference(), offset, self()->cg());
      vreg->setBackingStorage(location);
      TR_ASSERT(offset == 0 || offset == 4, "assertion failure");
      vreg->setIsSpilledToSecondHalf(offset > 0);
      cursor = new (self()->cg()->trHeapMemory()) TR::X86FPMemRegInstruction(cursor,
                                              isFloat ? FSTPMemReg : DSTPMemReg,
                                              tempMR,
                                              self()->fpMapToStackRelativeRegister(vreg), self()->cg());
#ifdef DEBUG
      self()->cg()->incNumSpilledRegisters();
#endif
      }

   self()->fpStackPop();
   return cursor;
   }


// Bring a spilled FPR back onto the FP stack.
//
TR::Instruction *OMR::X86::Machine::reverseFPRSpillState(TR::Instruction *prevInstruction,
                                                     TR::Register    *spilledRegister)
   {
   TR::Instruction *cursor = prevInstruction;

   if (self()->isFPStackFull())
      {
      // Spill an FPR from the stack.
      //
      cursor = self()->freeBestFPRegister(cursor);
      }

   TR_BackingStore *location = spilledRegister->getBackingStorage();
   TR::MemoryReference  *tempMR = generateX86MemoryReference(location->getSymbolReference(), spilledRegister->isSpilledToSecondHalf()? 4 : 0, self()->cg());

   self()->fpStackPush(spilledRegister);

   bool isFloat = spilledRegister->isSinglePrecision(); // && debug("floatSpills");

   TR::RealRegister *realFPReg = self()->fpMapToStackRelativeRegister(spilledRegister);
   cursor = new (self()->cg()->trHeapMemory()) TR::X86FPRegMemInstruction(cursor,
                                           isFloat ? FLDRegMem : DLDRegMem,
                                           realFPReg,
                                           tempMR, self()->cg());

   self()->cg()->freeSpill(location, isFloat? 4:8, spilledRegister->isSpilledToSecondHalf()? 4:0);

   return cursor;
   }


// Spill all values on the FP stack to storage.  This will empty the FP stack.
//
TR::Instruction *OMR::X86::Machine::fpSpillStack(TR::Instruction *prevInstruction)
   {
   TR::Instruction *cursor = prevInstruction;
   int32_t top = _fpTopOfStack;

   while (top != TR_X86FPStackRegister::fpStackEmpty)
      {
      TR::Register *vreg = _fpStack[top]->getAssignedRegister();
      cursor = self()->fpSpillFPR(cursor, vreg);
      top = _fpTopOfStack;
      }

   return cursor;
   }


#if defined(DEBUG)
static void printOneRegisterStatus(TR_FrontEnd *fe, TR::FILE *pOutFile, TR_Debug *debug, TR::RealRegister *reg)
   {
   trfprintf(pOutFile,"                      ");
   debug->printFullRegInfo(pOutFile, reg);
   }

void OMR::X86::Machine::printGPRegisterStatus(TR_FrontEnd *fe, TR::RealRegister ** registerFile, TR::FILE *pOutFile)
   {
   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile, "\n  GP Reg Status:          Register         State        Assigned\n");
   int i;
   for (i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastAssignableGPR; i++)
      {
      printOneRegisterStatus(fe, pOutFile, self()->getDebug(), registerFile[i]);
      }
   for (i = TR::RealRegister::FirstXMMR; i <= TR::RealRegister::LastXMMR; i++)
      {
      printOneRegisterStatus(fe, pOutFile, self()->getDebug(), registerFile[i]);
      }

   trfflush(pOutFile);
   }

// Dump the FP register stack
//
void OMR::X86::Machine::printFPRegisterStatus(TR_FrontEnd *fe, TR::FILE *pOutFile)
   {
   char buf[32];
   char *cursor;
   int32_t i,j;

   if (pOutFile == NULL)
      return;

   trfprintf(pOutFile, "\n  FP Reg Status:          Register         State        Assigned      Total Future\n");
   for (i=_fpTopOfStack, j=0; i != TR_X86FPStackRegister::fpStackEmpty; i--, j++)
      {
      memset(buf, ' ', 25);
      cursor = buf+17;
      sprintf(cursor, "st%d: ",j);
      trfprintf(pOutFile, buf);
      self()->getDebug()->printFullRegInfo(pOutFile, _fpStack[i]->getAssignedRegister());
      }

   for (i=j; i<8; i++)
      {
      memset(buf, ' ', 25);
      cursor = buf+17;
      sprintf(cursor, "st%d:",i);
      trfprintf(pOutFile, "%s [ empty      ]\n", buf);
      }

   trfflush(pOutFile);
   }
#endif
