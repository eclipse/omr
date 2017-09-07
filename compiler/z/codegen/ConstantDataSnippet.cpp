/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
   TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false)
   {

   if (c)
      memcpy(_value, c, size);
   _unresolvedDataSnippet = NULL;
   _length = size;
   _symbolReference = NULL;
   _reloType = 0;
   _isString = false;
   }

TR::S390ConstantDataSnippet::S390ConstantDataSnippet(TR::CodeGenerator * cg, TR::Node * n, char* c) :
   TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false)
   {
   _length = strlen(c);
   _string = (char *) cg->trMemory()->allocateMemory(_length, heapAlloc);
   memcpy(_string, c, strlen(c));
   _unresolvedDataSnippet = NULL;
   _symbolReference = NULL;
   _reloType = 0;
   _isString = true;
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
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) reloSymRef,
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
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)getNode()->getSymbolReference(),
                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                        (TR_ExternalRelocationTargetKind) reloType, cg()),
                        __FILE__, __LINE__, getNode());
         }
         break;

      case TR_DataAddress:
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getNode()->getSymbolReference(),
                                  getNode() ? (uint8_t *)(intptr_t)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                (TR_ExternalRelocationTargetKind) reloType, cg()),
                                  __FILE__,__LINE__, getNode());
         break;

      case TR_ArrayCopyHelper:
         AOTcgDiag3(comp, "add relocation (%d) cursor=%x symbolReference=%x\n", reloType, cursor, getSymbolReference());
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) getSymbolReference(),
                                                                                (TR_ExternalRelocationTargetKind) reloType, cg()),
                                __FILE__, __LINE__, getNode());
         break;

      case TR_HelperAddress:
         AOTcgDiag1(comp, "add TR_AbsoluteHelperAddress cursor=%x\n", cursor);
         if (TR::Compiler->target.is64Bit())
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor), TR_AbsoluteHelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
            }
         else
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor), TR_AbsoluteHelperAddress, cg()),
                                __FILE__, __LINE__, getNode());
            }
         break;

      case TR_AbsoluteMethodAddress:
      case TR_BodyInfoAddress:
         AOTcgDiag2(comp, "add relocation (%d) cursor=%x\n", reloType, cursor);
         if (TR::Compiler->target.is64Bit())
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) *((uint64_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType, cg()),
                  __FILE__, __LINE__, getNode());
            }
         else
            {
            cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)(intptr_t) *((uint32_t*) cursor),
                  (TR_ExternalRelocationTargetKind) reloType, cg()),
                  __FILE__, __LINE__, getNode());
            }
         break;

      case TR_GlobalValue:
         AOTcgDiag2(comp, "add TR_GlobalValue (countForRecompile) cursor=%x\n", reloType, cursor);
         cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) TR_CountForRecompile,
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
         cg()->addAOTRelocation(relo, __FILE__, __LINE__, getNode());
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
   setIsRefed(true);
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

//////////////////////////////////////////////////////////////////////////////////////
// TR::S390TargetAddressSnippet member functions
//////////////////////////////////////////////////////////////////////////////////////
TR::S390TargetAddressSnippet::S390TargetAddressSnippet(TR::CodeGenerator * cg, TR::Node * n, TR::LabelSymbol * targetLabel)
   : TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false), _targetsnippet(NULL), _targetaddress(0), _targetsym(NULL),
   _targetlabel(targetLabel)
   {
   }

TR::S390TargetAddressSnippet::S390TargetAddressSnippet(TR::CodeGenerator * cg, TR::Node * n, TR::Snippet * s)
   : TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false), _targetsnippet(s), _targetaddress(0), _targetsym(NULL), _targetlabel(NULL)
   {
   }

TR::S390TargetAddressSnippet::S390TargetAddressSnippet(TR::CodeGenerator * cg, TR::Node * n, uintptrj_t addr)
   : TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false), _targetsnippet(NULL), _targetaddress(addr), _targetsym(NULL),
   _targetlabel(NULL)
   {
   }

TR::S390TargetAddressSnippet::S390TargetAddressSnippet(TR::CodeGenerator * cg, TR::Node * n, TR::Symbol * sym)
   : TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false), _targetsnippet(NULL), _targetaddress(0), _targetsym(sym), _targetlabel(NULL)
   {
   }

