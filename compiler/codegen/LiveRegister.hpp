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

#ifndef LIVEREGISTER_INCL
#define LIVEREGISTER_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class RealRegister; }
namespace TR { class Register; }
namespace TR { class RegisterPair; }

// Live register information, used to determine live range information for
// virtual registers
//
class TR_LiveRegisterInfo
   {

public:

   TR_ALLOC(TR_Memory::LiveRegisterInfo)

   TR_LiveRegisterInfo(TR::Compilation *compilation) :
      _compilation(compilation)
      {}

   void initialize(TR::Register *reg)
      {
      _register = reg;
      _node = NULL;
      _numOwningNodes = 0;
      _interference = 0;
      _association = 0;
      }

   int32_t getNodeCount()          { return _numOwningNodes; }
   void    setNodeCount(int32_t n) { _numOwningNodes = n; }
   int32_t incNodeCount()          { return ++_numOwningNodes; }
   int32_t decNodeCount()          { TR_ASSERT(_numOwningNodes > 0, "Bad node count"); return --_numOwningNodes; }

#if defined(TR_TARGET_POWER)
   uint64_t getAssociation() { return _association; }
#else
   uint32_t getAssociation() { return _association; }
#endif

#if defined(TR_TARGET_POWER)
   uint64_t setAssociation(uint64_t realRegMask, TR::Compilation * c)
#else
   uint32_t setAssociation(TR_RegisterMask realRegMask, TR::Compilation * c)
#endif
      {
      if (c->target().cpu.isX86())
         _association &= 0x80000000;
      else
         _association = 0;

      if (realRegMask)
         _association |= realRegMask;

      return _association;
      }

#if defined(TR_TARGET_POWER)
   uint64_t removeAssociation(TR::Compilation * compilation, TR_RegisterMask realRegMask)
#else
   uint32_t removeAssociation(TR::Compilation * compilation, TR_RegisterMask realRegMask)
#endif
      {
      if (realRegMask && (_association & realRegMask))
         {
         if (compilation->target().cpu.isX86())
            _association &= 0x80000000;
         else
            _association = 0;
         }
      return _association;
      }

   void addByteRegisterAssociation() {_association |= 0x80000000;}

   TR_LiveRegisterInfo *getNext() { return _next; }
   TR_LiveRegisterInfo *getPrev() { return _prev; }

   void setNext(TR_LiveRegisterInfo *n) { _next = n; }
   void setPrev(TR_LiveRegisterInfo *p) { _prev = p; }

   TR::Register *getRegister() { return _register; }
   TR::Node     *getNode()     { return _node; }

   void setNode(TR::Node *n)
      {
      _node = n;
      }

   void addToList(TR_LiveRegisterInfo *&head)
      {
      _next = head;
      _prev = NULL;
      if (head)
         head->_prev = this;
      head = this;
      }

   void removeFromList(TR_LiveRegisterInfo *&head)
      {
      if (_prev)
         _prev->_next = _next;
      else
         head = _next;
      if (_next)
         _next->_prev = _prev;
      }

#if defined(TR_TARGET_POWER)
   uint64_t getInterference()         { return _interference; }
   void addInterference(uint64_t i)   { _interference |= i; }
#else
   uint32_t getInterference()         { return _interference; }
   void addInterference(uint32_t i)   { _interference |= i; }
#endif
   void addByteRegisterInterference() { if (!(_association & 0x80000000)) _interference |= 0x80000000; }

   TR::Compilation * comp()           { return _compilation; }
   TR::CodeGenerator * cg()           { return _compilation->cg(); }
   TR_Debug * getDebug()              { return comp()->getDebug(); }

private:

   TR_LiveRegisterInfo *_prev;           // Local live register chain
   TR_LiveRegisterInfo *_next;

   TR::Compilation *    _compilation;    // For debugging
   TR::Register *       _register;       // For debugging
   TR::Node *           _node;           // For debugging

#if defined(TR_TARGET_POWER)
   uint64_t             _interference;    // Real registers that interfere with this register
#else
   uint32_t             _interference;    // Real registers that interfere with this register
#endif

#if defined(TR_TARGET_POWER)
   uint64_t             _association;     // The real register associated with this register
#else
   uint32_t             _association;     // The real register associated with this register
#endif

   int32_t              _numOwningNodes;  // # of nodes holding this register

   };

// Information about live virtual registers
//
class TR_LiveRegisters
   {

public:

   TR_ALLOC(TR_Memory::LiveRegisters)

   TR_LiveRegisters(TR::Compilation *comp) :
      _compilation(comp),
      _head(NULL),
      _pool(NULL),
      _numLiveRegisters(0)
      {}

   TR_LiveRegisterInfo *addRegister(TR::Register *reg, bool updateInterferences = true);
   TR_LiveRegisterInfo *addRegisterPair(TR::RegisterPair *reg);

   void setAssociation(TR::Register *reg, TR::RealRegister *realReg);
   void setByteRegisterAssociation(TR::Register *reg);

   void stopUsingRegister(TR::Register *reg);
   void registerIsDead(TR::Register *reg, bool updateInterferences = true);
   void removeRegisterFromLiveList(TR::Register *reg);

   static void moveRegToList(TR_LiveRegisters* from, TR_LiveRegisters* to, TR::Register *reg);

   TR_LiveRegisterInfo *getFirstLiveRegister() { return _head; }

   int32_t getNumberOfLiveRegisters() { return _numLiveRegisters; }
   int32_t decNumberOfLiveRegisters() { return _numLiveRegisters--; }

   TR::Compilation * comp() { return _compilation; }
   TR::CodeGenerator * cg() { return _compilation->cg(); }
   TR_Debug * getDebug()    { return _compilation->getDebug(); }

private:

   TR::Compilation      *_compilation;
   TR_LiveRegisterInfo *_head;
   TR_LiveRegisterInfo *_pool;
   int32_t              _numLiveRegisters;

   };

#endif
