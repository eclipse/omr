/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "ras/ILValidationUtils.hpp"

#include <stdarg.h>

#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ILProps.hpp"
#include "il/ILOps.hpp"


TR::LiveNodeWindow::LiveNodeWindow(NodeSideTable<NodeState> &sideTable,
                                   TR_Memory *memory)
   :_sideTable(sideTable)
   ,_basis(0)
   ,_liveOffsets(10, memory, stackAlloc, growable)
   {
   }

bool TR::isILValidationLoggingEnabled(TR::Compilation *comp)
   {
   return (comp->getOption(TR_TraceILValidator));
   }

void TR::checkILCondition(TR::Node *node, bool condition,
                          TR::Compilation *comp, const char *formatStr, ...)
   {
   if (!condition)
      {
      printILDiagnostic(comp, "*** VALIDATION ERROR ***\nNode: %s n%dn\nMethod: %s\n",
                        node->getOpCode().getName(), node->getGlobalIndex(),
                        comp->signature());
      va_list args;
      va_start(args, formatStr);
      vprintILDiagnostic(comp, formatStr, args);
      va_end(args);
      printILDiagnostic(comp, "\n");
      printILDiagnostic(comp, "\n");
      if (!comp->getOption(TR_ContinueAfterILValidationError))
         {
         TR::trap();
         }
      }
   }

void TR::printILDiagnostic(TR::Compilation *comp, const char *formatStr, ...)
   {
   va_list stderr_args;
   va_start(stderr_args, formatStr);
   vfprintf(stderr, formatStr, stderr_args);
   va_end(stderr_args);
   if (comp->getOutFile() != NULL)
      {
      va_list log_args;
      va_start(log_args, formatStr);
      comp->diagnosticImplVA(formatStr, log_args);
      va_end(log_args);
      }
   }

void TR::vprintILDiagnostic(TR::Compilation *comp, const char *formatStr,
                            va_list ap)
   {
   va_list stderr_copy;
   va_copy(stderr_copy, ap);
   vfprintf(stderr, formatStr, stderr_copy);
   va_end(stderr_copy);
   if (comp->getOutFile() != NULL)
      {
      va_list log_copy;
      va_copy(log_copy, ap);
      comp->diagnosticImplVA(formatStr, log_copy);
      va_end(log_copy);
      }
   }
