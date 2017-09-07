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

#ifndef OMR_Z_S390SNIPPETS_INCL
#define OMR_Z_S390SNIPPETS_INCL

#include "codegen/Snippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class S390RestoreGPR7Snippet : public TR::Snippet
   {
   TR::LabelSymbol * _targetLabel;

public:
   S390RestoreGPR7Snippet(TR::CodeGenerator* cg, TR::Node *c, TR::LabelSymbol *lab, TR::LabelSymbol *targetLabel)
      : TR::Snippet(cg, c, lab, false), _targetLabel(targetLabel)
      {
      }

   S390RestoreGPR7Snippet(TR::CodeGenerator* cg, TR::Node *c, TR::LabelSymbol *lab, TR::Snippet *targetSnippet)
      : TR::Snippet(cg, c, lab, false), _targetLabel(targetSnippet->getSnippetLabel())
      {
      }

   TR::LabelSymbol *getTargetLabel() { return _targetLabel; }
   TR::LabelSymbol *setTargetLabel ( TR::LabelSymbol * targetLabel)
      {
      return _targetLabel = targetLabel;
      }

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual uint8_t *emitSnippetBody();
   virtual Kind getKind() { return IsRestoreGPR7; }

   };

}

#endif
