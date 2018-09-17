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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.

#pragma csect(CODE,"OMRZRegisterDependency#C")
#pragma csect(STATIC,"OMRZRegisterDependency#S")
#pragma csect(TEST,"OMRZRegisterDependency#T")

#include <stddef.h>                                // for NULL
#include <stdint.h>                                // for int32_t, uint32_t, etc
#include "codegen/BackingStore.hpp"                // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"               // for CodeGenerator
#include "codegen/FrontEnd.hpp"                    // for TR::IO::fprintf, etc
#include "codegen/InstOpCode.hpp"                  // for InstOpCode, etc
#include "codegen/Instruction.hpp"                 // for Instruction
#include "codegen/Machine.hpp"                     // for Machine, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                // for RealRegister, etc
#include "codegen/Register.hpp"                    // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"                // for RegisterPair
#include "compile/Compilation.hpp"                 // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/hashtab.h"                           // for HashTable
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                     // for ObjectModel
#include "env/TRMemory.hpp"                        // for TR_Memory, etc
#include "il/DataTypes.hpp"                        // for DataTypes::Int32
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::icall, etc
#include "il/ILOps.hpp"                            // for ILOpCode
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"                     // for Node::getChild, etc
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/symbol/AutomaticSymbol.hpp"           // for AutomaticSymbol
#include "il/symbol/LabelSymbol.hpp"               // for LabelSymbol
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/List.hpp"                          // for List, TR_Queue
#include "ras/Debug.hpp"                           // for TR_DebugBase
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "env/IO.hpp"

OMR::Z::RegisterDependencyConditions::RegisterDependencyConditions(TR::CodeGenerator * cg, TR::Node * node,
   uint32_t          extranum, TR::Instruction ** cursorPtr)
   {
   TR::Compilation *comp = cg->comp();
   _cg = cg;
   List<TR::Register> regList(cg->trMemory());
   TR::Instruction * iCursor = (cursorPtr == NULL) ? NULL : *cursorPtr;
   int32_t totalNum = node->getNumChildren() + extranum;
   int32_t i;

   comp->incVisitCount();

   int32_t numLongs = 0;
   int32_t numLongDoubles = 0;
   //
   // Pre-compute how many longs are global register candidates
   //
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node * child = node->getChild(i);
      TR::Register * reg = child->getRegister();
      if (reg->getKind() == TR_GPR)
         {
         if (child->getHighGlobalRegisterNumber() > -1)
            {
            numLongs++;
            }
         }
      }

   totalNum = totalNum + numLongs + numLongDoubles;

   _preConditions = new (totalNum, cg->trMemory()) TR_S390RegisterDependencyGroup;
   _postConditions = new (totalNum, cg->trMemory()) TR_S390RegisterDependencyGroup;
   _numPreConditions = totalNum;
   _addCursorForPre = 0;
   _numPostConditions = totalNum;
   _addCursorForPost = 0;
   _isUsed = false;
   _isHint = false;
   _conflictsResolved = false;

   for (i = 0; i < totalNum; i++)
      {
      _preConditions->clearDependencyInfo(i);
      _postConditions->clearDependencyInfo(i);
      }

   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node * child = node->getChild(i);
      TR::Register * reg = child->getRegister();
      TR::Register * highReg = NULL;
      TR::Register * copyReg = NULL;
      TR::Register * highCopyReg = NULL;

      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum)
         cg->getGlobalRegister(child->getGlobalRegisterNumber());


      TR::RealRegister::RegNum highRegNum;
      if (child->getHighGlobalRegisterNumber() > -1)
         {
         highRegNum = (TR::RealRegister::RegNum) cg->getGlobalRegister(child->getHighGlobalRegisterNumber());
         }

      TR::RegisterPair * regPair = reg->getRegisterPair();
      resolveSplitDependencies(cg, node, child, regPair, regList,
         reg, highReg, copyReg, highCopyReg, iCursor, regNum);

      TR::Register * copyHPR = NULL;
      if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
          cg->machine()->getHPRFromGlobalRegisterNumber(child->getGlobalRegisterNumber()))
         {
         if (reg->is64BitReg() && !reg->containsCollectedReference())
            {
            //if GRA wants a 64-bit scalar into an HPR we must split this register
            copyHPR = cg->allocateRegister(TR_GPR);
            iCursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, copyHPR, reg, iCursor);
            reg = copyHPR;
            }
         reg->setAssignToHPR(true);
         //traceMsg(comp, "\nregDeps: setAssignToHPR %s: %d\n", cg->getDebug()->getName(reg), child->getGlobalRegisterNumber());
         }
      reg->setAssociation(regNum);
      cg->setRealRegisterAssociation(reg, regNum);

      addPreCondition(reg, regNum);
      addPostCondition(reg, regNum);

      if (copyHPR != NULL)
         {
         cg->stopUsingRegister(copyHPR);
         }

      if (copyReg != NULL)
         {
         cg->stopUsingRegister(copyReg);
         }

      if (highReg)
         {
         highReg->setAssociation(highRegNum);
         cg->setRealRegisterAssociation(highReg, highRegNum);
         addPreCondition(highReg, highRegNum);
         addPostCondition(highReg, highRegNum);
         if (highCopyReg != NULL)
            {
            cg->stopUsingRegister(highCopyReg);
            }
         }
      }

   if (iCursor != NULL && cursorPtr != NULL)
      {
      *cursorPtr = iCursor;
      }
   }

OMR::Z::RegisterDependencyConditions::RegisterDependencyConditions(TR::RegisterDependencyConditions* iConds, uint16_t numNewPreConds, uint16_t numNewPostConds, TR::CodeGenerator * cg)
   : _preConditions(TR_S390RegisterDependencyGroup::create((iConds?iConds->getNumPreConditions():0)+numNewPreConds, cg->trMemory())),
     _postConditions(TR_S390RegisterDependencyGroup::create((iConds?iConds->getNumPostConditions():0)+numNewPostConds, cg->trMemory())),
     _numPreConditions((iConds?iConds->getNumPreConditions():0)+numNewPreConds),
     _addCursorForPre(0),
     _numPostConditions((iConds?iConds->getNumPostConditions():0)+numNewPostConds),
     _addCursorForPost(0),
     _isHint(false),
     _isUsed(false),
     _conflictsResolved(false),
     _cg(cg)
   {
     int32_t i;

     TR_S390RegisterDependencyGroup* depGroup;
     uint16_t numPreConds = (iConds?iConds->getNumPreConditions():0)+numNewPreConds;
     uint16_t numPostConds = (iConds?iConds->getNumPostConditions():0)+numNewPostConds;
     uint32_t flag;
     TR::RealRegister::RegNum rr;
     TR::Register* vr;
     for(i=0;i<numPreConds;i++)
        {
        _preConditions->clearDependencyInfo(i);
        }
     for(int32_t j=0;j<numPostConds;j++)
        {
        _postConditions->clearDependencyInfo(j);
        }

     if (iConds != NULL)
        {
        depGroup = iConds->getPreConditions();
        //      depGroup->printDeps(stdout, iConds->getNumPreConditions());
        for(i=0;i<iConds->getAddCursorForPre();i++)
           {
          TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

           flag = dep->getFlags();
           vr   = dep->getRegister();
           rr   = dep->getRealRegister();

           _preConditions->setDependencyInfo(_addCursorForPre++, vr, rr, flag);
           }

        depGroup = iConds->getPostConditions();
        //        depGroup->printDeps(stdout, iConds->getNumPostConditions());
        for(i=0;i<iConds->getAddCursorForPost();i++)
           {
           TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

           flag = dep->getFlags();
           vr   = dep->getRegister();
           rr   = dep->getRealRegister();

           _postConditions->setDependencyInfo(_addCursorForPost++, vr, rr, flag);
           }
        }
   }

