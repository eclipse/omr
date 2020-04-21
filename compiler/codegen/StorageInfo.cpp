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

#include "codegen/StorageInfo.hpp"

#include <algorithm>
#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

char *TR_StorageInfo::TR_StorageClassNames[TR_NumStorageClassTypes] =
   {
   "<unknown_class>",
   "<direct_mapped_auto>",
   "<direct_mapped_static>",
   "<static_base_address>",
   "<private_static_base_address>",
   };

char *TR_StorageInfo::TR_StorageOverlapKindNames[TR_NumOverlapTypes] =
   {
   "NoOverlap",
   "MayOverlap",
   "TR_PostPosOverlap",
   "TR_SamePosOverlap",
   "TR_DestructiveOverlap",
   "TR_PriorPosOverlap",
   };

TR_StorageInfo::TR_StorageInfo(TR::Node *node, size_t length, TR::Compilation *c) :
   _node(node),
   _length(length),
   _address(NULL),
   _symRef(NULL),
   _offset(0),
   _class(TR_UnknownClass),
   _comp(c)
   {
   if (node->getOpCode().isLoadVarOrStore())
      populateLoadOrStore(node);
   else if (node->getType().isAddress())
      populateAddress(node);
   else
      TR_ASSERT(false,"node %s (%p) should be a load or store or address\n",node->getOpCode().getName(),node);
   }

void
TR_StorageInfo::print()
   {
   traceMsg(comp(),"\t\t\t%s (%p) len %d: addr %s (%p) symRef #%d, offset %d, class %s\n",
      _node->getOpCode().getName(),_node,
      _length,
      _address?_address->getOpCode().getName():"NULL",_address,
      _symRef?_symRef->getReferenceNumber():-1,_offset,getName());
   }

void
TR_StorageInfo::populateAddress(TR::Node *address)
   {
   _address = address;
   if (comp()->cg()->isSupportedAdd(_address) &&
       _address->getFirstChild()->getNumChildren() == 0 &&
       _address->getSecondChild()->getOpCode().isLoadConst())
      {
      _offset += _address->getSecondChild()->get64bitIntegralValue();
      _address = _address->getFirstChild();
      }
   else if (comp()->cg()->isSupportedAdd(_address) &&
            comp()->cg()->isSupportedAdd(_address->getFirstChild()) &&
            _address->getFirstChild()->getFirstChild()->getNumChildren() == 0 &&
            _address->getFirstChild()->getSecondChild()->getOpCode().isLoadConst() &&
            _address->getSecondChild()->getOpCode().isLoadConst())
      {
      // aiadd
      //    aiadd
      //       aload
      //       iconst
      //    iconst
      _offset += _address->getSecondChild()->get64bitIntegralValue();
      _offset += _address->getFirstChild()->getSecondChild()->get64bitIntegralValue();
      _address = _address->getFirstChild()->getFirstChild();
      }

   if (_address->getOpCode().hasSymbolReference() &&
       _address->getSymbolReference())
      {
      _symRef = _address->getSymbolReference();

      if (_address->getOpCodeValue() == TR::loadaddr)
         {
         if (_symRef->getSymbol()->isAuto())
            {
            _class = TR_DirectMappedAuto;
            _offset += _symRef->getOffset();
            }
         else if (_symRef->getSymbol()->isStatic())
            {
            _class = TR_DirectMappedStatic;
            _offset += _symRef->getOffset();
            }
         }
      }
   }

