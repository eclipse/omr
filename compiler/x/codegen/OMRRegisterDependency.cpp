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

#include <algorithm>                             // for std::find, etc
#include <stdint.h>                              // for int32_t, uint32_t, etc
#include <string.h>                              // for NULL, memset
#include "codegen/BackingStore.hpp"              // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator
#include "codegen/Instruction.hpp"               // for Instruction
#include "codegen/LiveRegister.hpp"              // for TR_LiveRegisters
#include "codegen/Machine.hpp"                   // for Machine
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"              // for RealRegister, etc
#include "codegen/Register.hpp"                  // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"  // for RegisterDependency, etc
#include "codegen/RegisterPair.hpp"              // for RegisterPair
#include "compile/Compilation.hpp"               // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                          // for Block
#include "il/DataTypes.hpp"                      // for DataTypes::Double, etc
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"                           // for Node
#include "il/Node_inlines.hpp"                   // for Node::getDataType, etc
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/List.hpp"                        // for List, ListIterator
#include "ras/Debug.hpp"                         // for TR_DebugBase
#include "ras/DebugCounter.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                  // for LRegMem, MOVRegReg, etc
#include "x/codegen/X86Register.hpp"

////////////////////////
// Hack markers
//

// TODO:AMD64: IA32 FPR GRA currently interferes XMM GRA in a somewhat
// confusing way that doesn't occur on AMD64.
#define XMM_GPRS_USE_DISTINCT_NUMBERS (TR::Compiler->target.is64Bit())

static void generateRegcopyDebugCounter(TR::CodeGenerator *cg, const char *category)
   {
   if (!cg->comp()->getOptions()->enableDebugCounters())
      return;
   TR::TreeTop *tt = cg->getCurrentEvaluationTreeTop();
   const char *fullName = TR::DebugCounter::debugCounterName(cg->comp(),
      "regcopy/cg.genRegDepConditions/%s/(%s)/%s/block_%d",
      category,
      cg->comp()->signature(),
      cg->comp()->getHotnessName(cg->comp()->getMethodHotness()),
      tt->getEnclosingBlock()->getNumber());
   TR::Instruction *cursor = cg->lastInstructionBeforeCurrentEvaluationTreeTop();
   cg->generateDebugCounter(cursor, fullName, 1, TR::DebugCounter::Undetermined);
   }

