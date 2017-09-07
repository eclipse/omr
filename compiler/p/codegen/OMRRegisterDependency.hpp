/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_Power_REGISTER_DEPENDENCY_INCL
#define OMR_Power_REGISTER_DEPENDENCY_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_CONNECTOR
namespace OMR { namespace Power { class RegisterDependencyConditions; } }
namespace OMR { typedef OMR::Power::RegisterDependencyConditions RegisterDependencyConditionsConnector; }
#else
#error OMR::Power::RegisterDependencyConditions expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterDependency.hpp"

#include <stddef.h>
#include <stdint.h>
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

#define NUM_DEFAULT_DEPENDENCIES 1

class TR_PPCRegisterDependencyGroup
   {
   TR::RegisterDependency _dependencies[NUM_DEFAULT_DEPENDENCIES];

   public:

   TR_ALLOC_WITHOUT_NEW(TR_Memory::RegisterDependencyGroup)

   TR_PPCRegisterDependencyGroup() {}

   void * operator new(size_t s, TR_Memory * m) {return m->allocateHeapMemory(s);}

   // Use TR_PPCRegisterDependencyGroup::create to allocate an object of this type
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

   static TR_PPCRegisterDependencyGroup * create(int32_t numDependencies, TR_Memory * m)
      {
      return numDependencies ? new (numDependencies, m) TR_PPCRegisterDependencyGroup : 0;
      }

   TR::RegisterDependency *getRegisterDependency(uint32_t index)
      {
      return &_dependencies[index];
      }

   void setDependencyInfo(uint32_t                                  index,
                          TR::Register                              *vr,
                          TR::RealRegister::RegNum rr,
                          uint8_t                                   flag)
      {
      _dependencies[index].setRegister(vr);
      _dependencies[index].assignFlags(flag);
      _dependencies[index].setRealRegister(rr);
      }

   TR::Register *searchForRegister(TR::RealRegister::RegNum rr, uint32_t numberOfRegisters)
      {
      for (int i=0; i<numberOfRegisters; i++)
         {
         if (_dependencies[i].getRealRegister() == rr)
            return(_dependencies[i].getRegister());
         }
      return(NULL);
      }

   bool containsVirtualRegister(TR::Register *r, uint32_t numberOfRegisters)
      {
      for (int i=0; i<numberOfRegisters; i++)
         {
         if (_dependencies[i].getRegister() == r)
            return true;
         }
      return(false);
      }


   void assignRegisters(TR::Instruction   *currentInstruction,
                        TR_RegisterKinds  kindToBeAssigned,
                        uint32_t          numberOfRegisters,
                        TR::CodeGenerator *cg);

   void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state, uint32_t numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         state->addVirtualRegister(_dependencies[i].getRegister());
         }
      }

   void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state, uint32_t numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         state->removeVirtualRegister(_dependencies[i].getRegister());
         }
      }

   void blockRegisters(uint32_t numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         _dependencies[i].getRegister()->block();
         }
      }

   void unblockRegisters(uint32_t numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         _dependencies[i].getRegister()->unblock();
         }
      }

   void stopUsingDepRegs(uint32_t numberOfRegisters, int numRetReg, TR::Register **retReg, TR::CodeGenerator *cg)
      {
      for (uint32_t i = 0; i < numberOfRegisters; i++)
         {
         TR::Register *depReg = _dependencies[i].getRegister();
         bool found = false;
         for (int j = 0; j < numRetReg; j++)
            if (depReg == retReg[j])
               found = true;
         if (!found)
            cg->stopUsingRegister(depReg);
         }
      }

   void stopUsingDepRegs(uint32_t numberOfRegisters, TR::Register *ret1, TR::Register *ret2, TR::CodeGenerator *cg)
      {
      TR::Register* retRegs[2] = {ret1, ret2};
      stopUsingDepRegs(numberOfRegisters, 2, retRegs, cg);
      }

   void setExcludeGPR0(TR::Register *r, uint32_t numberOfRegisters)
      {
      for (uint32_t i = 0; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister() == r)
            {
            _dependencies[i].setExcludeGPR0();
            // Even if the virtual reg r is in the dependencies multiple times, it's sufficient to exclude gr0 on the first of such dependencies
            // because !gr0 NoReg dependencies are handled before NoReg dependencies, so we'll assign it to !gr0 first and then any remaining
            // dependencies become no-ops
            break;
            }
         }
      }
   };

