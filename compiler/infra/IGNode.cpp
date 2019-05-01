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

#include "infra/IGNode.hpp"

#include "compile/Compilation.hpp"
#include "infra/List.hpp"

void TR_IGNode::decWorkingDegreeOfNeighbours()
   {
   ListIterator<TR_IGNode> iterator(&getAdjList());
   TR_IGNode *cursor = iterator.getFirst();

   while (cursor)
      {
      if (!cursor->isRemovedFromIG())
         {
         cursor->decWorkingDegree();
         }

      cursor = iterator.getNext();
      }

   setWorkingDegree(0);
   }


#ifdef DEBUG
void TR_IGNode::print(TR::Compilation * comp)
   {
   diagnostic("[ %5d ]  :  IGnode addr:   %p", _index, this);
   diagnostic("\n              data addr:     %p", _pEntity);
   diagnostic("\n              flags [%04x]:  ", _flags.getValue());

   if (_flags.getValue() == 0)
      {
      diagnostic("none");
      }
   else
      {
      if (isRemovedFromIG()) {diagnostic("isRemovedFromIG ");}
      }

   diagnostic("\n              ------------------------------------------------------------------");
   diagnostic("\n              colour:              ");

   if (_colour == UNCOLOURED)
      {
      diagnostic("uncoloured");
      }
   else
      {
      diagnostic("%08x", _colour);
      }

   diagnostic("\n              degree:              %d", _degree);

   diagnostic("\n              interfering ranges:");

   int32_t icount = 0;
   if (_degree > 0)
      {
#if 0
      TR_BitVector *adjSet = new (STACK_NEW) TR_BitVector(ig->getNumNodes(), stackAlloc);

      ListIterator<TR_IGNode> iterator(&getAdjList());
      TR_IGNode *adjCursor = iterator.getFirst();

      while (adjCursor)
         {
         adjSet->set(adjCursor->getIndex());
         adjCursor = iterator.getNext();
         }

      int32_t              igNodeIndex;
      TR_BitVectorIterator bvi(*adjSet);

      while (bvi.hasMoreElements())
         {
         igNodeIndex = bvi.getNextElement();
         diagnostic("\n                 #%-3d [ %5d : %p ]",
                     ++icount, igNodeIndex, ig->getNodeTable(igNodeIndex));
         }
#else
      ListIterator<TR_IGNode> iterator(&getAdjList());
      TR_IGNode *adjCursor = iterator.getFirst();

      while (adjCursor)
         {
         diagnostic("\n                 #%-3d [ %5d : %p ]", ++icount, adjCursor->getIndex(), adjCursor);
         adjCursor = iterator.getNext();
         }
#endif

      }
   else
      {
      diagnostic("\n                 [ none ]");
      }

   diagnostic("\n\n");
   }
#endif
