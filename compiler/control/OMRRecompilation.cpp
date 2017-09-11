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

#include "control/Recompilation.hpp"

#include <limits.h>                          // for INT_MAX
#include <stdint.h>                          // for uint16_t, uint32_t, etc
#include <stdlib.h>                          // for NULL, strtoul
#include <string.h>                          // for memset
#include "codegen/CodeGenerator.hpp"         // for CodeGenerator
#include "codegen/FrontEnd.hpp"              // for TR_FrontEnd, feGetEnv, etc
#include "compile/Compilation.hpp"           // for Compilation
#include "compile/CompilationTypes.hpp"      // for TR_Hotness
#include "compile/ResolvedMethod.hpp"        // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"  // for SymbolReferenceTable
#include "control/OptimizationPlan.hpp"      // for TR_OptimizationPlan, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"       // for TR::Options, etc
#include "env/PersistentInfo.hpp"            // for PersistentInfo
#include "env/TRMemory.hpp"                  // for TR_Link::operator new, etc
#include "env/jittypes.h"                    // for uintptrj_t
#include "il/DataTypes.hpp"                  // for etc
#include "infra/Assert.hpp"                  // for TR_ASSERT
#include "infra/Link.hpp"                    // for TR_LinkHead
#include "infra/Timer.hpp"                   // for TR_SingleTimer

class TR_OpaqueMethodBlock;
namespace TR { class Instruction; }
namespace TR { class SymbolReference; }


OMR::Recompilation::Recompilation(TR::Compilation * comp) :
   _compilation(comp)
   {
   }


void
OMR::Recompilation::shutdown()
   {
   }


TR::Recompilation *
OMR::Recompilation::self()
   {
   return static_cast<TR::Recompilation *>(this);
   }
