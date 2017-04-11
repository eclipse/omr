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

#ifndef TIMER_INCL
#define TIMER_INCL

#include <stdint.h>          // for uint32_t, uint64_t
#include "env/IO.hpp"
#include "env/TRMemory.hpp"  // for TR_Memory, etc
#include "infra/Array.hpp"   // for TR_Array

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
