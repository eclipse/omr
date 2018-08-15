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

#include "il/symbol/OMRResolvedMethodSymbol.hpp"

#include <stdint.h>                             // for int32_t, etc
#include <stdlib.h>                             // for strtol, atoi
#include <string.h>                             // for NULL, strncmp, etc
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd, etc
#include "codegen/Linkage.hpp"                  // for Linkage
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"              // for Compilation
#include "compile/Method.hpp"                   // for TR_Method, etc
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"                         // for Block
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"                     // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                         // for ILOpCode
#include "il/Node.hpp"                          // for Node, etc
#include "il/Node_inlines.hpp"                  // for Node::getChild, etc
#include "il/Symbol.hpp"                        // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                       // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"           // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "ilgen/IlGen.hpp"                      // for TR_IlGenerator
#include "ilgen/IlGenRequest.hpp"               // for IlGenRequest
#include "infra/Array.hpp"                      // for TR_Array
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/BitVector.hpp"                  // for TR_BitVector, etc
#include "infra/Cfg.hpp"                        // for CFG
#include "infra/Flags.hpp"                      // for flags32_t
#include "infra/List.hpp"                       // for List, etc
#include "infra/Random.hpp"
#include "infra/CfgEdge.hpp"                    // for CFGEdge
#include "infra/CfgNode.hpp"                    // for CFGNode
#include "optimizer/Optimizer.hpp"              // for Optimizer
#include "ras/Debug.hpp"                        // for TR_Debug
#include "runtime/Runtime.hpp"
#include "infra/SimpleRegex.hpp"


TR::ResolvedMethodSymbol *
OMR::ResolvedMethodSymbol::self()
   {
   return static_cast<TR::ResolvedMethodSymbol *>(this);
   }

template <typename AllocatorType>
TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::create(AllocatorType m, TR_ResolvedMethod * rm, TR::Compilation * comp)
   {
   return new (m) TR::ResolvedMethodSymbol(rm,comp);
   }


static
TR::TreeTop *findEndTreeTop(TR::ResolvedMethodSymbol *rms)
   {
   TR::TreeTop *end = NULL;
   for (TR::TreeTop *tt = rms->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      tt = tt->getNode()->getBlock()->getExit();
      end = tt;
      }
   return end;
   }

void
OMR::ResolvedMethodSymbol::initForCompilation(TR::Compilation *comp)
   {
   TR::Region &heapRegion = comp->trMemory()->heapMemoryRegion();
   self()->getParameterList().setRegion(heapRegion);
   self()->getAutomaticList().setRegion(heapRegion);

   self()->getVariableSizeSymbolList().setRegion(heapRegion);
   self()->getTrivialDeadTreeBlocksList().setRegion(heapRegion);
   }


OMR::ResolvedMethodSymbol::ResolvedMethodSymbol(TR_ResolvedMethod * method, TR::Compilation * comp)
   : TR::MethodSymbol(TR_Private, method->convertToMethod()),
     _resolvedMethod(method),
     _flowGraph(0),
     _unimplementedOpcode(0),
     _firstTreeTop(0),
     _autoSymRefs(0),
     _pendingPushSymRefs(0),
     _parmSymRefs(0),
     _osrPoints(comp->trMemory()),
     _tempIndex(-1),
     _arrayCopyTempSlot(-1),
     _syncObjectTemp(0),
     _thisTempForObjectCtor(0),
     _atcDeferredCountTemp(0),
     _automaticList(comp->trMemory()),
     _parameterList(comp->trMemory()),
     _variableSizeSymbolList(comp->trMemory()),
     _methodMetaDataList(comp->trMemory()),
     _trivialDeadTreeBlocksList(comp->trMemory()),
     _comp(comp),
     _firstJitTempIndex(-1),
     _cannotAttemptOSR(NULL),
     _pythonConstsSymRef(NULL),
     _pythonNumLocalVars(0),
     _pythonLocalVarSymRefs(NULL),
     _properties(0),
     _bytecodeProfilingOffsets(comp->allocator())
   {
   _flags.setValue(KindMask, IsResolvedMethod);

   if (comp->isGPUCompileCPUCode())
      {
      self()->setLinkage(TR_System);
      }

    _methodIndex = comp->addOwningMethod(self());
   if (comp->getOption(TR_TraceMethodIndex))
      traceMsg(comp, "-- New symbol for method: M%p index: %d owningMethod: M%p sig: %s\n",
         method, (int)_methodIndex.value(), method->owningMethod(), self()->signature(comp->trMemory()));

   if (_methodIndex >= MAX_CALLER_INDEX)
      {
      comp->failCompilation<TR::MaxCallerIndexExceeded>("Exceeded MAX_CALLER_INDEX");
      }

   if (_resolvedMethod->isSynchronized())
      self()->setSynchronised();

   // Set the interpreted flag for an interpreted method unless we're calling
   // the method that's being jitted
   //
   if ((_methodIndex > JITTED_METHOD_INDEX && !_resolvedMethod->isSameMethod(comp->getJittedMethodSymbol()->getResolvedMethod()))
       || comp->isDLT()
       || (comp->getOption(TR_UseSymbolValidationManager) && comp->compileRelocatableCode()))
      {
      if (_resolvedMethod->isInterpreted())
         {
         self()->setInterpreted();
         self()->setMethodAddress(_resolvedMethod->resolvedMethodAddress());
         }
      else
         self()->setMethodAddress(_resolvedMethod->startAddressForJittedMethod());
      }

#ifdef J9_PROJECT_SPECIFIC
   if ((TR::Compiler->target.cpu.getSupportsHardwareRound() &&
        ((_resolvedMethod->getRecognizedMethod() == TR::java_lang_Math_floor) ||
         (_resolvedMethod->getRecognizedMethod() == TR::java_lang_StrictMath_floor) ||
         (_resolvedMethod->getRecognizedMethod() == TR::java_lang_Math_ceil) ||
         (_resolvedMethod->getRecognizedMethod() == TR::java_lang_StrictMath_ceil))) ||
       (TR::Compiler->target.cpu.getSupportsHardwareSQRT() &&
        ((_resolvedMethod->getRecognizedMethod() == TR::java_lang_Math_sqrt) ||
         (_resolvedMethod->getRecognizedMethod() == TR::java_lang_StrictMath_sqrt))) ||
       (TR::Compiler->target.cpu.getSupportsHardwareCopySign() &&
        ((_resolvedMethod->getRecognizedMethod() == TR::java_lang_Math_copySign_F) ||
         (_resolvedMethod->getRecognizedMethod() == TR::java_lang_Math_copySign_D))))
      {
      self()->setCanReplaceWithHWInstr(true);
      }

   if (_resolvedMethod->isJNINative())
      {
      self()->setJNI();
#if defined(TR_TARGET_POWER)
      switch(_resolvedMethod->getRecognizedMethod())
         {
         case TR::java_lang_Math_acos:
         case TR::java_lang_Math_asin:
         case TR::java_lang_Math_atan:
         case TR::java_lang_Math_atan2:
         case TR::java_lang_Math_cbrt:
         case TR::java_lang_Math_ceil:
         case TR::java_lang_Math_copySign_F:
         case TR::java_lang_Math_copySign_D:
         case TR::java_lang_Math_cos:
         case TR::java_lang_Math_cosh:
         case TR::java_lang_Math_exp:
         case TR::java_lang_Math_expm1:
         case TR::java_lang_Math_floor:
         case TR::java_lang_Math_hypot:
         case TR::java_lang_Math_IEEEremainder:
         case TR::java_lang_Math_log:
         case TR::java_lang_Math_log10:
         case TR::java_lang_Math_log1p:
         case TR::java_lang_Math_max_F:
         case TR::java_lang_Math_max_D:
         case TR::java_lang_Math_min_F:
         case TR::java_lang_Math_min_D:
         case TR::java_lang_Math_nextAfter_F:
         case TR::java_lang_Math_nextAfter_D:
         case TR::java_lang_Math_pow:
         case TR::java_lang_Math_rint:
         case TR::java_lang_Math_round_F:
         case TR::java_lang_Math_round_D:
         case TR::java_lang_Math_scalb_F:
         case TR::java_lang_Math_scalb_D:
         case TR::java_lang_Math_sin:
         case TR::java_lang_Math_sinh:
         case TR::java_lang_Math_sqrt:
         case TR::java_lang_Math_tan:
         case TR::java_lang_Math_tanh:
         case TR::java_lang_StrictMath_acos:
         case TR::java_lang_StrictMath_asin:
         case TR::java_lang_StrictMath_atan:
         case TR::java_lang_StrictMath_atan2:
         case TR::java_lang_StrictMath_cbrt:
         case TR::java_lang_StrictMath_ceil:
         case TR::java_lang_StrictMath_copySign_F:
         case TR::java_lang_StrictMath_copySign_D:
         case TR::java_lang_StrictMath_cos:
         case TR::java_lang_StrictMath_cosh:
         case TR::java_lang_StrictMath_exp:
         case TR::java_lang_StrictMath_expm1:
         case TR::java_lang_StrictMath_floor:
         case TR::java_lang_StrictMath_hypot:
         case TR::java_lang_StrictMath_IEEEremainder:
         case TR::java_lang_StrictMath_log:
         case TR::java_lang_StrictMath_log10:
         case TR::java_lang_StrictMath_log1p:
         case TR::java_lang_StrictMath_max_F:
         case TR::java_lang_StrictMath_max_D:
         case TR::java_lang_StrictMath_min_F:
         case TR::java_lang_StrictMath_min_D:
         case TR::java_lang_StrictMath_nextAfter_F:
         case TR::java_lang_StrictMath_nextAfter_D:
         case TR::java_lang_StrictMath_pow:
         case TR::java_lang_StrictMath_rint:
         case TR::java_lang_StrictMath_round_F:
         case TR::java_lang_StrictMath_round_D:
         case TR::java_lang_StrictMath_scalb_F:
         case TR::java_lang_StrictMath_scalb_D:
         case TR::java_lang_StrictMath_sin:
         case TR::java_lang_StrictMath_sinh:
         case TR::java_lang_StrictMath_sqrt:
         case TR::java_lang_StrictMath_tan:
         case TR::java_lang_StrictMath_tanh:
            setCanDirectNativeCall(true);
            break;
         default:
            break;
         }
#endif // TR_TARGET_POWER
      }
   else
#endif // J9_PROJECT_SPECIFIC
      if (_resolvedMethod->isNative())
      {
      if (!self()->isInterpreted() && _resolvedMethod->isJITInternalNative())
         {
         self()->setMethodAddress(_resolvedMethod->startAddressForJITInternalNativeMethod());
         self()->setJITInternalNative();
         }
      else
         {
         self()->setVMInternalNative();
         }
      }

   if (_resolvedMethod->isFinal())
      self()->setFinal();

   if (_resolvedMethod->isStatic())
      self()->setStatic();

   self()->setParameterList();

   _properties.set(CanSkipNullChecks                   , self()->safeToSkipNullChecks());
   _properties.set(CanSkipBoundChecks                  , self()->safeToSkipBoundChecks());
   _properties.set(CanSkipCheckCasts                   , self()->safeToSkipCheckCasts());
   _properties.set(CanSkipDivChecks                    , self()->safeToSkipDivChecks());
   _properties.set(CanSkipArrayStoreChecks             , self()->safeToSkipArrayStoreChecks());
   _properties.set(CanSkipChecksOnArrayCopies          , self()->safeToSkipChecksOnArrayCopies());
   _properties.set(CanSkipZeroInitializationOnNewarrays, self()->safeToSkipZeroInitializationOnNewarrays());
   }


int32_t
OMR::ResolvedMethodSymbol::getSyncObjectTempIndex()
   {
   int32_t delta = 0;
   if (self()->comp()->getOption(TR_MimicInterpreterFrameShape))
      {
      delta++;
      }

   return self()->getFirstJitTempIndex() - delta;
   }

int32_t
OMR::ResolvedMethodSymbol::getThisTempForObjectCtorIndex()
   {
   int32_t delta = 0;
   if (self()->comp()->getOption(TR_MimicInterpreterFrameShape))
      {
      delta++;
      }

   return self()->getFirstJitTempIndex() - delta;
   }

List<TR::ParameterSymbol>&
OMR::ResolvedMethodSymbol::getLogicalParameterList(TR::Compilation *comp)
   {
      return self()->getParameterList();
   }

template <typename AllocatorType>
TR::ResolvedMethodSymbol *
OMR::ResolvedMethodSymbol::createJittedMethodSymbol(AllocatorType m, TR_ResolvedMethod * method, TR::Compilation * comp)
   {
   auto sym = new (m) TR::ResolvedMethodSymbol(method, comp);
   sym->_localMappingCursor   = 0;
   sym->_prologuePushSlots    = 0;
   sym->_scalarTempSlots      = 0;
   sym->_objectTempSlots      = 0;
   sym->_flags.set(IsJittedMethod);
   return sym;
   }


