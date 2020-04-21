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

#pragma csect(CODE,"OMRZSnippet#C")
#pragma csect(STATIC,"OMRZSnippet#S")
#pragma csect(TEST,"OMRZSnippet#T")


#include <stddef.h>
#include <stdint.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
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
#include "z/codegen/S390HelperCallSnippet.hpp"

namespace TR { class Node; }

OMR::Z::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label,
      bool isGCSafePoint) :
   OMR::Snippet(cg, node, label, isGCSafePoint),
      _codeBaseOffset(-1), _pad_bytes(0), _snippetDestAddr(0), _zflags(0)
   {
   self()->setNeedLitPoolBasePtr();
   }



OMR::Z::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label) :
   OMR::Snippet(cg, node, label),
      _codeBaseOffset(-1), _pad_bytes(0), _snippetDestAddr(0), _zflags(0)
   {
   self()->setNeedLitPoolBasePtr();
   }


/**
 * Generate binary for PicBuilder Call
 */
uint8_t *
OMR::Z::Snippet::generatePICBinary(TR::CodeGenerator * cg, uint8_t * cursor, TR::SymbolReference* glueRef)
   {
   // Branch to the dispatcher.
   // Since N3 instructions are supported, we can use relative long instructions.
   // i.e.:
   //              BRASL r14, <target Addr>.
   //  - or -
   //              LARL  r14, <target Addr>.   // Unresolved Calls only.
   //              L/LG  rEP, 0(r14).
   //              BCR   rEP

   uint32_t rEP = (uint32_t) cg->getEntryPointRegister() - 1;

   if (self()->getKind() == TR::Snippet::IsUnresolvedCall)
      {
      // Generate LARL r14, <Start of Data Const>
      *(int16_t *) cursor = 0xC0E0;
      cursor += sizeof(int16_t);
      intptr_t destAddr = (intptr_t)(cursor + self()->getPICBinaryLength(cg) + self()->getPadBytes() - 2);
      *(int32_t *) cursor = (int32_t)((destAddr - (intptr_t)(cursor - 2)) / 2);
      cursor += sizeof(int32_t);

      // L/LG  rEP, 0(r14)
      if (cg->comp()->target().is64Bit())
         {
         *(int32_t *) cursor = 0xe300e000 + (rEP << 20);           // LG  rEP, 0(r14)
         cursor += sizeof(int32_t);
         *(int16_t *) cursor = 0x0004;
         cursor += sizeof(int16_t);
         }
      else
         {
         *(int32_t *) cursor = 0x5800e000 + (rEP << 20);           // L  rEP, 0(r14)
         cursor += sizeof(int32_t);
         }

      // BCR   rEP
      *(int16_t *) cursor = 0x07F0 + rEP;
      cursor += sizeof(int16_t);
      }
   else
      {
      // Generate BRASL instruction.
      intptr_t instructionStartAddress = (intptr_t)cursor;
      *(int16_t *) cursor = 0xC0E5;
      cursor += sizeof(int16_t);

      // Calculate the relative offset to get to helper method.
      // If MCC is not supported, everything should be reachable.
      // If MCC is supported, we will look up the appropriate trampoline, if
      //     necessary.
      intptr_t destAddr = (intptr_t)(glueRef->getSymbol()->castToMethodSymbol()->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (cg->comp()->getOption(TR_EnableRMODE64))
#endif
         {
         if (cg->directCallRequiresTrampoline(destAddr, instructionStartAddress))
            {
            // Destination is beyond our reachable jump distance, we'll find the
            // trampoline.
            destAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
            self()->setUsedTrampoline(true);
            }
         }
#endif

      TR_ASSERT_FATAL(cg->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(destAddr, instructionStartAddress),
                      "Helper Call is not reachable.");
      self()->setSnippetDestAddr(destAddr);

      *(int32_t *) cursor = (int32_t)((destAddr - instructionStartAddress) / 2);
      AOTcgDiag1(cg->comp(), "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
      cg->addProjectSpecializedRelocation(cursor, (uint8_t*) glueRef, NULL, TR_HelperAddress,
                                      __FILE__, __LINE__, self()->getNode());
      cursor += sizeof(int32_t);
      }
   return cursor;
   }

/**
 * Get PicBuilder Call Binary Length
 */
uint32_t
OMR::Z::Snippet::getPICBinaryLength(TR::CodeGenerator * cg)
   {
   int32_t lengthOfLoad = (cg->comp()->target().is64Bit())?6:4;

   if (self()->getKind() == TR::Snippet::IsUnresolvedCall)
      return 6 + lengthOfLoad + 2; // LARL + L/LG + BCR
   else
      return 6; // BRASL
   }

/**
 * Load the vm thread value into GPR13
 * @todo okay to assume that we can destroy previous contents of r13? is register volatile?
 */
uint8_t *
OMR::Z::Snippet::generateLoadVMThreadInstruction(TR::CodeGenerator *cg, uint8_t *cursor)
   {
   return cursor;
   }

uint32_t
OMR::Z::Snippet::getLoadVMThreadInstructionLength(TR::CodeGenerator *cg)
   {
   return 0;
   }

/**
 * Helper method to insert Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param cursor           Current binary encoding cursor
 * @param op               Runtime Instrumentation opcode: TR::InstOpCode::RION or TR::InstOpCode::RIOFF
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 Updated binary encoding cursor after RI hook generation.
 */
uint8_t *
OMR::Z::Snippet::generateRuntimeInstrumentationOnOffInstruction(TR::CodeGenerator *cg, uint8_t *cursor, TR::InstOpCode::Mnemonic op, bool isPrivateLinkage)
   {
   return cursor;
   }

/**
 * Helper method to query the length of Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 The length of RION or RIOFF encoding if generated.  Zero otherwise.
 */
uint32_t
OMR::Z::Snippet::getRuntimeInstrumentationOnOffInstructionLength(TR::CodeGenerator *cg, bool isPrivateLinkage)
   {
   return 0;
   }


#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/ConstantDataSnippet.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"

void
TR_Debug::printz(TR::FILE *pOutFile, TR::Snippet * snippet)
   {
   if (pOutFile == NULL)
      {
      return;
      }
   switch (snippet->getKind())
      {
      case TR::Snippet::IsCall:
         print(pOutFile, (TR::S390CallSnippet *) snippet);
         break;
      case TR::Snippet::IsHelperCall:
         print(pOutFile, (TR::S390HelperCallSnippet *) snippet);
         break;
#if J9_PROJECT_SPECIFIC
      case TR::Snippet::IsForceRecomp:
         print(pOutFile, (TR::S390ForceRecompilationSnippet *) snippet);
         break;
      case TR::Snippet::IsForceRecompData:
         print(pOutFile, (TR::S390ForceRecompilationDataSnippet *) snippet);
         break;
      case TR::Snippet::IsUnresolvedCall:
         print(pOutFile, (TR::S390UnresolvedCallSnippet *) snippet);
         break;
      case TR::Snippet::IsVirtual:
         print(pOutFile, (TR::S390VirtualSnippet *) snippet);
         break;
      case TR::Snippet::IsVirtualUnresolved:
         print(pOutFile, (TR::S390VirtualUnresolvedSnippet *) snippet);
         break;
      case TR::Snippet::IsInterfaceCall:
         print(pOutFile, (TR::S390InterfaceCallSnippet *) snippet);
         break;
      case TR::Snippet::IsStackCheckFailure:
         print(pOutFile, (TR::S390StackCheckFailureSnippet *) snippet);
         break;
      case TR::Snippet::IsInterfaceCallData:
         print(pOutFile, (TR::J9S390InterfaceCallDataSnippet *) snippet);
         break;
#endif
      case TR::Snippet::IsConstantData:
      case TR::Snippet::IsWritableData:
      case TR::Snippet::IsEyeCatcherData:
         print(pOutFile, (TR::S390ConstantDataSnippet *) snippet);
         break;
      case TR::Snippet::IsUnresolvedData:
         print(pOutFile, (TR::UnresolvedDataSnippet *) snippet);
         break;

      case TR::Snippet::IsConstantInstruction:
         print(pOutFile, (TR::S390ConstantInstructionSnippet *) snippet);
         break;
      case TR::Snippet::IsRestoreGPR7:
         print(pOutFile, (TR::S390RestoreGPR7Snippet *) snippet);
         break;

      default:
         snippet->print(pOutFile, this);
      }
   }
