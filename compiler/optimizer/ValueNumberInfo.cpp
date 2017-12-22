/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include "optimizer/ValueNumberInfo.hpp"

#include <stdint.h>                            // for int32_t, uint16_t, etc
#include <string.h>                            // for NULL, memset
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "compile/Compilation.hpp"             // for Compilation, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"
#include "cs2/hashtab.h"                       // for HashIndex
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                    // for TR_Memory, etc
#include "env/jittypes.h"                      // for intptrj_t
#include "il/DataTypes.hpp"                    // for TR::DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node, vcount_t
#include "il/Node_inlines.hpp"                 // for Node::getChild, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/ParameterSymbol.hpp"       // for ParameterSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"                     // for TR_Array
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector, etc
#include "infra/List.hpp"                      // for ListIterator, etc
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/UseDefInfo.hpp"            // for TR_UseDefInfo, etc
#include "ras/Debug.hpp"                       // for TR_DebugBase

TR_ValueNumberInfo::TR_ValueNumberInfo(TR::Compilation *comp)
   : _compilation(comp),
     _nodes(comp->allocator()),
     _valueNumbers(comp->allocator()),
     _nextInRing(comp->allocator())
{}

TR_ValueNumberInfo::TR_ValueNumberInfo(TR::Compilation *comp, TR::Optimizer *optimizer, bool requiresGlobals, bool prefersGlobals, bool noUseDefInfo)
   : _compilation(comp),
     _optimizer(optimizer),
     _trace(comp->getOption(TR_TraceValueNumbers)),
     _nodes(comp->allocator()),
     _valueNumbers(comp->allocator()),
     _nextInRing(comp->allocator())
   {
   dumpOptDetails(comp, "PREPARTITION VN   (Building value number info)\n");

   // For now, don't allow global value numbering because of
   // threads-changing-fields problems.
   //
   TR_ASSERT(!(requiresGlobals || prefersGlobals), "Don't do global value numbering");

   if (trace())
      traceMsg(comp, "Starting ValueNumbering %s\n", noUseDefInfo ? "without UseDefInfo" : "");

   if (noUseDefInfo)
      {
      _useDefInfo = NULL;
      }
    else
      {
      // Make sure we have valid use/def info
      //
      _useDefInfo = _optimizer->getUseDefInfo();
      if (_useDefInfo && requiresGlobals && !_useDefInfo->hasGlobalsUseDefs())
         _useDefInfo = NULL;
      if (_useDefInfo == NULL && !_optimizer->cantBuildLocalsUseDefInfo())
         {
         if (!(requiresGlobals && _optimizer->cantBuildGlobalsUseDefInfo()))
            {
            TR::LexicalMemProfiler mp("use defs (value numbers - I)", comp->phaseMemProfiler());

            _useDefInfo = new (comp->allocator()) TR_UseDefInfo(comp, comp->getFlowGraph(), optimizer, requiresGlobals, prefersGlobals);
            if (_useDefInfo->infoIsValid())
               {
               _optimizer->setUseDefInfo(_useDefInfo);
               }
            else
               {
               // release storage for failed _useDefInfo
               delete _useDefInfo;
               _useDefInfo = NULL;
               }
            }
         }

      if (_useDefInfo == NULL)
         {
         if (trace())
            traceMsg(comp, "Can't perform ValueNumbering, no use/def info\n");
         _infoIsValid = false;
         _optimizer->setCantBuildGlobalsValueNumberInfo(true);
         if (!requiresGlobals)
            _optimizer->setCantBuildLocalsValueNumberInfo(true);
         return;
         }

      _useDefInfo->buildDefUseInfo(true);
      }
   _infoIsValid = true;

   _hasGlobalsValueNumbers = requiresGlobals;
   _numberOfNodes = comp->getNodeCount();

   TR::TreeTop *treeTop;

   if (trace())
      {
      traceMsg(comp, "\nTrees for value numbering\n\n");
      comp->incVisitCount();
      for (treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         {
         comp->getDebug()->print(comp->getOutFile(), treeTop);
         }
      traceMsg(comp, "\n\n");
      }

   _nodes.GrowTo(_numberOfNodes);
   _valueNumbers.GrowTo(_numberOfNodes);
   _nextInRing.GrowTo(_numberOfNodes);

   // Any stack allocations past this point will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // Allocate hash table for nodes
   //
   _hashTable._numBuckets = 1023;
   _hashTable._buckets = (CollisionEntry**)trMemory()->allocateStackMemory(_hashTable._numBuckets*sizeof(CollisionEntry*), TR_Memory::ValueNumberInfo);
   memset(_hashTable._buckets, 0, _hashTable._numBuckets*sizeof(CollisionEntry*));
   _matchingNodes = new (trStackMemory()) TR_Array<CollisionEntry*>(trMemory(), _numberOfNodes, false, stackAlloc);
   _matchingNodes->setSize(_numberOfNodes);

   buildValueNumberInfo();

   int32_t i;
   if (trace())
      {
      // Show shared value numbers
      //
      TR_BitVector traced(_numberOfNodes,trMemory(), stackAlloc);
      for (i = 0; i < _numberOfNodes; i++)
         {
         TR::Node *node = getNode(i);
         if (node == NULL || traced.get(node->getGlobalIndex()))
            continue;
         if (getNext(node) == node)
            continue;
         traceMsg(comp, "   Nodes sharing value number %d:", getValueNumber(node));
         TR::Node *next = node;
         do
            {
            traced.set(next->getGlobalIndex());
            traceMsg(comp, " %d", next->getGlobalIndex());
            next = getNext(next);
            } while(next != node);
         traceMsg(comp, "\n");
         }

      traceMsg(comp, "\nEnding ValueNumbering\n");

      // Get hash table statistics
      //
      int32_t          numEntries = 0;
      int32_t          numBucketsUsed = 0;
      int32_t          maxBucketSize = 0;
      for (i = _hashTable._numBuckets-1; i >= 0; i--)
         {
         if (_hashTable._buckets[i] != NULL)
            {
            numBucketsUsed++;
            int32_t numInBucket = 0;
            for (CollisionEntry *entry = _hashTable._buckets[i]; entry; entry = entry->_next)
               for (NodeEntry *nodeEntry = entry->_nodes; nodeEntry; nodeEntry = nodeEntry->_next)
                  numInBucket++;
            numEntries += numInBucket;
            if (numInBucket > maxBucketSize)
               maxBucketSize = numInBucket;
            }
         }
      traceMsg(comp, "   HashTable entries = %d, buckets used = %d, max bucket size = %d\n",numEntries,numBucketsUsed,maxBucketSize);
      }

   if (trace())
      {
      traceMsg(comp, "\n\nValue Number Table\n\n");
      for (i = 0; i < _numberOfNodes; i++)
         {
         TR::Node *node = getNode(i);
         if (node == NULL)
            continue;
         traceMsg(comp, "node %4d [%p] has value number %4d", i, node, getValueNumber(node));
         if (getNext(node) != node)
            {
            traceMsg(comp, ", shared with ");
            for (TR::Node *next = getNext(node); next != node; next = getNext(next))
               traceMsg(comp, " %d", next->getGlobalIndex());
            }
         traceMsg(comp, "\n");
         }
      }

   }

void TR_ValueNumberInfo::buildValueNumberInfo()
   {
      // Allocate parameter entry value numbers starting at 1. Parameters are given
      // the first value numbers so that their value number on entry can be
      // derived from the parameter ordinal value.
      //
      _nextValue = 1;
      allocateParmValueNumbers();

      // Initialize value number information.
      // During the process non-shareable nodes are each given a unique negative
      // value, except those that are also def points - these value numbers can
      // actually be shared with use points so they have to be in the range of
      // shareable value numbers.
      //
      int32_t negativeValueNumber = -3;
      TR::TreeTop * treeTop;
      for (treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         {
         initializeNode(treeTop->getNode(), negativeValueNumber);
         }

      // Allocate shareable value numbers, using the next available (positive)
      // value numbers. The next available value number after this is ldone gives
      // the number of shareable value numbers.
      //
      allocateShareableValueNumbers();
      _numberOfShareableValues = _nextValue;

      // Reassign value numbers for non-shareable nodes.
      // This gives shareable nodes the lowest value numbers, followed by
      // non-shareable node.
      //
      allocateNonShareableValueNumbers();

   }

bool TR_ValueNumberInfo::congruentNodes(TR::Node * node, TR::Node * entryNode)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->getOpCode().isSetSignOnNode() && node->getSetSign() != entryNode->getSetSign())
      return false;

   if (node->getType().isBCD())
       {
       if (!node->isDecimalSizeAndShapeEquivalent(entryNode))
          {
          if (trace())
             traceMsg(comp(),"BCD node %s (%p) and BCD entryNode %s (%p) have size/shape mismatch -- do not consider as matching\n",
                node->getOpCode().getName(),node,
                entryNode->getOpCode().getName(),entryNode);
          return false;
          }

       if (!node->isSignStateEquivalent(entryNode))
          {
          if (trace() || comp()->cg()->traceBCDCodeGen())
             traceMsg(comp(),"x^x : BCD node %s (%p) and BCD entryNode %s (%p) have sign state mismatch -- do not consider as matching\n",
                node->getOpCode().getName(),node,
                entryNode->getOpCode().getName(),entryNode);
          return false;
          }
       }
   else if (node->getOpCode().isConversionWithFraction() &&
             node->getDecimalFraction() != entryNode->getDecimalFraction())
       {
       if (trace())
          traceMsg(comp(),"fracConv node %s (%p) and fracConv entryNode %s (%p) have fraction mismatch -- do not consider as matching\n",
             node->getOpCode().getName(),node,
             entryNode->getOpCode().getName(),entryNode);
       return false;
       }
   else if (node->chkOpsCastedToBCD() &&
            node->castedToBCD() != entryNode->castedToBCD())
      {
      if (trace())
         traceMsg(comp(),"castedToBCD mismatch : node %s (%p) castedToBCD %d and entryNode %s (%p) castedToBCD %d -- do not consider as matching\n",
            node->getOpCode().getName(),node,node->castedToBCD(),entryNode->getOpCode().getName(),entryNode,entryNode->castedToBCD());
      return false;
      }
   else if (node->getType().isDFP() && node->getOpCode().isModifyPrecision() && node->getDFPPrecision() != entryNode->getDFPPrecision())
      {
      return false;
      }
#endif

    // Check for loads of constants.  They're much like isLoadConst.
    //

    if (  node->getOpCode().isLoadVar()
       && node->getSymbolReference() == entryNode->getSymbolReference()
       && (  node->getSymbol()->isConst()
          || node->getSymbol()->isConstObjectRef()))
       {
       return true;
       }

    /*
    // Share chains for loads and stores, since they won't be matched with
    // other nodes in the chain.
    //

    if (node->getOpCode().isStore() ||
        node->getOpCode().isLoadVar())
       return true;
    */

    if (node->getOpCode().hasSymbolReference())
       {
       TR::SymbolReference *symRef = node->getSymbolReference();
       TR::SymbolReference *entrySymRef = entryNode->getSymbolReference();
       if (symRef && entrySymRef &&
           symRef->getSymbol() == entrySymRef->getSymbol() &&
           symRef->getOffset() == entrySymRef->getOffset())
          {
          uint16_t udIndex = node->getUseDefIndex();
          uint16_t entryUdIndex = entryNode->getUseDefIndex();
          if (!_useDefInfo || !_useDefInfo->isUseIndex(udIndex))
             return true;

          TR_ASSERT(_useDefInfo->isUseIndex(entryUdIndex), "Expecting use index");
          //
          // In rare cases when Jsrs exist in the method, use def info could be NULL
          //
          TR_UseDefInfo::BitVector udef(comp()->allocator());
          TR_UseDefInfo::BitVector entryudef(comp()->allocator());
          if (_useDefInfo->getUseDef(udef, udIndex) && _useDefInfo->getUseDef(entryudef, entryUdIndex))
             {
             return (udef == entryudef);
             }
          else
             {
             if (trace())
               TR_ASSERT(0, "No defs for a use\n");
             }
          }
       }

    if (node->getOpCode().isLoadConst())
       {
       bool isSame = false;
       switch (node->getDataType())
          {
          case TR::Int8: isSame = (node->getByte() == entryNode->getByte()); break;
          case TR::Int16: isSame = (node->getShortInt() == entryNode->getShortInt()); break;
          case TR::Int32: isSame = (node->getInt() == entryNode->getInt()); break;
          case TR::Float: isSame = (node->getFloatBits() == entryNode->getFloatBits()); break;
          case TR::Int64:
          case TR::Double: isSame = (node->getLongInt() == entryNode->getLongInt()); break;
#ifdef J9_PROJECT_SPECIFIC
          case TR::DecimalFloat: isSame = (node->getInt() == entryNode->getInt()); break;
          case TR::DecimalDouble: isSame = (node->getLongInt() == entryNode->getLongInt()); break;
#ifdef SUPPORT_DFP
          case TR::DecimalLongDouble: isSame = (node->getLongDouble() == entryNode->getLongDouble()); break;
#endif
#endif
          case TR::Address:isSame = (node->getAddress() == entryNode->getAddress()); break;
          default:
             {
             if (
#ifdef J9_PROJECT_SPECIFIC
                 node->getType().isBCD() ||
#endif
                 node->getType().isAggregate())
                {
                isSame = _optimizer->areNodesEquivalent(node, entryNode);
                }
             else
                {
                isSame = false;
                }
             }
          }
         return isSame;
       }
    else if (node->getOpCode().isArrayLength())
       {
       int32_t nodeStride = node->getArrayStride();
       int32_t entryNodeStride = entryNode->getArrayStride();
       return(nodeStride == entryNodeStride);
       }
    else
       return true;
   }

