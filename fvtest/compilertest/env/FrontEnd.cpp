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

#include "env/FrontEnd.hpp"

#include <limits.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/GCStackMap.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/Compilation.hpp"
#include "env/FEBase_t.hpp"
#include "env/Processors.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"
#include "runtime/CodeMetaDataPOD.hpp"
#include "runtime/StackAtlasPOD.hpp"

//#include "util_api.h"

#define RANGE_NEEDS_FOUR_BYTE_OFFSET(r) (((r) >= (USHRT_MAX   )) ? 1 : 0)

namespace TestCompiler
{

FrontEnd *FrontEnd::_instance = 0;

FrontEnd::FrontEnd()
   : TR::FEBase<FrontEnd>()
   {
   TR_ASSERT(!_instance, "FrontEnd must be initialized only once");
   _instance = this;
   }

void
FrontEnd::reserveTrampolineIfNecessary(TR::Compilation *comp, TR::SymbolReference *symRef, bool inBinaryEncoding)
   {
   // Do we handle trampoline reservations? return here for now.
   return;
   }

TR_ResolvedMethod *
FrontEnd::createResolvedMethod(TR_Memory * trMemory, TR_OpaqueMethodBlock * aMethod,
                                  TR_ResolvedMethod * owningMethod, TR_OpaqueClassBlock *classForNewInstance)
   {
   return new (trMemory->trHeapMemory()) ResolvedMethod(aMethod);
   }

intptr_t
FrontEnd::methodTrampolineLookup(TR::Compilation *comp, TR::SymbolReference *symRef, void *callSite)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


// -----------------------------------------------------------------------------





} //namespace TestCompiler
