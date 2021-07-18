/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef IDT_NODE_INCL
#define IDT_NODE_INCL

#include "env/Region.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/abstractinterpreter/InliningMethodSummary.hpp"

namespace TR { class IDTNode; }

namespace TR {

/** 
 * IDTNode is the abstract representation of a candidate method containing useful information for inlining. 
 */
class IDTNode
   {
   public:
   IDTNode(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      uint32_t byteCodeIndex, 
      float callRatio,
      TR::IDTNode *parent,
      uint32_t budget);

   /**
    * @brief Add a child
    * 
    * @param idx the global index of the child to be added
    * @param callTarget the call target
    * @param symbol the method symbol
    * @param byteCodeIndex the call site bytecode index
    * @param callRatio call ratio of the method
    * @param region the region where the child node will be allocated
    * 
    * @return the newly created node
    */
   TR::IDTNode* addChild(
      int32_t idx,
      TR_CallTarget* callTarget,
      TR::ResolvedMethodSymbol* symbol,
      uint32_t byteCodeIndex, 
      float callRatio, 
      TR::Region& region);

   TR::InliningMethodSummary* getInliningMethodSummary() { return _inliningMethodSummary; }
   void setInliningMethodSummary(TR::InliningMethodSummary* inliningMethodSummary) { _inliningMethodSummary = inliningMethodSummary; }

   const char* getName(TR_Memory* mem) { return _symbol->signature(mem); }

   TR::IDTNode *getParent() { return _parent; }

   int32_t getGlobalIndex() { return _idx; }
   int32_t getParentGloablIndex()  { return isRoot() ? -2 : getParent()->getGlobalIndex(); }

   double getBenefit();

   uint32_t getStaticBenefit() { return _staticBenefit; };

   void setStaticBenefit(uint32_t staticBenefit) { _staticBenefit = staticBenefit; }
   
   uint32_t getNumDescendants();

   uint32_t getCost() { return isRoot() ? 0 : getByteCodeSize(); }
   uint32_t getRecursiveCost();

   uint32_t getNumChildren();
   TR::IDTNode *getChild(uint32_t index);

   bool isRoot()  { return _parent == NULL; };

   TR::IDTNode* findChildWithBytecodeIndex(uint32_t bcIndex);

   TR::ResolvedMethodSymbol* getResolvedMethodSymbol() { return _symbol; }
   TR_ResolvedMethod* getResolvedMethod() { return _callTarget->_calleeMethod; }
   
   uint32_t getBudget()  { return _budget; };

   TR_CallTarget *getCallTarget() { return _callTarget; }

   uint32_t getByteCodeIndex() { return _byteCodeIndex; }
   uint32_t getByteCodeSize() { return _callTarget->_calleeMethod->maxBytecodeIndex(); }

   float getCallRatio() { return _callRatio; }
   double getRootCallRatio() { return _rootCallRatio; }

   private:
   TR_CallTarget* _callTarget;
   TR::ResolvedMethodSymbol* _symbol;

   TR::IDTNode *_parent;

   int32_t _idx;
   uint32_t _byteCodeIndex;
   
   TR::vector<TR::IDTNode*, TR::Region&>* _children; 
   uint32_t _staticBenefit;

   uint32_t _budget;

   float _callRatio;
   double _rootCallRatio;

   TR::InliningMethodSummary *_inliningMethodSummary;
   
   /**
    * @brief It is common that a large number of IDTNodes have only one child.
    * So instead of allocating a deque containing only one element, 
    * we use the _children as the address of that only child to save memory usage.
    * 
    * @return NULL if there are more than one or no children. The child node if there is only one child.
    */
   TR::IDTNode* getOnlyChild();

   void setOnlyChild(TR::IDTNode* child);
   };
}

#endif