OMR::Z::RegisterDependencyConditions::RegisterDependencyConditions(TR::RegisterDependencyConditions * conds_1,
                                     TR::RegisterDependencyConditions * conds_2,
                                     TR::CodeGenerator * cg)
   : _preConditions(TR_S390RegisterDependencyGroup::create(conds_1->getNumPreConditions()+conds_2->getNumPreConditions(), cg->trMemory())),
     _postConditions(TR_S390RegisterDependencyGroup::create(conds_1->getNumPostConditions()+conds_2->getNumPostConditions(), cg->trMemory())),
     _numPreConditions(conds_1->getNumPreConditions()+conds_2->getNumPreConditions()),
     _addCursorForPre(0),
     _numPostConditions(conds_1->getNumPostConditions()+conds_2->getNumPostConditions()),
     _addCursorForPost(0),
     _isUsed(false),
     _isHint(false),
     _conflictsResolved(false),
     _cg(cg)
   {
   int32_t i;
   TR_S390RegisterDependencyGroup* depGroup;
   uint32_t flag;
   TR::RealRegister::RegNum rr;
   TR::Register* vr;

   for( i = 0; i < _numPreConditions; i++)
      {
      _preConditions->clearDependencyInfo(i);
      }

   for( i = 0; i < _numPostConditions; i++)
      {
      _postConditions->clearDependencyInfo(i);
      }

   // Merge pre conditions
   depGroup = conds_1->getPreConditions();
   for( i = 0; i < conds_1->getAddCursorForPre(); i++)
      {
      TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

      flag = dep->getFlags();
      vr   = dep->getRegister();
      rr   = dep->getRealRegister();

      if( doesPreConditionExist( vr, rr, flag, true ) )
         {
         _numPreConditions--;
         }
      else if( !addPreConditionIfNotAlreadyInserted( vr, rr, flag ) )
         {
         _numPreConditions--;
         }
      }

   depGroup = conds_2->getPreConditions();
   for( i = 0; i < conds_2->getAddCursorForPre(); i++)
      {
      TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

      flag = dep->getFlags();
      vr   = dep->getRegister();
      rr   = dep->getRealRegister();

      if( doesPreConditionExist( vr, rr, flag, true ) )
         {
         _numPreConditions--;
         }
      else if( !addPreConditionIfNotAlreadyInserted( vr, rr, flag ) )
         {
         _numPreConditions--;
         }
      }

   // Merge post conditions
   depGroup = conds_1->getPostConditions();
   for( i = 0; i < conds_1->getAddCursorForPost(); i++)
      {
      TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

      flag = dep->getFlags();
      vr   = dep->getRegister();
      rr   = dep->getRealRegister();

      if( doesPostConditionExist( vr, rr, flag, true ) )
         {
         _numPostConditions--;
         }
      else if( !addPostConditionIfNotAlreadyInserted( vr, rr, flag ) )
         {
         _numPostConditions--;
         }
      }

   depGroup = conds_2->getPostConditions();
   for( i = 0; i < conds_2->getAddCursorForPost(); i++)
      {
      TR::RegisterDependency* dep = depGroup->getRegisterDependency(i);

      flag = dep->getFlags();
      vr   = dep->getRegister();
      rr   = dep->getRealRegister();

      if( doesPostConditionExist( vr, rr, flag, true ) )
         {
         _numPostConditions--;
         }
      else if( !addPostConditionIfNotAlreadyInserted( vr, rr, flag ) )
         {
         _numPostConditions--;
         }
      }
   }

void OMR::Z::RegisterDependencyConditions::resolveSplitDependencies(
   TR::CodeGenerator* cg,
   TR::Node* node, TR::Node* child,
   TR::RegisterPair* regPair, List<TR::Register>& regList,
   TR::Register*& reg, TR::Register*& highReg,
   TR::Register*& copyReg, TR::Register*& highCopyReg,
   TR::Instruction*& iCursor,
   TR::RealRegister::RegNum& regNum)
   {
   TR::Compilation *comp = cg->comp();
   TR_Debug * debugObj = cg->getDebug();
   bool foundLow = false;
   bool foundHigh = false;
   if (regPair)
      {
      foundLow = regList.find(regPair->getLowOrder());
      foundHigh = regList.find(regPair->getHighOrder());
      }
   else
      {
      foundLow = regList.find(reg);
      }

   if (foundLow || foundHigh)
      {
      TR::InstOpCode::Mnemonic opCode;
      TR_RegisterKinds kind = reg->getKind();
      bool isVector = kind == TR_VRF ? true : false;
      if (kind == TR_GPR || kind == TR_FPR)
         {
         if (regPair)
            {
            highReg = regPair->getHighOrder();
            reg = regPair->getLowOrder();
            }
         else if (child->getHighGlobalRegisterNumber() > -1)
            {
            TR_ASSERT( 0,"Long child does not have a register pair\n");
            }
         }

      bool containsInternalPointer = false;
      if (reg->getPinningArrayPointer())
         {
         containsInternalPointer = true;
         }

      copyReg = (reg->containsCollectedReference() && !containsInternalPointer) ?
         cg->allocateCollectedReferenceRegister() :
         cg->allocateRegister(kind);

      if (containsInternalPointer)
         {
         copyReg->setContainsInternalPointer();
         copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
         }
      if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
         {
         if (cg->machine()->getHPRFromGlobalRegisterNumber(child->getGlobalRegisterNumber()) != NULL)
            {
            copyReg->setAssignToHPR(true);
            }
         }
      switch (kind)
         {
         case TR_GPR64:
            opCode = TR::InstOpCode::LGR;
            break;
         case TR_GPR:
            opCode = TR::InstOpCode::getLoadRegOpCode();
            if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                  (child->getDataType() == TR::Int32 ||
                  child->getOpCodeValue() == TR::icall ||
                  child->getOpCodeValue() == TR::icalli ||
                  TR::RealRegister::isHPR(regNum)))
               {
               opCode = TR::InstOpCode::LR;
               }
            break;
         case TR_FPR:
            opCode = TR::InstOpCode::LDR;
            break;
         case TR_VRF:
            opCode = TR::InstOpCode::VLR;
            break;
         default:
            TR_ASSERT( 0, "Invalid register kind.");
         }

      iCursor = isVector ? generateVRRaInstruction(cg, opCode, node, copyReg, reg, iCursor):
                           generateRRInstruction  (cg, opCode, node, copyReg, reg, iCursor);

      reg = copyReg;
      if (debugObj)
         {
         if (isVector)
            debugObj->addInstructionComment( toS390VRRaInstruction(iCursor), "VLR=Reg_deps2");
         else
            debugObj->addInstructionComment( toS390RRInstruction(iCursor), "LR=Reg_deps2");
         }

      if (highReg)
        {
        if(kind == TR_GPR)
         {
         containsInternalPointer = false;
         if (highReg->getPinningArrayPointer())
            {
            containsInternalPointer = true;
            }

         highCopyReg = (highReg->containsCollectedReference() && !containsInternalPointer) ?
            cg->allocateCollectedReferenceRegister() :
            cg->allocateRegister(kind);

         if (containsInternalPointer)
            {
            highCopyReg->setContainsInternalPointer();
            highCopyReg->setPinningArrayPointer(highReg->getPinningArrayPointer());
            }

         TR::InstOpCode::Mnemonic loadOpHigh = TR::InstOpCode::getLoadRegOpCode();
         if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && kind == TR_GPR)
            {
            loadOpHigh = TR::InstOpCode::LR;
            }
         iCursor = generateRRInstruction(cg, loadOpHigh, node, highCopyReg, highReg, iCursor);
         highReg = highCopyReg;
         }
        else // kind == FPR
         {
         highCopyReg = cg->allocateRegister(kind);
         iCursor = generateRRInstruction  (cg, opCode, node, highCopyReg, highReg, iCursor);
         debugObj->addInstructionComment( toS390RRInstruction(iCursor), "LR=Reg_deps2");
         highReg = highCopyReg;
         }
        }
      }
   else
      {
      if (regPair)
         {
         regList.add(regPair->getHighOrder());
         regList.add(regPair->getLowOrder());
         }
      else
         {
         regList.add(reg);
         }

      if (regPair)
         {
         highReg = regPair->getHighOrder();
         reg = regPair->getLowOrder();
         }
      else if ((child->getHighGlobalRegisterNumber() > -1))
         {
         TR_ASSERT( 0, "Long child does not have a register pair\n");
         }
      }
   }

TR::RegisterDependencyConditions  *OMR::Z::RegisterDependencyConditions::clone(TR::CodeGenerator *cg, int32_t additionalRegDeps)
   {
   TR::RegisterDependencyConditions  *other =
      new (cg->trHeapMemory()) TR::RegisterDependencyConditions(_numPreConditions  + additionalRegDeps,
                                                                   _numPostConditions + additionalRegDeps, cg);
   int32_t i = 0;

   for (i = _numPreConditions-1; i >= 0; --i)
      {
      TR::RegisterDependency  *dep = getPreConditions()->getRegisterDependency(i);
      other->getPreConditions()->setDependencyInfo(i, dep->getRegister(), dep->getRealRegister(), dep->getFlags());
      }

   for (i = _numPostConditions-1; i >= 0; --i)
      {
      TR::RegisterDependency  *dep = getPostConditions()->getRegisterDependency(i);
      other->getPostConditions()->setDependencyInfo(i, dep->getRegister(), dep->getRealRegister(), dep->getFlags());
      }

   other->setAddCursorForPre(_addCursorForPre);
   other->setAddCursorForPost(_addCursorForPost);
   return other;
   }

bool
OMR::Z::RegisterDependencyConditions::refsRegister(TR::Register * r)
   {
   for (int32_t i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
         _preConditions->getRegisterDependency(i)->getRefsRegister())
         {
         return true;
         }
      }
   for (int32_t j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
         _postConditions->getRegisterDependency(j)->getRefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool
OMR::Z::RegisterDependencyConditions::defsRegister(TR::Register * r)
   {
   for (int32_t i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
         _preConditions->getRegisterDependency(i)->getDefsRegister())
         {
         return true;
         }
      }
   for (int32_t j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
         _postConditions->getRegisterDependency(j)->getDefsRegister())
         {
         return true;
         }
      }
   return false;
   }

bool
OMR::Z::RegisterDependencyConditions::mayDefineRegister(TR::Register * r)
   {
   for (int32_t j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          _postConditions->getRegisterDependency(j)->getRealRegister() == TR::RealRegister::MayDefine)
         {
         return true;
         }
      }
   return false;
   }

