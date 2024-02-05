/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#ifndef BENEFIT_INLINER_INCL
#define BENEFIT_INLINER_INCL

#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/abstractinterpreter/IDT.hpp"
#include "optimizer/abstractinterpreter/InliningProposal.hpp"

namespace TR {
class BenefitInlinerWrapper : public TR::Optimization
   {
   public:
   BenefitInlinerWrapper(TR::OptimizationManager* manager) : TR::Optimization(manager) {}

   static TR::Optimization* create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) BenefitInlinerWrapper(manager);
      }

   virtual int32_t perform();

   virtual const char * optDetailString() const throw()
      {
      return "O^O BENEFIT INLINER: ";
      }
   };

class BenefitInlinerBase : public TR_InlinerBase
   {
   protected:
   BenefitInlinerBase(TR::Optimizer* optimizer, TR::Optimization* optimization) :
         TR_InlinerBase(optimizer, optimization),
         _inliningProposal(NULL),
         _region(comp()->region()),
         _budget(getInliningBudget(comp()->getMethodSymbol())),
         _inliningDependencyTree(NULL),
         _nextIDTNodeToInlineInto(NULL)
      {}


   virtual bool inlineCallTargets(TR::ResolvedMethodSymbol* symbol, TR_CallStack* callStack, TR_InnerPreexistenceInfo* info);

   bool inlineIntoIDTNode(TR::ResolvedMethodSymbol *symbol, TR_CallStack *callStack, TR::IDTNode *idtNode);

   virtual bool exceedsSizeThreshold(TR_CallSite *callSite, int bytecodeSize, TR::Block * callNodeBlock, TR_ByteCodeInfo & bcInfo,  int32_t numLocals=0, TR_ResolvedMethod * caller = 0, TR_ResolvedMethod * calleeResolvedMethod = 0, TR::Node * callNode = 0, bool allConsts = false)
      {return false;}
   virtual bool analyzeCallSite(TR_CallStack * callStack, TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode, TR_CallTarget *calltargetToInline);

   TR::Region& region() { return _region; }

   TR::InliningProposal* _inliningProposal;

   protected:
   TR::Region _region;

   int32_t getInliningBudget(TR::ResolvedMethodSymbol* callerSymbol);

   uint32_t _budget;

   TR::IDT* _inliningDependencyTree;

   TR::IDTNode* _nextIDTNodeToInlineInto;
   };

class BenefitInliner : public BenefitInlinerBase
   {
   public:
   BenefitInliner(TR::Optimizer* optimizer, TR::Optimization* optimization) :
         BenefitInlinerBase(optimizer, optimization)
      {};

   void buildInliningDependencyTree();
   void inlinerPacking();
   };

}

#endif