void
bcIndexForFakeInduce(TR::Compilation* comp, int16_t* callSiteInsertionPoint,
                          int16_t* bcIndexInsertionPoint, char* childPath)
   {
   static char * fakeInduceOSR = feGetEnv("TR_fakeInduceOSR");
   char* induceOSR = comp->getOptions()->getInduceOSR();
   if (induceOSR == NULL)
      induceOSR = fakeInduceOSR;

   TR::TreeTop *lastTreeTop;
   char signatureRegex[500];
   const char * mSignature = comp->signature();
   char* p = induceOSR;
   if (callSiteInsertionPoint)
      *callSiteInsertionPoint = (int16_t)-2;
   if (bcIndexInsertionPoint)
      *bcIndexInsertionPoint = (int16_t)-1;
   while (p)
      {
      char* temp;
      temp = strchr(p, ':');
      //p = strstr(p, mSignature);
      if (!p) break;
      strncpy(signatureRegex, p, temp - p);
      signatureRegex[temp-p] = '\0';
      char* tempRegex = signatureRegex;
      TR::SimpleRegex* regex = TR::SimpleRegex::create(tempRegex);
      if (regex)
         {
         if (!TR::SimpleRegex::match(regex, mSignature, true))
            {
            traceMsg(comp, "regex not matching\n");
            break;
            }
         }
      else if (strcmp(signatureRegex, mSignature))
         {
         traceMsg(comp, "signature not matching\n");
         break;
         }
      p = temp+1;
      int16_t tempInt = strtol(p, &temp, 10);
      p = temp;
      if (callSiteInsertionPoint)
         *callSiteInsertionPoint = tempInt;
      p++; //skipping the second ,

      tempInt = strtol(p, &temp, 16);
      p = temp;
      if (bcIndexInsertionPoint)
         *bcIndexInsertionPoint = tempInt;
      p++; //skipping the third ,

      char* q = childPath;
      while (*p != '\0' && *p != '|')
         {
         *q = *p;
         p++; q++;
         }
      *q = '\0';

      traceMsg(comp, "signature: %s, callSiteInsertionPoint: %d, bcIndexInsertionPoint: %x\n",
              mSignature,
              (callSiteInsertionPoint?*callSiteInsertionPoint:-1),
              (bcIndexInsertionPoint?*bcIndexInsertionPoint:-1));
      break;
      }
   }

bool
OMR::ResolvedMethodSymbol::canInjectInduceOSR(TR::Node* node)
   {
   bool trace = self()->comp()->getOption(TR_TraceOSR);
   if (node->getOpCodeValue() != TR::treetop
       && node->getOpCodeValue() != TR::NULLCHK
       && node->getOpCodeValue() != TR::ResolveAndNULLCHK)
      {
      if (trace)
         traceMsg(self()->comp(), "node doesn't have a treetop, NULLCHK, or ResolveAndNULLCHK root\n");
      return false;
      }
   if (node->getNumChildren() != 1
       || !node->getChild(0)->getOpCode().isCall())
      {
      if (trace)
         traceMsg(self()->comp(), "there is no call under the treetop\n");
      return false;
      }
   TR::Node* callNode = node->getChild(0);
   if (callNode->getReferenceCount() != 1 && node->getOpCodeValue() == TR::treetop)
      {
      if (trace)
         traceMsg(self()->comp(), "call node has a refcount larger than 1 and is under a treetop\n");
      return false;
      }
   const char* rootSignature = self()->comp()->signature();
   if (!strncmp(rootSignature, "java/lang/Object.newInstancePrototype", 37))
      {
      if (trace)
         traceMsg(self()->comp(), "root method is a java/lang/Object.newInstancePrototype method\n");
      return false;
      }
   if (!strncmp(rootSignature, "java/lang/Class.newInstancePrototype", 36))
      {
      if (trace)
         traceMsg(self()->comp(), "root method is a java/lang/Class.newInstancePrototype method\n");
      return false;
      }
   const char* currentSignature = self()->signature(self()->comp()->trMemory());
   if (!strncmp(currentSignature, "com/ibm/jit/JITHelpers", 22))
      {
      if (trace)
         traceMsg(self()->comp(), "node is a com/ibm/jit/jit helper method\n");
      return false;
      }

   TR::Symbol * sym = callNode->getSymbolReference()->getSymbol();
   if (sym->isMethod())
      {
      TR::MethodSymbol* methSym = sym->castToMethodSymbol();
      if (methSym->getMethodKind() == TR::MethodSymbol::Helper ||
          methSym->isNative() ||
          methSym->getMethodKind() == TR::MethodSymbol::Special)
         {
         if (trace)
            traceMsg(self()->comp(), "node is a helper, native, or a special call\n");
         return false;
         }
      }
   if (sym->isResolvedMethod())
      {
      auto * methSym = sym->castToResolvedMethodSymbol();
      const char* signature = methSym->signature(self()->comp()->trMemory());
      if (!strncmp(signature, "com/ibm/jit/JITHelpers", 22))
         {
         if (trace)
            traceMsg(self()->comp(), "node is a com/ibm/jit/jit helper method\n");
         return false;
         }
      }
   return true;
   }

/**
 * Generate and prepend an induce OSR call tree.
 *
 * \param insertionPoint Tree to prepend the OSR call to. It may also copy children from this tree if it is a call.
 * \param numChildren The number of children to copy from the insertionPoint to the induce call.
 * \param copyChildren Flag controlling whether to copy children to the induce call.
 * \param shouldSplitBlock Flag controlling whether to split the block after the induce call.
 * \param cfg Specify the CFG to use when splitting the block. Defaults to the outermost CFG.
 */
TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCallNode(TR::TreeTop* insertionPoint,
                                              int32_t numChildren,
                                              bool copyChildren,
                                              bool shouldSplitBlock,
                                              TR::CFG *cfg)
   {
   // If not specified use the outermost CFG
   if (!cfg)
      cfg = self()->comp()->getFlowGraph();

   //Now we need to put the induceOSR call as the first treetop of its block.
   //Because if we don't do that and if there is a method call before induceOSR in induceOSR's block
   //and that method gets inlined
   //there is no guarantee that the future block containing the induceOSR call still will have an exception
   //edge to the OSR catch block.
   TR::SymbolReferenceTable* symRefTab = self()->comp()->getSymRefTab();
   TR::SymbolReference *induceOSRSymRef = symRefTab->findOrCreateRuntimeHelper(TR_induceOSRAtCurrentPC, true, true, true);
   // treat jitInduceOSR like an interpreted call so that each platform's codegen generate a snippet for it
   induceOSRSymRef->getSymbol()->getMethodSymbol()->setInterpreted();
   TR::Node *refNode = insertionPoint->getNode();

   if (self()->comp()->getOption(TR_TraceOSR))
     traceMsg(self()->comp(), "O^O OSR: Inject induceOSR call for [%p] at %3d:%d\n", refNode, refNode->getInlinedSiteIndex(), refNode->getByteCodeIndex());

   TR::Block * firstHalfBlock = insertionPoint->getEnclosingBlock();
   if (shouldSplitBlock)
       firstHalfBlock->split(insertionPoint, cfg, true);
   firstHalfBlock->setIsOSRInduceBlock();

   // The arguments for a transition to this bytecode index may have already been stashed.
   // If they have, use these directly rather than copying the reference node.
   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(
      refNode->getByteCodeInfo().getCallerIndex(), self());
   TR_Array<int32_t> *stashedArgs = osrMethodData->getArgInfo(refNode->getByteCodeIndex());
   int32_t firstArgIndex = 0;
   if (stashedArgs)
      numChildren = stashedArgs->size();
   else if ((refNode->getNumChildren() > 0) &&
       refNode->getFirstChild()->getOpCode().isCall())
      {
      refNode = refNode->getFirstChild();
      if (numChildren < 0)
         {
         numChildren   = refNode->getNumChildren();
         firstArgIndex = refNode->getFirstArgumentIndex();
         }
      }

   TR::Node *induceOSRCallNode = TR::Node::createWithSymRef(refNode, TR::call, numChildren - firstArgIndex, induceOSRSymRef);
   TR_OSRPoint *point = self()->findOSRPoint(refNode->getByteCodeInfo());
   TR_ASSERT(point, "Could not find osr point for bytecode %d:%d!", refNode->getByteCodeInfo().getCallerIndex(), refNode->getByteCodeInfo().getByteCodeIndex());

   // If arguments have been stashed against this BCI, copy them now, ignoring the copyChildren flag
   if (stashedArgs)
      {
      for (int32_t i = 0; i < numChildren; ++i)
         induceOSRCallNode->setAndIncChild(i,
            TR::Node::createLoad(induceOSRCallNode, self()->comp()->getSymRefTab()->getSymRef((*stashedArgs)[i])));
      }
   else if (copyChildren)
      {
      for (int i = firstArgIndex; i < numChildren; i++)
         induceOSRCallNode->setAndIncChild(i-firstArgIndex, refNode->getChild(i));
      }
   else
      {
      induceOSRCallNode->setNumChildren(0);
      }

   if (self()->comp()->getOptions()->getVerboseOption(TR_VerboseOSRDetails))
      TR_VerboseLog::writeLineLocked(TR_Vlog_OSRD, "Injected induceOSR call at %3d:%d in %s", refNode->getInlinedSiteIndex(), refNode->getByteCodeIndex(), self()->comp()->signature());

   TR::Node *induceOSRTreeTopNode = TR::Node::create(TR::treetop, 1, induceOSRCallNode);
   insertionPoint->insertBefore(TR::TreeTop::create(self()->comp(), induceOSRTreeTopNode));
   return insertionPoint->getPrevTreeTop();
   }


