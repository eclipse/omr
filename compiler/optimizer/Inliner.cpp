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

#include "optimizer/Inliner.hpp"

#include <algorithm>                                      // for std::max, etc
#include <assert.h>                                       // for assert
#include <limits.h>                                       // for INT_MAX
#include <stdarg.h>                                       // for va_list, etc
#include <stddef.h>                                       // for NULL, size_t
#include <stdint.h>                                       // for int32_t, etc
#include <stdio.h>                                        // for printf, fflush, etc
#include <stdlib.h>                                       // for atoi, atof
#include <string.h>                                       // for strncmp, etc
#include <exception>
#include <vector>                                         // for std::vector
#include "codegen/CodeGenerator.hpp"                      // for CodeGenerator
#include "codegen/FrontEnd.hpp"                           // for TR_FrontEnd, etc
#include "env/KnownObjectTable.hpp"
#include "codegen/Linkage.hpp"                            // for Linkage
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"                        // for Compilation
#include "compile/InlineBlock.hpp"
#include "compile/Method.hpp"                             // for TR_Method
#include "compile/OSRData.hpp"                            // for HCRMode, etc
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"                    // for TR::Options, etc
#include "control/Recompilation.hpp"
#include "cs2/allocator.h"
#include "cs2/bitvectr.h"
#include "cs2/sparsrbit.h"
#include "env/ClassEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"                            // for ObjectModel
#include "env/PersistentInfo.hpp"                         // for PersistentInfo
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                                   // for Block
#include "il/DataTypes.hpp"                               // for DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                   // for ILOpCode, etc
#include "il/Node.hpp"                                    // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                                  // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                                 // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"                     // for MethodSymbol
#include "il/symbol/ParameterSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"                     // for StaticSymbol
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Array.hpp"                                // for TR_Array
#include "infra/Assert.hpp"                               // for TR_ASSERT
#include "infra/BitVector.hpp"                            // for TR_BitVector
#include "infra/Cfg.hpp"                                  // for CFG
#include "infra/HashTab.hpp"
#include "infra/Link.hpp"                                 // for TR_LinkHead, etc
#include "infra/List.hpp"                                 // for ListIterator, etc
#include "infra/Random.hpp"
#include "infra/SimpleRegex.hpp"
#include "infra/Stack.hpp"                                // for TR_Stack
#include "infra/CfgEdge.hpp"                              // for CFGEdge
#include "infra/CfgNode.hpp"                              // for CFGNode
#include "optimizer/CallInfo.hpp"                         // for TR_CallTarget, etc
#include "optimizer/InlinerFailureReason.hpp"
#include "optimizer/Optimization.hpp"                     // for Optimization
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                        // for Optimizer
#include "optimizer/PreExistence.hpp"                     // for TR_PrexArgInfo, etc
#include "optimizer/RematTools.hpp"                       // for RematTools, RematSafetyInfo
#include "optimizer/Structure.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "ras/Debug.hpp"                                  // for TR_DebugBase, etc
#include "ras/DebugCounter.hpp"
#include "ras/LogTracer.hpp"                              // for heuristicTrace, etc
#include "runtime/Runtime.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "env/VMJ9.h"
#endif



namespace TR { class SimpleRegex; }

int32_t  *NumInlinedMethods   = NULL;
int32_t  *InlinedSizes        = NULL;

#define OPT_DETAILS "O^O INLINER: "

// == Hack markers ==

// To conserve owning method indexes, we share TR_ResolvedMethods even where
// the owning method differs.  Usually that is done only for methods that the
// inliner never sees, but if we get aggressive and share TR_ResolvedMethods
// that are exposed to the inliner, then the inliner needs to make sure it
// doesn't rely on the "owning method" accurately representing the calling method.
//
// This is a fragile design that needs some more thought.  We should either
// eliminate the limitations that motivate sharing in the first place, or else
// take the time to modify inliner (and everyone else) so it doesn't rely on
// owning method information to indicate the caller.
//
// For now, we mark such code with this macro.
//
#define OWNING_METHOD_MAY_NOT_BE_THE_CALLER (1)

#define MIN_NUM_CALLERS 20
#define MIN_FAN_IN_SIZE 50
#define SIZE_MULTIPLIER 4
#define FANIN_OTHER_BUCKET_THRESHOLD 0.5

#undef TRACE_CSI_IN_INLINER

bool rematerializeConstant(TR::Node *node, TR::Compilation *comp);

static bool isWarm(TR::Compilation *comp)
   {
   return comp->getMethodHotness() >= warm;
   }
static bool isHot(TR::Compilation *comp)
   {
   return comp->getMethodHotness() >= hot;
   }
static bool isScorching(TR::Compilation *comp)
   {
   return ((comp->getMethodHotness() >= scorching) || ((comp->getMethodHotness() >= veryHot) && comp->isProfilingCompilation())) ;
   }


static bool succAndPredAreNotOSRBlocks(TR::CFGEdge * edge)
   {
   return
      !toBlock(edge->getTo())->isOSRCodeBlock() &&
      !toBlock(edge->getFrom())->isOSRCodeBlock() &&
      !toBlock(edge->getTo())->isOSRCatchBlock() &&
      !toBlock(edge->getFrom())->isOSRCatchBlock();
   }

int32_t
OMR_InlinerPolicy::getInitialBytecodeSize(TR::ResolvedMethodSymbol * methodSymbol, TR::Compilation *comp)
   {
   return getInitialBytecodeSize(methodSymbol->getResolvedMethod(),methodSymbol, comp);
   }

int32_t
OMR_InlinerPolicy::getInitialBytecodeSize(TR_ResolvedMethod *feMethod, TR::ResolvedMethodSymbol * methodSymbol, TR::Compilation *comp)
   {
   int32_t size = feMethod->maxBytecodeIndex();
   if (!comp->getOption(TR_DisableAdaptiveDumbInliner))
      {
      if (methodSymbol && !methodSymbol->mayHaveInlineableCall() && size <= 5) // favor the inlining of methods that are very small
         size = 1;
      }
   TR_ASSERT(size > 0, "size cannot be zero because it is used as a divisor");
   return size;
   }

TR::DataType getStoreType(TR::Node *store, TR::Symbol *addrSymbol, TR::Compilation *comp)
   {
   TR::DataType storeType = addrSymbol->getDataType();
   return storeType;
   }

TR_TrivialInliner::TR_TrivialInliner(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_TrivialInliner::perform()
   {

   comp()->generateAccurateNodeCount();

   TR::ResolvedMethodSymbol * sym = comp()->getMethodSymbol();
   if (sym->mayHaveInlineableCall() && optimizer()->isEnabled(OMR::inlining))
      {
      uint32_t initialSize = comp()->getOptions()->getTrivialInlinerMaxSize();;
      if (comp()->getOption(TR_Randomize) || comp()->getOption(TR_VerbosePseudoRandom))
         {
         // In the following we ask the global cmdLineOptions about the getTrivialInlinerMaxSize()
         // because the result of convertNonDeterministicInput is cached the first time is executed
         // and don't want this result to be determined by the first compilation that comes through
         static uint32_t defaultRandomSize = comp()->convertNonDeterministicInput(TR::Options::getCmdLineOptions()->getTrivialInlinerMaxSize(), 30, randomGenerator(), 0);
         initialSize = defaultRandomSize;
         }

      initialSize = isHot(comp()) ? initialSize * 2 : initialSize;
      TR_DumbInliner inliner(optimizer(), this, initialSize);
      inliner.performInlining(sym);
      }

   comp()->setSupressEarlyInlining(false);
   return 1; // cost??
   }

const char *
TR_TrivialInliner::optDetailString() const throw()
   {
   return "O^O TRIVIAL INLINER: ";
   }


//---------------------------------------------------------------------
// TR_InlinerBase
//---------------------------------------------------------------------



TR_InlinerBase::TR_InlinerBase(TR::Optimizer * optimizer, TR::Optimization *optimization)
   : TR_HasRandomGenerator(optimizer->comp()),
     _visitCount(optimizer->comp()->incVisitCount()),
     _inliningAsWeWalk(false),
     _optimizer(optimizer),
     _trMemory(optimizer->comp()->trMemory()),
     _availableTemps(_trMemory),
     _availableBasicBlockTemps(_trMemory),
     _storeToCachedPrivateStatic(NULL),
     _disableTailRecursion(false),
     _currentNumberOfNodes(optimizer->comp()->getAccurateNodeCount()),
     _disableInnerPrex(false),
     _aggressivelyInlineInLoops(false),
     _GlobalLabels(_trMemory)
   {
   _policy = optimization->manager()->getOptPolicy() ? static_cast<OMR_InlinerPolicy*>(optimization->manager()->getOptPolicy()) : optimizer->getInlinerPolicy();
   _util = optimizer->getInlinerUtil();
   _policy->setInliner(this);
   _util->setInliner(this);
   if (!optimizer->isEnabled(OMR::innerPreexistence))
      _disableInnerPrex = true;

   setInlineVirtuals(true);
   if (optimizer->getInlineSynchronized())
      setInlineSynchronized(true);

   _GlobalLabels.init();

   _tracer = _util->getInlinerTracer(optimization);

   _maxRecursiveCallByteCodeSizeEstimate=0;
   _callerWeightLimit=0;
   _methodByteCodeSizeThreshold=0;
   _methodInColdBlockByteCodeSizeThreshold=0;
   _methodInWarmBlockByteCodeSizeThreshold=0;
   _nodeCountThreshold=0;
   _maxInliningCallSites=0;
   _numAsyncChecks=0;
   _isInLoop = false;

   _EDODisableInlinedProfilingInfo = false;

   setInlineThresholds(comp()->getMethodSymbol());
   }

void
TR_InlinerBase::setSizeThreshold(uint32_t size)
   {
   // This method is used to increase the threshold when inlining from places other than the Inliner. ex escape analysis, value prop, etc.

   _methodByteCodeSizeThreshold = size;

   alwaysTrace(tracer(),"Setting method size threshold (_methodByteCodeSizeThreshold) to %d\n",_methodByteCodeSizeThreshold);
   }

void
OMR_InlinerPolicy::determineInliningHeuristic(TR::ResolvedMethodSymbol *callerSymbol)
   {
   return;
   }

void
TR_InlinerBase::setInlineThresholds(TR::ResolvedMethodSymbol *callerSymbol)
   {
   int32_t size = getPolicy()->getInitialBytecodeSize(callerSymbol, comp());

   getPolicy()->determineInliningHeuristic(callerSymbol);

   //
   // !!!!! Setting default thresholds for compilation.
   //

   if (isScorching(comp()))     _callerWeightLimit = std::max(1500, size * 2);
   else if (isHot(comp()))      _callerWeightLimit = std::max(1500, size + (size >> 2));
   else if (size < 125)         _callerWeightLimit = 250;
   else if (size < 700)         _callerWeightLimit = std::max(700, size + (size >> 2));
   else                         _callerWeightLimit = size + (size >> 3);

   _callerWeightLimit -= size;

   _nodeCountThreshold = 16000;
   _methodInWarmBlockByteCodeSizeThreshold = _methodByteCodeSizeThreshold = 155;
   _methodInColdBlockByteCodeSizeThreshold = 30;
   _maxInliningCallSites = 4095;
   _maxRecursiveCallByteCodeSizeEstimate = 1024;

   if (comp()->getNodeCount() > _nodeCountThreshold)
      {
      _nodeCountThreshold = comp()->getNodeCount() * (1.05F);    // allow a little bit of inlining anyways to get smaller methods
      }

   // Code That should go to its frontend
   getUtil()->adjustCallerWeightLimit(callerSymbol, _callerWeightLimit);
   getUtil()->adjustMethodByteCodeSizeThreshold(callerSymbol, _methodByteCodeSizeThreshold);

   // let the FrontEnds have a chance to tweak inlining thresholds
   getUtil()->refineInliningThresholds(comp(), _callerWeightLimit, _maxRecursiveCallByteCodeSizeEstimate, _methodByteCodeSizeThreshold,
         _methodInWarmBlockByteCodeSizeThreshold, _methodInColdBlockByteCodeSizeThreshold, _nodeCountThreshold, size);

   // Possible env variable overrides
   static const char *a = feGetEnv("TR_MethodByteCodeSizeThreshold");
   if (a)
      _methodByteCodeSizeThreshold = atoi(a);

   static const char *b = feGetEnv("TR_MethodInWarmBlockByteCodeSizeThreshold");
   if (b)
      _methodInWarmBlockByteCodeSizeThreshold = atoi(b);

   static const char *c = feGetEnv("TR_MethodInColdBlockByteCodeSizeThreshold");
   if (c)
      _methodInColdBlockByteCodeSizeThreshold = atoi(c);

   static const char *d = feGetEnv("TR_CallerWeightLimit");
   if (d)
      _callerWeightLimit = atoi(d);

   static const char *e = feGetEnv("TR_NodeCountThreshold");
   if (e)
      _nodeCountThreshold = atoi(e);

   // Under voluntary OSR, OSR blocks and additional stores can
   // significantly increase the node count, particularly dead stores
   // in OSRCodeBlocks. To maintain the same inlining behaviour, the
   // threshold must be increased.
   //
   // The compile time overhead of this threshold increase has been tested
   // and was not significant, as this threshold is typically only reached
   // in hot or above compiles.
   if (comp()->getOption(TR_EnableOSR) && comp()->getOSRMode() == TR::voluntaryOSR && comp()->supportsInduceOSR())
      {
      static const char *f = feGetEnv("TR_OSRNodeCountThreshold");
      if (f)
         _nodeCountThreshold = atoi(f);
      else
         _nodeCountThreshold *= 2;
      }

   //call random functions to allow randomness to change limits
   if (comp()->getOption(TR_Randomize))
      {
      _nodeCountThreshold = (uint32_t)randomInt(32000);
      _methodByteCodeSizeThreshold = comp()->convertNonDeterministicInput( _methodByteCodeSizeThreshold, 500, randomGenerator(), 0);
      traceMsg(comp(),"\nTR_Randomize Enabled||TR_InlinerBase::inlineCallTarget, SeedValue:%d", comp()->getOptions()->getRandomSeed());
      }


   heuristicTrace(tracer(),"Inlining Limits:\n\tCaller Side Weight Limit (_callerWeightLimit) = %d \n\tCall Graph Size Threshold (_maxRecursiveCallByteCodeSizeEstimate) = %d "
                            "\n\tMethod size threshold (_methodByteCodeSizeThreshold) = %d \n\tMethod size threshold for warm (and above) compiles (_methodInWarmBlockByteCodeSizeThreshold) = %d"
                            "\n\tsize threshold for cold Calls (_methodInColdBlockByteCodeSizeThreshold) = %d\n\tNode Count Threshold (_nodeCountThreshold) = %d "
                            "\n\tSites Size (_maxInliningCallSites) = %d"
                            ,_callerWeightLimit,_maxRecursiveCallByteCodeSizeEstimate,_methodByteCodeSizeThreshold,_methodInWarmBlockByteCodeSizeThreshold,_methodInColdBlockByteCodeSizeThreshold,_nodeCountThreshold,_maxInliningCallSites);


   }

/**
 * Go through each OSR code block b.
 *
 * 1) If b corresponds to the top caller (root of the call tree), it doesn't do
 *    anything because we have already implanted, during ilgen, a TR::igoto at
 *    the end of b which transfers control back to the VM.
 * 2) Otherwise, replace the last treetop of b (which must be a goto or a
 *    return) to a goto to the OSR code block of the caller, and also make the
 *    corresponding changes to the CFG.
 */
void
TR_InlinerBase::linkOSRCodeBlocks()
   {
   TR_OSRCompilationData* compData = comp()->getOSRCompilationData();
   const TR_Array<TR_OSRMethodData *>& methodDataArray = compData->getOSRMethodDataArray();
   for (intptrj_t i = 0; i < methodDataArray.size(); ++i)
      {
      TR_OSRMethodData *osrMethodData = methodDataArray[i];
      if (osrMethodData == NULL
          || osrMethodData->getOSRCodeBlock() == NULL
          || osrMethodData->getInlinedSiteIndex() == -1
          || osrMethodData->linkedToCaller())
         continue;

      //For a non-top-level caller, we need to replace the return at the end of the OSR code block
      //with a goto to the OSR code block of the caller
      TR::Block *calleeOSRCodeBlock = osrMethodData->getOSRCodeBlock();
      if (calleeOSRCodeBlock->isUnreachable())
         continue;

      TR::TreeTop* lastTT = calleeOSRCodeBlock->getLastRealTreeTop();
      TR::Node* lastNode = lastTT->getNode();
      TR_ASSERT(lastNode->getOpCode().isGoto() || lastNode->getOpCode().isReturn(),
         "last treetop of a non-top-level caller OSR code block is neither a goto nor a return\n");

      TR::Block *callerOSRCodeBlock = compData->findCallerOSRMethodData(osrMethodData)->getOSRCodeBlock();
      TR_ASSERT(callerOSRCodeBlock != NULL, "caller's osr code block is empty\n");
      TR::Node *gotoNode =
         TR::Node::create(lastNode, TR::Goto, 0, callerOSRCodeBlock->getEntry());
      TR_ASSERT((calleeOSRCodeBlock->getSuccessors().size() == 1), "calleeOSRCodeBlock %d(%x) has zero or more than one successor\n", calleeOSRCodeBlock->getNumber(), calleeOSRCodeBlock);
      comp()->getFlowGraph()->removeEdge(calleeOSRCodeBlock->getSuccessors().front());
      lastTT->unlink(true);

      calleeOSRCodeBlock->append(TR::TreeTop::create(comp(), gotoNode));
      comp()->getFlowGraph()->addEdge(calleeOSRCodeBlock, callerOSRCodeBlock);
      osrMethodData->setLinkedToCaller(true);
      }
   }

void
TR_InlinerBase::performInlining(TR::ResolvedMethodSymbol * callerSymbol)
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR_InnerPreexistenceInfo *innerPrexInfo = getUtil()->createInnerPrexInfo(comp(), callerSymbol, 0, 0, 0, TR_NoGuard);

   bool inlinedSite = inlineCallTargets(callerSymbol, 0, innerPrexInfo);

   if (comp()->getOption(TR_EnableOSR))
      {
      linkOSRCodeBlocks();
      }

   if (inlinedSite && comp()->trace(OMR::inlining))
      {
      traceMsg(comp(), "inlined some calls for method %s\n", comp()->signature());
      }

   cleanup(callerSymbol, inlinedSite);

   if (debug("dumpInitialTrees") || comp()->getOption(TR_TraceTrees) )
      comp()->dumpMethodTrees("Post Inlining Trees");
   }

void
TR_InlinerBase::cleanup(TR::ResolvedMethodSymbol * callerSymbol, bool inlinedSite)
   {
   comp()->resetInlineDepth();

   if (inlinedSite)
      {
      callerSymbol->getFlowGraph()->removeUnreachableBlocks();
      }

   comp()->getSymRefTab()->clearAvailableAutos();

   if (inlinedSite)
      {
      _optimizer->setUseDefInfo(NULL);
      _optimizer->setValueNumberInfo(NULL);
      _optimizer->setRequestOptimization(OMR::catchBlockRemoval);
      _optimizer->setRequestOptimization(OMR::globalValuePropagation);
      _optimizer->setRequestOptimization(OMR::eachExpensiveGlobalValuePropagationGroup);
      _optimizer->setAliasSetsAreValid(false, true /*reset for WCode as well */);
      }

   }

//check if the interface implementation we found meet certain requirements
bool
OMR_InlinerUtil::validateInterfaceImplementation(TR_ResolvedMethod *interfaceMethod)
   {
   return true;
   }


bool
OMR_InlinerPolicy::mustBeInlinedEvenInDebug(TR_ResolvedMethod * calleeMethod, TR::TreeTop *callNodeTreeTop)
   {
   return false;
   }

bool
TR_InlinerBase::alwaysWorthInlining(TR_ResolvedMethod * calleeMethod, TR::Node *callNode)
   {
   return getPolicy()->alwaysWorthInlining(calleeMethod, callNode);
   }

bool
OMR_InlinerPolicy::alwaysWorthInlining(TR_ResolvedMethod * calleeMethod, TR::Node *callNode)
   {
   return false;
   }


bool
TR_InlinerBase::exceedsSizeThreshold(TR_CallSite *callsite, int bytecodeSize, TR::Block * block, TR_ByteCodeInfo & bcInfo,
      int32_t numLocals, TR_ResolvedMethod * callerResolvedMethod, TR_ResolvedMethod * calleeResolvedMethod,
      TR::Node *callNode, bool allConsts)
   {
   if (alwaysWorthInlining(calleeResolvedMethod, callNode))
      return false;


   heuristicTrace(tracer(),"### Checking inliner base sizeThreshold. bytecodeSize = %d, block = %p, numLocals = %p,"
                            "callerResolvedMethod = %s, calleeResolvedMethod = %s",
                             bytecodeSize, block,numLocals,tracer()->traceSignature(callerResolvedMethod),
                             tracer()->traceSignature(calleeResolvedMethod));

   int32_t blockFrequency = 0;
   int32_t borderFrequency;
   int32_t coldBorderFrequency; // only used in TR_MultipleCallTargetInliner::exceedsSizeThreshold (to date)
   getBorderFrequencies(borderFrequency, coldBorderFrequency, calleeResolvedMethod, callNode);

   OMR_InlinerPolicy *inlinerPolicy = getPolicy();
   if (block)
      {
      blockFrequency = comp()->convertNonDeterministicInput(block->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, randomGenerator(), 0);
      if (allowBiggerMethods())
         {
         static const char * p = feGetEnv("TR_HotBorderFrequency");

         if (p)
            {
            borderFrequency = atoi(p);
            }

         if (blockFrequency > borderFrequency)
            {
            int32_t oldSize = 0;
            if (comp()->trace(OMR::inlining))
              oldSize = bytecodeSize;

            bytecodeSize = scaleSizeBasedOnBlockFrequency(bytecodeSize, blockFrequency, borderFrequency, calleeResolvedMethod, callNode);

            if (comp()->trace(OMR::inlining))
              heuristicTrace(tracer(),"exceedsSizeThreshold: Scaled down size for call block_%d from %d to %d", block->getNumber(), oldSize, bytecodeSize);
            }
         }
      }
   else if (getPolicy()->aggressiveSmallAppOpts())
      {
      getUtil()->adjustByteCodeSize(calleeResolvedMethod, _isInLoop, block, bytecodeSize);
      }
   else
      {
      if (comp()->trace(OMR::inlining))
         heuristicTrace(tracer(),"exceedsSizeThreshold: No block passed in");
      }
   int32_t inlineThreshold = _methodByteCodeSizeThreshold;

   if (comp()->isServerInlining())
      {
      if (blockFrequency > borderFrequency)
         inlineThreshold = 200;
      }

   // reduce size if your args are consts


   if (callNode)
      {
      heuristicTrace(tracer(),"In ExceedsSizeThreshold.  Reducing size from %d",bytecodeSize);

      int32_t originalbcSize = bytecodeSize;
      uint32_t numArgs = callNode->getNumChildren();
      bool allconstsfromNode = true;

      uint32_t i = callNode->getFirstArgumentIndex();

      if ( callNode->getOpCode().isCall() &&
          !callNode->getSymbolReference()->isUnresolved() &&
          callNode->getSymbolReference()->getSymbol()->getMethodSymbol() &&
         !callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isHelper() &&
         !callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isSystemLinkageDispatch() &&
         !callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isStatic() )
               ++i;

      uint32_t numConstArgs=0;
      uint32_t numNonConstArgs=0;

      for (; i < numArgs; ++i)
         {

         // printf("callNode = %p\n");fflush(stdout);
         // printf("callNode->getOpCode().isCall() = %p\n");fflush(stdout);
         // printf("callNode->getSymbolReference() = %p\n");fflush(stdout);

         TR::Node * arg = callNode->getChild(i);

         if (arg->getOpCode().isLoadConst())
            {
            heuristicTrace(tracer(),"Node %p is load const\n",arg);
            bytecodeSize = bytecodeSize - (bytecodeSize/10);
            ++numConstArgs;
            }
         else if (arg->getOpCodeValue() == TR::aload && arg->getSymbolReference()->getSymbol()->isConstObjectRef())
            {
            heuristicTrace(tracer(),"Node %p is aload const\n",arg);
            bytecodeSize = bytecodeSize - (bytecodeSize/10);
            ++numConstArgs;
            }
         else
            {
            heuristicTrace(tracer(),"Node %p is not const\n",arg);
            allconstsfromNode = false;
            ++numNonConstArgs;
            }
         }


      if (!allconstsfromNode)
         {
         if ( _isInLoop && originalbcSize > inlineThreshold)
            {
            if (numConstArgs > 0 && (numConstArgs + numNonConstArgs) > 0 )
               bytecodeSize = (numConstArgs * originalbcSize) / (numConstArgs + numNonConstArgs) ;
            }
         else
            bytecodeSize = originalbcSize;

         }
      else if ( bytecodeSize < originalbcSize && originalbcSize > 100)
         {
         }

      heuristicTrace(tracer()," to %d because of const arguments",bytecodeSize);
      }
   else if (allConsts)
      {
      heuristicTrace(tracer(), "In ExceedsSizeThreshold.  Reducing size from %d", bytecodeSize);
      int32_t originalbcSize = bytecodeSize;
      int32_t numArgs = calleeResolvedMethod->numberOfExplicitParameters();
      for (int32_t i=0 ; i < numArgs; ++i)
         {
         bytecodeSize = bytecodeSize - (bytecodeSize / 10);
         }

      heuristicTrace(tracer()," to %d because of const arguments",bytecodeSize);
      }

   if (inlineThreshold && (uint32_t)bytecodeSize > inlineThreshold)
      {
      TR::Options::INLINE_calleeToBig ++;
      TR::Options::INLINE_calleeToBigSum += bytecodeSize;

      if (block)
         {
         TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/failed/calleeHasTooManyBytecodes", block->getFirstRealTreeTop());
         TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/failed/calleeHasTooManyBytecodes:#bytecodeSize", block->getFirstRealTreeTop(), bytecodeSize);
         TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/failed/calleeHasTooManyBytecodes:#excess", block->getFirstRealTreeTop(), bytecodeSize - inlineThreshold);
         TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/failed/calleeHasTooManyBytecodes:#localsInCallee", block->getFirstRealTreeTop(), numLocals);
         }
      heuristicTrace(tracer(),"### Exceeding size threshold because bytecodeSize %d > inlineThreshold %d ",bytecodeSize, inlineThreshold);
      return true; // Exceeds size threshold
      }

   heuristicTrace(tracer(),"### Did not exceed size threshold, bytecodeSize %d <= inlineThreshold %d ",bytecodeSize, inlineThreshold);
   return false;   // Does not exceed threshold
   }

void
TR_InlinerBase::createParmMap(TR::ResolvedMethodSymbol *calleeSymbol, TR_LinkHead<TR_ParameterMapping> &map)
   {
   ListIterator<TR::ParameterSymbol> parms(&calleeSymbol->getLogicalParameterList(comp()));

   for (TR::ParameterSymbol * p = parms.getFirst(); p; p = parms.getNext())
      {
      int32_t offset = p->getParameterOffset();
      TR_ParameterMapping* currPM = map.getFirst(), * prevPM = 0;
      for (; currPM && offset > currPM->_parmSymbol->getParameterOffset(); prevPM = currPM, currPM = currPM->getNext())
         ;
      map.insertAfter(prevPM, new (trStackMemory()) TR_ParameterMapping(p));
      }
   }


bool
TR_InlinerBase::inlineRecognizedMethod(TR::RecognizedMethod method)
   {
   return getPolicy()->inlineRecognizedMethod(method);
   }

bool
OMR_InlinerPolicy::inlineRecognizedMethod(TR::RecognizedMethod method)
   {
   return false;
   }

// only dumbinliner uses this and includes checking for variable initialization
bool
TR_DumbInliner::tryToInline(char *message, TR_CallTarget *calltarget)
   {
   TR_ResolvedMethod *method = calltarget->_calleeSymbol->getResolvedMethod();

   if (getPolicy()->tryToInline(calltarget,NULL,true))
      {
      if (comp()->trace(OMR::inlining))
         traceMsg(comp(), "tryToInline pattern matched; %s for %s\n", message, method->signature(comp()->trMemory()));
      return true;
      }

   return false;
   }

//general tryToInline methods
bool
OMR_InlinerPolicy::tryToInlineGeneral(TR_CallTarget * calltarget, TR_CallStack * callStack, bool toInline)
   {
   const char * signature = calltarget->_calleeMethod->signature(comp()->trMemory());

   TR::SimpleRegex * regex = NULL;

   if (toInline)
      regex=comp()->getOptions()->getTryToInline();
   else
      regex=comp()->getOptions()->getDontInline();

   if (regex && TR::SimpleRegex::match(regex, calltarget->_calleeMethod))
      {
      if (comp()->trace(OMR::inlining))
         {
         traceMsg(comp(), toInline?"Inliner: tryToInline pattern matched, ":"Inliner: dontInline pattern matched, ");
         traceMsg(comp(), "signature: %s\n", signature);
         }
      return true;
      }

   if (callStack && callStack->_inlineFilters)
      {
      TR_FilterBST *filterInfo = NULL;
      TR::CompilationFilters * inlineFilters = NULL;

      inlineFilters=callStack->_inlineFilters;

      if (inlineFilters)
         {
         bool inclusive=comp()->getDebug()->methodSigCanBeFound(signature, inlineFilters, filterInfo,
                                                                calltarget->_calleeMethod->convertToMethod()->methodType());

         if (filterInfo)
            {
            if (toInline && inclusive)
               return true;
            if (!toInline && !inclusive)
               return true;
            }
         }
      }

   // consider the global dontInline
      {
      if (!toInline)
         {
         TR_FilterBST *filterInfo = NULL;
         TR::CompilationFilters * inlineFilters = NULL;

         if (TR::Options::getDebug())
            {
            inlineFilters=TR::Options::getDebug()->getInlineFilters();
            }


         if (inlineFilters)
            {
            bool inclusive=comp()->getDebug()->methodSigCanBeFound(signature, inlineFilters, filterInfo,
                                                                  calltarget->_calleeMethod->convertToMethod()->methodType());

            if (filterInfo)
               {
               if (!inclusive)
                  return true;
               }
            }
         }
      }
   return false;
   }

bool
OMR_InlinerPolicy::tryToInline(TR_CallTarget * calltarget, TR_CallStack * callStack, bool toInline)
   {
   return tryToInlineGeneral(calltarget, callStack, toInline);
   }

//---------------------------------------------------------------------
// TR_CallStack
//---------------------------------------------------------------------

TR_CallStack::TR_CallStack(
   TR::Compilation * c,
   TR::ResolvedMethodSymbol * methodSymbol, TR_ResolvedMethod * method,
   TR_CallStack * nextCallStack, int32_t maxCallSize, bool safeToAddSymRefs)
   : TR_Link<TR_CallStack>(nextCallStack),
     _comp(c), _trMemory(c->trMemory()),
     _methodSymbol(methodSymbol),
     _method(method),
     _currentCallNode(NULL),
     _maxCallSize(maxCallSize),
     _inALoop(nextCallStack ? nextCallStack->_inALoop : 0),
     _alwaysCalled(nextCallStack ? nextCallStack->_alwaysCalled : 0),
     _autos(c->trMemory()),
     _temps(c->trMemory()),
     _injectedBasicBlockTemps(c->trMemory()),
     _inlineFilters(NULL),
     _blockInfo(0),
     _safeToAddSymRefs(safeToAddSymRefs)
   {
   TR_FilterBST *filterInfo = NULL;
   TR::CompilationFilters * inlineFilters = NULL;

   if (!nextCallStack)
      {
      if (TR::Options::getDebug())
         inlineFilters = TR::Options::getDebug()->getInlineFilters();

      if (inlineFilters)
         {
         bool inclusive = comp()->getDebug()->methodSigCanBeFound(_method->signature(comp()->trMemory()), inlineFilters, filterInfo,
                                                                  _method->convertToMethod()->methodType());

         if (filterInfo)
            {
            if (!inclusive)
               {
               _inlineFilters = filterInfo->subGroup;
               }
            }
         }

      }
   else
      {
      if (nextCallStack->_inlineFilters)
         inlineFilters = nextCallStack->_inlineFilters;

      if (inlineFilters)
         {
         bool inclusive = comp()->getDebug()->methodSigCanBeFound(_method->signature(comp()->trMemory()), inlineFilters, filterInfo,
                                                                  _method->convertToMethod()->methodType());

         if (filterInfo)
            {
            if (inclusive)
               {
               _inlineFilters = filterInfo->subGroup;
               }
            }
         }
      }

   }

void TR_CallStack::commit()
      {
   ListIterator<TR::AutomaticSymbol> autos(&_autos);
   ListIterator<TR::SymbolReference> temps(&_temps);
   ListIterator<TR::SymbolReference> injectedBasicBlockTemps(&_injectedBasicBlockTemps);
   TR::AutomaticSymbol * a = autos.getFirst();
   TR::SymbolReference * temp = temps.getFirst();
   TR::SymbolReference * injectedBasicBlockTemp = injectedBasicBlockTemps.getFirst();

   if (getNext())
      {
      for (; a; a = autos.getNext())
         getNext()->_autos.add(a);
      for (; temp; temp = temps.getNext())
         getNext()->_temps.add(temp);
      for (; injectedBasicBlockTemp; injectedBasicBlockTemp = injectedBasicBlockTemps.getNext())
         getNext()->_injectedBasicBlockTemps.add(injectedBasicBlockTemp);
      }
   else
      {
      TR_ASSERT(_methodSymbol, "_methodSymbol has not been set");
      //TR_ASSERT(!temp, "temps should have been added by a call to makeTempsAvailable");
      for (; a; a = autos.getNext())
         _methodSymbol->addAutomatic(a);
      for (; injectedBasicBlockTemp; injectedBasicBlockTemp = injectedBasicBlockTemps.getNext())
         _methodSymbol->addAutomatic(injectedBasicBlockTemp->getSymbol()->castToAutoSymbol());
      }


    //to make an assert in the destructor work
   _autos.deleteAll();
   _temps.deleteAll();
   _injectedBasicBlockTemps.deleteAll();
   }