OMR::X86::RegisterDependencyConditions::RegisterDependencyConditions(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR_X86RegisterDependencyIndex additionalRegDeps,
      List<TR::Register> *popRegisters)
   :_numPreConditions(-1),_numPostConditions(-1),
    _addCursorForPre(0),_addCursorForPost(0)
   {
   TR::Register      *copyReg = NULL;
   TR::Register      *highCopyReg = NULL;
   List<TR::Register> registers(cg->trMemory());
   TR_X86RegisterDependencyIndex          numFPGlobalRegs = 0,
                     totalNumFPGlobalRegs = 0;
   int32_t           i;
   TR::Machine *machine = cg->machine();
   TR::Compilation *comp = cg->comp();

   int32_t numLongs = 0;

   // Pre-compute how many x87 FP stack slots we need to accommodate all the FP global registers.
   //
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node     *child = node->getChild(i);
      TR::Register *reg   = child->getRegister();

      if (reg->getKind() == TR_GPR)
         {
         if (child->getHighGlobalRegisterNumber() > -1)
            numLongs++;
         }
      else if (reg->getKind() == TR_X87)
         {
         totalNumFPGlobalRegs++;
         }
      }

   _preConditions = TR_X86RegisterDependencyGroup::create(node->getNumChildren() + additionalRegDeps + numLongs, cg->trMemory());
   _postConditions = TR_X86RegisterDependencyGroup::create(node->getNumChildren() + additionalRegDeps + numLongs, cg->trMemory());
   _numPreConditions = node->getNumChildren() + additionalRegDeps + numLongs;
   _numPostConditions = node->getNumChildren() + additionalRegDeps + numLongs;

   int32_t numLongsAdded = 0;

   for (i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node                 *child        = node->getChild(i);
      TR::Register             *globalReg    = child->getRegister();
      TR::Register             *highGlobalReg = NULL;
      TR_GlobalRegisterNumber  globalRegNum = child->getGlobalRegisterNumber();
      TR_GlobalRegisterNumber  highGlobalRegNum = child->getHighGlobalRegisterNumber();

      TR::RealRegister::RegNum realRegNum = TR::RealRegister::NoReg, realHighRegNum = TR::RealRegister::NoReg;
      if (globalReg->getKind() == TR_GPR || globalReg->getKind() == TR_VRF || XMM_GPRS_USE_DISTINCT_NUMBERS)
         {
         realRegNum = (TR::RealRegister::RegNum) cg->getGlobalRegister(globalRegNum);

         if (highGlobalRegNum > -1)
            realHighRegNum = (TR::RealRegister::RegNum)
                             cg->getGlobalRegister(highGlobalRegNum);
         }
      else if (globalReg->getKind() == TR_FPR)
         {
         // Convert the global register number from an st0-based value to an xmm0-based one.
         //
         realRegNum = (TR::RealRegister::RegNum)
                         (cg->getGlobalRegister(globalRegNum)
                            - TR::RealRegister::FirstFPR
                            + TR::RealRegister::FirstXMMR);

         // Find the global register that has been allocated in this XMMR, if any.
         //
         TR::Register *xmmGlobalReg = cg->machine()->getXMMGlobalRegister(realRegNum - TR::RealRegister::FirstXMMR);
         if (xmmGlobalReg)
            globalReg = xmmGlobalReg;
         }
      else
         {
         TR_ASSERT(globalReg->getKind() == TR_X87, "invalid global register kind\n");

         // Compute the real global register number based on the number of global FPRs allocated so far.
         //
         realRegNum = (TR::RealRegister::RegNum)
                         (TR::RealRegister::FirstFPR + (totalNumFPGlobalRegs - numFPGlobalRegs - 1));

         // Find the global register that has been allocated in this FP stack slot, if any.
         //
         int32_t fpStackSlot = child->getGlobalRegisterNumber() - machine->getNumGlobalGPRs();
         TR::Register *fpGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
         if (fpGlobalReg)
            globalReg = fpGlobalReg;

         numFPGlobalRegs++;
         }

      if (registers.find(globalReg))
         {
         if (globalReg->getKind() == TR_GPR)
            {
            bool containsInternalPointer = false;
            TR::RegisterPair *globalRegPair = globalReg->getRegisterPair();
            if (globalRegPair)
               {
               highGlobalReg = globalRegPair->getHighOrder();
               globalReg = globalRegPair->getLowOrder();
               }
            else if (child->getHighGlobalRegisterNumber() > -1)
               TR_ASSERT(0, "Long child does not have a register pair\n");

            if (globalReg->getPinningArrayPointer())
               containsInternalPointer = true;

            copyReg = (globalReg->containsCollectedReference() && !containsInternalPointer) ?
                      cg->allocateCollectedReferenceRegister() : cg->allocateRegister();

            if (containsInternalPointer)
               {
               copyReg->setContainsInternalPointer();
               copyReg->setPinningArrayPointer(globalReg->getPinningArrayPointer());
               }

            TR::Instruction *prevInstr = cg->getAppendInstruction();
            TR::Instruction *placeToAdd = prevInstr;
            if (prevInstr && prevInstr->getOpCode().isFusableCompare())
               {
               TR::Instruction *prevPrevInstr = prevInstr->getPrev();
               if (prevPrevInstr)
                  {
                  if (comp->getOption(TR_TraceCG))
                     traceMsg(comp, "Moving reg reg copy earlier (after %p) in %s\n", prevPrevInstr, comp->signature());
                  placeToAdd = prevPrevInstr;
                  }
               }

            generateRegRegInstruction(placeToAdd, MOVRegReg(), copyReg, globalReg, cg);

            if (highGlobalReg)
               {
               generateRegcopyDebugCounter(cg, "gpr-pair");
               containsInternalPointer = false;
               if (highGlobalReg->getPinningArrayPointer())
                  containsInternalPointer = true;

               highCopyReg = (highGlobalReg->containsCollectedReference() && !containsInternalPointer) ?
                             cg->allocateCollectedReferenceRegister() : cg->allocateRegister();

               if (containsInternalPointer)
                  {
                  highCopyReg->setContainsInternalPointer();
                  highCopyReg->setPinningArrayPointer(highGlobalReg->getPinningArrayPointer());
                  }

               generateRegRegInstruction(MOV4RegReg, node, highCopyReg, highGlobalReg, cg);
               }
            else
               {
               generateRegcopyDebugCounter(cg, "gpr");
               highCopyReg = NULL;
               }
            }
         else if (globalReg->getKind() == TR_FPR)
            {
            generateRegcopyDebugCounter(cg, "fpr");
            if (globalReg->isSinglePrecision())
               {
               copyReg = cg->allocateSinglePrecisionRegister(TR_FPR);
               generateRegRegInstruction(MOVAPSRegReg, node, copyReg, child->getRegister(), cg);
               }
            else
               {
               copyReg = cg->allocateRegister(TR_FPR);
               generateRegRegInstruction(MOVAPDRegReg, node, copyReg, child->getRegister(), cg);
               }
            }
         else if (globalReg->getKind() == TR_VRF)
            {
            generateRegcopyDebugCounter(cg, "vrf");
            copyReg = cg->allocateRegister(TR_VRF);
            generateRegRegInstruction(MOVDQURegReg, node, copyReg, child->getRegister(), cg);
            }
         else
            {
            generateRegcopyDebugCounter(cg, "x87");
            if (globalReg->isSinglePrecision())
               {
               copyReg = cg->allocateSinglePrecisionRegister(TR_X87);
               }
            else
               {
               copyReg = cg->allocateRegister(TR_X87);
               }

            generateFPST0STiRegRegInstruction(FLDRegReg, node, copyReg, child->getRegister(), cg);

            int32_t fpRegNum = child->getGlobalRegisterNumber() - machine->getNumGlobalGPRs();
            //dumpOptDetails("FP reg num %d global reg num %d global reg %s\n", fpRegNum, child->getGlobalRegisterNumber(), getDebug()->getName(globalReg));
            cg->machine()->setCopiedFPStackRegister(fpRegNum, toX86FPStackRegister(globalReg));
            if (child->getOpCodeValue() == TR::PassThrough)
               {
               TR::Node *fpChild = child->getFirstChild();
               cg->machine()->setFPStackRegisterNode(fpRegNum, fpChild);
               }
            else
               cg->machine()->setFPStackRegisterNode(fpRegNum, child);

            int32_t i;
            for (i=0;i<TR_X86FPStackRegister::NumRegisters;i++)
               {
               TR::Register *reg = cg->machine()->getFPStackRegister(i);
               if ((reg == globalReg) &&
                   (i != fpRegNum))
                  {
                    //dumpOptDetails("FP reg num %d global reg num %d global reg %s\n", i, child->getGlobalRegisterNumber(), getDebug()->getName(copyReg));
                  cg->machine()->setCopiedFPStackRegister(i, toX86FPStackRegister(copyReg));
                  if (child->getOpCodeValue() == TR::PassThrough)
                     {
                     TR::Node *fpChild = child->getFirstChild();
                     cg->machine()->setFPStackRegisterNode(i, fpChild);
                     }
                  else
                     cg->machine()->setFPStackRegisterNode(i, child);

                  break;
                  }
               }

            }

         globalReg = copyReg;
         highGlobalReg = highCopyReg;
         }
      else
         {
         // Change the register of the floating-point child of a PassThrough,
         // as we must have a register that is live as the global register
         // corresponding to the child.
         //
         if (child->getOpCodeValue() == TR::PassThrough && globalReg->getKind() == TR_X87)
            {
            TR::Node *fpChild = child->getFirstChild();

            if ((fpChild->getDataType() == TR::Float) || (fpChild->getDataType() == TR::Double))
               {

               // if the child's register is live on another stack slot, dont change the register.
               TR::Register *fpGlobalReg = NULL;
               for (int j = 0 ; j < TR_X86FPStackRegister::NumRegisters && fpGlobalReg != child->getRegister(); j++)
                  {
                  fpGlobalReg = cg->machine()->getFPStackRegister(j);
                  }

               if (fpGlobalReg != child->getRegister())
                  {
                  fpChild->setRegister(globalReg);
                  machine->setFPStackRegister(child->getGlobalRegisterNumber() - machine->getNumGlobalGPRs(), toX86FPStackRegister(globalReg));
                  }
               }
            }

         registers.add(globalReg);
         TR::RegisterPair *globalRegPair = globalReg->getRegisterPair();
         if (globalRegPair)
            {
            highGlobalReg = globalRegPair->getHighOrder();
            globalReg = globalRegPair->getLowOrder();
            }
         else if (child->getHighGlobalRegisterNumber() > -1)
            TR_ASSERT(0, "Long child does not have a register pair\n");
         }

      if (globalReg->getKind() == TR_GPR)
         {
         addPreCondition(globalReg, realRegNum, cg);
         addPostCondition(globalReg, realRegNum, cg);
         if (highGlobalReg)
            {
            numLongsAdded++;
            addPreCondition(highGlobalReg, realHighRegNum, cg);
            addPostCondition(highGlobalReg, realHighRegNum, cg);
            }
         }
      else if (globalReg->getKind() == TR_FPR || globalReg->getKind() == TR_VRF)
         {
         addPreCondition(globalReg, realRegNum, cg);
         addPostCondition(globalReg, realRegNum, cg);
         cg->machine()->setXMMGlobalRegister(realRegNum - TR::RealRegister::FirstXMMR, globalReg);
         }
      else if (globalReg->getKind() == TR_X87)
         {
         addPreCondition(globalReg, realRegNum, cg, UsesGlobalDependentFPRegister);
         addPostCondition(globalReg, realRegNum, cg, UsesGlobalDependentFPRegister);
         int32_t fpRegNum = child->getGlobalRegisterNumber() - machine->getNumGlobalGPRs();
         cg->machine()->setFPStackRegister(fpRegNum, toX86FPStackRegister(globalReg));
         }

      // If the register dependency isn't actually used
      // (dead store elimination probably removed it)
      // and this is a float then we have to pop it off the stack.
      //
      if (child->getOpCodeValue() == TR::PassThrough)
         {
         child = child->getFirstChild();
         }

      if (popRegisters &&
          globalReg->getKind() == TR_X87 &&
          child->getReferenceCount() == 0 &&
          (child->getDataType() == TR::Float || child->getDataType() == TR::Double))
         {
         popRegisters->add(globalReg);
         }
      else if (copyReg)
         {
         cg->stopUsingRegister(copyReg);
         if (highCopyReg)
            cg->stopUsingRegister(highCopyReg);
         }
      }

   if (numLongs != numLongsAdded)
     TR_ASSERT(0, "Mismatch numLongs %d numLongsAdded %d\n", numLongs, numLongsAdded);
   }