bool
OMR::ResolvedMethodSymbol::induceOSRAfter(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, TR::TreeTop* branch,
    bool extendRemainder, int32_t offset, TR::TreeTop ** lastTreeTop)
   {
   TR::Block *block = insertionPoint->getEnclosingBlock();

   if (self()->supportsInduceOSR(induceBCI, block, self()->comp()))
      {
      TR::CFG *cfg = self()->comp()->getFlowGraph();
      cfg->setStructure(NULL);
      TR::TreeTop *remainderTree = insertionPoint->getNextTreeTop();
      if (remainderTree->getNode()->getOpCodeValue() != TR::BBEnd)
         {
         if (extendRemainder)
            {
            TR::Block *remainderBlock = block->split(remainderTree, cfg, false, true);
            remainderBlock->setIsExtensionOfPreviousBlock(true);
            if (self()->comp()->getOption(TR_TraceOSR))
               traceMsg(self()->comp(), "  Split of block_%d at n%dn produced block_%d which is an extension\n", block->getNumber(), remainderTree->getNode()->getGlobalIndex(), remainderBlock->getNumber());
            }
         else
            {
            TR::Block *remainderBlock = block->split(remainderTree, cfg, true, true);
            if (self()->comp()->getOption(TR_TraceOSR))
               traceMsg(self()->comp(), "  Split of block_%d at n%dn produced block_%d\n", block->getNumber(), remainderTree->getNode()->getGlobalIndex(), remainderBlock->getNumber());
            }
         }

      induceBCI.setByteCodeIndex(induceBCI.getByteCodeIndex() + offset);

      // create a block that will be the target of the branch with BCI for the induceOSR
      TR::Block *osrBlock = TR::Block::createEmptyBlock(self()->comp(), MAX_COLD_BLOCK_COUNT);
      osrBlock->setIsCold();
      osrBlock->getEntry()->getNode()->setByteCodeInfo(induceBCI);
      osrBlock->getExit()->getNode()->setByteCodeInfo(induceBCI);

      // Load the cached last treetop if available, and store it when finished
      TR::TreeTop *end = lastTreeTop && (*lastTreeTop) ? (*lastTreeTop) : cfg->findLastTreeTop();
      TR_ASSERT(end->getNextTreeTop() == NULL, "The last treetop must not have a treetop after it");
      end->join(osrBlock->getEntry());
      if (lastTreeTop)
         (*lastTreeTop) = osrBlock->getExit();

      cfg->addNode(osrBlock);
      cfg->addEdge(block, osrBlock);

      if (self()->comp()->getOption(TR_TraceOSR))
         traceMsg(self()->comp(), "  Created OSR block_%d and inserting it at the end of the method\n", osrBlock->getNumber());

      branch->getNode()->setBranchDestination(osrBlock->getEntry());
      block->append(branch);
      cfg->copyExceptionSuccessors(block, osrBlock);

      // induce OSR in the new block
      self()->genInduceOSRCallAndCleanUpFollowingTreesImmediately(osrBlock->getExit(), induceBCI, false, self()->comp());
      return true;
      }
   return false;
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::induceImmediateOSRWithoutChecksBefore(TR::TreeTop *insertionPoint)
   {
   if (self()->supportsInduceOSR(insertionPoint->getNode()->getByteCodeInfo(), insertionPoint->getEnclosingBlock(), self()->comp()))
      return self()->genInduceOSRCallAndCleanUpFollowingTreesImmediately(insertionPoint, insertionPoint->getNode()->getByteCodeInfo(), false, self()->comp());
   if (self()->comp()->getOption(TR_TraceOSR))
      traceMsg(self()->comp(), "induceImmediateOSRWithoutChecksBefore n%dn failed - supportsInduceOSR returned false\n", insertionPoint->getNode()->getGlobalIndex());
   return NULL;
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCallAndCleanUpFollowingTreesImmediately(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, bool shouldSplitBlock, TR::Compilation *comp)
   {
   TR::TreeTop *induceOSRCallTree = self()->genInduceOSRCall(insertionPoint, induceBCI.getCallerIndex(), 0, false, shouldSplitBlock);

   if (induceOSRCallTree)
      {
      TR::TreeTop *treeAfterInduceOSRCall = induceOSRCallTree->getNextTreeTop();

      while (treeAfterInduceOSRCall)
         {
         if (((treeAfterInduceOSRCall->getNode()->getOpCodeValue() == TR::athrow) &&
             treeAfterInduceOSRCall->getNode()->throwInsertedByOSR()) ||
             (treeAfterInduceOSRCall->getNode()->getOpCodeValue() == TR::BBEnd))
            break;
         TR::TreeTop *next = treeAfterInduceOSRCall->getNextTreeTop();
         induceOSRCallTree->join(next);
         treeAfterInduceOSRCall->getNode()->recursivelyDecReferenceCount();
         treeAfterInduceOSRCall = next;
         }
      }

   return induceOSRCallTree;
   }

/**
 * Generate and prepend an induce OSR call. This function will also modify the block to fit OSR requirements, such as
 * limiting successors to only the exit block and limiting exception successors to the matching OSR catch block.
 *
 * \param insertionPoint Tree to prepend the OSR call to. It may also copy children from this tree if it is a call.
 * \param inlinedSiteIndex The inlined site index of the desired transition target. This should match the insertion points inlined site index.
 * \param numChildren The number of children to copy from the insertionPoint to the induce OSR call.
 * \param copyChildren Flag controlling whether to copy children to the induce OSR call.
 * \param shouldSplitBlock Flag controlling whether to split the block after the induce OSR call.
 * \param cfg CFG used for the block modifications. This will default to the outermost CFG, but can be specified if adding induce OSR calls
 *            when it is not fully formed, such as during inlining.
 */
TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCall(TR::TreeTop* insertionPoint,
                                          int32_t inlinedSiteIndex,
                                          int32_t numChildren,
                                          bool copyChildren,
                                          bool shouldSplitBlock,
                                          TR::CFG *cfg)
   {
   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, self());
   return self()->genInduceOSRCall(insertionPoint, inlinedSiteIndex, osrMethodData, numChildren, copyChildren, shouldSplitBlock, cfg);
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCall(TR::TreeTop* insertionPoint,
                                          int32_t inlinedSiteIndex,
                                          TR_OSRMethodData *osrMethodData,
                                          int32_t numChildren,
                                          bool copyChildren,
                                          bool shouldSplitBlock,
                                          TR::CFG *callerCFG)
   {
   // If not specified use the outermost CFG
   if (!callerCFG)
      callerCFG = self()->comp()->getFlowGraph();

   TR::Node *insertionPointNode = insertionPoint->getNode();
   if (self()->comp()->getOption(TR_TraceOSR))
      traceMsg(self()->comp(), "Osr point added for %p, callerIndex=%d, bcindex=%d\n",
              insertionPointNode, insertionPointNode->getByteCodeInfo().getCallerIndex(),
              insertionPointNode->getByteCodeInfo().getByteCodeIndex());

   TR::Block * OSRCatchBlock = osrMethodData->getOSRCatchBlock();
   TR::TreeTop *induceOSRCallTree = self()->genInduceOSRCallNode(insertionPoint, numChildren, copyChildren, shouldSplitBlock, callerCFG);

   TR::Block *enclosingBlock = insertionPoint->getEnclosingBlock();
   if (!enclosingBlock->getLastRealTreeTop()->getNode()->getOpCode().isReturn())
      {
      callerCFG->addEdge(enclosingBlock, callerCFG->getEnd());

      for (auto nextEdge = enclosingBlock->getSuccessors().begin(); nextEdge != enclosingBlock->getSuccessors().end();)
         {
         if ((*nextEdge)->getTo() != callerCFG->getEnd())
            callerCFG->removeEdge(*(nextEdge++));
         else
            ++nextEdge;
         }
      }

   for (auto nextEdge = enclosingBlock->getExceptionSuccessors().begin(); nextEdge != enclosingBlock->getExceptionSuccessors().end();)
      {
      TR::CFGNode *succ = (*nextEdge)->getTo();
      if (succ != OSRCatchBlock)
         callerCFG->removeEdge(*(nextEdge++));
      else
         ++nextEdge;
      }

   TR::SymbolReference * tempSymRef = 0;
   TR::Node * loadExcpSymbol = TR::Node::createWithSymRef(insertionPointNode, TR::aload, 0, self()->comp()->getSymRefTab()->findOrCreateExcpSymbolRef());

   TR::TreeTop *lastTreeInEnclosingBlock = enclosingBlock->getLastRealTreeTop();
   if (lastTreeInEnclosingBlock != enclosingBlock->getLastNonControlFlowTreeTop())
      {
      TR::TreeTop *prev = lastTreeInEnclosingBlock->getPrevTreeTop();
      TR::TreeTop *next = lastTreeInEnclosingBlock->getNextTreeTop();
      prev->join(next);
      lastTreeInEnclosingBlock->getNode()->recursivelyDecReferenceCount();
      }

   enclosingBlock->append(TR::TreeTop::create(self()->comp(), TR::Node::createWithSymRef(TR::athrow, 1, 1, loadExcpSymbol, self()->comp()->getSymRefTab()->findOrCreateAThrowSymbolRef(self()))));
   enclosingBlock->getLastRealTreeTop()->getNode()->setThrowInsertedByOSR(true);

   bool firstOSRPoint = false;
   if (self()->getOSRPoints().isEmpty())
      firstOSRPoint = true;

   // TODO: The late addition of OSR infrastructure based on the lack of OSR points should not be possible, as we shouldn't have been able to induce without
   // an OSR point in the first place. This should be removed, as well as the genOSRHelperCall's cfg argument.
   if (firstOSRPoint)
      {
      TR::Block *OSRCodeBlock = osrMethodData->getOSRCodeBlock();
      TR::Block *OSRCatchBlock = osrMethodData->getOSRCatchBlock();

      if (self()->comp()->getOption(TR_TraceOSR))
         traceMsg(self()->comp(), "code %p %d catch %p %d\n", OSRCodeBlock, OSRCodeBlock->getNumber(), OSRCatchBlock, OSRCatchBlock->getNumber());

      self()->getLastTreeTop()->insertTreeTopsAfterMe(OSRCatchBlock->getEntry(), OSRCodeBlock->getExit());
      self()->genOSRHelperCall(inlinedSiteIndex, self()->comp()->getSymRefTab(), callerCFG);
      }

   self()->insertRematableStoresFromCallSites(self()->comp(), inlinedSiteIndex, induceOSRCallTree);
   self()->insertStoresForDeadStackSlotsBeforeInducingOSR(self()->comp(), inlinedSiteIndex, insertionPoint->getNode()->getByteCodeInfo(), induceOSRCallTree);

   if (self()->comp()->getOption(TR_TraceOSR))
      traceMsg(self()->comp(), "last real tree n%dn\n", enclosingBlock->getLastRealTreeTop()->getNode()->getGlobalIndex());
   return induceOSRCallTree;
   }





/**
 * return values:
 * 0 means there is no match
 * 1 means there is a match and inserting induceOSR is legal at that point
 * 2 means there is a match and inserting induceOSR may not be legal at that point
 */
int
OMR::ResolvedMethodSymbol::matchInduceOSRCall(TR::TreeTop* insertionPoint,
                                            int16_t callerIndex,
                                            int16_t byteCodeIndex,
                                            const char* childPath)
   {
   TR::Node *refNode = insertionPoint->getNode();
   static char* recipProbString = feGetEnv("TR_recipProb");
   int recipProb = 10;
   if (recipProbString)
      recipProb = atoi(recipProbString);

   TR::Node *fakeInduceOSRCallNode, *refCallNode = NULL;
   if (childPath[0] == 'b' || childPath[0] == 'a')
      {
      //callerIndex/byteCodeIndex value -3 means a wildcard *, i.e., do it for all caller/bytecode indices
      if ((callerIndex != -3 && refNode->getInlinedSiteIndex() != callerIndex) ||
          (byteCodeIndex != -3 && refNode->getByteCodeIndex() != byteCodeIndex))
         return 0;
      if (self()->canInjectInduceOSR(refNode))
         return 1;
      else
         {
         if (childPath[0] == 'b')
            return 0;
         else
            return 2;
         }
      }
   else if (childPath[0] == 'r')
      {
      if (callerIndex == -2) return 0;
      if (!self()->canInjectInduceOSR(refNode)) return 0;
      int32_t random = self()->comp()->adhocRandom().getRandom();
      if (self()->comp()->getOption(TR_TraceOSR))
         traceMsg(self()->comp(), "Random fake induceOSR injection: caller=%d bc=%x random=%d\n", callerIndex, byteCodeIndex, random);
      if (self()->comp()->adhocRandom().getRandom() % recipProb != 0) return 0;
      return 1;
      }
   else if (childPath[0] == 'g')
      {
      if ((callerIndex != -3 && refNode->getInlinedSiteIndex() != callerIndex) ||
          (byteCodeIndex != -3 && refNode->getByteCodeIndex() < byteCodeIndex))
         return 0;
      if (self()->canInjectInduceOSR(refNode))
         return 1;
      else
         return 0;
      }
   else
      return 0;
   TR_ASSERT(0, "should have returned by now\n");
   }

void
OMR::ResolvedMethodSymbol::genAndAttachOSRCodeBlocks(int32_t currentInlinedSiteIndex)
   {
   bool trace = self()->comp()->getOption(TR_TraceOSR);
   TR_ASSERT(self()->getFirstTreeTop(), "the method doesn't have any trees\n");
   int16_t callSiteInsertionPoint, bcIndexInsertionPoint;
   char childPath[10];
   childPath[0] = '\0';
   bcIndexForFakeInduce(self()->comp(), &callSiteInsertionPoint, &bcIndexInsertionPoint, childPath);
   const char * mSignature = self()->signature(self()->comp()->trMemory());

   TR::TreeTop *lastTreeTop = NULL;
   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(currentInlinedSiteIndex, self());

   // Under NextGenHCR, the method entry for the outermost method is an implicit OSR point
   // To ensure a transition from this point is possible, it is necessary to generate and attach
   // a catch and code block now, with an exception edge to the catch block from the first block
   // This exception edge should remain until OSR guards have been inserted, at which point it is no
   // longer needed
   //
   if (self()->comp()->isOutermostMethod() && self()->comp()->getHCRMode() == TR::osr)
      {
      // Add an exception edge to the OSR catch block from the first block
      TR::Block *OSRCatchBlock = osrMethodData->findOrCreateOSRCatchBlock(self()->getFirstTreeTop()->getNode());
      TR::Block *firstBlock = self()->getFirstTreeTop()->getEnclosingBlock();
      if (!firstBlock->hasExceptionSuccessor(OSRCatchBlock))
         self()->comp()->getFlowGraph()->addEdge(TR::CFGEdge::createExceptionEdge(firstBlock, OSRCatchBlock, self()->comp()->trMemory()));

      // Create an OSR point with bytecode index 0
      TR_ByteCodeInfo firstBCI = self()->getFirstTreeTop()->getNode()->getByteCodeInfo();
      firstBCI.setByteCodeIndex(0);
      TR_OSRPoint *osrPoint = new (self()->comp()->trHeapMemory()) TR_OSRPoint(firstBCI, osrMethodData, self()->comp()->trMemory());
      osrPoint->setOSRIndex(self()->addOSRPoint(osrPoint));
      }

   //Using this flag we avoid processing twice a call before which we are injecting an induceOSR
   bool skipNextTT = false;
   for (TR::TreeTop* tt = self()->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node* ttnode = tt->getNode();
      if (!skipNextTT)
         {
         int matchCode = self()->matchInduceOSRCall(tt, callSiteInsertionPoint, bcIndexInsertionPoint, childPath);
         if (matchCode > 0)
            {
            TR::Node *refNode = tt->getNode()->getFirstChild();
            int32_t numChildren   = refNode->getNumChildren();
            int32_t firstArgIndex = refNode->getFirstArgumentIndex();
            self()->genInduceOSRCallNode(tt, (numChildren - firstArgIndex), matchCode == 1);
            tt = tt->getPrevTreeTop();
            ttnode = tt->getNode();
            if (trace)
               {
               const char * mSignature = self()->signature(self()->comp()->trMemory());
               traceMsg(self()->comp(), "fake induce %p generated for %s at callsite %d bytecode %x\n",
                       ttnode, mSignature, callSiteInsertionPoint, bcIndexInsertionPoint);
               }
            skipNextTT = true;
            }
         }
      else
         skipNextTT = false;
      // check potential OSR point using a node to avoid a check for the exception successor
      // which we are responsible for creating if we decide to continue with this OSR point
      if (self()->comp()->isPotentialOSRPoint(tt->getNode()))
         {
         TR::Block * block = tt->getEnclosingBlock();
         // Add the OSR point to the list
         TR::Block * OSRCatchBlock = osrMethodData->findOrCreateOSRCatchBlock(ttnode);
         TR_OSRPoint *osrPoint = NULL;

         if (self()->comp()->isOSRTransitionTarget(TR::preExecutionOSR) || self()->comp()->requiresAnalysisOSRPoint(ttnode))
            {
            osrPoint = new (self()->comp()->trHeapMemory()) TR_OSRPoint(ttnode->getByteCodeInfo(), osrMethodData, self()->comp()->trMemory());
            osrPoint->setOSRIndex(self()->addOSRPoint(osrPoint));
            if (self()->comp()->getOption(TR_TraceOSR))
               traceMsg(self()->comp(), "pre osr point added for [%p] for bci %d:%d\n",
                  ttnode, ttnode->getByteCodeInfo().getCallerIndex(),
                  ttnode->getByteCodeInfo().getByteCodeIndex());
            }

         if (self()->comp()->isOSRTransitionTarget(TR::postExecutionOSR))
            {
            TR_ByteCodeInfo offsetBCI = ttnode->getByteCodeInfo();
            offsetBCI.setByteCodeIndex(offsetBCI.getByteCodeIndex() + self()->comp()->getOSRInductionOffset(ttnode));
            osrPoint = new (self()->comp()->trHeapMemory()) TR_OSRPoint(offsetBCI, osrMethodData, self()->comp()->trMemory());
            osrPoint->setOSRIndex(self()->addOSRPoint(osrPoint));
            if (self()->comp()->getOption(TR_TraceOSR))
               traceMsg(self()->comp(), "post osr point added for [%p] for bci %d:%d\n",
                  ttnode, offsetBCI.getCallerIndex(), offsetBCI.getByteCodeIndex());
            }

         // Add an exception edge from the current block to the OSR catch block if there isn't already one
         auto edge = block->getExceptionSuccessors().begin();
         for (; edge != block->getExceptionSuccessors().end(); ++edge)
            {
            TR::Block *to = toBlock((*edge)->getTo());
            if (to == OSRCatchBlock)
               break;
            }

         if (edge == block->getExceptionSuccessors().end())
            self()->comp()->getFlowGraph()->addEdge( TR::CFGEdge::createExceptionEdge(block, OSRCatchBlock, self()->comp()->trMemory()));
         }
      if (tt->getNextTreeTop() == NULL)
         lastTreeTop = tt;
      }

   // If OSR code and catch blocks have been created, stitch them into the
   // method's trees. Check for the OSR blocks directly, since they're specific
   // to the current inlined call site, unlike getOSRPoints().
   TR::Block * const osrCodeBlock = osrMethodData->getOSRCodeBlock();
   TR::Block * const osrCatchBlock = osrMethodData->getOSRCatchBlock();
   TR_ASSERT_FATAL(
      (osrCodeBlock != NULL) == (osrCatchBlock != NULL),
      "either OSR code or catch block was generated without the other\n");

   if (osrCodeBlock != NULL)
      lastTreeTop->insertTreeTopsAfterMe(osrCatchBlock->getEntry(), osrCodeBlock->getExit());
   }

static void getNeighboringSymRefLists(int32_t index, TR_Array<List<TR::SymbolReference> >* listArray,
   List<TR::SymbolReference> **listForPrevSlot,
   List<TR::SymbolReference> **list,
   List<TR::SymbolReference> **listForNextSlot)
   {
   *list = &(listArray->element(index));
   if (index >= 1)
      *listForPrevSlot = &(listArray->element(index-1));
   else
      *listForPrevSlot = NULL;
   if (index < listArray->lastIndex())
      *listForNextSlot = &(listArray->element(index+1));
   else
      *listForNextSlot = NULL;
   }

static bool isTwoSlotSymRefIn(List<TR::SymbolReference> *symRefList)
   {
   if (symRefList)
      {
      ListIterator<TR::SymbolReference> iter(symRefList);
      for (TR::SymbolReference* symRef = iter.getFirst(); symRef; symRef = iter.getNext())
         {
         switch (symRef->getSymbol()->getDataType())
            {
            case TR::Int64:
            case TR::Double:
               return true;
            default:
            	break;
            }
         }
      }

   return false;
   }

bool
OMR::ResolvedMethodSymbol::sharesStackSlot(TR::SymbolReference *symRef)
   {
   TR_ASSERT(symRef->getSymbol()->isAutoOrParm(), "sharesStackSlot requires #%d to be on the stack", symRef->getReferenceNumber());
   int32_t slot = symRef->getCPIndex();
   if (slot >= self()->getFirstJitTempIndex())
      return false; // Jit temps don't share slots
   TR::DataType dt = symRef->getSymbol()->getDataType();
   bool takesTwoSlots = dt == TR::Int64 || dt == TR::Double;

   List<TR::SymbolReference> *listForPrevSlot, *list, *listForNextSlot;
   if (slot < 0)
      getNeighboringSymRefLists(-slot - 1, self()->getPendingPushSymRefs(), &listForPrevSlot, &list, &listForNextSlot);
   else
      getNeighboringSymRefLists(slot, self()->getAutoSymRefs(), &listForPrevSlot, &list, &listForNextSlot);

   if (list->isMultipleEntry())
      return true;
   else if (isTwoSlotSymRefIn(listForPrevSlot))
      return true;
   else if (takesTwoSlots && listForNextSlot && !listForNextSlot->isEmpty())
      return true;
   else
      return false;
   }

void
OMR::ResolvedMethodSymbol::genOSRHelperCall(int32_t currentInlinedSiteIndex, TR::SymbolReferenceTable* symRefTab, TR::CFG *cfg)
   {
   // If not specified use the outermost CFG
   if (!cfg)
      cfg = self()->comp()->getFlowGraph();

   bool trace = self()->comp()->getOption(TR_TraceOSR);
   // Use first node of the method for bytecode info
   TR_ASSERT(self()->getFirstTreeTop(), "first tree top is NULL in %s", self()->signature(self()->comp()->trMemory()));
   TR::Node *firstNode = self()->getFirstTreeTop()->getNode();

   // Create the OSR helper symbol reference
   TR::Node *vmThread = TR::Node::createWithSymRef(firstNode, TR::loadaddr, 0, new (self()->comp()->trHeapMemory()) TR::SymbolReference(symRefTab, TR::RegisterMappedSymbol::createMethodMetaDataSymbol(self()->comp()->trHeapMemory(), "vmThread")));
   TR::SymbolReference *osrHelper = symRefTab->findOrCreateRuntimeHelper(TR_prepareForOSR, false, false, true);
#if defined(TR_TARGET_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM)
   osrHelper->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
#else
   osrHelper->getSymbol()->castToMethodSymbol()->setPreservesAllRegisters();
   osrHelper->getSymbol()->castToMethodSymbol()->setSystemLinkageDispatch();
#endif

   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(currentInlinedSiteIndex, self());

   // Create children to the OSR helper call node and save them to a list
   TR_Array<TR::Node*> loadNodes(self()->comp()->trMemory());
   loadNodes.add(vmThread);
   loadNodes.add(TR::Node::iconst(firstNode, osrMethodData->getInlinedSiteIndex()));
   TR::Node *loadNode = NULL;
   intptrj_t i = 0;

   bool alreadyLoadedSyncObjectTemp = false;
   bool alreadyLoadedThisTempForObjectCtor = false;

   // Pending push temporaries
   TR_Array<List<TR::SymbolReference> > *ppsListArray = self()->getPendingPushSymRefs();
   for (i = 0; ppsListArray && i < ppsListArray->size(); ++i)
      {
      List<TR::SymbolReference> ppsList = (*ppsListArray)[i];
      ListIterator<TR::SymbolReference> ppsIt(&ppsList);
      int symRefOrder = 0;
      for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext(), symRefOrder++)
         {
         bool sharesSlot = self()->sharesStackSlot(symRef);
         if (sharesSlot)
            {
            if (trace)
               traceMsg(self()->comp(), "#%d shares pending push slot\n", symRef->getReferenceNumber());
            if (self()->comp()->getOption(TR_DisableOSRSharedSlots))
               {
               self()->comp()->failCompilation<TR::ILGenFailure>("Pending push slot sharing detected");
               }
            }

         if (self()->getSyncObjectTemp() == symRef)
            alreadyLoadedSyncObjectTemp = true;
         else if (self()->getThisTempForObjectCtor() == symRef)
            alreadyLoadedThisTempForObjectCtor = true;

         loadNodes.add(TR::Node::createLoad(firstNode, symRef));
         loadNodes.add(TR::Node::iconst(firstNode, symRef->getReferenceNumber()));
         loadNodes.add(TR::Node::iconst(firstNode, sharesSlot?symRefOrder:-1));
         }
      }

   //  parameters and autos
   TR_Array<List<TR::SymbolReference> > *autosListArray = self()->getAutoSymRefs();
   for (i = 0; autosListArray && i < autosListArray->size(); ++i)
      {
      List<TR::SymbolReference> autosList = (*autosListArray)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      int symRefOrder = 0;
      for (TR::SymbolReference* symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext(), symRefOrder++)
         {
         bool sharesSlot = self()->sharesStackSlot(symRef);
         if (sharesSlot)
            {
            if (trace)
               traceMsg(self()->comp(), "#%d shares auto slot\n", symRef->getReferenceNumber());
            if (self()->comp()->getOption(TR_DisableOSRSharedSlots))
               {
               self()->comp()->failCompilation<TR::ILGenFailure>("Auto/parm slot sharing detected");
               }
            }
         // Certain special temps go into the OSR buffer, but most don't
         //
         if (self()->getSyncObjectTemp() == symRef)
            alreadyLoadedSyncObjectTemp = true;
         else if (self()->getThisTempForObjectCtor() == symRef)
            alreadyLoadedThisTempForObjectCtor = true;
         else if (symRef->getCPIndex() >= self()->getFirstJitTempIndex())
            continue;
         TR::Symbol *sym = symRef->getSymbol();
         if (trace)
            traceMsg(self()->comp(), "symref # %d is %s\n", symRef->getReferenceNumber(), (sym->isAuto()?"auto":(sym->isParm()?"parm":"not auto or parm")));

         loadNodes.add(TR::Node::createLoad(firstNode, symRef));
         loadNodes.add(TR::Node::iconst(firstNode, symRef->getReferenceNumber()));
         loadNodes.add(TR::Node::iconst(firstNode, sharesSlot?symRefOrder:-1));
         }

      }

   if (!alreadyLoadedSyncObjectTemp && (self()->getSyncObjectTemp() != NULL))
      {
      loadNodes.add(TR::Node::createLoad(firstNode, self()->getSyncObjectTemp()));
      loadNodes.add(TR::Node::iconst(firstNode, self()->getSyncObjectTemp()->getReferenceNumber()));
      loadNodes.add(TR::Node::iconst(firstNode, -1));
      }

   if (!alreadyLoadedThisTempForObjectCtor && (self()->getThisTempForObjectCtor() != NULL))
      {
      loadNodes.add(TR::Node::createLoad(firstNode, self()->getThisTempForObjectCtor()));
      loadNodes.add(TR::Node::iconst(firstNode, self()->getThisTempForObjectCtor()->getReferenceNumber()));
      loadNodes.add(TR::Node::iconst(firstNode, -1));
      }

   //Make sure we have at least 3 children because in the end (after processing children in CodeGenPrep),
   //we need an OSR helper call with 3 children.
   if (loadNodes.size() < 3)
      loadNodes.add(TR::Node::iconst(firstNode, 0));  //add a dummy child


   // Create the OSR helper call node and set its children
   TR::Node *osrCall = TR::Node::createWithSymRef(firstNode, TR::call, loadNodes.size(), osrHelper);
   for (int i = 0; i < loadNodes.size(); i++)
      osrCall->setAndIncChild(i, loadNodes[i]);
   TR_ASSERT(osrCall->getNumChildren() >= 3, "the osr helper call needs to have at least 3 children\n");

   // Add the call to the OSR Block
   TR::Node *osrNode = TR::Node::create(TR::treetop, 1, osrCall);
   TR::Block *OSRCodeBlock = osrMethodData->getOSRCodeBlock();
   TR::TreeTop *osrTT = TR::TreeTop::create(self()->comp(), osrNode);
   OSRCodeBlock->append(osrTT);

   static bool disableOSRwithTM = feGetEnv("TR_disableOSRwithTM") ? true: false;
   if (self()->comp()->cg()->getSupportsTM() && !self()->comp()->getOption(TR_DisableTLE) && !disableOSRwithTM)
      {
      TR::Node *tabortNode = TR::Node::create(osrNode, TR::tabort, 0, 0);
      TR::TreeTop *tabortTT = TR::TreeTop::create(self()->comp(),tabortNode, NULL,NULL);
      tabortNode->setSymbolReference(self()->comp()->getSymRefTab()->findOrCreateTransactionAbortSymbolRef(self()->comp()->getMethodSymbol()));
      if (trace)
         traceMsg(self()->comp(), "adding tabortNode %p, tabortTT %p, osrNode %p\n", tabortNode, tabortTT, osrNode);
      osrTT->insertBefore(tabortTT);
      if (trace)
         traceMsg(self()->comp(), "osrNode->getPrevTreeTop()->getNode() %p\n", osrTT->getPrevTreeTop()->getNode());
      }

   if (self()->comp()->getCurrentInlinedSiteIndex() == -1)
      {
      //I'm compiling the top-level caller, so put a TR::igoto to vmThread->osrReturnAddress
      //to get the control back to the VM after we fill the OSR buffer
      TR::Node* osrReturnAddress = TR::Node::createLoad(firstNode , self()->comp()->getSymRefTab()->findOrCreateOSRReturnAddressSymbolRef());
      TR::Node* igotoNode = TR::Node::create(firstNode, TR::igoto, 1);
      igotoNode->setChild(0, osrReturnAddress);
      osrReturnAddress->incReferenceCount();
      OSRCodeBlock->append(TR::TreeTop::create(self()->comp(), igotoNode));
      }
   else
      {
      // Add the dead stores for this call site
      TR_OSRMethodData* callerOSRData = self()->comp()->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
      TR_InlinedCallSite &callSiteInfo = self()->comp()->getInlinedCallSite(currentInlinedSiteIndex);
      callerOSRData->getMethodSymbol()->insertStoresForDeadStackSlots(self()->comp(), callSiteInfo._byteCodeInfo,
         OSRCodeBlock->getExit(), false);

      //create an appropritely-typed return node and append it to the OSR code block and add
      //the corresponding cfg edge
      bool voidReturn = self()->getResolvedMethod()->returnOpCode() == TR::Return;
      TR::Node *retNode = TR::Node::create(firstNode, self()->getResolvedMethod()->returnOpCode(), voidReturn?0:1);
      if (!voidReturn)
         {
         TR::Node *retValNode = TR::Node::create(firstNode, self()->comp()->il.opCodeForConst(self()->getResolvedMethod()->returnType()), 0);
         retValNode->setLongInt(0);
         retNode->setAndIncChild(0, retValNode);
         }
      OSRCodeBlock->append(TR::TreeTop::create(self()->comp(), retNode));
      }

   cfg->addEdge(OSRCodeBlock, cfg->getEnd());
   }


bool
OMR::ResolvedMethodSymbol::genIL(TR_FrontEnd * fe, TR::Compilation * comp, TR::SymbolReferenceTable * symRefTab, TR::IlGenRequest & customRequest)
   {
   LexicalTimer t("IL generation", comp->phaseTimer());
   TR::LexicalMemProfiler mp("IL generation", comp->phaseMemProfiler());
   bool traceIt = false;
   if (comp->ilGenTrace() || comp->isPeekingMethod())
      traceIt = true;

   if (traceIt && comp->getOutFile() != NULL && comp->getOption(TR_TraceBC))
      {
      // matching its traceflag
      if (comp->isPeekingMethod())
         traceMsg(comp, "<peeking ilgen\n"
                 "\tmethod=\"%s\">\n",
                 self()->signature(comp->trMemory()));
      else
         traceMsg(comp, "<ilgen\n"
                 "\tmethod=\"%s\">\n",
                 self()->signature(comp->trMemory()));
      if (comp->getDebug())
         {
         traceMsg(comp, "   <request> ");
         customRequest.print(fe, comp->getOutFile(), " </request>\n");
         }
      }

   if (!_firstTreeTop)
      comp->addGenILSym(self());

   // todo: should only have to generate the tree once.  Currently the
   // inliner modifies the generated tree so we need to regenerate it
   // each time...
   // replaced  "if (!_firstTreeTop || 1)" with "if (!_firstTreeTop || !comp->isPeekingMethod())"

   TR::Optimizer *optimizer = NULL;
   TR::Optimizer *previousOptimizer = NULL;
   try
      {
      if (!_firstTreeTop || !comp->isPeekingMethod())
         {
         _firstTreeTop = 0;
         _flowGraph = new (comp->trHeapMemory()) TR::CFG(comp, self());
         _flowGraph->setStartAndEnd(new (comp->trHeapMemory()) TR::Block(comp->trMemory()),
                                    new (comp->trHeapMemory()) TR::Block(comp->trMemory()));

         if (comp->getOption(TR_EnableOSR) && !comp->isPeekingMethod())
            {
            _cannotAttemptOSR = new (comp->trHeapMemory()) TR_BitVector(1, comp->trMemory(), heapAlloc, growable);
            }

         if (_tempIndex == -1)
            self()->setParameterList();
         _tempIndex = _firstJitTempIndex;
         //_automaticList.setListHead(0); // what's the point ? sym ref tab can use the sym refs anyway by using methodSymbol's auto sym refs and pending push sym refs list; this only confuses analyses into thinking these autos cannot be used when they actually can be

         TR_IlGenerator *ilGen = customRequest.getIlGenerator(self(), fe, comp, symRefTab);

         auto genIL_rc = ilGen->genIL();
         _methodFlags.set(IlGenSuccess, genIL_rc);

         if (comp->getOutFile() != NULL && comp->getOption(TR_TraceBC))
            {
            traceMsg(self()->comp(), "genIL() returned %d\n", genIL_rc);
            }

         if (_methodFlags.testAny(IlGenSuccess))
            {
            if (!comp->isPeekingMethod())
               {
               if (self()->catchBlocksHaveRealPredecessors(comp->getFlowGraph(), comp))
                  {
                  comp->failCompilation<TR::CompilationException>("Catch blocks have real predecessors");
                  }
               }

            // If OSR is enabled, need to create an OSR helper call to keep the pending pushes,
            // parms, and locals alive. Only do this in non HCR mode since under HCR we will be more selective about where to
            // add OSR points.
            //
            const int32_t siteIndex = comp->getCurrentInlinedSiteIndex();
            bool doOSR = comp->getOption(TR_EnableOSR)
               && !comp->isPeekingMethod()
               && !(comp->getOption(TR_DisableNextGenHCR) && comp->getOption(TR_EnableHCR))
               && comp->supportsInduceOSR()
               && !self()->cannotAttemptOSRDuring(siteIndex, comp);

            optimizer = TR::Optimizer::createOptimizer(comp, self(), true);
            previousOptimizer = comp->getOptimizer();
            comp->setOptimizer(optimizer);

            self()->detectInternalCycles(comp->getFlowGraph(), comp);

            if (doOSR)
               {
               TR_OSRCompilationData *osrCompData = comp->getOSRCompilationData();
               TR_ASSERT(osrCompData != NULL, "OSR compilation data is NULL\n");

               self()->genAndAttachOSRCodeBlocks(siteIndex);

               // Check for the OSR blocks directly, since they're specific
               // to the current inlined call site, unlike getOSRPoints().
               TR_OSRMethodData *osrMethodData =
                  osrCompData->findOrCreateOSRMethodData(siteIndex, self());

               if (osrMethodData->getOSRCodeBlock() != NULL)
                  {
                  self()->genOSRHelperCall(siteIndex, symRefTab);
                  if (comp->getOption(TR_TraceOSR))
                     comp->dumpMethodTrees("Trees after OSR in genIL", self());
                  }

               if (!comp->isOutermostMethod())
                  self()->cleanupUnreachableOSRBlocks(siteIndex, comp);
               }

            if (optimizer)
               {
               optimizer->optimize();
               comp->setOptimizer(previousOptimizer);
               }
            else
               {
               if (comp->getOutFile() != NULL && comp->getOption(TR_TraceBC))
                  traceMsg(comp, "Skipping ilgen opts\n");
               }
            }
         }
      }
   catch (const TR::RecoverableILGenException &e)
      {
      if (self()->comp()->isOutermostMethod())
         throw;
      _methodFlags.set(IlGenSuccess, false);
      if (optimizer) //if the exception is from ilgen opts we need to restore previous optimizer
         comp->setOptimizer(previousOptimizer);
      else //if the exception is from genIL() need to reset IlGenerator to 0,
           //otherwise, compilation would get wrong method symbol from a stale IlGenerator
         comp->setCurrentIlGenerator(0);
      }

   if (traceIt && (comp->getOutFile() != NULL) && comp->getOption(TR_TraceBC))
      {
      if (comp->isPeekingMethod())
         traceMsg(comp, "</peeking ilgen>\n");
      else
         traceMsg(comp, "</ilgen>\n");
      }
   return _methodFlags.testAny(IlGenSuccess);
   }


//returns true if there is a shared auto slot or a shared pending push temp slot, and returns false otherwise.
bool
OMR::ResolvedMethodSymbol::sharesStackSlots(TR::Compilation *comp)
   {
   auto *methodSymbol = self();
   intptrj_t i = 0;
   bool isRequired = false;

   // Check for pending pushes
   TR_Array<List<TR::SymbolReference> > *ppsListArray = methodSymbol->getPendingPushSymRefs();
   bool prevTakesTwoSlots = false;
   for (i = 0; !isRequired && ppsListArray && i < ppsListArray->size(); ++i)
      {
      List<TR::SymbolReference> ppsList = (*ppsListArray)[i];
      ListIterator<TR::SymbolReference> ppsIt(&ppsList);
      bool takesTwoSlots = false;
      for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
         {
         TR::DataType dt = symRef->getSymbol()->getDataType();
         takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
         if (takesTwoSlots)
            break;
         }

      if ((ppsList.getSize() > 1) ||
          (prevTakesTwoSlots && !ppsList.isEmpty()))
         {
         ListIterator<TR::SymbolReference> ppsIt(&ppsList);
         if (comp->getOption(TR_TraceOSR))
            {
            traceMsg(comp, "pending push temps share slots:");
            for (TR::SymbolReference* symRef = ppsIt.getFirst(); symRef; symRef = ppsIt.getNext())
               traceMsg(comp," %d ", symRef->getReferenceNumber());
            traceMsg(comp, "\n");
            }
         isRequired = true;
         }

      prevTakesTwoSlots = takesTwoSlots;
      }

   // Check for parameters and autos
   TR_Array<List<TR::SymbolReference> > *autosListArray = methodSymbol->getAutoSymRefs();
   prevTakesTwoSlots = false;
   for (i = 0; !isRequired && autosListArray && i < autosListArray->size(); ++i)
      {
      List<TR::SymbolReference> autosList = (*autosListArray)[i];
      ListIterator<TR::SymbolReference> autosIt(&autosList);
      TR::SymbolReference *symRef = autosIt.getFirst();
      bool takesTwoSlots = false;
      if (symRef == NULL)
         {
         prevTakesTwoSlots = takesTwoSlots;
         continue;
         }

      for (; symRef; symRef = autosIt.getNext())
         {
         TR::DataType dt = symRef->getSymbol()->getDataType();
         takesTwoSlots = dt == TR::Int64 || dt == TR::Double;
         if (takesTwoSlots)
            break;
         }

      symRef = autosIt.getFirst();

      if ((symRef->getCPIndex() < methodSymbol->getFirstJitTempIndex()) &&
          ((autosList.getSize() > 1) ||
           (prevTakesTwoSlots && !autosList.isEmpty())))
         {
         if (comp->getOption(TR_TraceOSR))
            {
            traceMsg(comp, "autos or parameters share slots:");
            for (symRef = autosIt.getFirst(); symRef; symRef = autosIt.getNext())
               traceMsg(comp," %d ", symRef->getReferenceNumber());
            traceMsg(comp, "\n");
            }
         isRequired = true;
         }

      prevTakesTwoSlots = takesTwoSlots;
      }

   return isRequired;
   }


int32_t
OMR::ResolvedMethodSymbol::getNumberOfBackEdges()
   {
   int32_t numBackEdges = 0;
   bool inColdBlock = false;
   for (TR::TreeTop *tt = self()->getFirstTreeTop();
        tt;
        tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getOpCodeValue() == TR::BBStart)
         {
         inColdBlock = false;
         if (tt->getNode()->getBlock()->isCold())
            inColdBlock = true;
         }

      if ((tt->getNode()->getOpCodeValue() == TR::asynccheck) &&
          !inColdBlock)
         numBackEdges++;
      }
   return numBackEdges;
   }

