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

#include "ras/LogTracer.hpp"

#include <stdarg.h>                            // for va_list
#include "compile/Compilation.hpp"             // for Compilation
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "ras/Debug.hpp"                       // for TR_DebugBase

TR_LogTracer::TR_LogTracer(TR::Compilation *comp, TR::Optimization *opt) : _comp(comp), _traceLevel(trace_notrace)
   {

   if(opt)
      {
      if(opt->trace())
         _traceLevel = trace_heuristic;
      else if (comp->getDebug())
         _traceLevel = trace_full;
      }
   // Class should be extended to take advantage of trace_debug, or maybe more changes to tracing infrastructure

   }

//This method should never be called directly.  Should be called through Macros defined at top of LogTracer.hpp
void TR_LogTracer::alwaysTraceM ( const char * fmt, ...)
   {
   if( !comp()->getDebug() )
      return;
   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

   //traceMsg(comp(), "%s\n",str);
   comp()->getDebug()->traceLnFromLogTracer(str);

   return;
   }
