/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

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
#include "infra/TRCfgEdge.hpp"                  // for CFGEdge
#include "infra/TRCfgNode.hpp"                  // for CFGNode
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
   self()->getParameterList()._trMemory = comp->trMemory();
   self()->getAutomaticList()._trMemory = comp->trMemory();

   self()->getVariableSizeSymbolList()._trMemory = comp->trMemory();
   self()->getTrivialDeadTreeBlocksList()._trMemory = comp->trMemory();
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
     _shouldNotAttemptOSR(NULL),
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
   if (_methodIndex > JITTED_METHOD_INDEX && !_resolvedMethod->isSameMethod(comp->getJittedMethodSymbol()->getResolvedMethod()) || comp->isDLT())
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
   if (comp->getMethodSymbol() == self())
      {
      List<TR::ParameterSymbol>* l = comp->cg()->getLinkage()->getMainBodyLogicalParameterList();
      if (l == NULL)
         {
         return self()->getParameterList();
         }
      else
         {
         return *l;
         }
      }
   else
      {
      return self()->getParameterList();
      }
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




TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCallNode(TR::TreeTop* insertionPoint,
                                              int32_t numChildren,
                                              bool copyChildren,
                                              bool shouldSplitBlock)
   {
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
       firstHalfBlock->split(insertionPoint, self()->comp()->getFlowGraph(), true);

   int32_t firstArgIndex = 0;
   if ((refNode->getNumChildren() > 0) &&
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

   if (copyChildren)
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
OMR::ResolvedMethodSymbol::induceOSRAfter(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, TR::TreeTop* branch, bool extendRemainder)
   {
   TR::Block *block = insertionPoint->getEnclosingBlock();

   if (self()->supportsInduceOSR(induceBCI, block, NULL, self()->comp()))
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

      // create a block that will be the target of the branch with BCI for the induceOSR
      TR::Block *osrBlock = TR::Block::createEmptyBlock(self()->comp(), MAX_COLD_BLOCK_COUNT);
      osrBlock->setIsCold();
      osrBlock->getEntry()->getNode()->setByteCodeInfo(induceBCI);
      osrBlock->getExit()->getNode()->setByteCodeInfo(induceBCI);

      cfg->findLastTreeTop()->join(osrBlock->getEntry());
      cfg->addNode(osrBlock);
      cfg->addEdge(block, osrBlock);

      if (self()->comp()->getOption(TR_TraceOSR))
         traceMsg(self()->comp(), "  Created OSR block_%d and inserting it at the end of the method\n", osrBlock->getNumber());

      branch->getNode()->setBranchDestination(osrBlock->getEntry());
      block->append(branch);
      cfg->copyExceptionSuccessors(block, osrBlock);

      // induce OSR in the new block
      self()->genInduceOSRCallAndCleanUpFollowingTreesImmediately(osrBlock->getExit(), induceBCI, false, false, self()->comp());
      return true;
      }
   return false;
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::induceImmediateOSRWithoutChecksBefore(TR::TreeTop *insertionPoint)
   {
   if (self()->supportsInduceOSR(insertionPoint->getNode()->getByteCodeInfo(), insertionPoint->getEnclosingBlock(), NULL, self()->comp()))
      return self()->genInduceOSRCallAndCleanUpFollowingTreesImmediately(insertionPoint, insertionPoint->getNode()->getByteCodeInfo(), false, false, self()->comp());
   if (self()->comp()->getOption(TR_TraceOSR))
      traceMsg(self()->comp(), "induceImmediateOSRWithoutChecksBefore n%dn failed - supportsInduceOSR returned false\n", insertionPoint->getNode()->getGlobalIndex());
   return NULL;
   }

TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCallAndCleanUpFollowingTreesImmediately(TR::TreeTop *insertionPoint, TR_ByteCodeInfo induceBCI, bool copyChildren, bool shouldSplitBlock, TR::Compilation *comp)
   {
   int32_t numChildrenOfInduceCall = 0;
   int32_t firstArgChild = 0;
   int32_t numOfChildren = 0;
   TR::Node *refCallNode = NULL;
   if ((insertionPoint->getNode()->getNumChildren() > 0) &&
       (insertionPoint->getNode()->getFirstChild()->getOpCode().isCall()))
      {
      refCallNode = insertionPoint->getNode()->getFirstChild();
      firstArgChild = refCallNode->getFirstArgumentIndex();
      numOfChildren = refCallNode->getNumChildren();
      numChildrenOfInduceCall = numOfChildren - firstArgChild;
      }

   TR::TreeTop *induceOSRCallTree = self()->genInduceOSRCall(insertionPoint, induceBCI.getCallerIndex(), numChildrenOfInduceCall, copyChildren, shouldSplitBlock);

   if (induceOSRCallTree)
      {
      TR::TreeTop *treeAfterInduceOSRCall = induceOSRCallTree->getNextTreeTop();

      if (refCallNode)
         {
         TR::Node *induceOSRCallNode = induceOSRCallTree->getNode()->getFirstChild();
         for (int32_t i = firstArgChild; i < numOfChildren; i++)
            induceOSRCallNode->setAndIncChild(i-firstArgChild, refCallNode->getChild(i));
         }

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


TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCallForGuardedCallee(TR::TreeTop* insertionPoint,
                                                          TR::ResolvedMethodSymbol *calleeSymbol,
                                                          int32_t numChildren,
                                                          bool copyChildren,
                                                          bool shouldSplitBlock)
   {
   if (self()->comp()->getOption(TR_TraceOSR))
      {
      const char * mSignature = calleeSymbol->signature(self()->comp()->trMemory());
      traceMsg(self()->comp(), "induce %p generated for %s index %d\n",
                      insertionPoint->getNode(), mSignature, insertionPoint->getNode()->getByteCodeInfo().getCallerIndex());
      }

   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findCallerOSRMethodData(self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(self()->comp()->getCurrentInlinedSiteIndex(), calleeSymbol));
   return self()->genInduceOSRCall(insertionPoint, self()->comp()->getCurrentInlinedSiteIndex(), osrMethodData, calleeSymbol, numChildren, copyChildren, shouldSplitBlock);
   }


TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCall(TR::TreeTop* insertionPoint, int32_t inlinedSiteIndex,
                        int32_t numChildren, bool copyChildren, bool shouldSplitBlock)
   {
   TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, self());
   return self()->genInduceOSRCall(insertionPoint, inlinedSiteIndex, osrMethodData, NULL, numChildren, copyChildren, shouldSplitBlock);
   }


TR::TreeTop *
OMR::ResolvedMethodSymbol::genInduceOSRCall(TR::TreeTop* insertionPoint,
                                          int32_t inlinedSiteIndex,
                                          TR_OSRMethodData *osrMethodData,
                                          TR::ResolvedMethodSymbol *callSymbolForDeadSlots,
                                          int32_t numChildren,
                                          bool copyChildren,
                                          bool shouldSplitBlock)
   {
   TR::CFG * callerCFG = self()->comp()->getFlowGraph();
   TR::Node *insertionPointNode = insertionPoint->getNode();
   if (self()->comp()->getOption(TR_TraceOSR))
      traceMsg(self()->comp(), "Osr point added for %p, callerIndex=%d, bcindex=%d\n",
              insertionPointNode, insertionPointNode->getByteCodeInfo().getCallerIndex(),
              insertionPointNode->getByteCodeInfo().getByteCodeIndex());

   TR::Block * OSRCatchBlock = osrMethodData->getOSRCatchBlock();
   TR::TreeTop *induceOSRCallTree = self()->genInduceOSRCallNode(insertionPoint, numChildren, copyChildren, shouldSplitBlock);

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

   if (firstOSRPoint)
      {
      TR::Block *OSRCodeBlock = osrMethodData->getOSRCodeBlock();
      TR::Block *OSRCatchBlock = osrMethodData->getOSRCatchBlock();

      if (self()->comp()->getOption(TR_TraceOSR))
         traceMsg(self()->comp(), "code %p %d catch %p %d\n", OSRCodeBlock, OSRCodeBlock->getNumber(), OSRCatchBlock, OSRCatchBlock->getNumber());

      self()->getLastTreeTop()->insertTreeTopsAfterMe(OSRCatchBlock->getEntry(), OSRCodeBlock->getExit());
      self()->genOSRHelperCall(inlinedSiteIndex, self()->comp()->getSymRefTab());
      }

   self()->insertRematableStoresFromCallSites(self()->comp(), inlinedSiteIndex, induceOSRCallTree);
   self()->insertStoresForDeadStackSlotsBeforeInducingOSR(self()->comp(), inlinedSiteIndex, insertionPoint->getNode()->getByteCodeInfo(), induceOSRCallTree, callSymbolForDeadSlots);
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

         // Generate an OSR point for the induction point, ignoring whether it is after or not
         TR_ByteCodeInfo offsetBCI = ttnode->getByteCodeInfo();
         offsetBCI.setByteCodeIndex(offsetBCI.getByteCodeIndex() + self()->comp()->getOSRInductionOffset(ttnode));
         osrPoint = new (self()->comp()->trHeapMemory()) TR_OSRPoint(offsetBCI, osrMethodData, self()->comp()->trMemory());
         osrPoint->setOSRIndex(self()->addOSRPoint(osrPoint));
         if (self()->comp()->getOption(TR_TraceOSR))
            traceMsg(self()->comp(), "offset osr point added for [%p] at offset bci %d:%d\n",
               ttnode, offsetBCI.getCallerIndex(), offsetBCI.getByteCodeIndex());

         // Generate an OSR point for the analysis point, if needed
         if (self()->comp()->requiresAnalysisOSRPoint(ttnode))
            {
            osrPoint = new (self()->comp()->trHeapMemory()) TR_OSRPoint(ttnode->getByteCodeInfo(), osrMethodData, self()->comp()->trMemory());
            osrPoint->setOSRIndex(self()->addOSRPoint(osrPoint));
            if (self()->comp()->getOption(TR_TraceOSR))
               traceMsg(self()->comp(), "osr point added for [%p] at %d:%d\n",
                  ttnode, ttnode->getByteCodeInfo().getCallerIndex(),
                  ttnode->getByteCodeInfo().getByteCodeIndex());
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

   //if an OSR code block has been created, attach its treetops and osr catch block's
   //treetops to the method's treetops
   if (!self()->getOSRPoints().isEmpty())
      {
      TR_OSRMethodData *osrMethodData = self()->comp()->getOSRCompilationData()->findOrCreateOSRMethodData(currentInlinedSiteIndex, self());
      TR::Block *OSRCodeBlock = osrMethodData->getOSRCodeBlock();
      TR::Block *OSRCatchBlock = osrMethodData->getOSRCatchBlock();
      lastTreeTop->insertTreeTopsAfterMe(OSRCatchBlock->getEntry(), OSRCodeBlock->getExit());
      }
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
OMR::ResolvedMethodSymbol::genOSRHelperCall(int32_t currentInlinedSiteIndex, TR::SymbolReferenceTable* symRefTab)
   {
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

   bool disableOSRwithTM = feGetEnv("TR_disableOSRwithTM") ? true: false;
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
   self()->comp()->getFlowGraph()->addEdge(OSRCodeBlock, self()->comp()->getFlowGraph()->getEnd());
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
            _shouldNotAttemptOSR = new (comp->trHeapMemory()) TR_BitVector(1, comp->trMemory(), heapAlloc, growable);
            }

         if (_tempIndex == -1)
            self()->setParameterList();
         _tempIndex = _firstJitTempIndex;
         //_automaticList.setListHead(0); // what's the point ? sym ref tab can use the sym refs anyway by using methodSymbol's auto sym refs and pending push sym refs list; this only confuses analyses into thinking these autos cannot be used when they actually can be

         TR_IlGenerator *ilGen = customRequest.getIlGenerator(self(), fe, comp, symRefTab);

         auto genIL_rc = ilGen->genIL();
         _methodFlags.set(IlGenSuccess, genIL_rc);
         traceMsg(self()->comp(), "genIL() returned %d\n", genIL_rc);

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
            bool doOSR =
               comp->getOption(TR_EnableOSR) && comp->supportsInduceOSR() &&
               !comp->isPeekingMethod()  && (comp->getOption(TR_EnableNextGenHCR) || !comp->getOption(TR_EnableHCR)) ;

            optimizer = TR::Optimizer::createOptimizer(comp, self(), true);
            previousOptimizer = comp->getOptimizer();
            comp->setOptimizer(optimizer);

            self()->detectInternalCycles(comp->getFlowGraph(), comp);

            if (doOSR)
               {
               TR_ASSERT(comp->getOSRCompilationData(), "OSR compilation data is NULL\n");
               self()->genAndAttachOSRCodeBlocks(comp->getCurrentInlinedSiteIndex());
               if (!self()->getOSRPoints().isEmpty())
                  {
                  self()->genOSRHelperCall(comp->getCurrentInlinedSiteIndex(), symRefTab);
                  if (comp->getOption(TR_TraceOSR))
                     comp->dumpMethodTrees("Trees after OSR in genIL", self());
                  }

               if (!comp->isOutermostMethod())
                  self()->cleanupUnreachableOSRBlocks(comp->getCurrentInlinedSiteIndex(), comp);
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
OMR::ResolvedMethodSymbol::supportsInduceOSR(TR_ByteCodeInfo bci,
                                           TR::Block *blockToOSRAt,
                                           TR::ResolvedMethodSymbol *calleeSymbolIfCallNode,
                                           TR::Compilation *comp,
                                           bool runCleanup)
   {
   if (!comp->supportsInduceOSR())
      return false;

   if (self()->cannotAttemptOSR(bci, blockToOSRAt, calleeSymbolIfCallNode, comp, runCleanup))
      return false;

   return true;
   }


void
OMR::ResolvedMethodSymbol::setCannotAttemptOSR(int32_t n)
   {
   _cannotAttemptOSR->set(n);
   }


void
OMR::ResolvedMethodSymbol::setShouldNotAttemptOSR(int32_t n)
   {
   _shouldNotAttemptOSR->set(n);
   }


bool
OMR::ResolvedMethodSymbol::cannotAttemptOSR(TR_ByteCodeInfo bci,
                                          TR::Block *blockToOSRAt,
                                          TR::ResolvedMethodSymbol *calleeSymbolIfCallNode,
                                          TR::Compilation *comp,
                                          bool runCleanup)
   {
   if (comp->getOption(TR_TraceOSR))
      traceMsg(comp, "Checking if OSR can be attempted at bytecode index %d\n", bci.getByteCodeIndex());

   if (!comp->canAffordOSRControlFlow())
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Cannot afford OSR control flow\n");
      return true;
      }

   if (_shouldNotAttemptOSR->get(bci.getByteCodeIndex()))
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Should not attempt OSR at this bytecode\n");
      return true;
      }

   if (bci.doNotProfile())
     {
     if (comp->getOption(TR_TraceOSR))
        traceMsg(comp, "Node was not present at IL gen time\n");

     return true; // if this is a call node created by the optimizer (string peepholes) it may not exist in the bytecodes
     }

   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
   int32_t callSite = bci.getCallerIndex();
   int32_t byteCodeIndex = bci.getByteCodeIndex();

   TR_OSRMethodData *osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callSite, self());
   TR::Block * OSRCatchBlock = osrMethodData->getOSRCatchBlock();

   // Walk up the inlined call stack and safety check every call to see if OSR is safe - note that we only check
   // cannotOSR in this case as we only abort the OSR if correctness is in jeopardy
   while (osrMethodData)
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Checking if OSR can be attempted at caller index %d and bytecode index %d\n", callSite, byteCodeIndex);

      auto *callerSymbol = osrMethodData->getMethodSymbol();

      if (callerSymbol->_cannotAttemptOSR->get(byteCodeIndex))
        {
        if (comp->getOption(TR_TraceOSR))
           traceMsg(comp, "Cannot attempt OSR at this bytecode in caller\n");
        return true;
        }

      TR_ASSERT((callSite == osrMethodData->getInlinedSiteIndex()), "OSR method data and inlined call sites array being walked out of sync\n");
      if (osrMethodData->getInlinedSiteIndex() > -1)
         {
         TR_InlinedCallSite &callSiteInfo = comp->getInlinedCallSite(callSite);
         callSite = callSiteInfo._byteCodeInfo.getCallerIndex();
         if (callSiteInfo._byteCodeInfo.doNotProfile())
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "Node in caller was not present at IL gen time\n");

            return true;
            }

         osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
         if (!osrMethodData)
            {
            if (comp->getOption(TR_TraceOSR))
               traceMsg(comp, "OSR method data is NULL\n");

            return true;
            }

         byteCodeIndex = callSiteInfo._byteCodeInfo.getByteCodeIndex();
         }
      else
         {
         osrMethodData = NULL;
         }
      }

   auto *symToCheckOSRBlocksReachability = self();
   if (calleeSymbolIfCallNode)
      symToCheckOSRBlocksReachability = calleeSymbolIfCallNode;

   if (comp->getMethodSymbol() != symToCheckOSRBlocksReachability
       && !symToCheckOSRBlocksReachability->allCallerOSRBlocksArePresent(callSite, comp))
      {
      if (runCleanup)
         symToCheckOSRBlocksReachability->cleanupUnreachableOSRBlocks(callSite, comp);
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Some caller OSR blocks are not present\n");

      return true;
      }

   if (blockToOSRAt && (!OSRCatchBlock || !blockToOSRAt->hasExceptionSuccessor(OSRCatchBlock)))
      {
      if (comp->getOption(TR_TraceOSR) && OSRCatchBlock)
         traceMsg(comp, "Missing OSR exception successor block_%d for block_%d - cannot OSR\n", OSRCatchBlock->getNumber(), blockToOSRAt->getNumber());
      if (comp->getOption(TR_TraceOSR) && !OSRCatchBlock)
         traceMsg(comp, "Missing OSR exception successor for block_%d - cannot OSR\n", blockToOSRAt->getNumber());
      return true;
      }

   if (comp->getOption(TR_TraceOSR))
     traceMsg(comp, "OSR can be attempted\n");

   return false;
   }

