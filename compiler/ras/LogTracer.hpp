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

#ifndef TRLOGTRACER
#define TRLOGTRACER

#include <stdint.h>

namespace TR { class Compilation; }
namespace TR { class Optimization; }

#define heuristicTrace(r, ...) \
      do { \
         if ((r)->heuristicLevel()) { (r)->alwaysTraceM(__VA_ARGS__); } \
      } while (0)


#define debugTrace(r, ...) \
      do { \
         if ((r)->debugLevel())\
            {\
            (r)->alwaysTraceM(__VA_ARGS__);\
            }\
      } while (0)

#define alwaysTrace(r, ...) \
      (r)->alwaysTraceM(__VA_ARGS__);

class TR_LogTracer
   {
public:
   TR_LogTracer(TR::Compilation *comp, TR::Optimization *opt);

   TR::Compilation * comp()                   { return _comp; }

   // determine the tracing level

   void setTraceLevelToDebug()                     { _traceLevel = trace_debug;  }
   bool debugLevel()                              { return _traceLevel == trace_debug; }
   bool heuristicLevel()                          { return _traceLevel >= trace_heuristic; }      // the > ensures heuristic tracing gets turned on for debug as well

   // trace statements for specific tracing levels
   void alwaysTraceM (const char *fmt, ...);                      // NEVER call this method directly. Use macros defined above.

protected:

   enum traceLevel
       {
       trace_notrace,
       trace_full,            //traceFull option used in commandline
       trace_heuristic,       //traceInlining option used in commandline
       trace_debug            //debugInlining option used in commandline
       };

   TR::Compilation *        _comp;
   uint8_t                 _traceLevel;

   };

#endif
