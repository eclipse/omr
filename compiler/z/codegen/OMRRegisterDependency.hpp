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

#ifndef OMR_Z_REGISTER_DEPENDENCY_INCL
#define OMR_Z_REGISTER_DEPENDENCY_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_CONNECTOR
namespace OMR { namespace Z { class RegisterDependencyConditions; } }
namespace OMR { typedef OMR::Z::RegisterDependencyConditions RegisterDependencyConditionsConnector; }
#else
#error OMR::Z::RegisterDependencyConditions expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterDependency.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

#define RefsAndDefsDependentRegister  (ReferencesDependentRegister | DefinesDependentRegister)

namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterPair; }
template <typename ListKind> class List;

#define RefsAndDefsDependentRegister  (ReferencesDependentRegister | DefinesDependentRegister)

#define NUM_VM_THREAD_REG_DEPS 1
#define NUM_DEFAULT_DEPENDENCIES 1

class TR_S390RegisterDependencyGroup
   {
   int8_t _numUses;
   TR::RegisterDependency _dependencies[NUM_DEFAULT_DEPENDENCIES];

   public:
   TR_ALLOC_WITHOUT_NEW(TR_Memory::RegisterDependencyGroup)

   TR_S390RegisterDependencyGroup() : _numUses(0) {}


   // use TR_S390RegisterDependencyGroup::create to allocate an object of this type

   void * operator new(size_t s, int32_t numDependencies, TR_Memory * m)
      {
      TR_ASSERT(numDependencies > 0, "operator new called with numDependencies == 0");
      if (numDependencies > NUM_DEFAULT_DEPENDENCIES)
         {
         s += (numDependencies-NUM_DEFAULT_DEPENDENCIES)*sizeof(TR::RegisterDependency);
         }
      return m->allocateHeapMemory(s, TR_MemoryBase::RegisterDependencyGroup);
      }

   static TR_S390RegisterDependencyGroup * create(int32_t numDependencies, TR_Memory * m)
      {
      return numDependencies ? new (numDependencies, m) TR_S390RegisterDependencyGroup : 0;
      }
   TR::RegisterDependency *getRegisterDependency(uint32_t index)
      {
      return &_dependencies[index];
      }

   void clearDependencyInfo(uint32_t index)
      {
      _dependencies[index].setRegister(NULL);
      _dependencies[index].assignFlags(0);
      _dependencies[index].setRealRegister(TR::RealRegister::NoReg);
      }

   void setDependencyInfo(uint32_t                                  index,
                          TR::Register                              *vr,
                          TR::RealRegister::RegDep                  rr,
                          uint8_t                                   flag)
     {
     setDependencyInfo(index, vr, static_cast<TR::RealRegister::RegNum>(rr), flag);
     }

   void setDependencyInfo(uint32_t                                  index,
                          TR::Register                              *vr,
                          TR::RealRegister::RegNum                  rr,
                          uint8_t                                   flag)
      {
      _dependencies[index].setRegister(vr);
      _dependencies[index].assignFlags(flag);
      _dependencies[index].setRealRegister(rr);
      if (vr) vr->setDependencySet(true);
      }

   TR::Register *searchForRegister(TR::Register* vr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (int32_t i=0; i<numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister() == vr && (_dependencies[i].getFlags() & flag))
            return _dependencies[i].getRegister();
         }
      return NULL;
      }

   TR::Register *searchForRegister(TR::RealRegister::RegNum rr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (int32_t i=0; i<numberOfRegisters; i++)
         {
         if (_dependencies[i].getRealRegister() == rr && (_dependencies[i].getFlags() & flag))
            return _dependencies[i].getRegister();
         }
      return NULL;
      }

   int32_t searchForRegisterPos(TR::Register* vr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (int32_t i=0; i<numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister() == vr && (_dependencies[i].getFlags() & flag))
            return i;
         }
      return -1;
      }

   uint32_t genBitMapOfAssignableGPRs(TR::CodeGenerator *cg, uint32_t numberOfRegisters);

   void setRealRegisterForDependency(int32_t index, TR::RealRegister::RegNum regNum)
      {
      _dependencies[index].setRealRegister(regNum);
      }

   void assignRegisters(TR::Instruction  *currentInstruction,
                        TR_RegisterKinds kindToBeAssigned,
                        uint32_t         numberOfRegisters,
                        TR::CodeGenerator *cg);

   void blockRegisters(uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister())
           _dependencies[i].getRegister()->block();
         }
      }

   void unblockRegisters(uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister())
           _dependencies[i].getRegister()->unblock();
         }
      }

   void stopUsingDepRegs(uint32_t numberOfRegisters, TR::Register *ret1, TR::Register *ret2, TR::CodeGenerator *cg)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *depReg = _dependencies[i].getRegister();
         if (depReg != ret1 && depReg != ret2)
            cg->stopUsingRegister(depReg);
         }
      }

   int8_t getNumUses() {return _numUses;}
   void incNumUses(int8_t n=1) { _numUses+=n;}

   };