void
OMR::ResolvedMethodSymbol::resetLiveLocalIndices()
   {
   TR::ParameterSymbol *p1;
   ListIterator<TR::ParameterSymbol> parms(&self()->getParameterList());
   for (p1 = parms.getFirst(); p1 != NULL; p1 = parms.getNext())
      p1->setLiveLocalIndexUninitialized();

   TR::AutomaticSymbol *p2;
   ListIterator<TR::AutomaticSymbol> locals(&self()->getAutomaticList());
   for (p2 = locals.getFirst(); p2 != NULL; p2 = locals.getNext())
      p2->setLiveLocalIndexUninitialized();
   }

bool
OMR::ResolvedMethodSymbol::supportsInduceOSR(TR_ByteCodeInfo &bci,
                                           TR::Block *blockToOSRAt,
                                           TR::Compilation *comp,
                                           bool runCleanup)
   {
   if (!comp->supportsInduceOSR())
      return false;

   // Check it is possible to transition within this call site
   if (self()->cannotAttemptOSRDuring(bci.getCallerIndex(), comp, runCleanup))
      return false;

   // Check it is possible to transition at this BCI
   if (self()->cannotAttemptOSRAt(bci, blockToOSRAt, comp))
      return false;

   return true;
   }

bool
OMR::ResolvedMethodSymbol::hasOSRProhibitions()
   {
   return _cannotAttemptOSR != NULL && !_cannotAttemptOSR->isEmpty();
   }

