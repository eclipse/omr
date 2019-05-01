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

#ifndef DIVIDECHECKSNIPPET_INCL
#define DIVIDECHECKSNIPPET_INCL

#include <stdint.h>
#include "codegen/Snippet.hpp"
#include "il/DataTypes.hpp"
#include "x/codegen/RestartSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class ILOpCode; }
namespace TR { class LabelSymbol; }

namespace TR {

class X86DivideCheckSnippet  : public TR::X86RestartSnippet
   {
   public:

   X86DivideCheckSnippet(TR::LabelSymbol           *restartLabel,
                            TR::LabelSymbol           *snippetLabel,
                            TR::LabelSymbol           *divideLabel,
                            TR::ILOpCode               &divOp,
                            TR::DataType               type,
                            TR::X86RegRegInstruction  *divideInstruction,
                            TR::CodeGenerator         *cg)
      : TR::X86RestartSnippet(cg, divideInstruction->getNode(), restartLabel, snippetLabel, true),
        _divOp(divOp),
        _type(type),
        _divideLabel(divideLabel),
        _divideInstruction(divideInstruction)
      {}

   virtual Kind getKind() { return IsDivideCheck; }

   TR::ILOpCode &getOpCode() {return _divOp;}
   TR::DataType getType() {return _type;}

   TR::LabelSymbol *getDivideLabel()                  {return _divideLabel;}
   TR::LabelSymbol *setDivideLabel(TR::LabelSymbol *l) {return (_divideLabel = l);}

   TR::X86RegRegInstruction  *getDivideInstruction() {return _divideInstruction;}

   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   private:

   TR::LabelSymbol          *_divideLabel;
   TR::X86RegRegInstruction *_divideInstruction;
   TR::ILOpCode              &_divOp;
   TR::DataType              _type;
   };

}

#endif
