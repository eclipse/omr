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

#include <stddef.h>                              // for NULL
#include <stdint.h>                              // for int32_t, uint8_t, etc
#include <algorithm>                             // for std::find
#include "codegen/BackingStore.hpp"              // for TR_BackingStore
#include "codegen/CodeGenPhase.hpp"              // for CodeGenPhase, etc
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator
#include "codegen/InstOpCode.hpp"                // for InstOpCode, etc
#include "codegen/Instruction.hpp"               // for Instruction
#include "codegen/Linkage.hpp"
#include "codegen/LiveRegister.hpp"              // for TR_LiveRegisters
#include "codegen/Machine.hpp"                   // for Machine
#include "codegen/MemoryReference.hpp"           // for MemoryReference
#include "codegen/RealRegister.hpp"              // for RealRegister, etc
#include "codegen/Register.hpp"                  // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"  // for RegisterDependency
#include "codegen/RegisterPair.hpp"              // for RegisterPair
#include "codegen/TreeEvaluator.hpp"             // for getHighGlobalRegisterNumberIfAny()
#include "compile/Compilation.hpp"               // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/ObjectModel.hpp"                   // for ObjectModel
#include "env/TRMemory.hpp"
#include "il/Node.hpp"                           // for Node
#include "il/Node_inlines.hpp"                   // for Node::getChild
#include "il/symbol/LabelSymbol.hpp"             // for LabelSymbol
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/List.hpp"                        // for List
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"           // for Op_load
#include "ras/Debug.hpp"                         // for TR_DebugBase

namespace TR { class SymbolReference; }

TR::RegisterDependencyConditions* TR_PPCScratchRegisterDependencyConditions::createDependencyConditions(TR::CodeGenerator *cg,
                                                                                                          TR_PPCScratchRegisterDependencyConditions *pre,
                                                                                                          TR_PPCScratchRegisterDependencyConditions *post)
   {
   int32_t preCount = pre ? pre->getNumberOfDependencies() : 0;
   int32_t postCount = post ? post->getNumberOfDependencies() : 0;
   TR_LiveRegisters *lrVector = cg->getLiveRegisters(TR_VSX_VECTOR);
   bool liveVSXVectorReg = (!lrVector || (lrVector->getNumberOfLiveRegisters() > 0));
   TR_LiveRegisters *lrScalar = cg->getLiveRegisters(TR_VSX_SCALAR);
   bool liveVSXScalarReg = (!lrScalar || (lrScalar->getNumberOfLiveRegisters() > 0));
   if (liveVSXVectorReg)
      {
      preCount += 64;
      postCount += 64;
      }
   else if (liveVSXScalarReg)
      {
      preCount += 32;
      postCount += 32;
      }

   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(preCount,
                                                                                                                  postCount,
                                                                                                                  cg->trMemory());
   for (int i = 0; i < (pre ? pre->_numGPRDeps : 0); ++i)
      {
      dependencies->addPreCondition(pre->_gprDeps[i].getRegister(), pre->_gprDeps[i].getRealRegister(), pre->_gprDeps[i].getFlags());
      if (pre->_excludeGPR0 & (1 << i))
         dependencies->getPreConditions()->getRegisterDependency(i)->setExcludeGPR0();
      }
   for (int i = 0; i < (post ? post->_numGPRDeps : 0); ++i)
      {
      dependencies->addPostCondition(post->_gprDeps[i].getRegister(), post->_gprDeps[i].getRealRegister(), post->_gprDeps[i].getFlags());
      if (post->_excludeGPR0 & (1 << i))
         dependencies->getPostConditions()->getRegisterDependency(i)->setExcludeGPR0();
      }
   for (int i = 0; i < (pre ? pre->_numCCRDeps : 0); ++i)
      {
      dependencies->addPreCondition(pre->_ccrDeps[i].getRegister(), pre->_ccrDeps[i].getRealRegister(), pre->_ccrDeps[i].getFlags());
      }
   for (int i = 0; i < (post ? post->_numCCRDeps : 0); ++i)
      {
      dependencies->addPostCondition(post->_ccrDeps[i].getRegister(), post->_ccrDeps[i].getRealRegister(), post->_ccrDeps[i].getFlags());
      }

   const TR::PPCLinkageProperties& properties = cg->getLinkage()->getProperties();
   if (liveVSXVectorReg)
      {
      for (int32_t i=TR::RealRegister::FirstVSR; i<=TR::RealRegister::LastVSR; i++)
         {
         if (!properties.getPreserved((TR::RealRegister::RegNum)i))
            {
            TR::Register *vreg = cg->allocateRegister(TR_FPR);
            vreg->setPlaceholderReg();
            dependencies->addPreCondition(vreg, (TR::RealRegister::RegNum)i);
            dependencies->addPostCondition(vreg, (TR::RealRegister::RegNum)i);
            }
         }
      }
   else
      {
      if (liveVSXScalarReg)
         {
         for (int32_t i=TR::RealRegister::vsr32; i<=TR::RealRegister::LastVSR; i++)
            {
            if (!properties.getPreserved((TR::RealRegister::RegNum)i))
               {
               TR::Register *vreg = cg->allocateRegister(TR_FPR);
               vreg->setPlaceholderReg();
               dependencies->addPreCondition(vreg, (TR::RealRegister::RegNum)i);
               dependencies->addPostCondition(vreg, (TR::RealRegister::RegNum)i);
               }
            }
         }
      }

   return dependencies;
   }


