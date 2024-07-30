/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "z/codegen/ConstantDataSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"

#if defined(TR_HOST_S390)
#include <time.h>
#endif

TR::S390ConstantDataSnippet::S390ConstantDataSnippet(TR::CodeGenerator * cg, TR::Node * n, void * c, uint16_t size) :
   TR::Snippet(cg, n, generateLabelSymbol(cg), false)
   {

   if (c)
      memcpy(_value, c, size);
   _unresolvedDataSnippet = NULL;
   _length = size;
   _symbolReference = NULL;
   _reloType = 0;
   }

void
TR::S390ConstantDataSnippet::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();

   uint32_t reloType = getReloType();
   TR::SymbolType symbolKind = TR::SymbolType::typeClass;
   switch (reloType)
      {
      case 0:
         break;

      case TR_ClassAddress:
      case TR_ClassObject:
         {
         TR::SymbolReference *reloSymRef= (reloType==TR_ClassAddress)?getNode()->getSymbolReference():getSymbolReference();
         if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
            {
            TR_ASSERT_FATAL(getDataAs8Bytes(), "Static Sym can not be NULL");

            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) getDataAs8Bytes(),
                  (uint8_t *) TR::SymbolType::typeClass,
                  TR_SymbolFromManager,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         else
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) reloSymRef,
                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         }
         break;
      case TR_MethodObject:
         {
         TR::SymbolReference *reloSymRef= (reloType==TR_ClassAddress)?getNode()->getSymbolReference():getSymbolReference();
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) reloSymRef,
               getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
               (TR_ExternalRelocationTargetKind) reloType,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_JNIVirtualTargetAddress:
         {
         TR_RelocationRecordInformation *info = new (comp->trHeapMemory()) TR_RelocationRecordInformation();
         info->data1 = 0;
         info->data2 = reinterpret_cast<uintptr_t>(getNode()->getSymbolReference());
         int16_t inlinedSiteIndex = getNode() ? getNode()->getInlinedSiteIndex() : -1;
         info->data3 = static_cast<uintptr_t>(inlinedSiteIndex);

         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               reinterpret_cast<uint8_t *>(info),
               static_cast<TR_ExternalRelocationTargetKind>(reloType),
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_DataAddress:
         {
         if (cg()->needRelocationsForStatics())
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) getNode()->getSymbolReference(),
                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                  (TR_ExternalRelocationTargetKind) reloType,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         }
         break;

      case TR_ArrayCopyHelper:
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) getSymbolReference(),
               (TR_ExternalRelocationTargetKind) reloType,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         break;

      case TR_HelperAddress:
         if (cg()->comp()->target().is64Bit())
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) *((uint64_t*) cursor),
                  TR_AbsoluteHelperAddress,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         else
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)(intptr_t) *((uint32_t*) cursor),
                  TR_AbsoluteHelperAddress,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         break;

      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
         if (cg()->comp()->target().is64Bit())
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) *((uint64_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         else
            {
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *)(intptr_t) *((uint32_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         break;

      case TR_CatchBlockCounter:
         {
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               NULL,
               TR_CatchBlockCounter,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_GlobalValue:
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) TR_CountForRecompile,
               TR_GlobalValue,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         break;

      case TR_RamMethod:
      case TR_MethodPointer:
         symbolKind = TR::SymbolType::typeMethod;
         // intentional fall through
      case TR_ClassPointer:
         if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
            {
            TR_ASSERT_FATAL(getDataAs8Bytes(), "Static Sym can not be NULL");
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) getDataAs8Bytes(),
                  (uint8_t *)symbolKind,
                  TR_SymbolFromManager,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         else
            {
            //for optimizations where we are trying to relocate either profiled j9class or getfrom signature we can't use node to get the target address
            //so we need to pass it to relocation in targetaddress2 for now
            //two instances where use this relotype in such way are: profile checkcast and arraystore check object check optimiztaions
            uint8_t * targetAddress2 = NULL;
            if (getNode()->getOpCodeValue() != TR::aconst)
               {
               if (cg()->comp()->target().is64Bit())
                  targetAddress2 = (uint8_t *) *((uint64_t*) cursor);
               else
                  targetAddress2 = (uint8_t *) *((uintptr_t*) cursor);
               }
            cg()->addExternalRelocation(
               TR::ExternalRelocation::create(
                  cursor,
                  (uint8_t *) getNode(),
                  targetAddress2,
                  (TR_ExternalRelocationTargetKind) reloType,
                  cg()),
               __FILE__,
               __LINE__,
               getNode());
            }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = cg()->comp()->getCounterFromStaticAddress(getNode()->getSymbolReference());
         if (counter == NULL)
            {
            cg()->comp()->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::X86::MemoryReference::addMetaDataForCodeAddress\n");
            }

         TR::DebugCounter::generateRelocation(cg()->comp(),
                                              cursor,
                                              getNode(),
                                              counter);
         }
         break;

      case TR_BlockFrequency:
         {
         TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)getNode()->getSymbolReference();
         recordInfo->data2 = 0; // seqKind
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *)recordInfo,
               TR_BlockFrequency,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_RecompQueuedFlag:
         {
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               NULL,
               TR_RecompQueuedFlag,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_MethodEnterExitHookAddress:
         {
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) getNode()->getSymbolReference(),
               NULL,
               TR_MethodEnterExitHookAddress,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      case TR_CallsiteTableEntryAddress:
         {
         cg()->addExternalRelocation(
            TR::ExternalRelocation::create(
               cursor,
               (uint8_t *) getNode()->getSymbolReference(),
               NULL,
               TR_CallsiteTableEntryAddress,
               cg()),
            __FILE__,
            __LINE__,
            getNode());
         }
         break;

      default:
         TR_ASSERT( 0,"relocation type not handled yet");
      }

   if (comp->getOption(TR_EnableHCR))
      {
      if (std::find(cg()->getSnippetsToBePatchedOnClassRedefinition()->begin(), cg()->getSnippetsToBePatchedOnClassRedefinition()->end(), this) != cg()->getSnippetsToBePatchedOnClassRedefinition()->end())
         {
         cg()->jitAddPicToPatchOnClassRedefinition(((void *) (*(uintptr_t *) cursor)), (void *) (uintptr_t *) cursor);
         }

      if (std::find(cg()->getSnippetsToBePatchedOnClassUnload()->begin(), cg()->getSnippetsToBePatchedOnClassUnload()->end(), this) != cg()->getSnippetsToBePatchedOnClassUnload()->end())
         cg()->jitAddPicToPatchOnClassUnload(((void *) (*(uintptr_t *) cursor)), (void *) (uintptr_t *) cursor);

      if (std::find(cg()->getMethodSnippetsToBePatchedOnClassUnload()->begin(), cg()->getMethodSnippetsToBePatchedOnClassUnload()->end(), this) != cg()->getMethodSnippetsToBePatchedOnClassUnload()->end())
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (*(uintptr_t *) cursor), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAddPicToPatchOnClassUnload(classPointer, (void *) (uintptr_t *) cursor);
         }
      }
   else
      {
      if (std::find(cg()->getSnippetsToBePatchedOnClassUnload()->begin(), cg()->getSnippetsToBePatchedOnClassUnload()->end(), this) != cg()->getSnippetsToBePatchedOnClassUnload()->end())
         {
         cg()->jitAddPicToPatchOnClassUnload(((void *) (*(uintptr_t *) cursor)), (void *) (uintptr_t *) cursor);
         }

      if (std::find(cg()->getMethodSnippetsToBePatchedOnClassUnload()->begin(), cg()->getMethodSnippetsToBePatchedOnClassUnload()->end(), this) != cg()->getMethodSnippetsToBePatchedOnClassUnload()->end())
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (*(uintptr_t *) cursor), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAddPicToPatchOnClassUnload(classPointer, (void *) (uintptr_t *) cursor);
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   // For Unresolved Data Calls, we need to insert the address of this literal
   // pool reference, so that the PICbuilder code can patch the resolved address
   if (getUnresolvedDataSnippet() != NULL)
      {
      uint8_t * udsPatchLocation = getUnresolvedDataSnippet()->getLiteralPoolPatchAddress();
      TR_ASSERT(udsPatchLocation != NULL,"Literal Pool Reference has NULL Unresolved Data Snippet patch site!");
      *(intptr_t *)udsPatchLocation = (intptr_t)cursor;
      getUnresolvedDataSnippet()->setLiteralPoolSlot(cursor);
      }
#endif

   }


uint8_t *
TR::S390ConstantDataSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();

   getSnippetLabel()->setCodeLocation(cursor);
   memcpy(cursor, &_value, _length);

   uint32_t reloType = getReloType();

   switch (reloType)
      {
      case TR_ClassAddress:
      case TR_ClassObject:
         {
         uintptr_t romClassPtr = TR::Compiler->cls.persistentClassPointerFromClassPointer(comp, (TR_OpaqueClassBlock*)(*((uintptr_t*)cursor)));
         memcpy(cursor, &romClassPtr, _length);
         }
         break;

      case TR_AbsoluteMethodAddress:
         TR_ASSERT(getNode()->getSymbol() && getNode()->getSymbol()->getStaticSymbol() &&
                getNode()->getSymbol()->getStaticSymbol()->isStartPC(), "Expecting start PC");
         *(uintptr_t *) cursor = (uintptr_t) getNode()->getSymbol()->getStaticSymbol()->getStaticAddress();
         break;
      }

   addMetaDataForCodeAddress(cursor);

   cursor += _length;

   return cursor;
   }