bool
OMR::Z::RegisterDependencyConditions::usesRegister(TR::Register * r)
   {
   for (int32_t i = 0; i < _numPreConditions; i++)
      {
      if (_preConditions->getRegisterDependency(i)->getRegister() == r &&
          (_preConditions->getRegisterDependency(i)->getRefsRegister() || _preConditions->getRegisterDependency(i)->getDefsRegister()))
         {
         return true;
         }
      }

   for (int32_t j = 0; j < _numPostConditions; j++)
      {
      if (_postConditions->getRegisterDependency(j)->getRegister() == r &&
          (_postConditions->getRegisterDependency(j)->getRefsRegister() || _postConditions->getRegisterDependency(j)->getDefsRegister()))
         {
         return true;
         }
      }
   return false;
   }

/**
 * Reason for oldPreCursor/oldPostCursor:
 * When the dependencies have been created by merging new registers into an existing list then only call useRegister for the new registers in the list.
 * Calling useRegister again on the existing registers will cause the totalUseCount to be too high and the virtual register will remain set to the real register
 * for the entire duration of register assignment.
 */
void
OMR::Z::RegisterDependencyConditions::bookKeepingRegisterUses(TR::Instruction * instr, TR::CodeGenerator * cg, int32_t oldPreCursor, int32_t oldPostCursor)
   {
   if (instr->getOpCodeValue() != TR::InstOpCode::ASSOCREGS)
      {
      // We create a register association directive for each dependency

      if (cg->enableRegisterAssociations() && !cg->isOutOfLineColdPath())
         {
         createRegisterAssociationDirective(instr, cg);
         }

      if (_preConditions != NULL)
         {
         for (int32_t i = oldPreCursor; i < _addCursorForPre; i++)
            {
            instr->useRegister(_preConditions->getRegisterDependency(i)->getRegister(), true);
            }
         }

      if (_postConditions != NULL)
         {
         for (int32_t j = oldPostCursor; j < _addCursorForPost; j++)
            {
            instr->useRegister(_postConditions->getRegisterDependency(j)->getRegister(), true);
            }
         }
      }
   }

uint32_t
TR_S390RegisterDependencyGroup::genBitMapOfAssignableGPRs(TR::CodeGenerator * cg,
                                                           uint32_t numberOfRegisters)
   {
   TR::Machine* machine = cg->machine();

   uint32_t availRegMap = machine->genBitMapOfAssignableGPRs();

   for (uint32_t i = 0; i < numberOfRegisters; i++)
      {
      TR::RealRegister::RegNum realReg = _dependencies[i].getRealRegister();
      if (TR::RealRegister::isGPR(realReg))
         {
         availRegMap &= ~machine->getRealRegister(realReg)->getRealRegisterMask();
         }
      }

   return availRegMap;
   }


/** \brief
 *     Checks to ensure that we have enough register pairs available to satisfy all register
 *     dependencies in the current dependency group and aborts the
 *     compilation if not enough register pairs are available.
 *
 *     Also, if highword facility is supported, this function checks the type of each register
 *     dependency condition and sets HPR related flags for future register assignment steps.
 *
 *  \param cg
 *     The code generator used to query various machine state.
 *
 *  \param currentInstruction
 *     The instruction on which the register dependency group is attached.
 *
 *  \param availableGPRMap
 *     The bitmask of available GRP registers.
 *
 *  \param numOfDependencies
 *     The number of pre or post dependencies in the register dependency group.
 **/