OMR::Power::RegisterDependencyConditions::RegisterDependencyConditions(
                                       TR::CodeGenerator *cg,
                                       TR::Node          *node,
                                       uint32_t          extranum,
                                       TR::Instruction  **cursorPtr)
   {
   List<TR::Register>  regList(cg->trMemory());
   TR::Instruction    *iCursor = (cursorPtr==NULL)?NULL:*cursorPtr;
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
	 if (TR::TreeEvaluator::getHighGlobalRegisterNumberIfAny(child, cg) != -1)
	    numLongs++;
	 }
      }

   totalNum = totalNum + numLongs;

   _preConditions = new (totalNum, cg->trMemory()) TR_PPCRegisterDependencyGroup;
   _postConditions = new (totalNum, cg->trMemory()) TR_PPCRegisterDependencyGroup;
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
      TR_GlobalRegisterNumber validHighRegNum = TR::TreeEvaluator::getHighGlobalRegisterNumberIfAny(child, cg); 

      if (validHighRegNum != -1)
         {

         highRegNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(validHighRegNum);
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
      TR_GlobalRegisterNumber validHighRegNum = TR::TreeEvaluator::getHighGlobalRegisterNumberIfAny(child, cg); 

      if (validHighRegNum != -1)
         {
         highRegNum = (TR::RealRegister::RegNum)cg->getGlobalRegister(validHighRegNum);
         TR::RegisterPair *regPair = reg->getRegisterPair();
         TR_ASSERT(regPair, "assertion failure");
	 highReg = regPair->getHighOrder();
	 reg = regPair->getLowOrder();

         if (highReg->getAssociation() == highRegNum &&
             reg->getAssociation() == regNum)
            continue;

         }
      else  if (reg->getAssociation() == regNum)
             continue;

      if (regList.find(reg) || (highReg && regList.find(highReg)))
	 {
         TR::InstOpCode::Mnemonic    opCode;
         TR_RegisterKinds kind = reg->getKind();

         switch (kind)
	    {
            case TR_GPR:
               opCode = TR::InstOpCode::mr;
               break;
            case TR_FPR:
               opCode = TR::InstOpCode::fmr;
               break;
            case TR_VRF:
               opCode = TR::InstOpCode::vor;
               //TR_ASSERT(0, "VMX not fully supported.");
               break;
            case TR_VSX_VECTOR:
               opCode = TR::InstOpCode::xxlor;
               break;
            case TR_CCR:
               opCode = TR::InstOpCode::mcrf;
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
            if (opCode == TR::InstOpCode::vor || opCode == TR::InstOpCode::xxlor)
               iCursor = generateTrg1Src2Instruction(cg, opCode, node, copyReg, reg, reg, iCursor);
            else
               iCursor = generateTrg1Src1Instruction(cg, opCode, node, copyReg, reg, iCursor);

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
            if (opCode == TR::InstOpCode::vor || opCode == TR::InstOpCode::xxlor)
               iCursor = generateTrg1Src2Instruction(cg, opCode, node, highCopyReg, highReg, highReg, iCursor);
            else
	       iCursor = generateTrg1Src1Instruction(cg, opCode, node, highCopyReg, highReg, iCursor);

	    highReg = highCopyReg;
	    }
	 }

      addPreCondition(reg, regNum);
      addPostCondition(reg, regNum);
      if (copyReg != NULL)
         cg->stopUsingRegister(copyReg);
      else
         regList.add(reg);

      if (highReg)
	 {
	 addPreCondition(highReg, highRegNum);
	 addPostCondition(highReg, highRegNum);
	 if (highCopyReg != NULL)
	    cg->stopUsingRegister(highCopyReg);
         else
            regList.add(highReg);
	 }
      }

   if (iCursor!=NULL && cursorPtr!=NULL)
      *cursorPtr = iCursor;
   }

