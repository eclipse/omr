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

#ifndef OMR_X86_REGISTER_DEPENDENCY_INCL
#define OMR_X86_REGISTER_DEPENDENCY_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_CONNECTOR
   namespace OMR { namespace X86 { class RegisterDependencyConditions; } }
   namespace OMR { typedef OMR::X86::RegisterDependencyConditions RegisterDependencyConditionsConnector; }
#else
   #error OMR::X86::RegisterDependencyConditions expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterDependency.hpp"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }
template <typename ListKind> class List;

#define UsesGlobalDependentFPRegister (UsesDependentRegister | GlobalRegisterFPDependency)
#define NUM_DEFAULT_DEPENDENCIES 1

typedef uint16_t depsize_t;

class TR_X86RegisterDependencyGroup
   {
   bool _mayNeedToPopFPRegisters;
   bool _needToClearFPStack;
   TR::RegisterDependency  _dependencies[NUM_DEFAULT_DEPENDENCIES];

   TR_ALLOC_WITHOUT_NEW(TR_Memory::RegisterDependencyGroup)

   // Use TR_X86RegisterDependencyGroup::create to allocate an object of this type
   //
   void * operator new(size_t s, int32_t numDependencies, TR_Memory * m)
      {
      TR_ASSERT(numDependencies > 0, "operator new called with numDependencies == 0");
      if (numDependencies > NUM_DEFAULT_DEPENDENCIES)
         {
         s += (numDependencies-NUM_DEFAULT_DEPENDENCIES)*sizeof(TR::RegisterDependency);
         }
      return m->allocateHeapMemory(s);
      }

   void operator delete(void *p, int32_t numDependencies, TR_Memory * m)
      {
	  m->freeMemory(p, TR_AllocationKind::heapAlloc);
      }

   public:

   TR_X86RegisterDependencyGroup() {_mayNeedToPopFPRegisters = false; _needToClearFPStack = false;}

   static TR_X86RegisterDependencyGroup * create(int32_t numDependencies, TR_Memory * m)
      {
      return numDependencies ? new (numDependencies, m) TR_X86RegisterDependencyGroup : 0;
      }

   TR::RegisterDependency  *getRegisterDependency(TR_X86RegisterDependencyIndex index)
      {
      return &_dependencies[index];
      }

   void setDependencyInfo(TR_X86RegisterDependencyIndex   index,
                          TR::Register                   *vr,
                          TR::RealRegister::RegNum        rr,
                          TR::CodeGenerator              *cg,
                          uint8_t                         flag = UsesDependentRegister,
                          bool                            isAssocRegDependency = false);


   TR::RegisterDependency *findDependency(TR::Register *vr, TR_X86RegisterDependencyIndex stop)
      {
      TR::RegisterDependency *result = NULL;
      for (TR_X86RegisterDependencyIndex i=0; !result && (i < stop); i++)
         if (_dependencies[i].getRegister() == vr)
            result = _dependencies+i;
      return result;
      }

   TR::RegisterDependency *findDependency(TR::RealRegister::RegNum rr, TR_X86RegisterDependencyIndex stop)
      {
      TR::RegisterDependency *result = NULL;
      for (TR_X86RegisterDependencyIndex i=0; !result && (i < stop); i++)
         if (_dependencies[i].getRealRegister() == rr)
            result = _dependencies+i;
      return result;
      }

   void assignRegisters(TR::Instruction   *currentInstruction,
                        TR_RegisterKinds  kindsToBeAssigned,
                        TR_X86RegisterDependencyIndex          numberOfRegisters,
                        TR::CodeGenerator *cg);

   void assignFPRegisters(TR::Instruction   *currentInstruction,
                          TR_RegisterKinds  kindsToBeAssigned,
                          TR_X86RegisterDependencyIndex          numberOfRegisters,
                          TR::CodeGenerator *cg);


   void orderGlobalRegsOnFPStack(TR::Instruction    *cursor,
                                 TR_RegisterKinds   kindsToBeAssigned,
                                 TR_X86RegisterDependencyIndex           numberOfRegisters,
                                 List<TR::Register> *poppedRegisters,
                                 TR::CodeGenerator  *cg);


   void blockRegisters(TR_X86RegisterDependencyIndex numberOfRegisters)
      {
      for (TR_X86RegisterDependencyIndex i = 0; i < numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister())
            {
            _dependencies[i].getRegister()->block();
            }
         }
      }

   void unblockRegisters(TR_X86RegisterDependencyIndex numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister())
            {
            _dependencies[i].getRegister()->unblock();
            }
         }
      }

   void setMayNeedToPopFPRegisters(bool b) {_mayNeedToPopFPRegisters = b;}
   bool getMayNeedToPopFPRegisters() {return _mayNeedToPopFPRegisters;}

   void setNeedToClearFPStack(bool b) {_needToClearFPStack = b;}
   bool getNeedToClearFPStack() {return _needToClearFPStack;}

   void blockRealDependencyRegisters(TR_X86RegisterDependencyIndex numberOfRegisters, TR::CodeGenerator *cg);
   void unblockRealDependencyRegisters(TR_X86RegisterDependencyIndex numberOfRegisters, TR::CodeGenerator *cg);
   };