TR_CallStack::~TR_CallStack()
   {
   //::commit is supposed to clear up the lists after everything has been propagated to a caller
   //if there are still some residual symRefs left it means that we missed a call to commit somewhere
#if !defined(AIXPPC)
   TR_ASSERT(std::uncaught_exception() || (_autos.isEmpty() && _temps.isEmpty() && _injectedBasicBlockTemps.isEmpty()), "lists should have been emptied by TR_CallStack::commit");
#endif
   }

void
TR_CallStack::addAutomatic(TR::AutomaticSymbol * a)
   {
   TR_ASSERT(_safeToAddSymRefs, "make sure a constructed callStack is committed then add _safeToAddSymRefs to your c-tor call");
   _autos.add(a);
   }

void
TR_CallStack::addTemp(TR::SymbolReference * temp)
   {
   TR_ASSERT(_safeToAddSymRefs, "make sure a constructed callStack is committed then add _safeToAddSymRefs to your c-tor call");
   _temps.add(temp);
   }

void
TR_CallStack::addInjectedBasicBlockTemp(TR::SymbolReference * temp)
   {
   TR_ASSERT(_safeToAddSymRefs, "make sure a constructed callStack is committed then add _safeToAddSymRefs to your c-tor call");
   _injectedBasicBlockTemps.add(temp);
   }

void
TR_CallStack::makeTempsAvailable(List<TR::SymbolReference> & availableTemps)
   {
   makeTempsAvailable(availableTemps, _temps);
   }

void
TR_CallStack::makeBasicBlockTempsAvailable(List<TR::SymbolReference> & availableTemps)
   {
   makeTempsAvailable(availableTemps, _injectedBasicBlockTemps);
   }

void
TR_CallStack::makeTempsAvailable(List<TR::SymbolReference> & availableTemps, List<TR::SymbolReference> & temps)
   {
   if (!getNext())
      {
      TR::SymbolReference * temp;
      while ((temp = temps.popHead()))
         {
         _methodSymbol->addAutomatic(temp->getSymbol()->castToAutoSymbol());
         availableTemps.add(temp);
         }
      }
   }

TR_CallStack *
TR_CallStack::isCurrentlyOnTheStack(TR_ResolvedMethod * method, int32_t occurrences)
   {
   int32_t counter = 0;
   for (TR_CallStack * cs = this; cs; cs = cs->getNext())
      if (cs->_method->isSameMethod(method) && ++counter == occurrences)
         return cs;
   return 0;
   }

bool
TR_CallStack::isAnywhereOnTheStack(TR_ResolvedMethod * method, int32_t occurrences)
   {
   int32_t counter = 0;
   for (TR_CallStack * cs = this; cs; cs = cs->getNext())
      {
      if (cs->_method->isSameMethod(method) && ++counter == occurrences)
         {
         return true;
         }
      else if (cs->_currentCallNode)
         {
         for (int16_t callerIndex = cs->_currentCallNode->getByteCodeInfo().getCallerIndex(); callerIndex != -1; )
            {
            TR_InlinedCallSite &ics = _comp->getInlinedCallSite(callerIndex);
            TR_ResolvedMethod *caller = _comp->getInlinedResolvedMethod(callerIndex);
            if (caller->isSameMethod(method))
               {
               if (++counter == occurrences)
                  return true;
               }
            callerIndex = ics._byteCodeInfo.getCallerIndex();
            }
         }
      }
   return 0;
   }

TR_CallStack::BlockInfo &
TR_CallStack::blockInfo(int32_t i)
   {
   TR_ASSERT(i != -1, "block number has not been assigned");
   TR_ASSERT(_blockInfo, "blockInfo has not been set");
   return _blockInfo[i];
   }

void
TR_CallStack::updateState(TR::Block * block)
   {
   int32_t blockNumber = block->getNumber();
   if (blockNumber != -1)
      {
      if (!getNext() || !getNext()->_inALoop)
         _inALoop = blockInfo(blockNumber)._inALoop;
      if (!getNext() || getNext()->_alwaysCalled)
         _alwaysCalled = blockInfo(blockNumber)._alwaysReached;
      }
   }

void
TR_CallStack::initializeControlFlowInfo(TR::ResolvedMethodSymbol * callerSymbol)
   {
   TR::CFG * cfg = callerSymbol->getFlowGraph();
   TR_BitVector loopingBlocks(comp()->trMemory()->currentStackRegion());

   cfg->findLoopingBlocks(loopingBlocks);

   int32_t numberOfBlocks = cfg->getNextNodeNumber();
   _blockInfo = new (trStackMemory()) BlockInfo[numberOfBlocks];

   for (int32_t i = 0; i < numberOfBlocks; ++i)
      {
      blockInfo(i)._inALoop = loopingBlocks.get(i);
      }
   // Walk forward following successor edges to mark blocks that are always reached
   //
   TR::Block * block = toBlock(cfg->getStart());
   for(;;)
      {
      if ((block->getSuccessors().size() != 1) || blockInfo(block->getNumber())._alwaysReached)
         break;
      block = toBlock(block->getSuccessors().front()->getTo());
      blockInfo(block->getNumber())._alwaysReached = true;
      }

   // Walk backwards following predecessors edges to mark blocks that are always reached
   //
   block = toBlock(cfg->getEnd());
   for(;;)
      {
      if (block->getPredecessors().empty() || (block->getPredecessors().size() > 1) || blockInfo(block->getNumber())._alwaysReached)
         break;
      block = toBlock(block->getPredecessors().front()->getFrom());
      blockInfo(block->getNumber())._alwaysReached = true;
      }
   }

//---------------------------------------------------------------------
// TR_InlineCallTarget
//---------------------------------------------------------------------

TR_InlineCall::TR_InlineCall(TR::Optimizer * optimizer, TR::Optimization *opt)
   : TR_DumbInliner(optimizer, opt, isScorching(optimizer->comp()) ? 140 : (isHot(optimizer->comp()) ? 70 : 35))
   {
   }

bool
TR_InlineCall::inlineRecognizedMethod(TR::RecognizedMethod method)
   {
   if (getPolicy()->willBeInlinedInCodeGen(method))
      return false;
   return true;
   }


bool
TR_InlineCall::inlineCall(TR::TreeTop * callNodeTreeTop, TR_OpaqueClassBlock * thisClass, bool recursiveInlining, TR_PrexArgInfo *argInfo, int32_t initialMaxSize)
   {
   TR_InlinerDelimiter delimiter(tracer(),"TR_InlineCall::inlineCall");
   if (!getOptimizer()->isEnabled(OMR::inlining))
      return false;

   TR::Node * parent = callNodeTreeTop->getNode();
   if (parent->getNumChildren() != 1)
      return false;
   TR::Node * callNode = parent->getFirstChild();
   if (!callNode->getOpCode().isCall())
      return false;

   TR::SymbolReference *symRef = callNode->getSymbolReference();

   TR::ResolvedMethodSymbol *resolvedCalleeSymbol = symRef->getSymbol()->castToResolvedMethodSymbol();
   if (resolvedCalleeSymbol && !getPolicy()->canInlineMethodWhileInstrumenting(resolvedCalleeSymbol->getResolvedMethod()))
      return false;

   TR::ResolvedMethodSymbol * callerSymbol = comp()->getMethodSymbol();

   if (recursiveInlining && initialMaxSize == 0)
      initialMaxSize = isScorching(comp()) ? 140 : isHot(comp())? 90 : isWarm(comp()) ? 60 : 45;

   static const char *fub = feGetEnv("TR_DumbInlineThreshold");

   if(fub)
      {
      initialMaxSize = atoi(fub);
      heuristicTrace(tracer(),"Setting initialMaxSize to %d",initialMaxSize);
      }


   TR_CallStack callStack(comp(), callerSymbol, comp()->getCurrentMethod(), 0, initialMaxSize, true);
   TR_InnerPreexistenceInfo *innerPrexInfo = getUtil()->createInnerPrexInfo(comp(), callerSymbol, 0, 0, 0, TR_NoGuard);
   callStack._innerPrexInfo = innerPrexInfo;

   TR_VirtualGuardSelection *guard;
   TR::MethodSymbol *calleeSymbol = symRef->getSymbol()->castToMethodSymbol();


   TR_CallSite *callsite = TR_CallSite::create(callNodeTreeTop, parent, callNode,
                                               thisClass, symRef, (TR_ResolvedMethod*) 0,
                                               comp(), trMemory() , stackAlloc);

   getSymbolAndFindInlineTargets(&callStack,callsite);

   if(!callsite->numTargets())
      return false; //nothing to callStack::commit yet

   bool inlinedSite = false;
   for(int32_t i = 0 ; i < callsite->numTargets() ; i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);

      if (initialMaxSize > 0 && getPolicy()->getInitialBytecodeSize(calltarget->_calleeSymbol, comp()) > initialMaxSize)
         {
         heuristicTrace(tracer(),"Failed at Inlining. getMaxByteCodeIndex of %d exceeds initialMaxSize of %d",getPolicy()->getInitialBytecodeSize(calltarget->_calleeSymbol, comp()),initialMaxSize);
         return false; //nothing to callStack::commit yet
         }

      {
      TR::StackMemoryRegion stackMemoryRegion(*trMemory());

      int16_t currentInlineDepth = comp()->adjustInlineDepth(callNode->getByteCodeInfo());

      if (comp()->trace(OMR::inlining))
         traceMsg(comp(), "inliner: Setting current inline depth=%d\n", currentInlineDepth);

      calltarget->_prexArgInfo = new (trHeapMemory()) TR_PrexArgInfo(calltarget->_myCallSite->_callNode->getNumArguments(), trMemory());

      // this is called on a case-by-case basis, and can get repeatedly called for recursive
      // methods triggering a loop.  Unfortunately the callStack is not pervasive
      // for all these seperate calls, so check the VM stack to prevent infinite inlining
      // of recursive calls
      //
      if (comp()->foundOnTheStack(calltarget->_calleeSymbol->getResolvedMethod(), 2))
         inlinedSite = false;
      else
         inlinedSite = inlineCallTarget(&callStack, calltarget, false, argInfo);

      comp()->resetInlineDepth();

      if (comp()->getOption(TR_EnableOSR))
         {
         linkOSRCodeBlocks();
         }

      cleanup(callerSymbol, inlinedSite);
      } // stack memory region scope

      }

   callStack.commit();
   return inlinedSite;
   }


//---------------------------------------------------------------------
// TR_DumbInliner
//---------------------------------------------------------------------

TR_DumbInliner::TR_DumbInliner(TR::Optimizer * optimizer, TR::Optimization *optimization, uint32_t initialSize, uint32_t dumbReductionIncrement)
   : TR_InlinerBase(optimizer, optimization),
     _initialSize(initialSize),
     _dumbReductionIncrement(dumbReductionIncrement)
   {
   static const char * p;
   static int32_t dri = (p = feGetEnv("TR_DumbReductionIncrement")) ? atoi(p) : -1;
   if (dri >= 0)
      _dumbReductionIncrement = dri;
   }

bool
OMR_InlinerPolicy::inlineMethodEvenForColdBlocks(TR_ResolvedMethod *method)
   {
   return false;
   }

//Maximum number of inlines to perform
#define MAX_INLINE_COUNT 1000

bool
TR_DumbInliner::inlineCallTargets(TR::ResolvedMethodSymbol * callerSymbol, TR_CallStack * prevCallStack, TR_InnerPreexistenceInfo *innerPrexInfo) {
   int32_t maxCallSize;

   if (!comp()->getOption(TR_DisableAdaptiveDumbInliner))
      {
      int32_t callerSize = callerSymbol->getResolvedMethod()->maxBytecodeIndex();//getInitialBytecodeSize(callerSymbol, comp());
      maxCallSize = prevCallStack == 0 ?
         // Initial budget should be inverse proportional to the size of the method
         (int32_t)_initialSize - callerSize*(int32_t)_initialSize/comp()->getOptions()->getDumbInlinerBytecodeSizeCutoff() :
      // If I inlined a bigger method, my budget should decrease by a bigger amount
      prevCallStack->_maxCallSize - callerSize/comp()->getOptions()->getDumbInlinerBytecodeSizeDivisor();
      }
   else
      {
      // For top most method start with a budget of _initialSize
      // For every level of inlining decrement the budget by _dumbReductionIncrement
      maxCallSize = prevCallStack == 0 ? _initialSize :prevCallStack->_maxCallSize - _dumbReductionIncrement;
      }

   if (maxCallSize <= 0) return false;

   TR_CallStack callStack(comp(), callerSymbol, callerSymbol->getResolvedMethod(), prevCallStack, maxCallSize, true);
   if (innerPrexInfo)
      callStack._innerPrexInfo = innerPrexInfo;

   bool isCold = false;
   bool prevInliningAsWeWalk = _inliningAsWeWalk;
   int32_t thisCallSite = callerSymbol->getFirstTreeTop()->getNode()->getInlinedSiteIndex();
   uint32_t inlineCount = 0;
   for (TR::TreeTop * tt = callerSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      _inliningAsWeWalk = true;

      // assume that all call nodes are the first child of a treetop
      TR::Node * parent = tt->getNode();
      if (parent->getOpCodeValue() == TR::BBStart)
         {
         isCold = false;
         TR::Block *block = parent->getBlock();

         int32_t veryColdBorderFrequency = 0;
         if (comp()->getMethodHotness() <= cold)
            {
            veryColdBorderFrequency = comp()->getOption(TR_DisableConservativeColdInlining) ? 0 : 1500;
            // Did the user specify a specific value? If so, use that
            if (comp()->getOptions()->getInlinerVeryColdBorderFrequencyAtCold() >= 0)
               veryColdBorderFrequency = comp()->getOptions()->getInlinerVeryColdBorderFrequencyAtCold();
            }

         // dont inline into cold blocks
         if (block->isCold() ||
            (TR::isJ9() && !getPolicy()->inlineMethodEvenForColdBlocks(callerSymbol->getResolvedMethod()) && block->getFrequency() >= 0 && block->getFrequency() < veryColdBorderFrequency) ||
            !block->getExceptionPredecessors().empty())
            {
            isCold = true;
            }
         }

      if (parent->getNumChildren())
         {
         TR::Node * node = parent->getChild(0);
         if (node->getOpCode().isCall() && (!node->getOpCode().isJumpWithMultipleTargets()) && node->getVisitCount() != _visitCount && node->getInlinedSiteIndex() == thisCallSite)
            {
            if ((isCold
#ifdef J9_PROJECT_SPECIFIC
                 || comp()->getPersistentInfo()->getInlinerTemporarilyRestricted()
#endif
                 ) && // getInlinerTemporarilyRestricted is used to avoid inlining during startup
                !comp()->getMethodSymbol()->doJSR292PerfTweaks() &&
                node->getSymbol() && node->getSymbol()->isResolvedMethod() &&
                !alwaysWorthInlining(node->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod(), node))
               TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/failed/coldCallee/1", tt);
            else
               if (analyzeCallSite(&callStack, tt, parent, node))
               {
               inlineCount++;
               if (inlineCount >= MAX_INLINE_COUNT)
                  {
                  if (comp()->trace(OMR::inlining))
                     traceMsg(comp(), "inliner: stopping inlining as max inline count of %d reached\n", MAX_INLINE_COUNT);
                  break;
                  }
               }
            node->setVisitCount(_visitCount);
            }
         }

      if (!prevCallStack &&
            parent->getOpCodeValue() == TR::BBStart &&
            !parent->getBlock()->isExtensionOfPreviousBlock())
         callStack.makeBasicBlockTempsAvailable(_availableBasicBlockTemps);
      }

   _inliningAsWeWalk = prevInliningAsWeWalk;
   callStack.commit();
   return (inlineCount != 0);
   }

bool
TR_DumbInliner::analyzeCallSite(
   TR_CallStack * callStack, TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode)
   {
   TR_InlinerDelimiter delimiter(tracer(),"TR_DumbInliner::analyzeCallSite");

   TR::SymbolReference *symRef = callNode->getSymbolReference();
   TR::MethodSymbol *calleeSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR_CallSite *callsite = TR_CallSite::create(callNodeTreeTop, parent, callNode,
                                               (TR_OpaqueClassBlock*) 0, symRef, (TR_ResolvedMethod*) 0,
                                               comp(), trMemory() , stackAlloc);

   getSymbolAndFindInlineTargets(callStack,callsite);

   if (!callsite->numTargets())
      return false;
   bool success = false;
   for(int32_t i=0;i<callsite->numTargets();i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);
      uint32_t byteCodeSize = getPolicy()->getInitialBytecodeSize(calltarget->_calleeSymbol, comp());

      uint32_t maxBCSize = (uint32_t)callStack->_maxCallSize;

      if ((byteCodeSize > maxBCSize))
         {
         if (tryToInline("overriding getMaxBytecodeIndex check", calltarget))
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "inliner: overriding getMaxBytecodeIndex check\n");
            }
         else if (alwaysWorthInlining(calltarget->_calleeSymbol->getResolvedMethod(), callNode))
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "inliner: overriding getMaxBytecodeIndex check because it's always worth inlining\n");
            }
         else
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "inliner: failed: getInitialBytecodeSize(%d) > %d for %s\n",
                       byteCodeSize, callStack->_maxCallSize, tracer()->traceSignature(calltarget->_calleeSymbol));
            if (comp()->cg()->traceBCDCodeGen())
               {
               traceMsg(comp(), "q^q : failing to inline %s into %s (callNode %p on line_no=%d) due to wcode size\n",
                       tracer()->traceSignature(calltarget->_calleeSymbol),tracer()->traceSignature(callStack->_methodSymbol),
                       callNode,comp()->getLineNumber(callNode));
               }
            calltarget->_myCallSite->_visitCount++;
            continue;
            }
         }

      success |= inlineCallTarget(callStack, calltarget, false);
      }
   return success;
   }

//---------------------------------------------------------------------
// TR_InlinerBase inlineCallTarget
//---------------------------------------------------------------------

static TR::TreeTop * cloneAndReplaceCallNodeReference(TR::TreeTop *, TR::Node *, TR::Node *, TR::TreeTop *, TR::Compilation *);
static void addSymRefsToList(List<TR::SymbolReference> &, List<TR::SymbolReference> &);

static TR::Node *findPotentialDecompilationPoint(TR::Node *node, TR::Compilation *comp)
   {
   if (node->getVisitCount() == comp->getVisitCount())
      return NULL;
   else
      node->setVisitCount(comp->getVisitCount());

   TR::Node *result = NULL;
   if (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->canCauseGC())
      return result = node;
   else
      for (int32_t i = 0; !result && i < node->getNumChildren(); i++)
         result = findPotentialDecompilationPoint(node->getChild(i), comp);

   return result;
   }

static TR::Node *findPotentialDecompilationPoint(TR::ResolvedMethodSymbol *calleeSymbol, TR::Compilation *comp)
   {
   comp->incVisitCount();
   TR::Node *result = NULL;
   for (TR::TreeTop *tt = calleeSymbol->getFirstTreeTop(); tt && !result; tt = tt->getNextTreeTop())
      result = findPotentialDecompilationPoint(tt->getNode(), comp);
   return result;
   }

void
TR_InlinerBase::cloneChildren(TR::Node * to, TR::Node * from, uint32_t fromStartIndex)
   {
   for (uint32_t i = fromStartIndex, j = 0; i < from->getNumChildren(); ++i, ++j)
      {
      TR::Node * fromChild = from->getChild(i);
      TR::Node * toChild;
      if (fromChild->getReferenceCount() == 1)
         {
         toChild = TR::Node::copy(fromChild);
         cloneChildren(toChild, fromChild, 0);
         }
      else
         {
         toChild = fromChild;
         toChild->incReferenceCount();
         }
      to->setChild(j, toChild);
      }
   }

/**
 * Finds all nodes that are references to the given call node in callerSymbol
 * and replaces them with the resultNode
 *
 *   OR
 *
 * removes the callNode from callerSymbol
*/
void
TR_InlinerBase::replaceCallNode(
   TR::ResolvedMethodSymbol * callerSymbol, TR::Node * resultNode, rcount_t originalCallNodeReferenceCount,
   TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode)
   {

   // replace the call node with resultNode or remove the caller tree top
   //
   if (resultNode)
      {
      resultNode->setVisitCount(_visitCount); // make sure we don't visit this node again
      parent->setChild(0, resultNode);
      callNode->recursivelyDecReferenceCount();
      resultNode->incReferenceCount();
      rcount_t numberOfReferencesToFind = originalCallNodeReferenceCount - 1;
      TR::TreeTop * tt = callNodeTreeTop->getNextTreeTop();
      comp()->incVisitCount();
      for (; tt && numberOfReferencesToFind; tt = tt->getNextTreeTop())
         replaceCallNodeReferences(tt->getNode(), 0, 0, callNode, resultNode, numberOfReferencesToFind);
      }
   else
      callerSymbol->removeTree(callNodeTreeTop);
   }

/**
 * Recursively searches node and replaces all references to callNode with
 * replacementNode
 */
void
TR_InlinerBase::replaceCallNodeReferences(
   TR::Node * node, TR::Node * parent, uint32_t childIndex,
   TR::Node * callNode, TR::Node * replacementNode, rcount_t & numberOfReferencesToFind)
   {
   bool replacedNode = false;
   if (node == callNode)
      {
      replacedNode = true;
      --numberOfReferencesToFind;
      parent->setChild(childIndex, replacementNode);
      callNode->recursivelyDecReferenceCount();
      replacementNode->incReferenceCount();
      }

   // We check _inliningAsWeWalk since call nodes' visit counts should not be changed
   // if we are inlining as we walk. We might think we have not inlined some call and
   // try to inline it again and again (as was seen during dumb inlining).
   // Note that this routine we are in could be called from code reached from dumb inliner where inline
   // as we walk the trees in inlineCallTargets as well as the call graph inliner inlineCallTargets
   // where we may/may not inline as we walk the trees. Handling both cases is something to
   // consider when changing below code.
   //
   if ((_inliningAsWeWalk && node->getOpCode().isCall() && (node->getVisitCount() == _visitCount)) ||
         (node->getVisitCount() == comp()->getVisitCount()))
      return;

   node->setVisitCount(comp()->getVisitCount());

   if (!replacedNode)
      {
      for (int32_t i = 0; i < node->getNumChildren() && numberOfReferencesToFind; ++i)
         replaceCallNodeReferences(node->getChild(i), node, i, callNode, replacementNode, numberOfReferencesToFind);
      }
   }

TR::Node *
TR_InlinerBase::createVirtualGuard(
   TR::Node * callNode, TR::ResolvedMethodSymbol * calleeSymbol, TR::TreeTop * destination,
   int16_t calleeIndex, TR_OpaqueClassBlock * thisClass, bool favourVftCompareGuard, TR_VirtualGuardSelection *guard)
   {
   TR::MethodSymbol * callNodeMethodSymbol= callNode->getSymbol()->castToMethodSymbol();
   TR::ResolvedMethodSymbol * callNodeResolvedMethodSymbol = callNodeMethodSymbol->getResolvedMethodSymbol();

   TR::TreeTop *debugCounterInsertionPoint = destination->getNextTreeTop(); // destination should always be a BBStart
   TR_ByteCodeInfo &bcInfo = destination->getNode()->getByteCodeInfo();
   TR::DebugCounter::Fidelities fidelity;
   switch (guard->_kind)
      {
      case TR_ProfiledGuard:
         if (guard->isHighProbablityProfiledGuard())
            fidelity = TR::DebugCounter::Free; // The taken path is marked isCold, so it had better never happen
         else
            fidelity = TR::DebugCounter::Moderate;
         break;
      case TR_MethodEnterExitGuard:
         fidelity = TR::DebugCounter::Cheap;
         break;
      default:
         // Nopable guards are considered Free (or else we shouldn't be using code patching!)
         fidelity = TR::DebugCounter::Free;
         break;
      }
   TR::DebugCounter::prependDebugCounter(comp(),
      TR::DebugCounter::debugCounterName(comp(), "virtualGuards.byKind/%s/(%s)/bcinfo=%d.%d", tracer()->getGuardKindString(guard), comp()->signature(), bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex()),
      debugCounterInsertionPoint, 1, fidelity);
   TR::DebugCounter::prependDebugCounter(comp(),
      TR::DebugCounter::debugCounterName(comp(), "virtualGuards.byJittedBody/%s/(%s)/(%s)/%s/bcinfo=%d.%d", comp()->getHotnessName(comp()->getMethodHotness()), comp()->signature(), calleeSymbol->signature(trMemory()), tracer()->getGuardKindString(guard), bcInfo.getCallerIndex(), bcInfo.getByteCodeIndex()),
      debugCounterInsertionPoint, 1, fidelity);

   if (guard->_kind == TR_DummyGuard)
      {
      return TR_VirtualGuard::createDummyGuard(comp(), calleeIndex, callNode, destination);
      }

   if (guard->_kind == TR_HCRGuard)
      {
      return TR_VirtualGuard::createHCRGuard(comp(), calleeIndex, callNode, destination, calleeSymbol, thisClass);
      }

   if (guard->_kind == TR_MutableCallSiteTargetGuard)
      {
      TR::KnownObjectTable *knot = comp()->getOrCreateKnownObjectTable();
      heuristicTrace(tracer(),"  createVirtualGuard: MutableCallSite.epoch is %p.obj%d (%p.%p)", guard->_mutableCallSiteObject, guard->_mutableCallSiteEpoch, *guard->_mutableCallSiteObject, *knot->getPointerLocation(guard->_mutableCallSiteEpoch));
      return TR_VirtualGuard::createMutableCallSiteTargetGuard(comp(), calleeIndex, callNode, destination, guard->_mutableCallSiteObject, guard->_mutableCallSiteEpoch);
      }

   if (guard->_kind == TR_DirectMethodGuard)
      {
      return TR_VirtualGuard::createAOTInliningGuard(comp(), calleeIndex, callNode, destination, guard->_kind);
      }

   if (guard->_type == TR_VftTest)
      {
      //TR_ASSERT(thisClass == info->getThisClass(), "type info mismatch");
      return TR_VirtualGuard::createVftGuard(guard->_kind, comp(), calleeIndex, callNode,
                                             destination, thisClass);
      }
   else if (guard->_type == TR_RubyInlineTest)
      {
      return TR_VirtualGuard::createRubyInlineGuard(guard->_kind, comp(), calleeIndex, callNode,
                                             destination, guard->_thisClass);
      }
   else if (guard->_type == TR_MethodTest)
      return TR_VirtualGuard::createMethodGuard(guard->_kind, comp(), calleeIndex, callNode,
             destination, calleeSymbol, thisClass);
   else if (guard->_kind == TR_BreakpointGuard)
      {
      return TR_VirtualGuard::createBreakpointGuard(comp(), calleeIndex, callNode,
             destination, calleeSymbol);
      }
   else
      {
      TR_ASSERT(guard->_type == TR_NonoverriddenTest, "assertion failure");
      return TR_VirtualGuard::createNonoverriddenGuard(guard->_kind, comp(),
             calleeIndex,
             callNode, destination, calleeSymbol);
      }
   }

static bool blocksHaveSameStartingByteCodeInfo(TR::Block *aBlock, TR::Block *bBlock)
   {
   TR_ByteCodeInfo a = aBlock->getEntry()->getNode()->getByteCodeInfo();
   TR_ByteCodeInfo b = bBlock->getEntry()->getNode()->getByteCodeInfo();
   return (a.getCallerIndex() == b.getCallerIndex()) && (a.getByteCodeIndex() == b.getByteCodeIndex());
   }

/**
 * Root function for privatized inliner argument rematerialization - this handles calculating remat
 * safety and performing the remat.
 */
void TR_InlinerBase::rematerializeCallArguments(TR_TransformInlinedFunction & tif,
   TR_VirtualGuardSelection *guard, TR::Node *callNode, TR::Block *block1, TR::TreeTop *rematPoint)
   {
   debugTrace(tracer(), "privatizedInlinerArg - consider arguments for rematerialization for guard [%p] of call node [n%dn]", guard, callNode->getGlobalIndex());
   //To get a full picture of what expressions are live and kicking
   //we have to walk the entire extended block rather than the block that
   //happened to contain the callNode
   block1 = block1->startOfExtendedBlock();
   static char *disableProfiledGuardRemat = feGetEnv("TR_DisableProfiledGuardRemat");

   bool suitableForRemat = !comp()->getOption(TR_DisableGuardedCallArgumentRemat) && comp()->getProfilingMode() != JitProfiling;
   if (suitableForRemat)
      {
      if (guard->_kind == TR_NoGuard)
         {
         suitableForRemat = false;
         }
      else if (guard->_kind == TR_ProfiledGuard)
         {
         if (disableProfiledGuardRemat)
            {
            suitableForRemat = false;
            }
         else
            {
            suitableForRemat = getPolicy()->suitableForRemat(comp(), callNode, guard);
            if (!suitableForRemat)
               {
               debugTrace(tracer(),"  skipping remat on profiled guard [%p] due to policy decision",guard);
               }
            }
         }
      }

   if (suitableForRemat)
      {
      static char *dumpRematTrees = feGetEnv("TR_DumpPrivArgRematTrees");
      TR::TreeTop *prevTree, *argStoreTree;

      TR::SparseBitVector scanTargets(comp()->allocator());
      RematSafetyInformation argSafetyInfo(comp());
      TR::list<TR::TreeTop *> failedArgs(getTypedAllocator<TR::TreeTop*>(comp()->allocator()));
      for (
         prevTree=NULL, argStoreTree = tif.getParameterMapper().firstTempTreeTop();
         prevTree != tif.getParameterMapper().lastTempTreeTop();
         prevTree = argStoreTree, argStoreTree = argStoreTree->getNextTreeTop())
         {
         TR::Node *argStore = argStoreTree->getNode();
         if (argStore->chkIsPrivatizedInlinerArg())
            {
            debugTrace(tracer(),"  considering priv arg store node [%p] - %d - for remat",argStore,argStore->getGlobalIndex());
            if (dumpRematTrees)
               comp()->getDebug()->print(comp()->getOutFile(),argStoreTree);
            TR::SparseBitVector argSymRefsToCheck(comp()->allocator());
            TR_YesNoMaybe result = RematTools::gatherNodesToCheck(comp(), argStore, argStore->getFirstChild(), scanTargets, argSymRefsToCheck, tracer()->debugLevel());
            if (result == TR_yes)
               {
               debugTrace(tracer(),"    priv arg remat may be possible for node [%p] - %d",argStore,argStore->getGlobalIndex());
               argSafetyInfo.add(argStoreTree, argSymRefsToCheck);
               }
            else if (result == TR_no)
               {
               debugTrace(tracer(),"    priv arg remat unsafe for node [%p] - %d",argStore,argStore->getGlobalIndex());
               failedArgs.push_back(argStoreTree);
               }
            }
         }

      // if we have failed to remat any arguments we want to see if there is another
      // store of the argument that we can use for partial remat purposes - hibb often
      // makes these for us
      if (failedArgs.size() > 0)
         {
         RematTools::walkTreeTopsCalculatingRematFailureAlternatives(comp(),
            block1->getFirstRealTreeTop(),
            tif.getParameterMapper().firstTempTreeTop()->getNextTreeTop(),
            failedArgs, scanTargets, argSafetyInfo, tracer()->debugLevel());

         for (auto iter = failedArgs.begin(); iter != failedArgs.end(); ++iter)
            {
            // NULL means we actually do have a candidate load to try and to
            // partially rematerialize the argument
            if (!*iter)
               continue;

            argStoreTree = *iter;//failedArgs.get(i);
            TR::Node *argStore = argStoreTree->getNode();
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privInlinerArg/Failed/%s", argStore->getFirstChild()->getOpCode().getName()), argStoreTree, 1, TR::DebugCounter::Expensive);
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byMethod/Failed/(%s)/%s", comp()->signature(), argStore->getFirstChild()->getOpCode().getName()));
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byReason/%s", argStore->getFirstChild()->getOpCode().getName()));
            }
         }

      debugTrace(tracer(),"  priv arg remat: symref checking walk required = %d", !scanTargets.IsZero());

      TR::SparseBitVector unsafeSymRefs(comp()->allocator());
      if (!scanTargets.IsZero())
         {
         RematTools::walkTreesCalculatingRematSafety(comp(), block1->getFirstRealTreeTop(),
            tif.getParameterMapper().lastTempTreeTop()->getNextTreeTop(),
            scanTargets, unsafeSymRefs, tracer()->debugLevel());
         }

      debugTrace(tracer(),"  priv arg remat: Proceeding with block_%d rematPoint=[%p]", rematPoint->getEnclosingBlock()->getNumber(), rematPoint);

      if (comp()->getOption(TR_DebugInliner))
         argSafetyInfo.dumpInfo(comp());
      for (uint32_t i = 0; i < argSafetyInfo.size(); ++i)
         {
         TR::TreeTop *argStoreTree = argSafetyInfo.argStore(i);
         TR::TreeTop *rematTree = argSafetyInfo.rematTreeTop(i);
         TR::Node *argStore = argStoreTree->getNode();
         if (!unsafeSymRefs.Intersects(argSafetyInfo.symRefDependencies(i)))
            {
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byMethod/(%s)/Succeeded", comp()->signature()));
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byReason/Success"));
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privInlinerArg/Succeeded/(%s)", argStore->getFirstChild()->getOpCode().getName()), argStoreTree, 1, TR::DebugCounter::Expensive);
            // equality of rematTree and argStoreTree means we want to duplicate the computation of
            // the argument and do a full rematerialization
            if (rematTree == argStoreTree)
               {
               TR::Node *duplicateStore = argStore->duplicateTree();
               if (performTransformation(comp(), "O^O GUARDED CALL REMAT: Rematerialize [%p] as [%p]\n", argStore, duplicateStore))
                  {
                  rematPoint = TR::TreeTop::create(comp(), rematPoint, duplicateStore);
                  }
               }
            // otherwise we are doing a partial remat where we are going to load from another temp
            else
               {
               TR::Node *duplicateStore = TR::Node::createStore(argStore->getSymbolReference(), TR::Node::createLoad(argStore, rematTree->getNode()->getSymbolReference()));
               duplicateStore->setByteCodeInfo(argStore->getByteCodeInfo());
               if (performTransformation(comp(), "O^O GUARDED CALL REMAT: Partial rematerialize of [%p] as [%p] - load of [%d]\n", argStore, duplicateStore, rematTree->getNode()->getSymbolReference()->getReferenceNumber()))
                  {
                  rematPoint = TR::TreeTop::create(comp(), rematPoint, duplicateStore);
                  }
               }
            }
         else
            {
            debugTrace(tracer()," priv arg remat failed for node [%p] - %d - due to data dependencies", argStoreTree->getNode(), argStoreTree->getNode()->getGlobalIndex());
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byMethod/unsafeSymRef/Failed/(%s)/%s", comp()->signature(), argStore->getFirstChild()->getOpCode().getName()));
            TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privatizedInlinerArgs.byReason/unsafeSymRef/%s", argStore->getFirstChild()->getOpCode().getName()));
            TR::DebugCounter::prependDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "privInlinerArg/Failed/(%s)", argStore->getFirstChild()->getOpCode().getName()), argStoreTree, 1, TR::DebugCounter::Expensive);
            }
         }
      }
   }