namespace OMR
{
namespace Z
{

class RegisterDependencyConditions: public OMR::RegisterDependencyConditions
   {
   TR_S390RegisterDependencyGroup *_preConditions;
   TR_S390RegisterDependencyGroup *_postConditions;
   uint16_t                         _numPreConditions;
   uint16_t                         _addCursorForPre;
   uint16_t                         _numPostConditions;
   uint16_t                         _addCursorForPost;
   bool                            _isUsed;
   TR::CodeGenerator               *_cg;

   public:

   TR_ALLOC(TR_Memory::RegisterDependencyConditions)

   RegisterDependencyConditions(TR::CodeGenerator *cg,
                                       TR::Node          *node,
                                       uint32_t          extranum,
                                       TR::Instruction  **cursorPtr);

   // Make a copy of and exiting dep list and make room for new deps.

   RegisterDependencyConditions(TR::RegisterDependencyConditions* iConds, uint16_t numNewPreConds, uint16_t numNewPostConds, TR::CodeGenerator *cg);


   RegisterDependencyConditions( TR::RegisterDependencyConditions * conds_1,
                                        TR::RegisterDependencyConditions * conds_2,
                                        TR::CodeGenerator *cg);

   RegisterDependencyConditions(TR_S390RegisterDependencyGroup *_preConditions,
				       TR_S390RegisterDependencyGroup *_postConditions,
				       uint16_t numPreConds, uint16_t numPostConds, TR::CodeGenerator *cg)
      : _preConditions(_preConditions),
        _postConditions(_postConditions),
        _numPreConditions(numPreConds),
        _addCursorForPre(numPreConds),
        _numPostConditions(numPostConds),
        _addCursorForPost(numPostConds),
        _isUsed(false),
	_cg(cg)
      {}

   RegisterDependencyConditions()
      : _preConditions(NULL),
        _postConditions(NULL),
        _numPreConditions(0),
        _addCursorForPre(0),
        _numPostConditions(0),
        _addCursorForPost(0),
        _isUsed(false),
	_cg(NULL)
      {}

   //VMThread work: implicitly add an extra post condition for a possible vm thread
   //register post dependency.  Do not need for pre because they are so
   //infrequent.
   RegisterDependencyConditions(uint16_t numPreConds, uint16_t numPostConds, TR::CodeGenerator *cg)
      : _preConditions(TR_S390RegisterDependencyGroup::create(numPreConds, cg->trMemory())),
        _postConditions(TR_S390RegisterDependencyGroup::create(numPostConds + NUM_VM_THREAD_REG_DEPS, cg->trMemory())),
        _numPreConditions(numPreConds),
        _addCursorForPre(0),
        _numPostConditions(numPostConds + NUM_VM_THREAD_REG_DEPS),
        _addCursorForPost(0),
        _isUsed(false),
        _cg(cg)
      {
      for(int32_t i=0;i<numPreConds;i++)
         {
	 _preConditions->clearDependencyInfo(i);
	 }
      for(int32_t j=0;j<numPostConds + NUM_VM_THREAD_REG_DEPS;j++)
         {
         _postConditions->clearDependencyInfo(j);
         }
      }

   void resolveSplitDependencies(
      TR::CodeGenerator* cg,
      TR::Node* Noce, TR::Node* child,
      TR::RegisterPair * regPair, List<TR::Register>& regList,
      TR::Register*& reg, TR::Register*& highReg,
      TR::Register*& copyReg, TR::Register*& highCopyReg,
      TR::Instruction*& iCursor,
      TR::RealRegister::RegNum& regNum);

   bool getIsUsed() {return _isUsed;}
   void setIsUsed() {_isUsed=true;}
   void resetIsUsed() {_isUsed=false;}

   TR_S390RegisterDependencyGroup *getPreConditions()  {return _preConditions;}

   void unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg)
      {
      addPostCondition(reg, TR::RealRegister::AssignAny);
      }

   uint32_t getNumPreConditions() {return _numPreConditions;}

