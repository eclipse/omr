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

#ifndef CATCHBLOCKREMOVER_INCL
#define CATCHBLOCKREMOVER_INCL

#include <stdint.h>
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"


/*
 * Class TR_CatchBlockRemover
 * ==========================
 *
 * Catch block removal is a simple optimization that aims to do two things
 *
 * 1) Eliminate catch blocks that have no incoming exception edges, i.e. the 
 * optimizer has proven that no exception thrown from any of its exception 
 * predecessors (usually blocks generated for the try region) can reach a 
 * particular catch block.
 * 
 * 2) Eliminate exception edges that cannot be taken, e.g. if a particular 
 * block had all its exception points removed by the optimizer then it can 
 * no longer throw anything; so all the exception edges going out from the 
 * block can be deleted. It also eliminates an exception edge if the possible 
 * exceptions are still not ones that can be caught by a particular catch block.
 * 
 * These transformations simplify the flow of control in general and make it 
 * simpler for later analyses to work.
 */

class TR_CatchBlockRemover : public TR::Optimization
   {
   public:
   TR_CatchBlockRemover(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CatchBlockRemover(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private :

   };

#endif