void
TR_InlinerBase::addAdditionalGuard(TR::Node *callNode, TR::ResolvedMethodSymbol * calleeSymbol, TR_OpaqueClassBlock * thisClass, TR::Block *prevBlock, TR::Block *inlinedBody, TR::Block *slowPath, TR_VirtualGuardKind kind, TR_VirtualGuardTestType type, bool favourVFTCompare, TR::CFG *callerCFG)
   {
   TR::Block *guardBlock = TR::Block::createEmptyBlock(callNode, comp(), prevBlock->getFrequency());
      callerCFG->addNode(guardBlock);
      callerCFG->addEdge(prevBlock, guardBlock);
      callerCFG->addEdge(guardBlock, inlinedBody);
      callerCFG->addEdge(guardBlock, slowPath);
      callerCFG->copyExceptionSuccessors(prevBlock, guardBlock);
      callerCFG->removeEdge(prevBlock, inlinedBody);

      TR_VirtualGuardSelection *guard = new (trStackMemory()) TR_VirtualGuardSelection(kind, type);
      TR::TreeTop *tt = guardBlock->append(TR::TreeTop::create(comp(),
                                    createVirtualGuard(callNode, calleeSymbol, slowPath->getEntry(),
                                       calleeSymbol->getFirstTreeTop()->getNode()->getInlinedSiteIndex(),
                                       thisClass, favourVFTCompare, guard)));
      guardBlock->setDoNotProfile();
      prevBlock->getExit()->join(guardBlock->getEntry());
      guardBlock->getExit()->join(inlinedBody->getEntry());
   }

TR::TreeTop *
TR_InlinerBase::addGuardForVirtual(
   TR::ResolvedMethodSymbol * callerSymbol,
   TR::ResolvedMethodSymbol * calleeSymbol,
   TR::TreeTop * callNodeTreeTop,
   TR::Node * callNode,
   TR_OpaqueClassBlock * thisClass,
   TR::TreeTop * startOfInlinedCall,
   TR::TreeTop * previousBBStartInCaller,
   TR::TreeTop * nextBBEndInCaller,
   TR_TransformInlinedFunction & tif,
   List<TR::SymbolReference> & tempList,
   TR_VirtualGuardSelection *guard,
   TR::TreeTop **induceOSRCallTree,
   TR_CallTarget *calltarget)
   {
   // generate:
   //
   //  (caller function treetops preceeding call site)
   //
   //  (storesOfArgumentsToTemps)
   //  ificmpeq Destination BBStart [4]
   //    iload inline_virtual_bit
   //    iconst = 0
   //  BBEnd [1]
   //  BBStart [2]
   //  inlined call
   //  store result in a temp (if there's a return value)
   //  BBEnd     (if the inlined call has blocks then this is the exit for the last block in the inlined code)
   //  BBStart [3]
   //
   //  (rest of the caller function)
   //
   //  BBStart [4]
   //  treetop
   //    existing call
   //  store result in a temp (if there's a return value)
   //  goto Destination BBStart [3]
   //  BBEnd [4]
   //
   TR::CFG * callerCFG = callerSymbol->getFlowGraph();

   TR::Block * block1 = previousBBStartInCaller->getNode()->getBlock();
   TR::Block * block2;
   TR::Block * hcrBlock;
   bool createdHCRGuard = false;
   bool createdHCRAndVirtualGuard = false;
   TR_VirtualGuardSelection *hcrGuard;
   TR::TreeTop * hcrTreeTop;
   block2 = block1->split(startOfInlinedCall, callerCFG);
   TR::Block * block3 = nextBBEndInCaller->getNode()->getBlock()->split(callNodeTreeTop, callerCFG);
   TR::Block * block4 = TR::Block::createEmptyBlock(callNode, comp());

   TR::TreeTop *failedCallInsertionPoint = block4->getEntry()->getNextTreeTop();

   callerCFG->addNode(block4);
   callerCFG->addEdge(block1, block4);
   callerSymbol->getLastTreeTop(block3)->join(block4->getEntry());
   callerCFG->copyExceptionSuccessors(block1, block4);
   callerCFG->copyExceptionSuccessors(block1, block3);

   // Don't profile blocks 3 and 4 if they have the same bytecode info (and
   // hence will use the same profiling slot) as block 1.
   //
   if (blocksHaveSameStartingByteCodeInfo(block1, block3))
      block3->setDoNotProfile();
   if (blocksHaveSameStartingByteCodeInfo(block1, block4))
      block4->setDoNotProfile();


   TR::TreeTop * virtualGuard = NULL;
   if (guard->_kind != TR_InnerGuard)
      virtualGuard = block1->append
                     (TR::TreeTop::create
                      (comp(), createVirtualGuard
                       (callNode, calleeSymbol, block4->getEntry(),
                        calleeSymbol->getFirstTreeTop()->getNode()->getInlinedSiteIndex(),
                        thisClass, tif.favourVftCompare(), guard)));


   static const char *disableHCRGuards = feGetEnv("TR_DisableHCRGuards");

   const bool skipHCRGuardForCallee = getPolicy()->skipHCRGuardForCallee(calleeSymbol->getResolvedMethod());

   bool skipHCRGuardCreation = false;

   // addGuardForVirtual: create an HCRGuard after the original guard
   if (!disableHCRGuards && comp()->getHCRMode() != TR::none && guard->_kind != TR_HCRGuard && !skipHCRGuardForCallee)
      {
      createdHCRAndVirtualGuard = true;

      // we merge virtual guards and OSR guards for simplicity in most modes
      // when using OSR to implement HCR we keep the HCR guards distinct since they
      // will undergo special processing later in the compilation
      if (virtualGuard &&
          comp()->cg()->supportsMergingGuards())
         {
         TR::Node *guardNode = virtualGuard->getNode();
         if (guardNode)
            {
            TR_VirtualGuard *virtualGuard = comp()->findVirtualGuardInfo(guardNode);
            if (virtualGuard)
               {
               virtualGuard->setMergedWithHCRGuard();
               skipHCRGuardCreation = true;
               }
            }
         }

      if (!skipHCRGuardCreation)
         {
         hcrBlock = TR::Block::createEmptyBlock(callNode, comp(), block1->getFrequency());
         callerCFG->addNode(hcrBlock);
         callerCFG->addEdge(block1, hcrBlock);
         callerCFG->addEdge(hcrBlock, block2);
         callerCFG->addEdge(hcrBlock, block4);
         callerCFG->copyExceptionSuccessors(block1, hcrBlock);
         callerCFG->removeEdge(block1, block2);
         hcrGuard = new (trStackMemory()) TR_VirtualGuardSelection(TR_HCRGuard, TR_NonoverriddenTest);
         hcrTreeTop = hcrBlock->append(TR::TreeTop::create(comp(),
                                       createVirtualGuard(callNode, calleeSymbol, block4->getEntry(),
                                          calleeSymbol->getFirstTreeTop()->getNode()->getInlinedSiteIndex(),
                                          thisClass, tif.favourVftCompare(), hcrGuard)));
         hcrBlock->setDoNotProfile();
         block1->getExit()->join(hcrBlock->getEntry());
         hcrBlock->getExit()->join(block2->getEntry());
         // printf("Inserting a HCRGuard %p after virtual guard %p in %s\n", hcrBlock, block1, comp()->signature());
         }
      }
   else if (!disableHCRGuards && comp()->getHCRMode() != TR::none)
      createdHCRGuard = true;

   static const char *disableFSDGuard = feGetEnv("TR_DisableFSDGuard");
   if ( !disableFSDGuard && comp()->getOption(TR_FullSpeedDebug) && guard->_kind != TR_BreakpointGuard)
      {
      addAdditionalGuard(callNode, calleeSymbol, thisClass, block1, block2, block4, TR_BreakpointGuard, TR_FSDTest, false /*favourVftCompare*/,callerCFG);
      }

   bool appendTestToBlock1 = false;
   if (guard->_kind == TR_InnerGuard)
      appendTestToBlock1 = true;

   TR::TreeTop *cursorTree = block1->getLastRealTreeTop();
   if (cursorTree->getNode()->getOpCodeValue() != TR::BBStart)
      cursorTree = cursorTree->getPrevTreeTop();

   getUtil()->refineInlineGuard(callNode, block1, block2, appendTestToBlock1, callerSymbol, cursorTree, virtualGuard, block4);

   if ((guard->_kind == TR_ProfiledGuard) || (guard->_kind == TR_InnerGuard))
      {
      if (block1->getFrequency() < 0)
         block4->setFrequency(block1->getFrequency());
      else
         {
         if (guard->isHighProbablityProfiledGuard())
            block4->setFrequency(MAX_COLD_BLOCK_COUNT+1);
         else
            block4->setFrequency(TR::Block::getScaledSpecializedFrequency(block1->getFrequency()));
         }
      }
   else
      {
      block4->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
      block4->setIsCold();
      }

   // store result in a temp (if there's a return value)
   //
   // If while transforming the inlined function a temp was
   // created for the return value then that temp (resultTempSymRef)
   // is used.
   //
   TR::SymbolReference * resultTempSymRef = tif.resultTempSymRef();
   if (tif.resultNode())
      OMR_InlinerUtil::storeValueInATemp(comp(), tif.resultNode(), resultTempSymRef, block3->getPrevBlock()->getLastRealTreeTop(), callerSymbol, tempList, _availableTemps, &_availableBasicBlockTemps);

   //  existing call
   //
   TR::Node* newTreeTopNode = (callNodeTreeTop->getNode()->getOpCode().isCheck()) ?
      TR::Node::createWithSymRef(callNodeTreeTop->getNode()->getOpCodeValue(), 1, callNode, 0, callNodeTreeTop->getNode()->getSymbolReference()) :

      TR::Node::create(TR::treetop, 1, callNode);


   TR::TreeTop* guardedCallNodeTreeTop = TR::TreeTop::create(comp(), newTreeTopNode);
   block4->append(guardedCallNodeTreeTop);

   callNode->setIsTheVirtualCallNodeForAGuardedInlinedCall();
   TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/succeeded:guardedCallee", block4->getLastRealTreeTop());

   // if this is postExecution OSR, the call will be followed by a pending push store of its result.
   // this is necessary to ensure the stack has the correct contents when it transitions, therefore, it
   // is necessary to add the store here as well
   //
   if (comp()->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      TR::TreeTop *cursor = callNodeTreeTop->getNextTreeTop();
      TR_ByteCodeInfo bci = callNode->getByteCodeInfo();
      while (cursor && comp()->getMethodSymbol()->isOSRRelatedNode(cursor->getNode(), bci))
         {
         if (cursor->getNode()->getOpCode().isStoreDirect() && cursor->getNode()->getFirstChild() == callNode)
            {
            debugTrace(tracer(),"  virtual call node pps: Pending push store of call [%p] found: [%p]", callNode, cursor->getNode());
            block4->append(TR::TreeTop::create(comp(), TR::Node::createStore(cursor->getNode()->getSymbolReference(), callNode)));
            break;
            }
         debugTrace(tracer(),"  virtual call node pps: Skipping node [%p] whilst searching for store of call [%p]", cursor->getNode(), callNode);
         cursor = cursor->getNextTreeTop();
         }
      }

   //  store result in a temp (if there's a return value)
   //
   TR_ASSERT(!tif.simpleCallReferenceTreeTop() || !resultTempSymRef, "both simpleCallReferenceTreeTop and resultTempSymRef are set");
   TR_ASSERT(!resultTempSymRef || callNode->getReferenceCount() > 2, "why do we have a resultTempSymRef when the callNode isn't referenced again?");

   if (tif.simpleCallReferenceTreeTop())
      cloneAndReplaceCallNodeReference(tif.simpleCallReferenceTreeTop(), 0, 0, block4->getLastRealTreeTop(), comp());

   else if (callNode->getReferenceCount() > 2)
      OMR_InlinerUtil::storeValueInATemp(comp(), callNode, resultTempSymRef, block4->getLastRealTreeTop(), callerSymbol, tempList, _availableTemps, &_availableBasicBlockTemps);

   TR::CFGEdge *edge4 = NULL;
   TR::TreeTop *lastTreeInBlock4 = NULL;
   if (block4->getLastRealTreeTop()->getNode()->getOpCode().isReturn())
      {
      lastTreeInBlock4 = block4->getLastRealTreeTop();
      callerCFG->addEdge(block4, callerCFG->getEnd());
      }
   else
      {
      lastTreeInBlock4 = block4->append(TR::TreeTop::create(comp(), TR::Node::create(callNode, TR::Goto, 0, block3->getEntry())));
      edge4 = callerCFG->addEdge(block4, block3);
      }

   //  return the temp to be used to replace the call node and
   //  references to the call node with temp
   //
   if (resultTempSymRef)
      {
      TR::Node * loadOfResultTemp = TR::Node::createLoad(callNode, resultTempSymRef);
      tif.setResultNode(loadOfResultTemp);
      }

   rematerializeCallArguments(tif, guard, callNode, block1,
                              block4->getFirstRealTreeTop()->getPrevTreeTop());

   debugTrace(tracer(), "Updating fields for callsite %p: \n"
      " _callNodeTreeTop %p -> %p , _parent %p -> %p\n",
      calltarget->_myCallSite,
      calltarget->_myCallSite->_callNodeTreeTop,
      guardedCallNodeTreeTop,
      calltarget->_myCallSite->_callNodeTreeTop->getNode(),
      guardedCallNodeTreeTop->getNode()
      );
   //
   calltarget->_myCallSite->_callNodeTreeTop = guardedCallNodeTreeTop;
   calltarget->_myCallSite->_parent = guardedCallNodeTreeTop->getNode();


   static const char *osrForHCRGuardsOfIndirect = feGetEnv("TR_OSRForHCRAndIndirect");
   static const char *osrAll = feGetEnv("TR_OSRAll");
   static const char *osrForNonHCRGuards = feGetEnv("TR_OSRForNonHCR");

   bool shouldAttemptOSR = true;
   if ((guard->_kind == TR_ProfiledGuard) ||
       (guard->_kind == TR_HierarchyGuard))
      shouldAttemptOSR = false;

   // a failed guard can be handled in one of two ways: 1) we simply branch to a block which will
   // do a virtual call on the correct receiver for the method we want to run or 2) we could
   // transfer control from the compiled code back to the interpreter using an On-Stack-Replacement
   // (OSR) mechanism.
   //
   // Hot-Code-Replace or HCR mode is where the compiler is running in a mode that assumes methods
   // could be redefined at runtime. The compiler can support this mode using traditional virtual
   // guards with calls or by using OSR elsewhere in the compiler.
   //
   // When running with HCR implemented using OSR plain HCR guards will be processed later in the
   // compilation and those later processes will handle them using OSR so we don't want to complicate
   // that with additional OSR at this point
   if ((comp()->getHCRMode() != TR::osr || guard->_kind != TR_HCRGuard)
       && callNode->getSymbolReference()->getOwningMethodSymbol(comp())->supportsInduceOSR(callNode->getByteCodeInfo(), block1, comp(), false))
      {
      bool shouldUseOSR = heuristicForUsingOSR(callNode, calleeSymbol, callerSymbol, createdHCRAndVirtualGuard);

      if (shouldUseOSR &&
          (osrAll ||
           comp()->getOption(TR_EnableOSROnGuardFailure) ||
          (createdHCRAndVirtualGuard && shouldAttemptOSR && osrForHCRGuardsOfIndirect) ||
           createdHCRGuard ||
           (osrForNonHCRGuards && shouldAttemptOSR)))
         {
         // Late inlining may result in callerSymbol not being the resolved method that actually calls the inlined method
         // This is problematic for linking OSR blocks
         TR::ResolvedMethodSymbol *callingMethod = callNode->getByteCodeInfo().getCallerIndex() == -1 ?
            comp()->getMethodSymbol() : comp()->getInlinedResolvedMethodSymbol(callNode->getByteCodeInfo().getCallerIndex());

         TR::TreeTop *induceTree = callingMethod->genInduceOSRCall(guardedCallNodeTreeTop, callNode->getByteCodeInfo().getCallerIndex(), (callNode->getNumChildren() - callNode->getFirstArgumentIndex()), false, false, callerSymbol->getFlowGraph());
         if (induceOSRCallTree)
            *induceOSRCallTree = induceTree;
         }
      }

   return virtualGuard;
   }


bool TR_InlinerBase::heuristicForUsingOSR(TR::Node *callNode, TR::ResolvedMethodSymbol *calleeSymbol, TR::ResolvedMethodSymbol *callerSymbol, bool isIndirectCall)
   {
   static const char *disallowNestedOSR = feGetEnv("TR_DisallowNestedOSR");

   static const char *OSRDepth;
   static int32_t maxOSRDepth = (OSRDepth = feGetEnv("TR_MaxOSRDepth")) ? atoi(OSRDepth) : 100000;

   static const char *OSRThreshold;
   static int32_t calleeThresh = (OSRThreshold = feGetEnv("TR_OSRCalleeThreshold")) ? atoi(OSRThreshold) : 100000;

   static const char *OSRIndirectThreshold;
   static int32_t calleeIndirectThresh = (OSRIndirectThreshold = feGetEnv("TR_OSRIndirectCalleeThreshold")) ? atoi(OSRIndirectThreshold) : 100000;

   static const char *OSRStackThreshold;
   static int32_t calleeStackThresh = (OSRStackThreshold = feGetEnv("TR_OSRCalleeStackThreshold")) ? atoi(OSRStackThreshold) : 100000;

   static const char *OSRThreshold2;
   static int32_t callerThresh = (OSRThreshold2 = feGetEnv("TR_OSRCallerThreshold")) ? atoi(OSRThreshold2) : 100000;

   static const char *OSRStackThreshold2;
   static int32_t callerStackThresh = (OSRStackThreshold2 = feGetEnv("TR_OSRCallerStackThreshold")) ? atoi(OSRStackThreshold2) : 100000;

   static const char *OSRStackThreshold3;
   static int32_t callerLiveStackThresh = (OSRStackThreshold3 = feGetEnv("TR_OSRCallerLiveStackThreshold")) ? atoi(OSRStackThreshold3) : 100000;

   static const char *OSRStackThreshold4;
   static int32_t callerLivePendingThresh = (OSRStackThreshold4 = feGetEnv("TR_OSRCallerLivePendingThreshold")) ? atoi(OSRStackThreshold4) : 100000;

   TR_ResolvedMethod *calleeMethod = calleeSymbol->getResolvedMethod();
   int32_t calleeNumStackSlots = calleeMethod->numberOfParameterSlots() + calleeMethod->numberOfTemps() + calleeMethod->numberOfPendingPushes();

   TR_ByteCodeInfo &byteCodeInfo = callNode->getByteCodeInfo();
   int32_t byteCodeIndex = byteCodeInfo.getByteCodeIndex();
   int32_t callSite = byteCodeInfo.getCallerIndex();

   if (calleeNumStackSlots > calleeStackThresh)
      return false;

   if (getPolicy()->getInitialBytecodeSize(calleeSymbol, comp()) > calleeThresh)
      return false;

   if (disallowNestedOSR)
      {
      if (callNode->getByteCodeInfo().getCallerIndex() != -1)
         return false;
      }

   int32_t totalOSRCallersSize = 0;
   int32_t totalOSRCallersStackSlots = 0;
   int32_t totalOSRCallersLiveStackSlots = 0;
   int32_t totalOSRLivePendingPushSlots = 0;
   int32_t depth = 0;
   TR_OSRMethodData *osrMethodData = comp()->getOSRCompilationData()->findCallerOSRMethodData(comp()->getOSRCompilationData()->findOrCreateOSRMethodData(comp()->getCurrentInlinedSiteIndex(), calleeSymbol));
   TR_OSRMethodData *finalOsrMethodData = NULL;
   while (osrMethodData)
      {
      depth++;
      if (depth > maxOSRDepth)
         {
         return false;
         }

      TR::ResolvedMethodSymbol *osrCallerSymbol = osrMethodData->getMethodSymbol();
      int32_t osrCallerSize = getPolicy()->getInitialBytecodeSize(osrCallerSymbol, comp());
      totalOSRCallersSize = totalOSRCallersSize + osrCallerSize;

      TR_ResolvedMethod *osrCallerMethod = osrCallerSymbol->getResolvedMethod();
      int32_t osrCallerNumStackSlots = osrCallerMethod->numberOfParameterSlots() + osrCallerMethod->numberOfTemps() + osrCallerMethod->numberOfPendingPushes();
      int32_t osrCallerNumLiveStackSlots = 0;
      totalOSRCallersStackSlots = totalOSRCallersStackSlots + osrCallerNumStackSlots;

      TR_BitVector *deadSymRefs = osrMethodData->getLiveRangeInfo(byteCodeIndex);
      if (deadSymRefs)
         {
         osrCallerNumLiveStackSlots = osrMethodData->getNumSymRefs() - deadSymRefs->elementCount();
         totalOSRCallersLiveStackSlots = totalOSRCallersLiveStackSlots + osrCallerNumLiveStackSlots;
         }
      else
         {
         osrCallerNumLiveStackSlots = osrMethodData->getNumSymRefs();
         totalOSRCallersLiveStackSlots = totalOSRCallersLiveStackSlots + osrCallerNumLiveStackSlots;
         }

      TR_Array<List<TR::SymbolReference> > *pendingPushSymRefs = osrCallerSymbol->getPendingPushSymRefs();
      int32_t numPendingSlots = 0;
      int32_t numLivePendingPushSlots = 0;
      if (pendingPushSymRefs)
         numPendingSlots = pendingPushSymRefs->size();

      for (int32_t i=0;i<numPendingSlots;i++)
         {
         List<TR::SymbolReference> symRefsAtThisSlot = (*pendingPushSymRefs)[i];

         if (symRefsAtThisSlot.isEmpty()) continue;

         ListIterator<TR::SymbolReference> symRefsIt(&symRefsAtThisSlot);
         TR::SymbolReference *nextSymRef;
         for (nextSymRef = symRefsIt.getCurrent(); nextSymRef; nextSymRef=symRefsIt.getNext())
            {
            if (!deadSymRefs || !deadSymRefs->get(nextSymRef->getReferenceNumber()))
               numLivePendingPushSlots++;
            }
         }

      totalOSRLivePendingPushSlots = totalOSRLivePendingPushSlots + numLivePendingPushSlots;

      if (comp()->getOption(TR_TraceOSR))
         traceMsg(comp(), "OSR caller at inlined site index %d has %d bytecodes and %d stack slots, total callers bytecodes %d total callers stack slots %d total callers live stack slots %d total pending push slots %d\n", osrMethodData->getInlinedSiteIndex(), osrCallerSize, osrCallerNumStackSlots, totalOSRCallersSize, totalOSRCallersStackSlots, totalOSRCallersLiveStackSlots, totalOSRLivePendingPushSlots);

      if (totalOSRLivePendingPushSlots > callerLivePendingThresh)
         {
         //const char * mSignature = calleeSymbol->signature(comp()->trMemory());
         //printf("avoid OSR at call %p generated for %s callerLiveStackThresh %d \n",
         //              callNode, mSignature, callerLiveStackThresh);
         return false;
         break;
         }

      if (totalOSRCallersLiveStackSlots > callerLiveStackThresh)
         {
         //const char * mSignature = calleeSymbol->signature(comp()->trMemory());
         //printf("avoid OSR at call %p generated for %s callerLiveStackThresh %d \n",
         //              callNode, mSignature, callerLiveStackThresh);
         return false;
         break;
         }

      if (totalOSRCallersStackSlots > callerStackThresh)
         {
         return false;
         break;
         }

      if (totalOSRCallersSize > callerThresh)
         {
         return false;
         break;
         }

      if (osrMethodData->getInlinedSiteIndex() > -1)
         {
         TR_InlinedCallSite &callSiteInfo = comp()->getInlinedCallSite(callSite);
         callSite = callSiteInfo._byteCodeInfo.getCallerIndex();
         osrMethodData = comp()->getOSRCompilationData()->findCallerOSRMethodData(osrMethodData);
         byteCodeIndex = callSiteInfo._byteCodeInfo.getByteCodeIndex();
         }
      else
         osrMethodData = NULL;
      }

   return true;
   }


bool rematerializeConstant(TR::Node *node, TR::Compilation *comp)
   {
   bool rematConst = false;

   if (node->getOpCode().isLoadConst())
      {
      rematConst = true;
      }
   else if (node->getOpCodeValue() == TR::loadaddr)
      {
      rematConst = true;
      }
   else
      {
      rematConst = false;
      }
   return rematConst;
   }

///////////////////////////////////////////////////////////////
// TR_ParameterToArgumentMapper
///////////////////////////////////////////////////////////////

TR_ParameterToArgumentMapper::TR_ParameterToArgumentMapper(
   TR::ResolvedMethodSymbol * callerSymbol, TR::ResolvedMethodSymbol * calleeSymbol, TR::Node * callNode,
   List<TR::SymbolReference> & temps, List<TR::SymbolReference> & availableTemps, List<TR::SymbolReference> & availableTemps2,
   TR_InlinerBase *inliner)
   : _callerSymbol(callerSymbol),
     _calleeSymbol(calleeSymbol),
     _callNode(callNode),
     _tempList(temps),
     _availableTemps(availableTemps),
     _availableTemps2(availableTemps2),
     _vftReplacementSymRef(0),
     _lastTempTreeTop(0),
     _firstTempTreeTop(0),
     _inliner(inliner)
   {
   }

void
TR_ParameterToArgumentMapper::printMapping()
   {
   if(!tracer()->debugLevel())
      return;
   for (TR_ParameterMapping * pm = _mappings.getFirst(); pm; pm = pm->getNext())
      {
      debugTrace(tracer(),"Mapping at addr %p:\n\tparmSymbol = %p (offset %d) \treplacementSymRef = %d\t_parameterNode = %p\treplacementSymRef2 = %d\treplacementSymRef3 = %d\n"
                           "\t_argIndex = %d\t_parmIsModified = %d\t_isConst = %d\t_addressTaken =%d",
                           pm, pm->_parmSymbol,pm->_parmSymbol->getOffset(),pm->_replacementSymRef ? pm->_replacementSymRef->getReferenceNumber() : -1, pm->_parameterNode,
                           pm->_replacementSymRef2 ? pm->_replacementSymRef2->getReferenceNumber() : -1, pm->_replacementSymRef3 ? pm->_replacementSymRef3->getReferenceNumber() : -1,
                           pm->_argIndex, pm->_parmIsModified, pm->_isConst, pm->_addressTaken);

      }
   }

void
TR_ParameterToArgumentMapper::initialize(TR_CallStack *callStack)
   {
   // create the TR_ParameterMapping chain and order it by offset
   //

   TR_InlinerDelimiter delimiter(tracer(),"pam.initialize");

   _inliner->createParmMap(_calleeSymbol,_mappings);

   lookForModifiedParameters();

   TR_ParameterMapping * parmMap = _mappings.getFirst();
   uint32_t argIndex = _callNode->getFirstArgumentIndex();

   if (_callNode->getOpCode().isCallIndirect())
      {
      TR::Node * arg = _callNode->getChild(0);
      if (arg->getReferenceCount() > 1)
         {
         TR::TreeTop *newStoreTreeTop = NULL;
         _firstTempTreeTop = _lastTempTreeTop = OMR_InlinerUtil::storeValueInATemp(comp(), arg, _vftReplacementSymRef, 0, _calleeSymbol,
                                                _tempList, _availableTemps, &_availableTemps2, false, &newStoreTreeTop);
         _firstTempTreeTop->getNode()->setIsPrivatizedInlinerArg(true);
         if (newStoreTreeTop)
            {
            _firstTempTreeTop->join(newStoreTreeTop);
            _lastTempTreeTop = newStoreTreeTop;
            }
         _vftReplacementSymRef->getSymbol()->setBehaveLikeNonTemp();
         }
      }

   bool hasStaticCallStack = _callNode->getSymbol()->isStatic();
   for (TR_CallStack *cs = callStack; hasStaticCallStack && cs; cs = cs->getNext())
      hasStaticCallStack = cs->_method->isStatic();

   bool neverNeedPrivatizedArguments = _inliner->getPolicy()->dontPrivatizeArgumentsForRecognizedMethod(_callNode->getSymbol()->castToMethodSymbol()->getRecognizedMethod());

   for (uint32_t argOffset = 0; parmMap; ++argIndex)
      {
      TR_ASSERT((argIndex < _callNode->getNumChildren()), "Inlining, not able to match-up parameters and arguments\n");

      // If the parameter isn't referenced it won't be in our parameter list,
      // so we have to check offsets and skip unused arguments
      //
      TR::Node * arg = _callNode->getChild(argIndex);

      if (parmMap->_parmSymbol->getParameterOffset() == argOffset)
         {
         parmMap->_argIndex = argIndex;
         if (!parmMap->_parmIsModified && (!arg->getOpCode().isFloatingPoint() || comp()->cg()->getSupportsJavaFloatSemantics()))
            {
            if (parmMap->_addressTaken)
               {
               if (arg->getOpCode().isLoadVarDirect() && arg->getReferenceCount() == 1 &&
                     arg->getSymbol()->isAutoOrParm())
                  parmMap->_replacementSymRef = arg->getSymbolReference();
               }
            else
               {
               debugTrace(tracer(),"Setting parameterNode to n%in %s, argOffset=%d, argIndex=%d, _callNode n%in",
                     arg->getGlobalIndex(), arg->getOpCode().getName(), argOffset, argIndex, _callNode->getGlobalIndex());
               parmMap->_parameterNode = arg;

               static const char *disableParmTempOpt = feGetEnv("TR_DisableParmTempOpt");

               if (disableParmTempOpt)
                  {
                  if (arg->getOpCode().isLoadVarDirect() && arg->getReferenceCount() == 1 && arg->getSymbol()->isAutoOrParm())
                     parmMap->_replacementSymRef = arg->getSymbolReference();
                  else if (rematerializeConstant(arg, comp()))
                     parmMap->_isConst = true;
                  }
               else
                  {
                  if ( (arg->getOpCode().isLoadVarDirect() && arg->getSymbol()->isAutoOrParm()) &&
                       ( (arg->getReferenceCount() == 1)
                         || (_callNode->getOpCode().isCallIndirect() &&
                               (argIndex == _callNode->getFirstArgumentIndex()) && (_callNode->getFirstChild()->getNumChildren() > 0) &&
                               (arg == _callNode->getFirstChild()->getFirstChild()) && arg->getSymbol()->isAuto() && (arg->getReferenceCount() == 2))
                          ))
                     {
                     debugTrace(tracer(),"Setting parmMap %p -> _replacementSymref to %d",parmMap,arg->getSymbolReference()->getReferenceNumber());
                     debugTrace(tracer(),"isCallIndirect = %d getFirstArgumentIndex = %d  arg isAuto = %d",_callNode->getOpCode().isCallIndirect(),_callNode->getFirstArgumentIndex(),arg->getSymbol()->isAuto());
                     parmMap->_replacementSymRef = arg->getSymbolReference();
                     }
                  else if (rematerializeConstant(arg, comp()))
                     parmMap->_isConst = true;
                  }
               }
            }

         // tries to put inreg parameters into correct register
         TR::TreeTop * tt=NULL;
         TR::TreeTop * tt2=NULL;
         TR::TreeTop *newValueStoreTreeTop = NULL;
         if (parmMap->_isConst)
            {
            // create treetop for constant arg so that inliner can later store the arg into correct register
            tt = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, arg));
            }
         else
            {
            if (parmMap->_replacementSymRef)
               tt = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, arg));
            else
               {
               TR::SymbolReference * symRef = NULL;

                  {
                  tt = OMR_InlinerUtil::storeValueInATemp(comp(), arg, symRef, 0, _calleeSymbol, _tempList, _availableTemps, &_availableTemps2, false, &newValueStoreTreeTop);
                  }

               symRef->getSymbol()->setBehaveLikeNonTemp();
               // set flag only if there is a virtual guard
               if (!hasStaticCallStack && !neverNeedPrivatizedArguments
                     && tt->getNode()->getOpCode().isStoreDirectOrReg()) // compjazz 53912: PLX sometimes privatizes using indirect stores
                  {
                  tt->getNode()->setIsPrivatizedInlinerArg(true);
                  }
               parmMap->_replacementSymRef = symRef;
               }
            }

         if (_lastTempTreeTop)
            {
            if (newValueStoreTreeTop)
               {
               tt->join(newValueStoreTreeTop);
               _lastTempTreeTop->join(tt);
               _lastTempTreeTop = newValueStoreTreeTop;
               }
            else
               {
               _lastTempTreeTop->join(tt);
               _lastTempTreeTop = tt;
               }
            }
         else if (newValueStoreTreeTop)
            {
            _firstTempTreeTop = tt;
            _firstTempTreeTop->join(newValueStoreTreeTop);
            _lastTempTreeTop = newValueStoreTreeTop;
            }
         else
            _firstTempTreeTop = _lastTempTreeTop = tt;


         parmMap = parmMap->getNext();
         }

      argOffset += (uint32_t)((arg->getDataType()==TR::Address)?arg->getSize():((arg->getSize()/4) * TR::Compiler->om.sizeofReferenceAddress()));
      }
   }

