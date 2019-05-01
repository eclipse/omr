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

#ifndef PPCHELPERCALLSNIPPET_INCL
#define PPCHELPERCALLSNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/RealRegister.hpp"
#include "il/SymbolReference.hpp"

#include "codegen/GCStackAtlas.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class PPCHelperCallSnippet : public TR::Snippet
   {
   TR::SymbolReference      *_destination;
   TR::LabelSymbol          *_restartLabel;

   public:

   PPCHelperCallSnippet(TR::CodeGenerator    *cg,
                        TR::Node             *node,
                        TR::LabelSymbol      *snippetlab,
                        TR::SymbolReference  *helper,
                        TR::LabelSymbol      *restartLabel=NULL)
      : TR::Snippet(cg, node, snippetlab, (restartLabel!=NULL && helper->canCauseGC())),
        _destination(helper),
        _restartLabel(restartLabel)
      {
      }

   virtual Kind getKind() { return IsHelperCall; }

   TR::SymbolReference *getDestination()             {return _destination;}
   TR::SymbolReference *setDestination(TR::SymbolReference *s) {return (_destination = s);}

   TR::LabelSymbol     *getRestartLabel() {return _restartLabel;}
   TR::LabelSymbol     *setRestartLabel(TR::LabelSymbol *l) {return (_restartLabel=l);}

   uint8_t *genHelperCall(uint8_t *cursor);
   uint32_t getHelperCallLength();

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

class PPCArrayCopyCallSnippet : public TR::PPCHelperCallSnippet
   {
   TR::RealRegister::RegNum _lengthRegNum;

   public:

   PPCArrayCopyCallSnippet(TR::CodeGenerator    *cg,
                           TR::Node             *node,
                           TR::LabelSymbol      *snippetlab,
                           TR::SymbolReference  *helper,
                           TR::RealRegister::RegNum lengthRegNum,
                           TR::LabelSymbol      *restartLabel=NULL)
      : TR::PPCHelperCallSnippet(cg, node, snippetlab, helper, restartLabel),
        _lengthRegNum(lengthRegNum)
      {
      }
   virtual Kind getKind() { return IsArrayCopyCall; }

   TR::RealRegister::RegNum getLengthRegNum() {return _lengthRegNum;}
   TR::RealRegister::RegNum
      setLengthRegNum(TR::RealRegister::RegNum r) {return (_lengthRegNum = r);}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif
