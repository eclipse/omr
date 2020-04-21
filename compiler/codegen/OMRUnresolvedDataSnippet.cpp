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

#include "codegen/UnresolvedDataSnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "codegen/UnresolvedDataSnippet_inlines.hpp"
#include "il/LabelSymbol.hpp"

namespace TR { class Node; }
namespace TR { class SymbolReference; }

OMR::UnresolvedDataSnippet::UnresolvedDataSnippet(TR::CodeGenerator * cg,
                                                  TR::Node * node,
                                                  TR::SymbolReference * symRef,
                                                  bool isStore,
                                                  bool isGCSafePoint) :
   Snippet(cg, node, generateLabelSymbol(cg)),
      _dataSymbolReference(symRef),
      _dataReferenceInstruction(NULL),
      _addressOfDataReference(0)
   {
   if (isStore)
      {
      setUnresolvedStore();
      }

   if (isGCSafePoint)
      {
      self()->prepareSnippetForGCSafePoint();
      }
   }

TR::UnresolvedDataSnippet *
OMR::UnresolvedDataSnippet::create(TR::CodeGenerator * cg, TR::Node * node, TR::SymbolReference *s, bool isStore, bool canCauseGC)
   {
   return new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, s, isStore, canCauseGC);
   }

#ifndef J9_PROJECT_SPECIFIC

#include "ras/Debug.hpp"

/*
 * This function is needed because the debug printing infrastructure is separate
 * from this class hierarchy.  At present, the debug print capability is very
 * J9-specific and therefore a simple clean version is needed for non-J9 builds.
 */

void
TR_Debug::print(TR::FILE *pOutFile, TR::UnresolvedDataSnippet * snippet)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));
   }

#endif