TR_X86RegisterDependencyIndex OMR::X86::RegisterDependencyConditions::unionDependencies(
   TR_X86RegisterDependencyGroup *deps,
   TR_X86RegisterDependencyIndex  cursor,
   TR::Register                  *vr,
   TR::RealRegister::RegNum       rr,
   TR::CodeGenerator             *cg,
   uint8_t                        flag,
   bool                           isAssocRegDependency)
   {
   TR::Machine *machine = cg->machine();

   // If vr is already in deps, combine the existing association with rr.
   //
   if (vr)
      {
      if (vr->getRealRegister())
         {
         TR::RealRegister::RegNum vrIndex = toRealRegister(vr)->getRegisterNumber();
         switch (vrIndex)
            {
            case TR::RealRegister::esp:
            case TR::RealRegister::vfp:
               // esp and vfp are ok, and are ignorable, since local RA already
               // deals with those without help from regdeps.
               //
               //
               TR_ASSERT(rr == vrIndex || rr == TR::RealRegister::NoReg, "esp and vfp can be assigned only to themselves or NoReg, not %s", cg->getDebug()->getRealRegisterName(rr));
               break;
            default:
               TR_ASSERT(0, "Unexpected real register %s in regdep", cg->getDebug()->getName(vr, TR_UnknownSizeReg));
               break;
            }
         return cursor;
         }

      for (TR_X86RegisterDependencyIndex candidate = 0; candidate < cursor; candidate++)
         {
         TR::RegisterDependency  *dep = deps->getRegisterDependency(candidate);
         if (dep->getRegister() == vr)
            {
            // Keep the stronger of the two constraints
            //
            TR::RealRegister::RegNum min = std::min(rr, dep->getRealRegister());
            TR::RealRegister::RegNum max = std::max(rr, dep->getRealRegister());
            if (min == TR::RealRegister::NoReg)
               {
               // Anything is stronger than NoReg
               deps->setDependencyInfo(candidate, vr, max, cg, flag, isAssocRegDependency);
               }
            else if(max == TR::RealRegister::ByteReg)
               {
               // Specific regs are stronger than ByteReg
               TR_ASSERT(min <= TR::RealRegister::Last8BitGPR, "Only byte registers are compatible with ByteReg dependency");
               deps->setDependencyInfo(candidate, vr, min, cg, flag, isAssocRegDependency);
               }
            else
               {
               // This assume fires for some assocRegs instructions
               //TR_ASSERT(min == max, "Specific register dependency is only compatible with itself");
               if (min != max)
                  continue;

               // Nothing to do
               }

            return cursor;
            }
         }
      }

   // vr is not already in deps, so add a new dep
   //
   deps->setDependencyInfo(cursor++, vr, rr, cg, flag, isAssocRegDependency);
   return cursor;
   }

void OMR::X86::RegisterDependencyConditions::unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg)
   {
   unionPostCondition(reg, TR::RealRegister::NoReg, cg);
   }


TR_X86RegisterDependencyIndex OMR::X86::RegisterDependencyConditions::unionRealDependencies(
   TR_X86RegisterDependencyGroup *deps,
   TR_X86RegisterDependencyIndex  cursor,
   TR::Register                  *vr,
   TR::RealRegister::RegNum       rr,
   TR::CodeGenerator             *cg,
   uint8_t                        flag,
   bool                           isAssocRegDependency)
   {
   TR::Machine *machine = cg->machine();
   // A vmThread/ebp dependency can be ousted by any other ebp dependency.
   // This situation should only occur when ebp is assigned as a global register.
   static TR::RealRegister::RegNum vmThreadRealRegisterIndex = TR::RealRegister::ebp;
   if (rr == vmThreadRealRegisterIndex)
      {
      depsize_t candidate;
      TR::Register *vmThreadRegister = cg->getVMThreadRegister();
      for (candidate = 0; candidate < cursor; candidate++)
         {
         TR::RegisterDependency  *dep = deps->getRegisterDependency(candidate);
         // Check for a pre-existing ebp dependency.
         if (dep->getRealRegister() == vmThreadRealRegisterIndex)
            {
            // Any ebp dependency is stronger than a vmThread/ebp dependency.
            // Oust any vmThread dependency.
            if (dep->getRegister() == vmThreadRegister)
               {
               //diagnostic("\nEnvicting virt reg %s dep for %s replaced with virt reg %s\n      {\"%s\"}",
               //   getDebug()->getName(dep->getRegister()),getDebug()->getName(machine->getX86RealRegister(rr)),getDebug()->getName(vr), cg->comp()->getCurrentMethod()->signature());
               deps->setDependencyInfo(candidate, vr, rr, cg, flag, isAssocRegDependency);
               }
            else
               {
               //diagnostic("\nSkipping virt reg %s dep for %s in favour of %s\n     {%s}}\n",
               //   getDebug()->getName(vr),getDebug()->getName(machine->getX86RealRegister(rr)),getDebug()->getName(dep->getRegister()), cg->comp()->getCurrentMethod()->signature());
               TR_ASSERT(vr == vmThreadRegister, "Conflicting EBP register dependencies.\n");
               }
            return cursor;
            }
         }
      }

   // Not handled above, so add a new dependency.
   //
   deps->setDependencyInfo(cursor++, vr, rr, cg, flag, isAssocRegDependency);
   return cursor;
   }