void OMR::Power::RegisterDependencyConditions::unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg)
   {
   addPostCondition(reg, TR::RealRegister::NoReg);
   }

bool OMR::Power::RegisterDependencyConditions::refsRegister(TR::Register *r)
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

bool OMR::Power::RegisterDependencyConditions::defsRegister(TR::Register *r)
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

bool OMR::Power::RegisterDependencyConditions::defsRealRegister(TR::Register *r)
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




bool OMR::Power::RegisterDependencyConditions::usesRegister(TR::Register *r)
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

void OMR::Power::RegisterDependencyConditions::bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg)
   {
   TR::Register *virtReg;
   TR::RealRegister::RegNum regNum;
   TR::RegisterDependencyConditions *assoc;
   int numAssoc = 0;

   if (instr->getOpCodeValue() == TR::InstOpCode::assocreg)
      return;

   // Don't track associations or emit assocregs in outlined code
   // Register assigner can save/restore associations across outlined sections properly, however no such mechanism exists for instruction selection
   // so we don't want these associations to clobber the associations that were set in main line code, which are more important
   // TODO: Fix this by saving/restoring the associations in swapInstructionListsWithCompilation()
   bool isOOL = cg->getIsInOOLSection();

   TR::Machine *machine = cg->machine();
   assoc = !isOOL ? new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0,_addCursorForPre, cg->trMemory()) : 0;

   for (int i = 0; i < _addCursorForPre; i++)
      {
      virtReg = _preConditions->getRegisterDependency(i)->getRegister();
      regNum  = _preConditions->getRegisterDependency(i)->getRealRegister();

      if (!isOOL)
         {
         // Add to the association condition when the association is changed
         // from one virtual register to another
         if (machine->getVirtualAssociatedWithReal(regNum) != virtReg && machine->getVirtualAssociatedWithReal(regNum) != 0)
            {
            assoc->addPostCondition(machine->getVirtualAssociatedWithReal(regNum), regNum);
            numAssoc++;
            }
         // Keep track of the virtual register map to real registers!
         machine->setVirtualAssociatedWithReal(regNum, virtReg);
         }

      instr->useRegister(virtReg);

      if (!isOOL)
         {
         cg->setRealRegisterAssociation(virtReg, regNum);
         if (_preConditions->getRegisterDependency(i)->getExcludeGPR0())
            cg->addRealRegisterInterference(virtReg, TR::RealRegister::gr0);
         }
      }

   if (numAssoc > 0)
      {
      // Emit an AssocRegs instruction to track the previous association
      assoc->setNumPostConditions(numAssoc, cg->trMemory());
      generateDepInstruction(cg, TR::InstOpCode::assocreg, instr->getNode(), assoc, instr->getPrev());
      }

   for (int j = 0; j < _addCursorForPost; j++)
      {
      virtReg = _postConditions->getRegisterDependency(j)->getRegister();
      regNum  = _postConditions->getRegisterDependency(j)->getRealRegister();

      instr->useRegister(virtReg);

      if (!isOOL)
         {
         cg->setRealRegisterAssociation(virtReg, regNum);
         if (_postConditions->getRegisterDependency(j)->getExcludeGPR0())
	    cg->addRealRegisterInterference(virtReg, TR::RealRegister::gr0);
         }
      }
   }

