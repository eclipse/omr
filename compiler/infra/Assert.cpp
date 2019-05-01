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

#include "infra/Assert.hpp"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "env/CompilerEnv.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Annotations.hpp"
#include "ras/Debug.hpp"
#include "stdarg.h"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif

void OMR_NORETURN TR::trap()
   {
   static char * noDebug = feGetEnv("TR_NoDebuggerBreakPoint");
   if (noDebug)
      {
      exit(1337);
      }
   else
      {
      static char *crashLogOnAssume = feGetEnv("TR_crashLogOnAssume");
      if (crashLogOnAssume)
         {
         //FIXME: this doesn't work on z/OS
         *(volatile int*)(0) = 0; // let crashlog do its thing
         }

#ifdef _MSC_VER
      static char *revertToDebugbreakWin = feGetEnv("TR_revertToDebugbreakWin");
#ifdef DEBUG
      DebugBreak();
#else
      if(revertToDebugbreakWin)
         DebugBreak();
      else
         *(int*)(NULL) = 1;
#endif
#else // of _MSC_VER

      assert(0);

#endif // ifdef _MSC_VER
      }
   }


static void traceAssertionFailure(const char * file, int32_t line, const char *condition, const char *s, va_list ap)
   {
   TR::Compilation *comp = TR::comp();

   if (!condition) condition = "";

   // Default is to always print to screen
   bool printOnscreen = true;

   if (printOnscreen)
      {
      fprintf(stderr, "Assertion failed at %s:%d: %s\n", file, line, condition);

      if (comp && TR::isJ9())
        fprintf(stderr, "%s\n", TR::Compiler->debug.extraAssertMessage(comp));

      if (s)
         {
         fprintf(stderr, "\t");
         va_list copy;
         va_copy(copy, ap);
         vfprintf(stderr, s, copy);
         va_end(copy);
         fprintf(stderr, "\n");
         }

      if (comp)
         {
         const char *methodName =
               comp->signature();
         const char *hotness = comp->getHotnessName();
         bool profiling = false;
         if (comp->getRecompilationInfo() && comp->getRecompilationInfo()->isProfilingCompilation())
            profiling = true;

         fprintf(stderr, "compiling %s at level: %s%s\n", methodName, hotness, profiling?" (profiling)":"");
         }

      TR_Debug::printStackBacktrace();

      fprintf(stderr, "\n");

      fflush(stderr);
      }

   // this diagnostic call adds useful info to log file and flushes it, if we're tracing the current method
   if (comp)
      {
      comp->diagnosticImpl("Assertion failed at %s:%d:%s", file, line, condition);
      if (s)
         {
         comp->diagnosticImpl(":\n");
         va_list copy;
         va_copy(copy, ap);
         comp->diagnosticImplVA(s, copy);
         va_end(copy);
         }
      comp->diagnosticImpl("\n");
      }
   }

namespace TR
   {
   void assertion(const char *file, int line, const char *condition, const char *format, ...)
      {
      TR::Compilation *comp = TR::comp();
      if (comp)
         {
         // TR_IgnoreAssert: ignore nonfatal assertion failures.
         if (comp->getOption(TR_IgnoreAssert))
            return;
         // TR_SoftFailOnAssume: on nonfatal assertion failure, cancel the compilation without crashing the process.
         if (comp->getOption(TR_SoftFailOnAssume))
            comp->failCompilation<TR::AssertionFailure>("Assertion Failure");
         }
      fatal_assertion(file, line, condition, format);
      }

   void OMR_NORETURN fatal_assertion(const char *file, int line, const char *condition, const char *format, ...)
      {
      va_list ap;
      va_start(ap, format);
      traceAssertionFailure(file, line, condition, format, ap);
      va_end(ap);
      TR::trap();
      }
   }