/*
 * Prevent OSR transitions to the specified bytecode index.
 * Will only prevent those directed immediately to this index,
 * not those from a deeper frame.
 * See setCannotAttemptOSRDuring to prevent these cases.
 */
void
OMR::ResolvedMethodSymbol::setCannotAttemptOSR(int32_t n)
   {
   _cannotAttemptOSR->set(n);
   }

/*
 * Check if OSR can be attempted within the specified call site index.
 * Will check OSR infrastructure, such as code and catch blocks, is still present.
 *
 * If it is not possible to transition, this will be stored in TR_InlinedCallSiteInfo, as it is
 * assumed that once a transition is not possible, it will not become possible later.
 *
 * runCleanup: If there are missing OSR blocks detected, runnning cleanup will remove those that are no longer needed.
 */
bool
OMR::ResolvedMethodSymbol::cannotAttemptOSRDuring(int32_t callSite, TR::Compilation *comp, bool runCleanup)
   {
   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "Checking if OSR can be attempted during call site %d\n", callSite);

   int32_t origCallSite = callSite;
   TR_OSRMethodData *osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callSite, self());
   bool cannotAttemptOSR = false;
   int32_t byteCodeIndex;

   // Walk up the inlined call stack and safety check every call to see if OSR is safe - note that we check
   // cannotAttemptOSRDuring, not cannotAttemptOSRAt
   while (osrMethodData->getInlinedSiteIndex() > -1)
      {
      TR_ASSERT(callSite == osrMethodData->getInlinedSiteIndex(), "OSR method data and inlined call sites array being walked out of sync\n");

      TR_InlinedCallSite &callSiteInfo = comp->getInlinedCallSite(callSite);
      cannotAttemptOSR = comp->cannotAttemptOSRDuring(callSite);
      if (!cannotAttemptOSR)
         {
         callSite = callSiteInfo._byteCodeInfo.getCallerIndex();
         byteCodeIndex = callSiteInfo._byteCodeInfo.getByteCodeIndex();
         if (comp->getOption(TR_TraceOSR))
            traceMsg(comp, "Checking if OSR can be attempted at caller bytecode index %d:%d\n", callSite, byteCodeIndex);

         // Check OSR method data has been generated for the caller
         osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
         if (!osrMethodData)
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Cannot attempt OSR as OSR method data for caller of callee %d is NULL\n", callSite);
            cannotAttemptOSR = true;
            break;
            }

         // Check the OSR catch/code blocks exist
         TR::Block * osrCodeBlock = osrMethodData->getOSRCodeBlock();
         if (!osrCodeBlock || osrCodeBlock->isUnreachable())
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Cannot attempt OSR as OSR code block for site index %d is absent\n",
                  osrMethodData->getInlinedSiteIndex());
            if (runCleanup)
               self()->cleanupUnreachableOSRBlocks(origCallSite, comp);
            cannotAttemptOSR = true;
            break;
            }

         // Check it is possible to transition during the caller
         auto *callerSymbol = osrMethodData->getMethodSymbol();
         if (callerSymbol->_cannotAttemptOSR->get(byteCodeIndex))
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Cannot attempt OSR during caller bytecode index %d:%d\n", callSite, byteCodeIndex);
            cannotAttemptOSR = true;
            break;
            }

         // Check the caller existed during ILGen
         if (callSiteInfo._byteCodeInfo.doNotProfile())
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Cannot attempt OSR during caller bytecode index %d:%d as it did not exist at ilgen\n", callSite, byteCodeIndex);
            cannotAttemptOSR = true;
            break;
            }
         }
      else
         break;
      }

   // Store the result against the call site now that it is known
   //
   if (origCallSite > -1 && !comp->cannotAttemptOSRDuring(origCallSite) && cannotAttemptOSR)
      comp->setCannotAttemptOSRDuring(origCallSite, cannotAttemptOSR);
   return cannotAttemptOSR;
   }