void
TR_ParameterToArgumentMapper::lookForModifiedParameters()
   {
   TR_InlinerDelimiter delimiter(tracer(),"pam.lookForModifiedParameters");
   TR::TreeTop* tt = _calleeSymbol->getFirstTreeTop();
   for (; tt; tt = tt->getNextTreeTop())
      lookForModifiedParameters(tt->getNode());
   }

TR_ParameterMapping *
TR_ParameterToArgumentMapper::findMapping(TR::Symbol * symbol)
   {
   for (TR_ParameterMapping * pm = _mappings.getFirst(); pm; pm = pm->getNext())
      if (pm->_parmSymbol == symbol)
         return pm;
   return 0;
   }

void
TR_ParameterToArgumentMapper::lookForModifiedParameters(TR::Node * node)
   {
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      lookForModifiedParameters(child);
      }

   TR_ParameterMapping * parmMap;
   if (node->getOpCode().hasSymbolReference() && node->getSymbol()->isParm() && (parmMap = findMapping(node->getSymbol())))
      {
      if (node->getOpCode().isStoreDirect())
         parmMap->_parmIsModified = true;
      else if (node->getOpCodeValue() == TR::loadaddr)
         {
         parmMap->_addressTaken = true;
         }
      }
   }

/*
 * The OSRCallSiteRematTables for this inlined method and those inlined within it
 * may contain symbol references for parms that have been mapped to args. Therefore,
 * its necessary to update the tables based on the mapper.
 *
 * This will only be applied in voluntary OSR when induction is still possible.
 */
void
TR_ParameterToArgumentMapper::mapOSRCallSiteRematTable(uint32_t siteIndex)
   {
   if (!comp()->getOption(TR_EnableOSR) || comp()->getOSRMode() != TR::voluntaryOSR ||
       comp()->osrInfrastructureRemoved() || comp()->getOption(TR_DisableOSRCallSiteRemat))
      return;

   TR::SymbolReference *ppSymRef, *loadSymRef;
   for (uint32_t i = 0; i < comp()->getOSRCallSiteRematSize(siteIndex); ++i)
      {
      comp()->getOSRCallSiteRemat(siteIndex, i, ppSymRef, loadSymRef);

      // Only apply mapper to parms contained within remat table
      if (!ppSymRef || !loadSymRef)
         continue;
      TR::Symbol *symbol = loadSymRef->getSymbol();
      if (!symbol->isParm())
         continue;

      // Map the parms to new symrefs
      TR::ParameterSymbol *parm = symbol->getParmSymbol();
      TR_ParameterMapping * parmMap = _mappings.getFirst();
      for (; parmMap; parmMap = parmMap->getNext())
         if (symbol == parmMap->_parmSymbol)
            {
            if (parmMap->_isConst)
               {
               // Should be able to do const, current side table does not allow it
               comp()->setOSRCallSiteRemat(siteIndex, ppSymRef, NULL);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrCallSiteRemat/mapParm/const/(%s)",  comp()->signature()));
               }
            else if (loadSymRef->getOffset() > 0)
               {
               comp()->setOSRCallSiteRemat(siteIndex, ppSymRef, NULL);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrCallSiteRemat/mapParm/addr/(%s)",  comp()->signature()));
               }
            else
               {
               comp()->setOSRCallSiteRemat(siteIndex, ppSymRef, parmMap->_replacementSymRef);
               TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrCallSiteRemat/mapParm/success/(%s)",  comp()->signature()));
               }
            break;
            }

      if (!parmMap)
         TR::DebugCounter::incStaticDebugCounter(comp(), TR::DebugCounter::debugCounterName(comp(), "osrCallSiteRemat/mapParm/missing/(%s)",  comp()->signature()));
      }

   // Update the remat tables for calls within the current
   for (int32_t childIndex = 0; childIndex < comp()->getNumInlinedCallSites(); ++childIndex)
      {
      TR_InlinedCallSite &ics = comp()->getInlinedCallSite(childIndex);
      if (siteIndex == ics._byteCodeInfo.getCallerIndex())
         mapOSRCallSiteRematTable(childIndex);
      }
   }

TR::Node *
TR_ParameterToArgumentMapper::map(TR::Node * node, TR::ParameterSymbol * parm, bool seenBBStart)
   {
   TR_InlinerDelimiter delimiter(tracer(),"pam.map");

   for (TR_ParameterMapping * parmMap = _mappings.getFirst(); parmMap; parmMap = parmMap->getNext())
      if (parm == parmMap->_parmSymbol)
         {
         if (parmMap->_parameterNode && !seenBBStart)
            {
            parmMap->_parameterNode->incReferenceCount();
            return parmMap->_parameterNode;
            }
         if (parmMap->_isConst)
            {
            TR_ASSERT(parmMap->_parameterNode, "Inlining, expected to have a replacementNode");
            TR::Node * newNode = parmMap->_parameterNode->duplicateTree();
            node->decReferenceCount();
            newNode->setReferenceCount(1);
            return newNode;
            }

         if (parmMap->_addressTaken && parmMap->_replacementSymRef->getSymbol()->isAuto())
            {
            parmMap->_replacementSymRef->getSymbol()->setAutoAddressTaken();
            }
         intptrj_t offset= node->getSymbolReference()->getOffset();

         node->setSymbolReference(parmMap->_replacementSymRef);

         if (offset != 0)
            {
            TR_ASSERT(node->getOpCodeValue()==TR::loadaddr, "expecting a loadaddr node \n");
            TR::Node *addrNode = TR::Node::createAddConstantToAddress(node, offset);
            node->decReferenceCount();
            addrNode->setReferenceCount(1);
            return addrNode;
            }

         return node;
         }

   return 0;
   }

TR::Node *
TR_ParameterToArgumentMapper::fixCallNodeArgs(bool createNullCheckReference)
   {
   TR_InlinerDelimiter delimiter(tracer(),"pam.fixCallNodeArgs");
   if (_vftReplacementSymRef)
      {
      _callNode->getChild(0)->decReferenceCount();
      TR::Node *newLoad = TR::Node::createLoad(_callNode, _vftReplacementSymRef);
      _callNode->setAndIncChild(0, newLoad);
      }

   TR::Node * nullCheckReference = 0;
   for (TR_ParameterMapping * parmMap = _mappings.getFirst(); parmMap; parmMap = parmMap->getNext())
      {
      int32_t i = parmMap->_argIndex;
      if (parmMap->_replacementSymRef)
         {
         _callNode->getChild(i)->decReferenceCount();
         TR::Node *arg=TR::Node::createLoad(_callNode, parmMap->_replacementSymRef);
         _callNode->setAndIncChild(i, arg);
         }


      if ((i == _callNode->getFirstArgumentIndex()) &&
          !_vftReplacementSymRef && parmMap->_replacementSymRef &&
         _callNode->getOpCode().isCallIndirect())
         {
         TR::Node *vftNode = _callNode->getChild(0);
         if (vftNode->getOpCode().hasSymbolReference() &&
             (vftNode->getSymbolReference() == comp()->getSymRefTab()->findVftSymbolRef()))
            {
            TR::Node *oldVftChild = vftNode->getChild(0);
            TR::Node *newLoad = TR::Node::createLoad(_callNode, parmMap->_replacementSymRef);
            vftNode->setAndIncChild(0, newLoad);
            oldVftChild->recursivelyDecReferenceCount();
            }
         }

      if (createNullCheckReference && i == _callNode->getFirstArgumentIndex())
         {
         if (parmMap->_replacementSymRef)
            {
            nullCheckReference = TR::Node::createLoad(_callNode, parmMap->_replacementSymRef);
            }
         else if (parmMap->_isConst)
            {
            TR_ASSERT(parmMap->_parameterNode->getOpCodeValue() == TR::aconst, "a const this arg should be a const");
            TR_ASSERT(parmMap->_parameterNode->getInt() == 0, "a const this arg should be an aconst 0");
            nullCheckReference = TR::Node::aconst(_callNode, 0);
            }
         }
      else if (parmMap->_isConst && _callNode->getChild(i)->getReferenceCount() > 1)
         {
         _callNode->getChild(i)->decReferenceCount();
         _callNode->setAndIncChild(i, _callNode->getChild(i)->duplicateTree());
         }
      }

   return nullCheckReference;
   }

///////////////////////////////////////////////////////////////
// TR_TransformInlinedFunction
///////////////////////////////////////////////////////////////

TR_TransformInlinedFunction::TR_TransformInlinedFunction(
   TR::Compilation *c, TR_InlinerTracer *tracer,TR::ResolvedMethodSymbol * callerSymbol, TR::ResolvedMethodSymbol * calleeSymbol,
   TR::Block * callNodeBlock, TR::TreeTop * callNodeTreeTop, TR::Node * callNode,
   TR_ParameterToArgumentMapper & mapper, TR_VirtualGuardSelection *guard,
   List<TR::SymbolReference> & temps, List<TR::SymbolReference> & availableTemps,
   List<TR::SymbolReference> & availableTemps2)
   : _comp(c),
     _tracer(tracer),
     _callerSymbol(callerSymbol),
     _calleeSymbol(calleeSymbol),
     _callNodeTreeTop(callNodeTreeTop),
     _callNode(callNode),
     _tempList(temps),
     _availableTemps(availableTemps),
     _availableTemps2(availableTemps2),
     _parameterMapper(mapper),
     _resultNode(0),
     _resultTempSymRef(0),
     _firstBBEnd(0),
     _firstCatchBlock(0),
     _crossedBasicBlock(guard->_kind != TR_NoGuard),
     _generatedLastBlock(0),
     _determineIfReturnCanBeReplacedWithCallNodeReference(true),
     _simpleCallReferenceTreeTop(0),
     _processingExceptionHandlers(false),
     _treeTopsToRemove(c->trMemory()),
     _blocksWithEdgesToTheEnd(c->trMemory()),
     _traceVIP(c->getOption(TR_TraceVIP)),
     _favourVftCompare(false),
     findCallNodeRecursionDepth(0),
     onlyMultiRefNodeIsCallNodeRecursionDepth(0)
   {
   }

/**
 * this is the basic implementation and project specific implementation should always call the basic implementation first
 */
void
TR_TransformInlinedFunction::transform()
   {
   TR_InlinerDelimiter delimiter(tracer(),"tif.transform");
   TR_ResolvedMethod * calleeResolvedMethod = _calleeSymbol->getResolvedMethod();

   TR::Block *lastBlock  = NULL;
   TR::Block *b          = NULL;
   TR::Block *firstBlock = _calleeSymbol->getFirstTreeTop()->getNode()->getBlock();
   for (b = firstBlock; b; lastBlock = b, b = b->getNextBlock())
      if (!_firstCatchBlock)
         {
         if (b->isCatchBlock())
            _firstCatchBlock = b;
         else
            _lastMainLineTreeTop = b->getExit();
         }

   _penultimateTreeTop = lastBlock->getExit()->getPrevRealTreeTop();

   // If the first block has exception predecessors or multiply predecessors then we can't merge
   // the first block with the caller's block
   //
   if (comp()->getOption(TR_EnableJProfiling) ||
       (firstBlock->getPredecessors().size() > 1) ||
       firstBlock->hasExceptionSuccessors() ||
       comp()->fe()->isMethodEnterTracingEnabled(calleeResolvedMethod->getPersistentIdentifier()) ||
       TR::Compiler->vm.canMethodEnterEventBeHooked(comp()))
      {
      int32_t freq = firstBlock->getFrequency();
      firstBlock = _calleeSymbol->prependEmptyFirstBlock();
      firstBlock->setFrequency(freq);
      }

   TR::TreeTop * tt = _calleeSymbol->getFirstTreeTop()->getNextTreeTop();

   // If the last block doesn't end with a return (eg it's a throw or a goto) then we need
   // to keep the BBEnd.
   //
   TR::Node * penultimateNode = _penultimateTreeTop->getNode();
   if (!_penultimateTreeTop->getNode()->getOpCode().isReturn() || _firstCatchBlock)
      _generatedLastBlock = TR::Block::createEmptyBlock(penultimateNode, comp(), firstBlock->getFrequency(), firstBlock);
   comp()->incVisitCount();

   for (_currentTreeTop = tt; _currentTreeTop; _currentTreeTop = _currentTreeTop->getNextTreeTop())
      {
      transformNode(_currentTreeTop->getNode(), 0, 0);
      }

   _parameterMapper.mapOSRCallSiteRematTable(comp()->getCurrentInlinedSiteIndex());

   if (_resultTempSymRef)
      {
      _resultNode = TR::Node::createLoad(penultimateNode, _resultTempSymRef);
      }

   // if the callee is declared to have a return type, but no return was found while
   // walking the trees [this is possible, for example if the callee ends with a 'throw'
   // instead of returning a value]; then create a zero const node (of the return type of the
   // callee) and make this the result
   TR::DataType returnType = _calleeSymbol->getMethod()->returnType();
   if (!_resultNode && returnType != TR::NoType && !_simpleCallReferenceTreeTop &&
         _callNode->getReferenceCount() > 1)
      {
      _resultNode = TR::Node::create(penultimateNode, comp()->il.opCodeForConst(returnType), 0);
      _resultNode->setLongInt(0);
      }

   // Append the goto target's BBStart and BBEnd to the callee
   //
   // This is delayed until we've seen the last return because the
   // algorithm for determining if we're at the last return is to see
   // if there's only one treetop after the return.
   //
   if (_generatedLastBlock)
      {
      _calleeSymbol->getFlowGraph()->addNode(_generatedLastBlock);
      if (!_firstBBEnd)
         _firstBBEnd = _lastMainLineTreeTop;
      _lastMainLineTreeTop->join(_generatedLastBlock->getEntry());
      _lastMainLineTreeTop = _generatedLastBlock->getExit();
      if (_firstCatchBlock)
         _lastMainLineTreeTop->join(_firstCatchBlock->getEntry());

      // Do not profile the generated last block, since it shares the same
      // bytecode info (and hence the same profiling slot) with other blocks.
      //
      _generatedLastBlock->setFrequency(firstBlock->getFrequency());
      _generatedLastBlock->setDoNotProfile();
      }
   }

void
TR_TransformInlinedFunction::transformNode(TR::Node * node, TR::Node * parent, uint32_t childIndex)
   {
   //     printf("transformNode on node with symbol ID %d, for childIndex %d\n", node->getSymbol()->getWCodeId(), childIndex);
   vcount_t i, visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      return;
   node->setVisitCount(visitCount);

   for (i = 0; i < node->getNumChildren(); ++i)
      transformNode(node->getChild(i), node, i);

   TR::ILOpCode opcode = node->getOpCode();
   if (opcode.isReturn())
      {
      transformReturn(node, parent);
      }
   else if (node->getOpCodeValue() == TR::BBStart)
      {
      _crossedBasicBlock = true;
      if (node->getBlock()->hasExceptionPredecessors())
         _processingExceptionHandlers = true;
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      if (!_firstBBEnd && _currentTreeTop != _lastMainLineTreeTop && !_processingExceptionHandlers)
         _firstBBEnd = _currentTreeTop;
      }
   else if (opcode.isCallIndirect() && node->getFirstArgumentIndex() < node->getNumChildren()
      )
      {
      TR::Node * callThisPointer = node->getChild(node->getFirstArgumentIndex());
      if (callThisPointer->getOpCode().hasSymbolReference() &&
            callThisPointer->getSymbolReference()->isThisPointer())
         {
         _favourVftCompare = true;
         }
      }
   else if (opcode.isStore() &&
            node->getFirstChild()->getOpCode().hasSymbolReference() &&
            node->getFirstChild()->getSymbolReference()->isThisPointer())
      {
      _favourVftCompare = true;
      }

   if (node->getOpCodeValue() == TR::athrow)
      _crossedBasicBlock = true;

   opcode = node->getOpCode();

   if (opcode.hasSymbolReference())
      {
      TR::Symbol * symbol = node->getSymbol();
      if (symbol->isParm())
         {
         TR::Node * newNode = _parameterMapper.map(node, symbol->getParmSymbol(), _crossedBasicBlock);
         if (newNode && newNode != node)
            {
            if (newNode->getOpCode().isLoadConst() && newNode->getType().isInt32() && node->getType().isInt8())
               {
               // do not create bstore/iconst -- work-around for minor type incorrectness in spec2006 hmmer (type mismatch but iconst actually fits in a byte)
               TR_ASSERT(newNode->getUnsignedInt() <= TR::getMaxUnsigned<TR::Int8>(), "intConst %d out of range for byte conversion on node %p\n",newNode->getUnsignedInt(),node);
               newNode = TR::Node::create(TR::i2b, 1, newNode);
               newNode->getFirstChild()->decReferenceCount();
               newNode->setReferenceCount(1);
               dumpOptDetails(comp(),"%screate %s (0x%p) to resolve type mismatch between %s (%p) and %s (%p)\n",OPT_DETAILS,
                  newNode->getOpCode().getName(),newNode,
                  newNode->getFirstChild()->getOpCode().getName(),newNode->getFirstChild(),
                  node->getOpCode().getName(),node);
               }

            bool newIsIntegral = newNode->getType().isIntegral();
            bool oldIsIntegral = node->getType().isIntegral();
            if (newIsIntegral && oldIsIntegral &&
                newNode->getDataType() != node->getDataType())
               {
               // ug.  we'll do some integral coersion here.
               // this really should be ABI-specific.
               TR::Node *oldNewNode = newNode;

               newNode = TR::Node::create(TR::ILOpCode::getProperConversion(newNode->getDataType(), node->getDataType(), false),
                                         1, oldNewNode);

               oldNewNode->decReferenceCount();
               newNode->setReferenceCount(1);
               }

            parent->setChild(childIndex, newNode);
            node->setVisitCount(visitCount - 1);
            }
         }
      }

   }

void
TR_TransformInlinedFunction::transformReturn(TR::Node * returnNode, TR::Node * parent)
   {
   TR_ASSERT(!parent, "Inlining, a return has a parent node?");
   bool isAtEOF = (_currentTreeTop == _penultimateTreeTop && !_firstCatchBlock);

   //traceMsg(TR::comp(), "Transform Return: returnNode = %p, parent = %p\n",returnNode,parent);

   if (returnNode->getNumChildren() && _callNode->getReferenceCount() > 1)
      {
      TR_ASSERT(returnNode->getNumChildren() == 1,  "Inlining, a return has more than one child?");

      // If the call node's only other reference is in the next tree top
      // and the tree top is a return or a store then replace the return in
      // the callee with a clone of the tree top in the caller that's
      // referencing the call node.  This is done to avoid the store and load
      // of a temp.
      //
      if (_determineIfReturnCanBeReplacedWithCallNodeReference)
         {
         _determineIfReturnCanBeReplacedWithCallNodeReference = false;
         _simpleCallReferenceTreeTop = findSimpleCallReference(_callNodeTreeTop, _callNode);
         }

      TR::Node * returnValue = returnNode->getFirstChild();
      if (_callNode->isNonNegative())
         returnValue->setIsNonNegative(true);

      if (_simpleCallReferenceTreeTop)
         {
         cloneAndReplaceCallNodeReference(_simpleCallReferenceTreeTop, _callNode, returnValue, _currentTreeTop->getPrevTreeTop(), comp());
         if (_simpleCallReferenceTreeTop->getNode()->getOpCode().isReturn())
            {
            _treeTopsToRemove.add(_currentTreeTop); // remove the return
            if (isAtEOF && !_generatedLastBlock)
               _generatedLastBlock = TR::Block::createEmptyBlock(returnNode, comp());
            return;
            }
         }
      else if (!isAtEOF || _resultTempSymRef)
         OMR_InlinerUtil::storeValueInATemp(comp(), returnValue, _resultTempSymRef, _currentTreeTop->getPrevTreeTop(), _callerSymbol, _tempList, _availableTemps, &_availableTemps2);
      else
         _resultNode = returnValue;
      }
   bool wasReturnTo = false;
   TR::Block * currentBlock = _currentTreeTop->getEnclosingBlock();
   TR::Block * firstBlock = _calleeSymbol->getFirstTreeTop()->getNode()->getBlock();

   if (!isAtEOF && !wasReturnTo)
      {
      if (!_generatedLastBlock)
         _generatedLastBlock = TR::Block::createEmptyBlock(returnNode, comp(), -1, firstBlock);

      TR::Node * gotoNode = TR::Node::create(returnNode, TR::Goto, 0, _generatedLastBlock->getEntry());
      TR::TreeTop::create(comp(), _currentTreeTop->getPrevTreeTop(), gotoNode);
      }

   if (_generatedLastBlock)
      {
      _calleeSymbol->getFlowGraph()->addEdge(currentBlock, _generatedLastBlock);
      }

   // remove the edge from the block with the return to the end block
   //
   for (auto e = currentBlock->getSuccessors().begin(); e != currentBlock->getSuccessors().end(); ++e)
      if ((*e)->getTo() == _calleeSymbol->getFlowGraph()->getEnd())
         {
         _calleeSymbol->getFlowGraph()->removeEdge(*e);
         break;
         }


   // The return value tree may contain references to arguments.  The
   // argument reference counts aren't updated until after we've finished
   // inlining, and we can't remove the return tree until the arguments
   // reference counts have been updated.
   //
   _treeTopsToRemove.add(_currentTreeTop);
   }

///////////////////////////////////////////////////////////////
// TR_HandleInjectedBasicBlock
///////////////////////////////////////////////////////////////

TR_HandleInjectedBasicBlock::TR_HandleInjectedBasicBlock(
   TR::Compilation *comp,
   TR_InlinerTracer *tracer,
   TR::ResolvedMethodSymbol * methodSymbol, List<TR::SymbolReference> & temps,
   List<TR::SymbolReference> & injectedBasicBlockTemps,
   List<TR::SymbolReference> & availableTemps,
   TR_ParameterMapping * mappings)
   : _comp(comp),
     _tracer(tracer),
     _tempList(temps),
     _injectedBasicBlockTemps(injectedBasicBlockTemps),
     _availableTemps(availableTemps),
     _methodSymbol(methodSymbol),
     _mappings(mappings)
   {

   if(!_tracer)
      _tracer = new (comp->trHeapMemory()) TR_InlinerTracer(comp,comp->fe(),NULL);  // if I wasn't provided a tracer, just use a dummy one.  No tracing will occur.


   }

void
TR_HandleInjectedBasicBlock::printNodesWithMultipleReferences()
   {
   if(!tracer()->debugLevel())
      return;
   for(MultiplyReferencedNode *mn = _multiplyReferencedNodes.getFirst() ; mn ; mn = mn->getNext())
      {
      debugTrace(tracer(),"MultiplyReferencedNode = %p\ttreetop = %p\n\treplacementSymRef =%d\treplacementSymRef2 = %d\treplacementSymRef3 = %d\t_referencesToBeFound = %d"
                           "\tisConst = %d\tsymbolCanBeReloaded = %d",mn->_node,mn->_treeTop,mn->_replacementSymRef ? mn->_replacementSymRef->getReferenceNumber() : -1,
                                 mn->_replacementSymRef2 ? mn->_replacementSymRef2->getReferenceNumber() : -1, mn->_replacementSymRef3 ? mn->_replacementSymRef3->getReferenceNumber() : -1,
                                 mn->_referencesToBeFound, mn->_isConst, mn->_symbolCanBeReloaded);
      }
   }

void
TR_HandleInjectedBasicBlock::findAndReplaceReferences(
   TR::TreeTop * callBBStart, TR::Block * replaceBlock1, TR::Block * replaceBlock2)
   {
   TR_InlinerDelimiter delimiter(tracer(),"hibb.findAndReplaceReferences");
   debugTrace(tracer(),"replaceBlock1 = %d replaceBlock2 = %d callBBStart->getNode = %p",replaceBlock1 ? replaceBlock1->getNumber():-1, replaceBlock2 ? replaceBlock2->getNumber() : -1 , callBBStart->getNode());
   comp()->incVisitCount();

   TR::Block * lastBlock = callBBStart->getNode()->getBlock();
   TR::Block * startBlock = lastBlock->startOfExtendedBlock();

   TR::TreeTop * tt = startBlock->getEntry();
   for (; tt != lastBlock->getExit(); tt = tt->getNextTreeTop())
      collectNodesWithMultipleReferences(tt, 0, tt->getNode());

   printNodesWithMultipleReferences();

   if (_multiplyReferencedNodes.getFirst())
      {
      createTemps(false);

      vcount_t visitCount = comp()->incVisitCount();
      replaceNodesReferencedFromAbove(replaceBlock1, visitCount);

      if (replaceBlock2)
         {
         replaceNodesReferencedFromAbove(replaceBlock2, visitCount);
         }
      }
   if (replaceBlock2)
      {
      TR::TreeTop * tt = replaceBlock2->getEntry(), * locationForTemp = lastBlock->getLastRealTreeTop();
      for (; tt != replaceBlock2->getExit(); tt = tt->getNextTreeTop())
         collectNodesWithMultipleReferences(locationForTemp, 0, tt->getNode());

      if (_multiplyReferencedNodes.getFirst())
         {
         createTemps(true);
         vcount_t visitCount = comp()->incVisitCount();
         replaceNodesReferencedFromAbove(replaceBlock1, visitCount);
         if (replaceBlock2)
            replaceNodesReferencedFromAbove(replaceBlock2, visitCount);
         }
      }
   }

void
TR_HandleInjectedBasicBlock::collectNodesWithMultipleReferences(TR::TreeTop * tt, TR::Node * parent, TR::Node * node)
   {
   MultiplyReferencedNode * found = 0;
   if (node->getReferenceCount() > 1)
      {
      found = find(node);
      if (!found)
         add(tt, node);
      else if (--found->_referencesToBeFound == 0)
         {
         _multiplyReferencedNodes.remove(found);
         }
      }

   if (!found)
      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         collectNodesWithMultipleReferences(tt, node, node->getChild(i));
   }

void
TR_HandleInjectedBasicBlock::replaceNodesReferencedFromAbove(TR::Block * block, vcount_t visitCount)
   {
   TR::Block * lastBlock = block;
   while (lastBlock->getNextBlock() && lastBlock->getNextBlock()->isExtensionOfPreviousBlock())
      lastBlock = lastBlock->getNextBlock();
   TR::TreeTop * tt = block->getEntry();
   for (; _multiplyReferencedNodes.getFirst() && tt != lastBlock->getExit(); tt = tt->getNextTreeTop())
      replaceNodesReferencedFromAbove(tt, tt->getNode(), 0, 0, visitCount);
   }

void
TR_HandleInjectedBasicBlock::replaceNodesReferencedFromAbove(
   TR::TreeTop * tt, TR::Node * node, TR::Node * parent, uint32_t childIndex, vcount_t visitCount)
   {
   MultiplyReferencedNode * found;
   if (node->getReferenceCount() > 1 && (found = find(node)))
      {
      replace(found, tt, parent, childIndex);
      if (--found->_referencesToBeFound == 0)
         {
         _multiplyReferencedNodes.remove(found);
         _fixedNodes.add(found);
         }
      }
   else
      {
      if (visitCount == node->getVisitCount())
         return;
      node->setVisitCount(visitCount);

      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         replaceNodesReferencedFromAbove(tt, node->getChild(i), node, i, visitCount);
      }
   }

void
TR_HandleInjectedBasicBlock::createTemps(bool replaceAllReferences)
   {
   TR_InlinerDelimiter delimiter(tracer(),"hibb.createTemps");
   if (_tracer && _tracer->debugLevel())
      debugTrace(_tracer,"\ncalling createTemps with %d", replaceAllReferences);

   for (MultiplyReferencedNode * ref = _multiplyReferencedNodes.getFirst(); ref; ref = ref->getNext())
      {
      TR::ILOpCode opcode = ref->_node->getOpCode();
      TR::DataType nodeDataType = ref->_node->getDataType();

      ref->_replacementSymRef = 0;
      ref->_isConst = false;
      if (replaceAllReferences)
         ref->_referencesToBeFound = ref->_node->getReferenceCount();

      // We can't reuse a symbol reference unless we check that its
      // value isn't modified between the two references to the node
      //
      bool rematConstant = rematerializeConstant(ref->_node, comp());
      if (rematConstant || opcode.getOpCodeValue() == TR::loadaddr)
         {
         ref->_isConst = true;
         }
      else
         {
         TR::SymbolReference * symRef = 0;

         static const char *enabletempCreationOpt = feGetEnv("TR_EnableTempCreationOpt");
         // This opt seems risky, and quite bad.
         // If this is enabled and TR_DisableParmTempOpt is not set, there will for sure be problems.
         // There possibly could be problems even when TR_EnableParmTempOpt is set.
         // The real underlying issue is that just because a symref is on a parmMap, it doesn't mean you can automatically add it to a list of injected basic block temps
         // When I enabled the ParmTempOpt, this is what happened.  An auto symbol got added to the injectedBasicBlockTemps list, and then was reused in a very bad manner.
         if(enabletempCreationOpt)
            {
            for (TR_ParameterMapping * parmMap = _mappings; parmMap; parmMap = parmMap->getNext())
               {
               if (parmMap->_parameterNode == ref->_node)
                  {
                  symRef = parmMap->_replacementSymRef;

                  debugTrace(_tracer,"\nadding %d to injected basic block temps for node %p is in temp list %d", symRef->getReferenceNumber(), ref->_node, _tempList.find(symRef));
                  _injectedBasicBlockTemps.add(symRef);
                  _tempList.remove(symRef);
                  break;
                  }
               }
            }
         if (!symRef)
            {
            TR::TreeTop * tt = ref->_treeTop;
            if (tt->getNode()->getOpCode().isBranch() || tt->getNode()->getOpCode().isSwitch())
               tt = tt->getPrevTreeTop();

            // If this treetop is an OSR point, a store cannot be placed between it and the
            // transition treetop in postExecutionOSR
            if (comp()->isPotentialOSRPoint(tt->getNode()))
               tt = comp()->getMethodSymbol()->getOSRTransitionTreeTop(tt);

            TR::Node *value = ref->_node;
            // Convert the node being stored to the type required by the FE
            if (comp()->fe()->dataTypeForLoadOrStore(nodeDataType) != nodeDataType)
               {
               TR::ILOpCodes  convOpCode = TR::ILOpCode::getProperConversion(nodeDataType, comp()->fe()->dataTypeForLoadOrStore(nodeDataType), false);
               TR::Node      *valueToTempConv = TR::Node::create(convOpCode, 1, value);
               value = valueToTempConv;
               }

            OMR_InlinerUtil::storeValueInATemp(comp(), value, symRef, tt, _methodSymbol, _injectedBasicBlockTemps, _availableTemps, 0);
            }

         ref->_replacementSymRef = symRef;
         }
      }
   }

TR::Node *
TR_HandleInjectedBasicBlock::findNullCheckReferenceSymbolReference(TR::TreeTop * nullCheckTreeTop)
   {
   TR::Node * nullCheckReference = nullCheckTreeTop->getNode()->getNullCheckReference();
   MultiplyReferencedNode * ref;
   for (ref = _fixedNodes.getFirst(); ref; ref = ref->getNext())
      if (ref->_node == nullCheckReference)
         break;

   //TR_ASSERT(ref && ref->_replacementSymRef, "couldn't find temp for null check reference");
   if (ref == NULL)
      return NULL;

   nullCheckReference = TR::Node::createLoad(ref->_node, ref->_replacementSymRef);
   return nullCheckReference;
   }

TR_HandleInjectedBasicBlock::MultiplyReferencedNode *
TR_HandleInjectedBasicBlock::find(TR::Node * node)
   {
   for (MultiplyReferencedNode * ref = _multiplyReferencedNodes.getFirst(); ref; ref = ref->getNext())
      if (ref->_node == node)
         return ref;
   return 0;
   }

TR_HandleInjectedBasicBlock::MultiplyReferencedNode::MultiplyReferencedNode(TR::Node *node, TR::TreeTop *tt, uint32_t refsToBeFound,bool symCanBeReloaded) :
      _node(node),_treeTop(tt),_referencesToBeFound(refsToBeFound),_symbolCanBeReloaded(symCanBeReloaded),
      _replacementSymRef(0), _replacementSymRef2(0), _replacementSymRef3(0), _isConst(false)
   {

   }

