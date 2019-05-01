/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

OMR::ARM::RegisterDependencyConditions::RegisterDependencyConditions( TR::Node          *node,
                                                                       uint32_t           extranum,
                                                                       TR::Instruction   **cursorPtr,
                                                                       TR::CodeGenerator  *cg)
   {

   List<TR::Register>  regList(cg->trMemory());
   TR::Instruction    *cursor = (cursorPtr == NULL ? NULL : *cursorPtr);
   int32_t totalNum = node->getNumChildren() + extranum;
   int32_t i;


   cg->comp()->incVisitCount();

   int32_t numLongs = 0;
   //
   // Pre-compute how many longs are global register candidates
   //
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node     *child = node->getChild(i);
      TR::Register *reg   = child->getRegister();

      if (reg!=NULL /* && reg->getKind()==TR_GPR */)
         {
         if (child->getHighGlobalRegisterNumber() > -1)
            numLongs++;
         }
      }

   totalNum = totalNum + numLongs;

   _preConditions = TR_ARMRegisterDependencyGroup::create(totalNum, cg->trMemory());
   _postConditions = TR_ARMRegisterDependencyGroup::create(totalNum, cg->trMemory());
   _numPreConditions = totalNum;
   _addCursorForPre = 0;
   _numPostConditions = totalNum;
   _addCursorForPost = 0;

   // First, handle dependencies that match current association
   for (i=0; i<node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      TR::Register *reg = child->getRegister();
      TR::Register *highReg = NULL;
      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(child->getGlobalRegisterNumber());

      TR::RealRegister::RegNum highRegNum;

      if (child->getHighGlobalRegisterNumber() > -1)
         {
         highRegNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(child->getHighGlobalRegisterNumber());

         TR::RegisterPair *regPair = reg->getRegisterPair();
         TR_ASSERT(regPair, "assertion failure");
         highReg = regPair->getHighOrder();
         reg = regPair->getLowOrder();

         if (highReg->getAssociation() != highRegNum ||
            reg->getAssociation() != regNum)
            continue;
         }
      else if (reg->getAssociation() != regNum)
         continue;

      TR_ASSERT(!regList.find(reg) && (!highReg || !regList.find(highReg)), "Should not happen\n");

      addPreCondition(reg, regNum);
      addPostCondition(reg, regNum);
      regList.add(reg);

      if (highReg)
         {
         addPreCondition(highReg, highRegNum);
         addPostCondition(highReg, highRegNum);
         regList.add(highReg);
         }
      }


   // Second pass to handle dependencies for which association does not exist
   // or does not match
   for (i=0; i<node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      TR::Register *reg = child->getRegister();
      TR::Register *highReg = NULL;
      TR::Register *copyReg = NULL;
      TR::Register *highCopyReg = NULL;
      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(child->getGlobalRegisterNumber());

      TR::RealRegister::RegNum highRegNum;

      if (child->getHighGlobalRegisterNumber() > -1)
         {
         highRegNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(child->getHighGlobalRegisterNumber());
         TR::RegisterPair *regPair = reg->getRegisterPair();
         TR_ASSERT(regPair, "assertion failure");
         highReg = regPair->getHighOrder();
         reg = regPair->getLowOrder();

         if (highReg->getAssociation() == highRegNum &&
            reg->getAssociation() == regNum)
            continue;
         }
      else if (reg->getAssociation() == regNum)
         continue;

      if (regList.find(reg) || (highReg && regList.find(highReg)))
         {
         TR_ARMOpCodes    opCode;
         TR_RegisterKinds kind = reg->getKind();

         switch (kind)
            {
            case TR_GPR:
               opCode = ARMOp_mov;
               break;
            case TR_FPR:
               opCode = ARMOp_fmrs;
               break;
            default:
               TR_ASSERT(0, "Invalid register kind.");
            }

         if (regList.find(reg))
            {
            bool containsInternalPointer = false;
            if (reg->getPinningArrayPointer())
               containsInternalPointer = true;

            copyReg = (reg->containsCollectedReference() && !containsInternalPointer) ?
               cg->allocateCollectedReferenceRegister() : cg->allocateRegister(kind);

            if (containsInternalPointer)
               {
               copyReg->setContainsInternalPointer();
               copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
               }

            cursor = generateTrg1Src1Instruction(cg, opCode, node, copyReg, reg, cursor);
            reg = copyReg;
            }

         if (highReg && regList.find(highReg))
            {
            bool containsInternalPointer = false;
            if (highReg->getPinningArrayPointer())
               containsInternalPointer = true;

            highCopyReg = (highReg->containsCollectedReference() && !containsInternalPointer) ?
               cg->allocateCollectedReferenceRegister() : cg->allocateRegister(kind);

            if (containsInternalPointer)
               {
               highCopyReg->setContainsInternalPointer();
               highCopyReg->setPinningArrayPointer(highReg->getPinningArrayPointer());
               }

            cursor = generateTrg1Src1Instruction(cg, opCode, node, highCopyReg, highReg, cursor);
            highReg = highCopyReg;
            }
         }

      reg->setAssociation(regNum);
      addPreCondition(reg, regNum);
      addPostCondition(reg, regNum);
      if (copyReg != NULL)
         cg->stopUsingRegister(copyReg);
      else
         regList.add(reg);

      if (highReg)
         {
         highReg->setAssociation(highRegNum);
         addPreCondition(highReg, highRegNum);
         addPostCondition(highReg, highRegNum);
         if (highCopyReg != NULL)
            cg->stopUsingRegister(highCopyReg);
         else
            regList.add(highReg);
         }
      }

   if (cursor != NULL && cursorPtr != NULL)
      *cursorPtr = cursor;
   }

