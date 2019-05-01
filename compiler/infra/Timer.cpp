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

//
// Methods to implement timing measurements for various phases of the JIT.
//
// TR_SingleTimer is used in production mode (for recompilation)
//

#include "infra/Timer.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "infra/Array.hpp"

void TR_SingleTimer::initialize(const char *title, TR_Memory * trMemory)
   {
   if (title)
      {
      _phaseTitle = (char *)trMemory->allocateHeapMemory( strlen(title)+1 );
      strcpy(_phaseTitle, title);
      }
   else
      _phaseTitle = NULL;
   _total = 0;
   _start = 0;
   _timerRunning = false;
   }

void TR_SingleTimer::startTiming(TR::Compilation *comp)
   {
   if (!_timerRunning)
      {
      _start = TR::Compiler->vm.getHighResClock(comp);
      _timerRunning = true;
      }
   }

uint32_t TR_SingleTimer::stopTiming(TR::Compilation *comp)
   {
   if (_timerRunning)
      {
      _total += TR::Compiler->vm.getHighResClock(comp) - _start;
      _timerRunning = false;
      }
   return (uint32_t) _total;
   }


double TR_SingleTimer::secondsTaken()
   {
   uint64_t frequency = TR::Compiler->vm.getHighResClockResolution();
   return frequency ? (double)_total / (double)frequency : 0.0;
   }