TR::RegisterDependencyConditions *
OMR::Power::RegisterDependencyConditions::clone(
   TR::CodeGenerator *cg,
   TR::RegisterDependencyConditions *added)
   {
   TR::RegisterDependencyConditions *result;
   TR::RegisterDependency           *singlePair;
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
   result = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(getNumPreConditions()+addPre,
                   getNumPostConditions()+addPost, cg->trMemory());

   for (idx=0; idx<postNum; idx++)
      {
      singlePair = this->getPostConditions()->getRegisterDependency(idx);
      //eliminate duplicate virtual->NoReg dependencies in 'this' conditions
      if (!((TR::RealRegister::NoReg == singlePair->getRealRegister()) && (added->postConditionContainsVirtual(singlePair->getRegister()))))
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
      //eliminate duplicate virtual->NoReg dependencies in 'this' conditions
      if (!((TR::RealRegister::NoReg == singlePair->getRealRegister()) && (added->preConditionContainsVirtual(singlePair->getRegister()))))
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
   TR_ASSERT(preGPR<=max_gpr && postGPR<=max_gpr, "Over the limit of available global regsiters.");
#endif
   return result;
   }

TR::RegisterDependencyConditions *OMR::Power::RegisterDependencyConditions::cloneAndFix(
   TR::CodeGenerator * cg, TR::RegisterDependencyConditions *added)
   {
   TR::RegisterDependencyConditions *result;
   TR::RegisterDependency           *singlePair;
   int32_t      idx, postNum, addPost=0;

   if (added != NULL)
      {
      addPost = added->getAddCursorForPost();
      }
   postNum = this->getAddCursorForPost();
   result = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, postNum+addPost, cg->trMemory());

   for (idx=0; idx<postNum; idx++)
      {
      singlePair = this->getPostConditions()->getRegisterDependency(idx);
      result->addPostCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                               singlePair->getFlags());
      }

   for (idx=0; idx<addPost; idx++)
      {
      singlePair = added->getPostConditions()->getRegisterDependency(idx);
      result->addPostCondition(singlePair->getRegister(), singlePair->getRealRegister(),
                               singlePair->getFlags());
      }

   return result;
   }

static TR::RegisterDependency*
findDependencyChainHead(TR::RegisterDependency *dep,
                        TR_PPCRegisterDependencyMap& map)
   {
   TR::RegisterDependency *cursor = map.getDependencyWithAssigned(dep->getRealRegister());


   // Already at head of chain
   if (!cursor)
      return dep;

   // Follow the chain until we get to a dep who's target isn't assigned to another dep
   // or until we're back at the first dep
   while (cursor != dep)
      {
      TR::RegisterDependency *nextDep = map.getDependencyWithAssigned(cursor->getRealRegister());
      if (!nextDep)
         break;
      cursor = nextDep;
      }

   return cursor;
   }

static void assignFreeRegisters(TR::Instruction              *currentInstruction,
                                TR::RegisterDependency       *dep,
                                TR_PPCRegisterDependencyMap&  map,
                                TR::CodeGenerator            *cg)
   {
   TR::Machine *machine = cg->machine();

   // Assign a chain of dependencies where the head of the chain depends on a free reg
   while (dep)
      {
      TR_ASSERT(machine->getPPCRealRegister(dep->getRealRegister())->getState() == TR::RealRegister::Free, "Expecting free target register");
      TR::RealRegister *assignedReg = dep->getRegister()->getAssignedRealRegister() ?
         toRealRegister(dep->getRegister()->getAssignedRealRegister()) : NULL;
      machine->coerceRegisterAssignment(currentInstruction, dep->getRegister(), dep->getRealRegister());
      dep->getRegister()->block();
      dep = assignedReg ?
         map.getDependencyWithTarget(assignedReg->getRegisterNumber()) : NULL;
      }
   }