bool OMR::ARM::RegisterDependencyConditions::refsRegister(TR::Register *r)
   {
   for (int i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getRefsRegister())
         {
         return true;
         }
      }
   for (int j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getRefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool OMR::ARM::RegisterDependencyConditions::defsRegister(TR::Register *r)
   {
   for (int i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }
   for (int j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool OMR::ARM::RegisterDependencyConditions::defsRealRegister(TR::Register *r)
   {
   for (int i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister()->getAssignedRegister() == r &&
          _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }
   for (int j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister()->getAssignedRegister() == r &&
          _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }
   return false;
   }


bool OMR::ARM::RegisterDependencyConditions::usesRegister(TR::Register *r)
   {
   for (int i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getUsesRegister())
         {
         return true;
         }
      }
   for (int j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getUsesRegister())
         {
         return true;
         }
      }
   return false;
   }

void OMR::ARM::RegisterDependencyConditions::incRegisterTotalUseCounts(TR::CodeGenerator * cg)
   {
   for (int i = 0; i < _addCursorForPre; i++)
      {
      _preConditions->getRegisterDependency(i)->getRegister()->incTotalUseCount();
      }
   for (int j = 0; j < _addCursorForPost; j++)
      {
      _postConditions->getRegisterDependency(j)->getRegister()->incTotalUseCount();
      }
   }

void TR_ARMRegisterDependencyGroup::assignRegisters(TR::Instruction  *currentInstruction,
                                                    TR_RegisterKinds kindToBeAssigned,
                                                    uint32_t         numberOfRegisters,
                                                    TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Machine *machine = cg->machine();
   TR::Register  *virtReg;
   TR::RealRegister::RegNum dependentRegNum;
   TR::RealRegister *dependentRealReg, *assignedRegister;
   uint32_t i, j;
   bool changed;

   if (!comp->getOption(TR_DisableOOL))
      {
      for (i = 0; i< numberOfRegisters; i++)
         {
         virtReg = dependencies[i].getRegister();
         dependentRegNum = dependencies[i].getRealRegister();
         if (dependentRegNum == TR::RealRegister::SpilledReg)
            {
            TR_ASSERT(virtReg->getBackingStorage(),"should have a backing store if dependentRegNum == spillRegIndex()\n");
            if (virtReg->getAssignedRealRegister())
               {
               // this happens when the register was first spilled in main line path then was reverse spilled
               // and assigned to a real register in OOL path. We protected the backing store when doing
               // the reverse spill so we could re-spill to the same slot now
               traceMsg (comp,"\nOOL: Found register spilled in main line and re-assigned inside OOL");
               TR::Node *currentNode = currentInstruction->getNode();
               TR::RealRegister *assignedReg    = toRealRegister(virtReg->getAssignedRegister());
               TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(currentNode, (TR::SymbolReference*)virtReg->getBackingStorage()->getSymbolReference(), sizeof(uintptr_t), cg);
               TR_ARMOpCodes opCode;
               TR_RegisterKinds rk = virtReg->getKind();
               switch (rk)
                  {
                  case TR_GPR:
                     opCode = ARMOp_ldr;
                     break;
                  case TR_FPR:
                     opCode = virtReg->isSinglePrecision() ? ARMOp_ldfs : ARMOp_ldfd;
                     break;
                  default:
                     TR_ASSERT(0, "\nRegister kind not supported in OOL spill\n");
                     break;
                  }

               TR::Instruction *inst = generateTrg1MemInstruction(cg, opCode, currentNode, assignedReg, tempMR, currentInstruction);

               assignedReg->setAssignedRegister(NULL);
               virtReg->setAssignedRegister(NULL);
               assignedReg->setState(TR::RealRegister::Free);

               if (comp->getDebug())
                  cg->traceRegisterAssignment("Generate reload of virt %s due to spillRegIndex dep at inst %p\n", cg->comp()->getDebug()->getName(virtReg),currentInstruction);
               cg->traceRAInstruction(inst);
               }
            if (!(std::find(cg->getSpilledRegisterList()->begin(), cg->getSpilledRegisterList()->end(), virtReg) != cg->getSpilledRegisterList()->end()))
               cg->getSpilledRegisterList()->push_front(virtReg);
            }
         // we also need to free up all locked backing storage if we are exiting the OOL during backwards RA assignment
         else if (currentInstruction->isLabel() && virtReg->getAssignedRealRegister())
            {
            TR::ARMLabelInstruction *labelInstr = (TR::ARMLabelInstruction *)currentInstruction;
            TR_BackingStore *location = virtReg->getBackingStorage();
            TR_RegisterKinds rk = virtReg->getKind();
            int32_t dataSize;
            if (labelInstr->getLabelSymbol()->isStartOfColdInstructionStream() && location)
               {
               traceMsg (comp,"\nOOL: Releasing backing storage (%p)\n", location);
               if (rk == TR_GPR)
                  dataSize = TR::Compiler->om.sizeofReferenceAddress();
               else
                  dataSize = 8;
               location->setMaxSpillDepth(0);
               cg->freeSpill(location,dataSize,0);
               virtReg->setBackingStorage(NULL);
               }
            }
         }
      }
   for (i = 0; i < numberOfRegisters; i++)
      {
      virtReg = dependencies[i].getRegister();

      if (virtReg->getAssignedRealRegister()!=NULL)
         {
         if (dependencies[i].getRealRegister() == TR::RealRegister::NoReg)
            {
            virtReg->block();
            }
         else
            {
            dependentRegNum = toRealRegister(virtReg->getAssignedRealRegister())->getRegisterNumber();
            for (j=0; j<numberOfRegisters; j++)
               {
               if (dependentRegNum == dependencies[j].getRealRegister())
                  {
                  virtReg->block();
                  break;
                  }
               }
            }
         }
      }

   do
      {
      changed = false;
      for (i = 0; i < numberOfRegisters; i++)
         {
         virtReg = dependencies[i].getRegister();
         dependentRegNum = dependencies[i].getRealRegister();
         dependentRealReg = machine->getRealRegister(dependentRegNum);

         if (dependentRegNum != TR::RealRegister::NoReg &&
             dependentRegNum != TR::RealRegister::SpilledReg &&
             dependentRealReg->getState() == TR::RealRegister::Free)
            {
            machine->coerceRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            virtReg->block();
            changed = true;
            }
         }
      } while (changed == true);


   do
      {
      changed = false;
      for (i = 0; i < numberOfRegisters; i++)
         {
         virtReg = dependencies[i].getRegister();
         assignedRegister = NULL;
         if (virtReg->getAssignedRealRegister() != NULL)
            {
            assignedRegister = toRealRegister(virtReg->getAssignedRealRegister());
            }
         dependentRegNum = dependencies[i].getRealRegister();
         dependentRealReg = machine->getRealRegister(dependentRegNum);
         if (dependentRegNum != TR::RealRegister::NoReg &&
             dependentRegNum != TR::RealRegister::SpilledReg &&
             dependentRealReg != assignedRegister)
            {
            machine->coerceRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            virtReg->block();
            changed = true;
            }
         }
      } while (changed == true);

   for (i=0; i<numberOfRegisters; i++)
      {
      if (dependencies[i].getRealRegister() == TR::RealRegister::NoReg)
         {
         bool excludeGPR0 = dependencies[i].getExcludeGPR0()?true:false;
         TR::RealRegister *realOne;

         virtReg = dependencies[i].getRegister();
         realOne = virtReg->getAssignedRealRegister();
         if (realOne!=NULL && excludeGPR0 && toRealRegister(realOne)->getRegisterNumber()==TR::RealRegister::gr0)
            {
            if ((assignedRegister = machine->findBestFreeRegister(virtReg->getKind(), true)) == NULL)
               {
               assignedRegister = machine->freeBestRegister(currentInstruction, virtReg->getKind(), NULL, true);
               }
            machine->coerceRegisterAssignment(currentInstruction, virtReg, assignedRegister->getRegisterNumber());
            }
         else if (realOne == NULL)
            {
            if (virtReg->getTotalUseCount() == virtReg->getFutureUseCount())
               {
               if ((assignedRegister = machine->findBestFreeRegister(virtReg->getKind(), excludeGPR0, true)) == NULL)
                  {
                  assignedRegister = machine->freeBestRegister(currentInstruction, virtReg->getKind(), NULL, excludeGPR0);
                  }
               }
            else
               {
               assignedRegister = machine->reverseSpillState(currentInstruction, virtReg, NULL, excludeGPR0);
               }
            virtReg->setAssignedRegister(assignedRegister);
            assignedRegister->setAssignedRegister(virtReg);
            assignedRegister->setState(TR::RealRegister::Assigned);
            virtReg->block();
            }
         }
      }

   unblockRegisters(numberOfRegisters);
   for (i = 0; i < numberOfRegisters; i++)
      {
      TR::Register *dependentRegister = getRegisterDependency(i)->getRegister();
      if (dependentRegister->getAssignedRegister())
      	 {
         TR::RealRegister *assignedRegister = dependentRegister->getAssignedRegister()->getRealRegister();

         if (getRegisterDependency(i)->getRealRegister() == TR::RealRegister::NoReg)
            getRegisterDependency(i)->setRealRegister(toRealRegister(assignedRegister)->getRegisterNumber());

         if (dependentRegister->decFutureUseCount() == 0)
            {
            dependentRegister->setAssignedRegister(NULL);
            assignedRegister->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched); // Was setting to Free
            }
         }
      }
   }