void
TR_HandleInjectedBasicBlock::add(TR::TreeTop * tt, TR::Node * node)
   {
   MultiplyReferencedNode * ref = new (comp()->trStackMemory()) MultiplyReferencedNode (node,tt,node->getReferenceCount()-1,node->getOpCode().isLoadVarDirect() && node->getSymbol()->isAutoOrParm());
//   ref->_node = node;
//   ref->_treeTop = tt;
//   ref->_referencesToBeFound = node->getReferenceCount() - 1;
//   ref->_symbolCanBeReloaded = node->getOpCode().isLoadVarDirect() && node->getSymbol()->isAutoOrParm();

   _multiplyReferencedNodes.add(ref);
   }

void
TR_HandleInjectedBasicBlock::replace(MultiplyReferencedNode * ref, TR::TreeTop *tt, TR::Node * parent, uint32_t childIndex)
   {
   ref->_node->decReferenceCount();
   TR::Node * replacementNode;
   if (ref->_isConst)
      {
      replacementNode = TR::Node::copy(ref->_node);
      if (ref->_node->getOpCodeValue() == TR::loadaddr)
         {
         TR::Node *treetopNode = TR::Node::create(TR::treetop, 1, replacementNode);
         TR::TreeTop *newTree = TR::TreeTop::create(comp(), treetopNode, NULL, NULL);
         tt->getPrevTreeTop()->join(newTree);
         newTree->join(tt);
         replacementNode->setReferenceCount(2);
         }
      else
         {
         replacementNode->setReferenceCount(1);
         }
      }
   else
      {
         {
         replacementNode = TR::Node::createLoad(ref->_node, ref->_replacementSymRef);
         }
      // Convert the node just loaded back to the original type in case the FE required us to store it as something else
      if (replacementNode->getDataType() != ref->_node->getDataType())
         {
         TR::ILOpCodes  convOpCode = TR::ILOpCode::getProperConversion(replacementNode->getDataType(), ref->_node->getDataType(), false);
         TR::Node      *replacementToRefConv = TR::Node::create(convOpCode, 1, replacementNode);
         replacementNode = replacementToRefConv;
         }
#ifdef J9_PROJECT_SPECIFIC
      if (ref->_node->getType().isBCD())
         replacementNode->setDecimalPrecision(ref->_node->getDecimalPrecision());
#endif
      replacementNode->setReferenceCount(1);
      }

   if (!parent->getChild(childIndex)->getByteCodeInfo().doNotProfile())
      replacementNode->getByteCodeInfo().setDoNotProfile(0); // allow profiling of replacement node if the original could be profiled
   parent->setChild(childIndex, replacementNode);
   }

/////////////////////////////////////////////////////////////////
// static functions
/////////////////////////////////////////////////////////////////

static TR::TreeTop *findPinningArrayStore(TR::Compilation *comp, TR::TreeTop *prevTT, TR::Node *value)
   {
   TR::TreeTop *tt = NULL;
   for (tt = prevTT; tt->getNode()->getOpCodeValue() != TR::BBStart; tt = tt->getPrevTreeTop())
      ;
   TR::TreeTop *lastTT = tt->getExtendedBlockExitTreeTop();
   bool found = false;
   for (tt = prevTT; tt != lastTT; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();
      if (node->getOpCode().isStore() &&
            node->getSymbolReference()->getSymbol()->isAuto() &&
            node->getSymbolReference()->getSymbol()->castToAutoSymbol()->isPinningArrayPointer())
         {
         if (node->getSymbolReference()->getSymbol() == value->getPinningArrayPointer())
            {
            found = true;
            break;
            }
         }
      }
   if (!found)
      {
      tt = lastTT->getPrevRealTreeTop();
      if (tt->getNode()->getOpCode().isBranch() ||
            tt->getNode()->getOpCode().isSwitch() ||
            tt->getNode()->getOpCode().isReturn() ||
            tt->getNode()->getOpCodeValue() == TR::athrow)
         {
         tt = tt->getPrevTreeTop();
         }
      }
   return tt;
   }

/**
 * handles the temp sharing between caller and inlined callee by storing a
 * value from the inlined method to a temp of the caller
 */
TR::TreeTop * OMR_InlinerUtil::storeValueInATemp(
   TR::Compilation *comp,
   TR::Node * value, TR::SymbolReference * & tempSymRef, TR::TreeTop * prevTreeTop, TR::ResolvedMethodSymbol * methodSymbol,
   List<TR::SymbolReference> & tempList, List<TR::SymbolReference> & availableTemps,
   List<TR::SymbolReference> * availableTemps2, bool behavesLikeTemp, TR::TreeTop ** newStoreValueTreeTop,
   bool isIndirect, int32_t offset)
   {
   TR::DataType dataType = value->getDataType();

   bool internalPtrHasPinningArrayPtr = false;

   if (value->isInternalPointer() &&
       value->getPinningArrayPointer())
      {
      TR_ASSERT(!tempSymRef, "storing an internal pointer into a possibly non-internal-ptr temp");

      tempSymRef = comp->getSymRefTab()->createTemporary(methodSymbol, TR::Address, true, 0);
      TR::Symbol *symbol = tempSymRef->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer
                          (value->getPinningArrayPointer());
      internalPtrHasPinningArrayPtr = true;
      }
   else
      {
      bool isInternalPointer = false;

      // disable internal pointer symbols for C/C++
      if ((value->hasPinningArrayPointer() &&
           value->computeIsInternalPointer()) ||
            (value->getOpCode().isLoadVarDirect() &&
             value->getSymbolReference()->getSymbol()->isAuto() &&
             value->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer()))
         isInternalPointer = true;

      if ((value->isNotCollected() && dataType != TR::Aggregate) || isIndirect)
         {
         TR::SymbolReference *valueRef;
         if (tempSymRef!=NULL)
            {
            valueRef = tempSymRef;
            }
         else
            {
            valueRef = comp->getSymRefTab()->createTemporary(methodSymbol, dataType, false,
#ifdef J9_PROJECT_SPECIFIC
                                                             value->getType().isBCD() ? value->getSize() :
#endif
                                                             0);
            valueRef->getSymbol()->setNotCollected();
            tempSymRef = valueRef;
            }
         isInternalPointer = false;
         }


      TR::TreeTop *newStoreValueTree = NULL;
      if (isInternalPointer)
         {
         TR::SymbolReference *valueRef = comp->getSymRefTab()->
                                        createTemporary(methodSymbol, TR::Address, true, 0);

         if (value->getOpCode().hasSymbolReference() && value->getSymbolReference()->getSymbol()->isNotCollected())
            valueRef->getSymbol()->setNotCollected();

         else if (value->getOpCode().isArrayRef())
            value->setIsInternalPointer(true);

         TR::AutomaticSymbol *pinningArray = NULL;
         if (value->getOpCode().isArrayRef())
            {
            TR::Node *valueChild = value->getFirstChild();
            if (valueChild->isInternalPointer() &&
                  valueChild->getPinningArrayPointer())
               {
               pinningArray = valueChild->getPinningArrayPointer();
               }
            else
               {
               while (valueChild->getOpCode().isArrayRef())
                  valueChild = valueChild->getFirstChild();

               if (valueChild->getOpCode().isLoadVarDirect() &&
                     valueChild->getSymbolReference()->getSymbol()->isAuto())
                  {
                  if (valueChild->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer())
                     {
                     pinningArray = valueChild->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();
                     }
                  else
                     {
                     pinningArray = valueChild->getSymbolReference()->getSymbol()->castToAutoSymbol();
                     pinningArray->setPinningArrayPointer();
                     }
                  }
               else
                  {
                  TR::SymbolReference *newValueArrayRef = comp->getSymRefTab()->
                                                         createTemporary(methodSymbol, TR::Address, false, 0);

                  TR::Node*newStoreValue = TR::Node::createStore(newValueArrayRef, valueChild);
                  newStoreValueTree = TR::TreeTop::create(comp, newStoreValue);
                  if (!newValueArrayRef->getSymbol()->isParm()) // newValueArrayRef could contain a parm symbol
                     {
                     pinningArray = newValueArrayRef->getSymbol()->castToAutoSymbol();
                     pinningArray->setPinningArrayPointer();
                     }

                  if (!prevTreeTop)
                     {
                     if (!newStoreValueTreeTop)
                        TR_ASSERT(0, "no previous treetop and no treetop pointer passed in");
                     *newStoreValueTreeTop = newStoreValueTree;
                     }
                  }
               }
            }
         else
            pinningArray = value->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer();

         valueRef->getSymbol()->castToInternalPointerAutoSymbol()->setPinningArrayPointer(pinningArray);
         if (value->isInternalPointer() && pinningArray)
            {
            value->setPinningArrayPointer(pinningArray);
            }
         tempSymRef = valueRef;
         }

      if (newStoreValueTree)
         {
         if (prevTreeTop)
            {
            TR::TreeTop *nextTreeTop = prevTreeTop->getNextTreeTop();
            prevTreeTop->join(newStoreValueTree);
            newStoreValueTree->join(nextTreeTop);
            }
         //else
         //  TR_ASSERT(0, "Must specify a place to insert the pinning array store\n");
         }
      }

   if (dataType == TR::Aggregate)
      {
      TR_ASSERT(tempSymRef == NULL, "Aggregate should not be mapped to symbol yet");
      uint32_t size = value->getSize();
      tempSymRef = new (comp->trHeapMemory())
      TR::SymbolReference(comp->getSymRefTab(),
                         TR::AutomaticSymbol::create(comp->trHeapMemory(),dataType,size),
                         methodSymbol->getResolvedMethodIndex(),
                         methodSymbol->incTempIndex(comp->fe()));


      if (value->getOpCode().hasSymbolReference() && value->getSymbolReference()->getSymbol()->isNotCollected())
         tempSymRef->getSymbol()->setNotCollected();
      tempList.add(tempSymRef);
      }

   if (!tempSymRef)
      {
      if (!value->getOpCode().hasSymbolReference() || !value->getSymbolReference()->getSymbol()->isNotCollected())
         {
         tempSymRef = comp->getSymRefTab()->findAvailableAuto(availableTemps, dataType, behavesLikeTemp);
         if (tempSymRef && tempSymRef->getSymbol()->isNotCollected())
            tempSymRef = NULL;
         if (!tempSymRef && availableTemps2)
            tempSymRef = comp->getSymRefTab()->findAvailableAuto(*availableTemps2, dataType, behavesLikeTemp);
         if (tempSymRef && tempSymRef->getSymbol()->isNotCollected())
            tempSymRef = NULL;
         }

      if (!tempSymRef)
         {
         tempSymRef = new (comp->trHeapMemory())
         TR::SymbolReference(comp->getSymRefTab(),
#ifdef J9_PROJECT_SPECIFIC
                            value->getType().isBCD() ?
                            TR::AutomaticSymbol::create(comp->trHeapMemory(),dataType,value->getSize()) :
#endif
                            TR::AutomaticSymbol::create(comp->trHeapMemory(),dataType),
                            methodSymbol->getResolvedMethodIndex(),
                            methodSymbol->incTempIndex(comp->fe()));

         if (value->getOpCode().hasSymbolReference() && value->getSymbolReference()->getSymbol()->isNotCollected())
            tempSymRef->getSymbol()->setNotCollected();
         }

      tempList.add(tempSymRef);
      }

   TR_ASSERT(comp->il.opCodeForDirectStore(dataType) != TR::BadILOp, "Inlining, unexpected data type for temporary");

   TR::Node * storeNode = NULL;
   if (isIndirect)
      {
      TR::Symbol *symShadow = TR::Symbol::createShadow(comp->trHeapMemory(), dataType, value->getSize());
      TR::SymbolReference *symRefShadow  = comp->getSymRefTab()->createSymbolReference(symShadow);
      TR::Node *storeAddress = TR::Node::createWithSymRef(value, TR::loadaddr, 0, tempSymRef);
      storeAddress = TR::Node::createAddConstantToAddress(storeAddress, offset);

      storeNode = TR::Node::createWithSymRef(comp->il.opCodeForIndirectStore(dataType), 2, 2, storeAddress, value, symRefShadow);
      }
   else
      {
      storeNode = TR::Node::createStore(tempSymRef, value);
      }
   if (comp->cg()->traceBCDCodeGen())
      traceMsg(comp,"\tcreate storeNode %p of tempSymRef #%d (possibly for node uncommoning during opcodeExpansion)\n",storeNode,tempSymRef->getReferenceNumber());

#ifdef J9_PROJECT_SPECIFIC
   if (value->getType().isBCD())
      {
      storeNode->setDecimalPrecision(value->getDecimalPrecision());
      tempSymRef->getSymbol()->setSize(value->getSize());
      }
#endif

   if (prevTreeTop)
      {
      TR::TreeTop *tt = NULL;
      // if the internal pointer already has a pinning
      // array pointer, then its better to place the
      // temp created for this internal pointer in the
      // same block as the pinning array store. (if the
      // pinning array store is not found in the block,
      // the temp is placed at the end of the block)
      // this is necessary since there could be a scenario
      // where the block containing the pinning array pointer (b1)
      // is different from the temp created for the internal pointer (b2).
      // if b1 is removed from the cfg and the internal pointer
      // temp remains (b2), then the codegen estimates the size of
      // the stack incorrectly.
      TR::TreeTop *pTTop = prevTreeTop;
      if (internalPtrHasPinningArrayPtr)
         {
         tt = findPinningArrayStore(comp, prevTreeTop, value);
         }
      if (tt)
         {
         pTTop = tt;
         }
      return TR::TreeTop::create(comp, pTTop, storeNode);
      }

   return TR::TreeTop::create(comp, storeNode);
   }

static TR::Node * cloneAndReplaceCallNodeReference(
   TR::Node * originalNode, TR::Node * callNode, TR::Node * replacementNode, TR::Compilation *comp)
   {
   TR::Node * newNode;
   if (originalNode == callNode)
      newNode = replacementNode;
   else if (originalNode->getReferenceCount() > 1)
      newNode = originalNode;
   else
      {
      newNode = TR::Node::copy(originalNode);
      newNode->setReferenceCount(0);
      for (uint32_t i = 0; i < originalNode->getNumChildren(); ++i)
         {
         TR::Node * newChild = cloneAndReplaceCallNodeReference(originalNode->getChild(i), callNode, replacementNode, comp);
         newNode->setAndIncChild(i, newChild);
         }
      }
   return newNode;
   }

static TR::TreeTop * cloneAndReplaceCallNodeReference(
   TR::TreeTop * callNodeReference, TR::Node * callNode, TR::Node * replacementNode, TR::TreeTop * insertAfterTreeTop, TR::Compilation *comp)
   {
   return TR::TreeTop::create(comp, insertAfterTreeTop, cloneAndReplaceCallNodeReference(callNodeReference->getNode(), callNode, replacementNode, comp));
   }



static void cloneAndReplaceReturnValueReference(TR::TreeTop *ifCmpTreeTop, TR::TreeTop * rvStoreTreeTop, TR::Block *curBlock, TR::CFG *cfg, TR::Compilation *comp)
   {
   //Split block for inserting new ifcmp
   curBlock->split(rvStoreTreeTop->getNextTreeTop(), cfg);

   //Duplicated ifcmp branch will be inserted before goto
   TR::Node * newNode = ifCmpTreeTop->getNode()->duplicateTree();
   TR::TreeTop * newTreeTop = TR::TreeTop::create(comp, newNode, ifCmpTreeTop->getPrevTreeTop(), ifCmpTreeTop->getNextTreeTop());
   newTreeTop->join(rvStoreTreeTop->getNextTreeTop());
   rvStoreTreeTop->join(newTreeTop);
   }

bool TR_TransformInlinedFunction::findCallNodeInTree(TR::Node * callNode, TR::Node * node)
   {
   if (node == callNode)
      return true;

   // we cannot use visitcounts here to control the recursion
   // (because the outer transformNode uses them)
   // so abandon the walk if the node was not found even x levels deep in this tree
   //
   if (findCallNodeRecursionDepth == 0)
      return false;

   --findCallNodeRecursionDepth;

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      if (findCallNodeInTree(callNode, node->getChild(i)))
         {
         ++findCallNodeRecursionDepth;
         return true;
         }

   ++findCallNodeRecursionDepth;
   return false;
   }

bool TR_TransformInlinedFunction::findReturnValueInTree(TR::Node * rvStoreNode, TR::Node * node, TR::Compilation *comp)
   {
   TR::SymbolReference *rvSymRef = rvStoreNode->getSymbolReference();
   // ibload
   //    loadaddr rvSymRef
   if (node->getOpCode().isLoadIndirect() &&
       node->getFirstChild()->getOpCodeValue() == TR::loadaddr &&
       node->getSize() == rvStoreNode->getSize() &&
       node->getNumChildren() == 1)
      {
      TR::Node            * child  = node->getFirstChild();
      TR::SymbolReference * symRef = child->getSymbolReference();
      if (symRef == rvSymRef)
         return true;
      }
   else if (node->getOpCode().isLoadDirect() &&
       node->getOpCode().hasSymbolReference() &&
       node->getSize() == rvStoreNode->getSize() &&
       node->getSymbolReference() == rvSymRef)
      {
      return true;
      }

   // we cannot use visitcounts here to control the recursion
   // (because the outer transformNode uses them)
   // so abandon the walk if the node was not found even x levels deep in this tree
   //
   if (findCallNodeRecursionDepth == 0)
      return false;

   --findCallNodeRecursionDepth;

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      if (findReturnValueInTree(rvStoreNode, node->getChild(i), comp))
         {
         ++findCallNodeRecursionDepth;
         return true;
         }
      }

   ++findCallNodeRecursionDepth;
   return false;
   }

bool TR_TransformInlinedFunction::onlyMultiRefNodeIsCallNode(TR::Node * callNode, TR::Node * node)
   {
   if (node != callNode)
      {
      if (node->getReferenceCount() > 1)
         return false;

      // stop the recursion
      //
      if (onlyMultiRefNodeIsCallNodeRecursionDepth == 0)
         return false;

      --onlyMultiRefNodeIsCallNodeRecursionDepth;

      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         if (!onlyMultiRefNodeIsCallNode(callNode, node->getChild(i)))
            {
            ++onlyMultiRefNodeIsCallNodeRecursionDepth;
            return false;
            }

      ++onlyMultiRefNodeIsCallNodeRecursionDepth;
      return true;
      }

   return true;
   }

TR::TreeTop * TR_TransformInlinedFunction::findSimpleCallReference(TR::TreeTop * callNodeTreeTop, TR::Node * callNode)
   {
   // If the call node's only other reference is in the next tree top
   // and the tree top is a return or a store then return the next treetop
   // so that it can be moved into the inlined function to avoid the store
   // and load of a temp.
   //
   if (callNode->getReferenceCount() == 2)
      {
      TR::TreeTop * nextTreeTop = callNodeTreeTop->getNextTreeTop();
      while (nextTreeTop->getNode()->getOpCodeValue() == TR::dbgFence)
         nextTreeTop = nextTreeTop->getNextTreeTop();
      TR::Node * nextTreeTopNode = nextTreeTop->getNode();
      TR::ILOpCode opcode = nextTreeTopNode->getOpCode();
      findCallNodeRecursionDepth = MAX_FIND_SIMPLE_CALL_REFERENCE_DEPTH;
      onlyMultiRefNodeIsCallNodeRecursionDepth = MAX_FIND_SIMPLE_CALL_REFERENCE_DEPTH;
      if ((opcode.isReturn() || opcode.isStore()) &&
            findCallNodeInTree(callNode, nextTreeTopNode) &&
            onlyMultiRefNodeIsCallNode(callNode, nextTreeTopNode))
         return nextTreeTop;
      }
   return 0;
   }
//migrated

static void addSymRefsToList(List<TR::SymbolReference> & calleeSymRefs, List<TR::SymbolReference> & callerSymRefs)
   {
   ListIterator<TR::SymbolReference> i(&calleeSymRefs);
   for (TR::SymbolReference * symRef = i.getFirst(); symRef; symRef = i.getNext())
      callerSymRefs.add(symRef);
   }

//new findInlineTargets API


bool TR_CallSite::findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   TR_ASSERT(0, "TR_CallSite::findCallSiteTarget is not implemented in %s", name());
   return false;
   }


TR_ResolvedMethod* TR_IndirectCallSite::getResolvedMethod (TR_OpaqueClassBlock* klass)
   {
   return _callerResolvedMethod->getResolvedVirtualMethod(comp(), klass, _vftSlot);
   }


TR_ResolvedMethod* TR_IndirectCallSite::findSingleJittedImplementer (TR_InlinerBase *inliner)
   {
   return inliner->getUtil()->findSingleJittedImplementer(this);
   }

TR_ResolvedMethod*
OMR_InlinerUtil::findSingleJittedImplementer(TR_IndirectCallSite *callsite)
   {
   return NULL;
   }

bool TR_IndirectCallSite::hasFixedTypeArgInfo()
   {
   return _ecsPrexArgInfo && _ecsPrexArgInfo->get(0) && _ecsPrexArgInfo->get(0)->classIsFixed();
   }

bool TR_IndirectCallSite::hasResolvedTypeArgInfo()
   {
   return _ecsPrexArgInfo && _ecsPrexArgInfo->get(0) &&
      _ecsPrexArgInfo->get(0)->classIsPreexistent() && _ecsPrexArgInfo->get(0)->getFixedClass();
   }

TR_OpaqueClassBlock* TR_IndirectCallSite::getClassFromArgInfo()
   {
   TR_ASSERT(_ecsPrexArgInfo && _ecsPrexArgInfo->get(0) &&
         _ecsPrexArgInfo->get(0)->getFixedClass(), "getClassFromArgInfo is NOT guarded by hasFixedTypeArgInfo,hasResolvedTypeArgInfo");

   return _ecsPrexArgInfo->get(0)->getFixedClass();
   }

bool TR_IndirectCallSite::tryToRefineReceiverClassBasedOnResolvedTypeArgInfo(TR_InlinerBase* inliner)
   {
   if (hasResolvedTypeArgInfo())
      {
      TR_OpaqueClassBlock* classFromArgInfo = getClassFromArgInfo();
      if (_receiverClass && comp()->fe()->isInstanceOf(classFromArgInfo, _receiverClass, true, true) == TR_yes)
         {
         heuristicTrace(inliner->tracer(), "refining _receiverClass %p to %p", _receiverClass, classFromArgInfo);
         _receiverClass = classFromArgInfo;
         return true;
         }
      else
         {
         //either class are incompatible
         //or _receiverClass is more refined
         //in either case classFromArgInfo is useless
         //no need to propagate it
         _ecsPrexArgInfo->set(0, NULL);
         }
      }
   return false;
   }

bool TR_IndirectCallSite::findCallTargetUsingArgumentPreexistence(TR_InlinerBase* inliner)
   {
   TR_OpaqueClassBlock* klass = extractAndLogClassArgument(inliner);

   //initialClass might be more specific than the initialMethod or interfaceMethod
   TR_OpaqueClassBlock* initialClass = _receiverClass ? _receiverClass : getClassFromMethod();

   //possible in a certain scenario?
   TR_ASSERT(initialClass, "There must be at least some receiver info!");

   //Prod code
   if (!initialClass)
      {
      heuristicTrace(inliner->tracer(), "ARGS PROPAGATION: couldn't get initialClass\n");
      _ecsPrexArgInfo->set(0, NULL);
      return true;
      }

   if (!fe()->isInstanceOf(klass, initialClass, true, true))
      {
      //This might happen if we try to inline dead code i.e.
      // a.foo(), a of type A
      //    process(a as o), o of B
      //       if (o instanceof B) , B and A are NOT related
      //           o.foo(), ARGS PROPAGATION says A, but initialClass is B

      heuristicTrace(inliner->tracer(), "The preexistence class (%p) is not compatible with initial class (%p)\n"
      "Bail out of findCallTargetUsingArgumentPreexistence \n", klass, initialClass);
      return false;
      }

   TR_ResolvedMethod * targetMethod = getResolvedMethod(klass);
   TR_ASSERT(targetMethod, "Couldn't resolve the method for klass %p", klass);

   if (!targetMethod)
      {
      heuristicTrace(inliner->tracer(), "ARGS PROPAGATION: couldn't get targetMethod\n");
      _ecsPrexArgInfo->set(0, NULL);
      return true;
      }
   TR_VirtualGuardSelection *guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_VftTest, klass);
   addTarget(comp()->trMemory(),inliner,guard,targetMethod,klass,heapAlloc);
   return true;
   }


bool TR_IndirectCallSite::addTargetIfMethodIsNotOverriden (TR_InlinerBase* inliner)
   {

   if (_initialCalleeMethod && !_initialCalleeMethod->virtualMethodIsOverridden())
      {
      if (comp()->compileRelocatableCode() && !TR::Options::getCmdLineOptions()->allowRecompilation())
         return false; // CHTable is not used when recompilation is disabled, hence we cannot use assumptions for AOT

      heuristicTrace(inliner->tracer(),"Call is not overridden.");
      TR_VirtualGuardSelection * guard = NULL;
      if (_initialCalleeMethod->isSubjectToPhaseChange(comp()))
         {
         guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_ProfiledGuard, TR_MethodTest);
         }
      else
         {
         guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_NonoverriddenGuard, TR_NonoverriddenTest);
         }

      addTarget(comp()->trMemory(), inliner, guard,_initialCalleeMethod, _receiverClass,heapAlloc);
      return true;
      }

   return false;
   }

bool
TR_IndirectCallSite::addTargetIfMethodIsNotOverridenInReceiversHierarchy (TR_InlinerBase* inliner)
   {
   return inliner->getUtil()->addTargetIfMethodIsNotOverridenInReceiversHierarchy(this);
   }

/**
 * if the initial callee method of this callsite is not overriden add it as the
 * target of the callsite
 */
bool
OMR_InlinerUtil::addTargetIfMethodIsNotOverridenInReceiversHierarchy(TR_IndirectCallSite *callsite)
   {
   return false;
   }

/**
 * find the single implementer and add it as the target of this callsite
 */
bool
OMR_InlinerUtil::addTargetIfThereIsSingleImplementer (TR_IndirectCallSite *callsite)
   {
   return false;
   }

bool
TR_IndirectCallSite::addTargetIfThereIsSingleImplementer (TR_InlinerBase* inliner)
   {
   return inliner->getUtil()->addTargetIfThereIsSingleImplementer(this);
   }

bool TR_IndirectCallSite::findCallSiteTarget (TR_CallStack *callStack, TR_InlinerBase* inliner)
   {
   //inliner->tracer()->dumpPrexArgInfo(_ecsPrexArgInfo);

   if (addTargetIfMethodIsNotOverriden(inliner) ||
       addTargetIfMethodIsNotOverridenInReceiversHierarchy(inliner) ||
       addTargetIfThereIsSingleImplementer(inliner) ||
       findCallTargetUsingArgumentPreexistence(inliner))
      {
      return true;
      }

   return false;
   }

TR_OpaqueClassBlock * TR_IndirectCallSite::extractAndLogClassArgument (TR_InlinerBase* inliner)
   {
   if (inliner->tracer()->heuristicLevel())
      {
      int32_t len;
      const char *fixedReceiverClassName = TR::Compiler->cls.classNameChars(comp(), getClassFromArgInfo(), len);
      heuristicTrace(inliner->tracer(), "Receiver to call is constrained by argument propagation to %s.", fixedReceiverClassName);
      }

   return  getClassFromArgInfo();
   }


bool TR_DirectCallSite::findCallSiteTarget (TR_CallStack* callStack, TR_InlinerBase* inliner)
   {

  if (inliner->getPolicy()->replaceSoftwareCheckWithHardwareCheck(_initialCalleeMethod))
      return false;


   TR_OpaqueClassBlock *tempreceiverClass;
   TR_VirtualGuardSelection *guard;
   static const char *disableHCRGuards2 = feGetEnv("TR_DisableHCRGuards");

   const bool skipHCRGuardForCallee = inliner->getPolicy()->skipHCRGuardForCallee(_initialCalleeMethod);

   static const char *disableFSDGuard = feGetEnv("TR_DisableFSDGuard");
   if (!disableHCRGuards2 && comp()->getHCRMode() != TR::none && !comp()->compileRelocatableCode() && !skipHCRGuardForCallee)
      {
      tempreceiverClass = _initialCalleeMethod->classOfMethod();
      guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_HCRGuard, TR_NonoverriddenTest);
      }
   else if (!disableFSDGuard && comp()->getOption(TR_FullSpeedDebug))
      {
      tempreceiverClass = _receiverClass;
      guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(TR_BreakpointGuard, TR_FSDTest);
      }
   else
      {
      TR_VirtualGuardKind kind = TR_NoGuard;
      if (comp()->compileRelocatableCode())
         kind = TR_DirectMethodGuard;
      guard = new (comp()->trHeapMemory()) TR_VirtualGuardSelection(kind);
      tempreceiverClass = _receiverClass;
      }

   debugTrace(inliner->tracer(),"Found a Direct Call.");
   addTarget(comp()->trMemory(), inliner, guard, _initialCalleeMethod,tempreceiverClass,heapAlloc);
   return true;
   }

void OMR_InlinerUtil::collectCalleeMethodClassInfo(TR_ResolvedMethod *calleeMethod)
   {
   return;
   }

void TR_InlinerBase::getSymbolAndFindInlineTargets(TR_CallStack *callStack, TR_CallSite *callsite, bool findNewTargets)
   {
   TR_InlinerDelimiter delimiter(tracer(),"getSymbolAndFindInlineTargets");

   TR::Node *callNode = callsite->_callNode;
   TR::TreeTop *callNodeTreeTop  = callsite->_callNodeTreeTop;

   TR::SymbolReference * symRef = callNode->getSymbolReference();
   TR_InlinerFailureReason isInlineable = InlineableTarget;


   if (callsite->_initialCalleeSymbol)
      {
      if (getPolicy()->supressInliningRecognizedInitialCallee(callsite, comp()))
         isInlineable = Recognized_Callee;

      if (isInlineable != InlineableTarget)
         {
         tracer()->insertCounter(isInlineable, callNodeTreeTop);
         callsite->_failureReason=isInlineable;
         callsite->removeAllTargets(tracer(),isInlineable);
         return;
         }

      if (comp()->fe()->isInlineableNativeMethod(comp(), callsite->_initialCalleeSymbol))
         {
         TR_VirtualGuardSelection *guard = new (trStackMemory()) TR_VirtualGuardSelection(TR_NoGuard);
         callsite->addTarget(trMemory(),this ,guard ,callsite->_initialCalleeSymbol->getResolvedMethod(),callsite->_receiverClass);
         }

      callsite->_initialCalleeMethod = callsite->_initialCalleeSymbol->getResolvedMethod();

      }
   else if ((isInlineable = static_cast<TR_InlinerFailureReason> (checkInlineableWithoutInitialCalleeSymbol (callsite, comp()))) != InlineableTarget)
      {
      tracer()->insertCounter(isInlineable, callNodeTreeTop);
      callsite->_failureReason=isInlineable;
      callsite->removeAllTargets(tracer(),isInlineable);
      return;
      }

   if (callsite->_initialCalleeSymbol && callsite->_initialCalleeSymbol->firstArgumentIsReceiver())
      {
      if (callsite->_isIndirectCall && !callsite->_receiverClass)
         {
         if (callsite->_initialCalleeMethod)
            callsite->_receiverClass = callsite->_initialCalleeMethod->classOfMethod();

         int32_t len;
         const char * s = callNode->getChild(callNode->getFirstArgumentIndex())->getTypeSignature(len);
         TR_OpaqueClassBlock * type = s ? fe()->getClassFromSignature(s, len, callsite->_callerResolvedMethod, true) : 0;
         if (type && (!callsite->_receiverClass || (type != callsite->_receiverClass && fe()->isInstanceOf(type, callsite->_receiverClass, true, true) == TR_yes)))
            callsite->_receiverClass = type;
         }
      }

   if(tracer()->debugLevel())
      tracer()->dumpCallSite(callsite, "CallSite Before finding call Targets");

   //////////

   if (findNewTargets)
      {
      callsite->findCallSiteTarget(callStack, this);
      applyPolicyToTargets(callStack, callsite);
      }

   if(tracer()->debugLevel())
      tracer()->dumpCallSite(callsite, "CallSite after finding call Targets");

   for (int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *target = callsite->getTarget(i);
      if (!target->_calleeSymbol || !target->_calleeMethod->isSameMethod(target->_calleeSymbol->getResolvedMethod()))
         {
         target->_calleeMethod->setOwningMethod(callsite->_callNode->getSymbolReference()->getOwningMethodSymbol(comp())->getResolvedMethod()); //need to point to the owning method to the same one that got ilgen'd d157939

         target->_calleeSymbol = comp()->getSymRefTab()->findOrCreateMethodSymbol(
               symRef->getOwningMethodIndex(), -1, target->_calleeMethod, TR::MethodSymbol::Virtual /* will be target->_calleeMethodKind */)->getSymbol()->castToResolvedMethodSymbol();
         }

      TR_InlinerFailureReason checkInlineableTarget = getPolicy()->checkIfTargetInlineable(target, callsite, comp());

      if (checkInlineableTarget != InlineableTarget)
         {
         tracer()->insertCounter(checkInlineableTarget,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),checkInlineableTarget);
         i--;
         continue;
         }

      TR::RecognizedMethod rm = target->_calleeSymbol ? target->_calleeSymbol->getRecognizedMethod() : target->_calleeMethod->getRecognizedMethod();
      if (rm != TR::unknownMethod && !inlineRecognizedMethod(rm))
         {
         tracer()->insertCounter(Recognized_Callee,callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Recognized_Callee);
         i--;
         continue;
         }

      getUtil()->collectCalleeMethodClassInfo(target->_calleeMethod);
      }

   if(callsite->numTargets())    //if we still have targets at this point we can return true as we would have in the loop
      return;

   if ( getUtil()->getCallCount(callNode) > 0)
      {
      if (callsite->isInterface())
         TR::Options::INLINE_failedToDevirtualize ++;
      else
         TR::Options::INLINE_failedToDevirtualizeInterface ++;
      }

   if (callsite->numTargets()>0 && callsite->getTarget(0) && !callsite->getTarget(0)->_calleeMethod && comp()->trace(OMR::inlining))
      {
      traceMsg(comp(), "inliner: method is unresolved: %s into %s\n", callsite->_interfaceMethod->signature(trMemory()), tracer()->traceSignature(callStack->_methodSymbol));
      callsite->_failureReason=Unresolved_Callee;
      }

   callsite->removeAllTargets(tracer(),Unresolved_Callee);
   callsite->_failureReason=No_Inlineable_Targets;
   return;
   }