TR::RegisterDependencyConditions  *OMR::X86::RegisterDependencyConditions::clone(TR::CodeGenerator *cg, TR_X86RegisterDependencyIndex additionalRegDeps)
   {
   TR::RegisterDependencyConditions  *other =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(_numPreConditions  + additionalRegDeps,
                                              _numPostConditions + additionalRegDeps, cg->trMemory());
   int32_t i;

   for (i = _numPreConditions-1; i >= 0; --i)
      {
      TR::RegisterDependency  *dep = getPreConditions()->getRegisterDependency(i);
      other->getPreConditions()->setDependencyInfo(i, dep->getRegister(), dep->getRealRegister(), cg, dep->getFlags());
      }

   for (i = _numPostConditions-1; i >= 0; --i)
      {
      TR::RegisterDependency  *dep = getPostConditions()->getRegisterDependency(i);
      other->getPostConditions()->setDependencyInfo(i, dep->getRegister(), dep->getRealRegister(), cg, dep->getFlags());
      }

   other->setAddCursorForPre(_addCursorForPre);
   other->setAddCursorForPost(_addCursorForPost);
   return other;
   }


bool OMR::X86::RegisterDependencyConditions::refsRegister(TR::Register *r)
   {

   for (int i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getRefsRegister())
         {
         return true;
         }
      }

   for (int j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getRefsRegister())
         {
         return true;
         }
      }

   return false;
   }


bool OMR::X86::RegisterDependencyConditions::defsRegister(TR::Register *r)
   {

   for (int i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }

   for (int j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }

   return false;
   }


bool OMR::X86::RegisterDependencyConditions::usesRegister(TR::Register *r)
   {
   for (int i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getUsesRegister())
         {
         return true;
         }
      }

   for (int j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getUsesRegister())
         {
         return true;
         }
      }

   return false;
   }


void OMR::X86::RegisterDependencyConditions::useRegisters(TR::Instruction *instr, TR::CodeGenerator *cg)
   {
   int32_t i;

   for (i = 0; i < _numPreConditions; i++)
      {
      TR::Register *virtReg = _preConditions->getRegisterDependency(i)->getRegister();
      if (virtReg)
         {
         instr->useRegister(virtReg);
         }
      }

   for (i = 0; i < _numPostConditions; i++)
      {
      TR::Register *virtReg = _postConditions->getRegisterDependency(i)->getRegister();
      if (virtReg)
         {
         instr->useRegister(virtReg);
         }
      }
   }


void TR_X86RegisterDependencyGroup::blockRealDependencyRegisters(TR_X86RegisterDependencyIndex numberOfRegisters, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   for (TR_X86RegisterDependencyIndex i = 0; i < numberOfRegisters; i++)
      {
      if (_dependencies[i].getRealRegister() != TR::RealRegister::NoReg)
         {
         machine->getX86RealRegister(_dependencies[i].getRealRegister())->block();
         }
      }
   }


void TR_X86RegisterDependencyGroup::unblockRealDependencyRegisters(TR_X86RegisterDependencyIndex numberOfRegisters, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   for (TR_X86RegisterDependencyIndex i = 0; i < numberOfRegisters; i++)
      {
      if (_dependencies[i].getRealRegister() != TR::RealRegister::NoReg)
         {
         machine->getX86RealRegister(_dependencies[i].getRealRegister())->unblock();
         }
      }
   }