namespace OMR
{
namespace X86
{
class RegisterDependencyConditions: public OMR::RegisterDependencyConditions
   {
   TR_X86RegisterDependencyGroup *_preConditions;
   TR_X86RegisterDependencyGroup *_postConditions;
   TR_X86RegisterDependencyIndex  _numPreConditions;
   TR_X86RegisterDependencyIndex  _addCursorForPre;
   TR_X86RegisterDependencyIndex  _numPostConditions;
   TR_X86RegisterDependencyIndex  _addCursorForPost;

   TR_X86RegisterDependencyIndex unionDependencies(TR_X86RegisterDependencyGroup *deps, TR_X86RegisterDependencyIndex cursor, TR::Register *vr, TR::RealRegister::RegNum rr, TR::CodeGenerator *cg, uint8_t flag, bool isAssocRegDependency);
   TR_X86RegisterDependencyIndex unionRealDependencies(TR_X86RegisterDependencyGroup *deps, TR_X86RegisterDependencyIndex cursor, TR::Register *vr, TR::RealRegister::RegNum rr, TR::CodeGenerator *cg, uint8_t flag, bool isAssocRegDependency);

   public:

   TR_ALLOC(TR_Memory::RegisterDependencyConditions)

   RegisterDependencyConditions()
      : _preConditions(NULL),
        _postConditions(NULL),
        _numPreConditions(0),
        _addCursorForPre(0),
        _numPostConditions(0),
        _addCursorForPost(0)
      {}

   RegisterDependencyConditions(TR_X86RegisterDependencyIndex numPreConds, TR_X86RegisterDependencyIndex numPostConds, TR_Memory * m)
      : _preConditions(TR_X86RegisterDependencyGroup::create(numPreConds, m)),
        _postConditions(TR_X86RegisterDependencyGroup::create(numPostConds, m)),
        _numPreConditions(numPreConds),
        _addCursorForPre(0),
        _numPostConditions(numPostConds),
        _addCursorForPost(0)
      {}

   RegisterDependencyConditions(TR::Node *node, TR::CodeGenerator *cg, TR_X86RegisterDependencyIndex additionalRegDeps = 0, List<TR::Register> * = 0);

   void unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg);

   TR::RegisterDependencyConditions  *clone(TR::CodeGenerator *cg, TR_X86RegisterDependencyIndex additionalRegDeps = 0);

   TR_X86RegisterDependencyGroup *getPreConditions()  {return _preConditions;}

   void setMayNeedToPopFPRegisters(bool b)
      {
      if (_preConditions)
         _preConditions->setMayNeedToPopFPRegisters(b);
      if (_postConditions)
         _postConditions->setMayNeedToPopFPRegisters(b);
      }

   void setNeedToClearFPStack(bool b)
      {
      if (_preConditions)
         _preConditions->setNeedToClearFPStack(b);
      else
         {
         if (_postConditions)
            _postConditions->setNeedToClearFPStack(b);
         }
      }

   TR_X86RegisterDependencyIndex getNumPreConditions() {return _numPreConditions;}

   TR_X86RegisterDependencyIndex setNumPreConditions(TR_X86RegisterDependencyIndex n, TR_Memory * m)
      {
      if (_preConditions == NULL)
         {
         _preConditions = TR_X86RegisterDependencyGroup::create(n, m);
         }
      return (_numPreConditions = n);
      }

