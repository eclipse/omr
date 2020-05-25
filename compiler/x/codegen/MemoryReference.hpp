/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef TR_MEMORYREFERENCE_INCL
#define TR_MEMORYREFERENCE_INCL

#include "codegen/OMRMemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class X86DataSnippet; }
class TR_ScratchRegisterManager;
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class SymbolReference; }

namespace TR
{

class OMR_EXTENSIBLE MemoryReference : public OMR::MemoryReferenceConnector
   {
   public:

   MemoryReference(TR::CodeGenerator *cg) :
      OMR::MemoryReferenceConnector(cg) {}

   MemoryReference(TR::Register *br,
      TR::SymbolReference *sr,
      TR::Register *ir,
      uint8_t s,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(br, sr, ir, s, cg) {}

   MemoryReference(TR::Register *br,
      TR::Register *ir,
      uint8_t s,
      TR::CodeGenerator*cg) :
         OMR::MemoryReferenceConnector(br, ir, s, cg) {}

   MemoryReference(TR::Register *br,
      intptr_t disp,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(br, disp, cg) {}

   MemoryReference(intptr_t disp,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(disp, cg) {}

   MemoryReference(TR::Register *br,
      TR::Register *ir,
      uint8_t s,
      intptr_t disp,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(br, ir, s, disp, cg) {}

   MemoryReference(TR::X86DataSnippet *cds,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(cds, cg) {}

   MemoryReference(TR::LabelSymbol *label,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(label, cg) {}

   MemoryReference(TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg,
      bool canRematerializeAddressAdds) :
         OMR::MemoryReferenceConnector(rootLoadOrStore, cg, canRematerializeAddressAdds) {}

   MemoryReference(TR::SymbolReference *symRef,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(symRef, cg) {}

   MemoryReference(TR::SymbolReference *symRef,
      intptr_t displacement,
      TR::CodeGenerator *cg) :
         OMR::MemoryReferenceConnector(symRef, displacement, cg) {}

   MemoryReference(MemoryReference& mr,
      intptr_t n,
      TR::CodeGenerator *cg,
      TR_ScratchRegisterManager *srm = NULL) :
         OMR::MemoryReferenceConnector(mr, n, cg) {}

   };
}

#endif
