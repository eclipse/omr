/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/AheadOfTimeCompile.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
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