void
TR_S390RegisterDependencyGroup::checkRegisterPairSufficiencyAndHPRAssignment(TR::CodeGenerator *cg,
                                                             TR::Instruction* currentInstruction,
                                                             const uint32_t availableGPRMap,
                                                             uint32_t numOfDependencies)
   {
   TR::Machine* machine = cg->machine();

   bool isHighWordSupported = cg->supportsHighWordFacility() && !(cg->comp()->getOption(TR_DisableHighWordRA));
   uint32_t numReqPairs = 0, numAvailPairs = 0;

   // Count the number of required pairs to be assigned in reg deps
   for (int32_t i = 0; i < numOfDependencies; i++)
      {
      TR::RealRegister::RegNum realRegI = _dependencies[i].getRealRegister();
      TR::Register* virtRegI            = _dependencies[i].getRegister();

      if (realRegI == TR::RealRegister::EvenOddPair)
         numReqPairs++;

      // Set HPR or 64-bit register assignment flags
      if (isHighWordSupported)
         {
         virtRegI->setIsNotHighWordUpgradable(true);

         if (virtRegI &&
             virtRegI->getKind() != TR_FPR &&
             virtRegI->getKind() != TR_VRF)
            {
            // on 64-bit conservatively assume rEP and rRA are going to clobber highword
            if (TR::Compiler->target.is64Bit() &&
                !(currentInstruction->isLabel() &&
                  toS390LabelInstruction(currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream()) &&
                (realRegI == cg->getReturnAddressRegister() ||
                 realRegI == cg->getEntryPointRegister()))
               {
               virtRegI->setIs64BitReg(true);
               }

            virtRegI->setAssignToHPR(false);
            if (TR::RealRegister::isHPR(realRegI))
               {
               virtRegI->setAssignToHPR(true);
               cg->maskAvailableHPRSpillMask(~(machine->getRealRegister(realRegI)->getRealRegisterMask()));
               }
            else if (TR::RealRegister::isGPR(realRegI) && virtRegI->is64BitReg())
               {
               cg->maskAvailableHPRSpillMask(~(machine->getRealRegister(realRegI)->getRealRegisterMask() << 16));
               }
            }
         }
      }

   // Count the number of even-odd GPR pairs remaining after deps are filled and breaks early if the
   // number of register pairs in available is sufficient.
   bool isRegPairSufficient = false;

   if (numReqPairs > 0)
      {
      for (int32_t i = 0; i < NUM_S390_GPR; i += 2)
         {
         if ((availableGPRMap >> i) & 0x3 == 0x3)
            {
            numAvailPairs++;
            if (numAvailPairs >= numReqPairs)
               {
               isRegPairSufficient = true;
               break;
               }
            }
         }
      }
   else
      {
      isRegPairSufficient = true;
      }

   if (!isRegPairSufficient)
      {
      TR::Compilation *comp = cg->comp();
      if (cg->getDebug())
         {
         trfprintf(comp->getOutFile(), "\nNum AVAIL PAIRS %d, Num REQ PAIRS %d\n", numAvailPairs, numReqPairs);
         trfprintf(comp->getOutFile(), "AVAIL REG MAP %x\n", availableGPRMap);
         cg->getDebug()->printRegisterDependencies(comp->getOutFile(), this, numOfDependencies);
         }
      TR_ASSERT( 0, "checkDependencyGroup::Insufficient available pairs to satisfy all reg deps.\n");
      comp->failCompilation<TR::CompilationException>("checkDependencyGroup::Insufficient available pairs to satisfy all reg deps, abort compilation\n");
      }
   }

/**
 *
 * \brief
 *      Performs pairwise checks in dependencies to find duplicates
 *
 *      1. If the duplicated registers are virtual registers or real registers, assert fail.
 *      2. If the duplicated registers are GPR/HPR dependencies, remove the duplicates.
 *
 * \param cg
 *      The CodeGenerator object used to query machine states
 *
 * \param numOfDependencies
 *      The number of register dependency conditions to examine.
 */
void
TR_S390RegisterDependencyGroup::checkRegisterDependencyDuplicates(TR::CodeGenerator* cg,
                                                                  const uint32_t numOfDependencies)
   {
   TR::Compilation *comp = cg->comp();

   if (numOfDependencies <= 1)
      return;

   bool isHighWordSupported = cg->supportsHighWordFacility() && !cg->comp()->getOption(TR_DisableHighWordRA);

   for (uint32_t i = 0; i < numOfDependencies - 1; ++i)
      {
      TR::Register* virtRegI = _dependencies[i].getRegister();
      TR::Register* virtRegJ;
      TR::RealRegister::RegNum realRegI = _dependencies[i].getRealRegister();
      TR::RealRegister::RegNum realRegJ;

      for (uint32_t j = i + 1; j < numOfDependencies; ++j)
         {
         virtRegJ = _dependencies[j].getRegister();
         realRegJ = _dependencies[j].getRealRegister();

         if (virtRegI == virtRegJ
               && realRegI != TR::RealRegister::SpilledReg
               && realRegJ != TR::RealRegister::SpilledReg)
            {
            TR_ASSERT(false, "Virtual register duplicate found in a register dependency\n");
            }

         if (realRegI == realRegJ &&
               TR::RealRegister::isRealReg(realRegI))
            {
            TR_ASSERT(false, "Real register duplicate found in a register dependency\n");
            }

         // highword RA: remove duplicated GPR/HPR deps
         if (isHighWordSupported &&
                realRegI == realRegJ + (TR::RealRegister::FirstHPR - TR::RealRegister::FirstGPR) &&
                virtRegI->isPlaceholderReg() &&
                virtRegJ->is64BitReg() &&
                virtRegJ->getKind() != TR_FPR &&
                virtRegJ->getKind() != TR_VRF)
            {
            traceMsg(comp, "Remove duplicate GPR (%s) / HPR (%s) register dependency\n", virtRegJ->getRegisterName(comp), virtRegI->getRegisterName(comp));
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            }

         if (isHighWordSupported &&
                realRegJ == realRegI + (TR::RealRegister::FirstHPR - TR::RealRegister::FirstGPR) &&
                virtRegJ->isPlaceholderReg() &&
                virtRegI->is64BitReg() &&
                virtRegI->getKind() != TR_FPR &&
                virtRegI->getKind() != TR_VRF)
            {
            traceMsg(comp, "Remove duplicate GPR (%s) / HPR (%s) register dependency\n", virtRegI->getRegisterName(comp), virtRegJ->getRegisterName(comp));
            _dependencies[j].setRealRegister(TR::RealRegister::NoReg);
            }
         }
      }
   }

/**
 *
 * \brief
 *      Generates a uint32_t bit map for available GPR registers.
 *      This function also checks to make sure that there are enough register pairs
 *      for assignment, and that there are no duplicates in real, virtual, and
 *      HPR registers.
 *
 * \param cg
 *      The CodeGenerator object used to query machine states
 *
 * \param currentInstruction
 *      The current instruction on which the register dependency group is attached.
 *
 * \param numOfDependencies
 *      The number of register dependency conditions to examine.
 *
 * \return
 *      a bit mask of available GPR map.
 *      Bit is set to 1 if the GPR is available, and 0 otherwise.
*/
uint32_t
TR_S390RegisterDependencyGroup::checkDependencyGroup(TR::CodeGenerator * cg,
                                                     TR::Instruction * currentInstruction,
                                                     uint32_t          numOfDependencies)
   {
   // availableGPRMap is a bitmap of all available (not locked) GPRs on the machine
   // excluding the real registers already specified in the _dependencies.
   uint32_t availableGPRMap = genBitMapOfAssignableGPRs(cg, numOfDependencies);
   cg->setAvailableHPRSpillMask(0xFFFF0000);

   ///// Check the number of register pairs. The compilation may fail due to insufficient register pairs
   checkRegisterPairSufficiencyAndHPRAssignment(cg, currentInstruction, availableGPRMap,
                                                numOfDependencies);

   ///// Check and remove duplicates
   checkRegisterDependencyDuplicates(cg, numOfDependencies);

   return availableGPRMap;
   }

void
TR_S390RegisterDependencyGroup::assignRegisters(TR::Instruction   *currentInstruction,
                                                TR_RegisterKinds  kindToBeAssigned,
                                                uint32_t          numOfDependencies,
                                                TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Machine *machine = cg->machine();
   TR::Register * virtReg;
   TR::RealRegister::RegNum dependentRegNum;
   TR::RealRegister * dependentRealReg, * assignedRegister;
   int32_t i, j;
   bool changed;
   uint32_t availRegMask = checkDependencyGroup(cg, currentInstruction, numOfDependencies);
   bool highRegSpill = false;

   if (!comp->getOption(TR_DisableOOL))
      {
      for (i = 0; i< numOfDependencies; i++)
         {
         virtReg = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();
         if (dependentRegNum == TR::RealRegister::SpilledReg && !_dependencies[i].getRegister()->getRealRegister())
            {
            TR_ASSERT( virtReg->getBackingStorage(),"should have a backing store if dependentRegNum == spillRegIndex()\n");
            TR::RealRegister * spilledHPR = NULL;
            if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) && virtReg->getKind() != TR_FPR &&
                  virtReg->getKind() != TR_VRF && virtReg->getAssignedRealRegister() == NULL)
               {
               spilledHPR = cg->machine()->findVirtRegInHighWordRegister(virtReg);
               cg->traceRegisterAssignment (" \nOOL: found %R spilled to %R", virtReg, spilledHPR);
               }
            if (virtReg->getAssignedRealRegister() || spilledHPR)
               {
               // this happens when the register was first spilled in main line path then was reverse spilled
               // and assigned to a real register in OOL path. We protected the backing store when doing
               // the reverse spill so we could re-spill to the same slot now
               TR::Node *currentNode = currentInstruction->getNode();
               TR::RealRegister *assignedReg;
               if (spilledHPR)
                  assignedReg = spilledHPR;
               else
                  assignedReg = toRealRegister(virtReg->getAssignedRegister());
               cg->traceRegisterAssignment ("\nOOL: Found %R spilled in main line and reused inside OOL", virtReg);
               TR::MemoryReference * tempMR = generateS390MemoryReference(currentNode, virtReg->getBackingStorage()->getSymbolReference(), cg);
               TR::InstOpCode::Mnemonic opCode;
               TR_RegisterKinds rk = virtReg->getKind();
               switch (rk)
                  {
                  case TR_GPR64:
                     opCode = TR::InstOpCode::LG;
                     break;
                  case TR_GPR:
                     opCode = TR::InstOpCode::getLoadOpCode();
                     break;
                  case TR_FPR:
                     opCode = TR::InstOpCode::LD;
                     break;
                  case TR_VRF:
                     opCode = TR::InstOpCode::VL;
                     break;
                  default:
                     TR_ASSERT( 0, "\nRegister kind not supported in OOL spill\n");
                     break;
                  }

               if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                   rk != TR_FPR && rk != TR_VRF)
                  {
                  opCode = virtReg->is64BitReg()? TR::InstOpCode::LG : TR::InstOpCode::L;

                  if (assignedReg->isHighWordRegister())
                     {
                     // virtReg was spilled to an HPR and now we need it spilled to stack
                     opCode = TR::InstOpCode::LFH;
                     if (virtReg->is64BitReg())
                        {
                        TR_ASSERT(virtReg->containsCollectedReference(),
                                "virtReg is assigned to HPR but is not a spilled compress pointer");
                        // we need to decompress pointer
                        tempMR = generateS390MemoryReference(*tempMR, 4, cg);
                        virtReg->setSpilledToHPR(false);
                        }
                     }
                  }
               bool isVector = rk == TR_VRF ? true : false;

               TR::Instruction *inst = isVector ? generateVRXInstruction(cg, opCode, currentNode, assignedReg, tempMR, 0, currentInstruction) :
                                                     generateRXInstruction (cg, opCode, currentNode, assignedReg, tempMR, currentInstruction);
               cg->traceRAInstruction(inst);

               assignedReg->setAssignedRegister(NULL);
               virtReg->setAssignedRegister(NULL);
               assignedReg->setState(TR::RealRegister::Free);

               if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA) &&
                   virtReg->is64BitReg() && rk != TR_FPR && rk != TR_VRF)
                  {
                  assignedReg->getHighWordRegister()->setAssignedRegister(NULL);
                  assignedReg->getHighWordRegister()->setState(TR::RealRegister::Free);
                  }
               }

            // now we are leaving the OOL sequence, anything that was previously spilled in OOL hot path or main line
            // should be considered as mainline spill for the next OOL sequence.
            virtReg->getBackingStorage()->setMaxSpillDepth(1);
            }
         // we also need to free up all locked backing storage if we are exiting the OOL during backwards RA assignment
         else if (currentInstruction->isLabel() && virtReg->getAssignedRealRegister())
            {
            TR_BackingStore * location = virtReg->getBackingStorage();
            TR_RegisterKinds rk = virtReg->getKind();
            int32_t dataSize;
            if (toS390LabelInstruction(currentInstruction)->getLabelSymbol()->isStartOfColdInstructionStream() &&
                location)
               {
               traceMsg (comp,"\nOOL: Releasing backing storage (%p)\n", location);
               if (rk == TR_GPR)
                  dataSize = TR::Compiler->om.sizeofReferenceAddress();
               else if (rk == TR_VRF)
                  dataSize = 16; // Will change to 32 in a few years..
               else
                  dataSize = 8;

               location->setMaxSpillDepth(0);
               cg->freeSpill(location,dataSize,0);
               virtReg->setBackingStorage(NULL);
               }
            }
         }
      }
   
   uint32_t numGPRs = 0;
   uint32_t numHPRs = 0;
   uint32_t numFPRs = 0;
   uint32_t numVRFs = 0;

   // Used to do lookups using real register numbers
   OMR::RegisterDependencyMap map(_dependencies, numOfDependencies);

   for (i = 0; i < numOfDependencies; i++)
      {
      map.addDependency(_dependencies[i], i);

      dependentRegNum = _dependencies[i].getRealRegister();

      if (dependentRegNum != TR::RealRegister::SpilledReg &&
          dependentRegNum != TR::RealRegister::KillVolHighRegs &&
          dependentRegNum != TR::RealRegister::NoReg)
         {
         virtReg = _dependencies[i].getRegister();
         switch (virtReg->getKind())
            {
            case TR_GPR:
               // TODO (Issue #2739): Currently HPRs are not propperly tagged with TR_HPR register kind and are instead
               // labeled as TR_GPR. This produces incorrect counts for the number of GPR vs. HPR dependencies that we
               // have. We use a workaround here to circumvent the issue but a larger change is needed to properly tag
               // the register kinds and eliminate this ugly code.
               if ((dependentRegNum >= TR::RealRegister::FirstHPR && dependentRegNum <= TR::RealRegister::LastHPR) ||

                     // This case is used to handle removed GPR / HPR dependencies in checkRegisterDependencyDuplicates
                     // in which we don't actually remove the HPR dependency but simply set the target real register to
                     // be TR::RealRegister::NoReg. This is also a hack that needs to be cleaned up, i.e. we shouldn't
                     // be creating the duplicate register dependency in the first place.
                     (virtReg->isPlaceholderReg() && dependentRegNum == TR::RealRegister::NoReg))
                  {
                  ++numHPRs;
                  }
               else
                  {
                  ++numGPRs;
                  }
               break;
            case TR_HPR:
               ++numHPRs;
               break;
            case TR_FPR:
               ++numFPRs;
               break;
            case TR_VRF:
               ++numVRFs;
               break;
            default:
               break;
            }
         }
      }

