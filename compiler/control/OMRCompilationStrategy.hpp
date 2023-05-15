/*******************************************************************************
 * Copyright (c) IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef OMR_COMPILATIONSTRATEGY_INCL
#define OMR_COMPILATIONSTRATEGY_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_COMPILATIONSTRATEGY_CONNECTOR
#define OMR_COMPILATIONSTRATEGY_CONNECTOR
namespace OMR { class CompilationStrategy; }
namespace OMR { typedef OMR::CompilationStrategy CompilationStrategyConnector; }
#endif

#include "env/TRMemory.hpp"

class TR_OptimizationPlan;
struct TR_MethodToBeCompiled;
namespace TR { class Recompilation; }
namespace TR { class CompilationStrategy; }
class TR_MethodEvent; // defined in downstream project

namespace OMR
{
class OMR_EXTENSIBLE CompilationStrategy
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   TR::CompilationStrategy* self();
   CompilationStrategy() {}
   TR_OptimizationPlan *processEvent(TR_MethodEvent *event, bool *newPlanCreated) { return NULL; }
   bool adjustOptimizationPlan(TR_MethodToBeCompiled *entry, int32_t adj) {return false;}
   void beforeCodeGen(TR_OptimizationPlan *plan, TR::Recompilation *recomp) {}
   void postCompilation(TR_OptimizationPlan *plan, TR::Recompilation *recomp) {}
   void shutdown() {} // called at shutdown time; useful for stats
   bool enableSwitchToProfiling() { return true; } // turn profiling on during optimizations
   };
} // namespace TR

#endif