bool
OMR::ResolvedMethodSymbol::allCallerOSRBlocksArePresent(int32_t inlinedSiteIndex, TR::Compilation *comp)
   {
   TR_OSRMethodData *osrMethodData = inlinedSiteIndex > -1 ? comp->getOSRCompilationData()->findCallerOSRMethodData(comp->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, self())) : NULL;
   TR::Block * OSRCatchBlock = osrMethodData ? osrMethodData->getOSRCatchBlock() : NULL;

   bool allCallersOSRCodeBlocksAreStillInCFG = true;
   while (osrMethodData)
      {
      TR::Block * CallerOSRCodeBlock = osrMethodData->getOSRCodeBlock();
      if (!CallerOSRCodeBlock ||
          CallerOSRCodeBlock->isUnreachable())
         {
         if (comp->getOption(TR_TraceOSR))
            traceMsg(comp, "Osr catch block at inlined site index %d is absent\n", osrMethodData->getInlinedSiteIndex());

         return false;
         }
      else if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Osr catch block at inlined site index %d is present\n", osrMethodData->getInlinedSiteIndex());

      if (osrMethodData->getInlinedSiteIndex() > -1)
         osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
      else
         osrMethodData = NULL;
      }
   return true;
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
   TR_ByteCodeInfo &nodeBCI = node->getByteCodeInfo();
   if (node->getNumChildren() > 0 && (node->getOpCodeValue() == TR::treetop || node->getOpCode().isCheck()))
      nodeBCI = node->getFirstChild()->getByteCodeInfo();
   return nodeBCI;
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
   if (self()->comp()->getOSRTransitionTarget() == TR::postExecutionOSR)
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