   uint32_t setNumPreConditions(uint16_t n, TR_Memory * m)
      {
      if (_preConditions == NULL)
         {
         _preConditions = new (n,m) TR_S390RegisterDependencyGroup;
	 for(int32_t i=0;i<n;i++)
	    {
	    _preConditions->clearDependencyInfo(i);
	    }
         }
      return _numPreConditions = n;
      }

   uint32_t getNumPostConditions() {return _numPostConditions;}

   uint32_t setNumPostConditions(uint16_t n, TR_Memory * m)
      {
      if (_postConditions == NULL)
         {
         _postConditions = new (n,m) TR_S390RegisterDependencyGroup;
	 for(int32_t i=0;i<n;i++)
	    {
	    _postConditions->clearDependencyInfo(i);
	    }
         }
      return _numPostConditions = n;
      }

   uint32_t getAddCursorForPre() {return _addCursorForPre;}
   uint32_t setAddCursorForPre(uint16_t a) {return _addCursorForPre = a;}

   uint32_t getAddCursorForPost() {return _addCursorForPost;}
   uint32_t setAddCursorForPost(uint16_t a) {return _addCursorForPost = a;}

   void addAssignAnyPreCondOnMemRef(TR::MemoryReference* memRef)
      {
      TR::Register* base = memRef->getBaseRegister();
      TR::Register* indx = memRef->getIndexRegister();

      if (base != NULL)
         {
         addPreConditionIfNotAlreadyInserted(base, TR::RealRegister::AssignAny);
         }
      if (indx != NULL)
         {
         addPreConditionIfNotAlreadyInserted(indx, TR::RealRegister::AssignAny);
         }
      }

   void addPreCondition(TR::Register                              *vr,
                        TR::RealRegister::RegNum rr,
                        uint8_t                                   flag = ReferencesDependentRegister)
       {
       addPreCondition(vr, static_cast<TR::RealRegister::RegDep>(rr), flag);
       }

   void addPreCondition(TR::Register                              *vr,
                        TR::RealRegister::RegDep rr,
                        uint8_t                                   flag = ReferencesDependentRegister)
      {
      TR_ASSERT(!getIsUsed(), "ERROR: cannot add pre conditions to an used dependency, create a copy first\n");

      // dont add dependencies if reg is real register
      if (vr && vr->getRealRegister()!=NULL) return;

      TR_ASSERT_FATAL(_addCursorForPre < _numPreConditions,"addPreCondition list overflow. addCursorForPre(%d), numPreConditions(%d), virtual register name(%s) and pointer(%p)\n",_addCursorForPre, _numPreConditions,vr->getRegisterName(_cg->comp()),vr);
      _preConditions->setDependencyInfo(_addCursorForPre++, vr, rr, flag);
      }

   TR_S390RegisterDependencyGroup *getPostConditions() {return _postConditions;}

   void addAssignAnyPostCondOnMemRef(TR::MemoryReference* memRef)
      {
      TR::Register* base = memRef->getBaseRegister();
      TR::Register* indx = memRef->getIndexRegister();

      if (base != NULL)
         {
         addPostConditionIfNotAlreadyInserted(base, TR::RealRegister::AssignAny, ReferencesDependentRegister);
         }
      if (indx != NULL)
         {
         addPostConditionIfNotAlreadyInserted(indx, TR::RealRegister::AssignAny, ReferencesDependentRegister);
         }
      }

   void addPostCondition(TR::Register                              *vr,
                         TR::RealRegister::RegNum rr,
                         uint8_t                                   flag = ReferencesDependentRegister)
      {
      addPostCondition(vr, static_cast<TR::RealRegister::RegDep>(rr), flag);
      }

   void addPostCondition(TR::Register                              *vr,
                         TR::RealRegister::RegDep rr,
                         uint8_t                                   flag = ReferencesDependentRegister)
      {
      TR_ASSERT(!getIsUsed(), "ERROR: cannot add post conditions to an used dependency, create a copy first\n");

      // dont add dependencies if reg is real register
      if (vr && vr->getRealRegister()!=NULL) return;
      TR_ASSERT_FATAL(_addCursorForPost < _numPostConditions,"addPostCondition list overflow. addCursorForPost(%d), numPostConditions(%d), virtual register name(%s) and pointer(%p)\n",_addCursorForPost, _numPostConditions,vr->getRegisterName(_cg->comp()),vr);
      _postConditions->setDependencyInfo(_addCursorForPost++, vr, rr, flag);
      }