TR::RegisterDependencyConditions *OMR::ARM::RegisterDependencyConditions::clone(
   TR::CodeGenerator * cg, TR::RegisterDependencyConditions *added)
   {
   TR::RegisterDependencyConditions *result;
   TR_ARMRegisterDependency           *singlePair;
   int32_t      idx, preNum, postNum, addPre=0, addPost=0;
#if defined(DEBUG)
   int32_t      preGPR=0, postGPR=0;
#endif

   if (added != NULL)
      {
      addPre = added->getAddCursorForPre();
      addPost = added->getAddCursorForPost();
      }
   preNum = this->getAddCursorForPre();
   postNum = this->getAddCursorForPost();
   result = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(preNum+addPre,
      postNum+addPost, cg->trMemory());

   for (idx=0; idx<postNum; idx++)
      {
      singlePair = this->getPostConditions()->getRegisterDependency(idx);
      result->addPostCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                               singlePair->getFlags());
#if defined(DEBUG)
      if (singlePair->getRegister()->getKind() == TR_GPR)
         postGPR++;
#endif
      }

   for (idx=0; idx<preNum; idx++)
      {
      singlePair = this->getPreConditions()->getRegisterDependency(idx);
      result->addPreCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                              singlePair->getFlags());
#if defined(DEBUG)
      if (singlePair->getRegister()->getKind() == TR_GPR)
         preGPR++;
