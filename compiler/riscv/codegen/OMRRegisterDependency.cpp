/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

#include <stddef.h>
#include "codegen/RVInstruction.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterDependency.hpp"
#include "il/Node.hpp"

OMR::RV::RegisterDependencyConditions::RegisterDependencyConditions(uint16_t numPreConds, uint16_t numPostConds, TR_Memory * m)
   :  _preConditions(new (numPreConds, m) TR::RegisterDependencyGroup),
      _postConditions(new (numPostConds, m) TR::RegisterDependencyGroup),
      _numPreConditions(numPreConds),
      _addCursorForPre(0),
      _numPostConditions(numPostConds),
      _addCursorForPost(0)
   {}

OMR::RV::RegisterDependencyConditions::RegisterDependencyConditions(
                                       TR::CodeGenerator *cg,
                                       TR::Node          *node,
                                       uint32_t          extranum,
                                       TR::Instruction  **cursorPtr)
   {
   TR_UNIMPLEMENTED();
   }

void OMR::RV::RegisterDependencyConditions::unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg)
   {
   addPostCondition(reg, TR::RealRegister::NoReg);
   }

void OMR::RV::RegisterDependencyConditions::addPreCondition(TR::Register *vr, TR::RealRegister::RegNum rr, uint8_t flag)
   {
   TR_ASSERT(_addCursorForPre < _numPreConditions, " Pre Condition array bounds overflow");
   _preConditions->setDependencyInfo(_addCursorForPre++, vr, rr, flag);
   }

void OMR::RV::RegisterDependencyConditions::addPostCondition(TR::Register *vr, TR::RealRegister::RegNum rr, uint8_t flag)
   {
   TR_ASSERT(_addCursorForPost < _numPostConditions, " Post Condition array bounds overflow");
   _postConditions->setDependencyInfo(_addCursorForPost++, vr, rr, flag);
   }

void OMR::RV::RegisterDependencyConditions::assignPreConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindToBeAssigned, TR::CodeGenerator *cg)
   {
   if (_preConditions != NULL)
      {
      cg->clearRegisterAssignmentFlags();
      cg->setRegisterAssignmentFlag(TR_PreDependencyCoercion);
      _preConditions->assignRegisters(currentInstruction, kindToBeAssigned, _addCursorForPre, cg);
      }
   }

void OMR::RV::RegisterDependencyConditions::assignPostConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindToBeAssigned, TR::CodeGenerator *cg)
   {
   if (_postConditions != NULL)
      {
      cg->clearRegisterAssignmentFlags();
      cg->setRegisterAssignmentFlag(TR_PostDependencyCoercion);
      _postConditions->assignRegisters(currentInstruction, kindToBeAssigned, _addCursorForPost, cg);
      }
   }

TR::Register *OMR::RV::RegisterDependencyConditions::searchPreConditionRegister(TR::RealRegister::RegNum rr)
   {
   return(_preConditions==NULL?NULL:_preConditions->searchForRegister(rr, _addCursorForPre));
   }

TR::Register *OMR::RV::RegisterDependencyConditions::searchPostConditionRegister(TR::RealRegister::RegNum rr)
   {
   return(_postConditions==NULL?NULL:_postConditions->searchForRegister(rr, _addCursorForPost));
   }

uint32_t OMR::RV::RegisterDependencyConditions::setNumPreConditions(uint16_t n, TR_Memory * m)
   {
   if (_preConditions == NULL)
      {
      _preConditions = new (n, m) TR::RegisterDependencyGroup;
      }
   if (_addCursorForPre > n)
      {
      _addCursorForPre = n;
      }
   return (_numPreConditions = n);
   }

uint32_t OMR::RV::RegisterDependencyConditions::setNumPostConditions(uint16_t n, TR_Memory * m)
   {
   if (_postConditions == NULL)
      {
      _postConditions = new (n, m) TR::RegisterDependencyGroup;
      }
   if (_addCursorForPost > n)
      {
      _addCursorForPost = n;
      }
   return (_numPostConditions = n);
   }