static void assignContendedRegisters(TR::Instruction              *currentInstruction,
                                     TR::RegisterDependency       *dep,
                                     TR_PPCRegisterDependencyMap&  map,
                                     bool                          depsBlocked,
                                     TR::CodeGenerator            *cg)
   {
   TR::Machine *machine = cg->machine();

   dep = findDependencyChainHead(dep, map);

   TR::Register *virtReg = dep->getRegister();
   TR::RealRegister::RegNum targetRegNum = dep->getRealRegister();
   TR::RealRegister *targetReg = machine->getPPCRealRegister(targetRegNum);
   TR::RealRegister *assignedReg = virtReg->getAssignedRealRegister() ?
      toRealRegister(virtReg->getAssignedRealRegister()) :  NULL;

   // Chain of length 1
   if (!assignedReg || !map.getDependencyWithTarget(assignedReg->getRegisterNumber()))
      {
      machine->coerceRegisterAssignment(currentInstruction, virtReg, targetRegNum);
      virtReg->block();
      return;
      }

   // Chain of length 2, handled here instead of below to get 3*xor exchange on GPRs
   if (map.getDependencyWithTarget(assignedReg->getRegisterNumber()) == map.getDependencyWithAssigned(targetRegNum))
      {
      TR::Register *targetVirtReg = targetReg->getAssignedRegister();
      machine->coerceRegisterAssignment(currentInstruction, virtReg, targetRegNum);
      virtReg->block();
      targetVirtReg->block();
      return;
      }

   // Grab a spare reg in order to free the target of the first dep
   // At this point the first dep's target could be blocked, assigned, or NoReg
   // If it's blocked or assigned we allocate a spare and assign the target's virtual to it
   // If it's NoReg, the spare reg will be used as the first dep's actual target
   TR::RealRegister *spareReg = machine->findBestFreeRegister(currentInstruction, virtReg->getKind(),
                                                                targetRegNum == TR::RealRegister::NoReg ? dep->getExcludeGPR0() : false, false,
                                                                targetRegNum == TR::RealRegister::NoReg ? virtReg : targetReg->getAssignedRegister());
   bool haveFreeSpare = spareReg != NULL;
   if (!spareReg)
      {
      // If the regs in this dep group are not blocked we need to make sure we don't spill a reg that's in the middle of the chain
      if (!depsBlocked)
         {
         if (targetRegNum == TR::RealRegister::NoReg)
            spareReg = machine->freeBestRegister(currentInstruction,
                                                 map.getDependencyWithTarget(assignedReg->getRegisterNumber())->getRegister(),
                                                 assignedReg, false);
         else
            spareReg = machine->freeBestRegister(currentInstruction, virtReg, targetReg, false);
         }
      else
         {
         if (targetRegNum == TR::RealRegister::NoReg)
            spareReg = machine->freeBestRegister(currentInstruction, virtReg, NULL, dep->getExcludeGPR0());
         else
            spareReg = machine->freeBestRegister(currentInstruction, targetReg->getAssignedRegister(), NULL, false);
         }
      }

   if (targetRegNum != TR::RealRegister::NoReg && spareReg != targetReg)
      {
      machine->coerceRegisterAssignment(currentInstruction, targetReg->getAssignedRegister(), spareReg->getRegisterNumber());
      }

   TR_ASSERT(targetRegNum == TR::RealRegister::NoReg ||
          targetReg->getState() == TR::RealRegister::Free, "Expecting free target register");

   if (depsBlocked || targetRegNum != TR::RealRegister::NoReg || haveFreeSpare)
      {

      machine->coerceRegisterAssignment(currentInstruction, virtReg,
                                        targetRegNum == TR::RealRegister::NoReg ?
                                        spareReg->getRegisterNumber() : targetRegNum);
      virtReg->block();
      }

   dep = map.getDependencyWithTarget(assignedReg->getRegisterNumber());
   while (dep)
      {
      virtReg = dep->getRegister();
      targetRegNum = dep->getRealRegister();
      targetReg = machine->getPPCRealRegister(targetRegNum);
      assignedReg = virtReg->getAssignedRealRegister() ?
         toRealRegister(virtReg->getAssignedRealRegister()) : NULL;

      TR_ASSERT(targetReg->getState() == TR::RealRegister::Free || targetReg == spareReg,
             "Expecting free target register or target to have been filled to free spare register");

      machine->coerceRegisterAssignment(currentInstruction, virtReg, targetRegNum);
      virtReg->block();
      dep = assignedReg ?
         map.getDependencyWithTarget(assignedReg->getRegisterNumber()) : NULL;
      }

   }

