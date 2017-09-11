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

#ifndef OMR_MONITOR_INCL
#define OMR_MONITOR_INCL

#ifndef OMR_MONITOR_CONNECTOR
#define OMR_MONITOR_CONNECTOR
namespace OMR { class Monitor; }
namespace OMR { typedef OMR::Monitor MonitorConnector; }
#endif

#include <stdint.h>          // for int32_t, int64_t
#include "infra/Assert.hpp"  // for TR_ASSERT
#include "omrmutex.h"        // for MUTEX

namespace TR { class Monitor; }

#define NOT_IMPL { TR_ASSERT(false, "not implemented by project"); return 0; }
#define NOT_IMPL_VOID { TR_ASSERT(false, "not implemented by project"); }

namespace OMR
{

class Monitor
   {
   public:

   ~Monitor();

   TR::Monitor *self();
   static TR::Monitor *create(char *name);
   static void destroy(TR::Monitor *monitor);
   void enter();
   int32_t try_enter() NOT_IMPL;
   int32_t exit(); // returns 0 on success
   void destroy();
   void wait() NOT_IMPL_VOID;
   intptr_t wait_timed(int64_t millis, int32_t nanos) NOT_IMPL;
   void notify() NOT_IMPL_VOID;
   void notifyAll() NOT_IMPL_VOID;
   int32_t num_waiting() NOT_IMPL;
   char const *getName();
   bool init(char *name);

   private:

   void *operator new(size_t size);
   void operator delete(void *p);

   char const *_name;
   MUTEX _monitor;
   };

}

#undef NOT_IMPL
#undef NOT_IMPL_VOID

#endif