uint8_t *
TR::S390TargetAddressSnippet::emitSnippetBody()
   {
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   AOTcgDiag1(cg()->comp(), "TR::S390TargetAddressSnippet::emitSnippetBody cursor=%x\n", cursor);
   getSnippetLabel()->setCodeLocation(cursor);
   TR::Compilation* comp = cg()->comp();

   if (_targetsnippet != NULL)
      {
      *(intptrj_t *) cursor = 0;
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
      cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, _targetsnippet->getSnippetLabel()));
      cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, getNode());
      }
   else if (_targetlabel != NULL)
      {
      *(intptrj_t *) cursor = 0;
      if (_targetlabel->getCodeLocation() != NULL)
         {
         *(intptrj_t *) cursor = (intptrj_t) (_targetlabel->getCodeLocation());
         }
      else
         {
         AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, _targetlabel));
         cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                                   __FILE__, __LINE__, getNode());
         }
      }
   else
      {
      if (_targetsym != NULL)
         {
         TR_ResolvedMethod * resolvedMethod = _targetsym->getResolvedMethodSymbol()->getResolvedMethod();

         if (resolvedMethod != NULL && resolvedMethod->isSameMethod(comp->getCurrentMethod()))
            {
            uint8_t * jitTojitStart = cg()->getCodeStart();

            // Calculate jit-to-jit entry point
            jitTojitStart += ((*(intptrj_t *) (jitTojitStart - 4)) >> 16) & 0x0000ffff;
            *(intptrj_t *) cursor = (intptrj_t) jitTojitStart;
            }
         }
      else
         {
         *(intptrj_t *) cursor = _targetaddress;
         }
      }
   cursor += sizeof(uintptrj_t);

   return cursor;
   }

uint32_t
TR::S390TargetAddressSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   return sizeof(uintptrj_t);
   }

//////////////////////////////////////////////////////////////////////////////////////

TR::S390LookupSwitchSnippet::S390LookupSwitchSnippet(TR::CodeGenerator * cg, TR::Node * switchNode)
   : TR::S390TargetAddressSnippet(cg, switchNode, TR::LabelSymbol::create(cg->trHeapMemory(),cg))
   {
   // *this   swipeable for debugger
   }


uint8_t *
TR::S390LookupSwitchSnippet::emitSnippetBody()
   {
   int32_t numChildren = getNode()->getCaseIndexUpperBound() - 1;
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   TR::Compilation* comp = cg()->comp();

   AOTcgDiag1(comp, "TR::S390LookupSwitchSnippet::emitSnippetBody cursor=%x\n", cursor);
   getSnippetLabel()->setCodeLocation(cursor);

   for (int32_t i = 1; i <= numChildren; i++)
      {
      // Each case stmt is described by a node on the child
      TR::Node * caseKeyNode = getNode()->getChild(i);
      CASECONST_TYPE caseKeyValue = caseKeyNode->getCaseConstant();
      intptrj_t caseKeyTarget = (intptrj_t) caseKeyNode->getBranchDestination()->getNode()->getLabel()->getCodeLocation();

      *(int32_t *) cursor = caseKeyValue;
      cursor += 4;
      *(intptrj_t *) cursor = caseKeyTarget;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }

   *(int32_t *) cursor = 0;
   cursor += 4;

   return cursor;
   }

uint32_t
TR::S390LookupSwitchSnippet::getLength(int32_t  estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   return (getNode()->getCaseIndexUpperBound() - 1) * (sizeof(int32_t) + TR::Compiler->om.sizeofReferenceAddress());
   }

///////////////////////////////////////////////////////////////////////////////
// TR::S390InterfaceCallDataSnippet member functions
///////////////////////////////////////////////////////////////////////////////
TR::S390InterfaceCallDataSnippet::S390InterfaceCallDataSnippet(TR::CodeGenerator * cg,
                                                                 TR::Node * node,
                                                                 uint8_t n,
                                                                 bool useCLFIandBRCL)
   : TR::S390ConstantDataSnippet(cg,node,TR::LabelSymbol::create(cg->trHeapMemory(),cg),0),
     _numInterfaceCallCacheSlots(n),
     _codeRA(NULL),
     thunkAddress(NULL),
     _useCLFIandBRCL(useCLFIandBRCL)
   {
   }