bool OMR_InlinerPolicy::canInlineMethodWhileInstrumenting(TR_ResolvedMethod *method)
   {
   return true;
   }

void TR_InlinerBase::applyPolicyToTargets(TR_CallStack *callStack, TR_CallSite *callsite)
   {
   for (int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);

      if (!supportsMultipleTargetInlining () && i > 0)
         {
         callsite->removecalltarget(i,tracer(),Exceeds_ByteCode_Threshold);
         i--;
         continue;
         }

      TR_ASSERT(calltarget->_guard, "assertion failure");
      if (!getPolicy()->canInlineMethodWhileInstrumenting(calltarget->_calleeMethod))
         {
         callsite->removecalltarget(i,tracer(),Needs_Method_Tracing);
         i--;
         continue;
         }

      int32_t bytecodeSize = getPolicy()->getInitialBytecodeSize(calltarget->_calleeMethod, calltarget->_calleeSymbol, comp());

      getUtil()->estimateAndRefineBytecodeSize(callsite, calltarget, callStack, bytecodeSize);
      if (calltarget->_calleeSymbol && strstr(calltarget->_calleeSymbol->signature(trMemory()), "FloatingDecimal"))
         {
         bytecodeSize >>= 1;

         if (comp()->trace(OMR::inlining))
            traceMsg( comp(), "Reducing bytecode size to %d because it's method of FloatingDecimal\n", bytecodeSize);
         }

      bool toInline = getPolicy()->tryToInline(calltarget, callStack, true);

      TR_ByteCodeInfo &bcInfo = callsite->_bcInfo;
      // get the number of locals in the callee
      int32_t numberOfLocalsInCallee = calltarget->_calleeMethod->numberOfParameterSlots();// + calleeResolvedMethod->numberOfTemps();
      if (!forceInline(calltarget) &&
            exceedsSizeThreshold(callsite, bytecodeSize,
                                   (callsite->_callerBlock != NULL) ? callsite->_callerBlock :
                                      (callsite->_callNodeTreeTop ? callsite->_callNodeTreeTop->getEnclosingBlock() : 0),
                                   bcInfo, numberOfLocalsInCallee, callsite->_callerResolvedMethod,
                                   calltarget->_calleeMethod,callsite->_callNode,callsite->_allConsts)

         )
         {
         if (toInline)
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "tryToInline pattern matched.  Skipping size check for %s\n", calltarget->_calleeMethod->signature(comp()->trMemory()));
            callsite->tagcalltarget(i, tracer(), OverrideInlineTarget);
            }
         else
           {
            // debugging counters inserted in call
            callsite->removecalltarget(i, tracer(), Exceeds_ByteCode_Threshold);
            i--;
            continue;
            }
         }
      else
         {
         if (toInline)
            {
            if (comp()->trace(OMR::inlining))
               traceMsg(comp(), "tryToInline pattern matched.  Within the size check for %s\n", calltarget->_calleeMethod->signature(comp()->trMemory()));
            // change the default InlineableTarget
            callsite->tagcalltarget(i,tracer(),TryToInlineTarget);
            }
         }

      // only inline recursive calls once
      //
      static char *selfInliningLimitStr = feGetEnv("TR_selfInliningLimit");
      int32_t selfInliningLimit =
           selfInliningLimitStr ? atoi(selfInliningLimitStr)
         : comp()->getMethodSymbol()->doJSR292PerfTweaks()  ? 1 // JSR292 methods already do plenty of inlining
         : 3;
      if (callStack && callStack->isAnywhereOnTheStack(calltarget->_calleeMethod, selfInliningLimit))
         {
         debugTrace(tracer(),"Don't inline recursive call %p %s\n", calltarget, tracer()->traceSignature(calltarget));
         tracer()->insertCounter(Recursive_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Recursive_Callee);
         i--;
         continue;
         }

      TR_InlinerFailureReason checkInlineableTarget = getPolicy()->checkIfTargetInlineable(calltarget, callsite, comp());

      if (checkInlineableTarget != InlineableTarget)
         {
         tracer()->insertCounter(checkInlineableTarget,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),checkInlineableTarget);
         i--;
         continue;
         }

      bool realGuard = false;

      if (comp()->getHCRMode() != TR::none)
         {
         if (calltarget->_guard->_kind != TR_HCRGuard)
            {
            if ((calltarget->_guard->_kind != TR_NoGuard) && (calltarget->_guard->_kind != TR_InnerGuard))
               realGuard = true;
            }
         }
      else
         {
         if ((calltarget->_guard->_kind != TR_NoGuard) && (calltarget->_guard->_kind != TR_InnerGuard))
            realGuard = true;
         }

      if (realGuard && (!inlineVirtuals() || comp()->getOption(TR_DisableVirtualInlining)))
         {
         tracer()->insertCounter(Virtual_Inlining_Disabled, callsite->_callNodeTreeTop);
         callsite->removecalltarget(i, tracer(), Virtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * onlyVirtualInlining = feGetEnv("TR_OnlyVirtualInlining");
      if (comp()->getOption(TR_DisableNonvirtualInlining) && !realGuard)
         {
         tracer()->insertCounter(NonVirtual_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),NonVirtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * dontInlineSyncMethods = feGetEnv("TR_DontInlineSyncMethods");
      if (calltarget->_calleeMethod->isSynchronized() && (!inlineSynchronized() || comp()->getOption(TR_DisableSyncMethodInlining)))
         {
         tracer()->insertCounter(Sync_Method_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Sync_Method_Inlining_Disabled);
         i--;
         continue;
         }

      if (debug("dontInlineEHAware") && calltarget->_calleeMethod->numberOfExceptionHandlers() > 0)
         {
         tracer()->insertCounter(EH_Aware_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),EH_Aware_Callee);
         i--;
         continue;
         }

      if (!callsite->_callerResolvedMethod->isStrictFP() && calltarget->_calleeMethod->isStrictFP())
         {
         tracer()->insertCounter(StrictFP_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),StrictFP_Callee);

         i--;
         continue;
         }

      if (getPolicy()->tryToInline(calltarget, callStack, false))
         {
         tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),DontInline_Callee);
         i--;
         continue;
         }

         {
         TR::SimpleRegex * regex = comp()->getOptions()->getOnlyInline();
         if (regex && !TR::SimpleRegex::match(regex, calltarget->_calleeMethod))
            {
            tracer()->insertCounter(Not_InlineOnly_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),Not_InlineOnly_Callee);
            i--;
            continue;
            }
         }


      }

   return;
   }

//return true if the call is dominated hot call
bool TR_InlinerBase::callMustBeInlinedRegardlessOfSize(TR_CallSite *callsite)
   {

   return false;
   }

static bool traceIfMatchesPattern(TR::Compilation* comp)
   {
   static char* cRegex = feGetEnv ("TR_printIfRegex");

   if (cRegex && comp->getOptions() && comp->getOptions()->getDebug())
      {
      static TR::SimpleRegex * regex = TR::SimpleRegex::create(cRegex);
      if (TR::SimpleRegex::match(regex, comp->signature(), false))
         {
         return true;
         }
      }

   return false;
   }


static void traceRequestToCSI (TR::Compilation* comp, TR_CallSite* callsite)
   {

   ::FILE* fout = stderr;

   fprintf(fout, "%p CALLEE %s indirect %d interface %d in %s\n",
      comp,
      callsite->_isInterface ?
         callsite->_interfaceMethod->signature(comp->trMemory()):
         callsite->_initialCalleeMethod->signature(comp->trMemory()),
      callsite->_isIndirectCall,
      callsite->_isInterface,
      comp->signature()
   );
   fflush(fout);

   TR_Stack<int32_t> & stack = comp->getInlinedCallStack();

   int callerIndex = callsite->_bcInfo.getByteCodeIndex() ;
   int calleeByteCodeIndex = callsite->_bcInfo.getCallerIndex() ;

   for (int32_t depth = stack.size()-1; depth>=-1; depth--)
   {
      const char* methodName = (depth == -1) ? comp->signature() : comp->getInlinedResolvedMethod(stack[depth])->signature (comp->trMemory());
      uint32_t offsetInInliner = (depth+1 == stack.size()) ? callsite->_bcInfo.getByteCodeIndex() :
      comp->getInlinedCallSite(stack[ depth+1])._byteCodeInfo.getByteCodeIndex();
      fprintf(fout, "STACK : %p %s %d [%d]\n", comp, methodName, offsetInInliner, depth != -1 ? stack[depth] : -1);
      fflush(fout);
   }


   /*
   for (;;)
      {
      fprintf(fout, "%p %s %d\n",
            comp,
            (callerIndex == -1) ? comp->signature() : comp->getInlinedResolvedMethod(stack[callerIndex])->signature (comp->trMemory()) ,
            calleeByteCodeIndex);
      fflush(fout);

      if (callerIndex == -1)
         break;

      calleeByteCodeIndex = comp->getInlinedCallSite(stack[ callerIndex])._byteCodeInfo.getByteCodeIndex();
      callerIndex = comp->getInlinedCallSite(stack[ callerIndex])._byteCodeInfo.getCallerIndex();
      }
    */


   }

TR_CallSite* TR_InlinerBase::findAndUpdateCallSiteInGraph(TR_CallStack *callStack,TR_ByteCodeInfo &bcInfo, TR::TreeTop *tt, TR::Node *parent, TR::Node *callNode, TR_CallTarget *calltarget)
   {
   if (calltarget->_myCallees.isEmpty())
      {
      debugTrace(tracer(),"findAndUpdateCallsiteInGraaph: calltarget %p has empty _myCallees",calltarget);
      return 0;
      }

   bool foundCall = false;
   TR_CallSite *callsite = 0;
   for (callsite = calltarget->_myCallees.getFirst() ; callsite ; callsite = callsite->getNext())
      {
      debugTrace(tracer(),"callNode->getByteCodeIndex = %d callsite->_byteCodeIndex = %d",callNode->getByteCodeIndex(),callsite->_byteCodeIndex);
      if (callNode->getByteCodeIndex() == callsite->_byteCodeIndex)     // we have the call
         {
         foundCall = true;
         break;
         }
      }

   if (foundCall && callNode->getSymbolReference()->isUnresolved() && !callsite->isInterface())
      {
      debugTrace(tracer(),"findAndUpdateCallsiteInGraaph: call at node %p is unresolved. Bail out", callNode);
      return 0;
      }


   //preventing recursive inlining by methodHandleInliningGroup
   if (callNode->isTheVirtualCallNodeForAGuardedInlinedCall())
      {
      heuristicTrace(tracer(),"findAndUpdateCallSiteInGraph: this call %p is on a cold side. Bail out", callNode);
      return 0;
      }


   //Add support for counters
   //
   //

   bool foundDeadCall = false;
   if (comp()->getOptions()->enableDebugCounters())
      {
      if (!foundCall)
         {
         for (callsite = calltarget->_deletedCallees.getFirst() ; callsite ; callsite = callsite->getNext())
            {
            debugTrace(tracer(),"considering deleted callee %p callNode->getByteCodeIndex = %d callsite->_byteCodeIndex = %d",callsite,callNode->getByteCodeIndex(),callsite->_byteCodeIndex);
            if (callNode->getByteCodeIndex() == callsite->_byteCodeIndex)     // we have the call
               {
               foundDeadCall = true;
               break;
               }
            }
         }
      }
   if (!foundCall && !foundDeadCall)
      {
      if (tracer()->debugLevel())
         tracer()->dumpCallSite(calltarget->_myCallees.getFirst(), "findAndUpdateCallsiteInGraaph: could not match call form IL to call in graph for the following callsite. callTarget %s, numCallees = %d numDeletedCallees = %d falureReason = %d", calltarget->_calleeSymbol->getResolvedMethod()->signature(comp()->trMemory()), calltarget->_numCallees, calltarget->_numDeletedCallees, calltarget->_failureReason);

      tracer()->insertCounter(Unresolved_In_ECS,tt);

      return 0;
      }

   //updating the fields on the callsite now that I have the info.  The rest should be filled out properly in estimatecodesize.
   callsite->_callNode = callNode;
   callsite->_callNodeTreeTop = tt;
   callsite->_parent = parent;
   callsite->_initialCalleeSymbol = callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getResolvedMethodSymbol();

   if (foundDeadCall)
      {
      int32_t k=0;
      for (k = 0; k < callsite->numRemovedTargets(); k++)
         {
         tracer()->insertCounter(callsite->getRemovedTarget(k)->getCallTargetFailureReason(), callsite->_callNodeTreeTop);
         }
      if (k == 0)  //if you never found a target in the first place.
         {
         tracer()->insertCounter(callsite->getCallSiteFailureReason(), callsite->_callNodeTreeTop);
         }
      }

   if (!foundCall)
      return 0;

   if (callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->isInterface() && callsite->_initialCalleeSymbol)
      debugTrace(tracer(), "findAndUpdateCallSiteInGraph: BAD: Interface call has an initialCalleeSYmbol %p for calNode %p", callsite->_initialCalleeSymbol, callNode);

   if (tracer()->heuristicLevel())
      {
      tracer()->dumpCallSite(callsite, "before findAndUpdateCallSiteInGraph for %p", callsite);
      }

   //if ilgen and localopts decided that this was a direct call
   //there isn't much sense to deal with more than one target
   //either find a matching one
   //or make any target look like callsite's calleeSymbol
   if (!callNode->getOpCode().isCallIndirect())
      {
      //find a matching target if there's any
      for (int32_t i = 0 ; i < callsite->numTargets() ; i++)
         {
         if (callsite->getTarget(i)->_calleeMethod->isSameMethod(callsite->_initialCalleeSymbol->getResolvedMethod()) && i > 0)
            {
            TR_CallTarget* tempTarget = callsite->getTarget(0);
            callsite->setTarget(0, callsite->getTarget(i));
            callsite->setTarget(i, tempTarget);
            break;
            }
         }

      //more than one target, clear the rest
      if (callsite->numTargets() > 1)
         {
         callsite->removeTargets(tracer(), 1, Not_Sane);
         }

     int32_t i = 0;
                     //at this point there should be just one target
                     //if we are lucky it is the one whose calleeMethod matches the one of callsite's calleeSymbol
                     //also it seemed less invasive to assign i to 0 rather than change every single occurence of (i)

      // Code to help Inliner cope with the fact that IAP might have refined targets
      if (callsite->numTargets() > 0)
         {
         if (callsite->isIndirectCall()) //local opts pre-existence has been performed
         {
         if (comp()->compileRelocatableCode())
            callsite->getTarget(i)->_guard->_kind = TR_DirectMethodGuard;
         else
            callsite->getTarget(i)->_guard->_kind = TR_NoGuard;                                // do not need a guard anymore
         callsite->getTarget(i)->_guard->_type = TR_NonoverriddenTest;
         callsite->getTarget(i)->_guard->_thisClass = 0;

         if (!callsite->getTarget(i)->_calleeMethod->isSameMethod(callsite->_initialCalleeSymbol->getResolvedMethod()))
            {
            callsite->getTarget(i)->_myCallees.setFirst(0);                // preventing furhter inlining as we now don't know what's beyond the new target
            callsite->getTarget(i)->_numCallees=0;                         // may want to improve this eventually
            callsite->getTarget(i)->_partialInline=0;                     // Can't do a partial inline anymore as the blocks are wrong.
            callsite->getTarget(i)->_isPartialInliningCandidate=false;
            }
         callsite->getTarget(i)->_calleeSymbol = callsite->_initialCalleeSymbol;
         callsite->getTarget(i)->_calleeMethod = callsite->_initialCalleeSymbol->getResolvedMethod();
         callsite->_isIndirectCall = false;
         }
         else //if(!callsite->isIndirectCall()) //local opts string peepholes has been performed
         {
         if (!callsite->getTarget(i)->_calleeMethod->isSameMethod(callsite->_initialCalleeSymbol->getResolvedMethod()))
            {
            callsite->getTarget(i)->_guard->_thisClass = 0;
            callsite->getTarget(i)->_myCallees.setFirst(0);                // preventing furhter inlining as we now don't know what's beyond the new target
            callsite->getTarget(i)->_numCallees=0;                         // may want to improve this eventually
            callsite->getTarget(i)->_partialInline=0;                     // Can't do a partial inline anymore as the blocks are wrong.
            callsite->getTarget(i)->_isPartialInliningCandidate=false;
            callsite->getTarget(i)->_calleeSymbol = callsite->_initialCalleeSymbol;
            callsite->getTarget(i)->_calleeMethod = callsite->_initialCalleeSymbol->getResolvedMethod();
               }
            }
            }
         }

   //if the code above kicked in this will only run once.
   for (int32_t i = 0 ; i< callsite->numTargets() ; i++)
      {
      if (callsite->getTarget(i)->_isPartialInliningCandidate && callsite->getTarget(i)->_partialInline)
          callsite->getTarget(i)->_partialInline->setCallNodeTreeTop(tt);

      if (!callsite->getTarget(i)->_calleeSymbol && !callsite->isInterface())
         {
         //in case you are curious about what we do for interfaces
         //we will fill in calleeSymbol next time we call getSymbolAndFindInlineTargets
         callsite->getTarget(i)->_calleeSymbol = callsite->_initialCalleeSymbol;
         }

      if (callsite->getTarget(i)->_guard->_kind == TR_InterfaceGuard && callsite->getTarget(i)->_guard->_type == TR_MethodTest && callsite->_initialCalleeSymbol)
         {
         if(tracer()->debugLevel())
            tracer()->dumpCallSite(callsite,"findAndUpdateCallSiteInGraph: BAD: Interface call has an initialCalleeSYmbol %p for calNode %p",callsite->_initialCalleeSymbol,callNode);
         TR_ASSERT(0, " target has an interface guard AND an initial callee symbol.. (bad!)\n");

         }
      }

   if (tracer()->heuristicLevel())
      {
      tracer()->dumpCallSite(callsite, "after findAndUpdateCallSiteInGraph for %p", callsite);
      }

   getSymbolAndFindInlineTargets(callStack,callsite,false);

   if (!callsite->numTargets())
      {
      debugTrace(tracer(),"getSymbolAndFindInlineTargets failed.");
      return 0;
      }

   return callsite;

   }


void TR_InlinerBase::inlineFromGraph(TR_CallStack *prevCallStack, TR_CallTarget *calltarget, TR_InnerPreexistenceInfo *innerPrexInfo)
   {
   bool trace = comp()->trace(OMR::inlining);
   TR_InlinerDelimiter delimiter(tracer(),"TR_InlinerBase::inlineFromGraph");

   TR::ResolvedMethodSymbol *callerSymbol = calltarget->_calleeSymbol;           // the current call target

   debugTrace(tracer(),"inlineFromGraph: calltarget %p, symbol %p",calltarget,callerSymbol);

   TR_CallStack callStack(comp(), callerSymbol, callerSymbol->getResolvedMethod(), prevCallStack, 0, true);

   if (innerPrexInfo)
      callStack._innerPrexInfo = innerPrexInfo;

   int32_t thisSiteIndex = callerSymbol->getFirstTreeTop()->getNode()->getInlinedSiteIndex();
   bool      isCold        = false;

   TR_ScratchList<TR_CallTarget> targetsToInline(comp()->trMemory());


   for (TR::TreeTop * tt = callerSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node * parent = tt->getNode();
      if (parent->getOpCodeValue() == TR::BBStart)
         {
         isCold = false;
         TR::Block *block = parent->getBlock();

         // dont inline into cold blocks
         //
         if (block->isCold() || !block->getExceptionPredecessors().empty())
            {
            isCold = true;
            //tt = block->getExit();
            //continue;
            }
         if (trace && isCold)
            traceMsg(comp(), "\n Block %d is cold\n" ,block->getNumber());
         }

      // Must be J9-specific; FE functions would assert otherwise

      // assume that all call nodes are the first child of a treetop
      bool isCall = tt->getNode()->getNumChildren() && tt->getNode()->getChild(0)->getOpCode().isCall();
      if (isCall &&
          tt->getNode()->getChild(0)->getVisitCount() != _visitCount &&
          tt->getNode()->getChild(0)->getInlinedSiteIndex() == thisSiteIndex &&
          !tt->getNode()->getChild(0)->getSymbolReference()->getSymbol()->castToMethodSymbol()->isInlinedByCG() &&
          //induceOSR has the same bcIndex and caller index of the call that follows it
          //the following conditions allows up to skip it
          tt->getNode()->getChild(0)->getSymbolReference() != comp()->getSymRefTab()->element(TR_induceOSRAtCurrentPC)
         )
         {
         TR::Node * node = parent->getChild(0);
         TR::Symbol *sym  = node->getSymbol();

         TR_CallStack::SetCurrentCallNode sccn(callStack, node);
         TR::SymbolReference * symRef = node->getSymbolReference();
         bool oldIsCold = isCold; // added to account for the fact that isCold is changed by refineColdness call
         getUtil()->refineColdness(node, isCold);

         if (!isCold)
            {
            debugTrace(tracer(),"inlineFromGraph:: about to call findAndUpdateCallSiteInGraph on call target %p for call at node %p",calltarget,node);
            TR_CallSite *site = findAndUpdateCallSiteInGraph(&callStack,node->getByteCodeInfo(), tt, parent, node, calltarget);
            debugTrace(tracer(),"inlineFromGraph: found a call at parent %p child %p, findAndUpdateCallsite returned callsite %p",parent,parent->getChild(0),site);

            if (site)
               {
               for (int32_t i = 0; i < site->numTargets(); i++)
                  {
                  TR_CallTarget *target = site->getTarget(i);
                  getUtil()->computePrexInfo(target);
                  targetsToInline.add(target);
                  }
               }
            }
         else
            {
            TR::MethodSymbol * calleeSymbol = symRef->getSymbol()->castToMethodSymbol();
            if (calleeSymbol->getMethod())
               heuristicTrace(tracer(), "Block containing call node %p (callee name: %s) but either caller block or function is cold. Skipping call.",node,
                  calleeSymbol->getMethod()->signature(trMemory(), stackAlloc));
            tracer()->insertCounter(Cold_Block,tt);
            }

         isCold = oldIsCold;
         node->setVisitCount(_visitCount);
         }
      }

   debugTrace(tracer(),"Done First Tree Iteration for updating PrexInfo.  Now starting inline Loop for calltarget %p",calltarget);

   ListIterator<TR_CallTarget> iter(&targetsToInline);
   for (TR::TreeTop * tt = callerSymbol->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
      {
      iter.reset();
      for (TR_CallTarget *target = iter.getFirst(); target; target = iter.getNext())
         {
         if (target->_myCallSite->_callNodeTreeTop == tt && !target->_alreadyInlined)
            {
            debugTrace(tracer(),"inlineFromGraph: Calling inlinecallTarget on calltarget %p in callsite %p", target, target->_myCallSite);
            inlineCallTarget(&callStack, target, true, NULL, &tt);
            break;
            }
         }
      }

   callStack.commit();
   }

static void updateCallersFlags(TR::ResolvedMethodSymbol* callerSymbol, TR::ResolvedMethodSymbol* calleeSymbol, TR::Optimizer * optimizer)
   {

   // Various properties of the callee are now also true of the caller

   if (calleeSymbol->mayHaveLoops())
      {
      if (callerSymbol->mayHaveLoops())
         callerSymbol->setMayHaveNestedLoops(true);
      else
         callerSymbol->setMayHaveLoops(true);
      }

   if (calleeSymbol->mayContainMonitors())
      {
      callerSymbol->setMayContainMonitors(true);
      optimizer->setRequestOptimization(OMR::redundantMonitorElimination);
      }

   if (calleeSymbol->hasNews())
      callerSymbol->setHasNews(true);

   if (calleeSymbol->hasDememoizationOpportunities())
      callerSymbol->setHasDememoizationOpportunities(true);

   if (calleeSymbol->hasMethodHandleInvokes())
      callerSymbol->setHasMethodHandleInvokes(true);

   // Make sure we propagate to the top-level symbol too
   //
   static char *disablePropagatingCalleeFlagsToTopLevelMethod = feGetEnv("TR_disablePropagatingCalleeFlagsToTopLevelMethod");
   if (callerSymbol != optimizer->comp()->getJittedMethodSymbol() && !disablePropagatingCalleeFlagsToTopLevelMethod)
      updateCallersFlags(optimizer->comp()->getJittedMethodSymbol(), calleeSymbol, optimizer);
   }

//decide whether to inline some small methods like unsafe and getClassAccessFlags
bool TR_InlinerBase::tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget)
   {
   return getPolicy()->tryToInlineTrivialMethod(callStack, calltarget);
   }

bool OMR_InlinerPolicy::tryToInlineTrivialMethod (TR_CallStack* callStack, TR_CallTarget* calltarget)
   {
   return false;
   }