#ifdef DEBUG
   int32_t lockedGPRs = 0;
   int32_t lockedHPRs = 0;
   int32_t lockedFPRs = 0;
   int32_t lockedVRFs = 0;

   // count up how many registers are locked for each type
   for (int32_t i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedGPRs;
      }

   for (int32_t i = TR::RealRegister::FirstHPR; i <= TR::RealRegister::LastHPR; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedHPRs;
      }

   for (int32_t i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedFPRs;
      }

   for (int32_t i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; ++i)
      {
      TR::RealRegister* realReg = machine->getRealRegister((TR::RealRegister::RegNum)i);
      if (realReg->getState() == TR::RealRegister::Locked)
         ++lockedVRFs;
      }

   TR_ASSERT(lockedGPRs == machine->getNumberOfLockedRegisters(TR_GPR), "Inconsistent number of locked GPRs");
   TR_ASSERT(lockedHPRs == machine->getNumberOfLockedRegisters(TR_HPR), "Inconsistent number of locked HPRs");
   TR_ASSERT(lockedFPRs == machine->getNumberOfLockedRegisters(TR_FPR), "Inconsistent number of locked FPRs");
   TR_ASSERT(lockedVRFs == machine->getNumberOfLockedRegisters(TR_VRF), "Inconsistent number of locked VRFs");