void TR_X86RegisterDependencyGroup::assignRegisters(TR::Instruction   *currentInstruction,
                                                    TR_RegisterKinds  kindsToBeAssigned,
                                                    TR_X86RegisterDependencyIndex          numberOfRegisters,
                                                    TR::CodeGenerator *cg)
   {
   TR::Register             *virtReg              = NULL;
   TR::RealRegister         *assignedReg          = NULL;
   TR::RealRegister::RegNum  dependentRegNum      = TR::RealRegister::NoReg;
   TR::RealRegister         *dependentRealReg     = NULL;
   TR::RealRegister         *bestFreeRealReg      = NULL;
   TR::RealRegister::RegNum  bestFreeRealRegIndex = TR::RealRegister::NoReg;
   bool                      changed;
   int                       i, j;
   TR::Compilation *comp = cg->comp();

   TR::Machine *machine = cg->machine();
   blockRegisters(numberOfRegisters);

   // Build a work list of assignable register dependencies so the test does not
   // have to be repeated on each pass.  Also, order the list so that real
   // associations appear first, followed by byte reg associations, followed by
   // NoReg associations.
   //
   TR::RegisterDependency  **dependencies =
      (TR::RegisterDependency  **)cg->trMemory()->allocateStackMemory(numberOfRegisters * sizeof(TR::RegisterDependency  *));

   bool    hasByteDeps = false;
   bool    hasNoRegDeps = false;
   bool    hasBestFreeRegDeps = false;
   int32_t numRealAssocRegisters;
   int32_t numDependencyRegisters = 0;
   int32_t firstByteRegAssoc = 0, lastByteRegAssoc = 0;
   int32_t firstNoRegAssoc = 0, lastNoRegAssoc = 0;
   int32_t bestFreeRegAssoc = 0;

   for (i=0; i<numberOfRegisters; i++)
      {
      virtReg = _dependencies[i].getRegister();
      dependentRegNum = _dependencies[i].getRealRegister();

      if (virtReg && (kindsToBeAssigned & virtReg->getKindAsMask())
          && dependentRegNum != TR::RealRegister::NoReg
          && dependentRegNum != TR::RealRegister::ByteReg
          && dependentRegNum != TR::RealRegister::SpilledReg
          && dependentRegNum != TR::RealRegister::BestFreeReg)
         {
         dependencies[numDependencyRegisters++] = getRegisterDependency(i);
         }
      else if (dependentRegNum == TR::RealRegister::NoReg)
         hasNoRegDeps = true;
      else if (dependentRegNum == TR::RealRegister::ByteReg)
         hasByteDeps = true;
      else if (dependentRegNum == TR::RealRegister::BestFreeReg)
         hasBestFreeRegDeps = true;

      // Handle spill registers first.
      //
      if (dependentRegNum == TR::RealRegister::SpilledReg)
         {
         if (cg->getUseNonLinearRegisterAssigner())
            {
            if (virtReg->getAssignedRegister())
               {
               TR_ASSERT(virtReg->getBackingStorage(), "should have a backing store for spilled reg virtuals");
               assignedReg = toRealRegister(virtReg->getAssignedRegister());

               TR::MemoryReference *tempMR = generateX86MemoryReference(virtReg->getBackingStorage()->getSymbolReference(), cg);

               TR_X86OpCodes op;
               if (assignedReg->getKind() == TR_FPR)
                  {
                  op = (assignedReg->isSinglePrecision()) ? MOVSSRegMem : (cg->getXMMDoubleLoadOpCode());
                  }
               else if (assignedReg->getKind() == TR_VRF)
                  {
                  op = MOVDQURegMem;
                  }
               else
                  {
                  op = LRegMem();
                  }

               TR::X86RegMemInstruction *inst =
                  new (cg->trHeapMemory()) TR::X86RegMemInstruction(currentInstruction, op, assignedReg, tempMR, cg);

               assignedReg->setAssignedRegister(NULL);
               virtReg->setAssignedRegister(NULL);
               assignedReg->setState(TR::RealRegister::Free);

               if (comp->getOptions()->getOption(TR_TraceNonLinearRegisterAssigner))
                  {
                  cg->traceRegisterAssignment("Generate reload of virt %s due to spillRegIndex dep at inst %p\n",comp->getDebug()->getName(virtReg),currentInstruction);
                  cg->traceRAInstruction(inst);
                  }
               }

            if (!(std::find(cg->getSpilledRegisterList()->begin(), cg->getSpilledRegisterList()->end(), virtReg) != cg->getSpilledRegisterList()->end()))
               {
               cg->getSpilledRegisterList()->push_front(virtReg);
               }
            }
         }
      }

   numRealAssocRegisters = numDependencyRegisters;

   if (hasByteDeps)
      {
      firstByteRegAssoc = numDependencyRegisters;
      for (i=0; i<numberOfRegisters; i++)
         {
         virtReg = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();
         if (virtReg && (kindsToBeAssigned & virtReg->getKindAsMask()) &&
             dependentRegNum == TR::RealRegister::ByteReg)
            {
            dependencies[numDependencyRegisters++] = getRegisterDependency(i);
            }
         }

      lastByteRegAssoc = numDependencyRegisters-1;
      }

   if (hasNoRegDeps)
      {
      firstNoRegAssoc = numDependencyRegisters;
      for (i=0; i<numberOfRegisters; i++)
         {
         virtReg = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();
         if (virtReg && (kindsToBeAssigned & virtReg->getKindAsMask()) &&
             dependentRegNum == TR::RealRegister::NoReg)
            {
            dependencies[numDependencyRegisters++] = getRegisterDependency(i);
            }
         }

      lastNoRegAssoc = numDependencyRegisters-1;
      }

   if (hasBestFreeRegDeps)
      {
      bestFreeRegAssoc = numDependencyRegisters;
      for (i=0; i<numberOfRegisters; i++)
         {
         virtReg = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();
         if (virtReg && (kindsToBeAssigned & virtReg->getKindAsMask()) &&
             dependentRegNum == TR::RealRegister::BestFreeReg)
            {
            dependencies[numDependencyRegisters++] = getRegisterDependency(i);
            }
         }

      TR_ASSERT((bestFreeRegAssoc == numDependencyRegisters-1), "there can only be one bestFreeRegDep in a dependency list");
      }

   // First look for long shot where two registers can be xchg'ed into their
   // proper real registers in one shot (seems to happen more often than one would think)
   //
   if (numRealAssocRegisters > 1)
      {
      for (i = 0; i < numRealAssocRegisters - 1; i++)
         {
         virtReg = dependencies[i]->getRegister();

         // We can only perform XCHG on GPRs.
         //
         if (virtReg->getKind() == TR_GPR)
            {
            dependentRegNum  = dependencies[i]->getRealRegister();
            dependentRealReg = machine->getX86RealRegister(dependentRegNum);
            assignedReg      = NULL;
            TR::RealRegister::RegNum assignedRegNum = TR::RealRegister::NoReg;
            if (virtReg->getAssignedRegister())
               {
               assignedReg    = toRealRegister(virtReg->getAssignedRegister());
               assignedRegNum = assignedReg->getRegisterNumber();
               }

            if (dependentRealReg->getState() == TR::RealRegister::Blocked &&
                dependentRegNum != assignedRegNum)
               {
               for (j = i+1; j < numRealAssocRegisters; j++)
                  {
                  TR::Register             *virtReg2         = dependencies[j]->getRegister();
                  TR::RealRegister::RegNum  dependentRegNum2 = dependencies[j]->getRealRegister();
                  TR::RealRegister         *assignedReg2     = NULL;
                  TR::RealRegister::RegNum  assignedRegNum2  = TR::RealRegister::NoReg;
                  if (virtReg2 && virtReg2->getAssignedRegister())
                     {
                     assignedReg2    = toRealRegister(virtReg2->getAssignedRegister());
                     assignedRegNum2 = assignedReg2->getRegisterNumber();
                     }

                  if (assignedRegNum2 == dependentRegNum &&
                      assignedRegNum == dependentRegNum2)
                     {
                     machine->swapGPRegisters(currentInstruction, assignedRegNum, assignedRegNum2);
                     break;
                     }
                  }
               }
            }
         }
      }

   // Next find registers for which there is an identified real register
   // and that register is currently free.  Want to do these first to get these
   // registers out of their existing registers and possibly freeing those registers
   // up to also be filled by the proper candidates as simply as possible
   //
   do
      {
      changed = false;
      for (i = 0; i < numRealAssocRegisters; i++)
         {
         virtReg = dependencies[i]->getRegister();
         dependentRegNum = dependencies[i]->getRealRegister();
         dependentRealReg = machine->getX86RealRegister(dependentRegNum);
         if (dependentRealReg->getState() == TR::RealRegister::Free)
            {
            if (virtReg->getKind() == TR_FPR || virtReg->getKind() == TR_VRF)
               machine->coerceXMMRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            else
               machine->coerceGPRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            virtReg->block();
            changed = true;
            }
         }
      } while (changed);

   // Next find registers for which there is an identified real register, but the
   // register is not yet assigned.  We could do this at the same time as the previous
   // loop, but that could lead to some naive changes that would have been easier once
   // all the opportunities found by the first loop have been found.
   //
   do
      {
      changed = false;
      for (i = 0; i < numRealAssocRegisters; i++)
         {
         virtReg = dependencies[i]->getRegister();
         assignedReg = toRealRegister(virtReg->getAssignedRealRegister());
         dependentRegNum = dependencies[i]->getRealRegister();
         dependentRealReg = machine->getX86RealRegister(dependentRegNum);
         if (dependentRealReg != assignedReg)
            {
            if (virtReg->getKind() == TR_FPR || virtReg->getKind() == TR_VRF)
               machine->coerceXMMRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            else
               machine->coerceGPRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            virtReg->block();
            changed = true;
            }
         }
      } while (changed);

   // Now all virtual registers which require a particular real register should
   // be in that register, so now find registers for which there is no required
   // real assignment, but which are not currently assigned to a real register
   // and make sure they are assigned.
   //
   // Assign byte register dependencies first.  Unblock any NoReg dependencies to not
   // constrain the byte register choices.
   //
   if (hasByteDeps)
      {
      if (hasNoRegDeps)
         {
         for (i=firstNoRegAssoc; i<=lastNoRegAssoc; i++)
            dependencies[i]->getRegister()->unblock();
         }

      do
         {
         changed = false;
         for (i = firstByteRegAssoc; i <= lastByteRegAssoc; i++)
            {
            virtReg = dependencies[i]->getRegister();
            if (toRealRegister(virtReg->getAssignedRealRegister()) == NULL)
               {
               machine->coerceGPRegisterAssignment(currentInstruction, virtReg, TR_ByteReg);
               virtReg->block();
               changed = true;
               }
            }
         } while (changed);

      if (hasNoRegDeps)
         {
         for (i=firstNoRegAssoc; i<=lastNoRegAssoc; i++)
            dependencies[i]->getRegister()->block();
         }
      }

   // Assign the remaining NoReg dependencies.
   //
   if (hasNoRegDeps)
      {
      do
         {
         changed = false;
         for (i = firstNoRegAssoc; i<=lastNoRegAssoc; i++)
            {
            virtReg = dependencies[i]->getRegister();
            if (toRealRegister(virtReg->getAssignedRealRegister()) == NULL)
               {
               if (virtReg->getKind() == TR_FPR || virtReg->getKind() == TR_VRF)
                  machine->coerceGPRegisterAssignment(currentInstruction, virtReg, TR_QuadWordReg);
               else
                  machine->coerceGPRegisterAssignment(currentInstruction, virtReg);
               virtReg->block();
               changed = true;
               }
            }
         } while (changed);
      }

   // Recommend the best available register without actually assigning it.
   //
   if (hasBestFreeRegDeps)
      {
      virtReg = dependencies[bestFreeRegAssoc]->getRegister();
      bestFreeRealReg = machine->findBestFreeGPRegister(currentInstruction, virtReg);
      bestFreeRealRegIndex = bestFreeRealReg ? bestFreeRealReg->getRegisterNumber() : TR::RealRegister::NoReg;
      }

   unblockRegisters(numberOfRegisters);

   for (i = 0; i < numDependencyRegisters; i++)
      {
      TR::Register *virtRegister = dependencies[i]->getRegister();
      assignedReg = toRealRegister(virtRegister->getAssignedRealRegister());

      // Document the registers to which NoReg registers get assigned for use
      // by snippets that need to look back and figure out which virtual
      // registers ended up assigned to which real registers.
      //
      if (dependencies[i]->getRealRegister() == TR::RealRegister::NoReg)
         dependencies[i]->setRealRegister(assignedReg->getRegisterNumber());

      if (dependencies[i]->getRealRegister() == TR::RealRegister::BestFreeReg)
         {
         dependencies[i]->setRealRegister(bestFreeRealRegIndex);
         virtRegister->decFutureUseCount();
         continue;
         }

      if (virtRegister->decFutureUseCount() == 0 &&
          assignedReg->getState() != TR::RealRegister::Locked)
         {
         virtRegister->setAssignedRegister(NULL);
         assignedReg->setAssignedRegister(NULL);
         assignedReg->setState(TR::RealRegister::Free);
         }
      }

   if (cg->getUseNonLinearRegisterAssigner())
      {
      for (i=0; i<numberOfRegisters; i++)
         {
         virtReg = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();

         if (virtReg
             && (kindsToBeAssigned & virtReg->getKindAsMask())
             && dependentRegNum == TR::RealRegister::SpilledReg)
            {
            virtReg->decFutureUseCount();
            }
         }
      }

   }


