/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include "il/NodePool.hpp"

#include <stddef.h>                 // for NULL
#include "compile/Compilation.hpp"  // for comp
#include "il/ILOps.hpp"             // for ILOpCode
#include "il/Node.hpp"              // for Node
#include "il/Node_inlines.hpp"      // for Node::getNodePoolIndex, etc
#include "infra/Assert.hpp"         // for TR_ASSERT

#define OPT_DETAILS_NODEPOOL "O^O NODEPOOL :"

void
TR::NodePool::cleanUp()
   {
   _pool.MakeEmpty();
   _optAttribPool.MakeEmpty();
   }

void
TR::NodePool::removeNode(NodeIndex poolIdx)
   {
   TR_ASSERT(_pool.Exists(poolIdx), "Node with Pool Index %d does not exist in the table", poolIdx);
   TR::Node &node = _pool.ElementAt(poolIdx);
   ncount_t globalIdx = node.getGlobalIndex();
   _pool.RemoveEntry(poolIdx);
   _optAttribPool.RemoveEntry(poolIdx);

   CS2::HashIndex symRefHid;
   if (debug("traceNodePool"))
      {
      diagnostic("%sRemoving Node[%p] with Global Index %d, Table Index %d\n", OPT_DETAILS_NODEPOOL, &node, globalIdx, poolIdx);
      }
   }

TR::Node *
TR::NodePool::getNodeAtPoolIndex(ncount_t poolIdx)
   {
   TR_ASSERT(_pool.Exists(poolIdx), "Node with Pool Index %d does not exist in the table", poolIdx);
   return &_pool.ElementAt(poolIdx);
   }

TR::Node *
TR::NodePool::allocate(ncount_t poolIndex)
   {
   if (!poolIndex)
      {
      // We are calling AddEntryNoConstruct instead of AddEntry
      // because TR::NodePool::allocate is only called from OMR::Node's
      // operator new, which means that AddEntry calls TR::Node's constructor
      // and then once TR::NodePool::allocate returns, operator new calls
      // a different TR::Node constructor resulting in TR::Node getting
      // constructed twice, increasing compilation time.
      poolIndex = _poolIndex = _pool.AddEntryNoConstruct();
      _globalIndex++;
      TR_ASSERT(_globalIndex < MAX_NODE_COUNT, "Reached TR::Node allocation limit");
      }
   TR::Node &newNode = _pool.ElementAt(poolIndex);
   if (debug("traceNodePool"))
      {
      diagnostic("%sAllocating Node[%p] with Global Index %d, Pool Index %d\n", OPT_DETAILS_NODEPOOL, &newNode, newNode.getGlobalIndex(), poolIndex);
      }
   return &newNode;
   }

OMR::Node::OptAttributes *
TR::NodePool::allocateOptAttributes()
   {
   // We are calling AddEntryAtPositionNoConstruct instead of AddEntryAtPosition
   // because TR::NodePool::allocateOptAttributes is only called from
   // OMR::Node::OptAttributes operator new, which means that AddEntryAtPosition
   // calls OMR::Node::OptAttributes' constructor and then once
   // TR::NodePool::allocateOptAttributes returns, operator new calls
   // OMR::Node::OptAttributes' constructor again, increasing compilation time.
   ncount_t optAttribPoolIndex = _optAttribPool.AddEntryAtPositionNoConstruct(_poolIndex);
   TR_ASSERT(optAttribPoolIndex == _poolIndex, "Cannot eliminate variable _optAttribPoolIndex and use _poolIndex instead. Was hoping this was possible");
   _optAttribGlobalIndex++;
   OMR::Node::OptAttributes & oa = _optAttribPool.ElementAt(_poolIndex);
   return &oa;
   }

void
TR::NodePool::FreeOptAttributes()
   {
   // Free all Opt Attributes
   if (debug("traceNodePool"))
      {
      traceMsg(TR::comp(), "TR::NodePool::FreeOptAttributes: freeing %lu bytes\n", _optAttribPool.MemoryUsage());
      }
   _optAttribPool.MakeEmpty();

   // Iterate over all Nodes, NULLing out the ptr
   NodeIter nodePoolIdx(_pool);
   for (nodePoolIdx.SetToFirst(); nodePoolIdx.Valid(); nodePoolIdx.SetToNext())
      {
      TR::Node &node = _pool.ElementAt(nodePoolIdx);
      node._optAttributes = NULL;
      }
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

   removeNode(node->getNodePoolIndex());
   return true;
   }

void
TR::NodePool::removeNodeAndReduceGlobalIndex(TR::Node * node)
   {
   removeNode(node->getNodePoolIndex());
   _globalIndex--;
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

   bool foundDeadNode = false;
   NodeIter nodePoolIdx(_pool);
   for (nodePoolIdx.SetToFirst(); nodePoolIdx.Valid(); nodePoolIdx.SetToNext())
       {
       TR::Node &node = _pool.ElementAt(nodePoolIdx);
       if (!(node.getOpCode().isTreeTop() || node.getReferenceCount() > 0))
          {
          removeNode(nodePoolIdx);
          foundDeadNode = true;
          }
       }

   return foundDeadNode;
   }

