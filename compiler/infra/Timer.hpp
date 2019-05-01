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

#ifndef TIMER_INCL
#define TIMER_INCL

#include <stdint.h>
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "infra/Array.hpp"

#include "infra/HashTab.hpp"

namespace TR { class Compilation; }

class TR_SingleTimer
   {
   public:
   TR_ALLOC(TR_Memory::SingleTimer);

   void     startTiming(TR::Compilation *);
   void     initialize(const char *title, TR_Memory *);

   uint32_t stopTiming(TR::Compilation *);

   char    *title()                       { return _phaseTitle; }
   uint64_t timeTaken()                   { return _total; }
   double   secondsTaken();
   bool isTimerRunning() const            { return _timerRunning; }

   private:

   char    *_phaseTitle;
   uint64_t _start,
            _total;
   bool     _timerRunning;
   };

#endif
