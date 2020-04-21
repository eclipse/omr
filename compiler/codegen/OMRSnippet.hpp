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

#ifndef OMR_SNIPPET_INCL
#define OMR_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR
namespace OMR { class Snippet; }
namespace OMR { typedef OMR::Snippet SnippetConnector; }
#endif

#include <stdint.h>
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "infra/Flags.hpp"
#include "infra/Annotations.hpp"
#include "codegen/SnippetGCMap.hpp"

class TR_Debug;
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class LabelSymbol; }
namespace TR { class Snippet; }

namespace OMR
{

class OMR_EXTENSIBLE Snippet
   {
   public:

   TR_ALLOC(TR_Memory::Snippet)

   Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label);

   Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint);

   TR::Snippet *self();

   TR::CodeGenerator *cg() { return _cg; }
   void setCodeGenerator(TR::CodeGenerator *cg) { _cg = cg; }

   TR::Node *getNode() { return _node; }
   void setNode(TR::Node *node) { _node = node; }

   TR::LabelSymbol *getSnippetLabel() {return _snippetLabel;}
   void setSnippetLabel(TR::LabelSymbol *label);

   virtual uint8_t *emitSnippet();
   virtual int32_t setEstimatedCodeLocation(int32_t p);
   virtual uint32_t getLength(int32_t estimatedSnippetStart) = 0;
   virtual uint8_t *emitSnippetBody() = 0;

   virtual void print(TR::FILE *, TR_Debug *debug);

   void prepareSnippetForGCSafePoint();

   /////////////////////////////////////////////////////////////////////////////
   //
   // Former mixin code -- this still needs to evolve into something better
   //
   public:

   TR::Block *getBlock() { return _block; }
   void setBlock(TR::Block *block) { _block = block; }

   bool needsExceptionTableEntry() { return _flags.testAll(TO_MASK32(NeedsExceptionTableEntry)); }
   void setNeedsExceptionTableEntry() { _flags.set(TO_MASK32(NeedsExceptionTableEntry)); }
   void resetNeedsExceptionTableEntry() { _flags.reset(TO_MASK32(NeedsExceptionTableEntry)); }

   TR::SnippetGCMap &gcMap() { return _gcMap; }

   protected:

   enum
      {
      NeedsExceptionTableEntry = 0,

      NextSnippetFlag,
      MaxSnippetFlag = (sizeof(uint32_t)*8)-1
      };

   static_assert(NextSnippetFlag <= MaxSnippetFlag, "OMR::SnippetFlags too many flag bits for flag width");

   flags32_t _flags;

   private:

   TR::SnippetGCMap _gcMap;

   /////////////////////////////////////////////////////////////////////////////

   private:

   TR::CodeGenerator *_cg;
   TR::LabelSymbol *_snippetLabel;
   TR::Node *_node;

   protected:

   TR::Block *_block;

   };

}

#endif
