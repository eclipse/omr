/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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
#include "codegen/Instruction.hpp"
#include "codegen/Instruction_inlines.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "env/CompilerEnv.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Annotations.hpp"
#include "infra/ILWalk.hpp"
#include "ras/Debug.hpp"
#include "stdarg.h"

void OMR_NORETURN TR::trap()
   {
   static char * noDebug = feGetEnv("TR_NoDebuggerBreakPoint");
   if (!noDebug)
      {
      static char *crashLogOnAssume = feGetEnv("TR_crashLogOnAssume");
      if (crashLogOnAssume)
         {
         //FIXME: this doesn't work on z/OS
         *(volatile int*)(0) = 0; // let crashlog do its thing
         }

#ifdef _MSC_VER
#ifdef DEBUG
      DebugBreak();
#else
      static char *revertToDebugbreakWin = feGetEnv("TR_revertToDebugbreakWin");
      if(revertToDebugbreakWin)
         {
         DebugBreak();
         }
      else
         {
         *(volatile int*)(0) = 1;
         }
#endif //ifdef DEBUG
#else // of _MSC_VER
      abort();
#endif // ifdef _MSC_VER
      }
   exit(1337);
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
   static void OMR_NORETURN va_fatal_assertion(const char *file, int line, const char *condition, const char *format, va_list ap)
      {
      traceAssertionFailure(file, line, condition, format, ap);
      TR::trap();
      }

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
      va_list ap;
      va_start(ap, format);
      va_fatal_assertion(file, line, condition, format, ap);
      va_end(ap);
      }

   void OMR_NORETURN fatal_assertion(const char *file, int line, const char *condition, const char *format, ...)
      {
      va_list ap;
      va_start(ap, format);
      va_fatal_assertion(file, line, condition, format, ap);
      va_end(ap);
      }

   static bool containsNode(TR::Node *node, TR::Node *target, TR::NodeChecklist& nodeChecklist)
      {
      if (node == target)
         return true;

      if (nodeChecklist.contains(node))
         return false;

      nodeChecklist.add(node);

      for (int i = 0; i < node->getNumChildren(); i++)
         {
         if (containsNode(node->getChild(i), target, nodeChecklist))
            return true;
         }

      return false;
      }

   static void markInChecklist(TR::Node *node, TR_BitVector &nodeChecklist)
      {
      if (nodeChecklist.isSet(node->getGlobalIndex()))
         return;

      nodeChecklist.set(node->getGlobalIndex());

      for (int i = 0; i < node->getNumChildren(); i++)
         markInChecklist(node->getChild(i), nodeChecklist);
      }

   void NodeAssertionContext::printContext() const
      {
      if (!_node) return;

      static bool printFullContext = feGetEnv("TR_AssertFullContext");
      TR::Compilation *comp = TR::comp();
      TR_Debug *debug = comp->findOrCreateDebug();

      fprintf(stderr, "\nNode context:\n\n");

      if (printFullContext)
         {
         debug->printIRTrees(TR::IO::Stderr, "Assertion Context", comp->getMethodSymbol());
         debug->print(TR::IO::Stderr, comp->getMethodSymbol()->getFlowGraph());
         if (comp->getKnownObjectTable())
            comp->getKnownObjectTable()->dumpTo(TR::IO::Stderr, comp);
         }
      else
         {
         fprintf(stderr, "...\n");

         TR::NodeChecklist checkedNodeChecklist(comp);

         TR_BitVector commonedNodeChecklist;
         commonedNodeChecklist.init(0, comp->trMemory(), heapAlloc, growable);

         bool foundNode = false;
         for (TR::TreeTopIterator it(comp->getStartTree(), comp); it != NULL; ++it)
            {
            if (containsNode(it.currentNode(), _node, checkedNodeChecklist))
               {
               foundNode = true;
               debug->restoreNodeChecklist(commonedNodeChecklist);
               debug->print(TR::IO::Stderr, it.currentTree());
               break;
               }
            else
               {
               markInChecklist(it.currentNode(), commonedNodeChecklist);
               }
            }

         if (!foundNode)
            fprintf(stderr, "!!! Treetop for node %p was not found !!!\n", _node);

         fprintf(stderr, "...\n(Set env var TR_AssertFullContext for full context)\n");
         }

      fflush(stderr);
      }

   void InstructionAssertionContext::printContext() const
      {
      if (!_instruction) return;

      static bool printFullContext = feGetEnv("TR_AssertFullContext");
      static int numInstructionsInContext = feGetEnv("TR_AssertNumInstructionsInContext") ?
         atoi(feGetEnv("TR_AssertNumInstructionsInContext")) : 11;
      TR_Debug *debug = TR::comp()->findOrCreateDebug();

      fprintf(stderr, "\nInstruction context:\n");

      if (printFullContext)
         {
         fprintf(stderr, "\n");
         debug->dumpMethodInstrs(TR::IO::Stderr, "Assertion Context", false, false);
         }
      else
         {
         TR::Instruction *cursor = _instruction;
         for (int i = 0; i < (numInstructionsInContext - 1) / 2 && cursor->getPrev(); i++)
            cursor = cursor->getPrev();

         if (cursor->getPrev())
            fprintf(stderr, "\n...");

         for (int i = 0; i < numInstructionsInContext && cursor; i++)
            {
            debug->print(TR::IO::Stderr, cursor);
            cursor = cursor->getNext();
            }

         if (cursor)
            fprintf(stderr, "\n...");

         fprintf(stderr, "\n(Set env var TR_AssertFullContext for full context)\n");
         }

      fflush(stderr);
      NodeAssertionContext(_instruction->getNode()).printContext();
      }

   void assert_with_instruction_detail(void *instr)
      {
      assert_with_instruction_detail(reinterpret_cast<TR::Instruction *>(instr));
      }

   void OMR_NORETURN fatal_assertion_with_detail(const AssertionContext& ctx, const char *file, int line, const char *condition, const char *format, ...)
      {
      static bool alreadyAsserting = false;

      va_list ap;
      va_start(ap, format);
      traceAssertionFailure(file, line, condition, format, ap);
      va_end(ap);

      if (!alreadyAsserting)
         {
         alreadyAsserting = true;
         ctx.printContext();
         }
      else
         {
         fprintf(stderr, "(Detected potential recursive assert, not printing context)\n");
         }

      TR::trap();
      }
   }
