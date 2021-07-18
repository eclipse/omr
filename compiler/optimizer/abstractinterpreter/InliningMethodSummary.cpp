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

#include "optimizer/abstractinterpreter/InliningMethodSummary.hpp"

uint32_t TR::InliningMethodSummary::testArgument(TR::AbsValue* arg, uint32_t argPos)
   {
   if (!arg)
      return 0;

   if (arg->isTop())
      return 0;

   if (_optsByArg.size() <= argPos || _optsByArg[argPos] == NULL || _optsByArg[argPos]->size() == 0)
      return 0;
   
   uint32_t benefit = 0;

   for (size_t i = 0; i < _optsByArg[argPos]->size(); i ++)
      {
      TR::PotentialOptimizationPredicate* predicate = (*_optsByArg[argPos])[i];
      if (predicate->test(arg))
         {
         benefit += 1;
         }
      }

   return benefit;
   }

void TR::InliningMethodSummary::trace(TR::Compilation* comp)
   {
   traceMsg(comp, "Inlining Method Summary:\n");

   if (_optsByArg.size() == 0)
      {
      traceMsg(comp, "EMPTY\n\n");
      return;
      }

   for (size_t i = 0; i < _optsByArg.size(); i ++) 
      {
      if (_optsByArg[i] != NULL)
         {
         for (size_t j = 0; j < _optsByArg[i]->size(); j ++)
            {
            TR::PotentialOptimizationPredicate* predicate = (*_optsByArg[i])[j];

            traceMsg(comp, "%s @%d for Argument %d ", predicate->getName(), predicate->getBytecodeIndex(), i);
            predicate->trace(comp);
            traceMsg(comp, "\n");
            }
         }
      }

   }


void TR::InliningMethodSummary::addPotentialOptimizationByArgument(TR::PotentialOptimizationPredicate* predicate, uint32_t argPos)
   {
   if (_optsByArg.size() <= argPos)
      _optsByArg.resize(argPos + 1);
      
   if (!_optsByArg[argPos])
       _optsByArg[argPos] = new (region()) PredicateContainer(region());
      
   _optsByArg[argPos]->push_back(predicate);
   }

const char* TR::PotentialOptimizationPredicate::getName()
   {
   switch (_kind)
      {
      case Kind::BranchFolding:
         return "Branch Folding";
      case Kind::CheckCastFolding:
         return "CheckCast Folding";
      case Kind::NullCheckFolding:
         return "NullCheck Folding";
      case Kind::InstanceOfFolding:
         return "InstanceOf Folding";
      default:
         TR_ASSERT_FATAL(false, "Unexpected Kind");
      }
   }

bool TR::PotentialOptimizationVPPredicate::holdPartialOrderRelation(TR::VPConstraint* valueConstraint, TR::VPConstraint* testConstraint)  
   {
   if (testConstraint->asIntConstraint()) // partial relation for int constraint
      {
      if (testConstraint->getLowInt() <= valueConstraint->getLowInt() && testConstraint->getHighInt() >= valueConstraint->getHighInt()) 
         return true;
      else 
         return false;
      }
   else if (testConstraint->asClassPresence()) // partial relation for nullness
      {
      if (testConstraint->isNonNullObject() && valueConstraint->isNonNullObject())
         return true;
      else if (testConstraint->isNullObject() && valueConstraint->isNullObject())
         return true;
      else 
         return false;
      }
   else if (testConstraint->asClassType()) // partial relation for class types
      {
      if (valueConstraint->isNonNullObject() && valueConstraint->getClass())
         {
         TR_YesNoMaybe yesNoMaybe = _vp->fe()->isInstanceOf(valueConstraint->getClass(), testConstraint->getClass(), valueConstraint->isFixedClass(), true);

         if (yesNoMaybe != TR_maybe)
            return true;
         else 
            return false;
         }
      else 
         return false;
      }

   return false;
   }

bool TR::PotentialOptimizationVPPredicate::test(TR::AbsValue *value)
   {
   if (value->isTop())
      return false;

   TR::AbsVPValue* vpValue = static_cast<TR::AbsVPValue*>(value);
   return holdPartialOrderRelation(vpValue->getConstraint(), _constraint);
   }

void TR::PotentialOptimizationVPPredicate::trace(TR::Compilation* comp)
   {
   traceMsg(comp, "Predicate Constraint: ");
   _constraint->print(_vp);
   }
   