uint32_t
TR::S390ConstantDataSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   return _length;
   }

TR::S390ConstantInstructionSnippet::S390ConstantInstructionSnippet(TR::CodeGenerator * cg, TR::Node * n, TR::Instruction *instr)
   : TR::S390ConstantDataSnippet(cg, n, NULL, 0)
   {
   _instruction = instr;
   setLength(instr->getOpCode().getInstructionLength());
   }

uint8_t *
TR::S390ConstantInstructionSnippet::emitSnippetBody()
   {
   TR::Instruction * instr = this->getInstruction();
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);
   instr->generateBinaryEncoding();
   for(int i = 0; i < _length; ++i)
      _value[i] = cursor[i];

   cursor += 8;
   return cursor;
   }

int64_t
TR::S390ConstantInstructionSnippet::getDataAs8Bytes()
   {
   emitSnippetBody();

   return *((uint64_t *)_value);
   }

TR::S390EyeCatcherDataSnippet::S390EyeCatcherDataSnippet(TR::CodeGenerator *cg, TR::Node *n)
   : TR::S390ConstantDataSnippet(cg, n, NULL, 0)
   {
   // Cold Eyecatcher is used for padding of endPC so that Return Address for exception snippets will never equal the endPC.
   /*
   char eyeCatcher[4]={ 'J','I','T','M'};
   int32_t eyeCatcherSize = 4;
   _value = (uint8_t *)malloc(eyeCatcherSize);
   void *target = (void*)_value;
   memcpy(target , eyeCatcher, eyeCatcherSize);
   */
   setLength(4);
   }

