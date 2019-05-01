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

#ifndef TR_CFGEDGE_INCL
#define TR_CFGEDGE_INCL

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"

namespace TR { class CFGNode; }

namespace TR
{

class CFGEdge : public TR_Link<CFGEdge>
   {
   public:
   TR_ALLOC(TR_Memory::CFGEdge)


   CFGEdge() : _pFrom(NULL), _pTo(NULL), _visitCount(0), _frequency(0), _id(-1)
   {}

   // Construct a normal edge between two nodes
   //

   static CFGEdge * createEdge (CFGNode *pF, CFGNode *pT, TR_Memory* trMemory, TR_AllocationKind allocKind = heapAlloc);
   static CFGEdge * createExceptionEdge (CFGNode *pF, CFGNode *pT, TR_Memory* trMemory, TR_AllocationKind allocKind = heapAlloc);
   static CFGEdge * createEdge (CFGNode *pF, CFGNode *pT, TR::Region &region);
   static CFGEdge * createExceptionEdge (CFGNode *pF, CFGNode *pT, TR::Region &reigon);

   CFGNode *getFrom() {return _pFrom;}
   CFGNode *getTo()   {return _pTo;}

   bool getCreatedByTailRecursionElimination() { return _flags.testAny(_createdByTailRecursionElimination); }
   void setCreatedByTailRecursionElimination(bool b) { _flags.set(_createdByTailRecursionElimination, b); }

   // Set the from and to nodes for this edge. Also adds this edge to the
   // appropriate successor or predecessor lists in the nodes.
   //
   void setFrom(CFGNode *pF);
   void setTo(CFGNode *pT);
   void setExceptionFrom(CFGNode *pF);
   void setExceptionTo(CFGNode *pF);
   void setFromTo(CFGNode *pF, CFGNode *pT);
   void setExceptionFromTo(CFGNode *pF, CFGNode *pT);
   vcount_t    getVisitCount()            {return _visitCount;}
   vcount_t    setVisitCount(vcount_t vc) {return (_visitCount = vc);}
   vcount_t    incVisitCount()
      {
      return ++_visitCount;
      }
   int32_t    getId()            {return _id;}
   int32_t    setId(int32_t id)  {return (_id = id);}

   int16_t    getFrequency()          { return _frequency; }

   void       setFrequency(int32_t f)
      {
      if (f >= SHRT_MAX)
         f = SHRT_MAX-1;

      _frequency = f;
      }

   void       normalizeFrequency(int32_t maxFrequency);

   private:

   //keeping this c-tor private since there is no real need to make it public right now
   CFGEdge(CFGNode *pF, CFGNode *pT);

   enum  // flags
      {
      _createdByTailRecursionElimination = 0x8000
      };


   CFGNode *_pFrom;
   CFGNode *_pTo;

   vcount_t   _visitCount;
   flags16_t  _flags;

   int16_t _frequency;

   int32_t _id;
   };

}

#endif
