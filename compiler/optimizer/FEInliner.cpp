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

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/CallInfo.hpp"
#include "optimizer/Inliner.hpp"

class TR_OpaqueClassBlock;
class TR_PrexArgInfo;
class TR_ResolvedMethod;
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReference; }
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class TreeTop; }

TR_CallSite* TR_CallSite::create(TR::TreeTop* callNodeTreeTop,
                           TR::Node *parent,
                           TR::Node* callNode,
                           TR_OpaqueClassBlock *receiverClass,
                           TR::SymbolReference *symRef,
                           TR_ResolvedMethod *resolvedMethod,
                           TR::Compilation* comp,
                           TR_Memory* trMemory,
                           TR_AllocationKind kind,
                           TR_ResolvedMethod* caller,
                           int32_t depth,
                           bool allConsts)

   {
   TR_ASSERT(0, "static ctor TR_CallSite::create must be implemented");
   return NULL;
   }

bool TR_InlinerBase::tryToGenerateILForMethod (TR::ResolvedMethodSymbol* calleeSymbol, TR::ResolvedMethodSymbol* callerSymbol, TR_CallTarget* calltarget)
   {
   return false;
   }

bool TR_InlinerBase::inlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo, TR::TreeTop** cursorTreeTop)
   {
   TR_ASSERT(0, "TR_InlinerBase::inlineCallTarget must be implemented");
   return false;
   }

void TR_InlinerBase::getBorderFrequencies(int32_t &hotBorderFrequency, int32_t &coldBorderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode)
   {
   hotBorderFrequency = 2500;
   coldBorderFrequency = 0;
   return;
   }

int32_t TR_InlinerBase::scaleSizeBasedOnBlockFrequency(int32_t bytecodeSize, int32_t frequency, int32_t borderFrequency, TR_ResolvedMethod * calleeResolvedMethod, TR::Node *callNode, int32_t coldBorderFrequency)
   {
   int32_t maxFrequency = MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT;
   bytecodeSize = (int)((float)bytecodeSize * (float)(maxFrequency-borderFrequency)/(float)maxFrequency);
              if (bytecodeSize < 10) bytecodeSize = 10;

   return bytecodeSize;
   }

int TR_InlinerBase::checkInlineableWithoutInitialCalleeSymbol (TR_CallSite* callsite, TR::Compilation* comp)
   {
   return Unknown_Reason;
   }