namespace OMR
{
namespace Power
{
class RegisterDependencyConditions: public OMR::RegisterDependencyConditions
   {
   TR_PPCRegisterDependencyGroup *_preConditions;
   TR_PPCRegisterDependencyGroup *_postConditions;
   uint16_t                       _numPreConditions;
   uint16_t                       _addCursorForPre;
   uint16_t                       _numPostConditions;
   uint16_t                       _addCursorForPost;

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

   RegisterDependencyConditions(uint16_t numPreConds, uint16_t numPostConds, TR_Memory * m)
      : _preConditions(TR_PPCRegisterDependencyGroup::create(numPreConds, m)),
        _postConditions(TR_PPCRegisterDependencyGroup::create(numPostConds, m)),
        _numPreConditions(numPreConds),
        _addCursorForPre(0),
        _numPostConditions(numPostConds),
        _addCursorForPost(0)
      {}

   RegisterDependencyConditions(TR::CodeGenerator *cg, TR::Node *node, uint32_t extranum, TR::Instruction **cursorPtr=NULL);
   TR::RegisterDependencyConditions *clone(TR::CodeGenerator *cg, TR::RegisterDependencyConditions *added=NULL);
   TR::RegisterDependencyConditions *cloneAndFix(TR::CodeGenerator *cg, TR::RegisterDependencyConditions *added=NULL);

   void unionNoRegPostCondition(TR::Register *reg, TR::CodeGenerator *cg);

   TR_PPCRegisterDependencyGroup *getPreConditions()  {return _preConditions;}

   uint32_t getNumPreConditions() {return _numPreConditions;}

   uint32_t setNumPreConditions(uint16_t n, TR_Memory * m)
      {
      if (_preConditions == NULL)
         {
         _preConditions = TR_PPCRegisterDependencyGroup::create(n, m);
         }
      return (_numPreConditions = n);
      }

   uint32_t getNumPostConditions() {return _numPostConditions;}

   uint32_t setNumPostConditions(uint16_t n, TR_Memory * m)
      {
      if (_postConditions == NULL)
         {
         _postConditions = TR_PPCRegisterDependencyGroup::create(n, m);
         }
      return (_numPostConditions = n);
      }

   uint32_t getAddCursorForPre() {return _addCursorForPre;}
   uint32_t setAddCursorForPre(uint16_t a) {return (_addCursorForPre = a);}

   uint32_t getAddCursorForPost() {return _addCursorForPost;}
   uint32_t setAddCursorForPost(uint16_t a) {return (_addCursorForPost = a);}

   void addPreCondition(TR::Register                              *vr,
                        TR::RealRegister::RegNum rr,
                        uint8_t                                   flag = UsesDependentRegister)
      {
      TR_ASSERT(_addCursorForPre < _numPreConditions, " Pre Condition array bounds overflow");
      _preConditions->setDependencyInfo(_addCursorForPre++, vr, rr, flag);
      }

   TR_PPCRegisterDependencyGroup *getPostConditions() {return _postConditions;}

   void addPostCondition(TR::Register                              *vr,
                         TR::RealRegister::RegNum rr,
                         uint8_t                                   flag = UsesDependentRegister)
      {
      TR_ASSERT(_addCursorForPost < _numPostConditions, " Post Condition array bounds overflow");
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

   void registersGoLive(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      _preConditions->registersGoLive(state, _addCursorForPre);
      _preConditions->registersGoDead(state, _addCursorForPre);
      _postConditions->registersGoLive(state, _addCursorForPost);
      }

   void registersGoDead(TR::CodeGenerator::TR_RegisterPressureState *state)
      {
      _postConditions->registersGoDead(state, _addCursorForPost);
      }

   TR::Register *searchPreConditionRegister(TR::RealRegister::RegNum rr)
      {
      return(_preConditions==NULL?NULL:_preConditions->searchForRegister(rr, _addCursorForPre));
      }

   TR::Register *searchPostConditionRegister(TR::RealRegister::RegNum rr)
      {
      return(_postConditions==NULL?NULL:_postConditions->searchForRegister(rr, _addCursorForPost));
      }

   bool preConditionContainsVirtual(TR::Register *r)
      {
      return(_preConditions==NULL?false:_preConditions->containsVirtualRegister(r, _addCursorForPre));
      }

   bool postConditionContainsVirtual(TR::Register *r)
      {
      return(_postConditions==NULL?false:_postConditions->containsVirtualRegister(r, _addCursorForPost));
      }

   TR::Register *getTargetRegister(uint32_t index, TR::CodeGenerator * cg);

   TR::Register *getSourceRegister(uint32_t index);

   void stopUsingDepRegs(TR::CodeGenerator *cg, int numRetReg, TR::Register ** retReg);

   void stopUsingDepRegs(TR::CodeGenerator *cg, TR::Register * ret1=NULL, TR::Register *ret2 = NULL);

   bool refsRegister(TR::Register *r);
   bool defsRegister(TR::Register *r);
   bool defsRealRegister(TR::Register *r);
   bool usesRegister(TR::Register *r);

   void bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg);