void TR_X86RegisterDependencyGroup::setDependencyInfo(
   TR_X86RegisterDependencyIndex  index,
   TR::Register                  *vr,
   TR::RealRegister::RegNum       rr,
   TR::CodeGenerator             *cg,
   uint8_t                        flag,
   bool                           isAssocRegDependency)
   {
   _dependencies[index].setRegister(vr);
   _dependencies[index].assignFlags(flag);
   _dependencies[index].setRealRegister(rr);

   if (vr &&
      vr->isLive() &&
      rr != TR::RealRegister::NoReg &&
      rr != TR::RealRegister::ByteReg)
      {
      TR::RealRegister *realReg = cg->machine()->getX86RealRegister(rr);
      if ((vr->getKind() == TR_GPR) && !isAssocRegDependency)
         {
         // Remember this association so that we can build interference info for
         // other live registers.
         //
         cg->getLiveRegisters(TR_GPR)->setAssociation(vr, realReg);
         }
      }
   }


void OMR::X86::RegisterDependencyConditions::createRegisterAssociationDirective(TR::Instruction *instruction, TR::CodeGenerator *cg)
   {

   TR::Machine *machine = cg->machine();

   machine->createRegisterAssociationDirective(instruction->getPrev());

   // Now set up the new register associations as required by the current
   // dependent register instruction onto the machine.
   // Only the registers that this instruction interferes with are modified.
   //
   TR_X86RegisterDependencyGroup *depGroup = getPreConditions();
   for (int j = 0; j < getNumPreConditions(); j++)
      {
      TR::RegisterDependency  *dependency = depGroup->getRegisterDependency(j);
      if (dependency->getRegister())
         machine->setVirtualAssociatedWithReal(dependency->getRealRegister(), dependency->getRegister());
      }

   depGroup = getPostConditions();
   for (int k = 0; k < getNumPostConditions(); k++)
      {
      TR::RegisterDependency  *dependency = depGroup->getRegisterDependency(k);
      if (dependency->getRegister())
         machine->setVirtualAssociatedWithReal(dependency->getRealRegister(), dependency->getRegister());
      }
   }


TR::RealRegister *OMR::X86::RegisterDependencyConditions::getRealRegisterFromVirtual(TR::Register *virtReg, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();

   TR_X86RegisterDependencyGroup *depGroup = getPostConditions();
   for (int j = 0; j < getNumPostConditions(); j++)
      {
      TR::RegisterDependency  *dependency = depGroup->getRegisterDependency(j);

      if (dependency->getRegister() == virtReg)
         {
         return machine->getX86RealRegister(dependency->getRealRegister());
         }
      }

   depGroup = getPreConditions();
   for (int k = 0; k < getNumPreConditions(); k++)
      {
      TR::RegisterDependency  *dependency = depGroup->getRegisterDependency(k);

      if (dependency->getRegister() == virtReg)
         {
         return machine->getX86RealRegister(dependency->getRealRegister());
         }
      }

   TR_ASSERT(0, "getRealRegisterFromVirtual: shouldn't get here");
   return 0;
   }