//returns false when inlining fails
//TODO: currently this method returns true in some cases when the inlining fails. This needs to be fixed
bool TR_InlinerBase::inlineCallTarget2(TR_CallStack * callStack, TR_CallTarget *calltarget, TR::TreeTop** cursorTreeTop, bool inlinefromgraph, int32_t)
   {
   TR_InlinerDelimiter delimiter(tracer(),"inlineCallTarget2");
   //printf("*****INLINERCALLSITE2: BEGIN for calltarget %p*****\n",calltarget);
   TR::ResolvedMethodSymbol * calleeSymbol = calltarget->_calleeSymbol;
   TR::TreeTop * callNodeTreeTop = calltarget->_myCallSite->_callNodeTreeTop;

   TR::Node * parent = calltarget->_myCallSite->_parent;
   TR::Node * callNode = calltarget->_myCallSite->_callNode;
   TR_VirtualGuardSelection *guard = calltarget->_guard;

   calltarget->_myCallSite->_visitCount++;

   TR::ResolvedMethodSymbol * callerSymbol = callStack->_methodSymbol;

   debugTrace(tracer(),"Starting Physical Inlining for call target %p at callsite %p with visit count %d and numtargets = %d \n Available temps: ",calltarget,calltarget->_myCallSite,calltarget->_myCallSite->_visitCount, calltarget->_myCallSite->numTargets());

   static char *teststring = feGetEnv("TR_WHOSINLININGME");
   if (teststring && strncmp(calleeSymbol->signature(trMemory()), teststring, strlen(teststring))==0 )
      printf(" Inlining %s INTO %s\n",calleeSymbol->signature(trMemory()),comp()->getMethodSymbol()->signature(trMemory()));

   if (tryToInlineTrivialMethod(callStack, calltarget))
      return true;

   if (comp()->getOption(TR_InlineNativeOnly))
      return false;
   comp()->getFlowGraph()->setMaxFrequency(-1);
   comp()->getFlowGraph()->setMaxEdgeFrequency(-1);

   bool genILSucceeded = false;
   int32_t numberOfNodesBefore = comp()->getNodeCount();        // want to know the number of nodes created before and after genIL

   TR::TreeTop * previousBBStartInCaller = 0, * tt, * nextBBEndInCaller = 0;
   for (tt = callNodeTreeTop->getPrevTreeTop(); tt; tt = tt->getPrevTreeTop())
      if (tt->getNode()->getOpCodeValue() == TR::BBStart)
         {
         previousBBStartInCaller = tt;
         break;
         }

   TR::Block * blockContainingTheCall = previousBBStartInCaller->getNode()->getBlock();

   if (calltarget->_myCallSite->numTargets()>1)
      {
      if (calltarget->_myCallSite->_visitCount == 1)
         {
         //this is the first time we are going to inline this call
         //let's cache the next treetop in the same block
         //this is needed to track the original block boundaries
         //so we free up injected basic block temps correctly
         //caching will allows us to skip over multiple merge blocks with single gotos that
         //subsequent inlines of the same call will create
         //also, it is better to cache the next treetop before things get too funky.
         calltarget->_myCallSite->_cursorTreeTop = callNodeTreeTop->getNextTreeTop();
         }
      else if (calltarget->_myCallSite->_visitCount == calltarget->_myCallSite->numTargets())
         // /*!blockContainingTheCall->getExit()->getPrevTreeTop()->getNode()->getOpCode().isReturn()*/)
         {
         //okay we are done inlining this call
         //restore the treetop
         *cursorTreeTop = calltarget->_myCallSite->_cursorTreeTop;

         heuristicTrace(tracer(), "Setting cursorTreeTop %p node %p", cursorTreeTop, (*cursorTreeTop)->getNode());
         }
      }

   // Generate the IL for the method to be inlined. If it is synchronized and
   // the call size is marked as safe for desynchronization, mark it as
   // not synchronized while its IL is being generated.

   bool wasSynchronized = false;
   if (calleeSymbol->isSynchronised() && callNode->canDesynchronizeCall())
      {
      wasSynchronized = true;
      calleeSymbol->setUnsynchronised();
      }

   genILSucceeded = tryToGenerateILForMethod(calleeSymbol, callerSymbol, calltarget);

   if (wasSynchronized)
      calleeSymbol->setSynchronised();

   getUtil()->calleeTreeTopPreMergeActions(calleeSymbol, calltarget);

   if (tracer()->heuristicLevel())
      comp()->dumpMethodTrees("calleeSymbol: after genIL", calleeSymbol);

   if (!genILSucceeded)
      return false;

   incCurrentNumberOfNodes(comp()->getNodeCount() - numberOfNodesBefore);

   TR::Block *cfgBlock = NULL;

   // if we are going to inline this method, find the first block frequency
   for (TR::CFGNode *node = calleeSymbol->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      cfgBlock = node->asBlock();
      if (cfgBlock->getEntry() != NULL) break;
      }

   if (comp()->getOption(TR_TraceBFGeneration))
      {
      comp()->dumpMethodTrees("Frequencies dump", calleeSymbol);
      }

   getUtil()->computeMethodBranchProfileInfo (cfgBlock, calltarget,  callerSymbol);

   if (calleeSymbol->getFirstTreeTop())
      {
      TR::TreeTop *curTree = calleeSymbol->getFirstTreeTop();
      while (curTree)
         {
         TR::Node *curNode = curTree->getNode();

         if (curNode->getOpCodeValue() == TR::asynccheck)
            _numAsyncChecks++;

         curTree = curTree->getNextTreeTop();
         }
      }


   if (debug("inliningTrees"))
      {
      dumpOptDetails(comp(), "Inliner: trees for %s\n", calleeSymbol->signature(trMemory()));
      comp()->dumpMethodTrees("after ilGen while inlining", calleeSymbol);
      }


   TR_InnerPreexistenceInfo *innerPrexInfo = getUtil()->createInnerPrexInfo(comp(), calleeSymbol, callStack, callNodeTreeTop, callNode, guard->_kind);
   if (calleeSymbol->mayHaveInlineableCall())
      {
      debugTrace(tracer(),"** Target %p symbol %p may have inlineable calls signature %s\n", calltarget, calleeSymbol, tracer()->traceSignature(calleeSymbol));

      if (comp()->getOption(TR_DisableNewInliningInfrastructure) || !inlinefromgraph)
         inlineCallTargets(calleeSymbol, callStack, innerPrexInfo);
      else
         inlineFromGraph(callStack,calltarget, innerPrexInfo);

      if (comp()->trace(OMR::inlining))
         {
         dumpOptDetails(comp(), "Inliner: trees for %s\n", calleeSymbol->signature(trMemory()));
         comp()->dumpMethodTrees("after inlining while inlining", calleeSymbol);
         }
      }

   if (comp()->getOption(TR_FullSpeedDebug) && !getPolicy()->mustBeInlinedEvenInDebug(calleeSymbol->getResolvedMethod(), callNodeTreeTop) && (!comp()->getOption(TR_EnableOSR) || comp()->getOption(TR_MimicInterpreterFrameShape)))
      {
      TR::Node *decompPoint = findPotentialDecompilationPoint(calleeSymbol, comp());
      if (decompPoint)
         {
         genILSucceeded = false;
         if (comp()->trace(OMR::inlining))
            {
            tracer()->insertCounter(Decompilation_Point, callNodeTreeTop);

            traceMsg(comp(), "FSD inlining: decompilation point %s (%s) prevented inlining of %s\n",
                    comp()->getDebug()->getName(decompPoint->getOpCodeValue()),
                    comp()->getDebug()->getName(decompPoint->getSymbolReference()),
                    tracer()->traceSignature(calleeSymbol));
            }
         return false; // genIlSucceeded has already been checked above
         }
      }

   // If InnerPreexistence added inner assumptions on this method -- and this method is being inlined
   // as a non-virtual - add a dummy virtual guard around this
   //
   if (innerPrexInfo->hasInnerAssumptions() && guard->_kind == TR_NoGuard)
      {
      if (comp()->trace(OMR::inlining))
         {
         traceMsg(comp(), "%sIPREX virtualize the non-virtual call %s because of inner preexistence for inner calls\n",
            OPT_DETAILS, calleeSymbol->signature(trMemory()));
         }
      // FIXME
      //printf("---$XX$--- inner prex virtualized call in %s\n", comp()->signature());
      guard->_kind = TR_DummyGuard;
      }

   TR_PrexArgInfo *argInfo = comp()->getCurrentInlinedCallArgInfo();
   if (argInfo)
      {
      if (comp()->usesPreexistence()) // Mark the preexistent arguments (maybe redundant if VP has already done that and provided the info in argInfo)
         {
         int32_t firstArgIndex = callNode->getFirstArgumentIndex();
         for (int32_t c = callNode->getNumChildren() -1; c >= firstArgIndex; c--)
            {
            TR_PrexArgument *p = argInfo->get(c - firstArgIndex);
            //FIXME: disabled for now
            if (0 && p && p->usedProfiledInfo())
               {
               TR_OpaqueClassBlock *pc = p->getFixedProfiledClass();
               if (pc)
                  {
                  if (guard->_kind == TR_NoGuard)
                     {
                     guard->_kind = TR_InnerGuard;
                     }
                  break;
                  }
               }
            }
         }
      }


   if (!performTransformation(comp(), "%sInlining qwerty %s into %s with a virtual guard kind=%s type=%s partialInline=%d\n",
                              OPT_DETAILS, calleeSymbol->signature(trMemory()),
                              callerSymbol->signature(trMemory()), tracer()->getGuardKindString(guard),
                              tracer()->getGuardTypeString(guard),
                              !comp()->getOption(TR_DisablePartialInlining) && calltarget->_partialInline
                              ))

      {
      return true;
      }

   comp()->incInlinedCalls();

   if (comp()->trace(OMR::inlining))
      traceMsg(comp(), "inlined %i calls so far,  maxInlinedCalls:%i\n", comp()->getInlinedCalls(), comp()->getOptions()->getMaxInlinedCalls());

   if (tracer()->debugLevel())
      {
      debugTrace(tracer(),"For call target %p available temps before pam.initialize(): ",calltarget);

      TR::SymbolReference * a = 0;
      ListIterator<TR::SymbolReference> autos(&_availableTemps);
      for (a = autos.getFirst(); a; a = autos.getNext())
         debugTrace(tracer(),"\t%d",a->getReferenceNumber());
      debugTrace(tracer(),"\navailableBasicBlockTemps: ");
      ListIterator<TR::SymbolReference> bautos(&_availableBasicBlockTemps);
      for(a=bautos.getFirst(); a; a=bautos.getNext())
         debugTrace(tracer(), "\t%d",a->getReferenceNumber());
      }

   TR_ScratchList<TR::SymbolReference> tempList(trMemory());
   TR_ParameterToArgumentMapper pam(callerSymbol, calleeSymbol, callNode, tempList, _availableTemps, _availableBasicBlockTemps, this);
   pam.initialize(callStack);

   pam.printMapping();

   if (calltarget->_myCallSite->_visitCount < calltarget->_myCallSite->numTargets())
      {
      ListIterator<TR::SymbolReference> temps(&tempList);
      TR::SymbolReference *temp = 0;

      debugTrace(tracer(),"For call target %p. Adding the following to unavailable temps:",calltarget);
      for (temp=temps.getFirst(); temp; temp=temps.getNext())
         {
         debugTrace(tracer(),"\t%d",temp->getReferenceNumber());
         calltarget->_myCallSite->_unavailableTemps.add(temp);
         }
      }

   comp()->setCurrentBlock(blockContainingTheCall);

   TR_TransformInlinedFunction *tif = getUtil()->getTransformInlinedFunction(callerSymbol, calleeSymbol, blockContainingTheCall, callNodeTreeTop, callNode, pam, guard, tempList, _availableTemps, _availableBasicBlockTemps);

   tif->transform();

   /**
    At this point, the return node in the callee function has been transformed and replaced with
    the caller's usage of the call to the callee - in other words, a new node has been created
    in the calleee that wasn' there when we gen'd IL for it
   */

   for (tt = callNodeTreeTop->getNextTreeTop(); tt; tt = tt->getNextTreeTop())
      if (tt->getNode()->getOpCodeValue() == TR::BBEnd)
         {
         nextBBEndInCaller = tt;
         break;
         }

   // Merge the main line tree tops from the callee into the caller.
   //
   // Omit the first BBstart and last BBend.
   //
   // Add in the tempTreeTops
   //

   TR::Block * blockAfterTheCall = blockContainingTheCall->getNextBlock();
   TR::TreeTop * treeTopAfterCall = callNodeTreeTop->getNextTreeTop();
   TR::TreeTop * prevTreeTop = callNodeTreeTop->getPrevTreeTop();
   TR::TreeTop * startOfInlinedCall = calleeSymbol->getFirstTreeTop()->getNextTreeTop();
   TR::Block * calleeFirstBlock = calleeSymbol->getFirstTreeTop()->getEnclosingBlock();
   TR::Block * calleeLastBlock = calleeSymbol->getLastTreeTop()->getEnclosingBlock();

   if (pam.firstTempTreeTop())
      {
      prevTreeTop->join(pam.firstTempTreeTop());
      pam.lastTempTreeTop()->join(startOfInlinedCall);
      }
   else
      prevTreeTop->join(startOfInlinedCall);
   tif->lastMainLineTreeTop()->getPrevTreeTop()->join(callNodeTreeTop);

   TR::CFG * calleeCFG = calleeSymbol->getFlowGraph();
   TR::CFG * callerCFG = callerSymbol->getFlowGraph();

   /**Just merged the callee into the caller, and avoided using
        its (the callee's) BBStart and BBEnd tree tops
      BUT
      The original callsite in the caller still hasn't been removed
    */

   // fix up the TR::Block pointers for any intermediate BBStart/BBEnd
   //
   TR::Block * firstCalleeBlock = calleeSymbol->getFirstTreeTop()->getNode()->getBlock();
   TR::Block * lastCalleeBlock = 0;
   if (tif->firstBBEnd())
      {
      // hook up the previous BBStart in the caller with the first BBEnd in the callee
      //
      TR_ASSERT(firstCalleeBlock == tif->firstBBEnd()->getNode()->getBlock(), "Inliner, firstCalleeBlock ambiguity");
      tif->firstBBEnd()->getNode()->setBlock(blockContainingTheCall);
      blockContainingTheCall->setExit(tif->firstBBEnd());

      firstCalleeBlock->setEntry(0);

      // hook up the last BBStart in the callee with the next BBEnd
      // in the caller
      //
      lastCalleeBlock = tif->lastMainLineTreeTop()->getNode()->getBlock();
      nextBBEndInCaller->getNode()->setBlock(lastCalleeBlock);
      lastCalleeBlock->setExit(nextBBEndInCaller);
      }

   // merge cfgs
   //
   callerCFG->setStructure(0);
   calleeCFG->getEnd()->movePredecessors(callerCFG->getEnd());
   //the goto we created for return to inlining might be in the blockcontaining the call
   //cannot remove the edge from the goto to its target
   TR::Block* RTgotoTarget = NULL;

   if (lastCalleeBlock)
      {
      //copy the successors from the call block as long as they are not the goto target
      for (auto e = blockContainingTheCall->getSuccessors().begin(); e != blockContainingTheCall->getSuccessors().end(); )
         {
         if ((*e)->getTo() != RTgotoTarget)
            {
            callerCFG->addEdge(lastCalleeBlock, (*e)->getTo());
            callerCFG->removeEdge(*(e++));
            }
         else
        	 ++e;
         }

      firstCalleeBlock->moveSuccessors(blockContainingTheCall);

      calleeCFG->removeEdge(calleeCFG->getStart(), firstCalleeBlock);
      calleeCFG->removeNode(calleeCFG->getStart());
      calleeCFG->removeNode(calleeCFG->getEnd());

      // Add the nodes from the callee's cfg to the caller's cfg and
      // add any exception successors from the current block to
      // each of the callee's nodes.
      //

      TR::CFGNode * n;
      while ((n = calleeCFG->getNodes().pop()))
         {
         int32_t calleeNodeNumber = n->getNumber();
         callerCFG->addNode(n);
         debugTrace(tracer(),"\nAdding callee blocks into caller: callee block %p:%p:%d --> %d (caller block) ",n, n->asBlock(), calleeNodeNumber, n->getNumber());
         if (!n->asBlock()->isOSRCatchBlock() && !n->asBlock()->isOSRCodeBlock())
            callerCFG->copyExceptionSuccessors(blockContainingTheCall, n, succAndPredAreNotOSRBlocks);
         else
            debugTrace(tracer(),"\ndon't add exception edges from callee OSR block(block_%d) to caller catch blocks\n",n->getNumber());
         }
      }

   blockContainingTheCall->inheritBlockInfo(firstCalleeBlock, firstCalleeBlock->isCold());

   //return block contains goto and vret, vret can't be reached

   bool needSplit = false;

   // add the virtual call guard
   //
   rcount_t originalCallNodeReferenceCount = callNode->getReferenceCount();
   TR::TreeTop * virtualGuard = 0;
   bool devirtualizedByInnerPreexistence = false;
   bool disableTailRecursion = false;
   TR::TreeTop *induceOSRCallTree = NULL;

   if (guard->_kind != TR_NoGuard)
      {
      virtualGuard =
         addGuardForVirtual(callerSymbol, calleeSymbol, callNodeTreeTop, callNode,
                            calltarget->_receiverClass, startOfInlinedCall, previousBBStartInCaller,
                            nextBBEndInCaller, *tif, tempList, guard, &induceOSRCallTree, calltarget);

      TR_VirtualGuard* vGuard = comp()->findVirtualGuardInfo(virtualGuard->getNode());
      heuristicTrace(tracer(), "calltarget %p vGuard %p calltarget->_receiverClass %p callsite->_receiverClass %p\n",
               calltarget, vGuard, calltarget->_receiverClass, calltarget->_myCallSite->_receiverClass);

      // Now that we have created the virtual guard - see if we can remove it by doing
      // inner preexistence (yah, i know what you are thinking -- why create it when i
      // am going to remove it later? if we are going to remove it, we still need it
      // later so that we can add to the outer guard as an inner assumption )
      //
      // FIXME: the following limitation can be removed if we fixup the trees correctly
      // in TR_InlinerBase::inlineCallTarget don't do inner pre on HCRGuard
      if (!_disableInnerPrex && guard->_kind != TR_ProfiledGuard && guard->_kind != TR_HCRGuard /* && guard->_kind != TR_NoGuard */ && guard->_kind != TR_InnerGuard)
         {
         devirtualizedByInnerPreexistence =
            innerPrexInfo->perform(comp(), virtualGuard->getNode(), disableTailRecursion);

         if (disableTailRecursion)
            _disableTailRecursion = true;
         }
      }
   else if (comp()->getOption(TR_EnableOSR) && tif->crossedBasicBlock() && !comp()->osrInfrastructureRemoved()
       && (tif->resultNode() == NULL || tif->resultNode()->getReferenceCount() == 0))
      {
      /**
       * In OSR, we need to split block even for cases without virtual guard. This is
       * because in OSR a block with OSR point must have an exception edge to the osrCatchBlock
       * of correct callerIndex. Split the block here so that the OSR points from callee
       * and from caller are separated.
       *
       * This will not uncommon the return value if the block is split, instead it expects a temporary
       * to have been generated so that nothing is commoned. This is valid under the current
       * inliner behaviour, as the existance of a catch block will result in the required temporary being created.
       */
      TR::Block * blockOfCaller = previousBBStartInCaller->getNode()->getBlock();
      TR::Block * blockOfCallerInCalleeCFG = nextBBEndInCaller->getNode()->getBlock()->split(callNodeTreeTop, callerCFG);
      callerCFG->copyExceptionSuccessors(blockOfCaller, blockOfCallerInCalleeCFG);
      }

   // move the NULLCHK to before the inlined code
   //
   TR::TreeTop * nullCheckTreeTop = 0;
   if (parent->getOpCode().isNullCheck())
      {
      if (parent->getOpCode().isResolveCheck())
         {
         TR::Node::recreate(parent, TR::NULLCHK);
         }
      nullCheckTreeTop = parent->extractTheNullCheck(prevTreeTop);
      }

   // fix up the original call node args
   //
   TR::Node * nullCheckReference = 0;
   if (guard->_kind != TR_NoGuard || needSplit)
      nullCheckReference = pam.fixCallNodeArgs(nullCheckTreeTop != 0);

   // replace the call node with resultNode or remove the caller tree top
   //
   if (callNode->getReferenceCount() > 0)
      replaceCallNode(callerSymbol, tif->resultNode(), originalCallNodeReferenceCount, callNodeTreeTop, parent, callNode);

   // If the next tree top after the call contains the only reference
   // to the call then the reference may have been moved into the inlined call
   // to avoid the creation of a temp.  If this is the case we have to remove
   // the tree top after the call node.
   //
   if (tif->simpleCallReferenceTreeTop())
      callerSymbol->removeTree(tif->simpleCallReferenceTreeTop());

   // The return tree in the callee can't be removed until
   // after the return value has been hooked up to another tree.
   //
   ListIterator<TR::TreeTop> trees(&tif->treeTopsToRemove());
   for (TR::TreeTop * tree = trees.getFirst(); tree; tree = trees.getNext())
      {
      callerSymbol->removeTree(tree);
      }

   // If any nodes before the call point are referenced after the call point
   // and inlining introduced a basic block then the references after the call
   // point have to be replaced with unique nodes
   //
   TR_ScratchList<TR::SymbolReference> injectedBasicBlockTemps(trMemory());
   if (tif->crossedBasicBlock())
      {
      debugTrace(tracer(),"Inlining caused a basic block to be crossed.  Need to fix broken commoning");
      TR::Block * virtualCallSnippet = virtualGuard ? virtualGuard->getNode()->getBranchDestination()->getNode()->getBlock() : 0;
      TR_HandleInjectedBasicBlock hibb(comp(), tracer(), callerSymbol, tempList, injectedBasicBlockTemps, _availableBasicBlockTemps, pam.getFirstMapping());
      hibb.findAndReplaceReferences(previousBBStartInCaller, nextBBEndInCaller->getNode()->getBlock(), virtualCallSnippet);
      if (guard->_kind != TR_NoGuard && nullCheckTreeTop && !nullCheckReference)
         nullCheckReference = hibb.findNullCheckReferenceSymbolReference(nullCheckTreeTop);
      }

   // privatized inliner args above a guard without modification limit code motion and
   // optimization so we now go and check the privatized arguments to see if we can
   // create a duplicate of the argument in the cold path - if so we do to decouple
   // the hot path from the cold path and give other opts more room to work
   // NOTE this used to be part of adding a virtual guard, but we need hibb to store
   // copies of surviving values first so we can exploit them so we do it here
   /*if (virtualGuard)
      rematerializeCallArguments(tif, guard, previousBBStartInCaller->getNode()->getBlock(),
         virtualGuard->getNode()->getBranchDestination()->getNode()->getBlock()->getEntry());*/

   if (induceOSRCallTree)
      {
      TR::TreeTop *guardedCallTree = induceOSRCallTree->getNextTreeTop();
      TR::Node *refCallNode = guardedCallTree->getNode()->getFirstChild();
      TR::Node *induceOSRCallNode = induceOSRCallTree->getNode()->getFirstChild();
      int32_t firstArgChild = refCallNode->getFirstArgumentIndex();
      int32_t numOfChildren = refCallNode->getNumChildren();
      induceOSRCallNode->setNumChildren(numOfChildren - firstArgChild); // tricky "swelling" of the node but we allocated it of the right size when we inserted the induce OSR call to begin with
      for (int32_t i = firstArgChild; i < numOfChildren; i++)
         induceOSRCallNode->setAndIncChild(i-firstArgChild, refCallNode->getChild(i));

      while (guardedCallTree)
         {
         TR::TreeTop *next = guardedCallTree->getNextTreeTop();
         induceOSRCallTree->join(next);
         guardedCallTree->getNode()->recursivelyDecReferenceCount();
         guardedCallTree = next;
         if (((guardedCallTree->getNode()->getOpCodeValue() == TR::athrow) &&
             guardedCallTree->getNode()->throwInsertedByOSR()) ||
             (guardedCallTree->getNode()->getOpCodeValue() == TR::BBEnd))
            break;
         }
      }

   if (tracer()->debugLevel())
      {
      debugTrace(tracer(),"\nAvailable Temps after handling injected basic block: ");
      TR::SymbolReference * a = 0;
      ListIterator<TR::SymbolReference> autos(&_availableTemps);
      for (a = autos.getFirst(); a; a = autos.getNext())
         debugTrace(tracer(),"\t%d",a->getReferenceNumber());
      debugTrace(tracer(),"\n");

      debugTrace(tracer(),"\ninjectedBasicBlockTemps: ");
      ListIterator<TR::SymbolReference> bautos(&injectedBasicBlockTemps);
      for(a=bautos.getFirst(); a; a=bautos.getNext())
         debugTrace(tracer(), "\t%d",a->getReferenceNumber());
      debugTrace(tracer(),"\n");

      debugTrace(tracer(),"\navailableBasicBlockTemps: ");
      ListIterator<TR::SymbolReference> bbautos(&_availableBasicBlockTemps);
      for(a=bbautos.getFirst(); a; a=bbautos.getNext())
         debugTrace(tracer(), "\t%d",a->getReferenceNumber());
      debugTrace(tracer(),"\n");
      }

   // move the null check node into the if clause
   //
   //   if (nullCheckReference && !virtualGuard->getNode()->inlineGuardIndirectsTheThisPointer())
   if (nullCheckReference &&
         !devirtualizedByInnerPreexistence &&
         ((guard->_kind == TR_InnerGuard) ||
          !comp()->findVirtualGuardInfo(virtualGuard->getNode())->indirectsTheThisPointer()))
      {
      TR::Node * passThru = nullCheckTreeTop->getNode()->getFirstChild();
      passThru->getFirstChild()->decReferenceCount();
      passThru->setAndIncChild(0, nullCheckReference);

      nullCheckTreeTop->getPrevTreeTop()->join(nullCheckTreeTop->getNextTreeTop());
      TR::Block * b = virtualGuard->getNextTreeTop()->getNextTreeTop()->getNode()->getBlock();
      b->prepend(nullCheckTreeTop);
      }


   //MT1

   if (calltarget->_myCallSite->numTargets() > 1 &&
         calltarget->_myCallSite->_visitCount!=calltarget->_myCallSite->numTargets())
      {
      TR::Block* virtualCallSnippet = virtualGuard->getNode()->getBranchDestination()->getNode()->getBlock();
      TR::Node* lastNodeInCallBlock = virtualCallSnippet->getExit()->getPrevTreeTop()->getNode();
      if (lastNodeInCallBlock->getOpCode().isReturn() || lastNodeInCallBlock->getOpCode().isGoto())
         {
         TR_ASSERT(!virtualCallSnippet->getExit()->getNextTreeTop(), "this is usually the last thing to be appended to the end of the trees");
         virtualCallSnippet->getPrevBlock()->getExit()->join(NULL);
         nextBBEndInCaller->getNode()->getBlock()->getPrevBlock()->getExit()->insertBefore(
            TR::TreeTop::create(comp(), TR::Node::create(virtualGuard->getNode(), TR::Goto, 0, nextBBEndInCaller->getNode()->getBlock()->getEntry()))
            ); //goto belongs to the callee (?)
         nextBBEndInCaller->getNode()->getBlock()->getPrevBlock()->getExit()->join(virtualCallSnippet->getEntry());
         virtualCallSnippet->getExit()->join(nextBBEndInCaller->getNode()->getBlock()->getEntry());
         //cursorTreeTop is used to communicate to the caller the treeTop from which it should continue iteration
         if (cursorTreeTop)
            {
            *cursorTreeTop = calltarget->_myCallSite->_callNodeTreeTop->getPrevTreeTop();
            heuristicTrace(tracer(), "Setting cursorTreeTop %p node %p", cursorTreeTop, (*cursorTreeTop)->getNode());
            }
         }
       else
         {
         TR_ASSERT(calltarget->_myCallSite->_visitCount == 1, "We can only back off after inlining the first implementation\n");
         bool swapped = false; //hopefully, compiler will get rid of this in prod build
                               //as there would be no use of this var except for the one in TR_ASSERT

         //swap the inlined target with the target at 0
         for (int i = 0; i < calltarget->_myCallSite->numTargets(); i++)
            if (calltarget->_myCallSite->getTarget(i) == calltarget && i > 0)
               {
               TR_CallTarget* tempTarget = calltarget->_myCallSite->getTarget(0);
               calltarget->_myCallSite->setTarget(0, calltarget);
               calltarget->_myCallSite->setTarget(i, tempTarget);
               swapped = true;
               }
         TR_ASSERT(swapped || calltarget->_myCallSite->getTarget(0) == calltarget, "Swapping didn't occur!");
         //remove the rest of targets
         calltarget->_myCallSite->removeTargets(tracer(), 1, Not_Sane);
         }
      }


   // move all the automatics from the callee as well as any generated temporaries
   // the call stack.
   //
   TR::AutomaticSymbol * a;
   ListIterator<TR::AutomaticSymbol> autos(&calleeSymbol->getAutomaticList());
   for (a = autos.getFirst(); a; a = autos.getNext())
      if (!a->isParm())
         callStack->addAutomatic(a);

   TR::SymbolReference * temp;
   ListIterator<TR::SymbolReference> temps(&tempList);
   debugTrace(tracer(),"\nAdding temps to callstack:");
   for (temp = temps.getFirst(); temp; temp = temps.getNext())
      {
      bool donotadd=false;
      if (calltarget->_myCallSite->_visitCount!=calltarget->_myCallSite->numTargets())
         {
         TR::SymbolReference *unavailabletemp=0;
         ListIterator<TR::SymbolReference> unavailabletemps(&calltarget->_myCallSite->_unavailableTemps);
         for(unavailabletemp=unavailabletemps.getFirst(); unavailabletemp; unavailabletemp=unavailabletemps.getNext())
            if (temp ==unavailabletemp)
               donotadd=true;
         }
      if (!donotadd)
         {
         debugTrace(tracer(),"\t%d",temp->getReferenceNumber());
         callStack->addTemp(temp);
         }
      }

   if (calltarget->_myCallSite->_visitCount==calltarget->_myCallSite->numTargets())
      {
      TR::SymbolReference *unavailabletemp=0;
      ListIterator<TR::SymbolReference> unavailabletemps(&calltarget->_myCallSite->_unavailableTemps);
      for(unavailabletemp=unavailabletemps.getFirst(); unavailabletemp; unavailabletemp=unavailabletemps.getNext())
         {
         debugTrace(tracer(),"\t%d",unavailabletemp->getReferenceNumber());
         callStack->addTemp(unavailabletemp);
         }
      }

   debugTrace(tracer(),"\n");
   callStack->makeTempsAvailable(_availableTemps);

   if (calltarget->_myCallSite->_visitCount==calltarget->_myCallSite->numTargets())
      {
      ListIterator<TR::SymbolReference> unavailabletemps(&calltarget->_myCallSite->_unavailableBlockTemps);
      for(TR::SymbolReference* unavailabletemp=unavailabletemps.getFirst(); unavailabletemp; unavailabletemp=unavailabletemps.getNext())
         {
         injectedBasicBlockTemps.add(unavailabletemp);
         }
      calltarget->_myCallSite->_unavailableBlockTemps.setListHead(NULL);
      }
   else
      {
      //add newly injected basic block temps to the list of unavailableBlockTemps till visit the last target
      ListIterator<TR::SymbolReference> unavailabletemps(&injectedBasicBlockTemps);
      for(TR::SymbolReference* unavailabletemp=unavailabletemps.getFirst(); unavailabletemp; unavailabletemp=unavailabletemps.getNext())
         {
         calltarget->_myCallSite->_unavailableBlockTemps.add(unavailabletemp);
         }
      injectedBasicBlockTemps.setListHead(NULL);
      }

   temps.set(&injectedBasicBlockTemps);
   for (temp = temps.getFirst(); temp; temp = temps.getNext())
      callStack->addInjectedBasicBlockTemp(temp);

   debugTrace(tracer(),"Making Autos available for ilgen: ");

   uint32_t lastSlot = calleeSymbol->incTempIndex(fe());
//   for (uint32_t i = calleeSymbol->getFirstJitTempIndex(); i < lastSlot; ++i)
   for (uint32_t i = calleeSymbol->getNumParameterSlots(); i < lastSlot ; ++i)
      {
      bool isTemp = false;
      if (i >= (uint32_t)calleeSymbol->getFirstJitTempIndex())
         {
         isTemp = true;
         ListIterator<TR::SymbolReference> autos(&calleeSymbol->getAutoSymRefs(i));
         for (TR::SymbolReference * sr = autos.getFirst(); sr; sr = autos.getNext())
            {
            debugTrace(tracer(),"\t%d",sr->getReferenceNumber());
            comp()->getSymRefTab()->makeAutoAvailableForIlGen(sr);
            }
         }
      if (isTemp || tif->resultNode())
         calleeSymbol->getAutoSymRefs(i).deleteAll();
      }
   //clear pending push sym refs to ensure symbol
   // do not overlap if we inline the same method
   // more then once in a compile
   if (calleeSymbol->getPendingPushSymRefs())
      {
      for (uint32_t i = 0; i < calleeSymbol->getPendingPushSymRefs()->size(); i++)
         calleeSymbol->getPendingPushSymRefs(i).deleteAll();
      }

   if (tracer()->debugLevel())
      {
      debugTrace(tracer(), "\nAvailable Temps after makeTempsAvailable: ");
      TR::SymbolReference * a = 0;
      ListIterator<TR::SymbolReference> autos(&_availableTemps);
      for (a = autos.getFirst(); a; a = autos.getNext())
         debugTrace(tracer(), "\t%d",a->getReferenceNumber());
      debugTrace(tracer(), "\n");
      }

   updateCallersFlags(callerSymbol, calleeSymbol, _optimizer);


   if (getUtil()->needTargetedInlining(calleeSymbol))
      {
      _optimizer->setRequestOptimization(OMR::methodHandleInvokeInliningGroup);
      if (comp()->trace(OMR::inlining))
         heuristicTrace(tracer(),"Requesting another pass of targeted inlining due to %s\n", tracer()->traceSignature(calleeSymbol));
      }

   // Append the callee's catch block to the end of the caller
   //
   if (tif->firstCatchBlock() && callerCFG->getNodes().find(tif->firstCatchBlock()))
      callerSymbol->getLastTreeTop(nextBBEndInCaller->getNode()->getBlock())->join(tif->firstCatchBlock()->getEntry());

   calleeSymbol->setFirstTreeTop(0); // can't reuse the trees once they've been inlined.


   calltarget->_alreadyInlined = true;

   int32_t bytecodeSize = getPolicy()->getInitialBytecodeSize(calleeSymbol->getResolvedMethod(), calleeSymbol, comp());
   TR_ResolvedMethod * reportCalleeMethod = calleeSymbol->getResolvedMethod();
   int32_t numberOfLocalsInCallee = reportCalleeMethod->numberOfParameterSlots() + reportCalleeMethod->numberOfTemps();

   // TODO (Issue #389): Inserting any of these debug counters below causes reference count exceptions during optimization phase
   //
   // TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/succeeded", callNodeTreeTop);
   // TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/succeeded:#bytecodeSize", callNodeTreeTop, bytecodeSize);
   // TR::DebugCounter::prependDebugCounter(comp(), "inliner.callSites/succeeded:#localsInCallee", callNodeTreeTop, numberOfLocalsInCallee);

   if (debug("inliningTrees"))
      {
      dumpOptDetails(comp(), "Inliner: trees for %s\n", callerSymbol->signature(trMemory()));
      //comp()->dumpMethodTrees("after inlining a call site", callerSymbol);
      }
   // printf("*****INLINERCALLSITE2: END*****\n");
   return true;
   }

bool
TR_InlinerBase::forceVarInitInlining(TR_CallTarget *calltarget)
   {
   return false;
   }

bool
TR_InlinerBase::forceCalcInlining(TR_CallTarget *calltarget)
   {
   return false;
   }

bool
OMR_InlinerPolicy::callMustBeInlined(TR_CallTarget *calltarget)
   {
   return false;
   }

bool
TR_InlinerBase::forceInline(TR_CallTarget *calltarget)
   {
   if (getPolicy()->callMustBeInlined(calltarget) || callMustBeInlinedRegardlessOfSize(calltarget->_myCallSite))
      return true;

   return false;
   }

void TR_CallSite::tagcalltarget(int32_t index, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason)
   {
  heuristicTrace(tracer,"Tag Call Target %p from callsite %p for Reason: %s",getTarget(index),this,tracer->getFailureReasonString(reason));
   if (_comp->cg()->traceBCDCodeGen())
      {
      char callerName[1024];
      traceMsg(_comp, "q^q : tag to inline %s into %s (callNode %p on line_no=%d)\n",
         signature(_comp->trMemory()),_comp->fe()->sampleSignature(_callerResolvedMethod->getPersistentIdentifier(), callerName, 1024, _comp->trMemory()),
         _callNode,_comp->getLineNumber(_callNode));
      }

   getTarget(index)->_failureReason = reason;

   TR_ASSERT(index < _mytargets.size(), "Index is greater or equal to the number of targets");
   }

void TR_CallSite::tagcalltarget(TR_CallTarget *calltarget, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason)
   {
   bool foundCallTarget = false;
   for(int32_t i = 0; i < _mytargets.size(); i++)
      {
      if(_mytargets[i] == calltarget)
         {
         tagcalltarget(i,tracer,reason);
         foundCallTarget = true;
         break;
         }

      }
   TR_ASSERT(foundCallTarget, "Call Target not found in CallSite when trying to remove it.\n");
   }

void TR_CallSite::removecalltarget(int32_t index, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason)
   {
   heuristicTrace(tracer,"Removing Call Target %p from callsite %p for Reason: %s",getTarget(index),this,tracer->getFailureReasonString(reason));
   if (_comp->cg()->traceBCDCodeGen() && _callNode != NULL)
      {
      char callerName[1024];
      traceMsg(_comp, "q^q : failing to inline %s into %s (callNode %p on line_no=%d)\n",
         signature(_comp->trMemory()),_comp->fe()->sampleSignature(_callerResolvedMethod->getPersistentIdentifier(), callerName, 1024, _comp->trMemory()),
         _callNode,_comp->getLineNumber(_callNode));
      }

   getTarget(index)->_failureReason = reason;

   TR_ASSERT(index < _mytargets.size(), "Index is greater or equal to the number of targets");
   if (index < _mytargets.size())
      {
      //for getting counters to work
      _myRemovedTargets.push_back(_mytargets[index]);
      _mytargets.erase(_mytargets.begin() + index);
      }

   // don't need to worry too much about freeing memory as it is Stack Memory
   }

void TR_CallSite::removecalltarget(TR_CallTarget *calltarget, TR_InlinerTracer *tracer, TR_InlinerFailureReason reason)
   {
   bool foundCallTarget = false;
   for(int32_t i = 0; i < _mytargets.size(); ++i)
      {
      if(_mytargets[i] == calltarget)
         {
         removecalltarget(i,tracer,reason);
         foundCallTarget = 1;
         break;
         }
      }
   TR_ASSERT(foundCallTarget, "Call Target not found in CallSite when trying to remove it.\n");
   }


void TR_CallSite::removeTargets(TR_InlinerTracer *tracer, int index, TR_InlinerFailureReason reason)
   {
   for (int num = _mytargets.size() - index; num > 0; --num)
      {
      removecalltarget(index,tracer,reason);
      }
   }

void TR_CallSite::removeAllTargets(TR_InlinerTracer *tracer, TR_InlinerFailureReason reason)
   {
   removeTargets(tracer, 0, reason);
   }

TR_CallTarget::TR_CallTarget(TR_CallSite *callsite,
                             TR::ResolvedMethodSymbol *calleeSymbol,
                             TR_ResolvedMethod *calleeMethod,
                             TR_VirtualGuardSelection *guard,
                             TR_OpaqueClassBlock *receiverClass,
                             TR_PrexArgInfo *ecsPrexArgInfo,
                             float freqAdj):
   _myCallSite(callsite),
   _calleeSymbol(calleeSymbol),
   _calleeMethod(calleeMethod),
   _guard(guard),
   _receiverClass(receiverClass),
   _frequencyAdjustment(freqAdj),
   _prexArgInfo(NULL),
   _ecsPrexArgInfo(ecsPrexArgInfo)
   {
   _weight=0;
   _callGraphAdjustedWeight=0;
   _alreadyInlined=false;
   _partialInline=0;
   _numCallees=0;
   _numDeletedCallees=0;
   _partialSize=-1;

   static const char* disableMaxBCI = feGetEnv ("TR_DisableMaxBCI");
   _fullSize= disableMaxBCI ?  0 : calleeMethod->maxBytecodeIndex();

   _isPartialInliningCandidate= (_myCallSite == NULL) ? false : true;
   _originatingBlock=0;
   _cfg=0;
   _failureReason=InlineableTarget;
   _size=-1;
   _calleeMethodKind = TR::MethodSymbol::Virtual;
   }

const char*
TR_CallTarget::signature(TR_Memory *trMemory)
   {
   if (_calleeMethod)
     return _calleeMethod->signature(trMemory);
   else
     return "No Resolved Method";
   }