#endif
      }

   for (idx=0; idx<addPost; idx++)
      {
      singlePair = added->getPostConditions()->getRegisterDependency(idx);
      result->addPostCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                               singlePair->getFlags());
#if defined(DEBUG)
      if (singlePair->getRegister()->getKind() == TR_GPR)
         postGPR++;
#endif
      }

   for (idx=0; idx<addPre; idx++)
      {
      singlePair = added->getPreConditions()->getRegisterDependency(idx);
      result->addPreCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                              singlePair->getFlags());
#if defined(DEBUG)
      if (singlePair->getRegister()->getKind() == TR_GPR)
         preGPR++;
#endif
      }

#if defined(DEBUG)
   int32_t max_gpr = cg->getProperties().getNumAllocatableIntegerRegisters();
   TR_ASSERT(preGPR<=max_gpr && postGPR<=max_gpr, "Over the limit of available global registers.");
#endif
   return result;
   }

TR::RegisterDependencyConditions *OMR::ARM::RegisterDependencyConditions::cloneAndFix(
   TR::CodeGenerator * cg, TR::RegisterDependencyConditions *added)
   {
   TR::RegisterDependencyConditions *result;
   TR_ARMRegisterDependency           *singlePair;
   int32_t      idx, preNum, postNum, addPre=0, addPost=0;
   TR::Register *postReg, *tempReg;
   TR::RealRegister::RegNum rnum;

   if (added != NULL)
      {
      addPre = added->getAddCursorForPre();
      addPost = added->getAddCursorForPost();
      }
   preNum = this->getAddCursorForPre();
   postNum = this->getAddCursorForPost();
   result = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(preNum + addPre, postNum + addPost, cg->trMemory());

   for (idx=0; idx<postNum; idx++)
      {
      singlePair = this->getPostConditions()->getRegisterDependency(idx);
      rnum       = singlePair->getRealRegister();
      tempReg    = singlePair->getRegister();
      result->addPostCondition(tempReg, rnum, singlePair->getFlags());
      if (rnum == TR::RealRegister::gr0)
         {
         postReg = tempReg;
         }
      }

   for (idx=0; idx<preNum; idx++)
      {
      singlePair = this->getPreConditions()->getRegisterDependency(idx);
      rnum       = singlePair->getRealRegister();
      tempReg    = singlePair->getRegister();
      if (rnum == TR::RealRegister::gr0)
         tempReg = postReg;
      result->addPreCondition(tempReg, rnum, singlePair->getFlags());
      }

   for (idx=0; idx<addPost; idx++)
      {
      singlePair = added->getPostConditions()->getRegisterDependency(idx);
      result->addPostCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                               singlePair->getFlags());
      }

   for (idx=0; idx<addPre; idx++)
      {
      singlePair = added->getPreConditions()->getRegisterDependency(idx);
      result->addPreCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                              singlePair->getFlags());
      }
   return result;
   }

void OMR::ARM::RegisterDependencyConditions::unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg)
   {
   addPostCondition(reg, TR::RealRegister::NoReg);
   }
