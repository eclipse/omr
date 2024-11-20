/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#ifndef OMR_REGISTER_DEPENDENCY_INCL
#define OMR_REGISTER_DEPENDENCY_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_CONNECTOR
   namespace OMR { class RegisterDependencyConditions; }
   namespace OMR { typedef OMR::RegisterDependencyConditions RegisterDependencyConditionsConnector; }
#endif

#ifndef OMR_REGISTER_DEPENDENCY_GROUP_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_GROUP_CONNECTOR
   namespace OMR { class RegisterDependencyGroup; }
   namespace OMR { typedef OMR::RegisterDependencyGroup RegisterDependencyGroupConnector; }
#endif

#include "env/TRMemory.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/RegisterDependencyStruct.hpp"

namespace OMR
{

class OMR_EXTENSIBLE RegisterDependencyGroup
   {
   public:

   TR_ALLOC_WITHOUT_NEW(TR_Memory::RegisterDependencyGroup)

   RegisterDependencyGroup()
#if defined(OMR_ARCH_S390)
      : _numUses(0)
#elif defined(__GNUC__) && !defined(__clang__) /* defined(OMR_ARCH_S390) */
      : _unused('\0')
#endif /* defined(OMR_ARCH_S390) */
      {}

   TR::RegisterDependencyGroup *self();

   void *operator new(size_t s, int32_t numDependencies, TR_Memory *m)
      {
      TR_ASSERT(numDependencies >= 0, "Number of dependencies must be non-negative");
      s += numDependencies * sizeof(TR::RegisterDependency);
#if defined(__clang__) || !defined(__GNUC__)
      s -= NUM_DEFAULT_DEPENDENCIES * sizeof(TR::RegisterDependency);
#endif /* defined(__clang__) || !defined(__GNUC__) */
      return m->allocateHeapMemory(s, TR_MemoryBase::RegisterDependencyGroup);
      }

   void operator delete(void *p, int32_t numDependencies, TR_Memory *m) {}

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

   void setDependencyInfo(uint32_t index, TR::Register *vr, TR::RealRegister::RegNum rr, uint8_t flag)
      {
      _dependencies[index].setRegister(vr);
      _dependencies[index].assignFlags(flag);
      _dependencies[index].setRealRegister(rr);
      }

   bool containsVirtualRegister(TR::Register *r, uint32_t numberOfRegisters)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister() == r)
            return true;
         }

      return false;
      }

   TR::Register *searchForRegister(TR::RealRegister::RegNum rr, uint32_t numberOfRegisters)
      {
      for (auto i = 0U;  i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRealRegister() == rr)
            return _dependencies[i].getRegister();
         }

      return NULL;
      }

   TR::Register *searchForRegister(TR::Register* vr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister() == vr && (_dependencies[i].getFlags() & flag))
            return _dependencies[i].getRegister();
         }

      return NULL;
      }

   TR::Register *searchForRegister(TR::RealRegister::RegNum rr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRealRegister() == rr && (_dependencies[i].getFlags() & flag))
            return _dependencies[i].getRegister();
         }

      return NULL;
      }

   int32_t searchForRegisterPos(TR::Register* vr, uint8_t flag, uint32_t numberOfRegisters, TR::CodeGenerator *cg)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister() == vr && (_dependencies[i].getFlags() & flag))
            return i;
         }

      return -1;
      }

   void setRealRegisterForDependency(int32_t index, TR::RealRegister::RegNum regNum)
      {
      _dependencies[index].setRealRegister(regNum);
      }

   void blockRegisters(uint32_t numberOfRegisters, TR::CodeGenerator *cg = NULL)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister())
            {
            _dependencies[i].getRegister()->block();
            }
         }
      }

   void unblockRegisters(uint32_t numberOfRegisters, TR::CodeGenerator *cg = NULL)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         if (_dependencies[i].getRegister())
            {
            _dependencies[i].getRegister()->unblock();
            }
         }
      }

   void stopUsingDepRegs(uint32_t numberOfRegisters, TR::Register *ret1, TR::Register *ret2, TR::CodeGenerator *cg)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
         {
         auto depReg = _dependencies[i].getRegister();
         if (depReg != ret1 && depReg != ret2)
            {
            cg->stopUsingRegister(depReg);
            }
         }
      }

   void stopUsingDepRegs(uint32_t numberOfRegisters, int numRetReg, TR::Register **retReg, TR::CodeGenerator *cg)
      {
      for (auto i = 0U; i < numberOfRegisters; ++i)
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

   protected:

#if defined(OMR_ARCH_S390)
   int8_t _numUses;
#elif defined(__GNUC__) && !defined(__clang__) /* defined(OMR_ARCH_S390) */
   /* a flexible array cannot be the only member of a class */
   private:
   char _unused;
   protected:
#endif /* defined(OMR_ARCH_S390) */

#if defined(__GNUC__) && !defined(__clang__)
   TR::RegisterDependency _dependencies[];
#else /* defined(__GNUC__) && !defined(__clang__) */
   /*
    * Only GCC appears to support extending a classs with a flexible array member,
    * so, for other compilers, we declare the length to be 1 and adjust accordingly
    * in the new operator.
    */
   private:
   static const size_t NUM_DEFAULT_DEPENDENCIES = 1;
   protected:
   TR::RegisterDependency _dependencies[NUM_DEFAULT_DEPENDENCIES];
#endif /* defined(__GNUC__) && !defined(__clang__) */
   };

