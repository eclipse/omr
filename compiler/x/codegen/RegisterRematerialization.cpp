/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "x/codegen/RegisterRematerialization.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

namespace TR { class Instruction; }

// Only nodes that are referenced more than this number of times
// will be rematerializable.
//
#define REMATERIALIZATION_THRESHOLD 1

// These generateRematerializationInfo() methods return NULL
// if the necessary discardability conditions are not met.
//
static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
      TR::MemoryReference    *mr,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg);

static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
                                                               intptr_t                   constant,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg);

static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
                                                               TR::SymbolReference       *symRef,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg);

// Checks if the register associated with a load or a store (of autos,
// parms, and static and instance fields) is discardable, and if so,
// records information needed for its rematerialization.
//
void setDiscardableIfPossible(TR_RematerializableTypes  type,
                              TR::Register              *candidate,
                              TR::Node                  *node,
                              TR::Instruction           *instr,
                              TR::MemoryReference    *mr,
                              TR::CodeGenerator         *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_RematerializationInfo *info = generateRematerializationInfo(node, mr, type, instr, cg);

   if (info)
      {
      candidate->setRematerializationInfo(info);
      cg->addLiveDiscardableRegister(candidate);
      if (info->isIndirect())
         {
         cg->addDependentDiscardableRegister(candidate);

         if (debug("dumpRemat"))
            {
            diagnostic("---> Creating %s discardable register %s (type %d, base %s) "
                        "for node [%p] at instruction %p in %s\n",
                        info->toString(comp), candidate->getRegisterName(comp), type,
                        mr->getBaseRegister()->getRegisterName(comp),
                        node, instr, comp->signature());
            }
         }
      else
         {
         if (debug("dumpRemat"))
            {
            diagnostic("---> Creating %s discardable register %s (type %d) "
                        "for node [%p] at instruction %p in %s\n",
                        info->toString(comp), candidate->getRegisterName(comp), type,
                        node, instr, comp->signature());
            }
         }
      }
   }


// Checks if the register containing a constant is discardable,
// and if so, records information needed for its rematerialization.
//
void setDiscardableIfPossible(TR_RematerializableTypes  type,
                              TR::Register              *candidate,
                              TR::Node                  *node,
                              TR::Instruction           *instr,
                              intptr_t                 constant,
                              TR::CodeGenerator         *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_RematerializationInfo *info = generateRematerializationInfo(node, constant, type, instr, cg);

   if (info)
      {
      candidate->setRematerializationInfo(info);
      cg->addLiveDiscardableRegister(candidate);
      if (debug("dumpRemat"))
         {
         diagnostic("---> Creating %s discardable register %s (type %d) "
                     "for node [%p] at instruction %p in %s\n",
                     info->toString(comp), candidate->getRegisterName(comp), type,
                     node, instr, comp->signature());
         }
      }
   }


// Checks if the register containing a stack address (of a local object) is
// discardable, and if so, records information needed for its rematerialization.
//
void setDiscardableIfPossible(TR_RematerializableTypes  type,
                              TR::Register              *candidate,
                              TR::Node                  *node,
                              TR::Instruction           *instr,
                              TR::SymbolReference       *symRef,
                              TR::CodeGenerator         *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_RematerializationInfo *info = generateRematerializationInfo(node, symRef, type, instr, cg);

   if (info)
      {
      candidate->setRematerializationInfo(info);
      cg->addLiveDiscardableRegister(candidate);
      if (debug("dumpRemat"))
         {
         diagnostic("---> Creating %s discardable register %s (type %d) "
                     "for node [%p] at instruction %p in %s\n",
                     info->toString(comp), candidate->getRegisterName(comp), type,
                     node, instr, comp->signature());
         }
      }
   }