   void setPreDependencyExcludeGPR0(TR::Register *r)
      {
      _preConditions->setExcludeGPR0(r, _addCursorForPre);
      }
   void setPostDependencyExcludeGPR0(TR::Register *r)
      {
      _postConditions->setExcludeGPR0(r, _addCursorForPost);
      }

   };
}
}

// Convenience class to temporarily hold register dependencies.
// This class allocates space for the maximum number of GPR and CCR dependencies that an evaluator can use
// and therefore frees you from trying to determine ahead of time how many dependencies you need.
// It can create a TR::RegisterDependencyConditions object with exactly the number of dependencies currently defined.
// Therefore, objects of this class should be stack allocated so that they'll automatically be freed when no longer
// needed.
//
class TR_PPCScratchRegisterDependencyConditions
   {
   public:
   // NOTE:
   // No TR_Memory type defined for this class
   // since current is as a stack-alloc'd object.
   // To heap-alloc, update the class def with something like:
   //    TR_ALLOC(TR_Memory::PPCScratchRegisterDependencyConditions)

   TR_PPCScratchRegisterDependencyConditions() : _numGPRDeps(0), _excludeGPR0(0), _numCCRDeps(0) {}

   uint32_t getNumberOfGPRDependencies() { return _numGPRDeps; }
   uint32_t getNumberOfCCRDependencies() { return _numCCRDeps; }
   uint32_t getNumberOfDependencies() { return _numGPRDeps + _numCCRDeps; }

   void addDependency(TR::CodeGenerator *cg, TR::Register *vr, TR::RealRegister::RegNum rr, bool excludeGPR0 = false, uint8_t flag = UsesDependentRegister)
      {
      TR_ASSERT(sizeof(_gprDeps) / sizeof(TR::RegisterDependency) <= sizeof(_excludeGPR0) * 8, "Too many GPR dependencies, use a bit vector if more are needed");
      TR_ASSERT(_numGPRDeps < TR::RealRegister::LastAssignableGPR - TR::RealRegister::FirstGPR + 1, "Too many GPR dependencies");
      TR_ASSERT(_numCCRDeps < TR::RealRegister::LastAssignableCCR - TR::RealRegister::FirstCCR + 1, "Too many CCR dependencies");
      bool isGPR = rr >= TR::RealRegister::FirstGPR && rr <= TR::RealRegister::LastAssignableGPR;
      TR_ASSERT(!excludeGPR0 || isGPR, "Exclude gr0 doesn't make sense for CCR dependency");
      TR_ASSERT(isGPR || (rr >= TR::RealRegister::FirstCCR && rr <= TR::RealRegister::LastAssignableCCR), "Expecting GPR or CCR only");
      if (!vr)
         {
         vr = cg->allocateRegister(isGPR ? TR_GPR : TR_CCR);
         cg->stopUsingRegister(vr);
         }
      if (isGPR)
         {
         _gprDeps[_numGPRDeps].setRegister(vr);
         _gprDeps[_numGPRDeps].assignFlags(flag);
         _gprDeps[_numGPRDeps].setRealRegister(rr);
         _excludeGPR0 = excludeGPR0 ? _excludeGPR0 | 1 << _numGPRDeps : _excludeGPR0 & ~(1 << _numGPRDeps);
         ++_numGPRDeps;
         }
      else
         {
         _ccrDeps[_numCCRDeps].setRegister(vr);
         _ccrDeps[_numCCRDeps].assignFlags(flag);
         _ccrDeps[_numCCRDeps].setRealRegister(rr);
         ++_numCCRDeps;
         }
      }

   static TR::RegisterDependencyConditions* createDependencyConditions(TR::CodeGenerator *cg,
                                                                         TR_PPCScratchRegisterDependencyConditions *pre,
                                                                         TR_PPCScratchRegisterDependencyConditions *post);

   private:
   uint32_t                     _numGPRDeps;
   TR::RegisterDependency      _gprDeps[TR::RealRegister::LastAssignableGPR - TR::RealRegister::FirstGPR + 1];
   uint32_t                     _excludeGPR0;
   uint32_t                     _numCCRDeps;
   TR::RegisterDependency      _ccrDeps[TR::RealRegister::LastAssignableCCR - TR::RealRegister::FirstCCR + 1];
   };