void TR_ValueNumberInfo::initializeNode(TR::Node *node, int32_t &negativeValueNumber)
   {
   int32_t index = node->getGlobalIndex();
   if (_nodes.ElementAt(index) != NULL)
      {
      // Node has already been initialized
      //
      TR_ASSERT(_nodes.ElementAt(index) == node,"Value Numbering initialization error");
      return;
      }
   _nodes.ElementAt(index) = node;
   _nextInRing.ElementAt(index) = index;

   // Initialize the children
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      initializeNode(node->getChild(i), negativeValueNumber);

   // For a non-shareable node, just allocate the value number now and return
   //
   if (!canShareValueNumber(node))
      {
      if (_useDefInfo && node->getUseDefIndex())
         _valueNumbers.ElementAt(index) = _nextValue++;
      else
         _valueNumbers.ElementAt(index) = negativeValueNumber--;
      return;
      }

   // For a shareable node add the node to the hashtable, collecting together
   // like nodes into chains.
   // TODO: replace this with a call to congruentNodes
   _valueNumbers.ElementAt(index) = -1;

   int32_t hashValue = hash(node);
   NodeEntry *newEntry = new (trStackMemory()) NodeEntry;
   newEntry->_node = node;
   CollisionEntry *entry;
   for (entry = _hashTable._buckets[hashValue]; entry; entry = entry->_next)
      {
      TR::Node *entryNode = entry->_nodes->_node;
      if (node->getOpCodeValue() != entryNode->getOpCodeValue() ||
          node->getNumChildren() != entryNode->getNumChildren())
         continue;
#ifdef J9_PROJECT_SPECIFIC
      if (node->getOpCode().isSetSignOnNode() && node->getSetSign() != entryNode->getSetSign())
         {
         if (trace())
            traceMsg(comp(), "Nodes %s (%p) and entryNode %s (%p) with setSignOnNode have sign mismatch -- do not consider as matching\n",
                  node->getOpCode().getName(),node,
                  entryNode->getOpCode().getName(),entryNode);
         continue;
         }

      if (node->getType().isBCD())
         {
         if (!node->isDecimalSizeAndShapeEquivalent(entryNode))
            {
            if (trace())
               traceMsg(comp(),"BCD node %s (%p) and BCD entryNode %s (%p) have size/shape mismatch -- do not consider as matching\n",
                  node->getOpCode().getName(),node,
                  entryNode->getOpCode().getName(),entryNode);
            continue;
            }

         if (!node->isSignStateEquivalent(entryNode))
            {
            if (trace() || comp()->cg()->traceBCDCodeGen())
               traceMsg(comp(),"x^x : BCD node %s (%p) and BCD entryNode %s (%p) have sign state mismatch -- do not consider as matching\n",
                  node->getOpCode().getName(),node,
                  entryNode->getOpCode().getName(),entryNode);
            continue;
            }
         }
      else if (node->getOpCode().isConversionWithFraction() &&
               node->getDecimalFraction() != entryNode->getDecimalFraction())
         {
         if (trace())
            traceMsg(comp(),"fracConv node %s (%p) and fracConv entryNode %s (%p) have fraction mismatch -- do not consider as matching\n",
               node->getOpCode().getName(),node,
               entryNode->getOpCode().getName(),entryNode);
         continue;
         }
      else if (node->chkOpsCastedToBCD() &&
               node->castedToBCD() != entryNode->castedToBCD())
         {
         if (trace())
            traceMsg(comp(),"castedToBCD mismatch : node %s (%p) castedToBCD %d and entryNode %s (%p) castedToBCD %d -- do not consider as matching\n",
               node->getOpCode().getName(),node,node->castedToBCD(),entryNode->getOpCode().getName(),entryNode,entryNode->castedToBCD());
         continue;
         }
      else if (node->getType().isDFP() && node->getOpCode().isModifyPrecision() && node->getDFPPrecision() != entryNode->getDFPPrecision())
         {
         if (trace())
            traceMsg(comp(), "DFP node %s (%p) and entryNode %s (%p) have different precisions -- do not consider as matching\n",
               node->getOpCode().getName(), node,
               entryNode->getOpCode().getName(), entryNode);
         continue;
         }
#endif

      // Check for loads of constants.  They're much like isLoadConst.
      //
      if (  node->getOpCode().isLoadVar()
         && node->getSymbolReference() == entryNode->getSymbolReference()
         && (  node->getSymbol()->isConst()
            || node->getSymbol()->isConstObjectRef()))
         {
         break;
         }

      // Share chains for loads and stores, since they won't be matched with
      // other nodes in the chain.
      //
      if (node->getOpCode().isStore() ||
          node->getOpCode().isLoadVar())
         break;

      if (node->getOpCode().hasSymbolReference())
         {
         TR::SymbolReference *symRef = node->getSymbolReference();
         TR::SymbolReference *entrySymRef = entryNode->getSymbolReference();
         if (symRef && entrySymRef &&
             symRef->getSymbol() == entrySymRef->getSymbol() &&
             symRef->getOffset() == entrySymRef->getOffset())
            {
            uint16_t udIndex = node->getUseDefIndex();
            uint16_t entryUdIndex = entryNode->getUseDefIndex();
            if (!_useDefInfo || !_useDefInfo->isUseIndex(udIndex))
               break;
            TR_ASSERT(_useDefInfo->isUseIndex(entryUdIndex), "Expecting use index");
            //
            // In rare cases when Jsrs exist in the method, use def info could be NULL
            //
            TR_UseDefInfo::BitVector udef(comp()->allocator());
            TR_UseDefInfo::BitVector entryudef(comp()->allocator());
            if (_useDefInfo->getUseDef(udef, udIndex) && _useDefInfo->getUseDef(entryudef, entryUdIndex))
               {
               if (udef == entryudef)
                  {
                  break;
                  }
               }
            else
               {
               if (trace())
                 TR_ASSERT(0, "No defs for a use\n");
               }
            }
         }
      else if (node->getOpCode().isLoadConst())
         {
         bool isSame = false;
         switch (node->getDataType())
            {
            case TR::Int8: isSame = (node->getByte() == entryNode->getByte()); break;
            case TR::Int16: isSame = (node->getShortInt() == entryNode->getShortInt()); break;
            case TR::Int32: isSame = (node->getInt() == entryNode->getInt()); break;
            case TR::Float: isSame = (node->getFloatBits() == entryNode->getFloatBits()); break;
            case TR::Int64:
            case TR::Double: isSame = (node->getLongInt() == entryNode->getLongInt()); break;
#ifdef J9_PROJECT_SPECIFIC
            case TR::DecimalFloat: isSame = (node->getInt() == entryNode->getInt()); break;
            case TR::DecimalDouble: isSame = (node->getLongInt() == entryNode->getLongInt()); break;
#ifdef SUPPORT_DFP
            case TR::DecimalLongDouble: isSame = (node->getLongDouble() == entryNode->getLongDouble()); break;
#endif
#endif
            case TR::Address:isSame = (node->getAddress() == entryNode->getAddress()); break;
            default:
               {
               if (
#ifdef J9_PROJECT_SPECIFIC
                   node->getType().isBCD() ||
#endif
                   node->getType().isAggregate())
                  {
                  isSame = _optimizer->areNodesEquivalent(node, entryNode);
                  }
               else
                  {
                  isSame = false;
                  }
               }
            }
         if (isSame)
            break;
         }
      else if (node->getOpCode().isArrayLength())
         {
         int32_t nodeStride = node->getArrayStride();
         int32_t entryNodeStride = entryNode->getArrayStride();

         if (nodeStride == entryNodeStride)
            break;
         }
      else
         break;
       }

   if (entry)
      {
      newEntry->_next = entry->_nodes;
      entry->_nodes = newEntry;
      }
   else
      {
      entry = new (trStackMemory()) CollisionEntry;
      entry->_nodes = newEntry;
      newEntry->_next = NULL;
      entry->_next = _hashTable._buckets[hashValue];
      _hashTable._buckets[hashValue] = entry;
      }

   _matchingNodes->element(index) = entry;
   }


