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

#include "il/NodePool.hpp"

#include <stddef.h>
#include "compile/Compilation.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"

#define OPT_DETAILS_NODEPOOL "O^O NODEPOOL :"

TR::NodePool::NodePool(TR::Compilation * comp, const TR::Allocator &allocator) :
   _comp(comp),
   _disableGC(true),
   _globalIndex(0),
   _nodeRegion(comp->trMemory()->heapMemoryRegion())
   {
   }

void
TR::NodePool::cleanUp()
   {
   TR::Region::reset(_nodeRegion, _comp->trMemory()->heapMemoryRegion());
   }

TR::Node *
TR::NodePool::allocate()
   {
   TR::Node *newNode = static_cast<TR::Node*>(_nodeRegion.allocate(sizeof(TR::Node)));//_pool.ElementAt(poolIndex);
   memset(newNode, 0, sizeof(TR::Node));
   newNode->_globalIndex = ++_globalIndex;
   TR_ASSERT(_globalIndex < MAX_NODE_COUNT, "Reached TR::Node allocation limit");
   
   if (debug("traceNodePool"))
      {
      diagnostic("%sAllocating Node[%p] with Global Index %d\n", OPT_DETAILS_NODEPOOL, newNode, newNode->getGlobalIndex());
      }
   return newNode;
   }

bool
TR::NodePool::deallocate(TR::Node * node)
   {
   if (_disableGC)
      {
      if (debug("traceNodePool"))
         {
         diagnostic("%s Node garbage collection disabled", OPT_DETAILS_NODEPOOL);
         }
       return false;
      }

   node->~Node();
   return true;
   }

bool
TR::NodePool::removeDeadNodes()
   {
   if (_disableGC)
      {
      if (debug("traceNodePool"))
         {
         diagnostic("%s Node garbage collection disabled", OPT_DETAILS_NODEPOOL);
         }
       return false;
      }

   TR_ASSERT(false, "Node garbage collection is not currently implemented");

   return false;
   }