   TR_X86RegisterDependencyIndex getNumPostConditions() {return _numPostConditions;}
   TR_X86RegisterDependencyIndex setNumPostConditions(TR_X86RegisterDependencyIndex n, TR_Memory * m)
      {
      if (_postConditions == NULL)
         {
         _postConditions = TR_X86RegisterDependencyGroup::create(n, m);
         }
      return (_numPostConditions = n);
      }

   TR_X86RegisterDependencyIndex getAddCursorForPre() {return _addCursorForPre;}
   TR_X86RegisterDependencyIndex setAddCursorForPre(TR_X86RegisterDependencyIndex a) {return (_addCursorForPre = a);}

   TR_X86RegisterDependencyIndex getAddCursorForPost() {return _addCursorForPost;}
   TR_X86RegisterDependencyIndex setAddCursorForPost(TR_X86RegisterDependencyIndex a) {return (_addCursorForPost = a);}

   void addPreCondition(TR::Register              *vr,
                        TR::RealRegister::RegNum   rr,
                        TR::CodeGenerator         *cg,
                        uint8_t                    flag = UsesDependentRegister,
                        bool                       isAssocRegDependency = false)
      {
      TR_X86RegisterDependencyIndex newCursor = unionRealDependencies(_preConditions, _addCursorForPre, vr, rr, cg, flag, isAssocRegDependency);
      TR_ASSERT(newCursor <= _numPreConditions, "Too many dependencies");
      if (newCursor == _addCursorForPre)
         _numPreConditions--; // A vmThread/ebp dependency was displaced, so there is now one less condition.
      else
         _addCursorForPre = newCursor;
      }

   void unionPreCondition(TR::Register              *vr,
                          TR::RealRegister::RegNum   rr,
                          TR::CodeGenerator         *cg,
                          uint8_t                    flag = UsesDependentRegister,
                          bool                       isAssocRegDependency = false)
      {
      TR_X86RegisterDependencyIndex newCursor = unionDependencies(_preConditions, _addCursorForPre, vr, rr, cg, flag, isAssocRegDependency);
      TR_ASSERT(newCursor <= _numPreConditions, "Too many dependencies");
      if (newCursor == _addCursorForPre)
         _numPreConditions--; // A union occurred, so there is now one less condition
      else
         _addCursorForPre = newCursor;
      }

   TR_X86RegisterDependencyGroup *getPostConditions() {return _postConditions;}

   void addPostCondition(TR::Register              *vr,
                         TR::RealRegister::RegNum   rr,
                         TR::CodeGenerator         *cg,
                         uint8_t                    flag = UsesDependentRegister,
                         bool                       isAssocRegDependency = false)
      {
      TR_X86RegisterDependencyIndex newCursor = unionRealDependencies(_postConditions, _addCursorForPost, vr, rr, cg, flag, isAssocRegDependency);
      TR_ASSERT(newCursor <= _numPostConditions, "Too many dependencies");
      if (newCursor == _addCursorForPost)
         _numPostConditions--; // A vmThread/ebp dependency was displaced, so there is now one less condition.
      else
         _addCursorForPost = newCursor;
      }

   void unionPostCondition(TR::Register              *vr,
                           TR::RealRegister::RegNum   rr,
                           TR::CodeGenerator         *cg,
                           uint8_t                    flag = UsesDependentRegister,
                           bool                       isAssocRegDependency = false)
      {
      TR_X86RegisterDependencyIndex newCursor = unionDependencies(_postConditions, _addCursorForPost, vr, rr, cg, flag, isAssocRegDependency);
      TR_ASSERT(newCursor <= _numPostConditions, "Too many dependencies");
      if (newCursor == _addCursorForPost)
         _numPostConditions--; // A union occurred, so there is now one less condition
      else
         _addCursorForPost = newCursor;
      }

   TR::RegisterDependency *findPreCondition (TR::Register *vr){ return _preConditions ->findDependency(vr, _addCursorForPre ); }
   TR::RegisterDependency *findPostCondition(TR::Register *vr){ return _postConditions->findDependency(vr, _addCursorForPost); }
   TR::RegisterDependency *findPreCondition (TR::RealRegister::RegNum rr){ return _preConditions ->findDependency(rr, _addCursorForPre ); }
   TR::RegisterDependency *findPostCondition(TR::RealRegister::RegNum rr){ return _postConditions->findDependency(rr, _addCursorForPost); }