/**
 * Hash on the opcode, number of children, and symbol information
 */
int32_t TR_ValueNumberInfo::hash(TR::Node *node)
   {

   uint32_t h, g;
   int32_t numChildren = node->getNumChildren();
   h = (node->getOpCodeValue() << 16) + numChildren;
   g = h & 0xF0000000;
   h ^= g >> 24;
   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      if (symRef)
         {
         TR::Symbol *sym = symRef->getSymbol();
         h = (h << 4) + (int32_t)(intptrj_t)sym;
         g = h & 0xF0000000;
         h ^= g >> 24;
         h = (h << 4) + symRef->getOffset();
         g = h & 0xF0000000;
         h ^= g >> 24;
         }
      }
   else if (node->getOpCode().isLoadConst())
      {
      if (node->getOpCode().is8Byte())
         {
         h = (h << 4) + node->getLongIntHigh();
         g = h & 0xF0000000;
         h ^= g >> 24;
         h = (h << 4) + node->getLongIntLow();
         g = h & 0xF0000000;
         h ^= g >> 24;
         }
      else
         {
         h = (h << 4) + (node->getOpCodeValue() == TR::fconst ? node->getFloatBits() : node->getInt());
         g = h & 0xF0000000;
         h ^= g >> 24;
         }
      }
   return (h ^ g) % _hashTable._numBuckets;
   }


