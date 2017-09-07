/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef REORDER_INDEX_EXPR_INCL
#define REORDER_INDEX_EXPR_INCL

#include <stdint.h>                           // for int32_t
#include "il/Node.hpp"                        // for vcount_t
#include "optimizer/Optimization.hpp"         // for Optimization
#include "optimizer/OptimizationManager.hpp"  // for OptimizationManager

class TR_PrimaryInductionVariable;
class TR_Structure;
namespace TR { class Node; }

class TR_IndexExprManipulator: public TR::Optimization
   {
   public:
   TR_IndexExprManipulator(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_IndexExprManipulator(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   void rewriteIndexExpression(TR_Structure *);

   private:
   void rewriteIndexExpression(TR_PrimaryInductionVariable *primeIV,TR::Node *parent,TR::Node *node,bool parentIsAiadd);
   vcount_t _visitCount;
   bool    _somethingChanged;
   bool    _debug;

};

#endif