TR::S390InterfaceCallDataSnippet::S390InterfaceCallDataSnippet(TR::CodeGenerator * cg,
                                                                 TR::Node * node,
                                                                 uint8_t n,
                                                                 uint8_t *thunkPtr,
                                                                 bool useCLFIandBRCL)
   : TR::S390ConstantDataSnippet(cg,node,TR::LabelSymbol::create(cg->trHeapMemory(),cg),0),
     _numInterfaceCallCacheSlots(n),
     _codeRA(NULL),
     thunkAddress(thunkPtr),
     _useCLFIandBRCL(useCLFIandBRCL)
   {
   }

uint8_t *
TR::S390InterfaceCallDataSnippet::emitSnippetBody()
   {

//  64-Bit Layout.
//  0x00000100211cfe58     DC  0x100211cf58a    ; Call Site RA
//  0x00000100211cfe60     DC  0x8020b380       ; Address Of Constant Pool
//  0x00000100211cfe68     DC  0x00000040       ; CP Index
//  0x00000100211cfe70     DC  0x00000000       ; Interface class
//  0x00000100211cfe78     DC  0x00000000       ; Method Index
//
//  0x00000100211cfe80     DC  0x00000000       ; Flags (resolved)
//  0x00000100211cfe88     DC  0x100211cfe90    ; lastCachedSlotField
//
//  0x00000100211cfe90     DC  0x100211cfea0    ; firstSlotField
//  0x00000100211cfe98     DC  0x100211cfed0    ; lastSlotField
//

//  If we use L/C/BR sequence:

//  0x00000100211cfea0     DC  0x00000000       ; Cached Class1
//  0x00000100211cfea8     DC  0x00000000       ; Cached Method1
//
//  0x00000100211cfeb0     DC  0x00000000       ; Cached Class2
//  0x00000100211cfeb8     DC  0x00000000       ; Cached Method2
//
//  0x00000100211cfec0     DC  0x00000000       ; Cached Class3
//  0x00000100211cfec8     DC  0x00000000       ; Cached Method3
//
//  0x00000100211cfed0     DC  0x00000000       ; Cached Class4
//  0x00000100211cfed8     DC  0x00000000       ; Cached Method4
//
//  and so on...

// If we use CLFI / BRCL
//  0x00000100211cfea0     DC  0x00000000       ; Patch slot of the first CLFI (Class 1)
//  0x00000100211cfea0     DC  0x00000000       ; Patch slot of the second CLFI (Class 2)
//  0x00000100211cfea0     DC  0x00000000       ; Patch slot of the third CLFI (Class 3)
// and so on ...

//  31-Bit Layout.
//  0x00000000211cfe50     DC  0x0000610ff8     ; Address Of Dispatch Glue
//  0x00000000211cfe54     DC  0x00211cf58a     ; Call Site RA
//  0x00000000211cfe58     DC  0x2020b380       ; Address Of Constant Pool
//  0x00000000211cfe5c     DC  0x00000040       ; CP Index
//  0x00000000211cfe60     DC  0x00000000       ; Interface class
//  0x00000000211cfe64     DC  0x00000000       ; Method Index
//
//  0x00000000211cfe68     DC  0x00000000       ; Cached Class1
//  0x00000000211cfe6c     DC  0x00000000       ; Cached Method1
//
//  0x00000000211cfe70     DC  0x00000000       ; Flags (resolved)

   TR::Compilation *comp = cg()->comp();
   int32_t i = 0;
   uint8_t * cursor = cg()->getBinaryBufferCursor();
   AOTcgDiag1(comp, "TR::S390InterfaceCallDataSnippet::emitSnippetBody cursor=%x\n", cursor);

   // Class Pointer must be double word aligned.
   int32_t padBytes = ((intptrj_t)cursor + 5 * sizeof(intptrj_t) + 7) / 8 * 8 - ((intptrj_t)cursor + 5 * sizeof(intptrj_t));
   cursor += padBytes;

   getSnippetLabel()->setCodeLocation(cursor);
   TR::Node * callNode = getNode();

   intptrj_t snippetStart = (intptrj_t)cursor;

   // code cache RA
   TR_ASSERT(_codeRA != NULL, "Interface Call Data Constant's code return address not initialized.\n");
   *(uintptrj_t *) cursor = (uintptrj_t)_codeRA;
   AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                             __FILE__, __LINE__, callNode);
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // constant pool
   *(uintptrj_t *) cursor = (uintptrj_t) callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();

   AOTcgDiag1(comp, "add TR_Thunks cursor=%x\n", cursor);
   cg()->addProjectSpecializedRelocation(cursor, *(uint8_t **)cursor, callNode ? (uint8_t *)(intptr_t)callNode->getInlinedSiteIndex() : (uint8_t *)-1, TR_Thunks,
                             __FILE__, __LINE__, callNode);
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   //  save CPIndex as sign extended 8 byte value on 64bit as it's assumed in J9 helpers
   // cp index
   *(intptrj_t *) cursor = (intptrj_t) callNode->getSymbolReference()->getCPIndex();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   //interface class
   *(uintptrj_t *) cursor = 0;
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // method index
   *(intptrj_t *) cursor = 0;
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   if (getNumInterfaceCallCacheSlots() == 0)
      {
      // Flags
      *(intptrj_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      return cursor;
      }
   uint8_t * cursorlastCachedSlot = 0;
   if (!comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      {
      // Flags
      *(intptrj_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // lastCachedSlot
      cursorlastCachedSlot = cursor;
      *(intptrj_t *) (cursor) =  snippetStart + getFirstSlotOffset() - (2 * TR::Compiler->om.sizeofReferenceAddress());
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
      cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, callNode);
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // firstSlot
      *(intptrj_t *) (cursor) = snippetStart + getFirstSlotOffset();
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
      cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, callNode);
      cursor += TR::Compiler->om.sizeofReferenceAddress();

      // lastSlot
      *(intptrj_t *) (cursor) =  snippetStart + getLastSlotOffset();
      AOTcgDiag1(comp, "add TR_AbsoluteMethodAddress cursor=%x\n", cursor);
      cg()->addProjectSpecializedRelocation(cursor, NULL, NULL, TR_AbsoluteMethodAddress,
                                __FILE__, __LINE__, callNode);
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }

    // Cursor must be double word aligned by this point.
    TR_ASSERT( (intptrj_t)cursor % 8 == 0, "Interface Call Data Snippet Class Ptr is not double word aligned.");

    bool updateField = false;
     int32_t numInterfaceCallCacheSlots = getNumInterfaceCallCacheSlots();
     TR::list<TR_OpaqueClassBlock*> * profiledClassesList = cg()->getPICsListForInterfaceSnippet(this);
     if (profiledClassesList)
        {
        for (auto valuesIt = profiledClassesList->begin(); valuesIt != profiledClassesList->end(); ++valuesIt)
           {
           TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
           TR_ResolvedMethod * profiledMethod = methodSymRef->getOwningMethod(comp)->getResolvedInterfaceMethod(comp,
                 (TR_OpaqueClassBlock *)(*valuesIt), methodSymRef->getCPIndex());
           numInterfaceCallCacheSlots--;
           updateField = true;
           if (TR::Compiler->target.is64Bit() && TR::Compiler->om.generateCompressedObjectHeaders())
              *(uintptrj_t *) cursor = (uintptrj_t) (*valuesIt) << 32;
           else
              *(uintptrj_t *) cursor = (uintptrj_t) (*valuesIt);

           if (comp->getOption(TR_EnableHCR))
              {
              cg()->jitAddPicToPatchOnClassRedefinition(*valuesIt, (void *) cursor);
              }

           if (cg()->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *)(*valuesIt), comp->getCurrentMethod()))
              {
              cg()->jitAddPicToPatchOnClassUnload(*valuesIt, (void *) cursor);
              }

           cursor += TR::Compiler->om.sizeofReferenceAddress();

           // Method Pointer
           *(uintptrj_t *) (cursor) = (uintptrj_t)profiledMethod->startAddressForJittedMethod();
           cursor += TR::Compiler->om.sizeofReferenceAddress();
           }

        }

     // Skip the top cache slots that are filled with IProfiler data by setting the cursorlastCachedSlot to point to the fist dynamic cache slot
     if (updateField)
        {
        *(intptrj_t *) (cursorlastCachedSlot) =  snippetStart + getFirstSlotOffset() + ((getNumInterfaceCallCacheSlots()-numInterfaceCallCacheSlots-1)*2 * TR::Compiler->om.sizeofReferenceAddress());
        }

     for (i = 0; i < numInterfaceCallCacheSlots; i++)
      {
      if (isUseCLFIandBRCL())
         {
         // Get Address of CLFI's immediate field (2 bytes into CLFI instruction).
         // jitAddPicToPatchOnClassUnload is called by PicBuilder code when the cache is populated.
         *(intptrj_t*) cursor = (intptrj_t)(getFirstCLFI()->getBinaryEncoding() ) + (intptrj_t)(i * 12 + 2);
         cursor += TR::Compiler->om.sizeofReferenceAddress();
         }
      else
         {
         // Class Pointer - jitAddPicToPatchOnClassUnload is called by PicBuilder code when the cache is populated.
         *(intptrj_t *) cursor = 0;
         cursor += TR::Compiler->om.sizeofReferenceAddress();

         // Method Pointer
         *(intptrj_t *) (cursor) = 0;
         cursor += TR::Compiler->om.sizeofReferenceAddress();
         }
      }

   if (comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      {
      // Flags
      *(intptrj_t *) (cursor) = 0;
      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }

   return cursor;
   }