TR_CallSite::TR_CallSite(TR_ResolvedMethod *callerResolvedMethod,
                         TR::TreeTop *callNodeTreeTop,
                         TR::Node *parent,
                         TR::Node *callNode,
                         TR_Method * interfaceMethod,
                         TR_OpaqueClassBlock *receiverClass,
                         int32_t vftSlot,
                         int32_t cpIndex,
                         TR_ResolvedMethod *initialCalleeMethod,
                         TR::ResolvedMethodSymbol * initialCalleeSymbol,
                         bool isIndirectCall,
                         bool isInterface,
                         TR_ByteCodeInfo & bcInfo,
                         TR::Compilation *comp,
                         int32_t depth,
                         bool allConsts) :
   _callerResolvedMethod(callerResolvedMethod),
   _callNodeTreeTop(callNodeTreeTop),
   _cursorTreeTop(NULL),
   _parent(parent),
   _callNode(callNode),
   _interfaceMethod(interfaceMethod),
   _receiverClass(receiverClass),
   _vftSlot(vftSlot),
   _cpIndex(cpIndex),
   _initialCalleeMethod(initialCalleeMethod),
   _initialCalleeSymbol(initialCalleeSymbol),
   _isIndirectCall(isIndirectCall),
   _isInterface(isInterface),
   _bcInfo(bcInfo),
   _unavailableTemps(comp->trMemory()),
   _unavailableBlockTemps(comp->trMemory()),
   _comp(comp),
   _depth(depth),
   _forceInline(false),
   _isBackEdge(false),
   _allConsts(allConsts),
   _mytargets(0, comp->allocator()),
   _myRemovedTargets(0, comp->allocator()),
   _ecsPrexArgInfo(0)
   {
   _visitCount=0;
   _failureReason=InlineableTarget;
   _byteCodeIndex = bcInfo.getByteCodeIndex();
   _callerBlock = NULL;
   }

const char*
TR_CallSite::signature(TR_Memory *trMemory)
   {
   if (_initialCalleeMethod)
      return _initialCalleeMethod->signature(trMemory);
   else if (_initialCalleeSymbol)
      return _initialCalleeSymbol->signature(trMemory);
   else if (_interfaceMethod)
      return _interfaceMethod->signature(trMemory);
   else
      return "No CallSite Signature";
   }


/**
 * create TR_PrexArgInfo for the calltarget
 * create and set the TR_PrexArgument of the calltarget's first argument, namely the receiver, with information provided by the \p guard
 */
TR_PrexArgInfo*
OMR_InlinerUtil::createPrexArgInfoForCallTarget(TR_VirtualGuardSelection *guard, TR_ResolvedMethod *implementer)
   {
   return NULL;
   }

TR_CallTarget *
TR_CallSite::addTarget(TR_Memory* mem, TR_InlinerBase *inliner, TR_VirtualGuardSelection *guard, TR_ResolvedMethod *implementer, TR_OpaqueClassBlock *receiverClass, TR_AllocationKind allocKind, float ratio)
   {
   TR_PrexArgInfo *myPrexArgInfo = inliner->getUtil()->createPrexArgInfoForCallTarget(guard, implementer);

   TR_CallTarget *result = new (mem,allocKind) TR_CallTarget(this,_initialCalleeSymbol,implementer,guard,receiverClass,myPrexArgInfo,ratio);

   addTarget(result);

   if(inliner->tracer()->heuristicLevel())
      {
      char name[1024];
      heuristicTrace(inliner->tracer(),"Creating a call target %p for callsite %p using a %s and %s .  Signature %s",
         result,this,inliner->tracer()->getGuardKindString(guard),inliner->tracer()->getGuardTypeString(guard),
         _comp->fe()->sampleSignature(implementer->getPersistentIdentifier(), name, 1024, _comp->trMemory()));
      }

   return result;
   }

TR_OpaqueClassBlock *
TR_CallSite::calleeClass()
   {
   if (isInterface())
      {
      TR::StackMemoryRegion stackMemoryRegion(*_comp->trMemory());

      int32_t len = _interfaceMethod->classNameLength();
      char * s = classNameToSignature(_interfaceMethod->classNameChars(), len, _comp, stackAlloc);
      TR_OpaqueClassBlock *result = _comp->fe()->getClassFromSignature(s, len, _callerResolvedMethod, true);

      return result;
      }
   else
      {
      return _initialCalleeMethod->classOfMethod();
      }
   }



/*********** TR_InlineBlocks *************/

TR_InlineBlocks::TR_InlineBlocks(TR_FrontEnd *fe2, TR::Compilation *comp) : fe(fe2), _comp(comp)
   {
   _inlineBlocks = new (_comp->trHeapMemory()) List<TR_InlineBlock>(_comp->trMemory());
   _exceptionBlocks = new (_comp->trHeapMemory()) List<TR_InlineBlock>(_comp->trMemory());

   _callNodeTreeTop=0;

   _lowestBCIndex=INT_MAX;
   _highestBCIndex=-1;
   _numBlocks=0;
   _generatedRestartTree=NULL;

   _numExceptionBlocks=0;
   }


void TR_InlineBlocks::addBlock(TR::Block *block)
   {
   _numBlocks++;

   _inlineBlocks->add(new (_comp->trHeapMemory()) TR_InlineBlock(block->getBlockBCIndex(), block->getNumber() ) );

   }

void TR_InlineBlocks::addExceptionBlock(TR::Block *block)
   {
   _numExceptionBlocks++;

   _exceptionBlocks->add(new (_comp->trHeapMemory()) TR_InlineBlock(block->getBlockBCIndex(), block->getNumber()));

   }
bool TR_InlineBlocks::isInList(int32_t index)
   {
   ListIterator<TR_InlineBlock> blocksIt(_inlineBlocks);
   TR_InlineBlock *aBlock = NULL;
   for (aBlock = blocksIt.getCurrent() ; aBlock; aBlock = blocksIt.getNext())
      {
      if(aBlock->_BCIndex == index)
         return true;
      }
   return false;
   }

bool TR_InlineBlocks::isInExceptionList(int32_t index)
   {
   ListIterator<TR_InlineBlock> blocksIt(_exceptionBlocks);
   TR_InlineBlock *aBlock = NULL;
   for (aBlock = blocksIt.getCurrent() ; aBlock; aBlock = blocksIt.getNext())
      {
      if(aBlock->_BCIndex == index)
         return true;
      }
   return false;
   }



/******************************************* TR_InlinerTracer Class ******************************************************************/
// A class for tracing inlining logic


TR_InlinerTracer::TR_InlinerTracer( TR::Compilation *comp, TR_FrontEnd *fe, TR::Optimization *opt )
  : TR_LogTracer(comp,opt), _fe(fe), _trMemory(_comp->trMemory())
   {
   _traceLevel = 0;
   if(opt)                      //this means I want tracing on
      {
      if (comp->trace(OMR::inlining))
         _traceLevel=trace_heuristic;

      if (comp->getOption(TR_DebugInliner))
         _traceLevel=trace_debug;
      }

   //traceMsg(comp,"_traceLevel set to %d, trace_heuristic = %d trace_debug = %d\n",_traceLevel,trace_heuristic,trace_debug);

   }



void TR_InlinerTracer::partialTraceM ( const char * fmt, ...)
   {
   // Need to guard this with trace Inlining and trace Level
   if(!partialLevel()  || !comp()->getDebug())
      return;
   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

//   traceMsg(comp(), "%s\n",str);
   comp()->getDebug()->traceLnFromLogTracer(str);

   return;
   }

void TR_InlinerTracer::insertCounter (TR_InlinerFailureReason reason, TR::TreeTop *tt)
   {
   const char *name = TR::DebugCounter::debugCounterName(comp(), "inliner.callSites/failed/%s",getFailureReasonString(reason));
   TR::DebugCounter::prependDebugCounter(comp(), name, tt);
   }



#define SIGNATURE_SIZE 1024
void
TR_InlinerTracer::dumpCallGraphs(TR_LinkHead<TR_CallTarget> *targets)
   {
   TR_InlinerDelimiter delimiter(this,"callGraph");

   alwaysTrace(this,"~~~ List of Call Graphs To Be Inlined:");

   TR_Stack<TR_CallTarget*> targetsToBeEvaluated(comp()->trMemory());

   if(!targets->getFirst())
      return;

   TR_CallTarget *calltarget;
   for(calltarget = targets->getFirst() ; calltarget ; calltarget = calltarget->getNext())
      {
      targetsToBeEvaluated.push(calltarget);

      int32_t size=0;
      TR_CallTarget *currentTarget=0;

      traceMsg(comp(),"Call at node %p\n\tDepth\tP.I.\tcalltarget\tsize\tfailure reason\t\t\tbc index\t\tSignature\n",calltarget->_myCallSite->_callNode);

      do
         {
         currentTarget = targetsToBeEvaluated.pop();

         TR_ASSERT(currentTarget->_calleeMethod, "dumping Call Graph: a target has no resolved method!");

         if (currentTarget->_isPartialInliningCandidate)
            size += currentTarget->_partialSize;
         else
            size +=currentTarget->_fullSize;

         char nameBuffer[SIGNATURE_SIZE];

         traceMsg(comp(),"\t%d\t%d\t%p\t%d\t%s",currentTarget->_myCallSite->getDepth(),currentTarget->_isPartialInliningCandidate,currentTarget,currentTarget->_isPartialInliningCandidate ? currentTarget->_partialSize : currentTarget->_fullSize,getFailureReasonString(currentTarget->_failureReason));
         traceMsg(comp(),"\t\t%d\t\t%s\n",currentTarget->_myCallSite->_byteCodeIndex,  comp()->fe()->sampleSignature(currentTarget->_calleeMethod->getPersistentIdentifier(),nameBuffer,SIGNATURE_SIZE,trMemory()));

         if(currentTarget->_partialInline)
            {
            dumpPartialInline(currentTarget->_partialInline);
            }

         if(!currentTarget->_myCallees.isEmpty())
            {
            TR_CallSite *callsite=0;
            for(callsite = currentTarget->_myCallees.getFirst(); callsite; callsite=callsite->getNext())
               {
//               traceMsg(comp(),"\t\t\t\tnumtargets = %d numRemovedTargets = %d currtarget->numdeletedcallees = %d \n",callsite->numTargets(),callsite->numRemovedTargets(),currentTarget->_numDeletedCallees);
               for(int32_t i=0; i<callsite->numTargets(); i++)
                  targetsToBeEvaluated.push(callsite->getTarget(i));

               if(heuristicLevel() && callsite->numRemovedTargets())
                  {
                  for(int32_t i=0; i<callsite->numRemovedTargets(); i++)
                     targetsToBeEvaluated.push(callsite->getRemovedTarget(i));
                  }
               }
            }

         if(heuristicLevel() && !currentTarget->_deletedCallees.isEmpty())
            {
            TR_CallSite *callsite=0;
            for(callsite = currentTarget->_deletedCallees.getFirst(); callsite; callsite=callsite->getNext())
               {
               if(callsite->numTargets())
                  {
                  for(int32_t i=0; i<callsite->numTargets(); i++)
                     targetsToBeEvaluated.push(callsite->getTarget(i));
                  }
               else if (callsite->numRemovedTargets())
                  {
                  for(int32_t i=0; i< callsite->numRemovedTargets(); i++)
                     {
                     targetsToBeEvaluated.push(callsite->getRemovedTarget(i));
                     }
                  }
               else
                  {
                  traceMsg(comp(),"\t%d\t%d\t%p\t%d\t%s",callsite->getDepth(),0,0,0,getFailureReasonString(callsite->_failureReason));
                  traceMsg(comp(),"\t%d\t\t%s\n",callsite->_byteCodeIndex,"No name  Consult bc index");
                  }
               }
            }
         }
      while (!targetsToBeEvaluated.isEmpty());

      traceMsg(comp(),"Total Estimated Size = %d Total Size After Multipliers = %d Total Weight = %d\n\n",size,calltarget->_size,calltarget->_weight);
      }
   }

void
TR_InlinerTracer::dumpDeadCalls(TR_LinkHead<TR_CallSite> *sites)
   {
   TR_InlinerDelimiter delimiter(this,"deadCall");

   TR_CallSite *callsite;
   char name[SIGNATURE_SIZE] ;

   for(callsite = sites->getFirst() ; callsite ; callsite = callsite->getNext())
      {
      traceMsg(comp(),"^^^ Top Level Dead CallSite %p Node %p bcIndex %p Failure Reason: %s\n",callsite,callsite->_callNode,callsite->_byteCodeIndex,getFailureReasonString(callsite->_failureReason));

      TR_CallTarget *calltarget;
      if(callsite->numTargets())
         traceMsg(comp(),"\tCall Targets\n\tDepth\tP.I.\tcalltarget\tsize\tfailure reason\t\t\tbc index\t\tSignature\n");
      for(int32_t i=0 ; i< callsite->numTargets() ; i++)
         {
         calltarget = callsite->getRemovedTarget(i);
         traceMsg(comp(),"\t%d\t%d\t%p\t%d\t%s",calltarget->_myCallSite->getDepth(),calltarget->_isPartialInliningCandidate,calltarget,calltarget->_isPartialInliningCandidate ? calltarget->_partialSize : calltarget->_fullSize,getFailureReasonString(calltarget->_failureReason));
         traceMsg(comp(),"\t\t%d\t\t%s\n",calltarget->_myCallSite->_byteCodeIndex,comp()->fe()->sampleSignature(calltarget->_calleeMethod->getPersistentIdentifier(),name,SIGNATURE_SIZE,trMemory()));
         }
      if(callsite->numRemovedTargets())
         traceMsg(comp(),"Call Targets\n\tDepth\tP.I.\tcalltarget\tsize\tfailure reason\t\t\tbc index\t\tSignature\n");
      for(int32_t i=0; i< callsite->numRemovedTargets() ; i++)
         {
         calltarget = callsite->getRemovedTarget(i);
         traceMsg(comp(),"\t%d\t%d\t%p\t%d\t%s",calltarget->_myCallSite->getDepth(),calltarget->_isPartialInliningCandidate,calltarget,calltarget->_isPartialInliningCandidate ? calltarget->_partialSize : calltarget->_fullSize,getFailureReasonString(calltarget->_failureReason));
         traceMsg(comp(),"\t\t%d\t\t%s\n",calltarget->_myCallSite->_byteCodeIndex, comp()->fe()->sampleSignature(calltarget->_calleeMethod->getPersistentIdentifier(),name,SIGNATURE_SIZE,trMemory()));
         }
      }
   }


void
TR_InlinerTracer::dumpCallSite(TR_CallSite *callsite, const char *fmt, ...)
   {

   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

   traceMsg(comp(), "Inliner: %s\n",str);

      {
      TR_InlinerDelimiter delimiter(this,"callSite");

      traceMsg(comp(), "\t_CallerResolvedMethod = %p",callsite->_callerResolvedMethod);

      traceMsg(comp(), "\t_callNodeTreeTop = %p",callsite->_callNodeTreeTop);
      traceMsg(comp(), "\t_parent = %p",callsite->_parent);
      traceMsg(comp(), "\t_callNode = %p",callsite->_callNode);

      traceMsg(comp(), "\n\t_interfaceMethod = %p",callsite->_interfaceMethod);
      traceMsg(comp(), "\t_receiverClass = %p",callsite->_receiverClass);
      traceMsg(comp(), "\t_vftSlot = %d",callsite->_vftSlot);
      traceMsg(comp(), "\t_cpIndex = %d",callsite->_cpIndex);

      traceMsg(comp(), "\n\t_initialCalleeMethod = %p",callsite->_initialCalleeMethod);
      traceMsg(comp(), "\t_initialCalleeSymbol = %p",callsite->_initialCalleeSymbol);

      traceMsg(comp(), "\t_bcInfo = %p", &(callsite->_bcInfo));
      traceMsg(comp(), "\t_byteCodeIndex = %d",callsite->_byteCodeIndex);

      traceMsg(comp(), "\t_isIndirectCall = %d",callsite->_isIndirectCall);
      traceMsg(comp(), "\n\t_isInterface = %d",callsite->_isInterface);

      traceMsg(comp(), "\tnumtargets() = %d",callsite->numTargets());

      traceMsg(comp(), "\t failureReason = %d %s\n",callsite->_failureReason,getFailureReasonString(callsite->_failureReason));

      if(callsite->_receiverClass)
         {
         char *sig = TR::Compiler->cls.classSignature(comp(), callsite->_receiverClass,trMemory());
         traceMsg(comp(),"\t Call SITE Class Signature = %s\n",sig);
         }

      if(callsite->_callerResolvedMethod)
         {
         char callerName[1024];
         traceMsg(comp(), "\t CALLER signature from method = %s\n", fe()->sampleSignature(callsite->_callerResolvedMethod->getPersistentIdentifier(), callerName, 1024, trMemory()));
         }

      if(callsite->_initialCalleeSymbol)
         traceMsg(comp(), "\t initial CALLEE signature from initial symbol = %s\n",callsite->_initialCalleeSymbol->signature(trMemory()));

      for(int32_t i=0; i<callsite->numTargets(); i++)
         dumpCallTarget(callsite->getTarget(i), "Call Target %d",i);

      for(int32_t i=0; i<callsite->numRemovedTargets(); i++)
         dumpCallTarget(callsite->getRemovedTarget(i), "Dead Target %d", i);

      traceMsg(comp(),"\n");

      }
   }

void
TR_InlinerTracer::dumpCallTarget(TR_CallTarget *calltarget, const char *fmt, ...)
   {
   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

   traceMsg(comp(), "Inliner: %s\n",str);

   traceMsg(comp(), "\tcalltarget= %p\n\t\tguard = %p guard->_kind = %s guard->_type = %s ",calltarget,calltarget->_guard,getGuardKindString(calltarget->_guard),getGuardTypeString(calltarget->_guard));
   traceMsg(comp(), "guard->_thisClass = %p _receiverclass = %p   (enum in compilation.hpp)\n",calltarget->_guard->_thisClass, calltarget->_receiverClass);

   if(calltarget->_calleeSymbol)
      traceMsg(comp(), "\t\t signature from symbol = %s\n",calltarget->_calleeSymbol->signature(trMemory()));
   else
      traceMsg(comp(), "\t\t No callee Symbol yet.\n");

   if(calltarget->_calleeMethod)
      {
      char calleeName[1024];
      traceMsg(comp(), "\t\t signature from method = %s\n",fe()->sampleSignature(calltarget->_calleeMethod->getPersistentIdentifier(), calleeName, 1024, trMemory()));
      }
   else
      traceMsg(comp(), "\t\tNo callee Method yet.\n");

   if(calltarget->_receiverClass != NULL)
      {
      char *sig = TR::Compiler->cls.classSignature(comp(), calltarget->_receiverClass,trMemory());
      traceMsg(comp(),"\t Call TARGET Class Signature = %s\n",sig);
      }
   if(calltarget->_guard->_thisClass && calltarget->_guard->_thisClass != calltarget->_receiverClass)
      {
      char *sig = TR::Compiler->cls.classSignature(comp(), calltarget->_guard->_thisClass, trMemory());
      traceMsg(comp(),"\t Call TARGET GUARD Class Signature = %s\n", sig);
      }

   traceMsg(comp(), "\t\t_size = %d _partialSize = %d _fullSize = %d _weight = %d ",calltarget->_size,calltarget->_partialSize,calltarget->_fullSize,calltarget->_weight);
   traceMsg(comp(), "_callGraphAdjustedWeight = %f \n\t\t_frequencyAdjustment = %f _isPartialInliningCandidate = %d _partialInline = %p\n",calltarget->_callGraphAdjustedWeight,calltarget->_frequencyAdjustment,calltarget->_isPartialInliningCandidate,calltarget->_partialInline);
   traceMsg(comp(), "\t\t_failureReason = %d (%s)  _alreadyInlined = %d\n",calltarget->_failureReason,getFailureReasonString(calltarget->_failureReason),calltarget->_alreadyInlined);

   }

void
TR_InlinerTracer::dumpCallStack(TR_CallStack *callStack, const char *fmt, ...)
   {
   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

   traceMsg(comp(), "Inliner: %s\n",str);

   for (TR_CallStack *cs = callStack; cs; cs = cs->getNext())
      {
      if (cs->_method)
         traceMsg(comp(), "\t0x%p\t%s\n", cs, cs->_method->signature(trMemory()));
      else
         traceMsg(comp(), "\t0x%p\t%s\n", cs, "No _method");
      }
   }

void
TR_InlinerTracer::dumpInline(TR_LinkHead<TR_CallTarget> *targets, const char *fmt, ...)
   {
   va_list args;
   va_start(args,fmt);

   char buffer[2056];

   const char *str = comp()->getDebug()->formattedString(buffer,sizeof(buffer)/sizeof(buffer[0]),fmt,args);

   va_end(args);

   traceMsg(comp(), "Inliner: %s\n",str);

   TR_InlinerDelimiter delimiter(this,"InlineScript");

   TR_Stack<TR_CallTarget*> targetsToBeEvaluated(comp()->trMemory());

   if(!targets->getFirst())
      return;

   // put the caller name here
   traceMsg(comp(),"\t%s\n","[");

   for(TR_CallTarget * calltarget = targets->getFirst() ; calltarget ; calltarget = calltarget->getNext())
      {
      targetsToBeEvaluated.push(calltarget);

      TR_CallTarget *currentTarget = NULL;

      traceMsg(comp(),"\t%s\n","[");

      do
         {
         currentTarget = targetsToBeEvaluated.pop();

         if (currentTarget)
            {
            TR_ASSERT(currentTarget->_calleeMethod, "dumping Call Graph: a target has no resolved method!");
            }
         else
            {
            //end mark
            traceMsg(comp(),"\t%s\n","]");
            continue;
            }

         char nameBuffer[SIGNATURE_SIZE];

         if (currentTarget->_failureReason == InlineableTarget ||
             currentTarget->_failureReason == OverrideInlineTarget ||
             currentTarget->_failureReason == TryToInlineTarget)
            {
            traceMsg(comp(),"\t%s","+");
            traceMsg(comp(),"\t\t\t%s\n",comp()->fe()->sampleSignature(currentTarget->_calleeMethod->getPersistentIdentifier(),nameBuffer,SIGNATURE_SIZE,trMemory()));
            }
         else if (currentTarget->_failureReason == DontInline_Callee)
            {
            traceMsg(comp(),"\t%s","-");
            traceMsg(comp(),"\t\t\t%s\n",comp()->fe()->sampleSignature(currentTarget->_calleeMethod->getPersistentIdentifier(),nameBuffer,SIGNATURE_SIZE,trMemory()));
            }
         else
            {
            //traceMsg(comp(),"\t%s","?");
            //traceMsg(comp(),"\t\t\t%s\n",comp()->fe()->sampleSignature(currentTarget->_calleeMethod->getPersistentIdentifier(),nameBuffer,SIGNATURE_SIZE,trMemory()));
            }

         traceMsg(comp(),"\t%s\n","[");

         if(currentTarget && !currentTarget->_myCallees.isEmpty())
            {

            TR_CallSite *callsite = NULL;

            for(callsite = currentTarget->_myCallees.getFirst(); callsite; callsite=callsite->getNext())
               {
               for(int i=0; i<callsite->numTargets(); i++)
                  targetsToBeEvaluated.push(callsite->getTarget(i));

               if(callsite->numRemovedTargets())
                  {
                  for(int i=0; i<callsite->numRemovedTargets(); i++)
                     targetsToBeEvaluated.push(callsite->getRemovedTarget(i));
                  }
               }
            }

         if(currentTarget && !currentTarget->_deletedCallees.isEmpty())
            {
            TR_CallSite *callsite = NULL;

            for(callsite = currentTarget->_deletedCallees.getFirst(); callsite; callsite=callsite->getNext())
               {
               if(callsite->numTargets())
                  {
                  for(int32_t i=0; i<callsite->numTargets(); i++)
                     targetsToBeEvaluated.push(callsite->getTarget(i));
                  }

               else if (callsite->numRemovedTargets())
                  {
                  for(int32_t i=0; i< callsite->numRemovedTargets(); i++)
                     {
                     targetsToBeEvaluated.push(callsite->getRemovedTarget(i));
                     }
                  }
               else
                  {
                  //No one to push??
                  //traceMsg(comp(),"\t%d\t%d\t%p\t%d\t%s",callsite->getDepth(),0,0,0,getFailureReasonString(callsite->_failureReason));
                  //traceMsg(comp(),"\t%d\t\t%s\n",callsite->_byteCodeIndex,"No name  Consult bc index");
                  }
               }
            }

         //after pushing all the children, let's put an end mark;
         currentTarget = NULL;
         targetsToBeEvaluated.push(currentTarget);
         }
      while (!targetsToBeEvaluated.isEmpty());

      }

   traceMsg(comp(),"\t%s\n","]");
   }

void TR_InlinerTracer::dumpPartialInline (TR_InlineBlocks *partialInline)
   {
   TR_InlineBlock *aBlock = NULL;
   List<TR_InlineBlock> *blocks = partialInline->_inlineBlocks;
   ListIterator<TR_InlineBlock> blocksIt(blocks);
   traceMsg(comp(), "\t\t\tBlocks To Be Inlined:");
   for (aBlock = blocksIt.getFirst() ; aBlock; aBlock = blocksIt.getNext())
      {
      traceMsg(comp()," %d(%d)",aBlock->_originalBlockNum,aBlock->_BCIndex);
      }
   traceMsg(comp(),"\n\t\t\tException Blocks To Be Generated:");

   blocks = partialInline->_exceptionBlocks;
   ListIterator<TR_InlineBlock> eBlocksIt(blocks);
   for (aBlock = eBlocksIt.getFirst() ; aBlock; aBlock = eBlocksIt.getNext())
      {
      traceMsg(comp()," %d(%d)",aBlock->_originalBlockNum,aBlock->_BCIndex);
      }
   traceMsg(comp(),"\n");
   }

const char * TR_InlinerTracer::getGuardKindString(TR_VirtualGuardSelection *guard)
   {
   if (comp()->getDebug())
      return comp()->getDebug()->getVirtualGuardKindName(guard->_kind);
   else
      return "???Guard";
   }

const char * TR_InlinerTracer::getGuardTypeString(TR_VirtualGuardSelection *guard)
   {
   if (comp()->getDebug())
      return comp()->getDebug()->getVirtualGuardTestTypeName(guard->_type);
   else
      return "???Test";
   }

TR_InlinerDelimiter::TR_InlinerDelimiter(TR_InlinerTracer *tracer, char * tag)
   :_tracer(tracer),_tag(tag)
   {
   debugTrace(tracer,"<%s>",_tag);
   }

TR_InlinerDelimiter::~TR_InlinerDelimiter()
   {
   debugTrace(_tracer,"</%s>",_tag);
   }

void TR_InlinerTracer::dumpPrexArgInfo(TR_PrexArgInfo* argInfo)
   {
   if (!argInfo || !heuristicLevel())
      return;

   traceMsg( comp(),  "<argInfo address = %p numArgs = %d>\n", argInfo, argInfo->getNumArgs());
   for (int i = 0 ; i < argInfo->getNumArgs(); i++)

      {
      TR_PrexArgument* arg = argInfo->get(i);
      if (arg && arg->getFixedClass())
         {
         char* className = TR::Compiler->cls.classSignature(comp(), arg->getFixedClass(), trMemory());
         traceMsg( comp(),  "<Argument no=%d address=%p classIsFixed=%d classIsPreexistent=%d class=%p className= %s/>\n",
         i, arg, arg->classIsFixed(), arg->classIsPreexistent(), arg->getFixedClass(), className);
         }
      else
         {
         traceMsg( comp(),  "<Argument no=%d address=%p classIsFixed=%d classIsPreexistent=%d/>\n", i, arg, arg ? arg->classIsFixed() : 0, arg ? arg->classIsPreexistent() : 0);
         }
      }
      traceMsg( comp(),  "</argInfo>\n");
   }

bool
OMR_InlinerPolicy::willBeInlinedInCodeGen(TR::RecognizedMethod method)
   {
   return false;
   }

OMR_InlinerPolicy::OMR_InlinerPolicy(TR::Compilation *comp)
   : TR::OptimizationPolicy (comp)
   {

   }


OMR_InlinerUtil::OMR_InlinerUtil(TR::Compilation *comp)
   : TR::OptimizationUtil (comp)
   {

   }

/**
 * replace the existing guard with a better one of more specific type if possible
 */
void
OMR_InlinerUtil::refineInlineGuard(TR::Node *callNode, TR::Block *&block1, TR::Block *&block2,
                  bool &appendTestToBlock1, TR::ResolvedMethodSymbol * callerSymbol, TR::TreeTop *cursorTree,
                  TR::TreeTop *&virtualGuard, TR::Block *block4)
   {
   }

/**
 * stop inlining for this callsite if TR_CallSite#_initialCalleeSymbol is certain specific method
 */
bool
OMR_InlinerPolicy::supressInliningRecognizedInitialCallee(TR_CallSite* callsite, TR::Compilation* comp)
   {
   return false;
   }


TR_InlinerFailureReason
OMR_InlinerPolicy::checkIfTargetInlineable(TR_CallTarget* target, TR_CallSite* callsite, TR::Compilation* comp)
   {
   return InlineableTarget;
   }

/**
 * Do not perform privated inliner argument rematerialization on high probability profiled
 * guards when distrusted
 *
 * @return true if privatized inliner argumetn rematerialization should be suppressed
 */
bool
OMR_InlinerPolicy::suitableForRemat(TR::Compilation *comp, TR::Node *node, TR_VirtualGuardSelection *guard)
   {
   return true;
   }

void
OMR_InlinerUtil::estimateAndRefineBytecodeSize(TR_CallSite* callsite, TR_CallTarget* target, TR_CallStack *callStack, int32_t &bytecodeSize)
   {
   }

/**
 * collect arguments information for the given target and store into TR_CallTarget::_prexArgInfo
 * the default implementation does nothing
 */
TR_PrexArgInfo *
OMR_InlinerUtil::computePrexInfo(TR_CallTarget *target)
   {
   return NULL;
   }

bool
OMR_InlinerUtil::needTargetedInlining(TR::ResolvedMethodSymbol *callee)
   {
   return false;
   }

void
OMR_InlinerUtil::refineColdness(TR::Node* node, bool& isCold)
   {
   return;
   }

/**
 * reset branch and block frequency of the target callee method
 */
void
OMR_InlinerUtil::computeMethodBranchProfileInfo (TR::Block * cfgBlock, TR_CallTarget* calltarget, TR::ResolvedMethodSymbol* callerSymbol)
   {
   return;
   }

TR_TransformInlinedFunction *
OMR_InlinerUtil::getTransformInlinedFunction(TR::ResolvedMethodSymbol *callerSymbol, TR::ResolvedMethodSymbol *calleeSymbol, TR::Block *blockContainingTheCall, TR::TreeTop *callNodeTreeTop,
                                 TR::Node *callNode, TR_ParameterToArgumentMapper & pam, TR_VirtualGuardSelection *guard, List<TR::SymbolReference> & tempList,
                                 List<TR::SymbolReference> & availableTemps, List<TR::SymbolReference> & availableBasicBlockTemps)
   {
   return new (comp()->trStackMemory()) TR_TransformInlinedFunction(comp(), tracer(), callerSymbol, calleeSymbol, blockContainingTheCall, callNodeTreeTop, callNode, pam, guard, tempList, availableTemps, availableBasicBlockTemps);
   }

/**
 * return how many times this call has been invoked. The data could come from some project specific intrumentation, profiler etc
 */
int32_t
OMR_InlinerUtil::getCallCount(TR::Node *callNode)
   {
   return 0;
   }

TR_InnerPreexistenceInfo*
OMR_InlinerUtil::createInnerPrexInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol, TR_CallStack *callStack,
                                    TR::TreeTop *callTree, TR::Node *callNode,
                                    TR_VirtualGuardKind guardKind)
   {
   return new(comp()->trStackMemory())TR_InnerPreexistenceInfo(c, methodSymbol, callStack, callTree, callNode, guardKind);
   }

bool
TR_InnerPreexistenceInfo::perform(TR::Compilation *comp, TR::Node *guardNode, bool & disableTailRecursion)
   {
   return false;
   }

TR_InnerPreexistenceInfo::TR_InnerPreexistenceInfo(TR::Compilation * c, TR::ResolvedMethodSymbol *methodSymbol,
      TR_CallStack *callStack, TR::TreeTop *treeTop,
      TR::Node *callNode, TR_VirtualGuardKind guardKind)
   : _comp(c), _trMemory(c->trMemory()), _methodSymbol(methodSymbol), _callStack(callStack),
     _callTree(treeTop), _callNode(callNode), _guardKind(guardKind), _assumptions(c->trMemory())
   {
   }

/**
 * Perform any required tree modifications prior to merging with the callee treetops.
 */
void
OMR_InlinerUtil::calleeTreeTopPreMergeActions(TR::ResolvedMethodSymbol *calleeResolvedMethodSymbol, TR_CallTarget* calltarget)
   {
   /* By default, nothing is requred */
   }

bool
OMR_InlinerPolicy::dontPrivatizeArgumentsForRecognizedMethod(TR::RecognizedMethod recognizedMethod)
   {
   return false;
   }

/**
 * return true if the \p calleeMethod is doing a software overflow check which can be replaced by a hardware check
 */
bool
OMR_InlinerPolicy::replaceSoftwareCheckWithHardwareCheck(TR_ResolvedMethod *calleeMethod)
   {
   return false;
   }


TR_InlinerTracer *
OMR_InlinerUtil::getInlinerTracer(TR::Optimization *optimization)
   {
   return new (comp()->trHeapMemory()) TR_InlinerTracer(comp(),fe(),optimization);
   }
