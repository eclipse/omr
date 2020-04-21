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

#include "control/Recompilation.hpp"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Link.hpp"
#include "infra/Timer.hpp"

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
