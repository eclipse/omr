/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#pragma csect(CODE,"OMRZSnippet#C")
#pragma csect(STATIC,"OMRZSnippet#S")
#pragma csect(TEST,"OMRZSnippet#T")


#include <stddef.h>                             // for NULL
#include <stdint.h>                             // for int32_t, uint32_t, etc
#include "codegen/BackingStore.hpp"             // for TR_BackingStore
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator, etc
#include "codegen/ConstantDataSnippet.hpp"
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"               // for InstOpCode, etc
#include "codegen/RealRegister.hpp"             // for RealRegister
#include "codegen/Register.hpp"                 // for Register
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"                  // for TR::S390Snippet, etc
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"              // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                       // for intptrj_t
#include "il/Symbol.hpp"                        // for Symbol
#include "il/SymbolReference.hpp"               // for SymbolReference
#include "il/symbol/LabelSymbol.hpp"            // for LabelSymbol
#include "il/symbol/MethodSymbol.hpp"           // for MethodSymbol
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "ras/Debug.hpp"                        // for TR_Debug
#include "runtime/Runtime.hpp"                  // for ::TR_HelperAddress, etc
#include "z/codegen/CallSnippet.hpp"            // for TR::S390CallSnippet
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
      intptrj_t destAddr = (intptrj_t)(cursor + self()->getPICBinaryLength(cg) + self()->getPadBytes() - 2);
      *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);
      cursor += sizeof(int32_t);

      // L/LG  rEP, 0(r14)
      if (TR::Compiler->target.is64Bit())
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
      *(int16_t *) cursor = 0xC0E5;
      cursor += sizeof(int16_t);

      // Calculate the relative offset to get to helper method.
      // If MCC is not supported, everything should be reachable.
      // If MCC is supported, we will look up the appropriate trampoline, if
      //     necessary.
      intptrj_t destAddr = (intptrj_t)(glueRef->getSymbol()->castToMethodSymbol()->getMethodAddress());

#if defined(TR_TARGET_64BIT)
#if defined(J9ZOS390)
      if (cg->comp()->getOption(TR_EnableRMODE64))
#endif
         {
         if (NEEDS_TRAMPOLINE(destAddr, cursor, cg))
            {
            // Destination is beyond our reachable jump distance, we'll find the
            // trampoline.
            destAddr = cg->fe()->indexedTrampolineLookup(glueRef->getReferenceNumber(), (void *)cursor);
            self()->setUsedTrampoline(true);
            }
         }
#endif

      TR_ASSERT(CHECK_32BIT_TRAMPOLINE_RANGE(destAddr, cursor),  "Helper Call is not reachable.");
      self()->setSnippetDestAddr(destAddr);

      *(int32_t *) cursor = (int32_t)((destAddr - (intptrj_t)(cursor - 2)) / 2);
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
   int32_t lengthOfLoad = (TR::Compiler->target.is64Bit())?6:4;

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
   if (cg->comp()->getOption(TR_Enable390FreeVMThreadReg))
      {
      TR_BackingStore *vmThreadBackingStore = cg->getVMThreadRegister()->getBackingStorage();
      if (vmThreadBackingStore)
         {
         intptrj_t symbolOffset = vmThreadBackingStore->getSymbolReference()->getSymbol()->getOffset();
         TR_ASSERT( symbolOffset <= 0xFFF, "displacement too large\n");
         uint32_t rSP = cg->getStackPointerRealRegister()->getRegisterNumber() - 1;
         if (TR::Compiler->target.is64Bit())
            {
            // LG
            *(uint32_t *)cursor = 0xE3D00000 + (rSP << 12) + symbolOffset;
            cursor += sizeof(uint32_t);
            *(uint16_t *)cursor = 0x0004;
            cursor += sizeof(uint16_t);
            }
         else
            {
            *(uint32_t *)cursor = 0x58D00000 + (rSP << 12) + symbolOffset; //L
            cursor += sizeof(uint32_t);
            }
         }
      }
   return cursor;
   }

uint32_t
OMR::Z::Snippet::getLoadVMThreadInstructionLength(TR::CodeGenerator *cg)
   {
   if (!cg->comp()->getOption(TR_Enable390FreeVMThreadReg))
      {
      return 0;
      }
   if (TR::Compiler->target.is64Bit())
      {
      return sizeof(uint32_t) + sizeof(uint16_t);
      }
   return sizeof(uint32_t);
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
#endif
      case TR::Snippet::IsLabelTable:
         print(pOutFile, (TR::S390LabelTableSnippet *) snippet);
         break;
      case TR::Snippet::IsConstantData:
      case TR::Snippet::IsWritableData:
      case TR::Snippet::IsEyeCatcherData:
         print(pOutFile, (TR::S390ConstantDataSnippet *) snippet);
         break;
      case TR::Snippet::IsTargetAddress:
         print(pOutFile, (TR::S390TargetAddressSnippet *) snippet);
         break;
      case TR::Snippet::IsLookupSwitch:
         print(pOutFile, (TR::S390LookupSwitchSnippet *) snippet);
         break;
      case TR::Snippet::IsUnresolvedData:
         print(pOutFile, (TR::UnresolvedDataSnippet *) snippet);
         break;
      case TR::Snippet::IsInterfaceCallData:
         print(pOutFile, (TR::S390InterfaceCallDataSnippet *) snippet);
         break;
      case TR::Snippet::IsConstantInstruction:
         print(pOutFile, (TR::S390ConstantInstructionSnippet *) snippet);
         break;
      case TR::Snippet::IsRestoreGPR7:
         print(pOutFile, (TR::S390RestoreGPR7Snippet *) snippet);
         break;

      /* These types are frontend specific - we use virtual dispatch
         this will be extended to all the other types in the future */
      case TR::Snippet::IsHeapAlloc:
      case TR::Snippet::IsJNICallData:
      case TR::Snippet::IsMonitorEnter:
      case TR::Snippet::IsMonitorExit:
         snippet->print(pOutFile, this);
         break;

      default:
         TR_ASSERT( 0,"unexpected snippet kind");
      }
   }