/*
 * Check whether OSR can be attempted at the specified bytecode index.
 * This will check transition is possible in the context of this call site, ignoring those higher in the call stack.
 *
 * TODO: Return enum encoding the failure case, so that OSR modes can manage the different failures.
 *
 * bci: Bytecode info to perform the transition at
 * blockToOSRAt: Block containing the transition, to verify the exception edge is present.
 * comp: Compilation object.
 */
bool
OMR::ResolvedMethodSymbol::cannotAttemptOSRAt(TR_ByteCodeInfo &bci,
                                          TR::Block *blockToOSRAt,
                                          TR::Compilation *comp)
   {
   int32_t callSite = bci.getCallerIndex();
   int32_t byteCodeIndex = bci.getByteCodeIndex();
   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "Checking if OSR can be attempted at bytecode index %d:%d\n",
         callSite, byteCodeIndex);

   // Check it is possible to transition at this index
   if (self()->_cannotAttemptOSR->get(byteCodeIndex))
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Cannot attempt OSR at bytecode index %d:%d\n",
            callSite, byteCodeIndex);
      return true;
      }

   // Check the BCI existed at ilgen
   if (bci.doNotProfile())
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Cannot attempt OSR at bytecode index %d:%d as it did not exist at ilgen\n",
            callSite, byteCodeIndex);
      return true;
      }

   TR_OSRMethodData *osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callSite, self());
   TR::Block * osrCatchBlock = osrMethodData->getOSRCatchBlock();

   // If a block is specified, check the osrCatchBlock exists and there is an exception edge to it
   if (blockToOSRAt && (!osrCatchBlock || !blockToOSRAt->hasExceptionSuccessor(osrCatchBlock)))
      {
      if (comp->getOption(TR_TraceOSR))
         {
         if (osrCatchBlock)
            traceMsg(comp, "Cannot attempt OSR as block_%d is missing an edge to OSR catch block: block_%d\n",
               blockToOSRAt->getNumber(), osrCatchBlock->getNumber());
         else
            traceMsg(comp, "Cannot attempt OSR as call site index %d lacks an OSR catch block for block_%d\n",
               callSite, blockToOSRAt->getNumber());
         }
      return true;
      }

   if (comp->getOption(TR_TraceOSR))
     traceMsg(comp, "OSR can be attempted\n");

   return false;
   }

void
OMR::ResolvedMethodSymbol::cleanupUnreachableOSRBlocks(int32_t inlinedSiteIndex, TR::Compilation *comp)
   {
   TR_OSRMethodData *osrMethodData = inlinedSiteIndex > -1 ? comp->getOSRCompilationData()->findCallerOSRMethodData(comp->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, self())) : NULL;
   TR::Block * OSRCatchBlock = osrMethodData ? osrMethodData->getOSRCatchBlock() : NULL;

   bool allCallersOSRCodeBlocksAreStillInCFG = true;
   TR_OSRMethodData *finalOsrMethodData = NULL;
   while (osrMethodData)
      {
      TR::Block * CallerOSRCodeBlock = osrMethodData->getOSRCodeBlock();
      if (!CallerOSRCodeBlock ||
          CallerOSRCodeBlock->isUnreachable())
         {
         if (comp->getOption(TR_TraceOSR))
            traceMsg(comp, "Osr catch block at inlined site index %d is absent\n", osrMethodData->getInlinedSiteIndex());

         allCallersOSRCodeBlocksAreStillInCFG = false;
         finalOsrMethodData = osrMethodData;
         break;
         }
      else if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Osr catch block at inlined site index %d is present\n", osrMethodData->getInlinedSiteIndex());

      if (osrMethodData->getInlinedSiteIndex() > -1)
         osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
      else
         osrMethodData = NULL;
      }

   if (!allCallersOSRCodeBlocksAreStillInCFG)
      {
      TR_OSRMethodData *cursorOsrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, self());
      while (cursorOsrMethodData != finalOsrMethodData)
         {
         TR::Block * CallerOSRCatchBlock = cursorOsrMethodData->getOSRCatchBlock();

         if (CallerOSRCatchBlock)
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Removing osr catch block %p at inlined site index %d\n", CallerOSRCatchBlock, osrMethodData->getInlinedSiteIndex());

            while (!CallerOSRCatchBlock->getExceptionPredecessors().empty())
               {
               self()->getFlowGraph()->removeEdge(CallerOSRCatchBlock->getExceptionPredecessors().front());
               }
            }
         else
            break;

         if (cursorOsrMethodData->getInlinedSiteIndex() > -1)
            cursorOsrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(cursorOsrMethodData);
         else
            cursorOsrMethodData = NULL;
         }
      }
   }

void
OMR::ResolvedMethodSymbol::insertRematableStoresFromCallSites(TR::Compilation *comp, int32_t siteIndex, TR::TreeTop *induceOSRTree)
   {
   TR::TreeTop *prev = induceOSRTree->getPrevTreeTop();
   TR::TreeTop *next = induceOSRTree;
   TR::SymbolReference *ppSymRef, *loadSymRef;

   if (comp->getOption(TR_DisableOSRCallSiteRemat))
      return;

   while (siteIndex > -1)
      {
      for (uint32_t i = 0; i < comp->getOSRCallSiteRematSize(siteIndex); ++i)
         {
         comp->getOSRCallSiteRemat(siteIndex, i, ppSymRef, loadSymRef);
         if (!ppSymRef || !loadSymRef)
            continue;
         TR::Node *load = TR::Node::createLoad(loadSymRef);
         TR::Node *store = TR::Node::createStore(ppSymRef, load);
         TR::TreeTop *storeTree = TR::TreeTop::create(comp, store);
         prev->join(storeTree);
         storeTree->join(next);
         prev = storeTree;
         }

      siteIndex = comp->getInlinedCallSite(siteIndex)._byteCodeInfo.getCallerIndex();
      }
   }

/*
 * Returns the bytecode info for an OSR point, taking into account the
 * possibility that the treetop for a call has different BCI to the OSR
 * point.
 */
TR_ByteCodeInfo&
OMR::ResolvedMethodSymbol::getOSRByteCodeInfo(TR::Node *node)
   {
   if (node->getNumChildren() > 0 && (node->getOpCodeValue() == TR::treetop || node->getOpCode().isCheck()))
      return node->getFirstChild()->getByteCodeInfo();
   return node->getByteCodeInfo();
   }

/*
 * Checks the provided node is pending push store or load expected to be
 * around an OSR point.
 *
 * Pending push stores are used to ensure the stack can be reconstructed
 * after the transition, whilst anchored loads are also related as they
 * may be used to avoid the side effects of the prior mentioned stores.
 * As these anchored loads can be placed before the stores, they may
 * sit between the OSR point and the transition.
 */
bool
OMR::ResolvedMethodSymbol::isOSRRelatedNode(TR::Node *node)
   {
   return (node->getOpCode().isStoreDirect()
         && node->getOpCode().hasSymbolReference()
         && node->getSymbolReference()->getSymbol()->isPendingPush())
      || (node->getOpCodeValue() == TR::treetop
         && node->getFirstChild()->getOpCode().isLoadVarDirect()
         && node->getFirstChild()->getOpCode().hasSymbolReference()
         && node->getFirstChild()->getSymbolReference()->getSymbol()->isPendingPush());
   }

/*
 * Checks that the provided node is related to the OSR point's BCI.
 * This applies to pending push stores and loads sharing the BCI.
 */
bool
OMR::ResolvedMethodSymbol::isOSRRelatedNode(TR::Node *node, TR_ByteCodeInfo &osrBCI)
   {
   TR_ByteCodeInfo &bci = node->getByteCodeInfo();
   bool byteCodeIndex = osrBCI.getCallerIndex() == bci.getCallerIndex()
      && osrBCI.getByteCodeIndex() == bci.getByteCodeIndex();
   return byteCodeIndex && self()->isOSRRelatedNode(node);
   }

/*
 * This function will return the transition treetop for the provided OSR point,
 * depending on the transition target and surrounding treetops.
 */
TR::TreeTop *
OMR::ResolvedMethodSymbol::getOSRTransitionTreeTop(TR::TreeTop *tt)
   {
   if (self()->comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      TR_ByteCodeInfo bci = self()->getOSRByteCodeInfo(tt->getNode());
      TR::TreeTop *cursor = tt->getNextTreeTop();
      while (cursor && self()->isOSRRelatedNode(cursor->getNode(), bci))
         {
         tt = cursor;
         cursor = cursor->getNextTreeTop();
         }
      }

   return tt;
   }

/**
 * Inserts irrelevant stores before the provided tree to limit symrefs that appear live at an OSR transition.
 * This is based on the results from OSRLiveRangeAnalysis. It is assumed that the byteCodeInfo's call site is a call
 * to this resolved method symbol.
 *
 * \param byteCodeInfo BCI the OSR transition targets, used to look up dead symrefs in OSRLiveRangeAnalysis results.
 * \param insertTree Tree to prepend dead stores to.
 * \param keepStashedArgsLive Flag to toggle how symrefs used as arguments to this BCI are treated. If transitioning directly to this
 *  BCI, it should be true. If transitioning within a deeper frame to the current, it should be false. Defaults true.
 */
