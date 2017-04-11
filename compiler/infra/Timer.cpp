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

//
// Methods to implement timing measurements for various phases of the JIT.
//
// TR_SingleTimer is used in production mode (for recompilation)
//

#include "infra/Timer.hpp"

#include <stdint.h>              // for uint32_t
#include <stdio.h>               // for sprintf, NULL
#include <string.h>              // for strcpy, strlen
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"      // for TR_Memory
#include "infra/Array.hpp"       // for TR_Array

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