// Small helper class to speedup queries
// Maps real register numbers to dependencies/virtuals
const uint8_t MAP_NIL = 255;
class TR_PPCRegisterDependencyMap
   {
   private:
   TR::RegisterDependency *deps;
   uint8_t targetTable[TR::RealRegister::NumRegisters];
   uint8_t assignedTable[TR::RealRegister::NumRegisters];

   public:
   // NOTE:
   // No TR_Memory type defined for this class
   // since current is as a stack-alloc'd object.
   // To heap-alloc, update the class def with something like:
   //    TR_ALLOC(TR_Memory::PPCRegisterDependencyMap)
   TR_PPCRegisterDependencyMap(TR::RegisterDependency *deps, uint32_t numDeps)
      : deps(deps)
      {
      TR_ASSERT(numDeps <= MAP_NIL, "Too many dependencies for this LUT");
      for (uint32_t i = 0; i < TR::RealRegister::NumRegisters; ++i)
         {
         targetTable[i] = MAP_NIL;
         assignedTable[i] = MAP_NIL;
         }
      }

   // Initialize the LUTs using this (we don't do it in the ctor because
   // in many places we already traverse the dep list and we can call
   // this method there and save an extra loop).
   void addDependency(TR::RealRegister::RegNum rr, TR::RealRegister *assignedReg, uint32_t index)
      {
      if (rr != TR::RealRegister::NoReg && rr != TR::RealRegister::SpilledReg)
         {
         TR_ASSERT(rr >= 0 && rr < TR::RealRegister::NumRegisters, "Register number %d used as index but out of range", rr);
         TR_ASSERT(targetTable[rr] == MAP_NIL || // TODO: Figure out where the same dep is being added more than once!
            deps[targetTable[rr]].getRegister() == deps[index].getRegister(),
            "Multiple virtual registers depend on a single real register %d", rr);
         targetTable[rr] = index;
         }

      if (assignedReg)
         {
         TR::RealRegister::RegNum arr = toRealRegister(assignedReg)->getRegisterNumber();
         TR_ASSERT(arr >= 0 && arr < TR::RealRegister::NumRegisters, "Register number %d used as index but out of range", arr);
         TR_ASSERT(assignedTable[arr] == MAP_NIL || // TODO: Figure out where the same dep is being added more than once!
            deps[assignedTable[arr]].getRegister() == deps[index].getRegister(),
            "Multiple virtual registers assigned to a single real register %d", arr);
         assignedTable[arr] = index;
         }
      }

   void addDependency(TR::RegisterDependency& dep, uint32_t index)
      {
      TR_ASSERT(&deps[index] == &dep, "Dep pointer/index mismatch");
      addDependency(dep.getRealRegister(), dep.getRegister()->getAssignedRealRegister(), index);
      }

   void addDependency(TR::RegisterDependency *dep, uint32_t index)
      {
      TR_ASSERT(&deps[index] == dep, "Dep pointer/index mismatch");
      addDependency(dep->getRealRegister(), dep->getRegister()->getAssignedRealRegister(), index);
      }

   TR::RegisterDependency* getDependencyWithTarget(TR::RealRegister::RegNum rr)
      {
      TR_ASSERT(rr >= 0 && rr < TR::RealRegister::NumRegisters, "Register number used as index but out of range");
      TR_ASSERT(rr != TR::RealRegister::NoReg, "Multiple dependencies can map to 'NoReg', can't return just one");
      TR_ASSERT(rr != TR::RealRegister::SpilledReg, "Multiple dependencies can map to 'SpilledReg', can't return just one");
      return targetTable[rr] != MAP_NIL ? &deps[targetTable[rr]] : NULL;
      }

   TR::Register* getVirtualWithTarget(TR::RealRegister::RegNum rr)
      {
      TR::RegisterDependency *d = getDependencyWithTarget(rr);
      return d ? d->getRegister() : NULL;
      }

   uint8_t getTargetIndex(TR::RealRegister::RegNum rr)
      {
      TR_ASSERT(targetTable[rr] != MAP_NIL, "No such target register in dependency condition");
      return targetTable[rr];
      }

   TR::RegisterDependency* getDependencyWithAssigned(TR::RealRegister::RegNum rr)
      {
      TR_ASSERT(rr >= 0 && rr < TR::RealRegister::NumRegisters, "Register number used as index but out of range");
      return assignedTable[rr] != MAP_NIL ? &deps[assignedTable[rr]] : NULL;
      }

   TR::Register* getVirtualWithAssigned(TR::RealRegister::RegNum rr)
      {
      TR::RegisterDependency *d = getDependencyWithAssigned(rr);
      return d ? d->getRegister() : NULL;
      }
   };

#endif