void TR_X86RegisterDependencyGroup::assignFPRegisters(TR::Instruction   *prevInstruction,
                                                       TR_RegisterKinds  kindsToBeAssigned,
                                                       TR_X86RegisterDependencyIndex          numberOfRegisters,
                                                       TR::CodeGenerator *cg)
   {

   TR::Machine *machine = cg->machine();
   TR::Instruction *cursor  = prevInstruction;

   if (numberOfRegisters > 0)
      {
      TR::Register *reqdTopVirtReg = NULL;

      int32_t i;

#ifdef DEBUG
      int32_t numberOfFPRegisters = 0;
      for (i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *virtReg = _dependencies[i].getRegister();
         if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
            {
            if (_dependencies[i].getGlobalFPRegister())
               numberOfFPRegisters++;
            }
         }
#endif

      TR::X86LabelInstruction  *labelInstruction = NULL;

      if (prevInstruction->getNext())
         labelInstruction = prevInstruction->getNext()->getIA32LabelInstruction();

      if (labelInstruction &&
          labelInstruction->getNeedToClearFPStack())
         {
         // Push the correct number of FP values in global registers
         // onto the stack; this would enable the stack height to be
         // correct at this point (start of extended basic block).
         //
         for (i = 0; i < numberOfRegisters; i++)
            {
            TR::Register *virtReg = _dependencies[i].getRegister();
            if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
               {
               if (_dependencies[i].getGlobalFPRegister())
                  {
                  if ((virtReg->getFutureUseCount() == 0) ||
                      (virtReg->getTotalUseCount() == virtReg->getFutureUseCount()))
                     {
                     machine->fpStackPush(virtReg);
                     }
                  }
               }
            }
         }

      // Reverse all FP spills so that FP stack height
      // equals number of global registers as this point
      //
      for (i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *virtReg = _dependencies[i].getRegister();
         if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
            {
            if (((virtReg->getFutureUseCount() != 0) &&
                 (virtReg->getTotalUseCount() != virtReg->getFutureUseCount())) &&
                !virtReg->getAssignedRealRegister())
               {
               cursor = machine->reverseFPRSpillState(cursor, virtReg);
               }
            }
         }

      // Call the routine that places the global registers in correct order;
      // this routine tries to order them in as few exchanges as possible. This
      // routine could be improved slightly in the future.
      //
      List<TR::Register> popRegisters(cg->trMemory());
      orderGlobalRegsOnFPStack(cursor, kindsToBeAssigned, numberOfRegisters, &popRegisters, cg);

      for (i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *virtReg = _dependencies[i].getRegister();
         if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
            {
            if (virtReg->getTotalUseCount() != virtReg->getFutureUseCount())
               {
               // Original code that handles non global FP regs
               //
               if (!_dependencies[i].getGlobalFPRegister())
                  {
                  if (!machine->isFPRTopOfStack(virtReg))
                     {
                     cursor = machine->fpStackFXCH(cursor, virtReg);
                     }

                  if (virtReg->decFutureUseCount() == 0)
                     {
                     machine->fpStackPop();
                     }
                  }
               }
            else
               {
               // If this is the first reference of a register, then this must be the caller
               // side return value.  Assume it already exists on the FP stack if not a global
               // FP register; else coerce it to the right stack location. The required stack
               // must be available at this point.
               //
               if (_dependencies[i].getGlobalFPRegister())
                  {
                  int32_t reqdStackHeight = _dependencies[i].getRealRegister() - TR::RealRegister::FirstFPR;
                  machine->fpStackCoerce(virtReg, (machine->getFPTopOfStack() - reqdStackHeight));
                  virtReg->decFutureUseCount();
                  }
               else
                  {
                  if (virtReg->decFutureUseCount() != 0)
                     {
                     machine->fpStackPush(virtReg);
                     }
                  }
               }
            }
         else if (_dependencies[i].getRealRegister() == TR::RealRegister::AllFPRegisters)
            {
            // Spill the entire FP stack to memory.
            //
            cursor = machine->fpSpillStack(cursor);
            }
         }

      if (reqdTopVirtReg)
         {
         if (!machine->isFPRTopOfStack(reqdTopVirtReg))
            {
            cursor = machine->fpStackFXCH(cursor, reqdTopVirtReg);
            }
         }

#ifdef DEBUG
      bool onlyGlobalFPRA = true;
      bool onlySpillAllFP = true;
      bool globalFPRA = false;
      for (i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *virtReg = _dependencies[i].getRegister();
         if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
            {
            if (_dependencies[i].getGlobalFPRegister())
               {
               globalFPRA = true;
               onlySpillAllFP = false;
               TR_X86FPStackRegister *assignedRegister = toX86FPStackRegister(virtReg->getAssignedRealRegister());
               if (assignedRegister)
                  {
                  int32_t reqdStackHeight    = _dependencies[i].getRealRegister() - TR::RealRegister::FirstFPR;
                  int32_t currentStackHeight = machine->getFPTopOfStack() - assignedRegister->getFPStackRegisterNumber();

                  TR_ASSERT(reqdStackHeight == currentStackHeight,
                         "Stack height for global FP register is NOT correct\n");
                  }
               }
            }
         else
            {
            if (_dependencies[i].getRealRegister() == TR::RealRegister::AllFPRegisters)
               {
               // Spill the entire FP stack to memory.
               //
               cursor = machine->fpSpillStack(cursor);
               onlyGlobalFPRA = false;
               }
            }

         TR_ASSERT((!globalFPRA || ((machine->getFPTopOfStack()+1) == numberOfFPRegisters)),
                "Stack height is NOT correct\n");

         TR_ASSERT(onlyGlobalFPRA || onlySpillAllFP, "Illegal dependency\n");
         }
#endif

      // We may need to pop registers off the FP stack at this late stage in certain cases;
      // in particular if there are 2 global registers having the same value (same child C
      // in the GLRegDeps on a branch) and one of them is never used in the extended block after
      // the branch; then only one value will be popped off the stack when the last reference
      // to child C is seen in the extended basic block. This case cannot be detected during
      // instruction selection as the reference count on the child C is non-zero at the dependency
      // but in fact one of the registers must be popped off (late) in this case.
      //
      if (getMayNeedToPopFPRegisters() &&
          !popRegisters.isEmpty())
         {
         ListIterator<TR::Register> popRegsIt(&popRegisters);
         for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
            {
            if (!machine->isFPRTopOfStack(popRegister))
               {
               cursor = machine->fpStackFXCH(cursor, popRegister);
               }

            TR::RealRegister *popRealRegister = machine->fpMapToStackRelativeRegister(popRegister);
            cursor = new (cg->trHeapMemory()) TR::X86FPRegInstruction(cursor, FSTPReg, popRealRegister, cg);
            machine->fpStackPop();
            }
         }
      }
   }