void TR_PPCRegisterDependencyGroup::assignRegisters(TR::Instruction   *currentInstruction,
                                                    TR_RegisterKinds   kindToBeAssigned,
                                                    uint32_t           numberOfRegisters,
                                                    TR::CodeGenerator *cg)
   {
   TR::Machine              *machine = cg->machine();
   TR::Register             *virtReg;
   TR::RealRegister::RegNum  dependentRegNum;
   TR::RealRegister         *dependentRealReg, *assignedRegister, *realReg;
   int                       i, j;
   TR::Compilation          *comp = cg->comp();

   int numGPRs = 0;
   int numFPRs = 0;
   int numVRFs = 0;
   bool haveSpilledCCRs = false;

   // Use to do lookups using real register numbers
   TR_PPCRegisterDependencyMap map(_dependencies, numberOfRegisters);

   if (!comp->getOption(TR_DisableOOL))
      {
      for (i = 0; i < numberOfRegisters; i++)
         {
         virtReg         = _dependencies[i].getRegister();
         dependentRegNum = _dependencies[i].getRealRegister();
         if (dependentRegNum == TR::RealRegister::SpilledReg)
            {
            TR_ASSERT(virtReg->getBackingStorage(), "Should have a backing store if dependentRegNum == SpilledReg");
            if (virtReg->getAssignedRealRegister())
               {
               // this happens when the register was first spilled in main line path then was reverse spilled
               // and assigned to a real register in OOL path. We protected the backing store when doing
               // the reverse spill so we could re-spill to the same slot now
               traceMsg(comp,"\nOOL: Found register spilled in main line and re-assigned inside OOL");
               TR::Node            *currentNode = currentInstruction->getNode();
               TR::RealRegister    *assignedReg = toRealRegister(virtReg->getAssignedRegister());
               TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(currentNode, (TR::SymbolReference*)virtReg->getBackingStorage()->getSymbolReference(), sizeof(uintptr_t), cg);
               TR_RegisterKinds     rk = virtReg->getKind();

               TR::InstOpCode::Mnemonic opCode;
               switch (rk)
                  {
                  case TR_GPR:
                     opCode =TR::InstOpCode::Op_load;
                     break;
                  case TR_FPR:
                     opCode = virtReg->isSinglePrecision() ? TR::InstOpCode::lfs : TR::InstOpCode::lfd;
                     break;
                  default:
                     TR_ASSERT(0, "Register kind not supported in OOL spill");
                     break;
                  }

               TR::Instruction *inst = generateTrg1MemInstruction(cg, opCode, currentNode, assignedReg, tempMR, currentInstruction);

               assignedReg->setAssignedRegister(NULL);
               virtReg->setAssignedRegister(NULL);
               assignedReg->setState(TR::RealRegister::Free);
               if (comp->getDebug())
                  cg->traceRegisterAssignment("Generate reload of virt %s due to spillRegIndex dep at inst %p\n", comp->getDebug()->getName(virtReg),currentInstruction);
               cg->traceRAInstruction(inst);
               }

            if (!(std::find(cg->getSpilledRegisterList()->begin(), cg->getSpilledRegisterList()->end(), virtReg) != cg->getSpilledRegisterList()->end()))
               cg->getSpilledRegisterList()->push_front(virtReg);
            }
         }
      }

   for (i = 0; i < numberOfRegisters; i++)
      {
      map.addDependency(_dependencies[i], i);

      dependentRegNum = _dependencies[i].getRealRegister();

      if (dependentRegNum != TR::RealRegister::SpilledReg)
         {
         virtReg = _dependencies[i].getRegister();
         switch (virtReg->getKind())
            {
            case TR_GPR:
               ++numGPRs;
               break;
            case TR_FPR:
               ++numFPRs;
               break;
            case TR_VRF:
               ++numVRFs;
               break;
            case TR_CCR:
               if (virtReg->getAssignedRegister() == NULL &&
                   virtReg->getBackingStorage() == NULL)
                  haveSpilledCCRs = true;
               break;
            default:
               break;
            }
         }
      }

#ifdef DEBUG
   int lockedGPRs = 0;
   int lockedFPRs = 0;
   int lockedVRFs = 0;

   // count up how many registers are locked for each type
   for(i = TR::RealRegister::FirstGPR; i <= TR::RealRegister::LastGPR; i++)
      {
        realReg = machine->getPPCRealRegister((TR::RealRegister::RegNum)i);
        if (realReg->getState() == TR::RealRegister::Locked)
           lockedGPRs++;
      }
   for(i = TR::RealRegister::FirstFPR; i <= TR::RealRegister::LastFPR; i++)
      {
        realReg = machine->getPPCRealRegister((TR::RealRegister::RegNum)i);
        if (realReg->getState() == TR::RealRegister::Locked)
           lockedFPRs++;
      }
   for(i = TR::RealRegister::FirstVRF; i <= TR::RealRegister::LastVRF; i++)
      {
        realReg = machine->getPPCRealRegister((TR::RealRegister::RegNum)i);
        if (realReg->getState() == TR::RealRegister::Locked)
           lockedVRFs++;
      }
   TR_ASSERT(lockedGPRs == machine->getNumberOfLockedRegisters(TR_GPR), "Inconsistent number of locked GPRs");
   TR_ASSERT(lockedFPRs == machine->getNumberOfLockedRegisters(TR_FPR), "Inconsistent number of locked FPRs");
   TR_ASSERT(lockedVRFs == machine->getNumberOfLockedRegisters(TR_VRF), "Inconsistent number of locked VRFs");
#endif

   // To handle circular dependencies, we block a real register if (1) it is already assigned to a correct
   // virtual register and (2) if it is assigned to one register in the list but is required by another.
   // However, if all available registers are requested, we do not block in case (2) to avoid all registers
   // being blocked.

   bool haveSpareGPRs = true;
   bool haveSpareFPRs = true;
   bool haveSpareVRFs = true;

   TR_ASSERT(numGPRs <= (TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1 - machine->getNumberOfLockedRegisters(TR_GPR)), "Too many GPR dependencies, unable to assign" );
   TR_ASSERT(numFPRs <= (TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1 - machine->getNumberOfLockedRegisters(TR_FPR)), "Too many FPR dependencies, unable to assign" );
   TR_ASSERT(numVRFs <= (TR::RealRegister::LastVRF - TR::RealRegister::FirstVRF + 1 - machine->getNumberOfLockedRegisters(TR_VRF)), "Too many VRF dependencies, unable to assign" );

   if (numGPRs == (TR::RealRegister::LastGPR - TR::RealRegister::FirstGPR + 1 - machine->getNumberOfLockedRegisters(TR_GPR)))
      haveSpareGPRs = false;
   if (numFPRs == (TR::RealRegister::LastFPR - TR::RealRegister::FirstFPR + 1 - machine->getNumberOfLockedRegisters(TR_FPR)))
      haveSpareFPRs = false;
   if (numVRFs == (TR::RealRegister::LastVRF - TR::RealRegister::FirstVRF + 1 - machine->getNumberOfLockedRegisters(TR_VRF)))
      haveSpareVRFs = false;

   for (i = 0; i < numberOfRegisters; i++)
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
            TR::RealRegister::RegNum assignedRegNum;
            assignedRegNum = toRealRegister(virtReg->getAssignedRealRegister())->getRegisterNumber();

            // always block if required register and assigned register match;
            // block if assigned register is required by other dependency but only if
            // any spare registers are left to avoid blocking all existing registers
            if (_dependencies[i].getRealRegister() == assignedRegNum ||
                (map.getDependencyWithTarget(assignedRegNum) &&
                 ((virtReg->getKind() != TR_GPR || haveSpareGPRs) &&
                  (virtReg->getKind() != TR_FPR || haveSpareFPRs) &&
                  (virtReg->getKind() != TR_VRF || haveSpareVRFs))))
               {
               virtReg->block();
               }
            }
         }
      }

   // If spilled CCRs are present in the dependencies and we are not blocking GPRs
   // (because we may not have any spare ones) we will assign the CCRs first.
   // This is because in order to assign a spilled virtual to a physical CCR
   // we need to reverse spill it to a GPR first and then assign it to the
   // CCR, so we do that first to avoid having all the GPRs become assigned and blocked.
   // Note: It is safe to assign CCRs before GPRs in any circumstance, but the extra
   // checks are cheap and let us avoid an extra loop through the dependencies for
   // what should be a relatively infrequent situation anyway.
   if (haveSpilledCCRs && !haveSpareGPRs)
      {
      for (i = 0; i < numberOfRegisters; i++)
         {
         virtReg = _dependencies[i].getRegister();
         if (virtReg->getKind() != TR_CCR)
            continue;
         dependentRegNum = _dependencies[i].getRealRegister();
         dependentRealReg = machine->getPPCRealRegister(dependentRegNum);

         if (dependentRegNum != TR::RealRegister::NoReg &&
             dependentRegNum != TR::RealRegister::SpilledReg &&
             dependentRealReg->getState() == TR::RealRegister::Free)
            {
            assignFreeRegisters(currentInstruction, &_dependencies[i], map, cg);
            }
         }
      }

   // Assign all virtual regs that depend on a specific real reg that is free
   for (i = 0; i < numberOfRegisters; i++)
      {
      virtReg = _dependencies[i].getRegister();
      dependentRegNum = _dependencies[i].getRealRegister();
      dependentRealReg = machine->getPPCRealRegister(dependentRegNum);

      if (dependentRegNum != TR::RealRegister::NoReg &&
          dependentRegNum != TR::RealRegister::SpilledReg &&
          dependentRealReg->getState() == TR::RealRegister::Free)
         {
         assignFreeRegisters(currentInstruction, &_dependencies[i], map, cg);
         }
      }

   // Assign all virtual regs that depend on a specfic real reg that is not free
   for (i = 0; i < numberOfRegisters; i++)
      {
      virtReg          = _dependencies[i].getRegister();
      assignedRegister = NULL;
      if (virtReg->getAssignedRealRegister() != NULL)
         {
         assignedRegister = toRealRegister(virtReg->getAssignedRealRegister());
         }
      dependentRegNum  = _dependencies[i].getRealRegister();
      dependentRealReg = machine->getPPCRealRegister(dependentRegNum);
      if (dependentRegNum != TR::RealRegister::NoReg &&
          dependentRegNum != TR::RealRegister::SpilledReg &&
          dependentRealReg != assignedRegister)
         {
         bool depsBlocked = false;
         switch (_dependencies[i].getRegister()->getKind())
            {
            case TR_GPR:
               depsBlocked = haveSpareGPRs;
               break;
            case TR_FPR:
               depsBlocked = haveSpareFPRs;
               break;
            case TR_VRF:
               depsBlocked = haveSpareVRFs;
               break;
            }
         assignContendedRegisters(currentInstruction, &_dependencies[i], map, depsBlocked, cg);
         }
      }

   // Assign all virtual regs that depend on NoReg but exclude gr0
   for (i = 0; i < numberOfRegisters; i++)
      {
      if (_dependencies[i].getRealRegister() == TR::RealRegister::NoReg && _dependencies[i].getExcludeGPR0())
         {
         TR::RealRegister *realOne;

         virtReg = _dependencies[i].getRegister();
         realOne = virtReg->getAssignedRealRegister();
         if (realOne != NULL && toRealRegister(realOne)->getRegisterNumber() == TR::RealRegister::gr0)
            {
            if ((assignedRegister = machine->findBestFreeRegister(currentInstruction, virtReg->getKind(), true, false, virtReg)) == NULL)
               {
               assignedRegister = machine->freeBestRegister(currentInstruction, virtReg, NULL, true);
               }
            machine->coerceRegisterAssignment(currentInstruction, virtReg, assignedRegister->getRegisterNumber());
            }
         else if (realOne == NULL)
            {
            machine->assignOneRegister(currentInstruction, virtReg, true);
            }
         virtReg->block();
         }
      }

   // Assign all virtual regs that depend on NoReg
   for (i = 0; i < numberOfRegisters; i++)
      {
      if (_dependencies[i].getRealRegister() == TR::RealRegister::NoReg && !_dependencies[i].getExcludeGPR0())
         {
         TR::RealRegister *realOne;

         virtReg = _dependencies[i].getRegister();
         realOne = virtReg->getAssignedRealRegister();
         if (!realOne)
            {
            machine->assignOneRegister(currentInstruction, virtReg, false);
            }
         virtReg->block();
         }
      }

   unblockRegisters(numberOfRegisters);
   for (i = 0; i < numberOfRegisters; i++)
      {
      TR::Register *dependentRegister = getRegisterDependency(i)->getRegister();
      // dependentRegister->getAssignedRegister() is NULL if the reg has already been spilled due to a spilledReg dep
      if (comp->getOption(TR_DisableOOL) || (!(cg->isOutOfLineColdPath()) && !(cg->isOutOfLineHotPath())))
         {
         TR_ASSERT(dependentRegister->getAssignedRegister(), "Assigned register can not be NULL");
         }
      if (dependentRegister->getAssignedRegister())
         {
         TR::RealRegister *assignedRegister = dependentRegister->getAssignedRegister()->getRealRegister();

         if (getRegisterDependency(i)->getRealRegister() == TR::RealRegister::NoReg)
            getRegisterDependency(i)->setRealRegister(toRealRegister(assignedRegister)->getRegisterNumber());

         machine->decFutureUseCountAndUnlatch(dependentRegister);
         }
      }
   }