/**
 * Non-shareable nodes already have a negative value number assigned.
 * Change their value numbers to be positive.
 */
void TR_ValueNumberInfo::allocateNonShareableValueNumbers()
   {

   int32_t maxValue = _nextValue-1;
   for (int32_t i = 0; i < _numberOfNodes; ++i)
      {
      int32_t value = _valueNumbers.ElementAt(i);
      if (value <= -3)
         {
         value = _nextValue - value - 2;
         _valueNumbers.ElementAt(i) = value;
         if (value > maxValue)
            maxValue = value;
         }
      }
   _nextValue = maxValue + 1;
   }

void TR_ValueNumberInfo::allocateParmValueNumbers()
   {
   _numberOfParms = 0;
   TR::ResolvedMethodSymbol *method = _compilation->getMethodSymbol();
   TR::ParameterSymbol *p;
   ListIterator<TR::ParameterSymbol> parms(&method->getParameterList());
   for (p = parms.getFirst(); p; p = parms.getNext())
      {
      _numberOfParms++;
      }
   if (_numberOfParms > 0)
      {
      _parmSymbols = (TR::ParameterSymbol**)trMemory()->allocateHeapMemory(_numberOfParms*sizeof(TR::ParameterSymbol*), TR_Memory::ValueNumberInfo);
      parms.reset();
      int32_t i = 0;
      for (p = parms.getFirst(); p; p = parms.getNext())
         {
         _parmSymbols[i++] = p;
         }
      }
   else
      _parmSymbols = NULL;
   _nextValue += _numberOfParms;
   }

/**
 * Non-shareable nodes already have a value number assigned.
 * Shareable nodes are all added to the hash table, so just go through the
 * table and allocate value numbers.
 * Value numbers for pre-requisite nodes are allocated by recursing.
 */
void TR_ValueNumberInfo::allocateShareableValueNumbers()
   {

   _recursionDepth = 0;

   vcount_t visitCount = _compilation->incVisitCount();
   TR::TreeTop *treeTop = _compilation->getStartTree();
   while (treeTop)
      {
      allocateValueNumber(treeTop->getNode(), visitCount);
      treeTop = treeTop->getNextTreeTop();
      }
   }


void TR_ValueNumberInfo::allocateValueNumber(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   // Make sure all the children have been given value numbers
   //
   int32_t i;
   for (i = node->getNumChildren()-1; i >= 0; --i)
      allocateValueNumber(node->getChild(i), visitCount);

   if (canShareValueNumber(node))
      allocateValueNumber(node);
   }


bool TR_ValueNumberInfo::canShareValueNumber(TR::Node *node)
   {
   if (node->getOpCode().canShareValueNumber() &&
       !node->getOpCode().isResolveCheck() &&
       !node->hasUnresolvedSymbolReference())
      {
      return true;
      }
   return false;
   }



void TR_ValueNumberInfo::allocateValueNumber(TR::Node *node)
   {

   int32_t index = node->getGlobalIndex();
   if (_valueNumbers.ElementAt(index) >= 0 ||
       _valueNumbers.ElementAt(index) <= -3)
      return;

   // To prevent infinite recursion, mark this node as being processed
   //
   _valueNumbers.ElementAt(index) = -2;

#if DEBUG
   if (trace())
      {
      traceMsg(comp(), "Processing node %d at depth %d\n",index,_recursionDepth);
      }
#endif
   ++_recursionDepth;


   // Make sure all the children have been given value numbers
   //
   int32_t i;
   for (i = node->getNumChildren()-1; i >= 0; --i)
      {
      if (_valueNumbers.ElementAt(node->getChild(i)->getGlobalIndex()) != -2)
         allocateValueNumber(node->getChild(i));
      }

   // This node may now have been given a value due to recursion
   //
   if (_valueNumbers.ElementAt(index) >= 0)
      {
      --_recursionDepth;
#if DEBUG
      if (trace())
         {
         traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
         }
#endif
      return;
      }

   bool            removeFromHashTable = false;
   CollisionEntry *head                = _matchingNodes->element(index);
   NodeEntry      *entry;

   // If this is a store node give it the value number of the node being
   // assigned.
   //
   if (node->getOpCode().isStore())
      {
      int32_t valueIndex = node->getOpCode().isIndirect() ? 1 : 0;
      TR::Node *valueChild = node->getChild(valueIndex);
      allocateValueNumber(valueChild);

      // This node may now have been given a value due to recursion
      //
      if (_valueNumbers.ElementAt(index) >= 0)
         {
         --_recursionDepth;
#if DEBUG
         if (trace())
            {
            traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
            }
#endif
         return;
         }

      // This node must be given a shareable value number. If the child has a
      // non-shareable value number, give it a shareable one, and then also
      // give it to this store.
      //
      if (getVN(valueChild) <= -3)
         changeValueNumber(valueChild, _nextValue++);
      setValueNumber(node, valueChild);

      // The store will never be matched from the hash table, so just remove
      // it from the table.
      //
      removeFromHashTable = true;
      }

   // If this is a load node, see if all the defs have the same value number.
   // If so, give the node that value number.
   //
   else if (node->getOpCode().isLoadVar()
      && !( node->getSymbol()->isConst()
         || node->getSymbol()->isConstObjectRef()))
      {
      TR::Node * defNode = getValueNumberForLoad(node);

      // This node may now have been given a value due to recursion
      //
      if (_valueNumbers.ElementAt(index) >= 0)
         {
         --_recursionDepth;
#if DEBUG
         if (trace())
            {
            traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
            }
#endif
         return;
         }

      if (defNode)
         setValueNumber(node, defNode);
      else
         changeValueNumber(node, _nextValue++);

      // The load will never be matched from the hash table, so just remove
      // it from the table.
      //
      removeFromHashTable = true;
      }

   // For all other nodes look to see if there is another node whose children
   // all have the same value number. If so, use its value number.
   //
   else
      {
      int32_t numChildren = node->getNumChildren();
      for (entry = head->_nodes; entry; entry = entry->_next)
         {
         TR::Node *entryNode = entry->_node;
         int32_t  entryValue = getVN(entryNode);
         if (entryValue < 0)
            break;
         if (node->getOpCode().hasSymbolReference())
            {
            // Nodes with symrefs only match other nodes with the same symref
            //
            if (!entryNode->getOpCode().hasSymbolReference())
               continue;
            if (node->getSymbolReference() != entryNode->getSymbolReference())
               continue;
            }
         if (numChildren > 0)
            {
            for (i = numChildren-1; i >= 0; --i)
               {
               int32_t v1 = getVN(node->getChild(i));
               int32_t v2 = getVN(entryNode->getChild(i));
               if ((v1 < 0) || (v2 < 0))
                  break;

               ///if (getVN(node->getChild(i)) != getVN(entryNode->getChild(i)))
               if (v1 != v2)
                  break;
               }
            if (i >= 0)
               continue;
            }

         // A match found - we can reuse the value number of this other node. At the
         // same time, remove this node from the node chain.
         //
         setValueNumber(node, entryNode);
         NodeEntry *next;
         for (next = entry->_next ; next->_node != node; entry = next, next = next->_next)
            {}
         entry->_next = next->_next;
         --_recursionDepth;
   #if DEBUG
         if (trace())
            {
            traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
            }
   #endif
         return;
         }

      // No match found - assign a new value number to this node.
      //
      changeValueNumber(node, _nextValue++);
      }

   // Find the node in its hash entry chain and de-chain it. If it is not to be
   // removed from the table, put it back at the head of the chain.
   //
   NodeEntry *prev;
   for (entry = head->_nodes, prev = NULL; entry->_node != node; prev = entry, entry = entry->_next)
      {}
   if (prev)
      {
      prev->_next = entry->_next;
      if (!removeFromHashTable)
         {
         entry->_next = head->_nodes;
         head->_nodes = entry;
         }
      }
   else
      {
      if (removeFromHashTable)
         head->_nodes = entry->_next;
      }

   --_recursionDepth;
#if DEBUG
   if (trace())
      {
      traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
      }
#endif
   }