void
TR_StorageInfo::populateLoadOrStore(TR::Node *loadOrStore)
   {
   if (loadOrStore == NULL)
      return;

   if (loadOrStore->getOpCode().isIndirect())
      {
      //Nothing can be concluded from TR::aloadi
      if (loadOrStore->getOpCodeValue() != TR::aloadi)
         {
         _offset += loadOrStore->getSymbolReference()->getOffset();
         populateAddress(loadOrStore->getFirstChild());
         }
      }
   else // direct store
      {
       if (loadOrStore->getOpCodeValue() == TR::aload)
          {
          //Nothing can be concluded from TR::aload
          }
      else if (loadOrStore->getSymbol()->isAuto())
         {
         _class = TR_DirectMappedAuto;
         _symRef = loadOrStore->getSymbolReference();
         _offset += _symRef->getOffset();
         }
      else if (loadOrStore->getSymbol()->isStatic())
         {
         _class = TR_DirectMappedStatic;
         _symRef = loadOrStore->getSymbolReference();
         _offset += _symRef->getOffset();
         }
      }
   }

// Returns details on the overlap between the two.
TR_StorageOverlapKind
TR_StorageInfo::mayOverlapWith(TR_StorageInfo *info)
   {
   if (comp()->cg()->traceBCDCodeGen())
      {
      traceMsg(comp(),"\t\toverlapCheck between:\n");
      print();
      info->print();
      }

   // lengths and offset do not matter in these first two cases as disjointness is known from the storage classes alone
   bool symbolMismatch = _symRef && info->_symRef && _symRef->getSymbol() != info->_symRef->getSymbol();
   if (symbolMismatch &&
       _class == TR_DirectMappedAuto && info->_class == TR_DirectMappedAuto)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\toverlap=false : autoDirectMapped and diff symbols (#%d (%p) and #%d (%p))\n",
            _symRef->getReferenceNumber(),_symRef->getSymbol(),info->_symRef->getReferenceNumber(),info->_symRef->getSymbol());

      return TR_NoOverlap; // separately directly mapped autos from loadaddrs or direct stores/loads
      }
   else if (symbolMismatch &&
            _class == TR_DirectMappedStatic && info->_class == TR_DirectMappedStatic)
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\toverlap=false : staticDirectMapped and diff symbols (#%d (%p) and #%d (%p))\n",
            _symRef->getReferenceNumber(),_symRef->getSymbol(),info->_symRef->getReferenceNumber(),info->_symRef->getSymbol());

      return TR_NoOverlap; // separately directly mapped autos from loadaddrs or cached static aloads
      }
   else if ((_class == TR_DirectMappedAuto     && info->_class == TR_DirectMappedStatic)       ||
            (_class == TR_DirectMappedAuto     && info->_class == TR_StaticBaseAddress)        ||
            (_class == TR_DirectMappedAuto     && info->_class == TR_PrivateStaticBaseAddress) ||

            (_class == TR_DirectMappedStatic   && info->_class == TR_DirectMappedAuto)         ||

            (_class == TR_StaticBaseAddress    && info->_class == TR_DirectMappedAuto)         ||
            (_class == TR_StaticBaseAddress    && info->_class == TR_PrivateStaticBaseAddress) ||

            (_class == TR_PrivateStaticBaseAddress  && info->_class == TR_DirectMappedAuto)    ||
            (_class == TR_PrivateStaticBaseAddress  && info->_class == TR_StaticBaseAddress))
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\toverlap=false : diff storage classes (%s and %s)\n",getName(),info->getName());

      return TR_NoOverlap; // directly mapped autos from loadaddrs or direct stores cannot interfere with static addresses
                           // note that aload from autos and static addresses certainly can as the aload may contain a static address
      }

   // TR_StaticBaseAddress and TR_DirectMappedStatic are unlikely to be found in the same compile unit (different fundamental ways
   // of accessing statics) but if they are both present then they may overlap as the TR_StaticBaseAddress points to the start of the
   // the static area and TR_DirectMappedStatic points to a particular static symbol

   if (_length == 0 || info->_length == 0) // unknown lengths, can do no more
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\toverlap=true : unknown lengths (%d and %d)\n",_length,info->_length);

      return TR_MayOverlap; //Possible overlap
      }

   // next only consider cases where classes are the same but the lengths and offsets may differ
   bool compareRanges = false;
   if (_symRef && info->_symRef &&
       _symRef == info->_symRef)
      {
      if (_class == TR_StaticBaseAddress && info->_class == TR_StaticBaseAddress)
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\t\tcompareRanges : staticBaseAddress case\n");
         compareRanges = true;
         }
      else if (_class == TR_PrivateStaticBaseAddress && info->_class == TR_PrivateStaticBaseAddress)
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\t\tcompareRanges : privateStaticBaseAddress case\n");
         compareRanges = true;
         }
      else if (_class == TR_DirectMappedStatic && info->_class == TR_DirectMappedStatic)
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\t\tcompareRanges : directMappedStatic case\n");
         compareRanges = true;
         }
      else if (_class == TR_DirectMappedAuto && info->_class == TR_DirectMappedAuto)
         {
         if (comp()->cg()->traceBCDCodeGen())
            traceMsg(comp(),"\t\t\tcompareRanges : directMappedAuto case\n");
         compareRanges = true;
         }
      }

   // finally just compare address nodes (of any kind)
   if (!compareRanges &&
       _address && info->_address &&
       comp()->cg()->nodeMatches(_address, info->_address))
      {
      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\t\tcompareRanges : nodes match case (%s (%p) and %s (%p))\n",
            _address->getOpCode().getName(),_address,info->_address->getOpCode().getName(),info->_address);
      compareRanges = true;
      }

   if (compareRanges)
      {
      intptr_t startOne = _offset;
      intptr_t endOne   = startOne + _length;

      intptr_t startTwo = info->_offset;
      intptr_t endTwo   = startTwo + info->_length;

      intptr_t overlapStart = std::max(startOne, startTwo);
      intptr_t overlapEnd = std::min(endOne, endTwo);

      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\t\tcompareRanges : range1 %d->%d vs range2 %d->%d --> overlap range %d->%d\n",
            startOne,endOne,startTwo,endTwo,overlapStart,overlapEnd);

      TR_StorageOverlapKind overlap = TR_NoOverlap;

      if (overlapStart >= overlapEnd) // i.e. is this an impossible (>) or trivial range (==) with start >= end? If so then there is no overlap
         {
         overlap = TR_NoOverlap;
         }
      else
         {
         // If overlapStart < overlapEnd then this is a valid range and there is overlap (overlapStart->overlapEnd is the co-incident)
         if (startOne == startTwo)
            {
            // startOne == startTwo == a
            // src: abcde
            // dst: abc
            overlap = TR_SamePosOverlap;
            }
         else if (startOne == overlapStart)
            {
            // startOne == overlapStart == c
            // src: abcde
            // dst:   cd
            overlap = TR_PostPosOverlap;
            }
         else
            {
            if (startOne + std::min(_length, info->_length) - 1 >= overlapStart)
               {
               // 1 + std::min(3,6) - 1 >= 3 --> 3 >= 3
               //        123456
               // src:   abcdef
               // dst:     cdef
               // after copy from src to dst for 4 (dst) bytes
               //    :   abababab  (i.e. result not the same as if a temp was used in which case answer would be ababcd)
               overlap = TR_DestructiveOverlap;
               }
            else
               {
               // src:   abcdef
               // dst:      def
               // after copy from src to dst for 3 (dst) bytes
               //    :   abcabc (i.e. result is the same as if a temp was used)
               overlap = TR_PriorPosOverlap;
               }
            }
         }

      if (comp()->cg()->traceBCDCodeGen())
         traceMsg(comp(),"\t\toverlap=%s (%s) : overlap range %d->%d is %spossible\n",overlap?"true":"false",getName(overlap),overlapStart,overlapEnd,overlap?"":"im");

      return overlap;
      }

   if (comp()->cg()->traceBCDCodeGen())
      traceMsg(comp(),"\t\toverlap=true : no pattern matched case\n");

   return TR_MayOverlap; //Possible overlap
   }