   void assignPreConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindsToBeAssigned, TR::CodeGenerator *cg)
      {
      if (_preConditions != NULL)
         {
         if ((kindsToBeAssigned & TR_X87_Mask))
            {
            _preConditions->assignFPRegisters(currentInstruction, kindsToBeAssigned, _numPreConditions, cg);
            }
         else
            {
            cg->clearRegisterAssignmentFlags();
            cg->setRegisterAssignmentFlag(TR_PreDependencyCoercion);
            _preConditions->assignRegisters(currentInstruction, kindsToBeAssigned, _numPreConditions, cg);
            }
         }
      }

   void assignPostConditionRegisters(TR::Instruction *currentInstruction, TR_RegisterKinds kindsToBeAssigned, TR::CodeGenerator *cg)
      {
      if (_postConditions != NULL)
         {
         if ((kindsToBeAssigned & TR_X87_Mask))
            {
            _postConditions->assignFPRegisters(currentInstruction, kindsToBeAssigned, _numPostConditions, cg);
            }
         else
            {
            cg->clearRegisterAssignmentFlags();
            cg->setRegisterAssignmentFlag(TR_PostDependencyCoercion);
            _postConditions->assignRegisters(currentInstruction, kindsToBeAssigned, _numPostConditions, cg);
            }
         }
      }

   void blockPreConditionRegisters()
      {
      _preConditions->blockRegisters(_numPreConditions);
      }

   void unblockPreConditionRegisters()
      {
      _preConditions->unblockRegisters(_numPreConditions);
      }

   void blockPostConditionRegisters()
      {
      _postConditions->blockRegisters(_numPostConditions);
      }

   void unblockPostConditionRegisters()
      {
      _postConditions->unblockRegisters(_numPostConditions);
      }

   void blockPostConditionRealDependencyRegisters(TR::CodeGenerator *cg)
      {
      _postConditions->blockRealDependencyRegisters(_numPostConditions, cg);
      }

   void unblockPostConditionRealDependencyRegisters(TR::CodeGenerator *cg)
      {
      _postConditions->unblockRealDependencyRegisters(_numPostConditions, cg);
      }

   void blockPreConditionRealDependencyRegisters(TR::CodeGenerator *cg)
      {
      _preConditions->blockRealDependencyRegisters(_numPreConditions, cg);
      }

   void unblockPreConditionRealDependencyRegisters(TR::CodeGenerator *cg)
      {
      _preConditions->unblockRealDependencyRegisters(_numPreConditions, cg);
      }

   // All conditions are added - set the number of conditions to be the number
   // currently added
   //
   void stopAddingPreConditions()
      {
      _numPreConditions  = _addCursorForPre;
      }

   void stopAddingPostConditions()
      {
      _numPostConditions  = _addCursorForPost;
      }

   void stopAddingConditions()
      {
      stopAddingPreConditions();
      stopAddingPostConditions();
      }

   void createRegisterAssociationDirective(TR::Instruction *instruction, TR::CodeGenerator *cg);

   TR::RealRegister *getRealRegisterFromVirtual(TR::Register *virtReg, TR::CodeGenerator *cg);

   bool refsRegister(TR::Register *r);
   bool defsRegister(TR::Register *r);
   bool usesRegister(TR::Register *r);

   void useRegisters(TR::Instruction  *instr, TR::CodeGenerator *cg);

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   uint32_t numReferencedGPRegisters(TR::CodeGenerator *);
   uint32_t numReferencedFPRegisters(TR::CodeGenerator *);
   void printFullRegisterDependencyInfo(FILE *pOutFile);
   void printDependencyConditions(TR_X86RegisterDependencyGroup *conditions,
                                  TR_X86RegisterDependencyIndex   numConditions,
                                  char                           *prefix,
                                  FILE                           *pOutFile);
#endif

   };
}
}

////////////////////////////////////////////////////
// Generate Routines
////////////////////////////////////////////////////

TR::RegisterDependencyConditions  * generateRegisterDependencyConditions(TR::Node *, TR::CodeGenerator *, TR_X86RegisterDependencyIndex = 0, List<TR::Register> * = 0);
TR::RegisterDependencyConditions  * generateRegisterDependencyConditions(TR_X86RegisterDependencyIndex, TR_X86RegisterDependencyIndex, TR::CodeGenerator *);

#endif
