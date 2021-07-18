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

#ifndef INLINING_METHOD_SUMMARY_INCL
#define INLINING_METHOD_SUMMARY_INCL

#include "optimizer/VPConstraint.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/abstractinterpreter/AbsValue.hpp"

namespace TR { class PotentialOptimizationPredicate; }

namespace TR {

/**
 * The Inlining Method Summary captures potential optimization opportunities of inlining one particular method 
 * and also specifies the constraints that are the maximal safe values to make the optimizations happen.
 */
class InliningMethodSummary
   {
   public:
   
   InliningMethodSummary(TR::Region& region) :
      _region(region),
      _optsByArg(region)
   {}

   /**
    * @brief calculate the total static benefits from a particular argument after inlining. 
    *
    * @param arg the argument
    * @param argPos the position of the argument
    * 
    * @return the total static benefit
    */
   uint32_t testArgument(TR::AbsValue* arg, uint32_t argPos);
   
   void trace(TR::Compilation* comp);

   void addPotentialOptimizationByArgument(TR::PotentialOptimizationPredicate* predicate, uint32_t argPos);

   private:

   TR::Region& region() { return _region; }

   typedef TR::vector<TR::PotentialOptimizationPredicate*, TR::Region&> PredicateContainer;
   TR::vector<PredicateContainer*, TR::Region&> _optsByArg;
   TR::Region &_region;
   };

class PotentialOptimizationPredicate
   {
   public:

   enum Kind
      {
      BranchFolding,
      NullCheckFolding,
      InstanceOfFolding,
      CheckCastFolding
      };

   PotentialOptimizationPredicate(uint32_t bytecodeIndex, TR::PotentialOptimizationPredicate::Kind kind) :
         _bytecodeIndex(bytecodeIndex),
         _kind(kind)
      {}

   virtual void trace(TR::Compilation* comp)=0; 
   
   /**
    * @brief Test whether the given value is a safe value given the optimization's constraint.
    *
    * @param value the value to be tested against the constraint
    * 
    * @return true if it is a safe value to unlock the optimization. false otherwise.
    */
   virtual bool test(TR::AbsValue* value)=0;

   const char* getName();
   uint32_t getBytecodeIndex() { return _bytecodeIndex; }

   protected:

   uint32_t _bytecodeIndex; 
   TR::PotentialOptimizationPredicate::Kind _kind;
   };

class PotentialOptimizationVPPredicate : public PotentialOptimizationPredicate
   {
   public:
   PotentialOptimizationVPPredicate(TR::VPConstraint* constraint, uint32_t bytecodeIndex, TR::PotentialOptimizationPredicate::Kind kind, TR::ValuePropagation* vp) :
         PotentialOptimizationPredicate(bytecodeIndex, kind),
         _constraint(constraint),
         _vp(vp)
      {}

   virtual bool test(TR::AbsValue *value);
   virtual void trace(TR::Compilation* comp);

   private:

   bool holdPartialOrderRelation(TR::VPConstraint* valueConstraint, TR::VPConstraint* testConstraint);

   TR::ValuePropagation* _vp; 
   TR::VPConstraint* _constraint;
   
   };
}

#endif
