/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "z/codegen/ConstantDataSnippet.hpp"

#include <stddef.h>                             // for NULL
#include <stdint.h>                             // for uint8_t, uint32_t, etc
#include <string.h>                             // for memcpy, strlen
#include <algorithm>                            // for std::find
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd
#include "codegen/InstOpCode.hpp"               // for InstOpCode
#include "codegen/Instruction.hpp"              // for Instruction
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"              // for Compilation
#include "compile/Method.hpp"                   // for TR_Method
#include "compile/ResolvedMethod.hpp"           // for TR_ResolvedMethod
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"                      // for TR_PatchJNICallSite
#endif
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"                  // for ObjectModel
#include "env/TRMemory.hpp"
#include "env/jittypes.h"                       // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"                     // for ILOpCodes::aconst
#include "il/Node.hpp"                          // for Node
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                        // for Symbol
#include "il/SymbolReference.hpp"               // for SymbolReference
#include "il/TreeTop.hpp"                       // for TreeTop
#include "il/TreeTop_inlines.hpp"               // for TreeTop::getNode
#include "il/symbol/LabelSymbol.hpp"            // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"   // for ResolvedMethodSymbol
#include "il/symbol/StaticSymbol.hpp"           // for StaticSymbol
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/Link.hpp"                       // for TR_Pair
#include "infra/List.hpp"                       // for List, ListIterator
#include "ras/Debug.hpp"                        // for TR_Debug
#include "runtime/Runtime.hpp"

