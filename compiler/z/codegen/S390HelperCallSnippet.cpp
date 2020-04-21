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

#include "z/codegen/S390HelperCallSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/GCStackMap.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/CallSnippet.hpp"

namespace TR { class Node; }

uint8_t *
TR::S390HelperCallSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   TR::Node * callNode = getNode();
   TR::SymbolReference * helperSymRef = getHelperSymRef();
   bool jitInduceOSR = helperSymRef->isOSRInductionHelper();
   if (jitInduceOSR)
      {
      // Flush in-register arguments back to the stack for interpreter
      cursor = TR::S390CallSnippet::S390flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());
      }


   uint32_t rEP = (uint32_t) cg()->getEntryPointRegister() - 1;

   //load vm thread into gpr13
   cursor = generateLoadVMThreadInstruction(cg(), cursor);

   // Generate RIOFF if RI is supported.
   cursor = generateRuntimeInstrumentationOnOffInstruction(cg(), cursor, TR::InstOpCode::RIOFF);

   intptr_t branchInstructionStartAddress;

   if (                                                                               // Methods that require
             alwaysExcept())                                                          // R14 to point to snippet:
      {
      // For trace method entry/exit, we need to set up R14 to point to the
      // beginning of the data segment.  We will use BRASL to automatically
      // set R14 correctly.

      // For methods that lead to exceptions, and never return to the
      // main code, we set up R14, so that if GC occurs, the stackwalker
      // will see R14 is pointing to this snippet, and pick up the correct
      // stack map.

      branchInstructionStartAddress = (intptr_t)cursor;
      *(int16_t *) cursor = 0xC0E5;                                                   // BRASL  R14, <Helper Addr>
      cursor += sizeof(int16_t);
      }
   else                                                                               // Otherwise:
      {
      // We're not sure if the helper will return.  So, we need to provide
      // the return addr of the main line code, so that when helper call
      // completes, it can jump back properly.

      // Load Return Address into R14.
      intptr_t returnAddr = (intptr_t)getReStartLabel()->getCodeLocation();         // LARL   R14, <Return Addr>
      *(int16_t *) cursor = 0xC0E0;
      cursor += sizeof(int16_t);
      *(int32_t *) cursor = (int32_t)((returnAddr - (intptr_t)(cursor - 2)) / 2);
      cursor += sizeof(int32_t);

      branchInstructionStartAddress = (intptr_t)cursor;
      *(int16_t *) cursor = 0xC0F4;                                                   // BRCL   <Helper Addr>
      cursor += sizeof(int16_t);
      }

   // Calculate the relative offset to get to helper method.
   // If MCC is not supported, everything should be reachable.
   // If MCC is supported, we will look up the appropriate trampoline, if
   //     necessary.
   intptr_t destAddr = (intptr_t)(helperSymRef->getSymbol()->castToMethodSymbol()->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
   if (cg()->comp()->getOption(TR_EnableRMODE64))
#endif
      {
      if (cg()->directCallRequiresTrampoline(destAddr, branchInstructionStartAddress))
         {
         destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(helperSymRef->getReferenceNumber(), (void *)cursor);
         this->setUsedTrampoline(true);

         // We clobber rEP if we take a trampoline. Update our register map if necessary.
         if (gcMap().getStackMap() != NULL)
            {
            gcMap().getStackMap()->maskRegisters(~(0x1 << (rEP)));
            }
         }
      }
#endif

   TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(destAddr, branchInstructionStartAddress),
                   "Helper Call is not reachable.");
   this->setSnippetDestAddr(destAddr);

   *(int32_t *) cursor = (int32_t)((destAddr - branchInstructionStartAddress) / 2);
   AOTcgDiag1(cg()->comp(), "add TR_HelperAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, (uint8_t*) helperSymRef, NULL, TR_HelperAddress,
                             __FILE__, __LINE__, getNode());
   cursor += sizeof(int32_t);

   gcMap().registerStackMap(cursor, cg());

   return cursor;
   }

uint32_t
TR::S390HelperCallSnippet::getLength(int32_t)
   {
   uint32_t length;
   TR::SymbolReference * helperSymRef = getHelperSymRef();

   // LARL + BRASL/BRCL
   length = 12;

   if (helperSymRef->isOSRInductionHelper())
      {
      length += TR::S390CallSnippet::instructionCountForArguments(getNode(), cg());
      }

   length += getLoadVMThreadInstructionLength(cg());

   length += getRuntimeInstrumentationOnOffInstructionLength(cg());

   return length;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390HelperCallSnippet * snippet)
   {
   if (pOutFile == NULL)
      {
      return;
      }

   TR::SymbolReference * helperSymRef = snippet->getHelperSymRef();

   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Helper Call Snippet", getName(helperSymRef));

   bufferPos = printLoadVMThreadInstruction(pOutFile, bufferPos);

   bufferPos = printRuntimeInstrumentationOnOffInstruction(pOutFile, bufferPos, false); // RIOFF

   if (
             snippet->alwaysExcept())
      {
      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "BRASL \tGPR14, <%p>\t# Branch to Helper Method %s",
                       snippet->getSnippetDestAddr(),
                       snippet->usedTrampoline()?"- Trampoline Used.":"");
      bufferPos += 6;
      }
   else
      {
      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "LARL \tGPR14, <%p>\t# Return Addr of Main Line.",
                                   (intptr_t) snippet->getReStartLabel()->getCodeLocation());
      bufferPos += 6;
      printPrefix(pOutFile, NULL, bufferPos, 6);
      trfprintf(pOutFile, "BRCL \t<%p>\t# Branch to Helper Method %s",
                       snippet->getSnippetDestAddr(),
                       snippet->usedTrampoline()?"- Trampoline Used.":"");
      bufferPos += 6;
      }
   }