uint32_t
TR::S390InterfaceCallDataSnippet::getCallReturnAddressOffset()
   {
   return 0;
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getSingleDynamicSlotOffset()
   {
   return getCallReturnAddressOffset() + 5 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getLastCachedSlotFieldOffset()
   {
   return getCallReturnAddressOffset() + 6 * TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getFirstSlotFieldOffset()
   {
   return getLastCachedSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getLastSlotFieldOffset()
   {
   return getFirstSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getFirstSlotOffset()
   {
   return getLastSlotFieldOffset() + TR::Compiler->om.sizeofReferenceAddress();
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getLastSlotOffset()
   {
   return (getFirstSlotOffset() +
          (getNumInterfaceCallCacheSlots() - 1) *
             ((isUseCLFIandBRCL()?1:2) * TR::Compiler->om.sizeofReferenceAddress()));
   }

uint32_t
TR::S390InterfaceCallDataSnippet::getLength(int32_t)
   {
   TR::Compilation *comp = cg()->comp();
   // the 1st item is for padding...
   if (getNumInterfaceCallCacheSlots()== 0)
      return 2 * TR::Compiler->om.sizeofReferenceAddress() + 8 * TR::Compiler->om.sizeofReferenceAddress();

   if (comp->getOption(TR_enableInterfaceCallCachingSingleDynamicSlot))
      return 2 * TR::Compiler->om.sizeofReferenceAddress() + getSingleDynamicSlotOffset() + 4 * TR::Compiler->om.sizeofReferenceAddress();

   if (getNumInterfaceCallCacheSlots() > 0)
      return 2 * TR::Compiler->om.sizeofReferenceAddress() + getLastSlotOffset() + TR::Compiler->om.sizeofReferenceAddress();

   TR_ASSERT(0,"Interface Call Data Snippet has 0 size.");
   return 0;
   }


///////////////////////////////////////////////////////////////////////////////
// TR::S390InterfaceCallDataSnippet member functions
///////////////////////////////////////////////////////////////////////////////

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
: TR::S390ConstantDataSnippet(cg, node, TR::LabelSymbol::create(cg->trHeapMemory(),cg),0),
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
      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) callNode->getSymbolReference(),
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
       cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, NULL, TR_AbsoluteMethodAddress, cg()),
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

      cg()->addAOTRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) callNode->getSymbolReference(),
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
TR_Debug::print(TR::FILE *pOutFile, TR::S390TargetAddressSnippet * snippet)
   {
   // *this   swipeable for debugger
   if (pOutFile == NULL)
      {
      return;
      }

   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Target Address Snippet");
   printPrefix(pOutFile, NULL, bufferPos, sizeof(uintptrj_t));

   if (snippet->getTargetSnippet() != NULL)
      {
      print(pOutFile, snippet->getTargetSnippet()->getSnippetLabel());
      }
   else
      {
      trfprintf(pOutFile, "DC   \t%p", snippet->getTargetAddress());
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390LookupSwitchSnippet * snippet)
   {
   int upperBound = snippet->getNode()->getCaseIndexUpperBound();
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Lookup Switch Table");

   for (int i = 1; i < upperBound; i++)
      {
      // Each case stmt is described by a node on the child
      TR::Node * caseKeyNode = snippet->getNode()->getChild(i);
      int32_t caseKeyValue = caseKeyNode->getCaseConstant();
      int32_t caseKeyTarget = (intptrj_t) caseKeyNode->getBranchDestination()->getNode()->getLabel()->getCodeLocation();

      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "DC   \t %p", caseKeyValue);
      bufferPos += 4;
      printPrefix(pOutFile, NULL, bufferPos, 4);
      trfprintf(pOutFile, "DC   \t %p", caseKeyTarget);
      bufferPos += 4;
      }
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::S390ConstantDataSnippet * snippet)
   {
   // *this   swipeable for debugger
   if (pOutFile == NULL)
      {
      return;
      }

   uint32_t size=0, start_addr=0, offset=0;


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
   else if (snippet->getKind() == TR::Snippet::IsLabelTable)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Label Table Snippet");
      trfprintf(pOutFile, "\n");
      TR::S390LabelTableSnippet *labelTable = (TR::S390LabelTableSnippet *) snippet;
      for (int32_t i = 0; i < labelTable->getSize(); i++)
         {
         TR::LabelSymbol *label = labelTable->getLabel(i);
         if (label)
            {
            trfprintf(pOutFile, "[");
            print(pOutFile, label);
            trfprintf(pOutFile, "] ");
            }
         else
            trfprintf(pOutFile, "[NULL] ");
         }
      return;
      }
   else if (snippet->getKind() == TR::Snippet::IsDeclTramp)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Declarative Trampoline Snippet");
      trfprintf(pOutFile, "\n");
      TR::S390DeclTrampSnippet *declTrampSnippet = (TR::S390DeclTrampSnippet *) snippet;
      TR::LabelSymbol *label = declTrampSnippet->getLabel();
      trfprintf(pOutFile, "[");
      print(pOutFile, label);
      trfprintf(pOutFile, "] ");
      return;
      }
   else if (snippet->getKind() == TR::Snippet::IsSortJumpTramp)
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Sort Jump Trampoline Snippet");
      trfprintf(pOutFile, "\n");
      TR::S390SortJumpTrampSnippet *sortJumpTrampSnippet = (TR::S390SortJumpTrampSnippet *) snippet;
      TR::LabelSymbol *label = sortJumpTrampSnippet->getLabel();
      trfprintf(pOutFile, "[");
      print(pOutFile, label);
      trfprintf(pOutFile, "] ");
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


void
TR_Debug::print(TR::FILE *pOutFile, TR::S390InterfaceCallDataSnippet * snippet)
   {
   uint8_t * bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   int isSingleDynamicSlot = 0;
   int i = 0;

   if (snippet->getLength(0) == 6 * sizeof(intptrj_t) + snippet->getSingleDynamicSlotOffset())
      {
      isSingleDynamicSlot = 1;
      }

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, "Interface Call Data Snippet");

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Call Site RA", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Address Of Constant Pool", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# CP Index", *((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Interface Class",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
   trfprintf(pOutFile, "DC   \t%p \t\t# Method Index",*((intptrj_t*)bufferPos));
   bufferPos += sizeof(intptrj_t);

   if (snippet->getNumInterfaceCallCacheSlots() == 0)
      {
   printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
      trfprintf(pOutFile, "DC   \t%p \t\t# Resolved Flag",*((intptrj_t*)bufferPos));
      }
   else
      {
      if (!isSingleDynamicSlot)
         {
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "DC   \t%p \t\t# Resolved Flag",*((intptrj_t*)bufferPos));
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "DC   \t%p \t\t# lastCachedSlotField", *((intptrj_t*)bufferPos));
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "DC   \t%p \t\t# firstSlotField",*((intptrj_t*)bufferPos));
         bufferPos += sizeof(intptrj_t);

         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "DC   \t%p \t\t# lastSlotField",*((intptrj_t*)bufferPos));
         bufferPos += sizeof(intptrj_t);
         }

      for (i=0;i<snippet->getNumInterfaceCallCacheSlots();i++)
         {
         if (snippet->isUseCLFIandBRCL())
            {
            printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
            trfprintf(pOutFile, "DC   \t%p \t\t# Addr of CLFI immed field for cached class %d",*((intptrj_t*)bufferPos),i+1);
            bufferPos += sizeof(intptrj_t);
            }
         else
            {
            printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
            trfprintf(pOutFile, "DC   \t%p \t\t# cached class%d",*((intptrj_t*)bufferPos),i+1);
            bufferPos += sizeof(intptrj_t);

            printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
            trfprintf(pOutFile, "DC   \t%p \t\t# cached method%d",*((intptrj_t*)bufferPos),i+1);
            bufferPos += sizeof(intptrj_t);
            }
         }
      if (isSingleDynamicSlot)
         {
         printPrefix(pOutFile, NULL, bufferPos, sizeof(intptrj_t));
         trfprintf(pOutFile, "DC   \t%p \t\t# Resolved Flag",*((intptrj_t*)bufferPos));
         bufferPos += sizeof(intptrj_t);
         }
      }
   }