TR::Register *
OMR::Power::RegisterDependencyConditions::getTargetRegister(
      uint32_t index,
      TR::CodeGenerator * cg)
   {
   if (index >= _addCursorForPost)
      return NULL;

   return _postConditions->getRegisterDependency(index)->getRegister();
   }

TR::Register *
OMR::Power::RegisterDependencyConditions::getSourceRegister(uint32_t index)
   {
   if (index >= _addCursorForPre)
      return NULL;
   return _preConditions->getRegisterDependency(index)->getRegister();
   }

void
OMR::Power::RegisterDependencyConditions::stopUsingDepRegs(
      TR::CodeGenerator *cg,
      int numRetReg,
      TR::Register ** retReg)
   {
   if (_preConditions != NULL)
      _preConditions->stopUsingDepRegs(_addCursorForPre, numRetReg, retReg, cg);
   if (_postConditions != NULL)
      _postConditions->stopUsingDepRegs(_addCursorForPost, numRetReg, retReg, cg);
   }

void
OMR::Power::RegisterDependencyConditions::stopUsingDepRegs(
      TR::CodeGenerator *cg,
      TR::Register * ret1,
      TR::Register *ret2)
   {
   TR::Register* regs[2] = {ret1, ret2};
   stopUsingDepRegs(cg, 2, regs);
   }