#if defined(TR_HOST_S390)
#include <time.h>                               // for NULL
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

   switch (reloType)
      {
      case 0:
         break;

      case TR_ClassAddress:
      case TR_ClassObject:
      case TR_MethodObject:
         {
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         TR::SymbolReference *reloSymRef= (reloType==TR_ClassAddress)?getNode()->getSymbolReference():getSymbolReference();
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) reloSymRef,
                                                                                getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                (TR_ExternalRelocationTargetKind) reloType, cg()),
                                   __FILE__, __LINE__, getNode());
         }
         break;

      case TR_JNIStaticTargetAddress:
      case TR_JNISpecialTargetAddress:
      case TR_JNIVirtualTargetAddress:
         {
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getNode()->getSymbolReference(),
                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                        (TR_ExternalRelocationTargetKind) reloType, cg()),
                        __FILE__, __LINE__, getNode());
         }
         break;

      case TR_DataAddress:
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getNode()->getSymbolReference(),
                                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                (TR_ExternalRelocationTargetKind) reloType, cg()),
                                  __FILE__,__LINE__, getNode());
         break;

      case TR_ArrayCopyHelper:
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getSymbolReference(),
                                                                                (TR_ExternalRelocationTargetKind) reloType, cg()),
                                __FILE__, __LINE__, getNode());
         break;

      case TR_HelperAddress:
         AOTcgDiag1(comp, "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
         if (TR::Compiler->target.is64Bit())
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor), TR_AbsoluteHelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
            }
         else
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor), TR_AbsoluteHelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
            }
         break;

      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
         AOTcgDiag2(comp, "add relocation (%d) cursor=%x\n", reloType, cursor);
         if (TR::Compiler->target.is64Bit())
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType, cg()),
                  __FILE__, __LINE__, getNode());
            }
         else
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType, cg()),
                  __FILE__, __LINE__, getNode());
            }
         break;

      case TR_GlobalValue:
         AOTcgDiag2(comp, "add TR_GlobalValue (countForRecompile) cursor=%x\n", reloType, cursor);
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) TR_CountForRecompile,
                                                                                TR_GlobalValue, cg()),
                                  __FILE__, __LINE__, getNode());
         break;

      case TR_RamMethod:
      case TR_MethodPointer:
      case TR_ClassPointer:
         {
         AOTcgDiag2(comp, "add relocation (%d) cursor=%x\n", reloType, cursor);
         TR::Relocation *relo;
         //for optimizations where we are trying to relocate either profiled j9class or getfrom signature we can't use node to get the target address
         //so we need to pass it to relocation in targetaddress2 for now
         //two instances where use this relotype in such way are: profile checkcast and arraystore check object check optimiztaions
         uint8_t * targetAdress2 = NULL;
         if (getNode()->getOpCodeValue() != TR::aconst)
            {
            if (TR::Compiler->target.is64Bit())
               targetAdress2 = (uint8_t *) *((uint64_t*) cursor);
            else
               targetAdress2 = (uint8_t *) *((uintptrj_t*) cursor);
            }
         relo = new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getNode(), targetAdress2, (TR_ExternalRelocationTargetKind) reloType, cg());
         cg()->addExternalRelocation(relo, __FILE__, __LINE__, getNode());
         }
         break;

      case TR_DebugCounter:
         {
         TR::DebugCounterBase *counter = cg()->comp()->getCounterFromStaticAddress(getNode()->getSymbolReference());
         if (counter == NULL)
            {
            cg()->comp()->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::X86::MemoryReference::addMetaDataForCodeAddress\n");
            }
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x counter=%x\n", reloType, cursor, counter);
         TR::DebugCounter::generateRelocation(cg()->comp(),
                                              cursor,
                                              getNode(),
                                              counter);
         }
         break;

      default:
         TR_ASSERT( 0,"relocation type not handled yet");
      }

   if (comp->getOption(TR_EnableHCR))
      {
      if (std::find(comp->getSnippetsToBePatchedOnClassRedefinition()->begin(), comp->getSnippetsToBePatchedOnClassRedefinition()->end(), this) != comp->getSnippetsToBePatchedOnClassRedefinition()->end())
         {
         cg()->jitAddPicToPatchOnClassRedefinition(((void *) (*(uintptrj_t *) cursor)), (void *) (uintptrj_t *) cursor);
         }

      if (std::find(comp->getSnippetsToBePatchedOnClassUnload()->begin(), comp->getSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getSnippetsToBePatchedOnClassUnload()->end())
         cg()->jitAddPicToPatchOnClassUnload(((void *) (*(uintptrj_t *) cursor)), (void *) (uintptrj_t *) cursor);

      if (std::find(comp->getMethodSnippetsToBePatchedOnClassUnload()->begin(), comp->getMethodSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getMethodSnippetsToBePatchedOnClassUnload()->end())
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (*(uintptrj_t *) cursor), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAddPicToPatchOnClassUnload(classPointer, (void *) (uintptrj_t *) cursor);
         }
      }
   else
      {
      if (std::find(comp->getSnippetsToBePatchedOnClassUnload()->begin(), comp->getSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getSnippetsToBePatchedOnClassUnload()->end())
         {
         cg()->jitAddPicToPatchOnClassUnload(((void *) (*(uintptrj_t *) cursor)), (void *) (uintptrj_t *) cursor);
         }

      if (std::find(comp->getMethodSnippetsToBePatchedOnClassUnload()->begin(), comp->getMethodSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getMethodSnippetsToBePatchedOnClassUnload()->end())
         {
         void *classPointer = (void *) cg()->fe()->createResolvedMethod(cg()->trMemory(), (TR_OpaqueMethodBlock *) (*(uintptrj_t *) cursor), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAddPicToPatchOnClassUnload(classPointer, (void *) (uintptrj_t *) cursor);
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   // Check if this is a jni method address, and if so, register assumption
   for (auto it = comp->getSnippetsToBePatchedOnRegisterNative()->begin(); it != comp->getSnippetsToBePatchedOnRegisterNative()->end(); ++it)
      {
      TR::Snippet *s = (*it)->getKey();
      if (this == s)
         {
         TR_OpaqueMethodBlock *method = (*it)->getValue()->getPersistentIdentifier();
         TR_PatchJNICallSite::make(cg()->fe(), cg()->trPersistentMemory(), (uintptrj_t) method, cursor, comp->getMetadataAssumptionList());
         }
      }

   // For Unresolved Data Calls, we need to insert the address of this literal
   // pool reference, so that the PICbuilder code can patch the resolved address
   if (getUnresolvedDataSnippet() != NULL)
      {
      uint8_t * udsPatchLocation = getUnresolvedDataSnippet()->getLiteralPoolPatchAddress();
      TR_ASSERT(udsPatchLocation != NULL,"Literal Pool Reference has NULL Unresolved Data Snippet patch site!");
      *(intptrj_t *)udsPatchLocation = (intptrj_t)cursor;
      getUnresolvedDataSnippet()->setLiteralPoolSlot(cursor);
      if (getUnresolvedDataSnippet()->getDataSymbol()->isClassObject() && cg()->wantToPatchClassPointer(NULL, cursor)) // unresolved
         {
         cg()->jitAddPicToPatchOnClassRedefinition((void*) *(uintptrj_t *)cursor , (void *) cursor, true);
         }
      }
#endif

   }


uint8_t *
TR::S390ConstantDataSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();

   AOTcgDiag1(comp, "TR::S390ConstantDataSnippet::emitSnippetBody cursor=%x\n", cursor);
   getSnippetLabel()->setCodeLocation(cursor);
   memcpy(cursor, &_value, _length);

   uint32_t reloType = getReloType();

   AOTcgDiag6(comp, "this=%p cursor=%p node=%p value=%p length=%x reloType=%x\n", this, cursor, getNode(), *((int*)cursor), _length, reloType);

   switch (reloType)
      {
      case TR_ClassAddress:
      case TR_ClassObject:
         {
         uintptrj_t romClassPtr = TR::Compiler->cls.persistentClassPointerFromClassPointer(comp, (TR_OpaqueClassBlock*)(*((uintptrj_t*)cursor)));
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

uint32_t
TR::S390JNICallDataSnippet::getJNICallOutFrameFlagsOffset()
   {
   return TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getReturnFromJNICallOffset()
   {
   return 2 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getSavedPCOffset()
   {
   return 3 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getTagBitsOffset()
   {
   return 4 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getPCOffset()
   {
   return 5 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getLiteralsOffset()
   {
   return 6 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getJitStackFrameFlagsOffset()
   {
   return 7 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getConstReleaseVMAccessMaskOffset()
   {
   return 8 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getConstReleaseVMAccessOutOfLineMaskOffset()
   {
   return 9 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getTargetAddressOffset()
   {
   return 10 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390JNICallDataSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 12 * TR::Compiler->om.sizeofReferenceAddress(); /*one ptr more for possible padding */
   }


TR::S390JNICallDataSnippet::S390JNICallDataSnippet(TR::CodeGenerator * cg,
                               TR::Node * node)
: TR::S390ConstantDataSnippet(cg, node, generateLabelSymbol(cg),0),
 _baseRegister(0),
 _ramMethod(0),
 _JNICallOutFrameFlags(0),
 _returnFromJNICallLabel(0),
 _savedPC(0),
 _tagBits(0),
 _pc(0),
 _literals(0),
 _jitStackFrameFlags(0),
 _constReleaseVMAccessMask(0),
 _constReleaseVMAccessOutOfLineMask(0),
 _targetAddress(0)
   {
   return;
   }

uint8_t *
TR::S390JNICallDataSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();

#ifdef J9_PROJECT_SPECIFIC
   /* TR::S390JNICallDataSnippet Layout: all fields are pointer sized
       ramMethod
       JNICallOutFrameFlags
       returnFromJNICall
       savedPC
       tagBits
       pc
       literals
       jitStackFrameFlags
       constReleaseVMAccessMask
       constReleaseVMAccessOutOfLineMask
       targetAddress
   */
      TR::Compilation *comp = cg()->comp();

      AOTcgDiag1(comp, "TR::S390JNICallDataSnippet::emitSnippetBody cursor=%x\n", cursor);
      // Ensure pointer sized alignment
      int32_t alignSize = TR::Compiler->om.sizeofReferenceAddress();
      int32_t padBytes = ((intptrj_t)cursor + alignSize -1) / alignSize * alignSize - (intptrj_t)cursor;
      cursor += padBytes;

      getSnippetLabel()->setCodeLocation(cursor);
      TR::Node * callNode = getNode();

      intptrj_t snippetStart = (intptrj_t)cursor;

      //  JNI Callout Frame data
      // _ramMethod
      *(intptrj_t *) cursor = (intptrj_t) _ramMethod;

      uint32_t reloType;
      if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_SpecialRamMethodConst;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_StaticRamMethodConst;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_VirtualRamMethodConst;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }

      AOTcgDiag2(comp, "add relocation (%d) cursor=%x\n", reloType, cursor);
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) callNode->getSymbolReference(),
               callNode  ? (uint8_t *)(intptr_t)callNode->getInlinedSiteIndex() : (uint8_t *)-1,
                     (TR_ExternalRelocationTargetKind) reloType, cg()),
                     __FILE__, __LINE__, callNode);

      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // _JNICallOutFrameFlags
       *(intptrj_t *) cursor = (intptrj_t) _JNICallOutFrameFlags;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _returnFromJNICall
       *(intptrj_t *) cursor = (intptrj_t) (_returnFromJNICallLabel->getCodeLocation());

       AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
       cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, _returnFromJNICallLabel));
       cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
             __FILE__, __LINE__, getNode());

       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _savedPC
       *(intptrj_t *) cursor = (intptrj_t) _savedPC;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _tagBits
       *(intptrj_t *) cursor = (intptrj_t) _tagBits;
       cursor += TR::Compiler->om.sizeofReferenceAddress();

       //VMThread data
       // _pc
       *(intptrj_t *) cursor = (intptrj_t) _pc;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _literals
       *(intptrj_t *) cursor = (intptrj_t) _literals;
       cursor += TR::Compiler->om.sizeofReferenceAddress();
       // _jitStackFrameFlags
       *(intptrj_t *) cursor = (intptrj_t) _jitStackFrameFlags;
       cursor += TR::Compiler->om.sizeofReferenceAddress();

       // _constReleaseVMAccessMask
      *(intptrj_t *) cursor = (intptrj_t) _constReleaseVMAccessMask;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      // _constReleaseVMAccessOutOfLineMask
      *(intptrj_t *) cursor = (intptrj_t) _constReleaseVMAccessOutOfLineMask;
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // _targetAddress/function pointer of native method
      *(intptrj_t *) cursor = (intptrj_t) _targetAddress;
      TR_OpaqueMethodBlock *method = getNode()->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier();
      TR_PatchJNICallSite::make(cg()->fe(), cg()->trPersistentMemory(), (uintptrj_t) method, cursor, comp->getMetadataAssumptionList());

      if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isSpecial())
         reloType = TR_JNISpecialTargetAddress;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isStatic())
         reloType = TR_JNIStaticTargetAddress;
      else if (getNode()->getSymbol()->castToResolvedMethodSymbol()->isVirtual())
         reloType = TR_JNIVirtualTargetAddress;
      else
         {
         reloType = TR_NoRelocation;
         TR_ASSERT(0,"JNI relocation not supported.");
         }

      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) callNode->getSymbolReference(),
               callNode  ? (uint8_t *)(intptr_t)callNode->getInlinedSiteIndex() : (uint8_t *)-1,
                     (TR_ExternalRelocationTargetKind) reloType, cg()),
                     __FILE__, __LINE__, callNode);

      cursor += TR::Compiler->om.sizeofReferenceAddress();
#endif

   return cursor;
   }

void
TR::S390JNICallDataSnippet::print(TR::FILE *pOutFile, TR_Debug *debug)
   {
/*
       ramMethod
       JNICallOutFrameFlags
       returnFromJNICall
       savedPC
       tagBits
       pc
       literals
       jitStackFrameFlags
       constReleaseVMAccessMask
       constReleaseVMAccessOutOfLineMask
       targetAddress

*/
   TR_FrontEnd *fe = cg()->comp()->fe();
   uint8_t * bufferPos = getSnippetLabel()->getCodeLocation();

   int i = 0;

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), bufferPos, "JNI Call Data Snippet");

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# ramMethod",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# JNICallOutFrameFlags",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# returnFromJNICall", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# savedPC", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# tagBits", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# pc", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# literals", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# jitStackFrameFlags", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# constReleaseVMAccessMask",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# constReleaseVMAccessOutOfLineMask",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   debug->printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# targetAddress",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);
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