TR::S390LabelTableSnippet::S390LabelTableSnippet(TR::CodeGenerator *cg, TR::Node *node, uint32_t size)
   : TR::S390ConstantDataSnippet(cg, node, NULL, 0), _size(size)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), "only 31bit mode supported for TR::S390LabelTableSnippet\n");
   _labelTable = (TR::LabelSymbol **) cg->trMemory()->allocateMemory(sizeof(TR::LabelSymbol *) * size, heapAlloc);
   setLength(0);
   }

uint8_t *
TR::S390LabelTableSnippet::emitSnippetBody()
   {
   uint8_t * snip = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(snip);
   uint8_t * cursor = snip;
   for (int32_t i = 0; i < _size; i++)
      {
      if (_labelTable[i])
         {
         cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelTable32BitRelocation(cursor, _labelTable[i]));
         *(uint32_t *) cursor = cursor - snip;
         }
      else
         *(uint32_t *) cursor = 0xdeadbeaf;
      cursor += 4;
      }
   return cursor;
   }

uint32_t
TR::S390LabelTableSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return _size * 4;
   }

TR::S390DeclTrampSnippet::S390DeclTrampSnippet(TR::CodeGenerator *cg, TR::LabelSymbol *label)
   : TR::S390ConstantDataSnippet(cg, NULL, NULL, 0), _label(label)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), "only 31bit mode supported for TR::S390DeclTrampSnippet\n");
   setLength(16);
   }