void
OMR::ResolvedMethodSymbol::insertStoresForDeadStackSlotsBeforeInducingOSR(TR::Compilation *comp, int32_t inlinedSiteIndex, TR_ByteCodeInfo &byteCodeInfo, TR::TreeTop *induceOSRTree, TR::ResolvedMethodSymbol *callSymbolForDeadSlots)
   {
   if (!comp->osrStateIsReliable())
      {
      traceMsg(comp, "OSR state may not be reliable enough to trust liveness info computed at IL gen time; so avoiding dead stack slot store insertion\n");
      return;
      }

   TR::SymbolReferenceTable *symRefTab = comp->getSymRefTab();
   int32_t callSite = byteCodeInfo.getCallerIndex();
   int32_t byteCodeIndex = byteCodeInfo.getByteCodeIndex();
   TR_OSRMethodData *osrMethodData;
   if (callSymbolForDeadSlots)
      osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(comp->getOSRCompilationData()->findOrCreateOSRMethodData(inlinedSiteIndex, callSymbolForDeadSlots));
   else
      osrMethodData = comp->getOSRCompilationData()->findOrCreateOSRMethodData(callSite, self());

   traceMsg(comp, "About to do dead store insertion using osrMethodData %p - %d:%d\n", osrMethodData, callSite, byteCodeIndex);

   TR::TreeTop *prev = induceOSRTree->getPrevTreeTop();
   TR::TreeTop *next = induceOSRTree;

   TR::OSRPointType pointType = TR::inductionOSR;

   while (osrMethodData)
      {
      if (comp->getOption(TR_TraceOSR))
         traceMsg(comp, "Inserting stores for dead stack slots in method at caller index %d and bytecode index %d for induceOSR call %p\n", callSite, byteCodeIndex, induceOSRTree->getNode());

      TR_BitVector *deadSymRefs = osrMethodData->getLiveRangeInfo(byteCodeIndex, pointType);
      if (deadSymRefs)
         {
         TR_BitVectorIterator bvi(*deadSymRefs);
         while (bvi.hasMoreElements())
            {
            int32_t nextDeadVar = bvi.getNextElement();
            TR::SymbolReference *nextSymRef = symRefTab->getSymRef(nextDeadVar);

            if (performTransformation(comp, "OSR LIVE RANGE ANALYSIS : Local %d is reset before induceOSR call tree %p (caller index %d bytecode index %d)\n", nextSymRef->getReferenceNumber(), induceOSRTree->getNode(), callSite, byteCodeIndex))
               {
               TR::Node *storeNode = TR::Node::createWithSymRef(comp->il.opCodeForDirectStore(nextSymRef->getSymbol()->getDataType()), 1, 1,
                                                    TR::Node::createConstDead(induceOSRTree->getNode(), nextSymRef->getSymbol()->getDataType()), nextSymRef);
               storeNode->setStoredValueIsIrrelevant(true);
               TR::TreeTop *storeTree = TR::TreeTop::create(comp, storeNode);
               prev->join(storeTree);
               storeTree->join(next);
               prev = storeTree;
               }
            }
         }

      TR_ASSERT((callSite == osrMethodData->getInlinedSiteIndex()), "OSR method data and inlined call sites array being walked out of sync\n");
      if (osrMethodData->getInlinedSiteIndex() > -1)
         {
         TR_InlinedCallSite &callSiteInfo = comp->getInlinedCallSite(callSite);
         callSite = callSiteInfo._byteCodeInfo.getCallerIndex();
         osrMethodData = comp->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
         byteCodeIndex = callSiteInfo._byteCodeInfo.getByteCodeIndex();
         pointType = comp->getOSRTransitionTarget() == TR::postExecutionOSR ? TR::analysisOSR : TR::inductionOSR;
         }
      else
         osrMethodData = NULL;
      }
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

   (*_autoSymRefs)[slot]._trMemory = m;

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

   (*_pendingPushSymRefs)[slot]._trMemory = m;

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
                     if (comp->getOSRTransitionTarget() == TR::postExecutionOSR)
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
