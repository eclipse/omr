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

#ifndef IA32FPCONVERSIONSNIPPET_INCL
#define IA32FPCONVERSIONSNIPPET_INCL

#include <stdint.h>
#include "codegen/RealRegister.hpp"
#include "codegen/Snippet.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "x/codegen/RestartSnippet.hpp"
#include "codegen/X86Instruction.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class X86FPConversionSnippet : public TR::X86RestartSnippet
   {
   TR::SymbolReference *_helperSymRef;

   public:

   X86FPConversionSnippet(TR::CodeGenerator   *codeGen,
                          TR::Node            *node,
                          TR::LabelSymbol      *restartlab,
                          TR::LabelSymbol      *snippetlab,
                          TR::SymbolReference *helperSymRef)
      : TR::X86RestartSnippet(codeGen, node, restartlab, snippetlab, helperSymRef->canCauseGC()),
           _helperSymRef(helperSymRef)
      {
      // The code generation for this snippet does not allow a proper GC map
      // to be built, so assert that GC will not happen.
      //
      TR_ASSERT(!helperSymRef->canCauseGC(), "assertion failure");
      }

   TR::SymbolReference *getHelperSymRef()                      {return _helperSymRef;}
   TR::SymbolReference *setHelperSymRef(TR::SymbolReference *s) {return (_helperSymRef = s);}

   uint8_t *emitCallToConversionHelper(uint8_t *buffer);
   virtual uint8_t *emitSnippetBody();
   virtual uint8_t *genFPConversion(uint8_t *buffer) = 0;
   virtual Kind getKind() {return IsFPConversion;}
   };


class X86FPConvertToIntSnippet  : public TR::X86FPConversionSnippet
   {
   TR::X86RegInstruction  *_convertInstruction;

   public:

   X86FPConvertToIntSnippet(TR::LabelSymbol            *restartlab,
                            TR::LabelSymbol            *snippetlab,
                            TR::SymbolReference       *helperSymRef,
                            TR::X86RegInstruction     *convertInstr,
                            TR::CodeGenerator *codeGen)
      : TR::X86FPConversionSnippet(codeGen, convertInstr->getNode(), restartlab, snippetlab, helperSymRef),
           _convertInstruction(convertInstr) {}

   TR::X86RegInstruction  * getConvertInstruction() {return _convertInstruction;}
   uint8_t *genFPConversion(uint8_t *buffer);
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual Kind getKind() {return IsFPConvertToInt;}
   };


class X86FPConvertToLongSnippet  : public TR::X86FPConversionSnippet
   {
   TR::X86RegMemInstruction          *_loadHighInstruction,
                                    *_loadLowInstruction;
   TR::X86FPST0STiRegRegInstruction  *_clobberInstruction;
   TR::RealRegister               *_lowRegister,
                                    *_highRegister,
                                    *_doubleRegister;
   uint8_t                           _action;

   void analyseLongConversion();

   public:

   static const uint8_t              _registerActions[16];

   enum actionFlags
      {
      kXCHG        = 0x01,
      kMOVHigh     = 0x02,
      kMOVLow      = 0x04,
      kPreserveEDX = 0x08,
      kPreserveEAX = 0x10,
      kNeedFXCH    = 0x80
      };

   X86FPConvertToLongSnippet(TR::LabelSymbol                    *restartlab,
                             TR::LabelSymbol                    *snippetlab,
                             TR::SymbolReference               *helperSymRef,
                             TR::X86FPST0STiRegRegInstruction  *clobInstr,
                             TR::X86RegMemInstruction          *loadHighInstr,
                             TR::X86RegMemInstruction          *loadLowInstr,
                             TR::CodeGenerator *codeGen)
      : TR::X86FPConversionSnippet(codeGen, clobInstr->getNode(), restartlab, snippetlab, helperSymRef),
           _loadHighInstruction(loadHighInstr),
           _loadLowInstruction(loadLowInstr),
           _clobberInstruction(clobInstr),
           _lowRegister(0),
           _highRegister(0),
           _doubleRegister(0),
           _action(0) {}

   uint8_t getAction() { return _action; }

   TR::RealRegister * getLowRegister()   { return _lowRegister; }
   TR::RealRegister * getHighRegister()   { return _highRegister; }
   TR::RealRegister * getDoubleRegister() { return _doubleRegister; }

   uint8_t *genFPConversion(uint8_t *buffer);
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual Kind getKind() {return IsFPConvertToLong;}
   };

}

#endif