uint8_t *
TR::S390DeclTrampSnippet::emitSnippetBody()
   {
   uint8_t * snip = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(snip);
   uint8_t * cursor = snip;

   TR_ASSERT(0, "BinOpt only from folding of getBinOptGPRSlotOffset");

   // Assume GPR10 on entry to declarative trampoline is that expected by method
   // ST   R13,r13Slot(R10)   # 4 bytes
   // BRCL 15,declarativeBB # 6 bytes
   *(uint32_t *) cursor = 0x50d0a000;
   cursor += 4;
   *(uint16_t *) cursor = 0xc0f4;
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, _label));
   cursor += 6;
   *(uint32_t *) cursor = 0xdead;
   cursor += 2;
   return cursor;
   }

TR::S390SortJumpTrampSnippet::S390SortJumpTrampSnippet(TR::CodeGenerator *cg, TR::LabelSymbol *label)
   : TR::S390ConstantDataSnippet(cg, NULL, NULL, 0), _label(label)
   {
   TR_ASSERT(TR::Compiler->target.is32Bit(), "only 31bit mode supported for TR::S390SortJumpTrampSnippet\n");
   setLength(16);
   }

uint8_t *
TR::S390SortJumpTrampSnippet::emitSnippetBody()
   {
   uint8_t * snip = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(snip);
   uint8_t * cursor = snip;

   TR_ASSERT(0, "BinOpt only from folding of getBinOptGPRSlotOffset");

   // Assume GPR10 on entry to sort jump trampoline is that expected by method
   // ST R13,r13Slot(R10)   # 4 bytes
   // BRCL 15,declarativeBB # 6 bytes
   *(uint32_t *) cursor = 0x50d0a000;
   cursor += 4;
   *(uint16_t *) cursor = 0xc0f4;
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, _label));
   cursor += 6;
   *(uint32_t *) cursor = 0xdead;
   cursor += 2;
   return cursor;
   }