TR::Node *TR_ValueNumberInfo::getValueNumberForLoad(TR::Node *node)
   {
   uint16_t useDefIndex = node->getUseDefIndex();
   if (!_useDefInfo ||
       !_useDefInfo->isUseIndex(useDefIndex) ||
       (_recursionDepth > 50))
      return NULL;

   TR_BitVectorIterator bvi;
   TR::Node             *defNode = NULL;
   int32_t              index    = 0;
   int32_t              baseVN   = node->getOpCode().isIndirect() ? getVN(node->getFirstChild()) : -1;
   int32_t              value    = 0;
   int32_t              defValue = 0;

   // If there is a single def node which is really another load that matches
   // this one and dominates it, use the value number of that load.
   //
   defNode = _useDefInfo->getSingleDefiningLoad(node);

   if (defNode)
      {
      TR_ASSERT(defNode->getOpCode().isLoadVar(), "Defining load is not a load");
      allocateValueNumber(defNode);

      if (baseVN == -1 || (defNode->getOpCode().isIndirect() && baseVN == getVN(defNode->getFirstChild())))
         {
         // If this node now has a value number, we can equate the value numbers
         // of it and its dominating load
         //
         value = getVN(node);
         if (value >= 0)
            {
            defValue = getVN(defNode);
            if (value != defValue)
               changeValueNumber(node, defValue);
            if (trace())
               {
               traceMsg(comp(), "  Change value number for load %d at [%p] to value number %d of dominating load %d at [%p]\n", node->getGlobalIndex(), node, defValue, defNode->getGlobalIndex(), defNode);
               }
            return NULL;
            }

         if (trace())
            {
            traceMsg(comp(), "  Use value number %d of dominating load %d at [%p] for load %d at [%p]\n", getVN(defNode), defNode->getGlobalIndex(), defNode, node->getGlobalIndex(), node);
            }
         return defNode;
         }
      }

   // This node may now have been given a value due to recursion
   //
   if (getVN(node) >= 0)
      return NULL;

   TR_UseDefInfo::BitVector definingLoads(comp()->allocator());
   bool isNonZero = _useDefInfo->getDefiningLoads(definingLoads, node);

   if (isNonZero && node->getOpCode().isLoadVarDirect())
      {
      int32_t defValue = -1;
      TR::Node *defNode = NULL;
      TR_UseDefInfo::BitVector::Cursor cursor(definingLoads);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t index = cursor;
         defNode = _useDefInfo->getNode(index);

         if (defNode)
            {
            TR_ASSERT(defNode->getOpCode().isLoadVar(), "Defining load is not a load");
            allocateValueNumber(defNode);
            int32_t newDefValue = getVN(defNode);
            if (trace())
               traceMsg(comp(), "node %p defNode %p newDefValue %d\n", node, defNode, newDefValue);
            if (newDefValue < 0)
               {
               defValue = -1;
               break;
               }

            if (defValue == -1)
               defValue = newDefValue;
            else if (defValue != newDefValue)
               {
               defValue = -1;
               break;
               }
            }
         else
            break;
         }
      // If this node now has a value number, we can equate the value numbers
      // of it and its dominating loads
      //
      value = getVN(node);
      if (defValue >= 0)
         {
         if (value >= 0)
            {
            if (value != defValue)
               changeValueNumber(node, defValue);
            if (trace())
               {
               traceMsg(comp(), "  Change value number for load %d at [%p] to value number %d of dominating loads\n", node->getGlobalIndex(), node, defValue);
               }

            return NULL;
            }
         else
            {
            if (trace())
               {
               traceMsg(comp(), "  2Use value number %d of dominating load %d at [%p] for load %d at [%p]\n", getVN(defNode), defNode->getGlobalIndex(), defNode, node->getGlobalIndex(), node);
               }
            return defNode;
            }
         }
      }

   // This node may now have been given a value due to recursion
   //
   if (getVN(node) >= 0)
      return NULL;

   if (isNonZero && node->getOpCode().isLoadVar())
      {
      TR_UseDefInfo::BitVector::Cursor cursor(definingLoads);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         int32_t index = cursor;
         TR_UseDefInfo::BitVector usesFromDefs(comp()->allocator());
         //traceMsg(comp(), "def %d uses from defs %p\n", index, usesFromDefs);
         if (_useDefInfo->getUsesFromDef(usesFromDefs, index, true))
            {
            //traceMsg(comp(), "2looking at use node %p\n", node);
            TR_UseDefInfo::BitVector::Cursor cursor2(usesFromDefs);
            for (cursor2.SetToFirstOne(); cursor2.Valid(); cursor2.SetToNextOne())
               {
               int32_t useIndex = cursor2;
               TR::Node *useNode = _useDefInfo->getNode(useIndex+_useDefInfo->getFirstUseIndex());
               //traceMsg(comp(), "3compare with use node %p\n", useNode);
               if (useNode &&
                     (useNode != node) &&
                     useNode->getOpCode().isLoadVar())
                  {
                  TR_UseDefInfo::BitVector otherUseDefiningLoads(comp()->allocator());
                  if (_useDefInfo->getDefiningLoads(otherUseDefiningLoads, useNode) && (otherUseDefiningLoads == definingLoads))
                     {
                     //TR_ASSERT(useNode->getOpCode().isLoadVar(), "Defining load is not a load");
                     if (_valueNumbers.ElementAt(useNode->getGlobalIndex()) != -2)
                        allocateValueNumber(useNode);
                     else
                        changeValueNumber(useNode, _nextValue++);

                     if (baseVN == -1 || (useNode->getOpCode().isIndirect() && baseVN == getVN(useNode->getFirstChild())))
                        {
                        // If this node now has a value number, we can equate the value numbers
                        // of it and its dominating load
                        //
                        value = getVN(node);
                        if (value >= 0)
                           {
                           int32_t useValue = getVN(useNode);
                           if (value != useValue)
                              changeValueNumber(node, useValue);
                           if (trace())
                              {
                              traceMsg(comp(), "  Change value number for load %d at [%p] to value number %d of load %d at [%p] reached by same dominating loads (defs)\n", node->getGlobalIndex(), node, useValue, useNode->getGlobalIndex(), useNode);
                              }
                           return NULL;
                           }

                        if (trace())
                           {
                           traceMsg(comp(), "  Use value number %d of dominating load %d at [%p] for load %d at [%p]\n", getVN(useNode), useNode->getGlobalIndex(), useNode, node->getGlobalIndex(), node);
                           }
                        return useNode;
                        }
                     }
                  }
               }
            }
         }
      }

   // This node may now have been given a value due to recursion
   //
   if (getVN(node) >= 0)
      return NULL;

   // Get the set of real def nodes for this load.
   // Then look to see if they all have the same value number.
   // If so, use that value number for the load.
   // Otherwise give
   // the load its own value number.
   //
   TR_UseDefInfo::BitVector defsForLoad(comp()->allocator());

   // In rare case when Jsrs exist in a method a use could exist without a def
   //
   if (!_useDefInfo->getUseDef(defsForLoad, useDefIndex))
      return NULL;

   if (trace())
      {
      traceMsg(comp(), "  Defs for load at [%p]: ",node);
      (*comp()) << defsForLoad;
      traceMsg(comp(), "\n");
      }

   TR::SymbolReference  *loadSymRef  = node->getSymbolReference();

   value = -1;
   TR_UseDefInfo::BitVector::Cursor cursor(defsForLoad);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t defIndex = cursor;
      defNode = _useDefInfo->getNode(defIndex);

      if (!defNode)
         {
         // Defined by method entry. If it is a parameter, assign the value
         // number for this parameter on entry.
         //
         TR::Symbol *sym = node->getSymbol();
         if (sym->isParm())
            {
            for (int32_t i = 0; i < _numberOfParms; i++)
               {
               if (sym == _parmSymbols[i])
                  {
                  defValue = i+1;
                  break;
                  }
               }
            }
         else
            {
            // TODO
            // Field or static defined by method entry - no way currently of
            // commoning value numbers, so just assign a unique value number.
            //
            return NULL;
            }
         }

      else
         {
         // Break recursion by assigning a new value number. The child of a
         // store can be this load, which has the store as one of its def nodes
         //
         if (getVN(defNode) == -2)
            {
            return NULL;
            }

         allocateValueNumber(defNode);

         // This node may now have been given a value due to recursion
         //
         if (getVN(node) >= 0)
            return NULL;

         defValue = getVN(defNode);

         if (defValue < 0)
            {
            // TODO
            // Defined by a call node - no way currently of commoning value
            // numbers, so just assign a unique value number.
            //
            return NULL;
            }

         // If the def node has different symbol information or (for an indirect
         // load) a different base object, give up
         //
         TR::SymbolReference *defSymRef = defNode->getSymbolReference();
         if (loadSymRef->getSymbol() != defSymRef->getSymbol() ||
             loadSymRef->getOffset() != defSymRef->getOffset())
            return NULL;

         if (baseVN != -1 && baseVN != getVN(defNode->getFirstChild()))
            return NULL;
         }

      if (value == -1)
         value = defValue;
      else if (value != defValue)
         return NULL;
      }

   // If there is no def node, the value number was obtained from method entry
   // or a call. In this case, just assign the new value number.
   //
   if (!defNode)
      {
      changeValueNumber(node, value);
      }

   return defNode;
   }