void
OMR::ResolvedMethodSymbol::insertStoresForDeadStackSlots(TR::Compilation *comp, TR_ByteCodeInfo &byteCodeInfo, TR::TreeTop *insertTree, bool keepStashedArgsLive)
   {
   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
   int32_t callSite = byteCodeInfo.getCallerIndex();
   int32_t byteCodeIndex = byteCodeInfo.getByteCodeIndex();
   TR_OSRMethodData *osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callSite, self());

   TR_BitVector *deadSymRefs = osrMethodData->getLiveRangeInfo(byteCodeIndex);
   if (deadSymRefs == NULL)
      return;

   // The arguments to the induce OSR call may be stashed against its bytecode index.
   // If so, dead stores must not be generated for these symrefs.
   if (keepStashedArgsLive)
      {
      TR_Array<int32_t> *args = osrMethodData->getArgInfo(byteCodeIndex);
      for (int32_t i = 0; args && i < args->size(); ++i)
         deadSymRefs->reset((*args)[i]);
      }

   TR::TreeTop *prev = insertTree->getPrevTreeTop();
   TR::TreeTop *next = insertTree;

   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "Inserting stores for dead stack slots in method at caller index %d and bytecode index %d for induceOSR call %p\n", callSite, byteCodeIndex, insertTree->getNode());

   TR_BitVectorIterator bvi(*deadSymRefs);
   while (bvi.hasMoreElements())
      {
      int32_t nextDeadVar = bvi.getNextElement();
      TR::SymbolReference *nextSymRef = symRefTab->getSymRef(nextDeadVar);

      if (!nextSymRef->getSymbol()->isParm()
          && performTransformation(comp, "OSR LIVE RANGE ANALYSIS : Local %d is reset before tree [%p] (caller index %d bytecode index %d)\n", nextSymRef->getReferenceNumber(), insertTree->getNode(), callSite, byteCodeIndex))
         {
         TR::Node *storeNode = TR::Node::createWithSymRef(comp->il.opCodeForDirectStore(nextSymRef->getSymbol()->getDataType()), 1, 1,
                                              TR::Node::createConstDead(insertTree->getNode(), nextSymRef->getSymbol()->getDataType()), nextSymRef);
         storeNode->setStoredValueIsIrrelevant(true);
         TR::TreeTop *storeTree = TR::TreeTop::create(comp, storeNode);
         prev->join(storeTree);
         storeTree->join(next);
         prev = storeTree;
         }
      }
   }

/**
 * \verbatim
 * OSR dead stores for a transition at BCI 1:N
 *
 * CalleeIndex CallerIndex ByteCodeIndex
 *     0         -1           P
 *     1         0            M
 *
 *        |
 *        |
 *   -------------    ---------------------------
 *   | MAINLINE  |    | OSR INDUCE BLOCK        |
 *   -------------    ---------------------------
 *   | OSR Guard |--->| Dead stores for 1:N     |
 *   -------------    ---------------------------
 *        |                        |
 *        |                        |
 *        V           ---------------------------
 *                    | OSR CATCH + CODE BLOCKS |
 *                    ---------------------------
 *                    | Callee 1 prepareForOSR  |
 *                    | Dead stores for 0:M     |
 *                    ---------------------------
 *                                 |
 *                                 |
 *                    ---------------------------
 *                    | OSR CATCH + CODE BLOCKS |
 *                    ---------------------------
 *                    | Callee 0 prepareForOSR  |
 *                    | Dead stores for -1:P    |
 *                    ---------------------------
 *                                 |
 *                                 |
 *                    ---------------------------
 *                    | OSR CATCH + CODE BLOCKS |
 *                    ---------------------------
 *                    | Callee -1 prepareForOSR |
 *                    ---------------------------
 *                                 |
 *                                 V
 * \endverbatim
 * This function adds the dead stores for the current BCI.
 * It does not have to concern itself with dead stores for any outer call sites, as they are located in the OSRCodeBlocks for
 * each frame, as demonstrated in the above example. This is safe as all OSR transitions in a callee will have the same liveness
 * in the caller's frame.
 *
 * \param byteCodeInfo BCI the OSR transition targets, used to look up dead symrefs in OSRLiveRangeAnalysis results.
 * \param induceOSRTree Tree to prepend dead stores to.
 */
void
OMR::ResolvedMethodSymbol::insertStoresForDeadStackSlotsBeforeInducingOSR(TR::Compilation *comp, int32_t inlinedSiteIndex, TR_ByteCodeInfo &byteCodeInfo, TR::TreeTop *induceOSRTree)
   {
   if (!comp->osrStateIsReliable())
      {
      traceMsg(comp, "OSR state may not be reliable enough to trust liveness info computed at IL gen time; so avoiding dead stack slot store insertion\n");
      return;
      }

   self()->insertStoresForDeadStackSlots(comp, byteCodeInfo, induceOSRTree);
   }

TR_OSRPoint *
OMR::ResolvedMethodSymbol::findOSRPoint(TR_ByteCodeInfo &bcInfo)
   {
   for (intptrj_t i = 0; i < _osrPoints.size(); ++i)
      {
      TR_ByteCodeInfo& pointBCInfo = _osrPoints[i]->getByteCodeInfo();
      if (pointBCInfo.getByteCodeIndex() == bcInfo.getByteCodeIndex() &&
         pointBCInfo.getCallerIndex() == bcInfo.getCallerIndex())
         return _osrPoints[i];
      }

   return NULL;
   }


void
OMR::ResolvedMethodSymbol::addAutomatic(TR::AutomaticSymbol *p)
   {
   if (!_automaticList.find(p))
      {
      bool compiledMethod = self()->comp()->getJittedMethodSymbol() == self();

      TR::CodeGenerator *cg = self()->comp()->cg();
      if (cg->getMappingAutomatics() && compiledMethod)
         cg->getLinkage()->mapSingleAutomatic(p, self()->getLocalMappingCursor());

      _automaticList.add(p);
      }
   }


void
OMR::ResolvedMethodSymbol::addVariableSizeSymbol(TR::AutomaticSymbol *s)
   {
   TR_ASSERT(!s || s->isVariableSizeSymbol(), "should be variable sized");
   if (!_variableSizeSymbolList.find(s))
      _variableSizeSymbolList.add(s);
   }

void
OMR::ResolvedMethodSymbol::addMethodMetaDataSymbol(TR::RegisterMappedSymbol *s)
   {
   TR_ASSERT(!s || s->isMethodMetaData(), "should be method metadata");
   if (!_methodMetaDataList.find(s))
      _methodMetaDataList.add(s);
   }

void
OMR::ResolvedMethodSymbol::addTrivialDeadTreeBlock(TR::Block *b)
   {
   if (!_trivialDeadTreeBlocksList.find(b))
      _trivialDeadTreeBlocksList.add(b);
   }

// get/setTempIndex is called from TR_ResolvedMethod::makeParameterList
int32_t
OMR::ResolvedMethodSymbol::setTempIndex(int32_t index, TR_FrontEnd * fe)
   {
   if ((_tempIndex = index) < 0)
      {
      self()->comp()->failCompilation<TR::CompilationException>("TR::ResolvedMethodSymbol::_tempIndex overflow");
      }
   return index;
   }

ncount_t
OMR::ResolvedMethodSymbol::generateAccurateNodeCount()
   {
   TR::TreeTop *tt = self()->getFirstTreeTop();

   self()->comp()->incOrResetVisitCount();

   ncount_t count=0;

   for (; tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      count += self()->recursivelyCountChildren(node);
      }
   return count;
   }

ncount_t
OMR::ResolvedMethodSymbol::recursivelyCountChildren(TR::Node *node)
   {
   ncount_t count=1;

   if( node->getVisitCount() >= self()->comp()->getVisitCount() )
      return 0;

   node->setVisitCount(self()->comp()->getVisitCount());

   for(int32_t i=0 ; i < node->getNumChildren(); i++)
      {
      if( node->getChild(i) )
         count += self()->recursivelyCountChildren(node->getChild(i));
      }

   return count;
   }

List<TR::SymbolReference> &
OMR::ResolvedMethodSymbol::getAutoSymRefs(int32_t slot)
   {
   TR_Memory * m = self()->comp()->trMemory();
   if (!_autoSymRefs)
      {
      if (self()->comp()->getJittedMethodSymbol() == self())
         _autoSymRefs = new (m->trHeapMemory()) TR_Array<List<TR::SymbolReference> >(m, 100, true);
      else
         _autoSymRefs = new (m->trHeapMemory()) TR_Array<List<TR::SymbolReference> >(m, _resolvedMethod->numberOfParameterSlots() + _resolvedMethod->numberOfTemps() + 5, true);
      }

   (*_autoSymRefs)[slot].setRegion(m->heapMemoryRegion());

   return (*_autoSymRefs)[slot];
   }

TR::SymbolReference *
OMR::ResolvedMethodSymbol::getParmSymRef(int32_t slot)
   {
   TR_ASSERT(_parmSymRefs, "_parmSymRefs should not be NULL!");
   return (*_parmSymRefs)[slot];
   }

void
OMR::ResolvedMethodSymbol::setParmSymRef(int32_t slot, TR::SymbolReference *symRef)
   {
   if (!_parmSymRefs)
      _parmSymRefs = new (self()->comp()->trHeapMemory()) TR_Array<TR::SymbolReference*>(self()->comp()->trMemory(), self()->getNumParameterSlots());
   (*_parmSymRefs)[slot] = symRef;
   }

List<TR::SymbolReference> &
OMR::ResolvedMethodSymbol::getPendingPushSymRefs(int32_t slot)
   {
   TR_Memory * m = self()->comp()->trMemory();
   if (!_pendingPushSymRefs)
      {
      _pendingPushSymRefs = new (m->trHeapMemory()) TR_Array<List<TR::SymbolReference> >(m, 10, true);
      }

   (*_pendingPushSymRefs)[slot].setRegion(m->heapMemoryRegion());

   return (*_pendingPushSymRefs)[slot];
   }

void
OMR::ResolvedMethodSymbol::removeTree(TR::TreeTop *tt)
   {
   TR::Node *node = tt->getNode();
   if (node != NULL)
      {
      node->recursivelyDecReferenceCount();
      if (self()->comp()->getOption(TR_TraceAddAndRemoveEdge))
         traceMsg(self()->comp(), "remove [%s]\n", node->getName(self()->comp()->getDebug()));
      }

   TR::TreeTop *prev = tt->getPrevTreeTop();
   TR::TreeTop *next = tt->getNextTreeTop();
   if (prev != NULL)
      prev->setNextTreeTop(next);
   else
      {
      TR_ASSERT(_firstTreeTop == tt, "treetops are not forming a single doubly linked list: node address = %p, global index = %d", node, (node ? node->getGlobalIndex() : -1));
      _firstTreeTop = next;
      }
   if (next != NULL)
      next->setPrevTreeTop(prev);
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::getLastTreeTop(TR::Block * b)
   {
   if (!b)
      b = _firstTreeTop->getNode()->getBlock();

   for (TR::Block * next = b->getNextBlock(); next; b = next, next = b->getNextBlock())
      ;
   return b->getExit();
   }

TR::Block *
OMR::ResolvedMethodSymbol::prependEmptyFirstBlock()
   {
   TR::Node * firstNode = self()->getFirstTreeTop()->getNode();
   TR::Block * firstBlock = firstNode->getBlock();

   TR::Block * newBlock = TR::Block::createEmptyBlock(firstNode, _flowGraph->comp(), firstBlock->getFrequency());
   self()->setFirstTreeTop(newBlock->getEntry());

   _flowGraph->insertBefore(newBlock, firstBlock);
   _flowGraph->addEdge(_flowGraph->getStart(), newBlock);
   _flowGraph->removeEdge(_flowGraph->getStart(), firstBlock);

   return newBlock;
   }


void
OMR::ResolvedMethodSymbol::setFirstTreeTop(TR::TreeTop * tt)
   {
   _firstTreeTop = tt;
   if (tt)
      tt->setPrevTreeTop(NULL);
   }

bool
OMR::ResolvedMethodSymbol::detectInternalCycles(TR::CFG *cfg, TR::Compilation *comp)
   {
   if (cfg)
      {
      int32_t numNodesInCFG = 0;
      int32_t index = 0;
      int32_t N = 0;
      for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
         {
         numNodesInCFG++;
         if (!node->getExceptionPredecessors().empty())
            {
            //catch block
            TR::CFGEdgeList excepSucc = node->getExceptionSuccessors();
            if (!excepSucc.empty())
               {
               for (auto e = excepSucc.begin(); e != excepSucc.end(); ++e)
                  {
                  TR::Block *dest = toBlock((*e)->getTo());
                  if (dest == node)
                     {
                     //found a catch block with exception successor as itself
                     dumpOptDetails(comp, "Detected catch block with exception successor as itself %d\n",
                                       node->getNumber());
                     TR::TreeTop *endTT = findEndTreeTop(self());
                     //clone the exception block
                     TR_BlockCloner cloner(cfg, true);
                     TR::Block *clonedCatch = cloner.cloneBlocks(toBlock(node), toBlock(node));
                     dumpOptDetails(comp, "Cloned catch block (%d) -> clone (%d)\n",
                                          node->getNumber(), clonedCatch->getNumber());
                     // if the catch block unlocks a monitor, then the new catch block
                     // created below must also unlock the monitor
                     //
                     bool containsMonexit = false;
                     for (TR::TreeTop *tt = clonedCatch->getEntry(); tt != clonedCatch->getExit(); tt = tt->getNextTreeTop())
                        {
                        if (tt->getNode()->getOpCode().getOpCodeValue() == TR::monexitfence)
                           {
                           containsMonexit = true;
                           break;
                           }
                        }

                     TR::TreeTop *retain = clonedCatch->getEntry();
                     // As this method is performed soon after ilgen, the exception handler
                     // may be prepended with an asynccheck and pending pushes
                     // These should be retained in the copy, so skip them when ripping out trees
                     if (comp->isOSRTransitionTarget(TR::postExecutionOSR) || comp->getOSRMode() == TR::involuntaryOSR)
                        {
                        TR::TreeTop *next = retain->getNextTreeTop();
                        if (next && next->getNode()->getOpCodeValue() == TR::asynccheck)
                           retain = self()->getOSRTransitionTreeTop(next);
                        }

                     retain->join(clonedCatch->getExit());
                     clonedCatch->getExit()->setNextTreeTop(NULL);
                     endTT->join(clonedCatch->getEntry());
                     ///dumpOptDetails(comp, "attached cloned (%d) to trees\n", clonedCatch->getNumber());

                     // unlock the monitor
                     //
                     if (containsMonexit)
                        {
                        TR::TreeTop *storeTT = TR::TreeTop::create(comp, (TR::Node::create(clonedCatch->getEntry()->getNode(),TR::monexitfence,0)));
                        dumpOptDetails(comp, "\tInserted monitor exit fence %p into cloned catch %d\n", storeTT->getNode(), clonedCatch->getNumber());
                        clonedCatch->append(storeTT);
                        }


                     //create gotoblock
                     TR::Block *gotoBlock = TR::Block::createEmptyBlock(dest->getEntry()->getNode(), comp, 0);
                     gotoBlock->getExit()->setNextTreeTop(NULL);
                     clonedCatch->getExit()->join(gotoBlock->getEntry());
                     ///dumpOptDetails(comp, "attached gotoblock (%d) to trees\n", gotoBlock->getNumber());
                     cfg->addNode(gotoBlock);
                     TR::Node *asyncNode = TR::Node::createWithSymRef(dest->getEntry()->getNextTreeTop()->getNode(),
                                                            TR::asynccheck, 0,
                                                            comp->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(comp->getMethodSymbol()));
                     asyncNode->getByteCodeInfo().setDoNotProfile(0);
                     TR::TreeTop *asyncTT = TR::TreeTop::create(comp, asyncNode);
                     gotoBlock->getEntry()->join(asyncTT);
                     TR::Node *gotoNode = TR::Node::create(dest->getEntry()->getNextTreeTop()->getNode(),
                                                            TR::Goto, 0, gotoBlock->getEntry());
                     TR::TreeTop *gotoTT = TR::TreeTop::create(comp, gotoNode);
                     asyncTT->join(gotoTT);
                     gotoTT->join(gotoBlock->getExit());
                     TR::CFGEdge *newEdge = new (comp->trHeapMemory()) TR::CFGEdge();
                     newEdge->setExceptionFromTo(node, clonedCatch);
                     ///dumpOptDetails(comp, "added ex.edge (%d)->(%d)\n", node->getNumber(), clonedCatch->getNumber());
                     cfg->addEdge(TR::CFGEdge::createEdge(clonedCatch,  gotoBlock, comp->trMemory()));
                     ///dumpOptDetails(comp, "added edge (%d)->(%d)\n", clonedCatch->getNumber(), gotoBlock->getNumber());
                     cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  gotoBlock, comp->trMemory()));
                     ///dumpOptDetails(comp, "added edge (%d)->(%d)\n", gotoBlock->getNumber(), gotoBlock->getNumber());
                     cfg->removeEdge(*e);
                     ///dumpOptDetails(comp, "removed edge (%d)->(%d)\n", node->getNumber(), dest->getNumber());
                     clonedCatch->setIsCold();
                     gotoBlock->setIsCold();
                     clonedCatch->setFrequency(CATCH_COLD_BLOCK_COUNT);
                     gotoBlock->setFrequency(CATCH_COLD_BLOCK_COUNT);

                     break;
                     }
                  }
               }
            }
         }
      ///dumpOptDetails(comp, "count of nodes in CFG = %d(num) %d(N) %d(index)\n", numNodesInCFG,
      ///                  N, index);
      return true;
      }
   else
      return false;
   }