bool OMR::RV::RegisterDependencyConditions::refsRegister(TR::Register *r)
   {
   for (uint16_t i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getRefsRegister())
         {
         return true;
         }
      }
   for (uint16_t j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getRefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool OMR::RV::RegisterDependencyConditions::defsRegister(TR::Register *r)
   {
   for (uint16_t i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }
   for (uint16_t j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool OMR::RV::RegisterDependencyConditions::defsRealRegister(TR::Register *r)
   {
   for (uint16_t i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister()->getAssignedRegister() == r &&
          _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }
   for (uint16_t j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister()->getAssignedRegister() == r &&
          _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool OMR::RV::RegisterDependencyConditions::usesRegister(TR::Register *r)
   {
   for (uint16_t i = 0; i < _addCursorForPre; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          _preConditions->getRegisterDependency(i)->getUsesRegister())
         {
         return true;
         }
      }
   for (uint16_t j = 0; j < _addCursorForPost; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getUsesRegister())
         {
         return true;
         }
      }
   return false;
   }

void OMR::RV::RegisterDependencyConditions::incRegisterTotalUseCounts(TR::CodeGenerator *cg)
   {
   for (uint16_t i = 0; i < _addCursorForPre; i++)
      {
      _preConditions->getRegisterDependency(i)->getRegister()->incTotalUseCount();
      }
   for (uint16_t j = 0; j < _addCursorForPost; j++)
      {
      _postConditions->getRegisterDependency(j)->getRegister()->incTotalUseCount();
      }
   }

TR::RegisterDependencyConditions *
OMR::RV::RegisterDependencyConditions::clone(
   TR::CodeGenerator *cg,
   TR::RegisterDependencyConditions *added)
   {
   TR_UNIMPLEMENTED();

   return NULL;
   }

void OMR::RV::RegisterDependencyGroup::assignRegisters(
                        TR::Instruction *currentInstruction,
                        TR_RegisterKinds kindToBeAssigned,
                        uint32_t numberOfRegisters,
                        TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Machine *machine = cg->machine();
   TR::Register  *virtReg;
   TR::RealRegister::RegNum dependentRegNum;
   TR::RealRegister *dependentRealReg, *assignedRegister;
   uint32_t i, j;
   bool changed;

   for (i = 0; i< numberOfRegisters; i++)
      {
      virtReg = _dependencies[i].getRegister();
      if (_dependencies[i].isSpilledReg())
         {
         TR_ASSERT(virtReg->getBackingStorage(), "should have a backing store if SpilledReg");
         if (virtReg->getAssignedRealRegister())
            {
            // this happens when the register was first spilled in main line path then was reverse spilled
            // and assigned to a real register in OOL path. We protected the backing store when doing
            // the reverse spill so we could re-spill to the same slot now
            traceMsg (comp,"\nOOL: Found register spilled in main line and re-assigned inside OOL");
            TR::Node *currentNode = currentInstruction->getNode();
            TR::RealRegister *assignedReg = toRealRegister(virtReg->getAssignedRegister());
            TR::MemoryReference *tempMR = TR::MemoryReference::createWithSymRef(cg, currentNode, (TR::SymbolReference*)virtReg->getBackingStorage()->getSymbolReference());
            TR_RegisterKinds rk = virtReg->getKind();
            TR::InstOpCode::Mnemonic opCode;
            switch (rk)
               {
               case TR_GPR:
                  opCode = TR::InstOpCode::_ld;
                  break;
               case TR_FPR:
                  opCode = TR::InstOpCode::_fld;
                  break;
               default:
                  TR_ASSERT(0, "\nRegister kind not supported in OOL spill\n");
                  break;
               }

            TR::Instruction *inst = generateLOAD(opCode, currentNode, assignedReg, tempMR, cg, currentInstruction);

            assignedReg->setAssignedRegister(NULL);
            virtReg->setAssignedRegister(NULL);
            assignedReg->setState(TR::RealRegister::Free);

            if (comp->getDebug())
               cg->traceRegisterAssignment("Generate reload of virt %s due to spillRegIndex dep at inst %p\n", cg->comp()->getDebug()->getName(virtReg), currentInstruction);
            cg->traceRAInstruction(inst);
            }

         if (!(std::find(cg->getSpilledRegisterList()->begin(), cg->getSpilledRegisterList()->end(), virtReg) != cg->getSpilledRegisterList()->end()))
            cg->getSpilledRegisterList()->push_front(virtReg);
         }
      // we also need to free up all locked backing storage if we are exiting the OOL during backwards RA assignment
      else if (currentInstruction->isLabel() && virtReg->getAssignedRealRegister())
         {
         TR::LabelInstruction *labelInstr = (TR::LabelInstruction *)currentInstruction;
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
            cg->freeSpill(location, dataSize, 0);
            virtReg->setBackingStorage(NULL);
            }
         }
      }

   for (i = 0; i < numberOfRegisters; i++)
      {
      virtReg = _dependencies[i].getRegister();

      if (virtReg->getAssignedRealRegister() != NULL)
         {
         if (_dependencies[i].isNoReg())
            {
            virtReg->block();
            }
         else
            {
            dependentRegNum = toRealRegister(virtReg->getAssignedRealRegister())->getRegisterNumber();
            for (j = 0; j < numberOfRegisters; j++)
               {
               if (dependentRegNum == _dependencies[j].getRealRegister())
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
         TR::RegisterDependency &regDep = _dependencies[i];
         virtReg = regDep.getRegister();
         dependentRegNum = regDep.getRealRegister();
         dependentRealReg = machine->getRealRegister(dependentRegNum);

         if (!regDep.isNoReg() &&
             !regDep.isSpilledReg() &&
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
         TR::RegisterDependency &regDep = _dependencies[i];
         virtReg = regDep.getRegister();
         assignedRegister = NULL;
         if (virtReg->getAssignedRealRegister() != NULL)
            {
            assignedRegister = toRealRegister(virtReg->getAssignedRealRegister());
            }
         dependentRegNum = regDep.getRealRegister();
         dependentRealReg = machine->getRealRegister(dependentRegNum);
         if (!regDep.isNoReg() &&
             !regDep.isSpilledReg() &&
             dependentRealReg != assignedRegister)
            {
            machine->coerceRegisterAssignment(currentInstruction, virtReg, dependentRegNum);
            virtReg->block();
            changed = true;
            }
         }
      } while (changed == true);

   for (i = 0; i < numberOfRegisters; i++)
      {
      if (_dependencies[i].isNoReg())
         {
         TR::RealRegister *realOne;

         virtReg = _dependencies[i].getRegister();
         realOne = virtReg->getAssignedRealRegister();
         if (realOne == NULL)
            {
            if (virtReg->getTotalUseCount() == virtReg->getFutureUseCount())
               {
               if ((assignedRegister = machine->findBestFreeRegister(virtReg->getKind(), true)) == NULL)
                  {
                  assignedRegister = machine->freeBestRegister(currentInstruction, virtReg, NULL);
                  }
               }
            else
               {
               assignedRegister = machine->reverseSpillState(currentInstruction, virtReg, NULL);
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

         if (getRegisterDependency(i)->isNoReg())
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
void OMR::RV::RegisterDependencyConditions::stopUsingDepRegs(TR::CodeGenerator *cg, TR::Register *notKilledReg)
   {
   if (_preConditions != NULL)
      _preConditions->stopUsingDepRegs(getAddCursorForPre(), NULL, notKilledReg, cg);
   if (_postConditions != NULL)
      _postConditions->stopUsingDepRegs(getAddCursorForPost(), NULL, notKilledReg, cg);
   }
