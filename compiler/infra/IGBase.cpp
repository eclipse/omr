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

#include "infra/IGBase.hpp"

#include "infra/Assert.hpp"

IMIndex TR_IGBase::_highIndexTable[NUM_PRECOMPUTED_HIGH_INDICES] =
   {    0,
        0,    1,    3,    6,   10,   15,   21,   28,     // 8
       36,   45,   55,   66,   78,   91,  105,  120,     // 16
      136,  153,  171,  190,  210,  231,  253,  276,     // 24
      300,  325,  351,  378,  406,  435,  465,  496,     // 32
      528,  561,  595,  630,  666,  703,  741,  780,     // 40
      820,  861,  903,  946,  990, 1035, 1081, 1128,     // 48
     1176, 1225, 1275, 1326, 1378, 1431, 1485, 1540,     // 56
     1596, 1653, 1711, 1770, 1830, 1891, 1953        };  // 63


IMIndex TR_IGBase::getNodePairToBVIndex(IGNodeIndex index1,
                                        IGNodeIndex index2)
   {
   IMIndex low, high;

   //TR_ASSERT((int16_t)index1 >= 0, "getNodePairToBVIndex: invalid node index1: %d\n", index1);
   //TR_ASSERT((int16_t)index2 >= 0, "getNodePairToBVIndex: invalid node index2: %d\n", index2);

   // Order the indices.
   //
   if (index1 < index2)
      {
      low = index1;
      high = index2;
      }
   else
      {
      low = index2;
      high = index1;
      }

   if (high < NUM_PRECOMPUTED_HIGH_INDICES)
      {
      return low + _highIndexTable[high];
      }
   else
      {
      return (((high-1)*high) >> 1) + low;
      }
   }


// Assumes node indices are passed in ascending order.
//
IMIndex TR_IGBase::getOrderedNodePairToBVIndex(IGNodeIndex low,
                                               IGNodeIndex high)
   {
   //   TR_ASSERT((int16_t)low >= 0,  "getOrderedNodePairToBVIndex: invalid node index low: %d\n", low);
   //   TR_ASSERT((int16_t)high >= 0, "getOrderedNodePairToBVIndex: invalid node index high: %d\n", high);
   TR_ASSERT(high > low, "getOrderedNodePairToBVIndex: high <= low  :  %d <= %d\n", high, low);

   if (high < NUM_PRECOMPUTED_HIGH_INDICES)
      {
      return (IMIndex)low + _highIndexTable[high];
      }
   else
      {
      return (((IMIndex)(high-2)*(IMIndex)(high-1)) >> 1) + (IMIndex)low - 1;
      }
   }