// Tries to coerce global FP registers into their required stack positions
// in fewest exchanges possible. Can be still improved; current worst case
// is 3N/2 exchanges where N is number of incorrect stack locations (this could happen
// if there are N/2 pairwise incorrect global registers. This could possibly be done in
// N+1 approx maybe (?)
//
void TR_X86RegisterDependencyGroup::orderGlobalRegsOnFPStack(TR::Instruction    *cursor,
                                                              TR_RegisterKinds   kindsToBeAssigned,
                                                              TR_X86RegisterDependencyIndex          numberOfRegisters,
                                                              List<TR::Register> *poppedRegisters,
                                                              TR::CodeGenerator  *cg)
   {

   //
   TR::Machine *machine = cg->machine();
   int32_t *stackShape = machine->getFPStackShape();
   memset(stackShape, 0xff, TR_X86FPStackRegister::NumRegisters*sizeof(int32_t));

   // The data structure stackShape holds the required stack position j for each
   // value on the stack at a particular location i. This will be used in the
   // algorithm later.
   //
   int32_t topOfStack = machine->getFPTopOfStack();
   int32_t i;
   for (i = 0; i < numberOfRegisters; i++)
      {
      TR::Register *virtReg = _dependencies[i].getRegister();
      if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
         {
         if (virtReg->getTotalUseCount() != virtReg->getFutureUseCount())
            {
            TR_X86FPStackRegister *assignedRegister = toX86FPStackRegister(virtReg->getAssignedRealRegister());
            TR_ASSERT(assignedRegister != NULL, "All spilled registers should have been reloaded by now\n");

            if (_dependencies[i].getGlobalFPRegister())
               {
               int32_t reqdStackHeight    = _dependencies[i].getRealRegister() - TR::RealRegister::FirstFPR;
               int32_t currentStackHeight = topOfStack - assignedRegister->getFPStackRegisterNumber();
               stackShape[currentStackHeight] = reqdStackHeight;
               }
            }
         }
      }

   TR::Register *reqdTopVirtReg = NULL;
   for (i = 0; i < numberOfRegisters; i++)
      {
      TR::Register *virtReg = _dependencies[i].getRegister();
      if (virtReg && kindsToBeAssigned & virtReg->getKindAsMask())
         {
         if (virtReg->getTotalUseCount() != virtReg->getFutureUseCount())
            {
            // If this is not the first reference of a register, then coerce it to the
            // top of stack.
            //
            if (_dependencies[i].getGlobalFPRegister())
               {
               int32_t reqdStackHeight = _dependencies[i].getRealRegister() - TR::RealRegister::FirstFPR;
               TR_X86FPStackRegister *assignedRegister = toX86FPStackRegister(virtReg->getAssignedRealRegister());
               int32_t currentStackHeight =  topOfStack - assignedRegister->getFPStackRegisterNumber();

               if (reqdStackHeight == 0)
                  {
                  reqdTopVirtReg = virtReg;
                  }

               TR::Register *origRegister = virtReg;
               while ((reqdStackHeight != currentStackHeight) &&
                      (reqdStackHeight >= 0))
                  {
                  if (!machine->isFPRTopOfStack(virtReg))
                     {
                     cursor = machine->fpStackFXCH(cursor, virtReg);
                     }

                  assignedRegister = toX86FPStackRegister(virtReg->getAssignedRealRegister());
                  int32_t tempCurrentStackHeight =  topOfStack - assignedRegister->getFPStackRegisterNumber();
                  if (reqdStackHeight != tempCurrentStackHeight)
                     {
                     cursor = machine->fpStackFXCH(cursor, reqdStackHeight);
                     }

#if DEBUG
                  if ((currentStackHeight < 0) ||
                      (currentStackHeight >= TR_X86FPStackRegister::NumRegisters))
                     TR_ASSERT(0, "Array out of bounds exception in array stackShape\n");
                  if ((reqdStackHeight < 0) ||
                      (reqdStackHeight >= TR_X86FPStackRegister::NumRegisters))
                     TR_ASSERT(0, "Array out of bounds exception in array stackShape\n");
#endif

                  // This is the new stack shape now after the exchanges done above
                  //
                  stackShape[currentStackHeight] = stackShape[0];
                  stackShape[0] = stackShape[reqdStackHeight];
                  stackShape[reqdStackHeight] = reqdStackHeight;

                  // Drive this loop by the value on top of the stack
                  //
                  reqdStackHeight = stackShape[0];
                  currentStackHeight = 0;
                  virtReg = machine->getFPStackLocationPtr(topOfStack)->getAssignedRegister();
                  }

               if (origRegister->decFutureUseCount() == 0)
                  {
                  // It is possible for a global register to go dead at a dependency;
                  // add it to the list of registers to be popped in this case (discussed
                  // in more detail above)
                  //
                  poppedRegisters->add(origRegister);
                  }
               }
            }
         }
      }

   if (reqdTopVirtReg)
      {
      if (!machine->isFPRTopOfStack(reqdTopVirtReg))
         {
         cursor = machine->fpStackFXCH(cursor, reqdTopVirtReg);
         }
      }
   }


#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
uint32_t OMR::X86::RegisterDependencyConditions::numReferencedFPRegisters(TR::CodeGenerator * cg)
   {
   TR::Machine *machine = cg->machine();
   uint32_t i, total = 0;
   TR::Register *reg;

   for (i=0; i<_numPreConditions; i++)
      {
      reg = _preConditions->getRegisterDependency(i)->getRegister();
      if ((reg && reg->getKind() == TR_X87) ||
          (!reg && _preConditions->getRegisterDependency(i)->getRealRegister() == TR::RealRegister::AllFPRegisters))
         {
         total++;
         }
      }

   for (i=0; i<_numPostConditions; i++)
      {
      reg = _postConditions->getRegisterDependency(i)->getRegister();
      if ((reg && reg->getKind() == TR_X87) ||
          (!reg && _postConditions->getRegisterDependency(i)->getRealRegister() == TR::RealRegister::AllFPRegisters))
         {
         total++;
         }
      }

   return total;
   }

uint32_t OMR::X86::RegisterDependencyConditions::numReferencedGPRegisters(TR::CodeGenerator * cg)
   {
   uint32_t i, total = 0;
   TR::Register *reg;

   for (i=0; i<_numPreConditions; i++)
      {
      reg = _preConditions->getRegisterDependency(i)->getRegister();
      if (reg && (reg->getKind() == TR_GPR || reg->getKind() == TR_FPR || reg->getKind() == TR_VRF))
         {
         total++;
         }
      }

   for (i=0; i<_numPostConditions; i++)
      {
      reg = _postConditions->getRegisterDependency(i)->getRegister();
      if (reg && (reg->getKind() == TR_GPR || reg->getKind() == TR_FPR || reg->getKind() == TR_VRF))
         {
         total++;
         }
      }

   return total;
   }

#endif // defined(DEBUG) || defined(PROD_WITH_ASSUMES)


////////////////////////////////////////////////////////////////////////////////
// Generate methods
////////////////////////////////////////////////////////////////////////////////

TR::RegisterDependencyConditions  *
generateRegisterDependencyConditions(TR_X86RegisterDependencyIndex numPreConds,
                                     TR_X86RegisterDependencyIndex numPostConds,
                                     TR::CodeGenerator * cg)
   {
   return new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numPreConds, numPostConds, cg->trMemory());
   }

TR::RegisterDependencyConditions  *
generateRegisterDependencyConditions(TR::Node           *node,
                                     TR::CodeGenerator  *cg,
                                     TR_X86RegisterDependencyIndex           additionalRegDeps,
                                     List<TR::Register> *popRegisters)
   {
   return new (cg->trHeapMemory()) TR::RegisterDependencyConditions(node, cg, additionalRegDeps, popRegisters);
   }
