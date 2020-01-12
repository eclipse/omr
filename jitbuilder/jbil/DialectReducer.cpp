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

#include "DialectReducer.hpp"
#include "Builder.hpp"
#include "FunctionBuilder.hpp"
#include "Operation.hpp"
#include "Type.hpp"
#include "Value.hpp"

using namespace OMR::JitBuilder;


DialectReducer::DialectReducer(FunctionBuilder * fb, Dialect target)
   : Transformer(fb)
   , _target(target)
   {
   setTraceEnabled(fb->config()->traceReducer());
   }

Builder *
DialectReducer::transformOperation(Operation * op)
   {
   switch (op->action())
      {
      case aForLoop :
         {
         // ForLoop is dialect jbil so nothing to do unless target is lower than jbil
         if (_target >= DIALECT(jbil))
            return NULL;

         bool countsUp = (bool) op->literal(0)->getInt8();
         std::string indVar = op->literal(1)->getString();
         Builder *loopCode = op->builder(0);
         Builder *providedLoopContinue = op->builder(1);
         Builder *breakBuilder = op->builder(2);
         Value *initial = op->operand(0);
         assert(initial);
         Value *end = op->operand(1);
         assert(end);
         Value *increment = op->operand(2);
         assert(increment);

         // ForLoop sets isTarget to true, so clear it now and the reduction should
         //  cause it to be appropriate set (or left) depending on the final structure
         loopCode->setTarget(false);

         // Also make loopCode unbound, since it will no longer be bound by a for loop operation
         loopCode->setBoundness(May)->setBound(false);
         if (providedLoopContinue)
            providedLoopContinue->setBoundness(May)->setBound(false);
         if (breakBuilder)
            breakBuilder->setBoundness(May)->setBound(false);

         Builder * parent = op->parent();
         Builder * b = parent->OrphanBuilder();
         Builder * loopBody = b->OrphanBuilder();
         Builder * loopContinue = (providedLoopContinue != NULL) ? providedLoopContinue : b->OrphanBuilder();
         Builder * loopExit = b->OrphanBuilder();

         b->Store(indVar, initial);

         if (countsUp)
            b->IfCmpGreaterOrEqual(loopExit, b->Load(indVar), end);
         else
            b->IfCmpLessOrEqual(loopExit, b->Load(indVar), end);

         if (countsUp)
            {
            loopContinue->Store(indVar,
            loopContinue->   Add(
            loopContinue->      Load(indVar),
                                increment));
            loopContinue->IfCmpLessThan(loopBody, 
            loopContinue->   Load(indVar),
                             end);
            }
         else
            {
            loopContinue->Store(indVar,
            loopContinue->   Sub(
            loopContinue->      Load(indVar),
                                increment));
            loopContinue->IfCmpGreaterThan(loopBody, 
            loopContinue->   Load(indVar),
                             end);
            }

         appendOrInline(loopBody, loopCode);
         appendOrInline(loopBody, loopContinue);

         b->AppendBuilder(loopBody);

         if (breakBuilder)
            appendOrInline(b, breakBuilder);

         b->AppendBuilder(loopExit);

         return b;
         }

      default :
         break;
      }

   return NULL;
   }