#endif

   // To handle circular dependencies, we block a real register if (1) it is already assigned to a correct
   // virtual register and (2) if it is assigned to one register in the list but is required by another.
   // However, if all available registers are requested, we do not block in case (2) to avoid all registers
   // being blocked.

   bool haveSpareGPRs = true;
   bool haveSpareHPRs = true;
   bool haveSpareFPRs = true;
   bool haveSpareVRFs = true;

   TR_ASSERT(numGPRs <= (TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1 - machine->getNumberOfLockedRegisters(TR_GPR)), "Too many GPR dependencies, unable to assign");
   TR_ASSERT(numHPRs <= (TR::RealRegister::LastHPR - TR::RealRegister::FirstHPR + 1 - machine->getNumberOfLockedRegisters(TR_HPR)), "Too many HPR dependencies, unable to assign");
   TR_ASSERT(numFPRs <= (TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1 - machine->getNumberOfLockedRegisters(TR_FPR)), "Too many FPR dependencies, unable to assign");
   TR_ASSERT(numVRFs <= (TR::RealRegister::LastVRF - TR::RealRegister::FirstVRF + 1 - machine->getNumberOfLockedRegisters(TR_VRF)), "Too many VRF dependencies, unable to assign");

   if (numGPRs == (TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1 - machine->getNumberOfLockedRegisters(TR_GPR)))
      haveSpareGPRs = false;
   if (numHPRs == (TR::RealRegister::LastHPR - TR::RealRegister::FirstHPR + 1 - machine->getNumberOfLockedRegisters(TR_HPR)))
      haveSpareHPRs = false;
   if (numFPRs == (TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1 - machine->getNumberOfLockedRegisters(TR_FPR)))
      haveSpareFPRs = false;
   if (numVRFs == (TR::RealRegister::LastVRF - TR::RealRegister::FirstVRF + 1 - machine->getNumberOfLockedRegisters(TR_VRF)))
      haveSpareVRFs = false;

   for (i = 0; i < numOfDependencies; i++)
      {
      virtReg = _dependencies[i].getRegister();

      if (virtReg->getAssignedRealRegister() != NULL)
         {
         if (_dependencies[i].getRealRegister() == TR::RealRegister::NoReg)
            {
            virtReg->block();
            }
         else
            {
            TR::RealRegister::RegNum assignedRegNum = toRealRegister(virtReg->getAssignedRealRegister())->getRegisterNumber();

            // Always block if the required register and assigned register match or if the assigned register is 
            // required by another dependency but only if there are any spare registers left so as to avoid blocking
            // all existing registers
            if (_dependencies[i].getRealRegister() == assignedRegNum ||
                (map.getDependencyWithTarget(assignedRegNum) &&
                 ((virtReg->getKind() != TR_GPR || haveSpareGPRs) &&
                  (virtReg->getKind() != TR_HPR || haveSpareHPRs) &&
                  (virtReg->getKind() != TR_FPR || haveSpareFPRs) &&
                  (virtReg->getKind() != TR_VRF || haveSpareVRFs))))
               {
               virtReg->block();
               }
            }
         }

      // Check for directive to spill high registers. Used on callouts
      if ( _dependencies[i].getRealRegister() == TR::RealRegister::KillVolHighRegs &&
           cg->use64BitRegsOn32Bit() )
         {
         machine->spillAllVolatileHighRegisters(currentInstruction);
         _dependencies[i].setRealRegister(TR::RealRegister::NoReg);  // ignore from now on
         highRegSpill = true;
         }

      }

   /////    REGISTER PAIRS

   // We handle register pairs by passing regpair dependencies stubs into the register dependency
   // list.  The following pieces of code are responsible for replacing these stubs with actual
   // real register values, using register allocation routines in the S390Machine, and hence,
   // allow the dependency routines to complete the allocation.
   //
   // We need to run through the list of dependencies looking for register pairs.
   // We assign a register pair to any such pairs that are found and then look for the low
   // and high order regs in the list of deps.  We update the low and high order pair deps with
   // the real reg assigned by to the pair, and then let the these deps be handled as any other
   // single reg dep.
   //
   // If the virt registers have already been assigned, the low and high order
   // regs obtained from regPair will not match the ones in the dependency list.
   // Hence, will fall through the dep entries for the associated virt high
   // and low regs as the real high and low will not match the dep virt reg entries.
   //
   for (i = 0; i < numOfDependencies; i++)
      {
      // Get reg pair pointer
      //
      TR::RegisterPair * virtRegPair = _dependencies[i].getRegister()->getRegisterPair();

      // If it is a register pair
      //
      if (virtRegPair == NULL)
         {
         continue;
         }
      else
         {
         dependentRegNum = _dependencies[i].getRealRegister();
         }

      // Is this an Even-Odd pair assignment
      //
      if (dependentRegNum == TR::RealRegister::EvenOddPair
          || dependentRegNum == TR::RealRegister::FPPair)
         {
         TR::Register * virtRegHigh = virtRegPair->getHighOrder();
         TR::Register * virtRegLow = virtRegPair->getLowOrder();
         TR::RegisterPair * retPair = NULL;

         #if 0
         fprintf(stderr, "DEP: PRE REG PAIR ASS: %p(%p,%p)->(%p,%p)\n", virtRegPair, virtRegPair->getHighOrder(),
            virtRegPair->getLowOrder(), virtRegPair->getHighOrder()->getAssignedRegister(),
            virtRegPair->getLowOrder()->getAssignedRegister());
         #endif

         // Assign a real register pair
         // This call returns a pair placeholder that points to the assigned real registers
         //     retPair:
         //                 high -> realRegHigh -> virtRegHigh
         //                 low  -> realRegLow  -> virtRegLow
         //
         // No bookkeeping on assignment call as we do bookkeeping at end of this method
         //
         retPair = (TR::RegisterPair *) machine->assignBestRegister((TR::Register *) virtRegPair,
                                                                   currentInstruction,
                                                                   NOBOOKKEEPING,
                                                                   availRegMask);

         // Grab real regs that are returned as high/low in retPair
         //
         TR::Register * realRegHigh = retPair->getHighOrder();
         TR::Register * realRegLow  = retPair->getLowOrder();

         // We need to update the virtRegPair assignedRegister pointers
         // The real regs should already be pointing at the virtuals after
         // returning from the assignment call.
         //
         virtRegPair->getHighOrder()->setAssignedRegister(realRegHigh);
         virtRegPair->getLowOrder()->setAssignedRegister(realRegLow);

         #if 0
         fprintf(stderr, "DEP: POST REG PAIR ASS: %p(%p,%p)->(%p,%p)=(%d,%d)\n", virtRegPair, virtRegPair->getHighOrder(),
            virtRegPair->getLowOrder(), virtRegPair->getHighOrder()->getAssignedRegister(),
            virtRegPair->getLowOrder()->getAssignedRegister(), toRealRegister(realRegHigh)->getRegisterNumber() - 1,
            toRealRegister(realRegLow)->getRegisterNumber() - 1);
         #endif

         // We are done with the reg pair structure.  No assignment later here.
         //
         _dependencies[i].setRealRegister(TR::RealRegister::NoReg);

         // See if there are any LegalEvenOfPair/LegalOddOfPair deps in list that correspond
         // to the assigned pair
         //
         for (j = 0; j < numOfDependencies; j++)
            {

            // Check to ensure correct assignment of even/odd pair.
            //
            if (((_dependencies[j].getRealRegister() == TR::RealRegister::LegalEvenOfPair
                 || _dependencies[j].getRealRegister() == TR::RealRegister::LegalFirstOfFPPair) &&
                 _dependencies[j].getRegister() == virtRegLow) ||
                ((_dependencies[j].getRealRegister() == TR::RealRegister::LegalOddOfPair
                 || _dependencies[j].getRealRegister() == TR::RealRegister::LegalSecondOfFPPair)  &&
                 _dependencies[j].getRegister() == virtRegHigh))
               {
               TR_ASSERT( 0, "Register pair odd and even assigned wrong\n");
               }

            // Look for the Even reg in deps
            if ((_dependencies[j].getRealRegister() == TR::RealRegister::LegalEvenOfPair
                || _dependencies[j].getRealRegister() == TR::RealRegister::LegalFirstOfFPPair) &&
                _dependencies[j].getRegister() == virtRegHigh)
               {
               _dependencies[j].setRealRegister(TR::RealRegister::NoReg);
               toRealRegister(realRegLow)->block();
               }

            // Look for the Odd reg in deps
            if ((_dependencies[j].getRealRegister() == TR::RealRegister::LegalOddOfPair
                || _dependencies[j].getRealRegister() == TR::RealRegister::LegalSecondOfFPPair) &&
                _dependencies[j].getRegister() == virtRegLow)
               {
               _dependencies[j].setRealRegister(TR::RealRegister::NoReg);
               toRealRegister(realRegHigh)->block();
               }
            }

         changed = true;
         }
      }

   // If we have an [Even|Odd]OfLegal of that has not been associated
   // to a reg pair in this list (in case we only need to ensure that the
   // reg can be assigned to a pair in the future), we need to handle such
   // cases here.
   //
   // It is possible that left over legal even/odd regs are actually real regs
   // that were already assigned in a previous instruction.  We check for this
   // case before assigning a new reg by checking for assigned real regs.  If
   // real reg is assigned, we need to make sure it is a legal assignment.
   //
   for (i = 0; i < numOfDependencies; i++)
      {
      TR::Register * virtReg = _dependencies[i].getRegister();
      TR::Register * assRealReg = virtReg->getAssignedRealRegister();
      dependentRegNum = _dependencies[i].getRealRegister();

      if (dependentRegNum == TR::RealRegister::LegalEvenOfPair)
         {
         // if not assigned, or assigned but not legal.
         //
         if (assRealReg == NULL || (!machine->isLegalEvenRegister(toRealRegister(assRealReg), ALLOWBLOCKED)))
            {
            TR::RealRegister * bestEven = machine->findBestLegalEvenRegister();
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            toRealRegister(bestEven)->block();

            #ifdef DEBUG_MMIT
            fprintf(stderr, "DEP ASS Best Legal Even %p (NOT pair)\n", toRealRegister(bestEven)->getRegisterNumber() - 1);
            #endif
            }
         else
            {
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            }
         }

      if (dependentRegNum == TR::RealRegister::LegalOddOfPair)
         {
         // if not assigned, or assigned but not legal.
         //
         if (assRealReg == NULL || (!machine->isLegalOddRegister(toRealRegister(assRealReg), ALLOWBLOCKED)))
            {
            TR::RealRegister * bestOdd = machine->findBestLegalOddRegister();
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            toRealRegister(bestOdd)->block();

            #ifdef DEBUG_MMIT
            fprintf(stderr, "DEP ASS Best Legal Odd %p (NOT pair)\n", toRealRegister(bestOdd)->getRegisterNumber() - 1);
            #endif
            }
         else
            {
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            }
         }

      if (dependentRegNum == TR::RealRegister::LegalFirstOfFPPair)
         {
         // if not assigned, or assigned but not legal.
         //
         if (assRealReg == NULL || (!machine->isLegalFirstOfFPRegister(toRealRegister(assRealReg), ALLOWBLOCKED)))
            {
            TR::RealRegister * bestFirstFP = machine->findBestLegalSiblingFPRegister(true);
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            toRealRegister(bestFirstFP)->block();

            #ifdef DEBUG_FPPAIR
            fprintf(stderr, "DEP ASS Best Legal FirstOfFPPair %p (NOT pair)\n", toRealRegister(bestFirstFP)->getRegisterNumber() - 1);
            #endif
            }
         else
            {
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            }
         }

      if (dependentRegNum == TR::RealRegister::LegalSecondOfFPPair)
         {
         // if not assigned, or assigned but not legal.
         //
         if (assRealReg == NULL || (!machine->isLegalSecondOfFPRegister(toRealRegister(assRealReg), ALLOWBLOCKED)))
            {
            TR::RealRegister * bestSecondFP = machine->findBestLegalSiblingFPRegister(false);
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            toRealRegister(bestSecondFP)->block();

            #ifdef DEBUG_FPPAIR
            fprintf(stderr, "DEP ASS Best Legal SecondOfFPPair  %p (NOT pair)\n", toRealRegister(bestSecondFP)->getRegisterNumber() - 1);
            #endif
            }
         else
            {
            _dependencies[i].setRealRegister(TR::RealRegister::NoReg);
            }
         }

      }// for

   #if 0
    {
      fprintf(stdout,"PART II ---------------------------------------\n");
      fprintf(stdout,"DEPENDS for INST [%p]\n",currentInstruction);
      printDeps(stdout, numberOfRegisters);
    }
   #endif


   /////    COERCE

   // These routines are responsible for assigning registers and using necessary
   // ops to ensure that any defined dependency [virt reg, real reg] (where real reg
   // is not NoReg), is assigned such that virtreg->realreg.
   //
   do
      {
      changed = false;

      // For each dependency
      //
      for (i = 0; i < numOfDependencies; i++)
         {
         virtReg = _dependencies[i].getRegister();

         TR_ASSERT( virtReg != NULL, "null virtual register during coercion");
         dependentRegNum = _dependencies[i].getRealRegister();

         dependentRealReg = machine->getRealRegister(dependentRegNum);

         // If dep requires a specific real reg, and the real reg is free
         if (dependentRegNum != TR::RealRegister::NoReg     &&
             dependentRegNum != TR::RealRegister::AssignAny &&
             dependentRegNum != TR::RealRegister::SpilledReg &&
             dependentRealReg->getState() == TR::RealRegister::Free)
            {
            #if 0
               fprintf(stdout, "COERCE1 %i (%s, %d)\n", i, cg->getDebug()->getName(virtReg), dependentRegNum-1);
            #endif

            // Assign the real reg to the virt reg
            machine->coerceRegisterAssignment(currentInstruction, virtReg, dependentRegNum, DEPSREG);
            if(comp->getOption(TR_EnableTrueRegisterModel) && _dependencies[i].getDefsRegister() &&
                virtReg->isPendingSpillOnDef())
              {
              TR::RealRegister * realReg = machine->getRealRegister(dependentRegNum);
              if(cg->insideInternalControlFlow())
                machine->reverseSpillState(cg->getInstructionAtEndInternalControlFlow(), virtReg, toRealRegister(realReg));
              else
                machine->reverseSpillState(currentInstruction, virtReg, toRealRegister(realReg));
              }
            virtReg->block();
            changed = true;
            }
         }
      }
   while (changed == true);


   do
      {
      changed = false;
      // For each dependency
      for (i = 0; i < numOfDependencies; i++)
         {
         virtReg = _dependencies[i].getRegister();
         assignedRegister = NULL;

         // Is a real reg assigned to the virt reg
         if (virtReg->getAssignedRealRegister() != NULL)
            {
            assignedRegister = toRealRegister(virtReg->getAssignedRealRegister());
            }

         dependentRegNum = _dependencies[i].getRealRegister();
         dependentRealReg = machine->getRealRegister(dependentRegNum);

         // If the dependency requires a real reg
         // and the assigned real reg is not equal to the required one
         if (dependentRegNum  != TR::RealRegister::NoReg     &&
             dependentRegNum  != TR::RealRegister::AssignAny &&
             dependentRegNum  != TR::RealRegister::SpilledReg)
            {
            if(dependentRealReg != assignedRegister)
              {
              // Coerce the assignment
              #if 0
               fprintf(stdout, "COERCE2 %i (%s, %d)\n", i, cg->getDebug()->getName(virtReg), dependentRegNum-1);
              #endif

               machine->coerceRegisterAssignment(currentInstruction, virtReg, dependentRegNum, DEPSREG);
               virtReg->block();
               changed = true;

               // If there was a wrongly assigned real register
               if (assignedRegister != NULL && assignedRegister->getState() == TR::RealRegister::Free)
                 {
                 // For each dependency
                 for (int32_t lcount = 0; lcount < numOfDependencies; lcount++)
                   {
                   // get the dependencies required real reg
                   TR::RealRegister::RegNum aRealNum = _dependencies[lcount].getRealRegister();
                   // If the real reg is the same as the one just assigned
                   if (aRealNum != TR::RealRegister::NoReg     &&
                       aRealNum != TR::RealRegister::AssignAny &&
                       aRealNum != TR::RealRegister::SpilledReg &&
                       assignedRegister == machine->getRealRegister(aRealNum))
                     {
                       #if 0
                        fprintf(stdout, "COERCE3 %i (%s, %d)\n", i, cg->getDebug()->getName(virtReg), aRealNum-1);
                       #endif

                        // Coerce the assignment
                        TR::Register *depReg = _dependencies[lcount].getRegister();
                        machine->coerceRegisterAssignment(currentInstruction, depReg, aRealNum, DEPSREG);
                        _dependencies[lcount].getRegister()->block();
                        if(comp->getOption(TR_EnableTrueRegisterModel) && _dependencies[lcount].getDefsRegister() &&
                            depReg->isPendingSpillOnDef())
                          {
                          TR::RealRegister * realReg = machine->getRealRegister(aRealNum);
                          if(cg->insideInternalControlFlow())
                            machine->reverseSpillState(cg->getInstructionAtEndInternalControlFlow(), depReg, toRealRegister(realReg));
                          else
                            machine->reverseSpillState(currentInstruction, depReg, toRealRegister(realReg));
                          }
                        break;
                     }
                   }
                 }
              }
            if(comp->getOption(TR_EnableTrueRegisterModel) && _dependencies[i].getDefsRegister() &&
                virtReg->isPendingSpillOnDef())
              {
              TR::RealRegister * realReg = machine->getRealRegister(dependentRegNum);
              if(cg->insideInternalControlFlow())
                machine->reverseSpillState(cg->getInstructionAtEndInternalControlFlow(), virtReg, toRealRegister(realReg));
              else
                machine->reverseSpillState(currentInstruction, virtReg, toRealRegister(realReg));
              }
            }
         }
      }
   while (changed == true);



   /////    ASSIGN ANY

   // AssignAny Support
   // This code allows any register to be assigned to the dependency.
   // This is generally used when we wish to force an early assignment to a virt reg
   // (before the first inst that actually uses the reg).
   //
   // assign those used in memref first
   for (i = 0; i < numOfDependencies; i++)
      {
      if (_dependencies[i].getRealRegister() == TR::RealRegister::AssignAny)
         {
         virtReg = _dependencies[i].getRegister();

         if(virtReg->isUsedInMemRef())
           {
           virtReg->setAssignToHPR(false);
           uint32_t availRegMask =~TR::RealRegister::GPR0Mask;

            // No bookkeeping on assignment call as we do bookkeeping at end of this method
            TR::Register * targetReg = machine->assignBestRegister(virtReg, currentInstruction, NOBOOKKEEPING, availRegMask);

            // Disable this dependency
            _dependencies[i].setRealRegister(toRealRegister(targetReg)->getRegisterNumber());
            virtReg->block();
            if(comp->getOption(TR_EnableTrueRegisterModel) && _dependencies[i].getDefsRegister() &&
                virtReg->isPendingSpillOnDef())
              {
              if(cg->insideInternalControlFlow())
                machine->reverseSpillState(cg->getInstructionAtEndInternalControlFlow(), virtReg, toRealRegister(targetReg));
              else
                machine->reverseSpillState(currentInstruction, virtReg, toRealRegister(targetReg));
              }
           }
         }
      }

   // assign those which are not used in memref
   for (i = 0; i < numOfDependencies; i++)
      {
      if (_dependencies[i].getRealRegister() == TR::RealRegister::AssignAny)
         {
         virtReg = _dependencies[i].getRegister();

         if(!virtReg->isUsedInMemRef())
            {
            virtReg = _dependencies[i].getRegister();
            // No bookkeeping on assignment call as we do bookkeeping at end of this method
            TR::Register * targetReg = machine->assignBestRegister(virtReg, currentInstruction, NOBOOKKEEPING, 0xffffffff);

            // Disable this dependency
            _dependencies[i].setRealRegister(toRealRegister(targetReg)->getRegisterNumber());

            virtReg->block();
            if(comp->getOption(TR_EnableTrueRegisterModel) && _dependencies[i].getDefsRegister() &&
                virtReg->isPendingSpillOnDef())
              {
              if(cg->insideInternalControlFlow())
                machine->reverseSpillState(cg->getInstructionAtEndInternalControlFlow(), virtReg, toRealRegister(targetReg));
              else
                machine->reverseSpillState(currentInstruction, virtReg, toRealRegister(targetReg));
              }
            }
          }
      }

   /////    UNBLOCK


   // Unblock all other regs
   //
   unblockRegisters(numOfDependencies, cg);



   /////    BOOKKEEPING

   // Check to see if we can release any registers that are not used in the future
   // This is why we omit BOOKKEEPING in the assign calls above
   //
   for (i = 0; i < numOfDependencies; i++)
      {
      TR::Register * dependentRegister = getRegisterDependency(i)->getRegister();

      // We decrement the use count, and kill if there are no further uses
      // We pay special attention to pairs, as it is not the parent placeholder
      // that must have its count decremented, but rather the children.
      if (dependentRegister->getRegisterPair() == NULL &&
          (dependentRegister->getAssignedRegister() || (comp->getOption(TR_EnableTrueRegisterModel) && dependentRegister->isLive())) &&
          getRegisterDependency(i)->getRealRegister() != TR::RealRegister::SpilledReg
         )
         {
         TR::Register * assignedRegister = dependentRegister->getAssignedRegister();
         TR::RealRegister * assignedRealRegister = assignedRegister != NULL ? assignedRegister->getRealRegister() : NULL;
         dependentRegister->decFutureUseCount();
         dependentRegister->setIsLive();
         if (assignedRegister && assignedRealRegister->getState() != TR::RealRegister::Locked)
            {
            TR_ASSERT(dependentRegister->getFutureUseCount() >=0,
                    "\nReg dep assignment: futureUseCount should not be negative\n");
            }
         if ((dependentRegister->getFutureUseCount() == 0) ||
             (comp->getOption(TR_EnableTrueRegisterModel) && !currentInstruction->isLabel() &&
              getRegisterDependency(i)->getDefsRegister() && !getRegisterDependency(i)->getRefsRegister() &&
              !currentInstruction->defsRegister(dependentRegister)))
            {
            // check if need to free HW
            if (assignedRealRegister != NULL && cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
               {
               if (dependentRegister->is64BitReg() && dependentRegister->getKind() != TR_FPR && dependentRegister->getKind() != TR_VRF)
                  {
                  toRealRegister(assignedRealRegister)->getHighWordRegister()->setState(TR::RealRegister::Unlatched);
                  cg->traceRegFreed(dependentRegister, toRealRegister(assignedRealRegister)->getHighWordRegister());
                  }
               }
            dependentRegister->setAssignedRegister(NULL);
            dependentRegister->resetIsLive();
            if(assignedRealRegister)
              {
              assignedRealRegister->setState(TR::RealRegister::Unlatched);
              if (assignedRealRegister->getState() == TR::RealRegister::Locked)
                 assignedRealRegister->setAssignedRegister(NULL);
              cg->traceRegFreed(dependentRegister, assignedRealRegister);
              }
            }
         }
      else if (dependentRegister->getRegisterPair() != NULL)
         {
         TR::Register * dependentRegisterHigh = dependentRegister->getHighOrder();

         dependentRegisterHigh->decFutureUseCount();
         dependentRegisterHigh->setIsLive();
         TR::Register * highAssignedRegister = dependentRegisterHigh->getAssignedRegister();

         if (highAssignedRegister &&
             highAssignedRegister->getRealRegister()->getState() != TR::RealRegister::Locked)
            {
            TR_ASSERT(dependentRegisterHigh->getFutureUseCount() >=0,
                    "\nReg dep assignment: futureUseCount should not be negative\n");
            }
         if (highAssignedRegister &&
             dependentRegisterHigh->getFutureUseCount() == 0)
            {
            TR::RealRegister * assignedRegister = highAssignedRegister->getRealRegister();
            dependentRegisterHigh->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
            if (assignedRegister->getState() == TR::RealRegister::Locked)
               assignedRegister->setAssignedRegister(NULL);

            cg->traceRegFreed(dependentRegisterHigh, assignedRegister);
            // check if need to free HW
            if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
               {
               if (dependentRegisterHigh->is64BitReg() && dependentRegisterHigh->getKind() != TR_FPR &&
                     dependentRegisterHigh->getKind() != TR_VRF)
                  {
                  toRealRegister(assignedRegister)->getHighWordRegister()->setState(TR::RealRegister::Unlatched);
                  cg->traceRegFreed(dependentRegisterHigh, toRealRegister(assignedRegister)->getHighWordRegister());
                  }
               }
            }

         TR::Register * dependentRegisterLow = dependentRegister->getLowOrder();

         dependentRegisterLow->decFutureUseCount();
         dependentRegisterLow->setIsLive();

         TR::Register * lowAssignedRegister = dependentRegisterLow->getAssignedRegister();


         if (lowAssignedRegister &&
             lowAssignedRegister->getRealRegister()->getState() != TR::RealRegister::Locked)
            {
            TR_ASSERT(dependentRegisterLow->getFutureUseCount() >=0,
                    "\nReg dep assignment: futureUseCount should not be negative\n");
            }
         if (lowAssignedRegister &&
             dependentRegisterLow->getFutureUseCount() == 0)
            {
            TR::RealRegister * assignedRegister = lowAssignedRegister->getRealRegister();
            dependentRegisterLow->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
            if (assignedRegister->getState() == TR::RealRegister::Locked)
               assignedRegister->setAssignedRegister(NULL);
            cg->traceRegFreed(dependentRegisterLow, assignedRegister);
            // check if need to free HW
            if (cg->supportsHighWordFacility() && !comp->getOption(TR_DisableHighWordRA))
               {
               if (dependentRegisterLow->is64BitReg() && dependentRegisterLow->getKind() != TR_FPR &&
                     dependentRegisterLow->getKind() != TR_VRF)
                  {
                  toRealRegister(assignedRegister)->getHighWordRegister()->setState(TR::RealRegister::Unlatched);
                  cg->traceRegFreed(dependentRegisterLow, toRealRegister(assignedRegister)->getHighWordRegister());
                  }
               }
            }
         }
      }


   // OOL slow path HPR spills
   if (!comp->getOption(TR_DisableOOL))
      {
      for (i = 0; i< numOfDependencies; i++)
         {
         // we put {real HPR:SpilledReg} as deps for OOL HPR spills
         if (_dependencies[i].getRegister()->getRealRegister())
            {
            TR::RealRegister * highWordReg = toRealRegister(_dependencies[i].getRegister());
            dependentRegNum = _dependencies[i].getRealRegister();
            if (dependentRegNum == TR::RealRegister::SpilledReg)
               {
               virtReg = highWordReg->getAssignedRegister();
               //printf ("\nOOL HPR Spill in %s", comp->signature());fflush(stdout);
               traceMsg(comp,"\nOOL HPR Spill: %s", cg->getDebug()->getName(highWordReg));
               traceMsg(comp,":%s\n", cg->getDebug()->getName(virtReg));
               TR_ASSERT(virtReg, "\nOOL HPR spill: spilled HPR should have a virt Reg assigned!");
               TR_ASSERT(highWordReg->isHighWordRegister(), "\nOOL HPR spill: spilled HPR should be a real HPR!");
               virtReg->setAssignedRegister(NULL);
               virtReg->setSpilledToHPR(true);
               }
            }
         }
      }

   }