bool
OMR::ResolvedMethodSymbol::catchBlocksHaveRealPredecessors(TR::CFG *cfg, TR::Compilation *comp)
   {
   for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
      {
      if (!node->getExceptionPredecessors().empty())
         {
         //catch block
         if (!node->getPredecessors().empty())
            {
            dumpOptDetails(comp, "detected catch block_%d with real predecessors\n", node->getNumber());
            return true;
            }
         }
      }
   return false;
   }

void
OMR::ResolvedMethodSymbol::removeUnusedLocals()
   {
   ListElement<TR::AutomaticSymbol> *cursor = _automaticList.getListHead();
   ListElement<TR::AutomaticSymbol> *prev   = NULL;
   TR::AutomaticSymbol *local;

#ifdef DEBUG
   static uint32_t afterTotalSize = 0;
   static uint32_t removedTotalSize = 0;
   uint32_t beforeSize = 0;
   uint32_t removedSize = 0;
#endif

   bool compiledMethod = (self()->comp()->getMethodSymbol() == self());

   TR_BitVector *liveButMaybeUnreferencedLocals = self()->comp()->cg()->getLiveButMaybeUnreferencedLocals();

   while (cursor)
      {
      local = cursor->getData();

#ifdef DEBUG
      beforeSize += (uint32_t)local->getSize();
#endif

      // A local maybe unreferenced but still be considered live, if all references were
      // removed as a result of coalescing (i.e. a store of a load of coalesced autos that
      // are mapped to the same slot is removed).
      if (local->getReferenceCount() == 0 &&
          !(liveButMaybeUnreferencedLocals && !local->isLiveLocalIndexUninitialized() && liveButMaybeUnreferencedLocals->isSet(local->getLiveLocalIndex())))
         {
#ifdef DEBUG
         removedSize += (uint32_t)local->getSize();
#endif
         _automaticList.removeNext(prev);

         if (prev)
            {
            cursor = prev;
            }
         else
            {
            cursor = _automaticList.getListHead();
            continue;
            }
         }
      prev = cursor;
      cursor = cursor->getNextElement();
      }

#ifdef DEBUG
   if (debug("traceUnusedLocals"))
      {
      afterTotalSize += beforeSize - removedSize;
      removedTotalSize += removedSize;
      diagnostic("%08d(%08d) %08d(%08d)   %s\n",
                  beforeSize-removedSize,
                  afterTotalSize,
                  removedSize,
                  removedTotalSize,
                  self()->comp()->signature());
      }
#endif
   }

int32_t
OMR::ResolvedMethodSymbol::incTempIndex(TR_FrontEnd * fe)
   {
   return self()->setTempIndex(_tempIndex+1, fe);
   }

bool
OMR::ResolvedMethodSymbol::hasEscapeAnalysisOpportunities()
   {
   return self()->hasNews() || self()->hasDememoizationOpportunities();
   }

bool
OMR::ResolvedMethodSymbol::doJSR292PerfTweaks()
   {
   return false;
   }

int32_t
OMR::ResolvedMethodSymbol::getArrayCopyTempSlot(TR_FrontEnd * fe)
   {
   if (_arrayCopyTempSlot == -1)
      _arrayCopyTempSlot = self()->incTempIndex(fe);
   return _arrayCopyTempSlot;
   }

// Getters

uint32_t&
OMR::ResolvedMethodSymbol::getLocalMappingCursor()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _localMappingCursor;
   }
void
OMR::ResolvedMethodSymbol::setLocalMappingCursor(uint32_t i)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   _localMappingCursor = i;
   }

uint32_t
OMR::ResolvedMethodSymbol::getProloguePushSlots()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _prologuePushSlots;
   }
uint32_t
OMR::ResolvedMethodSymbol::setProloguePushSlots(uint32_t s)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return (_prologuePushSlots = s);
   }

uint32_t
OMR::ResolvedMethodSymbol::getScalarTempSlots()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _scalarTempSlots;
   }
uint32_t
OMR::ResolvedMethodSymbol::setScalarTempSlots(uint32_t s)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return (_scalarTempSlots = s);
   }

uint32_t
OMR::ResolvedMethodSymbol::getObjectTempSlots()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _objectTempSlots;
   }
uint32_t
OMR::ResolvedMethodSymbol::setObjectTempSlots(uint32_t s)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return (_objectTempSlots = s);
   }

bool
OMR::ResolvedMethodSymbol::containsOnlySinglePrecision()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _methodFlags.testAny(OnlySinglePrecision);
   }
void
OMR::ResolvedMethodSymbol::setContainsOnlySinglePrecision(bool b)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   _methodFlags.set(OnlySinglePrecision, b);
   }

bool
OMR::ResolvedMethodSymbol::usesSinglePrecisionMode()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _methodFlags.testAny(SinglePrecisionMode);
   }
void
OMR::ResolvedMethodSymbol::setUsesSinglePrecisionMode(bool b)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   _methodFlags.set(SinglePrecisionMode, b);
   }

bool
OMR::ResolvedMethodSymbol::isNoTemps()
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   return _methodFlags.testAny(NoTempsSet);
   }
void
OMR::ResolvedMethodSymbol::setNoTemps(bool b)
   {
   TR_ASSERT(self()->isJittedMethod(), "Should have been created as a jitted method.");
   _methodFlags.set(NoTempsSet, b);
   }

/**
 * \brief Returns the bytecode that represents the start of the ilgen created basic block
 *
 * This function takes a bytecode index and looks up to find the stashed bytecode
 * index which corresponds to the start of the basic block in which bytecodeIndex
 * was generated at ilgen time. This is useful when trying to reassociate profiling
 * information when blocks have been split due to inlining
 *
 *  @param bytecodeIndex The bytecode of the instruction whose basic block start is to be found
 *  @return The bytecode index of the start of the containing ilgen basic block, -1 if unknown
 */
int32_t OMR::ResolvedMethodSymbol::getProfilingByteCodeIndex(int32_t bytecodeIndex)
   {
   for (auto itr = _bytecodeProfilingOffsets.begin(), end = _bytecodeProfilingOffsets.end(); itr != end; ++itr)
      {
      if (itr->first >= bytecodeIndex)
         return itr->second.first;
      }
   return -1;
   }

/**
 * \brief Save the profiling frequency of a given bytecode index
 *
 * This function stashes a profiling frequency against a given bytecode index for later retrieval
 * (usually as part of profiling re-association.
 *
 * @param bytecodeIndex The bytecode index of the start of the block whose frequency we wish to save
 * @param frequency The normalized block frequency to save for later use
 */
void OMR::ResolvedMethodSymbol::setProfilerFrequency(int32_t bytecodeIndex, int32_t frequency)
   {
   for (auto itr = _bytecodeProfilingOffsets.begin(), end = _bytecodeProfilingOffsets.end(); itr != end; ++itr)
      {
      if (itr->first >= bytecodeIndex)
         itr->second.second = frequency;
      }
   }

/**
 * \brief Return the saved profiling frequency for the given bytecode index
 *
 * This function returns a profiling frequency that was saved against a given byte code index for
 * later retrieval.
 *
 * @param bytecodeIndex The bytecode index of the start of the block whose frequency we wish to retrieve
 * @return The normalized saved frequency, -1 if unknown
 */
int32_t OMR::ResolvedMethodSymbol::getProfilerFrequency(int32_t bytecodeIndex)
   {
   for (auto itr = _bytecodeProfilingOffsets.begin(), end = _bytecodeProfilingOffsets.end(); itr != end; ++itr)
      {
      if (itr->first >= bytecodeIndex)
         return itr->second.second;
      }
   return -1;
   }

static bool compareProfilingOffsetInfo(const std::pair<int32_t, int32_t> &first, const std::pair<int32_t, int32_t> &second)
   {
   return first.first < second.first;
   }

/**
 * \brief Save the bytecode range of a basic block during ilgen
 *
 * @param startBCI The bytecode index of the start of the basic block
 * @param endBCI The bytecode index of the end of the basic block
 */
void OMR::ResolvedMethodSymbol::addProfilingOffsetInfo(int32_t startBCI, int32_t endBCI)
   {
   for (auto itr = _bytecodeProfilingOffsets.begin(), end = _bytecodeProfilingOffsets.end(); itr != end; ++itr)
       {
       if (itr->first >= endBCI)
          {
          _bytecodeProfilingOffsets.insert(itr, std::make_pair(endBCI, std::make_pair(startBCI, -1)));
          return;
          }
       }
   _bytecodeProfilingOffsets.push_back(std::make_pair(endBCI, std::make_pair(startBCI, -1)));
   }

/**
 * \brief Print the saved basic block bytecode ranges and associated block frequencies
 *
 * @param comp The compilation object
 */
void OMR::ResolvedMethodSymbol::dumpProfilingOffsetInfo(TR::Compilation *comp)
   {
   for (auto itr = _bytecodeProfilingOffsets.begin(), end = _bytecodeProfilingOffsets.end(); itr != end; ++itr)
       traceMsg(comp, "  %d:%d\n", itr->first, itr->second.first);
   }

/**
 * \brief Clear the saved basic block and profiling information
 */
void OMR::ResolvedMethodSymbol::clearProfilingOffsetInfo()
   {
   _bytecodeProfilingOffsets.clear();
   }

//Explicit instantiations

template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::create(TR_StackMemory m, TR_ResolvedMethod * rm, TR::Compilation * comp);
template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::create(TR_HeapMemory m, TR_ResolvedMethod * rm, TR::Compilation * comp);
template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::create(PERSISTENT_NEW_DECLARE m, TR_ResolvedMethod * rm, TR::Compilation * comp);

template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::createJittedMethodSymbol(TR_StackMemory         m, TR_ResolvedMethod * method, TR::Compilation * comp);
template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::createJittedMethodSymbol(TR_HeapMemory          m, TR_ResolvedMethod * method, TR::Compilation * comp);
template TR::ResolvedMethodSymbol * OMR::ResolvedMethodSymbol::createJittedMethodSymbol(PERSISTENT_NEW_DECLARE m, TR_ResolvedMethod * method, TR::Compilation * comp);