void TR_ValueNumberInfo::setValueNumber(TR::Node *node, TR::Node *other)
   {
   int32_t index = node->getGlobalIndex();
   int32_t otherIndex = other->getGlobalIndex();

   // Make sure there's enough room in the arrays.
   //
   if (index >= _numberOfNodes)
      {
      growTo(index);
      _nodes.ElementAt(index) = node;
      }
   else
      {
      // Remove the node from its existing value number chain
      //
      if (_nextInRing.ElementAt(index) != index)
         {
         int32_t i;
         for (i = _nextInRing.ElementAt(index); _nextInRing.ElementAt(i) != index; i = _nextInRing.ElementAt(i))
            ;
         _nextInRing.ElementAt(i) = _nextInRing.ElementAt(index);
         }
      }

   // Add the node to the new value number chain
   //
   _nextInRing.ElementAt(index) = _nextInRing.ElementAt(otherIndex);
   _nextInRing.ElementAt(otherIndex) = index;

   // Assign the new value number
   //
   _valueNumbers.ElementAt(index) = _valueNumbers.ElementAt(otherIndex);
   }

void TR_ValueNumberInfo::setUniqueValueNumber(TR::Node *node)
   {
   int32_t index = node->getGlobalIndex();

   // Make sure there's enough room in the arrays.
   //
   if (index >= _numberOfNodes)
      {
      growTo(index);
      _nodes.ElementAt(index) = node;
      }
   else
      {
      // Remove the node from its existing value number chain
      if (_nextInRing.ElementAt(index) != index)
         {
         int32_t i;
         for (i = _nextInRing.ElementAt(index); _nextInRing.ElementAt(i) != index; i = _nextInRing.ElementAt(i))
            ;
         _nextInRing.ElementAt(i) = _nextInRing.ElementAt(index);
         }
      }

   // Create a new ring for this node
   //
   _nextInRing.ElementAt(index) = index;

   // Assign the new value number
   //
   _valueNumbers.ElementAt(index) = _nextValue++;
   }

void TR_ValueNumberInfo::changeValueNumber(TR::Node *node, int32_t newVN)
   {
   // Change the value number of all nodes in the root node's value number chain
   //
   int32_t index = node->getGlobalIndex();

   // Make sure there's enough room in the arrays.
   //
   if (index >= _numberOfNodes)
      {
      growTo(index);
      _nodes.ElementAt(index) = node;
      _nextInRing.ElementAt(index) = index;
      _valueNumbers.ElementAt(index) = newVN;
      }
   else
      {
      // Change the value number of all other nodes in the root node's value
      // number chain, then change its own value number.
      //
      for (int32_t i = _nextInRing.ElementAt(index); i != index; i = _nextInRing.ElementAt(i))
         _valueNumbers.ElementAt(i) = newVN;
      _valueNumbers.ElementAt(index) = newVN;
      }
   if (newVN >= _nextValue)
      _nextValue = newVN+1;
   }