/**
 * Register association
 */
void
OMR::Z::RegisterDependencyConditions::createRegisterAssociationDirective(TR::Instruction * instruction, TR::CodeGenerator * cg)
   {
   TR::Machine *machine = cg->machine();

   machine->createRegisterAssociationDirective(instruction->getPrev());

   // Now set up the new register associations as required by the current
   // dependent register instruction onto the machine.
   // Only the registers that this instruction interferes with are modified.
   //
   TR_S390RegisterDependencyGroup * depGroup = getPreConditions();
   for (int32_t j = 0; j < getNumPreConditions(); j++)
      {
      TR::RegisterDependency * dependency = depGroup->getRegisterDependency(j);
      if (dependency->getRegister())
         {
         machine->setVirtualAssociatedWithReal(dependency->getRealRegister(), dependency->getRegister());
         }
      }

   depGroup = getPostConditions();
   for (int32_t k = 0; k < getNumPostConditions(); k++)
      {
      TR::RegisterDependency * dependency = depGroup->getRegisterDependency(k);
      if (dependency->getRegister())
         {
         machine->setVirtualAssociatedWithReal(dependency->getRealRegister(), dependency->getRegister());
         }
      }
   }

bool OMR::Z::RegisterDependencyConditions::doesConditionExist( TR_S390RegisterDependencyGroup * regDepArr, TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, uint32_t numberOfRegisters, bool overwriteAssignAny )
   {
   for( int32_t i = 0; i < numberOfRegisters; i++ )
      {
      TR::RegisterDependency * regDep = regDepArr->getRegisterDependency(i);

      if( ( regDep->getRegister() == vr ) && ( regDep->getFlags() == flag ) )
         {
         // If all properties of these two dependencies are the same, then it exists
         // already in the dependency group.
         if( regDep->getRealRegister() == rr )
            {
            return true;
            }

         // Post-condition number: Virtual register pointer, RegNum number, condition flags
         // 0: 4AE60068, 71(AssignAny), 00000003
         // 1: 4AE5F04C, 71(AssignAny), 00000003
         // 2: 4AE60158, 15(GPR14), 00000003
         //
         // merged with:
         //
         // 0: 4AE60068, 1(GPR0), 00000003
         //
         // Produces an assertion. The fix for this is to overwrite the AssignAny dependency.
         //
         else if( regDep->getRealRegister() == TR::RealRegister::AssignAny )
            {
            // AssignAny exists already, and the argument real register is not AssignAny
            // - overwrite the existing real register with the argument real register
            regDep->setRealRegister( rr );
            return true;
            }
         else if( rr == TR::RealRegister::AssignAny )
            {
            // The existing register dependency is not AssignAny but the argument real register is
            // - simply return true
            return true;
            }
         }
      }

   return false;
   }

