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

#ifndef X86HELPERCALLSNIPPET_INCL
#define X86HELPERCALLSNIPPET_INCL

#include <stdint.h>
#include "codegen/Snippet.hpp"
#include "il/Node.hpp"
#include "x/codegen/RestartSnippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class SymbolReference; }

namespace TR {

class X86HelperCallSnippet : public TR::X86RestartSnippet
   {
   TR::Node            *_callNode;
   TR::SymbolReference *_destination;
   uint8_t            *_callInstructionBufferAddress;
   int32_t             _stackPointerAdjustment; // For helper calls that need a stack frame temporarily deallocated

   // If the displacement of the helper call instruction might be patched dynamically.
   //
   bool                _alignCallDisplacementForPatching;

   public:

   X86HelperCallSnippet(TR::CodeGenerator   *cg,
                        TR::Node            *node,
                        TR::LabelSymbol      *restartLabel,
                        TR::LabelSymbol      *snippetLabel,
                        TR::SymbolReference *helper,
                        int32_t             stackPointerAdjustment=0);

   X86HelperCallSnippet(TR::CodeGenerator   *cg,
                        TR::LabelSymbol      *restartLabel,
                        TR::LabelSymbol      *snippetLabel,
                        TR::Node            *callNode,
                        int32_t             stackPointerAdjustment=0);

   virtual Kind getKind() { return IsHelperCall; }

   TR::Node            *getCallNode()                         {return _callNode;}
   TR::SymbolReference *getDestination()                      {return _destination;}
   TR::SymbolReference *setDestination(TR::SymbolReference *s) {return (_destination = s);}
   int32_t             getStackPointerAdjustment()           {return _stackPointerAdjustment;}

   static int32_t branchDisplacementToHelper(uint8_t *callInstructionAddress, TR::SymbolReference *helper, TR::CodeGenerator *cg);

   int32_t getOffset() {return _offset;}

   uint8_t *getCallInstructionBufferAddress() {return _callInstructionBufferAddress;}
   bool getAlignCallDisplacementForPatching() {return _alignCallDisplacementForPatching;}
   void setAlignCallDisplacementForPatching(bool a) {_alignCallDisplacementForPatching = a;}

   void addMetaDataForLoadAddrArg(uint8_t *buffer, TR::Node *child);

   virtual uint8_t *genHelperCall(uint8_t *buffer);

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   private:

   int32_t _offset; // special field for the jitReportMethodEnter helper
   };

}

#endif