void TR_ValueNumberInfo::removeNodeInfo(TR::Node *node)
   {
   // Remove the node from its existing value number chain
   //
   int32_t index = node->getGlobalIndex();
   int32_t i;

   if (index < _numberOfNodes)
      {
      if (_nextInRing.ElementAt(index) != index)
         {
         for (i = _nextInRing.ElementAt(index); _nextInRing.ElementAt(i) != index; i = _nextInRing.ElementAt(i))
            ;
         // _nextInRing.ElementAt(i) == index
         _nextInRing.ElementAt(i) = _nextInRing.ElementAt(index);
         _nextInRing.ElementAt(index) = index;
         }
      _nodes.ElementAt(index) = NULL;
      }
   }

void TR_ValueNumberInfo::growTo(int32_t index)
   {
   _nodes.GrowTo(index+1);
   _valueNumbers.GrowTo(index+1);
   _nextInRing.GrowTo(index+1);

   // Initialize nodes between the old maximum and the new to have unique
   // value numbers
   //
   int32_t i = _numberOfNodes;
   _numberOfNodes = index+1;
   for ( ; i < index; ++i)
      {
      _nodes.ElementAt(i) = NULL;
      _nextInRing.ElementAt(i) = i;
      _valueNumbers.ElementAt(i) = _nextValue++;
      }
   }



void TR_ValueNumberInfo::printValueNumberInfo(TR::Node *node)
   {
   traceMsg(comp(), "Node : %p    Index = %d    Value number = %d\n", node, node->getUseDefIndex(), getVN(node));

   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      printValueNumberInfo(child);
      }
   }


/******************************************************
 * TR_HashValueNumberInfo Implementation
 ********************************************************/


TR_HashValueNumberInfo::VNHashKey::VNHashKey(TR::Node * node, TR_ValueNumberInfo * VN):_node(node),_VN(VN)
{
   _hashVal = 2166136261;

   hash(node->getOpCodeValue());

   if (node->getOpCode().hasSymbolReference())
       hash(node->getSymbolReference()->getReferenceNumber());
   else if (node->getOpCode().isLoadConst())
   {
         if (node->getOpCode().is8Byte())
            {
             hash(node->getLongIntHigh());
             hash(node->getLongIntLow());
            }
        else if (node->getOpCodeValue() == TR::fconst)
            {
             hash(node->getFloatBits());
            }
        else
           {
                 hash(node->getInt());
           }
   }

   for(uint32_t childNum = 0; childNum < node->getNumChildren(); childNum++ )
   {
       int32_t childVN = _VN->getValueNumber(node->getChild(childNum));
       TR_ASSERT(childVN > 0,"Allocating VN for node %p without allocating VN for child %p",node,node->getChild(childNum));
         hash(childVN);
   }


}

bool TR_HashValueNumberInfo::VNHashKey::operator==(const VNHashKey & other) const
   {
   TR::Node * otherNode = other._node;
   if(_node->getOpCodeValue() == otherNode->getOpCodeValue() && _node->getNumChildren() == otherNode->getNumChildren())
      {
      bool childVNEqual = true;
      for(uint32_t childNum = 0; (childNum < _node->getNumChildren() && childVNEqual); childNum++ )
         childVNEqual = (_VN->getValueNumber(_node->getChild(childNum)) == _VN->getValueNumber(otherNode->getChild(childNum)));
      if(childVNEqual)
         {
         bool areNodesCongruent = _VN->congruentNodes(_node,otherNode);
         return areNodesCongruent;
         }
      else
         return false;
      }
   else
      {
      return false;
      }
   }

TR_HashValueNumberInfo::TR_HashValueNumberInfo(TR::Compilation *comp, TR::Optimizer *optimizer, bool requiresGlobals, bool prefersGlobals, bool noUseDefInfo)
 : TR_ValueNumberInfo(comp),
   _nodeHash(comp->allocator())
   {
   _compilation = comp;
   _optimizer = optimizer;
   _trace = comp->getOption(TR_TraceValueNumbers);

   dumpOptDetails(comp, " HASHVN  (Building value number info)\n");

   // For now, don't allow global value numbering because of
   // threads-changing-fields problems.
   //
   TR_ASSERT(!(requiresGlobals || prefersGlobals), "Don't do global value numbering");

   if (trace())
      traceMsg(comp, "Starting ValueNumbering %s\n", noUseDefInfo ? "without UseDefInfo" : "");

   if (noUseDefInfo)
      {
      _useDefInfo = NULL;
      }
   else
      {
      // Make sure we have valid use/def info
      //
      _useDefInfo = _optimizer->getUseDefInfo();
      if (_useDefInfo && requiresGlobals && !_useDefInfo->hasGlobalsUseDefs())
         _useDefInfo = NULL;
      if (_useDefInfo == NULL && !_optimizer->cantBuildLocalsUseDefInfo())
         {
         if (!(requiresGlobals && _optimizer->cantBuildGlobalsUseDefInfo()))
            {
            TR::LexicalMemProfiler mp("use defs (value numbers - II)", comp->phaseMemProfiler());
            _useDefInfo = new (comp->allocator()) TR_UseDefInfo(comp, comp->getFlowGraph(), optimizer, requiresGlobals, prefersGlobals);
            if (_useDefInfo->infoIsValid())
               {
               _optimizer->setUseDefInfo(_useDefInfo);
               }
            else
               {
               // release storage for failed _useDefInfo
               delete _useDefInfo;
               _useDefInfo = NULL;
               }
            }
         }

      if (_useDefInfo == NULL)
         {
         if (trace())
            traceMsg(comp, "Can't perform ValueNumbering, no use/def info\n");
         _infoIsValid = false;
         _optimizer->setCantBuildGlobalsValueNumberInfo(true);
         if (!requiresGlobals)
            _optimizer->setCantBuildLocalsValueNumberInfo(true);
         return;
         }

      _useDefInfo->buildDefUseInfo(true);
      }
   _infoIsValid = true;
   _hasGlobalsValueNumbers = requiresGlobals;
   _numberOfNodes = comp->getNodeCount();

   TR::TreeTop *treeTop;

   if (trace())
      {
      traceMsg(comp, "\nTrees for value numbering\n\n");
      comp->incVisitCount();
      for (treeTop = comp->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
         {
         comp->getDebug()->print(comp->getOutFile(), treeTop);
         }
      traceMsg(comp, "\n\n");
      }

   _nodes.GrowTo(_numberOfNodes);
   _valueNumbers.GrowTo(_numberOfNodes);
   _nextInRing.GrowTo(_numberOfNodes);

   // Any stack allocations made past this point will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   buildValueNumberInfo();

   int32_t i;
   if (trace())
      {
      // Show shared value numbers
      //
      TR_BitVector traced(_numberOfNodes,trMemory(), stackAlloc);
      for (i = 0; i < _numberOfNodes; i++)
         {
         TR::Node *node = getNode(i);
         if (node == NULL || traced.get(node->getGlobalIndex()))
            continue;
         if (getNext(node) == node)
            continue;
         traceMsg(comp, "   Nodes sharing value number %d:", getValueNumber(node));
         TR::Node *next = node;
         do
            {
            traced.set(next->getGlobalIndex());
            traceMsg(comp, " %d", next->getGlobalIndex());
            next = getNext(next);
            } while(next != node);
         traceMsg(comp, "\n");
         }
      traceMsg(comp, "\nEnding ValueNumbering\n");

      //nodeHash.DumpStatistics();
      traceMsg(comp, "\n\nValue Number Table\n\n");
      for (i = 0; i < _numberOfNodes; i++)
         {
         TR::Node *node = getNode(i);
         if (node == NULL)
            continue;
         traceMsg(comp, "HVN: node %4d [%p] has value number %4d", i, node, getValueNumber(node));
         if (getNext(node) != node)
            {
            traceMsg(comp, ", shared with ");
            for (TR::Node *next = getNext(node); next != node; next = getNext(next))
               traceMsg(comp, " %d", next->getGlobalIndex());
            }
         traceMsg(comp, "\n");
         }
      traceMsg(comp, "\nEnded ValueNumbering\n");
      }
   _nodeHash.MakeEmpty();
   }


void TR_HashValueNumberInfo::initializeNode(TR::Node * node, int32_t & negativeValueNumber)
{
      int32_t index = node->getGlobalIndex();
      if (_nodes.ElementAt(index) != NULL)
         {
         // Node has already been initialized
         //
         TR_ASSERT(_nodes.ElementAt(index) == node,"Value Numbering initialization error");
         return;
         }
      _nodes.ElementAt(index) = node;
      _nextInRing.ElementAt(index) = index;

      // Initialize the children
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         initializeNode(node->getChild(i), negativeValueNumber);

      // For a non-shareable node, just allocate the value number now and return
      //
      if (!canShareValueNumber(node))
         {
         if (_useDefInfo && node->getUseDefIndex())
            _valueNumbers.ElementAt(index) = _nextValue++;
         else
            _valueNumbers.ElementAt(index) = negativeValueNumber--;
         return;
         }


      _valueNumbers.ElementAt(index) = -1;
}

void TR_HashValueNumberInfo::allocateValueNumber(TR::Node * node)
{
      int32_t index = node->getGlobalIndex();
      if (_valueNumbers.ElementAt(index) >= 0 ||
          _valueNumbers.ElementAt(index) <= -3)
         return;

      // To prevent infinite recursion, mark this node as being processed
      //
      _valueNumbers.ElementAt(index) = -2;

   #if DEBUG
      if (trace())
         {
         traceMsg(comp(), "Processing node %d at depth %d\n",index,_recursionDepth);
         }
   #endif
      ++_recursionDepth;


      // Make sure all the children have been given value numbers
      //
      int32_t i;
      for (i = node->getNumChildren()-1; i >= 0; --i)
         {
         if (_valueNumbers.ElementAt(node->getChild(i)->getGlobalIndex()) != -2)
            {
            //traceMsg(comp(), "About to recurse on node %p\n",node->getChild(i));
            allocateValueNumber(node->getChild(i));
            }
//         else
//            {
//            traceMsg(comp(), "Did not recurse on node %p because getGloblalIndex = -2\n",node->getChild(i));
//            }
         }

      // This node may now have been given a value due to recursion
      //
      if (_valueNumbers.ElementAt(index) >= 0)
         {
         --_recursionDepth;
   #if DEBUG
         if (trace())
            {
            traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
            }
   #endif
         return;
         }

      // If this is a store node give it the value number of the node being
      // assigned.
      //
      if (node->getOpCode().isStore())
         {
         int32_t valueIndex = node->getOpCode().isIndirect() ? 1 : 0;
         TR::Node *valueChild = node->getChild(valueIndex);
         allocateValueNumber(valueChild);

         // This node may now have been given a value due to recursion
         //
         if (_valueNumbers.ElementAt(index) >= 0)
            {
            --_recursionDepth;
   #if DEBUG
            if (trace())
               {
               traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
               }
   #endif
            return;
            }

         // This node must be given a shareable value number. If the child has a
         // non-shareable value number, give it a shareable one, and then also
         // give it to this store.
         //
         if (getVN(valueChild) <= -3)
            changeValueNumber(valueChild, _nextValue++);
         setValueNumber(node, valueChild);

         }

      // If this is a load node, see if all the defs have the same value number.
      // If so, give the node that value number.
      //
      else if (node->getOpCode().isLoadVar()
         && !( node->getSymbol()->isConst()
            || node->getSymbol()->isConstObjectRef()))
         {
         TR::Node * defNode = getValueNumberForLoad(node);

         // This node may now have been given a value due to recursion
         //
         if (_valueNumbers.ElementAt(index) >= 0)
            {
            --_recursionDepth;
   #if DEBUG
            if (trace())
               {
               traceMsg(comp(), "Value of node %d at depth %d is %d\n",index,_recursionDepth,_valueNumbers.ElementAt(index));
               }
   #endif
            return;
            }

         if (defNode)
            {
            setValueNumber(node, defNode);
            //traceMsg(comp(), "For load node %p found defNode %p, setting value number of node to %d ",node,defNode,_valueNumbers.ElementAt(index));
            }
         else
            {
            changeValueNumber(node, _nextValue++);
            //traceMsg(comp(), "For load node %p found no defNode , setting value number of node to %d ",node,defNode,_valueNumbers.ElementAt(index));
            }
         }

      // For all other nodes look to see if there is another node whose children
      // all have the same value number. If so, use its value number.
      //
      else
         {
         bool isValidToLookIntoHash = true;
         for(int32_t k = 0 ; k < node->getNumChildren() ; k++)
            {
            if (_valueNumbers.ElementAt(node->getChild(k)->getGlobalIndex()) < 0)
               {
               isValidToLookIntoHash = false;
               break;
               }

//            traceMsg(comp(), "JIAG: for node %p, value number = %d\n",node->getChild(k),_valueNumbers.ElementAt(node->getChild(k)->getGlobalIndex()));
            }
         if(isValidToLookIntoHash)
            {
            VNHashKey nodeKey(node,this);
            CS2::HashIndex loc;
            if( _nodeHash.Locate(nodeKey,loc))
               {
               int32_t otherNodeIndex = _nodeHash.DataAt(loc);
               TR::Node * otherNode = _nodes.ElementAt(otherNodeIndex);
               TR_ASSERT((otherNode != NULL),"HASHVN :Non-null nodeTable entry expected for globalIndex:%d",otherNodeIndex);

 //              traceMsg(comp(),"JIAG1: Setting node %p value number to same number at node %p\n",node,otherNode);

               setValueNumber(node,otherNode);
               }
            else
               {
               bool added = _nodeHash.Add(nodeKey,node->getGlobalIndex());
               TR_ASSERT(added,"HASHVN: Unexpected collision for node %p",node);
               changeValueNumber(node,_nextValue++);
               }
            }
         else
            {
            changeValueNumber(node,_nextValue++);
            }
         }

#if DEBUG
      if (trace())
         {
         traceMsg(comp(), "Done processing node %p Value of node %d at depth %d is %d\n",node, index,_recursionDepth,_valueNumbers.ElementAt(index));
         }
#endif


     --_recursionDepth;
}
