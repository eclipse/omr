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

#ifndef OMR_RECOMPILATION_INCL
#define OMR_RECOMPILATION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_RECOMPILATION_CONNECTOR
#define OMR_RECOMPILATION_CONNECTOR
namespace OMR { class Recompilation; }
namespace OMR { typedef OMR::Recompilation RecompilationConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"

namespace TR { class Instruction; }
namespace TR { class Recompilation; }

namespace OMR
{

class Recompilation
   {
public:

   TR_ALLOC(TR_Memory::Recompilation)

   TR::Recompilation *self();

   virtual TR::Instruction *generatePrePrologue() { return 0; }
   virtual TR::Instruction *generatePrologue(TR::Instruction *) { return 0; }

   bool isProfilingCompilation() { return false; }
   bool couldBeCompiledAgain() { return false; }
   bool shouldBeCompiledAgain() { return false; }

   void startOfCompilation() { return; }
   void beforeOptimization() { return; }
   void beforeCodeGen() { return; }
   void endOfCompilation() { return; }
   virtual void postCompilation() { return; }

   static void shutdown();

protected:

   Recompilation(TR::Compilation *);

   TR::Compilation *comp() { return _compilation; }
   TR_Memory *trMemory() { return comp()->trMemory(); }
   TR_HeapMemory trHeapMemory() { return trMemory(); }

   TR::Compilation *_compilation;

   };

}

#endif
