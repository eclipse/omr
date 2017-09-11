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

#ifndef DEADSTOR_INCL
#define DEADSTOR_INCL

#include "optimizer/OptimizationManager.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"

namespace TR { class Optimization; }

// Dead Store Elimination
//
// Uses the information computed by ReachingDefinitions (stored in
// UseDefInfo) to perform the transformation. If a store (def) node has no
// uses then remove the node from the IL trees. Usually useful
// after doing copy propagation.
//
// This class is derived from Isolated Store Elimination, which uses the use/def
// information if it is available and uses a tree walk if it isn't. When entered
// through this class there will always be use/def information unless it can't be
// built for size reasons.
//


class TR_DeadStoreElimination : public TR_IsolatedStoreElimination
   {
   public:

   // Performs dead store elimination using the
   // use/def values of relevant nodes.
   //
   TR_DeadStoreElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_DeadStoreElimination(manager);
      }

   virtual const char * optDetailString() const throw();
   };

#endif