bool OMR::Z::RegisterDependencyConditions::doesPreConditionExist( TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, bool overwriteAssignAny )
   {
   return doesConditionExist( _preConditions, vr, rr, flag, _addCursorForPre, overwriteAssignAny );
   }

bool OMR::Z::RegisterDependencyConditions::doesPostConditionExist( TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, bool overwriteAssignAny )
   {
   return doesConditionExist( _postConditions, vr, rr, flag, _addCursorForPost, overwriteAssignAny );
   }


bool OMR::Z::RegisterDependencyConditions::addPreConditionIfNotAlreadyInserted(TR::Register *vr,
                                                                                  TR::RealRegister::RegNum rr,
                                                                                  uint8_t flag)
   {
   return addPreConditionIfNotAlreadyInserted(vr, static_cast<TR::RealRegister::RegDep>(rr), flag);
   }

/**
 * Checks for an existing pre-condition for given virtual register.  If found,
 * the flags are updated.  If not found, a new pre-condition is created with
 * the given virtual register and associated real register / property.
 *
 * @param vr    The virtual register of the pre-condition.
 * @param rr    The real register or real register property of the pre-condition.
 * @param flags Flags to be updated with associated pre-condition.
 * @sa addPostConditionIfNotAlreadyInserted()
 */
bool OMR::Z::RegisterDependencyConditions::addPreConditionIfNotAlreadyInserted(TR::Register *vr,
                                                                                  TR::RealRegister::RegDep rr,
                                                                                  uint8_t flag)
   {
   int32_t pos = -1;
   if (_preConditions != NULL && (pos = _preConditions->searchForRegisterPos(vr, RefsAndDefsDependentRegister, _addCursorForPre, _cg)) < 0)
      {
      if (!(vr->getRealRegister() && vr->getRealRegister()->getState() == TR::RealRegister::Locked))
         addPreCondition(vr, rr, flag);
      return true;
      }
   else if (pos >= 0 && _preConditions->getRegisterDependency(pos)->getFlags() != flag)
      {
      _preConditions->getRegisterDependency(pos)->setFlags(flag);
      return false;
      }
   return false;
   }

bool OMR::Z::RegisterDependencyConditions::addPostConditionIfNotAlreadyInserted(TR::Register *vr,
                                                                                   TR::RealRegister::RegNum rr,
                                                                                   uint8_t flag)
   {
   return addPostConditionIfNotAlreadyInserted(vr, static_cast<TR::RealRegister::RegDep>(rr), flag);
   }

/**
 * Checks for an existing post-condition for given virtual register.  If found,
 * the flags are updated.  If not found, a new post condition is created with
 * the given virtual register and associated real register / property.
 *
 * @param vr    The virtual register of the post-condition.
 * @param rr    The real register or real register property of the post-condition.
 * @param flags Flags to be updated with associated post-condition.
 * @sa addPreConditionIfNotAlreadyInserted()
 */
bool OMR::Z::RegisterDependencyConditions::addPostConditionIfNotAlreadyInserted(TR::Register *vr,
                                                                                   TR::RealRegister::RegDep rr,
                                                                                   uint8_t flag)
   {
   int32_t pos = -1;
   if (_postConditions != NULL && (pos = _postConditions->searchForRegisterPos(vr, RefsAndDefsDependentRegister, _addCursorForPost, _cg)) < 0)
      {
      if (!(vr->getRealRegister() && vr->getRealRegister()->getState() == TR::RealRegister::Locked))
         addPostCondition(vr, rr, flag);
      return true;
      }
   else if (pos >= 0 && _postConditions->getRegisterDependency(pos)->getFlags() != flag)
      {
      _postConditions->getRegisterDependency(pos)->setFlags(flag);
      if((flag & DefinesDependentRegister) != 0)
        {
        bool redefined=(vr->getStartOfRange() != NULL);
        vr->setRedefined(redefined);
        }
      return false;
      }
   return false;
   }
