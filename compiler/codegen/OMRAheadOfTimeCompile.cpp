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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/AheadOfTimeCompile.hpp"

#include "codegen/CodeGenerator.hpp"         // for CodeGenerator
#include "codegen/Relocation.hpp"            // for TR::Relocation
#include "compile/Method.hpp"                // for TR_AOTMethodInfo
#include "compile/ResolvedMethod.hpp"        // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"  // for SymbolReferenceTable
#include "compile/VirtualGuard.hpp"          // for TR_VirtualGuard
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"                   // for TR_AOTGuardSite
#endif
#include "env/jittypes.h"                    // for TR_InlinedCallSite
#include "il/Node.hpp"                       // for Node
#include "infra/Assert.hpp"                  // for TR_ASSERT
#include "infra/List.hpp"                    // for ListHeadAndTail, ListElement
#include "ras/Debug.hpp"                     // for TR_DebugBase
#include "env/CompilerEnv.hpp"

namespace TR { class SymbolReference; }

extern bool isOrderedPair(uint8_t reloType);

TR::AheadOfTimeCompile *
OMR::AheadOfTimeCompile::self()
   {
   return static_cast<TR::AheadOfTimeCompile *>(this);
   }

TR_Debug *
OMR::AheadOfTimeCompile::getDebug()
   {
   return self()->comp()->getDebug();
   }

TR_Memory *
OMR::AheadOfTimeCompile::trMemory()
   {
   return self()->comp()->trMemory();
   }

void OMR::AheadOfTimeCompile::traceRelocationOffsets(uint8_t *&cursor, int32_t offsetSize, const uint8_t *endOfCurrentRecord, bool isOrderedPair)
   {
   // Location Offsets
   int divisor;     // num of data that fit on an 80 char wide term
   uint8_t count;   // start count is number of data that could have fit so far

   if (isOrderedPair)
      {
      divisor = ((offsetSize == 2) ? 6 : 4);   // for nice output
      count = ((offsetSize == 2) ? 5 : 3);
      }
   else
      {
      divisor = ((offsetSize == 2) ? 16 : 8);   // for nice output
      count = ((offsetSize == 2) ? 11 : 6);
      }

   while (cursor < endOfCurrentRecord)
      {
      if ((count % divisor) == 0)
         {
         traceMsg(self()->comp(), "\n                                                                       ");
         }
      count++;
      if (offsetSize == 2)
         {
         if (isOrderedPair)
            {
         traceMsg(self()->comp(), "(%04x ", *(uint16_t *)cursor);
         cursor += offsetSize;
         traceMsg(self()->comp(), "%04x) ", *(uint16_t *)cursor);
            }
         else
            {
         traceMsg(self()->comp(), "%04x ", *(uint16_t *)cursor);
            }
         }
      else      // offsetSize == 4
         {
         if (isOrderedPair)
            {
         traceMsg(self()->comp(), "(%08x ", *(uint32_t *)cursor);
         cursor += offsetSize;
         traceMsg(self()->comp(), "%08x) ", *(uint32_t *)cursor);
            }
         else
            {
         traceMsg(self()->comp(), "%08x ", *(uint32_t *)cursor);
            }
         }
      cursor += offsetSize;
      }
   }

uintptr_t
findCorrectInlinedSiteIndex(void *constantPool, TR::Compilation *comp, uintptr_t currentInlinedSiteIndex)
   {
   uintptr_t cp2 = 0;

   uintptr_t inlinedSiteIndex = currentInlinedSiteIndex;
   if (inlinedSiteIndex==(uintptr_t)-1)
      {
      cp2 = (uintptr_t) comp->getCurrentMethod()->constantPool();
      }
   else
      {
      TR_InlinedCallSite &ics2 = comp->getInlinedCallSite(inlinedSiteIndex);
      TR_AOTMethodInfo *aotMethodInfo2 = (TR_AOTMethodInfo *)ics2._methodInfo;
      cp2 = (uintptr_t)aotMethodInfo2->resolvedMethod->constantPool();
      }

   bool matchFound = false;

   if ( (uintptr_t) constantPool == cp2)
      {
      matchFound = true;
      }
   else
      {
      if ((uintptr_t)constantPool == (uintptr_t)comp->getCurrentMethod()->constantPool())
         {
         matchFound = true;
         inlinedSiteIndex = (uintptr_t)-1;
         }
      else
         {
         for (uintptr_t i = 0; i < comp->getNumInlinedCallSites(); i++)
            {
            TR_InlinedCallSite &ics = comp->getInlinedCallSite(i);

            TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)ics._methodInfo;

            if ((uintptr_t)constantPool == (uintptr_t)aotMethodInfo->resolvedMethod->constantPool())
               {
               matchFound = true;
               inlinedSiteIndex = i;
               break;
               }
            }
         }
      }
   TR_ASSERT(matchFound, "couldn't find this CP in inlined site list");
   return inlinedSiteIndex;
   }

void createGuardSiteForRemovedGuard(TR::Compilation *comp, TR::Node *ifNode)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (comp->cg()->needGuardSitesEvenWhenGuardRemoved() && ifNode->isTheVirtualGuardForAGuardedInlinedCall())
      {
      TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(ifNode);

      if (virtualGuard->getKind() == TR_HCRGuard)
         {
         if (comp->getOption(TR_TraceReloCG))
            traceMsg(comp, "createGuardSiteForRemovedGuard: removing HCRGuard, no need to add AOTNOPsite, node %p\n", ifNode);
         return;
         }

      if (virtualGuard->getKind() == TR_BreakpointGuard)
         {
         if (comp->getOption(TR_TraceReloCG))
            traceMsg(comp, "createGuardSiteForRemovedGuard: removing BreakpointGuard, no need to add AOTNOPsite, node %p\n", ifNode);
         return;
         }

      TR_VirtualGuardKind removedGuardKind;
      switch (virtualGuard->getKind())
         {
         case TR_ProfiledGuard:
            removedGuardKind = TR_RemovedProfiledGuard;
            TR_ASSERT(ifNode->isProfiledGuard(), "guard for profiled guard has unexpected kind");
            break;
         case TR_InterfaceGuard:
            removedGuardKind = TR_RemovedInterfaceGuard;
            TR_ASSERT(ifNode->isInterfaceGuard(), "guard for interface guard has unexpected kind");
            break;
         case TR_NonoverriddenGuard:
         case TR_DirectMethodGuard:
            removedGuardKind = TR_RemovedNonoverriddenGuard;
            TR_ASSERT(ifNode->isNonoverriddenGuard(), "guard for nonoverridden guard has unexpected kind");
            break;
         default:
            TR_ASSERT(false, "removed virtual guard type %d on node %p not supported yet.", virtualGuard->getKind(), ifNode);
            break;
         };

      TR_AOTGuardSite *site = comp->addAOTNOPSite();
      site->setLocation(NULL);
      site->setType(removedGuardKind);
      site->setGuard(virtualGuard);
      site->setNode(NULL);

      if (comp->getOption(TR_TraceAll))
         traceMsg(comp, "createGuardSiteForRemovedGuard: removedGuardKind %d, removedGurad %p, _callNode %p, _guardNode %p, _thisClass %p, _calleeIndex %d, _byteCodeIndex %d, addedAOTNopSite %p\n",
                     removedGuardKind, virtualGuard, virtualGuard->getCallNode(), virtualGuard->getGuardNode(), virtualGuard->getThisClass(),
                     virtualGuard->getCalleeIndex(), virtualGuard->getByteCodeIndex(), site);
      }
#endif
   }
