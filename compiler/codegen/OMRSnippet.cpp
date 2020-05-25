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

#include <stddef.h>
#include "il/LabelSymbol.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/CodeGenerator.hpp"

OMR::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label,
      bool isGCSafePoint) :
   _cg(cg), _node(node), _snippetLabel(label), _block(0), _flags(0)
   {
   self()->setSnippetLabel(label);

   if (isGCSafePoint)
      {
      self()->prepareSnippetForGCSafePoint();
      }

   self()->gcMap().setGCRegisterMask(0xFF00FFFF);
   }


OMR::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label) :
   _cg(cg), _node(node), _snippetLabel(label), _block(0), _flags(0)
   {
   self()->setSnippetLabel(label);
   self()->gcMap().setGCRegisterMask(0xFF00FFFF);
   }

TR::Snippet *
OMR::Snippet::self()
   {
   return static_cast<TR::Snippet *>(this);
   }

void
OMR::Snippet::setSnippetLabel(TR::LabelSymbol *label)
   {
   if (_snippetLabel)
      {
      _snippetLabel->setSnippet(NULL);
      }

   label->setSnippet(static_cast<TR::Snippet *>(this));
   _snippetLabel = label;
   }


int32_t
OMR::Snippet::setEstimatedCodeLocation(int32_t p)
   {
   return self()->getSnippetLabel()->setEstimatedCodeLocation(p);
   }


uint8_t *
OMR::Snippet::emitSnippet()
   {
   return self()->emitSnippetBody();
   }

void
OMR::Snippet::prepareSnippetForGCSafePoint()
   {
   self()->gcMap().setGCSafePoint();
   self()->setBlock(self()->cg()->getCurrentEvaluationBlock());
   self()->setNeedsExceptionTableEntry();
   }

void
OMR::Snippet::print(TR::FILE* f, TR_Debug* debug)
   {
   uint8_t* cursor = self()->getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(f, self()->getSnippetLabel(), cursor, "<Unknown Snippet>");

   for (auto i = 0; i < self()->getLength(0) / sizeof(uint64_t); ++i)
      {
      debug->printPrefix(f, NULL, cursor, sizeof(uint64_t));
      cursor += sizeof(uint64_t);
      }

   if (self()->getLength(0) % sizeof(uint64_t) != 0)
      {
      debug->printPrefix(f, NULL, cursor, self()->getLength(0) % sizeof(uint64_t));
      }
   }