   void assignPreConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindToBeAssigned, TR::CodeGenerator *cg)
      {
      if (_preConditions != NULL)
         {
         cg->clearRegisterAssignmentFlags();
         cg->setRegisterAssignmentFlag(TR_PreDependencyCoercion);
         _preConditions->assignRegisters(currentInstruction, kindToBeAssigned, _addCursorForPre, cg);
         }
      }

   void assignPostConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindToBeAssigned, TR::CodeGenerator *cg)
      {
      if (_postConditions != NULL)
         {
         cg->clearRegisterAssignmentFlags();
         cg->setRegisterAssignmentFlag(TR_PostDependencyCoercion);
         _postConditions->assignRegisters(currentInstruction, kindToBeAssigned, _addCursorForPost, cg);
         }
      }

   TR::Register *searchPreConditionRegister(TR::RealRegister::RegNum rr, uint8_t flag = RefsAndDefsDependentRegister)
      {
      return _preConditions==NULL?NULL:_preConditions->searchForRegister(rr, flag, _addCursorForPre, _cg);
      }

   TR::Register *searchPostConditionRegister(TR::RealRegister::RegNum rr, uint8_t flag = RefsAndDefsDependentRegister)
      {
      return _postConditions==NULL?NULL:_postConditions->searchForRegister(rr, flag, _addCursorForPost, _cg);
      }

   TR::Register *searchPreConditionRegister(TR::Register * vr, uint8_t flag = RefsAndDefsDependentRegister)
      {
      return _preConditions==NULL?NULL:_preConditions->searchForRegister(vr, flag, _addCursorForPre, _cg);
      }

   TR::Register *searchPostConditionRegister(TR::Register * vr, uint8_t flag = RefsAndDefsDependentRegister)
      {
      return _postConditions==NULL?NULL:_postConditions->searchForRegister(vr, flag, _addCursorForPost, _cg);
      }

   int32_t searchPostConditionRegisterPos(TR::Register * vr, uint8_t flag = RefsAndDefsDependentRegister)
      {
      return _postConditions==NULL?-1:_postConditions->searchForRegisterPos(vr, flag, _addCursorForPost, _cg);
      }

   void stopUsingDepRegs(TR::CodeGenerator *cg, TR::Register *ret1=NULL, TR::Register *ret2=NULL)
     {
     if (_preConditions != NULL)
        _preConditions->stopUsingDepRegs(_addCursorForPre, ret1, ret2, cg);
     if (_postConditions != NULL)
        _postConditions->stopUsingDepRegs(_addCursorForPost, ret1, ret2, cg);
     }


   // These methods are temporary until dependencies are re-engineered down the road
   // We only add in the post condition if the desired virt reg is not already spoken for
   bool addPreConditionIfNotAlreadyInserted(TR::Register *vr,
                                            TR::RealRegister::RegNum rr,
				                                uint8_t flag = ReferencesDependentRegister);
   bool addPreConditionIfNotAlreadyInserted(TR::Register *vr,
                                            TR::RealRegister::RegDep rr,
				                                uint8_t flag = ReferencesDependentRegister);

   bool addPostConditionIfNotAlreadyInserted(TR::Register *vr,
                                             TR::RealRegister::RegNum rr,
				                                 uint8_t flag = ReferencesDependentRegister);
   bool addPostConditionIfNotAlreadyInserted(TR::Register *vr,
                                             TR::RealRegister::RegDep rr,
				                                 uint8_t flag = ReferencesDependentRegister);

   TR::RegisterDependencyConditions *clone(TR::CodeGenerator *cg, int32_t additionalRegDeps);

   bool refsRegister(TR::Register *r);
   bool defsRegister(TR::Register *r);
   bool usesRegister(TR::Register *r);
   bool mayDefineRegister(TR::Register *r);

   void bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg, int32_t oldPreCursor=0, int32_t oldPostCursor=0);
   void createRegisterAssociationDirective(TR::Instruction *instruction, TR::CodeGenerator *cg);

   bool doesConditionExist( TR_S390RegisterDependencyGroup * regDepArr, TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, uint32_t numberOfRegisters, bool overwriteAssignAny = false );
   bool doesPreConditionExist( TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, bool overwriteAssignAny = false );
   bool doesPostConditionExist( TR::Register * vr, TR::RealRegister::RegNum rr, uint32_t flag, bool overwriteAssignAny = false );

   TR::CodeGenerator *cg()   { return _cg; }
   };

}
}

#endif