class RegisterDependencyConditions
   {
   protected:
   RegisterDependencyConditions() {}

   public:
   TR_ALLOC(TR_Memory::RegisterDependencyConditions)

   };

/** \brief
 *     Maps real register numbers to dependencies / virtuals to help speed up register dependency queries.
 *
 *     For a given point in time during register allocation this utility is able to answer the following types of
 *     questions in O(1) time which are commonly queried during dependency assignment:
 *
 *     1. For a given real register R return the dependency whose target (real register) is R
 *     2. For a given real register R return the dependency whose source (virtual register) is currently assigned to R
 *
 *  \note
 *     This class does not support heap allocation and is currently meant to be used only as a stack allocated object.
 */
class RegisterDependencyMap
   {
   public:

   /** \brief
    *     Initializes the RegisterDependencyMap cache.
    *
    *  \param deps
    *     The dependency list which is cached inside this object.

    *  \param numDeps
    *     The number of dependencies inside \param deps used to validate that this map can be used.

    *  \note
    *     This constructor does not actually populate the cache. This is because it is very common to loop through the
    *     set of dependencies during register assignment already to calculate the total number of real register
    *     dependencies. As such to avoid having a whole separate loop to populate this cache we provide an
    *     \see addDependency API which populates the cache one dependency at a time. Use the aforementioned API to
    *     populate this cache and avoid having to loop over the dependency list multiple times.
    */
   RegisterDependencyMap(TR::RegisterDependency* deps, uint32_t numDeps)
      : deps(deps)
      {
      TR_ASSERT(numDeps <= SENTINEL, "Number of dependencies supplied (%d) cannot exceed %d!", numDeps, SENTINEL);

      for (auto i = 0; i < TR::RealRegister::NumRegisters; ++i)
         {
         realRegisterToTargetTable[i] = SENTINEL;
         realRegisterToSourceAssignedTable[i] = SENTINEL;
         }
      }

   /** \brief
    *     Adds a dependency to the cache.
    *
    *  \param dep
    *     The dependency to add.
    *
    *  \param index
    *     The index in the dependency list at which \param dep is located.
    */
   void addDependency(TR::RegisterDependency& dep, uint32_t index)
      {
      TR_ASSERT(&deps[index] == &dep, "Dependency pointer/index mismatch!");
      addDependency(dep.getRealRegister(), dep.getRegister()->getAssignedRealRegister(), index);
      }

   /** \brief
    *     Adds a dependency to the cache.
    *
    *  \param dep
    *     The dependency to add.
    *
    *  \param index
    *     The index in the dependency list at which \param dep is located.
    */
   void addDependency(TR::RegisterDependency* dep, uint32_t index)
      {
      TR_ASSERT(&deps[index] == dep, "Dependency pointer/index mismatch!");
      addDependency(dep->getRealRegister(), dep->getRegister()->getAssignedRealRegister(), index);
      }

   /** \brief
    *     Gets the dependency whose source (virtual register) is currently assigned to \param regNum.
    *
    *  \param regNum
    *     The real register target to look for.
    *
    *  \return
    *     The dependency whose source (virtual register) is currently assigned to \param regNum if such a dependency
    *     exists; <c>NULL</c> otherwise.
    */
   TR::RegisterDependency* getDependencyWithSourceAssigned(TR::RealRegister::RegNum regNum)
      {
      TR_ASSERT(regNum >= 0 && regNum < TR::RealRegister::NumRegisters, "Register number (%d) used as index but out of range!", regNum);
      return realRegisterToSourceAssignedTable[regNum] != SENTINEL ? &deps[realRegisterToSourceAssignedTable[regNum]] : NULL;
      }

   /** \brief
    *     Gets the dependency whose target (real register) is \param regNum.
    *
    *  \param regNum
    *     The real register target to look for.
    *
    *  \return
    *     The dependency whose target (real register) is \param regNum if such a dependency exists; <c>NULL</c>
    *     otherwise.
    */
   TR::RegisterDependency* getDependencyWithTarget(TR::RealRegister::RegNum regNum)
      {
      TR_ASSERT(regNum >= 0 && regNum < TR::RealRegister::NumRegisters, "Register number (%d) used as index but out of range!", regNum);
      TR_ASSERT(regNum != TR::RealRegister::NoReg, "Multiple dependencies can map to 'NoReg', can't return just one");
      TR_ASSERT(regNum != TR::RealRegister::SpilledReg, "Multiple dependencies can map to 'SpilledReg', can't return just one");
      return realRegisterToTargetTable[regNum] != SENTINEL ? &deps[realRegisterToTargetTable[regNum]] : NULL;
      }

   /** \brief
    *     Gets the source (virtual register) whose dependent target (real register) is \param regNum.
    *
    *  \param regNum
    *     The real register target to look for.
    *
    *  \return
    *     The source (virtual register) whose dependent target (real register) is \param regNum if such a dependency
    *     exists; <c>NULL</c> otherwise.
    */
   TR::Register* getSourceWithTarget(TR::RealRegister::RegNum regNum)
      {
      TR::RegisterDependency* dep = getDependencyWithTarget(regNum);
      return dep != NULL ? dep->getRegister() : NULL;
      }

   /** \brief
    *     Gets the index into the dependency list whose dependency has \param regNum as the target.
    *
    *  \param regNum
    *     The real register target to look for.
    *
    *  \return
    *     The index into the dependency list whose dependency has \param regNum. It is up to the caller to validate
    *     or assume that such a register dpendency exists.
    */
   uint16_t getTargetIndex(TR::RealRegister::RegNum regNum)
      {
      TR_ASSERT(realRegisterToTargetTable[regNum] != SENTINEL, "No such target register in dependency condition");
      return realRegisterToTargetTable[regNum];
      }

   private:

   void addDependency(TR::RealRegister::RegNum regNum, TR::RealRegister *assignedReg, uint32_t index)
      {
      if (regNum != TR::RealRegister::NoReg && regNum != TR::RealRegister::SpilledReg && regNum < TR::RealRegister::NumRegisters)
         {
         TR_ASSERT(regNum >= 0 && regNum < TR::RealRegister::NumRegisters, "Register number (%d) used as index but out of range!", regNum);
         TR_ASSERT(realRegisterToTargetTable[regNum] == SENTINEL || deps[realRegisterToTargetTable[regNum]].getRegister() == deps[index].getRegister(),
            "Multiple virtual registers depend on a single real register %d", regNum);
         realRegisterToTargetTable[regNum] = index;
         }

      if (assignedReg)
         {
         TR::RealRegister::RegNum arr = toRealRegister(assignedReg)->getRegisterNumber();
         TR_ASSERT(arr >= 0 && arr < TR::RealRegister::NumRegisters, "Register number (%d) used as index but out of range!", arr);
         TR_ASSERT(realRegisterToSourceAssignedTable[arr] == SENTINEL || deps[realRegisterToSourceAssignedTable[arr]].getRegister() == deps[index].getRegister(),
            "Multiple virtual registers assigned to a single real register %d", arr);
         realRegisterToSourceAssignedTable[arr] = index;
         }
      }

   private:

   static const uint16_t SENTINEL = 65535;

   TR::RegisterDependency *deps;
   uint16_t realRegisterToTargetTable[TR::RealRegister::NumRegisters];
   uint16_t realRegisterToSourceAssignedTable[TR::RealRegister::NumRegisters];
   };
}

#endif /* OMR_REGISTER_DEPENDENCY_INCL */