uint8_t *
TR::S390EyeCatcherDataSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   const int32_t eyeCatcherSize = 4;
   char eyeCatcher[eyeCatcherSize]={ 'J','I','T','M'};
   memcpy(cursor, eyeCatcher, eyeCatcherSize);
   cursor += eyeCatcherSize;
   return cursor;
   }

TR::S390WritableDataSnippet::S390WritableDataSnippet(TR::CodeGenerator * cg, TR::Node * n, void * c, uint16_t size)
   : TR::S390ConstantDataSnippet(cg, n, c, size)
   {
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ConstantDataSnippet * snippet)
   {
   // *this   swipeable for debugger
   if (pOutFile == NULL)
      {
      return;
      }

   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   if (snippet->getKind() == TR::Snippet::IsWritableData)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Writable Data Snippet");
      }
   else if (snippet->getKind() == TR::Snippet::IsEyeCatcherData)
      {
      // Cold Eyecatcher is used for padding of endPC so that Return Address for exception snippets will never equal the endPC.
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "EyeCatcher Data Snippet");
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "Eye Catcher = JITM\n");
      return;
      }
   else if (snippet->getKind() == TR::Snippet::IsConstantInstruction)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Constant Instruction Snippet");
      print(pOutFile, ((TR::S390ConstantInstructionSnippet *)snippet)->getInstruction());
      return;
      }
   else if (snippet->getKind() == TR::Snippet::IsInterfaceCallData)
      {
#ifdef J9_PROJECT_SPECIFIC
      print(pOutFile, reinterpret_cast<TR::J9S390InterfaceCallDataSnippet*>(snippet));
#endif
      return;
      }
   else
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Constant Data Snippet");
      }

   printPrefix(pOutFile, NULL, bufferPos, snippet->getConstantSize());


   if (snippet->getConstantSize() == 8)
      {
      trfprintf(pOutFile, "DC   \t0x%016lx ", snippet->getDataAs8Bytes());
      }
   else if (snippet->getConstantSize() == 4)
      {
      trfprintf(pOutFile, "DC   \t0x%08x ", snippet->getDataAs4Bytes());
      }
   else if (snippet->getConstantSize() == 2)
      {
      trfprintf(pOutFile, "DC   \t0x%04x ", snippet->getDataAs2Bytes());
      }
   else
      {
      trfprintf(pOutFile, "DC\n");
      int n = snippet->getConstantSize();
      uint8_t *p = snippet->getRawData();
      while (n >= 8)
         {
         trfprintf(pOutFile, "\t%016llx\n", *(uint64_t *) p);
         n -= 8;
         p += 8;
         }
      if (n)
         {
         trfprintf(pOutFile, "\t");
         for (int32_t i = 0; i < n; i++)
            {
            trfprintf(pOutFile, "%02x ", *p++);
            }
         }
      }
   }
