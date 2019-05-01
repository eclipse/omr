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

#ifndef S390HELPERCALLSNIPPET_INCL
#define S390HELPERCALLSNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class S390HelperCallSnippet : public TR::Snippet
   {
   TR::LabelSymbol           *_reStartLabel;         ///< Label of Return Address in Main Line Code.
   TR::SymbolReference      *_helperSymRef;         ///< Helper Symbol Reference.
   int32_t  sizeOfArguments;

   public:

   S390HelperCallSnippet(TR::CodeGenerator        *cg,
                         TR::Node                 *node,
                         TR::LabelSymbol           *snippetlab,
                         TR::SymbolReference      *helper,
                         TR::LabelSymbol           *restartlab = NULL,
                         int32_t                  s = 0)
      : TR::Snippet(cg, node, snippetlab, (restartlab == NULL)),
        _reStartLabel(restartlab),
        _helperSymRef(helper),
	    sizeOfArguments(s)
      {
      // If we don't have a restart label, then we must not be returning to the mainline code -
      // hence, always except.
      TR_ASSERT(restartlab || (!restartlab && helper->canCauseGC()),
                 "An exception snippet is marked as cannot cause GC");

      // Set up appropriate GC Map
      if (!restartlab)
        gcMap().setGCRegisterMask((uint32_t)0x00000000);  // everything gets clobbered if we're taking an exception.
      }

   virtual Kind getKind() { return IsHelperCall; }

   int32_t getSizeOfArguments()          {return sizeOfArguments;}
   int32_t setSizeOfArguments(int32_t s) {return sizeOfArguments = s;}

   TR::SymbolReference *getHelperSymRef()                      {return _helperSymRef;}
   TR::SymbolReference *setHelperSymRef(TR::SymbolReference *s) {return _helperSymRef = s;}

   TR::LabelSymbol *getReStartLabel()                  {return _reStartLabel;}
   TR::LabelSymbol *setReStartLabel(TR::LabelSymbol *l) {return _reStartLabel = l;}

   bool alwaysExcept()               {return _reStartLabel == NULL;}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t);
   };

}

#endif