static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
      TR::MemoryReference    *mr,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg)
   {
   TR::Compilation *comp = cg->comp();
   bool                      isStore = node->getOpCode().isStore();
   TR::SymbolReference       *symRef  = &mr->getSymbolReference();
   TR::Symbol                 *sym     = symRef->getSymbol();
   TR_RematerializationInfo *info    = NULL;

   // If a loaded value is going to be used only a couple of times, do not
   // try to rematerialize it (we are not saving anything). On the other hand,
   // we always want to try rematerializing from stores (we may be saving one
   // or more spill stores).
   //
   if (!isStore && node->getReferenceCount() <= REMATERIALIZATION_THRESHOLD)
      return NULL;

   // We can only rematerialize floating point values if we are using XMMRs.
   //
   if (  (type == TR_RematerializableFloat || type == TR_RematerializableDouble)
      && !cg->supportsXMMRRematerialization())
      return NULL;

   if (symRef->isUnresolved() || sym->isVolatile())
      return NULL;

   if (cg->supportsLocalMemoryRematerialization() && sym->isAutoOrParm())
      {
      // Conservatively avoid rematerializing autos that are really fields
      // of local objects.
      if (node->getOpCode().isIndirect())
         return NULL;
      // A dependent discardable register can only depend on a base register.
      if (mr->getIndexRegister())
         return NULL;
      info = new (cg->trHeapMemory()) TR_RematerializationInfo(instr, type, symRef, NULL);
      }
   else if (cg->supportsStaticMemoryRematerialization() && sym->isStatic())
      {
      info = new (cg->trHeapMemory()) TR_RematerializationInfo(instr, type, symRef, NULL);
      }
   else if (cg->supportsIndirectMemoryRematerialization() && sym->isShadow())
      {
      TR::Node     *baseNode = mr->getBaseNode();
      TR::Register *baseReg  = mr->getBaseRegister();

#ifdef DEBUG
      TR_ASSERT(!baseReg || baseNode,
             "base register for memory rematerialization should be associated with a base node\n");

      TR::Register *baseNodeReg = baseNode->getRegister();

      TR_ASSERT(!baseReg || sym->isLocalObject() || baseNodeReg == baseReg,
             "node register of non-local object base node [%p] is %s; %s expected\n",
             baseNode, baseNodeReg ? baseNodeReg->getRegisterName(comp) : "null", baseReg->getRegisterName(comp));
#endif

      // A dependent discardable register can only depend on a base register.
      if (mr->getIndexRegister())
         return NULL;

      // The symbol being referenced must be resolved to avoid having
      // to insert UnresolvedDataSnippets when rematerializing.
      if (symRef->isUnresolved())
         return NULL;

      // The base register must still be live if we are to rematerialize a candidate with it.
      if (baseReg && baseNode->getReferenceCount() <= 1)
         return NULL;

      info = new (cg->trHeapMemory()) TR_RematerializationInfo(instr, type, symRef, baseReg);
      }

   if (info && isStore)
      info->setStore();

   return info;
   }


static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
                                                               intptr_t                 constant,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg)
   {
   if (node->getReferenceCount() <= REMATERIALIZATION_THRESHOLD)
      return NULL;

   if (!cg->supportsConstantRematerialization())
      return NULL;

   if (  (type == TR_RematerializableFloat || type == TR_RematerializableDouble)
      && !cg->supportsXMMRRematerialization())
      return NULL;

   return new (cg->trHeapMemory()) TR_RematerializationInfo(instr, type, constant);
   }


static TR_RematerializationInfo *generateRematerializationInfo(TR::Node                  *node,
                                                               TR::SymbolReference       *symRef,
                                                               TR_RematerializableTypes  type,
                                                               TR::Instruction           *instr,
                                                               TR::CodeGenerator         *cg)
   {
   TR::Symbol *sym = symRef->getSymbol();

   if (node->getReferenceCount() <= REMATERIALIZATION_THRESHOLD)
      return NULL;

   if (!cg->supportsAddressRematerialization())
      return NULL;

   if (sym->isLocalObject() || (sym->isStatic() && !symRef->isUnresolved()))
      return new (cg->trHeapMemory()) TR_RematerializationInfo(instr, type, symRef);

   return NULL;
   }
