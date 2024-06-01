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

#include "compile/Compilation.hpp"

#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Compilation.hpp"
#include "compile/Compilation_inlines.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/OSRData.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/CompileTimeProfiler.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/KnownObjectTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/TypeLayout.hpp"
#include "env/VerboseLog.hpp"
#include "env/defines.h"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/ILWalk.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Random.hpp"
#include "infra/Stack.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/Timer.hpp"
#include "infra/ThreadLocal.hpp"
#include "optimizer/DebuggingCounters.hpp"
#include "optimizer/Inliner.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/RegisterCandidate.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "ras/ILValidator.hpp"
#include "ras/IlVerifier.hpp"
#include "control/Recompilation.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "ilgen/IlGen.hpp"
#include "env/RegionProfiler.hpp"
#include "omrformatconsts.h"

// this ratio defines how full the alias memory region is allowed to become before
// it is recreated after an optimization finishes
#define ALIAS_REGION_LOAD_FACTOR 0.75

#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "env/CHTable.hpp"
#include "env/VMJ9.h"
#endif


#ifdef J9ZOS390
#include<new>

// Workaround CSECT compile option bug on z/OS
#pragma csect(CODE,"Compilation#C")
#pragma csect(STATIC,"Compilation#S")
#pragma csect(TEST,"Compilation#T")
#endif


class TR_FrontEnd;
class TR_HWPRecord;
class TR_Memory;
class TR_OptimizationPlan;
class TR_PrexArgInfo;
class TR_PseudoRandomNumbersListElement;
class TR_ResolvedMethod;
namespace TR { class IlGenRequest; }
namespace TR { class Options; }
namespace TR { class CodeCache; }
namespace TR { class RegisterMappedSymbol; }


namespace OMR
{
TR_TLS_DEFINE(TR::Compilation *, compilation);
}

TR::SymbolReference *
OMR::Compilation::getSymbolReferenceByReferenceNumber(int32_t referenceNumber)
   {
   return self()->getSymRefTab()->getSymRef(referenceNumber);
   }

ncount_t
OMR::Compilation::getNodeCount()
   {
   _nodeCount =  _compilationNodes->getMaxIndex() + 1;
   return _nodeCount;
   }


// define some variables to keep track of compilation time
TR_SingleTimer compTime;
TR_SingleTimer genILTime;
TR_SingleTimer optTime;
TR_SingleTimer codegenTime;


static const char *pHotnessNames[numHotnessLevels] =
   {
   "no-opt",      // noOpt
   "cold",        // cold
   "warm",        // warm
   "hot",         // hot
   "very-hot",    // veryHot
   "scorching",   // scorching
   "reducedWarm", //
   "unknown",     // unknownHotness
   };

const char *
OMR::Compilation::getHotnessName(TR_Hotness h)
   {
   if (h < minHotness || h >= numHotnessLevels)
      return "unknownHotness";
   return pHotnessNames[h];
   }


OMR::Compilation::Compilation(
      int32_t id,
      OMR_VMThread *omrVMThread,
      TR_FrontEnd *fe,
      TR_ResolvedMethod *compilee,
      TR::IlGenRequest &ilGenRequest,
      TR::Options &options,
      TR::Region &heapMemoryRegion,
      TR_Memory *m,
      TR_OptimizationPlan *optimizationPlan,
      TR::Environment *target) :
   _signature(compilee->signature(m)),
   _options(&options),
   _heapMemoryRegion(heapMemoryRegion),
   _trMemory(m),
   _fe(fe),
   _ilGenRequest(ilGenRequest),
   _currentOptIndex(0),
   _lastBegunOptIndex(0),
   _lastPerformedOptIndex(0),
   _currentOptSubIndex(0), // The transformation index within the current opt
   _lastPerformedOptSubIndex(0),
   _debug(0),
   _knownObjectTable(NULL),
   _omrVMThread(omrVMThread),
   _allocator(TRCS2MemoryAllocator(m)),
   _method(compilee),
   _arenaAllocator(TR::Allocator(self()->allocator("Arena"))),
   _aliasRegion(heapMemoryRegion),
   _ilGenerator(0),
   _ilValidator(NULL),
   _optimizer(0),
   _currentSymRefTab(NULL),
   _recompilationInfo(0),
   _optimizationPlan(optimizationPlan),
   _primaryRandom(NULL),
   _adhocRandom(NULL),
   _methodSymbols(m, 10),
   _resolvedMethodSymbolReferences(m),
   _inlinedCallSites(m),
   _inlinedCallStack(m),
   _inlinedCallArgInfoStack(m),
   _devirtualizedCalls(getTypedAllocator<TR_DevirtualizedCallInfo*>(self()->allocator())),
   _inlinedCalls(0),
   _inlinedFramesAdded(0),
   _virtualGuards(GuardSetCmp(), heapMemoryRegion),
   _staticPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _staticHCRPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _staticMethodPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _genILSyms(getTypedAllocator<TR::ResolvedMethodSymbol*>(self()->allocator())),
   _returnInfo(TR_VoidReturn),
   _visitCount(0),
   _nodeCount(0),
   _accurateNodeCount(0),
   _lastValidNodeCount(0),
   _maxInlineDepth(0),
   _numLivePendingPushSlots(0),
   _numNestingLevels(0),
   _usesPreexistence(options.getOption(TR_ForceUsePreexistence)),
   _loopVersionedWrtAsyncChecks(false),
   _commitedCallSiteInfo(false),
   _containsBigDecimalLoad(false),
   _osrStateIsReliable(true),
   _canAffordOSRControlFlow(true),
   _osrInfrastructureRemoved(false),
   _toNumberMap(self()->allocator("toNumberMap")),
   _toStringMap(self()->allocator("toStringMap")),
   _toCommentMap(self()->allocator("toCommentMap")),
   _nextOptLevel(unknownHotness),
   _errorCode(COMPILATION_SUCCEEDED),
   _peekingArgInfo(m),
   _peekingSymRefTab(NULL),
   _checkcastNullChkInfo(getTypedAllocator<TR_Pair<TR_ByteCodeInfo, TR::Node> *>(self()->allocator())),
   _nodesThatShouldPrefetchOffset(getTypedAllocator<TR_Pair<TR::Node,uint32_t> *>(self()->allocator())),
   _extraPrefetchInfo(getTypedAllocator<TR_PrefetchInfo*>(self()->allocator())),
   _debugCounterMap(std::less<const void *>(), getTypedAllocator<DebugCounterEntry>(self()->allocator())),
   _currentBlock(NULL),
   _verboseOptTransformationCount(0),
   _relocatableMethodCodeStart(NULL),
   _compThreadID(id),
   _failCHtableCommitFlag(false),
   _phaseTimer("Compilation", self()->allocator("phaseTimer"), self()->getOption(TR_Timing)),
   _phaseMemProfiler("Compilation", self()->allocator("phaseMemProfiler"), self()->getOption(TR_LexicalMemProfiler)),
   _compilationNodes(NULL),
   _copyPropagationRematerializationCandidates(self()->allocator("CP rematerialization")),
   _nodeOpCodeLength(0),
   _prevSymRefTabSize(0),
   _scratchSpaceLimit(TR::Options::_scratchSpaceLimit),
   _cpuTimeAtStartOfCompilation(-1),
   _ilVerifier(NULL),
   _gpuPtxList(m),
   _gpuKernelLineNumberList(m),
   _gpuPtxCount(0),
   _bitVectorPool(self()),
   _typeLayoutMap((LayoutComparator()), LayoutAllocator(self()->region())),
   _currentILGenCallTarget(NULL),
   _tlsManager(*self())
   {
   if (target != NULL)
      {
      _target = *target;
      }
   else
      {
      _target = TR::Compiler->target;
      }

   //Avoid expensive initialization and uneeded option checking if we are doing AOT Loads
   if (_optimizationPlan && _optimizationPlan->getIsAotLoad())
      {
#ifdef J9_PROJECT_SPECIFIC
      _transientCHTable = NULL;
      _metadataAssumptionList = NULL;
#endif
      _symRefTab = NULL;
      _methodSymbol = NULL;
      _codeGenerator = NULL;
      _recompilationInfo = NULL;
      _globalRegisterCandidates = NULL;
      _osrCompilationData = NULL;
      return;
      }

#ifdef J9_PROJECT_SPECIFIC
   _transientCHTable = new (_trMemory->trHeapMemory()) TR_CHTable(_trMemory);
   _metadataAssumptionList = NULL;
#endif
   _symRefTab = new (_trMemory->trHeapMemory()) TR::SymbolReferenceTable(_method->maxBytecodeIndex(), self());
   _compilationNodes = new (_trMemory->trHeapMemory()) TR::NodePool(self());

#ifdef J9_PROJECT_SPECIFIC
   // The following must stay before the first assumption gets created which could happen
   // while the PersistentMethodInfo is allocated during codegen creation
   // Access to this list must be performed with assumptionTableMutex in hand
   //
   if (!options.getOption(TR_DisableFastAssumptionReclamation))
      _metadataAssumptionList = new (m->trPersistentMemory()) TR::SentinelRuntimeAssumption();
#endif

   //Random fields must be set before allocating codegen
   _primaryRandom = new (m->trHeapMemory()) TR_RandomGenerator(options.getRandomSeed());
   _adhocRandom = new (m->trHeapMemory()) TR_RandomGenerator(options.getRandomSeed());
   if (options.getOption(TR_RandomSeedSignatureHash))
      {
      uint32_t hash = 0;
      for (const char *c = self()->signature(); *c; c++)
         hash = 33*hash + (uint32_t)(*c);
      uint32_t seed = _options->getRandomSeed();
      seed ^= hash;
      _primaryRandom->setSeed(seed);
      _adhocRandom->setSeed(_primaryRandom->getRandom());
      }

   if (ilGenRequest.details().isMethodInProgress())
      {
      _flags.set(IsDLTCompile);
      options.setAllowRecompilation(false);
      }

   if (optimizationPlan)
      {
      if (optimizationPlan->isGPUCompilation())
          _flags.set(IsGPUCompilation);

      if (optimizationPlan->isGPUCompileCPUCode())
          _flags.set(IsGPUCompileCPUCode);
      }

   // if we are not in the selective NoOptServer mode
   _isOptServer = (!options.getOption(TR_NoOptServer)) &&
         ( options.getOption(TR_Server)
#ifdef J9_PROJECT_SPECIFIC
           || (self()->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold)
#endif
         );
   _isServerInlining = !options.getOption(TR_NoOptServer);

   // TR_DisableInternalPointers must be set before the TR::CodeGenerator object is created because
   // CodeGenerator's _disableInternalPointers member is set in its constructor and this is one of
   // options that is checked for
   if (_isOptServer)
      {
      if(self()->getMethodHotness() <= warm)
         {
            self()->setOption(TR_DisableInternalPointers);
         }
      }

   //_methodSymbol must be done after symRefTab, but before codegen
   // _methodSymbol must be initialized here because creating a jitted method symbol
   //   actually inspects TR::comp()->_methodSymbol (to compare against the new object)
   _methodSymbol = TR::ResolvedMethodSymbol::createJittedMethodSymbol(self()->trHeapMemory(), compilee, self());

   // initPersistentCPUField and createOpCode must be done after method symbol creation

   if (self()->getOption(TR_EnableNodeGC))
      {
      self()->getNodePool().enableNodeGC();
      }

   if (self()->getOption(TR_ForceGenerateReadOnlyCode))
      {
      self()->setGenerateReadOnlyCode();
      }

   //codegen also needs _methodSymbol
   _codeGenerator = TR::CodeGenerator::create(self());

   _recompilationInfo = _codeGenerator->getSupportsRecompilation() ? _codeGenerator->allocateRecompilationInfo() : NULL;

   _globalRegisterCandidates = new (self()->trHeapMemory()) TR::RegisterCandidates(self());

#ifdef J9_PROJECT_SPECIFIC
   if (_recompilationInfo && options.getOptLevelDowngraded())
      _recompilationInfo->getMethodInfo()->setOptLevelDowngraded(true);
#endif

   static bool firstTime = true;
   if (firstTime)
      {
      firstTime = false;
      TR::ILOpCode::checkILOpArrayLengths();
#ifdef J9_PROJECT_SPECIFIC
      TR_ASSERT(TR::DataType::getMaxPackedDecimalSize() == TR::DataType::packedDecimalPrecisionToByteLength(TR_MAX_DECIMAL_PRECISION),
                "TR::DataType::getMaxPackedDecimalSize() (%d) does not agree with precToSize(TR_MAX_DECIMAL_PRECISION) (%d)\n",
                TR::DataType::getMaxPackedDecimalSize(), TR::DataType::packedDecimalPrecisionToByteLength(TR_MAX_DECIMAL_PRECISION));
#endif

      }

   static const char *enableOSRAtAllOptLevels = feGetEnv("TR_EnableOSRAtAllOptLevels");

   // If the outermost method is native, OSR is not supported
   if (_methodSymbol->isNative())
      self()->setOption(TR_DisableOSR);

   // Do not default OSR on if:
   // NextGenHCR is disabled, as it is enabled for it
   // OSR is explicitly disabled
   // FSD is enabled, as HCR cannot be enabled with it
   // HCR has not been enabled
   if (!self()->getOption(TR_DisableNextGenHCR) && !self()->getOption(TR_DisableOSR) && !self()->getOption(TR_FullSpeedDebug) && self()->getOption(TR_EnableHCR))
      {
      self()->setOption(TR_EnableOSR); // OSR must be enabled for NextGenHCR
      }

   if (self()->isDLT() || (((self()->getMethodHotness() < warm) || self()->compileRelocatableCode() || (self()->isProfilingCompilation() && self()->getProfilingMode() != JProfiling)) && !enableOSRAtAllOptLevels && !_options->getOption(TR_FullSpeedDebug)))
      {
      self()->setOption(TR_DisableOSR);
      _options->setOption(TR_EnableOSR, false);
      _options->setOption(TR_EnableOSROnGuardFailure, false);
      }

   if (options.getOption(TR_EnableOSR))
      {
      // Current implementation of partial inlining will break OSR
      self()->setOption(TR_DisablePartialInlining);

      //TODO: investigate the memory footprint of this allocation
      _osrCompilationData = new (self()->trHeapMemory()) TR_OSRCompilationData(self());

      if (((self()->getMethodHotness() < warm) || self()->compileRelocatableCode() || self()->isProfilingCompilation()) && !enableOSRAtAllOptLevels && !options.getOption(TR_FullSpeedDebug)) // Off for two reasons : 1) not sure if we can afford the increase in compile time due to the extra OSR control flow at cold and 2) not sure at this stage in 727 whether OSR can work with AOT (will try to find out soon) but disabling till I do find out
         _canAffordOSRControlFlow = false;
      }
   else
      _osrCompilationData = NULL;
   }

OMR::Compilation::~Compilation() throw()
   {
   }


TR::KnownObjectTable *
OMR::Compilation::getOrCreateKnownObjectTable()
   {
   _knownObjectTable = NULL;
   return _knownObjectTable;
   }


void
OMR::Compilation::freeKnownObjectTable()
   {
   _knownObjectTable = NULL;
   }


int32_t
OMR::Compilation::maxInternalPointers()
   {
   return 0;
   }

bool
OMR::Compilation::isOutermostMethod()
   {
   if ((self()->getInlineDepth() != 0) || self()->isPeekingMethod())
      return false;
   return true;
   }

bool
OMR::Compilation::ilGenTrace()
   {
   if (self()->isOutermostMethod() || self()->getOption(TR_DebugInliner) || self()->trace(OMR::inlining))
      return true;
   return false;
   }

int32_t
OMR::Compilation::getOptLevel()
   {
   return _options->getOptLevel();
   }

bool
OMR::Compilation::allocateAtThisOptLevel()
   {
   return self()->getOptLevel() > warm;
   }

TR::PersistentInfo *
OMR::Compilation::getPersistentInfo()
   {
   return self()->trPersistentMemory()->getPersistentInfo();
   }

int32_t
OMR::Compilation::getMaxAliasIndex()
   {
   return self()->getSymRefCount();
   }

bool
OMR::Compilation::isPeekingMethod()
   {
   return self()->getCurrentSymRefTab() != 0;
   }

TR_Hotness
OMR::Compilation::getMethodHotness()
   {
   return (TR_Hotness) self()->getOptLevel();
   }

ncount_t
OMR::Compilation::getAccurateNodeCount()
   {
   self()->generateAccurateNodeCount();
   return _accurateNodeCount;
   }

const char *
OMR::Compilation::getHotnessName()
   {
   return TR::Compilation::getHotnessName(self()->getMethodHotness());
   }

bool OMR::Compilation::canAffordOSRControlFlow()
   {
   if (self()->getOption(TR_DisableOSR) || !self()->getOption(TR_EnableOSR))
      {
      return false;
      }

   if (self()->osrInfrastructureRemoved())
      {
      return false;
      }

   return _canAffordOSRControlFlow;
   }

void OMR::Compilation::setSeenClassPreventingInducedOSR()
   {
   if (_osrCompilationData)
     _osrCompilationData->setSeenClassPreventingInducedOSR();
   }

bool OMR::Compilation::supportsInduceOSR()
   {
   if (_osrInfrastructureRemoved)
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "OSR induction cannot be performed after OSR infrastructure has been removed\n");
      return false;
      }

   if (!self()->canAffordOSRControlFlow())
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "canAffordOSRControlFlow is false - OSR induction is not supported\n");
      return false;
      }

   if (self()->getOption(TR_MimicInterpreterFrameShape) && !self()->getOption(TR_FullSpeedDebug)/* && areSlotsSharedByRefAndNonRef() */)
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "MimicInterpreterFrameShape is set - OSR induction is not supported\n");
      return false;
      }

   if (self()->isDLT() /* && getJittedMethodSymbol()->sharesStackSlots(self()) */)
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "DLT compilation - OSR induction is not supported\n");
      return false;
      }

   if (_osrCompilationData && _osrCompilationData->seenClassPreventingInducedOSR())
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "Cannot guarantee OSR transfer of control to the interpreter will work for calls preventing induced OSR (e.g. Quad) because of differences in JIT vs interpreter representations\n");
      return false;
      }

   return true;
   }


bool OMR::Compilation::penalizePredsOfOSRCatchBlocksInGRA()
   {
   if (!self()->getOption(TR_EnableOSR))
      return false;

   if (self()->getOption(TR_FullSpeedDebug))
      return false;

   return true;
   }

bool OMR::Compilation::isShortRunningMethod(int32_t callerIndex)
   {
   return false;
   }

bool OMR::Compilation::isPotentialOSRPoint(TR::Node *node, TR::Node **osrPointNode, bool ignoreInfra)
   {
   static char *disableAsyncCheckOSR = feGetEnv("TR_disableAsyncCheckOSR");
   static char *disableGuardedCallOSR = feGetEnv("TR_disableGuardedCallOSR");
   static char *disableMonentOSR = feGetEnv("TR_disableMonentOSR");

   bool potentialOSRPoint = false;
   if (self()->isOSRTransitionTarget(TR::postExecutionOSR))
      {
      if (node->getOpCodeValue() == TR::treetop || node->getOpCode().isCheck())
         node = node->getFirstChild();

      if (_osrInfrastructureRemoved && !ignoreInfra)
         potentialOSRPoint = false;
      else if (node->getOpCodeValue() == TR::asynccheck)
         {
         if (disableAsyncCheckOSR == NULL)
            potentialOSRPoint = !self()->isShortRunningMethod(node->getByteCodeInfo().getCallerIndex());
         }
      else if (node->getOpCode().isCall())
         {
         TR::SymbolReference *callSymRef = node->getSymbolReference();
         if (node->isPotentialOSRPointHelperCall())
            {
            potentialOSRPoint = true;
            }
         else if (callSymRef->getReferenceNumber() >=
             self()->getSymRefTab()->getNonhelperIndex(self()->getSymRefTab()->getLastCommonNonhelperSymbol())
             && !((TR::MethodSymbol*)(callSymRef->getSymbol()))->functionCallDoesNotYieldOSR())
            {
            potentialOSRPoint = (disableGuardedCallOSR == NULL);
            }
         }
      else if (node->getOpCodeValue() == TR::monent)
         potentialOSRPoint = (disableMonentOSR == NULL);
      }
   else if (node->canGCandReturn())
      potentialOSRPoint = true;
   else if (self()->getOSRMode() == TR::involuntaryOSR && node->canGCandExcept())
      potentialOSRPoint = true;

   if (osrPointNode && potentialOSRPoint)
      (*osrPointNode) = node;

   return potentialOSRPoint;
   }

bool OMR::Compilation::isPotentialOSRPointWithSupport(TR::TreeTop *tt)
   {
   TR::Node *osrNode;
   bool potentialOSRPoint = self()->isPotentialOSRPoint(tt->getNode(), &osrNode);

   if (potentialOSRPoint && self()->getOSRMode() == TR::voluntaryOSR)
      {
      if (self()->isOSRTransitionTarget(TR::postExecutionOSR) && tt->getNode() != osrNode)
         {
         // The OSR point applies where the node is anchored, rather than where it may
         // be commoned. Therefore, it is necessary to check if the node is anchored under
         // a prior treetop.
         if (osrNode->getReferenceCount() > 1)
            {
            TR::TreeTop *cursor = tt->getPrevTreeTop();
            while (cursor)
               {
               if ((cursor->getNode()->getOpCode().isCheck() || cursor->getNode()->getOpCodeValue() == TR::treetop)
                   && cursor->getNode()->getFirstChild() == osrNode)
                  {
                  potentialOSRPoint = false;
                  break;
                  }
               if (cursor->getNode()->getOpCodeValue() == TR::BBStart &&
                   !cursor->getNode()->getBlock()->isExtensionOfPreviousBlock())
                  break;
               cursor = cursor->getPrevTreeTop();
               }
            }
         }

      if (potentialOSRPoint)
         {
         TR_ByteCodeInfo &bci = osrNode->getByteCodeInfo();
         TR::ResolvedMethodSymbol *method = bci.getCallerIndex() == -1 ?
            self()->getMethodSymbol() : self()->getInlinedResolvedMethodSymbol(bci.getCallerIndex());
         potentialOSRPoint = method->supportsInduceOSR(bci, tt->getEnclosingBlock(), self(), false);
         }
      }

   return potentialOSRPoint;
   }

/*
 * OSR can operate in two modes, voluntary and involuntary.
 *
 * In involuntary OSR, the JITed code does not control when an OSR transition occurs. It can be
 * initiated externally at any potential OSR point.
 *
 * In voluntary OSR, the JITed code does control when an OSR transition occurs, allowing it to
 * limit the OSR points with transitions.
 */
TR::OSRMode
OMR::Compilation::getOSRMode()
   {
   if (self()->getOption(TR_FullSpeedDebug))
      return TR::involuntaryOSR;
   return TR::voluntaryOSR;
   }

/*
 * The OSR transition destination may be before or after the OSR point.
 * When located before, the transition will target the bytecode index of
 * the OSR point, whilst those located after may have an offset bytecode
 * index.
 */
TR::OSRTransitionTarget
OMR::Compilation::getOSRTransitionTarget()
   {
   TR::OSRTransitionTarget target = TR::disableOSR;

   // Under NextGenHCR, transitions will occur after the OSR points
   // Otherwise, the default is before
   if (self()->getHCRMode() == TR::osr)
      {
      target = TR::postExecutionOSR;
      // If OSROnGuardFailure is enabled, transitions will also occur before
      if (self()->getOption(TR_EnableOSROnGuardFailure))
         target = TR::preAndPostExecutionOSR;
      }
   else if (self()->getOption(TR_EnableOSR))
      target = TR::preExecutionOSR;

   return target;
   }

bool
OMR::Compilation::isOSRTransitionTarget(TR::OSRTransitionTarget target)
   {
   return (target & self()->getOSRTransitionTarget()) != 0;
   }

/*
 * Provides the bytecode offset between the OSR point and the destination
 * of the transition. Only used when doing postExecutionOSR to indicate
 * the appropriate bytecode index after the OSR point.
 */
int32_t
OMR::Compilation::getOSRInductionOffset(TR::Node *node)
   {
   // If no induction after the OSR point, offset must be 0
   if (!self()->isOSRTransitionTarget(TR::postExecutionOSR))
      return 0;

   TR::Node *osrNode;
   if (!self()->isPotentialOSRPoint(node, &osrNode))
      {
      TR_ASSERT(0, "getOSRInductionOffset should only be called on OSR points");
      }

   if (osrNode->isPotentialOSRPointHelperCall())
      {
      return osrNode->getOSRInductionOffset();
      }

   if (osrNode->getOpCode().isCall())
      return 3;

   // If the monent has a bytecode index of 0, it must be the
   // monent for a synchronized method. A transition here
   // must target the method entry.
   if (osrNode->getOpCodeValue() == TR::monent)
      {
      if (osrNode->getByteCodeIndex() == 0)
         return 0;
      else
         return 1;
      }

   if (osrNode->getOpCodeValue() == TR::asynccheck)
      return 0;

   TR_ASSERT(0, "OSR points should only be calls, monents or asyncchecks");
   return 0;
   }

/*
 * An OSR analysis point is used only for OSRDefAnalysis
 * and will not become a transition point. It is required
 * for certain OSR points when doing postExecutionOSR.
 * For example, liveness data must be know before an inlined
 * call, to reconstruct the caller's frame, but the transition
 * can only occur after the call.
 *
 * An analysis point does not have an induction offset.
 */
bool
OMR::Compilation::requiresAnalysisOSRPoint(TR::Node *node)
   {
   // If no induction after the OSR point, cannot use analysis point
   if (!self()->isOSRTransitionTarget(TR::postExecutionOSR))
      return false;

   TR::Node *osrNode;
   if (!self()->isPotentialOSRPoint(node, &osrNode))
      {
      TR_ASSERT(0, "requiresAnalysisOSRPoint should only be called on OSR points\n");
      return false;
      }

   // Calls require an analysis and transition point as liveness may change across them
   if (osrNode->getOpCode().isCall())
      return true;

   switch (osrNode->getOpCodeValue())
      {
      // Monents only require a trailing OSR point as they will perform OSR when executing the
      // monitor and there is no change in liveness due to the monent
      case TR::monent:
      // Asyncchecks will not modify liveness
      case TR::asynccheck:
         return false;
      default:
         TR_ASSERT(0, "OSR points should only be calls, monents or asyncchecks");
         return false;
      }
   }

/**
 * To reduce OSR overhead, OSRLiveRangeAnalysis supports solving liveness analysis
 * during IlGen, as an approximation of this information is commonly known at this
 * stage.
 *
 * If this function returns true, OSRLiveRangeAnalysis expects pending push liveness
 * information to be stashed in OSRData using addPendingPushLivenessInfo prior to
 * its execution in IlGenOpts. Returning true without the correct information stashed
 * will result in reduced performance and failed OSR transitions.
 *
 * If this function returns false, OSRLiveRangeAnalysis will perform liveness analysis
 * on pending push symbols.
 */
bool
OMR::Compilation::pendingPushLivenessDuringIlgen()
   {
   return false;
   }

/**
 * A profiling compilation will include instrumentation to collect information
 * on block and value frequencies. isProfilingCompilation() should return true for
 * such a compilation and getProfilingMode() can distinguish between the profiling
 * implementations.
 */
bool
OMR::Compilation::isProfilingCompilation()
   {
   return _recompilationInfo ? _recompilationInfo->isProfilingCompilation() : false;
   }

ProfilingMode
OMR::Compilation::getProfilingMode()
   {
   if (!self()->isProfilingCompilation())
      return DisabledProfiling;

   if (self()->getOption(TR_EnableJProfiling) || !self()->getOption(TR_DisableJProfilingInProfilingCompilations))
      return JProfiling;

   return JitProfiling;
   }

bool
OMR::Compilation::isJProfilingCompilation()
   {
   return false;
   }

#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS)
static void stopBeforeCompile()
   {
   static int first = 1;
   // only print the following message once
   if (first)
      {
      printf("stopBeforeCompile is a dummy routine.\n");
      first = 0;
      }
   }
#endif /* defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS) */

static int32_t strHash(const char *str)
   {
   // The string hash from Java
   int32_t result = 0;
   for (const unsigned char *s = reinterpret_cast<const unsigned char*>(str); *s; s++)
      result = result * 33 + *s;
   return result;
   }

int32_t OMR::Compilation::compile()
   {

   if (!self()->getOption(TR_DisableSupportForCpuSpentInCompilation))
      _cpuTimeAtStartOfCompilation = TR::Compiler->vm.cpuTimeSpentInCompilationThread(self());

   bool printCodegenTime = self()->getOption(TR_CummTiming);

   if (self()->isOptServer())
      self()->setOption(TR_DisablePartialInlining);

#ifdef J9_PROJECT_SPECIFIC
   if (self()->getOptions()->getDelayCompileWithCPUBurn())
      {
      uint64_t limit = (uint64_t)self()->getOptions()->getDelayCompileWithCPUBurn();
      uint64_t starttime = self()->getPersistentInfo()->getElapsedTime();
      if (self()->getOptions()->getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "DelayCompileWithCPUBurn: Starting a delay of length %" OMR_PRIu64 " for method %s at time %" OMR_PRIu64 ".", limit, self()->signature(), starttime);
      uint64_t temp=0;
      while(1)
         {
         temp = self()->getPersistentInfo()->getElapsedTime();
         if( ( temp - starttime ) > limit)
            {
            if (self()->getOptions()->getVerboseOption(TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "DelayCompileWithCPUBurn: Finished delay at time = %" OMR_PRIu64 ", elapsed time = %" OMR_PRIu64 ".", temp, (temp - starttime));
            break;
            }
         }
      }

   self()->setGetImplInlineable(self()->fej9()->isGetImplInliningSupported());
#endif

   if (self()->getOption(TR_BreakBeforeCompile))
      {
      fprintf(stderr, "\n=== About to compile %s ===\n", self()->signature());
      TR::Compiler->debug.breakPoint();
      }

#if defined(AIXPPC)
   if (self()->getOption(TR_DebugBeforeCompile))
      {
      self()->getDebug()->setupDebugger((void *) *((long*)&(stopBeforeCompile)));
      stopBeforeCompile();
      }
#elif defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS)
   if (self()->getOption(TR_DebugBeforeCompile))
      {
#if defined(LINUXPPC64)
      self()->getDebug()->setupDebugger((void *) *((long*)&(stopBeforeCompile)),(void *) *((long*)&(stopBeforeCompile)), true);
#else
      self()->getDebug()->setupDebugger((void *) &stopBeforeCompile,(void *) &stopBeforeCompile,true);
#endif /* defined(LINUXPPC64) */
      stopBeforeCompile();
      }
#endif /* defined(AIXPPC) */

   if (self()->getOutFile() != NULL && (self()->getOption(TR_TraceAll) || debug("traceStartCompile") || self()->getOption(TR_Timing)))
      {
      self()->getDebug()->printHeader();
      static char *randomExercisePeriodStr = feGetEnv("TR_randomExercisePeriod");
      if (self()->getOption(TR_Randomize) || randomExercisePeriodStr != NULL)
         traceMsg(self(), "Random seed is %d%s\n", _options->getRandomSeed(), self()->getOption(TR_RandomSeedSignatureHash)? " hashed with signature":"");
      if (randomExercisePeriodStr != NULL)
         {
         auto period = atoi(randomExercisePeriodStr);
         if (period > 0)
            TR_RandomGenerator::exercise(period, self());
         }
      }

   if (printCodegenTime) compTime.startTiming(self());
   if (_recompilationInfo)
      _recompilationInfo->startOfCompilation();

   // Create the compile time profiler
   TR::CompileTimeProfiler perf(self(), "compileTimePerf");

   {
     TR::RegionProfiler rpIlgen(self()->trMemory()->heapMemoryRegion(), *self(), "comp/ilgen");
     if (printCodegenTime) genILTime.startTiming(self());
     _ilGenSuccess = _methodSymbol->genIL(self()->fe(), self(), self()->getSymRefTab(), _ilGenRequest);
     if (printCodegenTime) genILTime.stopTiming(self());
   }

   // Force a crash during compilation if the crashDuringCompile option is set
   TR_ASSERT_FATAL(!self()->getOption(TR_CrashDuringCompilation), "crashDuringCompile option is set");

   {
   LexicalTimer t("compile", self()->signature(), self()->phaseTimer());
   TR::LexicalMemProfiler mp("compile", self()->signature(), self()->phaseMemProfiler());

   if (_ilGenSuccess)
      {
      _methodSymbol->detectInternalCycles();

      //detect catch blocks that could have normal predecessors
      //if so, fail the compile
      //
      if (_methodSymbol->catchBlocksHaveRealPredecessors())
         {
         self()->failCompilation<TR::CompilationException>("Catch blocks have real predecessors");
         }

      if ((debug("dumpInitialTrees") || self()->getOption(TR_TraceTrees)) && self()->getOutFile() != NULL)
         {
         self()->dumpMethodTrees("Initial Trees");
         self()->getDebug()->print(self()->getOutFile(), self()->getSymRefTab());
         }
#if !defined(DISABLE_CFG_CHECK)
      if (self()->getOption(TR_UseILValidator))
         {
         self()->validateIL(TR::postILgenValidation);
         }
      else
         {
         self()->verifyTrees (_methodSymbol);
         self()->verifyBlocks(_methodSymbol);
         }
#endif

      if (_recompilationInfo)
         {
         _recompilationInfo->beforeOptimization();
         }
      else if (self()->getOptLevel() == -1)
         {
         TR_ASSERT(false, "we must know an opt level at this stage");
         }

      if (self()->getOutFile() != NULL && (self()->getOption(TR_TraceAll) || debug("traceStartCompile")))
         self()->getDebug()->printMethodHotness();

      TR_DebuggingCounters::initializeCompilation();
      if (printCodegenTime) optTime.startTiming(self());

         {
         TR::RegionProfiler rpOpt(self()->trMemory()->heapMemoryRegion(), *self(), "comp/opt");
         self()->performOptimizations();
         }

      if (printCodegenTime) optTime.stopTiming(self());

#ifdef J9_PROJECT_SPECIFIC
      if (self()->useCompressedPointers())
         {
         if (self()->verifyCompressedRefsAnchors(true))
            dumpOptDetails(self(), "successfully verified compressedRefs anchors\n");
         else
            dumpOptDetails(self(), "failed while verifying compressedRefs anchors\n");
         }
#endif

#if !defined(DEBUG) && !defined(PROD_WITH_ASSUMES)
      if (self()->incompleteOptimizerSupportForReadWriteBarriers())
#endif
         self()->verifyAndFixRdbarAnchors();

#if !defined(DISABLE_CFG_CHECK)
      if (self()->getOption(TR_UseILValidator))
         {
         self()->validateIL(TR::preCodegenValidation);
         }
#endif

      if (_ilVerifier && _ilVerifier->verify(_methodSymbol))
         {
         self()->failCompilation<TR::CompilationException>("Aborting after Optimization due to verifier failure");
         }

      static char *abortafterilgen = feGetEnv("TR_TOSS_IL");
      if(abortafterilgen)
         {
         self()->failCompilation<TR::CompilationException>("Aborting after IL Gen due to TR_TOSS_IL");
         }

      if (_recompilationInfo)
         _recompilationInfo->beforeCodeGen();

        {
        TR::RegionProfiler rpCodegen(self()->trMemory()->heapMemoryRegion(), *self(), "comp/codegen");

        if (printCodegenTime)
           codegenTime.startTiming(self());

        self()->cg()->generateCode();

        if (printCodegenTime)
           codegenTime.stopTiming(self());
        }

      if (_recompilationInfo)
         _recompilationInfo->endOfCompilation();

#ifdef J9_PROJECT_SPECIFIC
      if (self()->getOptions()->getVerboseOption(TR_VerboseInlining))
         {
         int32_t jittedBodyHash = strHash(self()->signature());
         char callerBuf[501], calleeBuf[501];
         TR_VerboseLog::CriticalSection vlogLock;
         TR_VerboseLog::writeLine(TR_Vlog_INL, "%d methods inlined into %x %s @ %p", self()->getNumInlinedCallSites(), jittedBodyHash, self()->signature(), self()->cg()->getCodeStart());
         for (int32_t i = 0; i < self()->getNumInlinedCallSites(); i++)
            {
            TR_InlinedCallSite &site = self()->getInlinedCallSite(i);
            int32_t calleeLen = self()->fej9()->printTruncatedSignature(calleeBuf, sizeof(calleeBuf)-1, self()->fej9()->getInlinedCallSiteMethod(&site));
            calleeBuf[calleeLen] = 0; // null terminate
            const char *callerSig = self()->signature();
            int16_t callerIndex = site._byteCodeInfo.getCallerIndex();
            if (callerIndex != -1)
               {
               int32_t callerLen = self()->fej9()->printTruncatedSignature(callerBuf, sizeof(callerBuf)-1, self()->fej9()->getInlinedCallSiteMethod(&self()->getInlinedCallSite(callerIndex)));
               callerBuf[callerLen] = 0; // null terminate
               callerSig = callerBuf;
               }
            TR_VerboseLog::writeLine(TR_Vlog_INL, "#%d: %x #%d inlined %x@%d -> %x bcsz=%d %s",
               i, jittedBodyHash, callerIndex,
               strHash(callerSig), site._byteCodeInfo.getByteCodeIndex(),
               strHash(calleeBuf), TR::Compiler->mtd.bytecodeSize(site._methodInfo), calleeBuf);
            }

         TR::Block *curBlock = NULL;
         for (TR::TreeTop *tt = self()->getStartTree(); tt; tt = tt->getNextTreeTop())
            {
            if (tt->getNode()->getOpCodeValue() == TR::BBStart)
               {
               curBlock = tt->getNode()->getBlock();
               }
            else if (tt->getNode()->getNumChildren() >= 1)
               {
               TR::Node*node = tt->getNode()->getFirstChild();
               if (node->getOpCode().isCall())
                  {
                  TR::Method *method = node->getSymbol()->getMethodSymbol()->getMethod();
                  if (method)
                     {
                     TR_ByteCodeInfo &bcInfo = node->getByteCodeInfo();
                     const char *callerSig = self()->signature();
                     int16_t callerIndex = bcInfo.getCallerIndex();
                     if (callerIndex != -1)
                        {
                        int32_t callerLen = self()->fej9()->printTruncatedSignature(callerBuf, sizeof(callerBuf)-1, self()->fej9()->getInlinedCallSiteMethod(&self()->getInlinedCallSite(callerIndex)));
                        callerBuf[callerLen] = 0; // null terminate
                        callerSig = callerBuf;
                        }
                     const char *calleeSig = method->signature(self()->trMemory(), stackAlloc);
                     uint32_t calleeSize = UINT_MAX;
                     if (node->getSymbol()->getResolvedMethodSymbol())
                        calleeSize = TR::Compiler->mtd.bytecodeSize(node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier());
                     TR_VerboseLog::writeLine(TR_Vlog_INL, "%x %s %x@%d -> %x bcsz=%d %s",
                        jittedBodyHash,
                        curBlock->isCold()? "coldCalled":"called",
                        strHash(callerSig), bcInfo.getByteCodeIndex(),
                        strHash(calleeSig), calleeSize, calleeSig);
                     }
                  }
               }
            }
         }
#endif
      }

   if (printCodegenTime) compTime.stopTiming(self());

   // Flush the log
   //
   if (self()->getOutFile() != NULL && self()->getOption(TR_TraceAll))
      trfflush(self()->getOutFile());


   }
   if (self()->getOption(TR_Timing))
      {
      self()->phaseTimer().DumpSummary(*self());
      }

   if (self()->getOption(TR_LexicalMemProfiler))
      {
      self()->phaseMemProfiler().DumpSummary(*self());
      }

   // Even though there are unimplemented opcodes, we may have allowed code
   // generation to continue (under control of an option).
   // If that happened we want to fail the compilation at this point.
   //
   if (_methodSymbol->unimplementedOpcode())
      self()->failCompilation<TR::UnimplementedOpCode>("Unimplemented Op Code");

   if (!_ilGenSuccess)
      self()->failCompilation<TR::ILGenFailure>("IL Gen Failure");

#ifdef J9_PROJECT_SPECIFIC
   if (self()->getOption(TR_TraceCG))
      {
      TR_CHTable * chTable = self()->getCHTable();
      if (chTable)
         self()->getDebug()->dump(self()->getOutFile(), chTable);
      }
#endif /* ifdef(J9_PROJECT_SPECIFIC) */

#if defined(AIXPPC) || defined(LINUXPPC)
   if (self()->getOption(TR_DebugOnEntry))
      {
      intptr_t jitToJitStart = self()->cg()->getLinkage()->entryPointFromCompiledMethod();
#if defined(AIXPPC)
      self()->getDebug()->setupDebugger((void *)jitToJitStart);
#else
      self()->getDebug()->setupDebugger((void *)jitToJitStart, self()->cg()->getCodeEnd(), false);
#endif /* defined(AIXPPC) */
      }
#elif defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS)
   if (self()->getOption(TR_DebugOnEntry))
      {
      self()->getDebug()->setupDebugger(self()->cg()->getCodeStart(),self()->cg()->getCodeEnd(),false);
      }
#endif /* defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS) */

   return COMPILATION_SUCCEEDED;
   }

int64_t OMR::Compilation::getCpuTimeSpentInCompilation()
   {
   if (_cpuTimeAtStartOfCompilation >= 0) // negative values means no support for compCPU
      {
      int64_t cpuTimeSpentInCompThread = TR::Compiler->vm.cpuTimeSpentInCompilationThread(self());
      return cpuTimeSpentInCompThread >= 0 ? (cpuTimeSpentInCompThread - _cpuTimeAtStartOfCompilation) : -1;
      }
   return -1;
   }

TR_YesNoMaybe OMR::Compilation::isCpuExpensiveCompilation(int64_t threshold)
   {
   int64_t t = self()->getCpuTimeSpentInCompilation();
   return t < 0 ? TR_maybe : (t > threshold ? TR_yes : TR_no);
   }

void OMR::Compilation::performOptimizations()
   {

   _optimizer = TR::Optimizer::createOptimizer(self(), self()->getJittedMethodSymbol(), false);

   if (_optimizer)
      _optimizer->optimize();
   }

bool OMR::Compilation::incInlineDepth(TR::ResolvedMethodSymbol * method, TR::Node *callNode, bool directCall, TR_VirtualGuardSelection *guard, TR_OpaqueClassBlock *receiverClass, TR_PrexArgInfo *argInfo)
   {
   TR::SymbolReference *callSymRef = callNode->getSymbolReference();
   int32_t cpIndex = callSymRef->getCPIndex();
   TR_ByteCodeInfo &bcInfo = callNode->getByteCodeInfo();

   TR_AOTMethodInfo *aotMethodInfo = NULL;
   if (self()->compileRelocatableCode())
      {
      aotMethodInfo = reinterpret_cast<TR_AOTMethodInfo *>(self()->trMemory()->allocateHeapMemory(sizeof(TR_AOTMethodInfo)));
      aotMethodInfo->resolvedMethod = method->getResolvedMethod();
      aotMethodInfo->cpIndex = cpIndex;
      aotMethodInfo->receiver = receiverClass;
      aotMethodInfo->callSymRef = callSymRef;
      aotMethodInfo->reloKind = self()->getReloTypeForMethodToBeInlined(guard, callNode, receiverClass);
      }
   return self()->incInlineDepth(method->getResolvedMethod()->getPersistentIdentifier(), method, bcInfo, callSymRef, directCall, argInfo, aotMethodInfo);
   }

bool OMR::Compilation::incInlineDepth(TR::ResolvedMethodSymbol * method, TR_ByteCodeInfo & bcInfo, int32_t cpIndex, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo)
   {
   TR_AOTMethodInfo *aotMethodInfo = NULL;
   if (self()->compileRelocatableCode())
      {
      aotMethodInfo = reinterpret_cast<TR_AOTMethodInfo *>(self()->trMemory()->allocateHeapMemory(sizeof(TR_AOTMethodInfo)));
      aotMethodInfo->resolvedMethod = method->getResolvedMethod();
      aotMethodInfo->cpIndex = cpIndex;
      }
   return self()->incInlineDepth(method->getResolvedMethod()->getPersistentIdentifier(), method, bcInfo, callSymRef, directCall, argInfo, aotMethodInfo);
   }

ncount_t OMR::Compilation::generateAccurateNodeCount()
   {
   if( _lastValidNodeCount != _nodeCount)
      _accurateNodeCount = self()->getMethodSymbol()->generateAccurateNodeCount();
    _lastValidNodeCount = _nodeCount;

    return _accurateNodeCount;
   }

// return true if found at least occurances # of methods on the call stack
bool OMR::Compilation::foundOnTheStack(TR_ResolvedMethod *method, int32_t occurrences)
  {

  if(_inlinedCallStack.isEmpty()) return false;

  const bool debug                   = false;
  int32_t counter                    = 0;
  int32_t topIndex                   = _inlinedCallStack.topIndex();
  TR_OpaqueMethodBlock *methodInfo   = method->getPersistentIdentifier();

  for(int32_t i=topIndex;i >= 0; --i)
    {
    int32_t stackEl = _inlinedCallStack.element(i);
    TR_InlinedCallSite & el = self()->getInlinedCallSite(stackEl);
    bool matches = (methodInfo == self()->fe()->getInlinedCallSiteMethod(&el));
    if(matches && ++counter == occurrences)
      return true;
    }

  return false;
  }

bool OMR::Compilation::incInlineDepth(TR_OpaqueMethodBlock *methodInfo, TR::ResolvedMethodSymbol * method, TR_ByteCodeInfo & bcInfo, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo, TR_AOTMethodInfo *aotMethodInfo)
   {
   int32_t maxCallerIndex = TR_ByteCodeInfo::maxCallerIndex;
   //This restriction is due to a limited number of bits allocated to callerIndex in TR_ByteCodeInfo
   //For example, in Java TR_ByteCodeInfo::maxCallerIndex is set to 4095 (12 bits and one used for signness)
   if (self()->getNumInlinedCallSites() >= unsigned(maxCallerIndex))
      {
      traceMsg(self(), "The maximum number of inlined methods %d is reached\n", TR_ByteCodeInfo::maxCallerIndex);
      return false;
      }

   //in jsr292 "smart" inliner can run multiple times. we need to reconstruct a callstack
   //for all the callsites with the caller indices != -1 to make sure each catch block
   //can be uniquely identified by a (depthIndex, handlerIndex) pair.
   //otherwise JVM might print out an incorrect stacktrace if an exception is thrown
   int16_t inlinedFramesAdded = self()->adjustInlineDepth(bcInfo);

   if ( inlinedFramesAdded != 0 )
      {
      TR_ASSERT(_inlinedFramesAdded == 0 && self()->getInlineDepth() == inlinedFramesAdded, "We can only perform the above inline stack correction on an empty stack");
      _inlinedFramesAdded = inlinedFramesAdded;
      }


   uint32_t callSiteIndex = _inlinedCallSites.add( TR_InlinedCallSiteInfo(methodInfo, bcInfo, method, callSymRef, directCall, aotMethodInfo) );
   _inlinedCallStack.push(callSiteIndex);
   _inlinedCallArgInfoStack.push(argInfo);

   int16_t inlinedCallStackSize = self()->getInlineDepth();

   if (inlinedCallStackSize >= TR_ByteCodeInfo::maxCallerIndex)
      {
      TR_ASSERT(0, "max number of inlined calls exceeded");
      self()->failCompilation<TR::ExcessiveComplexity>("max number of inlined calls exceeded");
      }

   if (inlinedCallStackSize > _maxInlineDepth)
      {
      _maxInlineDepth = inlinedCallStackSize;
      }

   return true;
   }

void OMR::Compilation::decInlineDepth(bool removeInlinedCallSitesEntry)
   {
   if (removeInlinedCallSitesEntry)
      {
      while (unsigned(self()->getCurrentInlinedSiteIndex()) < _inlinedCallSites.size())
         _inlinedCallSites.remove(self()->getCurrentInlinedSiteIndex());
      if (self()->getOption(TR_EnableOSR))
         {
         //OSR's method data array needs to reflect the changes made to inline table
         self()->getOSRCompilationData()->setOSRMethodDataArraySize(self()->getNumInlinedCallSites()+1);
         }
      }
   _inlinedCallArgInfoStack.pop();
   _inlinedCallStack.pop();

   if ( self()->getInlineDepth() == _inlinedFramesAdded )
      {
      self()->resetInlineDepth();
      }
   }


int16_t OMR::Compilation::matchingCallStackPrefixLength(TR_ByteCodeInfo &bcInfo)
   {
   if (bcInfo.getCallerIndex() == -1)
      return 0;

   int16_t          callerIndex        = bcInfo.getCallerIndex();
   TR_ByteCodeInfo &callerBCInfo       = self()->getInlinedCallSite(callerIndex)._byteCodeInfo;
   int16_t          callerPrefixLength = self()->matchingCallStackPrefixLength(callerBCInfo);

   int16_t result = callerPrefixLength;
   if (callerPrefixLength < self()->getInlineDepth())
      {
      int32_t nextCallOnStack = self()->getInlinedCallStack().element(callerPrefixLength);
      if (nextCallOnStack == callerIndex)
         result++;
      }

   TR_ASSERT(result <= self()->getInlineDepth(), "matchingCallStackPrefixLength can be no greater than getInlineDepth");
   return result;
   }

int16_t OMR::Compilation::restoreInlineDepthUntil(int32_t stopIndex, TR_ByteCodeInfo &currentInfo)
   {
   if (currentInfo.getCallerIndex() == -1)
      return 0; // Resulting call stack is empty; nothing to do
   else if (stopIndex == currentInfo.getCallerIndex())
      return 0;

   int16_t callerIndex = currentInfo.getCallerIndex();
   int16_t framesAdded = self()->restoreInlineDepthUntil(stopIndex, self()->getInlinedCallSite(callerIndex)._byteCodeInfo);
   _inlinedCallStack.push(callerIndex);
   _inlinedCallArgInfoStack.push(NULL); // We have no argInfo for previously-inlined frames, though there's not much we could do with it anyway

   return framesAdded + 1;
   }

int16_t OMR::Compilation::restoreInlineDepth(TR_ByteCodeInfo &existingInfo)
   {
   TR_ASSERT((int32_t)existingInfo.getCallerIndex() <= (int32_t)self()->getNumInlinedCallSites(), "restoreInlineDepth requires valid caller index");
   int16_t numStackEntriesToKeep = self()->matchingCallStackPrefixLength(existingInfo);

   int16_t inlineFrameChange = 0;
   while (self()->getInlineDepth() > numStackEntriesToKeep)
      {
      self()->decInlineDepth();
      inlineFrameChange--;
      }
   inlineFrameChange += self()->restoreInlineDepthUntil(self()->getCurrentInlinedSiteIndex(), existingInfo);
   return inlineFrameChange;
   }

int16_t OMR::Compilation::adjustInlineDepth(TR_ByteCodeInfo & bcInfo)
   {
   if (bcInfo.getCallerIndex() == -1)
      return 0;
   else
      return self()->restoreInlineDepth(bcInfo);
   }

void OMR::Compilation::resetInlineDepth()
   {
   while (self()->getInlineDepth() > 0)
      self()->decInlineDepth();

   _inlinedFramesAdded = 0;
   }

int32_t OMR::Compilation::convertNonDeterministicInput(int32_t i, int32_t max, TR_RandomGenerator *randomGenerator, int32_t min, bool emitVerbose)
   {
   int32_t answer = i;
   TR_PseudoRandomNumbersListElement *pseudoRandomList = self()->getPersistentInfo()->getPseudoRandomNumbersList();
   if (pseudoRandomList && self()->getOption(TR_VerbosePseudoRandom))
      {
      answer = self()->getPersistentInfo()->getNextPseudoRandomNumber(i);
      }
   else
      {
      if (self()->getOption(TR_Randomize))
         {
         if (!randomGenerator)
            randomGenerator = _adhocRandom;

         answer = randomGenerator->getRandom(min, max);
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   if (emitVerbose && self()->getOption(TR_VerbosePseudoRandom))
      {
      self()->fej9()->emitNewPseudoRandomNumberVerboseLine(answer);
      }
#endif

   return answer;
   }



TR_DevirtualizedCallInfo * OMR::Compilation::getFirstDevirtualizedCall()
   {
   if (!_devirtualizedCalls.empty())
      return _devirtualizedCalls.front();
   return NULL;
   }

TR_DevirtualizedCallInfo * OMR::Compilation::createDevirtualizedCall(TR::Node* callNode, TR_OpaqueClassBlock *thisType)
   {
   TR_DevirtualizedCallInfo * dc = (TR_DevirtualizedCallInfo *) self()->trMemory()->allocateHeapMemory(sizeof(TR_DevirtualizedCallInfo));
   dc->_callNode = callNode;
   dc->_thisType = thisType;
   _devirtualizedCalls.push_front(dc);
   return dc;
   }

TR_DevirtualizedCallInfo * OMR::Compilation::findDevirtualizedCall(TR::Node* callNode)
   {
   auto currDc = _devirtualizedCalls.begin(); //getFirstDevirtualizedCall();
   while (currDc != _devirtualizedCalls.end())
      {
      if ((*currDc)->_callNode == callNode)
         return (*currDc);
      ++currDc;
      }
   return NULL;
   }


TR_DevirtualizedCallInfo * OMR::Compilation::findOrCreateDevirtualizedCall(TR::Node* callNode, TR_OpaqueClassBlock *thisType)
   {
   TR_DevirtualizedCallInfo * dc = self()->findDevirtualizedCall(callNode);
   if (!dc || self()->fe()->isInstanceOf(thisType, dc->_thisType, false) == TR_yes)
      {
      if (dc)
         dc->_thisType = thisType;
      else
    {
    TR_DevirtualizedCallInfo * newDc = self()->createDevirtualizedCall(callNode, thisType);
    dc = newDc;
    }
      }
   return dc;
   }


void
OMR::Compilation::addVirtualGuard(TR_VirtualGuard *guard)
   {
   TR::Node *node = guard->getGuardNode();
   bool ok = _virtualGuards.insert(guard).second;
   TR_ASSERT_FATAL_WITH_NODE(node, ok, "failed to insert guard %p", guard);
   }

TR_VirtualGuard *
OMR::Compilation::findVirtualGuardInfo(TR::Node *guardNode)
   {
   TR_VirtualGuard *guard = guardNode->virtualGuardInfo();
   TR_ASSERT_FATAL_WITH_NODE(guardNode, guard != NULL, "missing guard info");
   return guard;
   }

void
OMR::Compilation::removeVirtualGuard(TR_VirtualGuard *guard)
   {
   if (self()->getOption(TR_TraceRelocatableDataDetailsCG))
      {
      traceMsg(
         self(),
         "removeVirtualGuard %p, kind %d bcindex %d calleeindex %d\n",
         guard,
         guard->getKind(),
         guard->getByteCodeIndex(),
         guard->getCalleeIndex());
      }

   bool wasPresent = _virtualGuards.erase(guard) != 0;
   TR_ASSERT_FATAL_WITH_NODE(guard->getGuardNode(), wasPresent, "missing guard");
   }


TR::Node *
OMR::Compilation::createSideEffectGuard(TR::Compilation * comp, TR::Node * node, TR::TreeTop * destination)
   {
   return TR_VirtualGuard::createSideEffectGuard(comp, node, destination);
   }

TR::Node *
OMR::Compilation::createAOTInliningGuard(TR::Compilation * comp, int16_t calleeIndex, TR::Node * node, TR::TreeTop * destination, TR_VirtualGuardKind kind)
   {
   return TR_VirtualGuard::createAOTInliningGuard(comp, calleeIndex, node, destination, kind);
   }

TR::Node *
OMR::Compilation::createAOTGuard(TR::Compilation * comp, int16_t calleeIndex, TR::Node * node, TR::TreeTop * destination, TR_VirtualGuardKind kind)
   {
   return TR_VirtualGuard::createAOTGuard(comp, calleeIndex, node, destination, kind);
   }

TR::Node *
OMR::Compilation::createDummyGuard(TR::Compilation * comp, int16_t calleeIndex, TR::Node * node, TR::TreeTop * destination)
   {
   return TR_VirtualGuard::createDummyGuard(comp, calleeIndex, node, destination);
   }


bool OMR::Compilation::generateArraylets()
   {
   if (TR::Compiler->om.canGenerateArraylets())
      {
      return TR::Compiler->om.useHybridArraylets() ? false : true;
      }
   else
      {
      return false;
      }
   }


// Are spine checks for array element accesses required in this compilation unit.
//
bool
OMR::Compilation::requiresSpineChecks()
   {
   return TR::Compiler->om.useHybridArraylets();
   }

// used only for compressed pointers
//
bool OMR::Compilation::useCompressedPointers()
   {
   return false;
   }

bool OMR::Compilation::useAnchors()
   {
   return false;
   }



int32_t OMR::Compilation::findPrefetchInfo(TR::Node * node)
   {
   for (auto pair = self()->getNodesThatShouldPrefetchOffset().begin(); pair !=self()->getNodesThatShouldPrefetchOffset().end(); ++pair)
      {
      if ((*pair)->getKey() == node)
         {
         uintptr_t value = (uintptr_t)(*pair)->getValue();
         return (int32_t)value;
         }
      }
   return -1;
   }

TR_PrefetchInfo *
OMR::Compilation::findExtraPrefetchInfo(TR::Node * node, bool use)
   {
   for (auto it = self()->getExtraPrefetchInfo().begin(); it != self()->getExtraPrefetchInfo().end(); ++it)
      {
      if (use)
         {
         if ((*it)->_useNode == node)
            return *it;
         }
      else
         {
         if ((*it)->_addrNode == node)
            return *it;
         }
      }

   return NULL;
   }


bool OMR::Compilation::canTransformUnsafeCopyToArrayCopy()
   {
   if (!self()->getOptions()->realTimeGC() &&
       !TR::Compiler->om.canGenerateArraylets() &&
       !self()->getOption(TR_DisableUnsafe) &&
       self()->cg()->canTransformUnsafeCopyToArrayCopy())
      return true;

   return false;
   }

bool OMR::Compilation::canTransformUnsafeSetMemory()
   {
   if (!self()->getOptions()->realTimeGC() &&
       !TR::Compiler->om.canGenerateArraylets() &&
       !self()->getOption(TR_DisableUnsafe) &&
       self()->cg()->canTransformUnsafeSetMemory())
      return true;

   return false;
   }


TR_PrefetchInfo *
OMR::Compilation::removeExtraPrefetchInfo(TR::Node * node)
   {
   auto iter = self()->getExtraPrefetchInfo().begin();
   for (; iter != self()->getExtraPrefetchInfo().end(); ++iter)
      {
      if ((*iter)->_useNode == node)
         break;
      }

   if (iter != self()->getExtraPrefetchInfo().end())
      {
      TR_PrefetchInfo *info = *iter;
      self()->getExtraPrefetchInfo().erase(iter);
      return info;
      }
   return NULL;
   }


TR_OpaqueMethodBlock *OMR::Compilation::getMethodFromNode(TR::Node * node)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   TR_OpaqueMethodBlock *method = NULL;
   if (bcInfo.getCallerIndex() >= 0 && self()->getNumInlinedCallSites() > 0)
      method = self()->getInlinedCallSite(bcInfo.getCallerIndex())._methodInfo;
   else
      method = self()->getCurrentMethod()->getPersistentIdentifier();
   return method;
   }

int32_t OMR::Compilation::getLineNumber(TR::Node * node)
   {
   TR_ByteCodeInfo bcInfo = node->getByteCodeInfo();
   return self()->fe()->getLineNumberForMethodAndByteCodeIndex(self()->getMethodFromNode(node), bcInfo.getByteCodeIndex());
   }


int32_t OMR::Compilation::getLineNumberInCurrentMethod(TR::Node * node)
   {
   TR_InlinedCallSite site;
   int16_t callerIndex = node->getInlinedSiteIndex();

   if (callerIndex == -1)
      return self()->getLineNumber(node);

   do
      {
      site = self()->getInlinedCallSite(callerIndex);
      callerIndex = site._byteCodeInfo.getCallerIndex();
      } while (callerIndex != -1);

   return self()->fe()->getLineNumberForMethodAndByteCodeIndex(self()->getCurrentMethod()->getPersistentIdentifier(), site._byteCodeInfo.getByteCodeIndex());
   }



bool OMR::Compilation::useRegisterMaps()
   {
   return self()->cg()->usesRegisterMaps() ||
          _options->getOption(TR_RegisterMaps) ||
          debug("regmap");
   }


void OMR::Compilation::resetVisitCounts(vcount_t count)
   {
   if (self()->getMethodSymbol() == self()->getJittedMethodSymbol())
      {
      self()->resetVisitCounts(count, self()->getMethodSymbol());
      for (auto current = _genILSyms.begin(); current != _genILSyms.end(); ++current)
         {
         if ((*current) && (*current)->getFlowGraph() && (*current) != self()->getMethodSymbol())
            self()->resetVisitCounts(count, *current);
         }
      }
   }

void OMR::Compilation::resetVisitCounts(vcount_t count, TR::ResolvedMethodSymbol *method)
   {
   self()->resetVisitCounts(count, method->getFirstTreeTop());
   method->getFlowGraph()->resetVisitCounts(count);
   self()->setVisitCount(count);
   }

void OMR::Compilation::resetVisitCounts(vcount_t count, TR::TreeTop *start)
   {
   TR::TreeTop *tt;
   //Make sure every node has the same unused visit count,
   //so the second loop can set visitCount correctly
   for (tt = start; tt; tt = tt->getNextTreeTop())
      tt->getNode()->resetVisitCounts(MAX_VCOUNT);

   for (tt = start; tt; tt = tt->getNextTreeTop())
      tt->getNode()->resetVisitCounts(count);
   }

void OMR::Compilation::reportFailure(const char *reason)
   {
   traceMsg(self(), "Compilation Failed Because: %s\n", reason);
   if (self()->getOption(TR_PrintErrorInfoOnCompFailure))
      {
      fprintf(stderr, "Compilation Failed Because: %s\n", reason);
      }
   }

void OMR::Compilation::AddCopyPropagationRematerializationCandidate(TR::SymbolReference * sr)
   {
   _copyPropagationRematerializationCandidates[sr->getReferenceNumber()] = 1;
   }

void OMR::Compilation::RemoveCopyPropagationRematerializationCandidate(TR::SymbolReference * sr)
   {
   _copyPropagationRematerializationCandidates[sr->getReferenceNumber()] = 0;
   }

bool OMR::Compilation::IsCopyPropagationRematerializationCandidate(TR::SymbolReference * sr)
   {
   return (_copyPropagationRematerializationCandidates[sr->getReferenceNumber()] == 1);
   }

// Get the first block in the method
//
TR::Block *OMR::Compilation::getStartBlock()
   {
   TR::Node *startNode = self()->getStartTree()->getNode();
   TR_ASSERT(startNode->getOpCodeValue() == TR::BBStart, "start node is not a BBStart");
   return startNode->getBlock();
   }

bool OMR::Compilation::mustNotBeRecompiled()
   {
   return !self()->couldBeRecompiled();
   }

bool OMR::Compilation::shouldBeRecompiled()
   {
   return _recompilationInfo && _recompilationInfo->shouldBeCompiledAgain();
   }

bool OMR::Compilation::couldBeRecompiled()
   {
   return _recompilationInfo && _recompilationInfo->couldBeCompiledAgain();
   }

bool OMR::Compilation::performVirtualGuardNOPing()
   {
   if (!self()->getRecompilationInfo() ||
       !self()->cg()->getSupportsVirtualGuardNOPing() ||
       self()->getOption(TR_DisableVirtualGuardNOPing) ||
       self()->getOption(TR_DisableCHOpts)
      )
      return false;

   static char *skipCold = feGetEnv("TR_NoColdNOPing");
   TR_Hotness minLevel = skipCold ? hot : cold;
   if (self()->getMethodHotness() < minLevel)
      return false;
   return true;
   }


bool OMR::Compilation::isVirtualGuardNOPingRequired(TR_VirtualGuard *virtualGuard)
   {
   if (self()->isProfilingCompilation())
      {
      if (!virtualGuard)
         {
         for (auto current = _virtualGuards.begin(); current != _virtualGuards.end(); ++current)
            if (self()->isVirtualGuardNOPingRequired(*current))
               return true;
         return false;
         }
      else if ((virtualGuard->getKind() == TR_SideEffectGuard) ||
               (virtualGuard->getKind() == TR_DummyGuard) ||
               (virtualGuard->getKind() == TR_OSRGuard) ||
               (virtualGuard->getKind() == TR_HCRGuard) ||
               (virtualGuard->getKind() == TR_MutableCallSiteTargetGuard) ||
               ((virtualGuard->getKind() == TR_InterfaceGuard) && (virtualGuard->getTestType() == TR_MethodTest)))
         return true;
      else
         return false;
      }
   else
      return true;
   }


bool OMR::Compilation::hasBlockFrequencyInfo()
   {
   return false;
   }

void OMR::Compilation::setUsesPreexistence(bool v)
   {
   if (v)
      TR_ASSERT(self()->ilGenRequest().details().supportsInvalidation(), "Can't use preexistence on ilgen request that does not support invalidation");
   _usesPreexistence = v;
   }

// Dump the trees for the method and return the number of nodes in the trees.
//
void OMR::Compilation::dumpMethodTrees(const char *title, TR::ResolvedMethodSymbol * methodSymbol)
   {
   if (self()->getOutFile() == NULL)
      return;

   if (methodSymbol == 0) methodSymbol = _methodSymbol;

   self()->getDebug()->printIRTrees(self()->getOutFile(), title, methodSymbol);

   if (!self()->getOption(TR_DisableDumpFlowGraph))
      self()->dumpFlowGraph(methodSymbol->getFlowGraph());

   if (self()->isOutermostMethod() && self()->getKnownObjectTable()) // This is pretty verbose.  Let's just dump it when we're dumping the whole method.
      self()->getKnownObjectTable()->dumpTo(self()->getOutFile(), self());

   trfflush(self()->getOutFile());
   }

void OMR::Compilation::dumpMethodTrees(const char *title1, const char *title2, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());
   char *title = (char*)self()->trMemory()->allocateStackMemory(20 + strlen(title1) + strlen(title2));
   sprintf(title, "%s%s", title1, title2);
   self()->dumpMethodTrees(title, methodSymbol);
   }

void OMR::Compilation::dumpFlowGraph(TR::CFG * cfg)
   {
   if (cfg == 0) cfg = self()->getFlowGraph();
   if (debug("dumpCFG") || self()->getOption(TR_TraceTrees) || self()->getOption(TR_TraceCG) || self()->getOption(TR_TraceUseDefs))
      {
      if (cfg)
         self()->getDebug()->print(self()->getOutFile(), cfg);
      else
         trfprintf(self()->getOutFile(),"\nControl Flow Graph is empty\n");
      }
   trfflush(self()->getOutFile());
   }

void
OMR::Compilation::mapStaticAddressToCounter(TR::SymbolReference *symRef, TR::DebugCounterBase *counter)
   {
   const void *staticAddress = (const void *)symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
   self()->getDebugCounterMap().insert(std::make_pair(staticAddress, counter));
   }

TR::DebugCounterBase *
OMR::Compilation::getCounterFromStaticAddress(TR::SymbolReference *symRef)
   {
   const void *staticAddress = (const void *)symRef->getSymbol()->castToStaticSymbol()->getStaticAddress();
   TR::Compilation::DebugCounterMap::iterator entry = self()->getDebugCounterMap().find(staticAddress);
   if (entry != self()->getDebugCounterMap().end())
      {
      return entry->second;
      }
   else
      {
      return NULL;
      }
   }

void OMR::Compilation::validateIL(TR::ILValidationContext ilValidationContext)
   {
   TR_ASSERT_FATAL(_ilValidator != NULL, "Attempting to validate the IL without the ILValidator being initialized");
   _ilValidator->validate(TR::omrValidationStrategies[ilValidationContext]);
   }

void OMR::Compilation::verifyTrees(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
         methodSymbol = _methodSymbol;
      self()->getDebug()->verifyTrees(methodSymbol);
      }
   }

void OMR::Compilation::verifyBlocks(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
         methodSymbol = _methodSymbol;
      self()->getDebug()->verifyBlocks(methodSymbol);
      }
   }

void OMR::Compilation::verifyCFG(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
    methodSymbol = _methodSymbol;
      self()->getDebug()->verifyCFG(methodSymbol);
      }
   }

void OMR::Compilation::verifyAndFixRdbarAnchors()
   {
   TR::NodeChecklist anchoredRdbarNodes(self());
   for (TR::PreorderNodeIterator iter(self()->getStartTree(), self()); iter.currentTree() != NULL; ++iter)
      {
      TR::Node *node = iter.currentNode();
      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCode().isResolveOrNullCheck() ||
          node->getOpCode().isAnchor())
         {
         if (node->getFirstChild()->getOpCode().isReadBar())
            anchoredRdbarNodes.add(node->getFirstChild());
         }
      else if (node->getOpCode().isReadBar())
         {
         if (!anchoredRdbarNodes.contains(node))
            {
            TR_ASSERT(0, "node (n%dn) %p is rdbar but not anchored\n", node->getGlobalIndex(), node);
            TR::Node *newttNode = TR::Node::create(TR::treetop, 1, node);
            iter.currentTree()->insertBefore(TR::TreeTop::create(self(), newttNode));
            traceMsg(self(), "node (n%dn) %p is an unanchored readbar, anchor it now under treetop node (n%dn) %p\n", node->getGlobalIndex(), node, newttNode->getGlobalIndex(), newttNode);
            }
         }
      }
   }

#ifdef DEBUG
void OMR::Compilation::dumpMethodGraph(int index, TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getOutFile() == NULL) return;

   if (methodSymbol == 0) methodSymbol = _methodSymbol;

   TR::CFG * cfg = methodSymbol->getFlowGraph();
   if (cfg == 0) cfg = self()->getJittedMethodSymbol()->getFlowGraph();
   if (cfg)
      {
      char fileName[20];
      sprintf(fileName, "cfg%d.vcg", index);

      char tmp[1025];
      char *fn = self()->fe()->getFormattedName(tmp, 1025, fileName, NULL, false);
      TR::FILE *pFile = trfopen(fn, "wb", false);
      TR_ASSERT(pFile != NULL, "unable to open cfg file");
      self()->getDebug()->printVCG(pFile, cfg, self()->signature());
      trfprintf(self()->getOutFile(), "VCG graph dumped in file %s\n", fn);
      trfclose(pFile);
      }
   else
      trfprintf(self()->getOutFile(), "CFG is empty, VCG file not printed\n");
   }
#endif

void
OMR::Compilation::shutdown(TR_FrontEnd * fe)
   {
   TR::FILE *logFile = NULL;
   if (TR::Options::isFullyInitialized() && TR::Options::getCmdLineOptions())
      logFile = TR::Options::getCmdLineOptions()->getLogFile();

   bool printCummStats = ((fe!=0) && TR::Options::getCmdLineOptions() && TR::Options::getCmdLineOptions()->getOption(TR_CummTiming));
   if (printCummStats)
      {
      fprintf(stderr, "Compilation Time   = %9.6f\n", compTime.secondsTaken());
      fprintf(stderr, "Gen IL Time        = %9.6f\n", genILTime.secondsTaken());
      fprintf(stderr, "Optimization Time  = %9.6f\n", optTime.secondsTaken());
      fprintf(stderr, "Code Gen Time      = %9.6f\n", codegenTime.secondsTaken());
      }

#ifdef DEBUG
   TR::CodeGenerator::shutdown(fe, logFile);
#endif

   TR::Recompilation::shutdown();

   TR::Options::shutdown(fe);

#ifdef J9_PROJECT_SPECIFIC
   if (TR::Options::getCmdLineOptions() && TR::Options::getCmdLineOptions()->getOption(TR_EnableCompYieldStats))
      {
      fprintf(stderr, "Statistics regarding time between 2 consective compilation yield points\n");
      J9::Compilation::printCompYieldStatsMatrix();
      }
#endif

#ifdef OPT_TIMING
   fprintf(stderr, "Statistics for time taken by individual options. Time in ms\n");
   for (int i=0; i < OMR::numOpts; i++)
      if (statOptTiming[i].samples() > 0)
         {
         fprintf(stderr, "\n");
         statOptTiming[i].report(stderr);
         }
   statStructuralAnalysisTiming.report(stderr);
   statUseDefsTiming.report(stderr);
   statGlobalValNumTiming.report(stderr);
#endif
   }


void OMR::Compilation::registerResolvedMethodSymbolReference(TR::SymbolReference *symRef)
   {
   _resolvedMethodSymbolReferences[((TR::ResolvedMethodSymbol *)symRef->getSymbol())->getResolvedMethodIndex().value()] = symRef;
   }


bool
OMR::Compilation::notYetRunMeansCold()
   {
   return false;
   }


void
OMR::Compilation::setReturnInfo(TR_ReturnInfo i)
   {
   // For object constructors, set the fixed return type if owning class
   // contains final fields.
   //
   if (_method->isConstructor() &&
       TR::Compiler->cls.hasFinalFieldsInClass(self(), _method->containingClass()))
      {
      _returnInfo = TR_ConstructorReturn;
      return;
      }

   // Note: a dummy void return node can be added after a node, for example a
   // NULLCHK, if it's determined that the node will not return.  In this case
   // we don't want the void return affecting the returnInfo.
   //
   if (i != TR_VoidReturn) _returnInfo = i;
   }

mcount_t
OMR::Compilation::addOwningMethod(TR::ResolvedMethodSymbol * p)
   {
   return mcount_t::valueOf(_methodSymbols.add(p));
   }

TR::ResolvedMethodSymbol *
OMR::Compilation::getOwningMethodSymbol(mcount_t i)
   {
   return _methodSymbols.element(i.value());
   }

TR::ResolvedMethodSymbol *
OMR::Compilation::getOwningMethodSymbol(TR_ResolvedMethod * method)
   {
   for (int32_t i = _methodSymbols.size() - 1; i >= 0; --i)
      if (_methodSymbols[i]->getResolvedMethod() == method)
         return _methodSymbols[i];
   return 0;
   }

TR::ResolvedMethodSymbol *
OMR::Compilation::getOwningMethodSymbol(TR_OpaqueMethodBlock * method)
   {
   for (int32_t i = _methodSymbols.size() - 1; i >= 0; --i)
      if (_methodSymbols[i]->getResolvedMethod()->getPersistentIdentifier() == method)
         return _methodSymbols[i];
   return 0;
   }

vcount_t
OMR::Compilation::getVisitCount()
   {
   return _visitCount;
   }

vcount_t
OMR::Compilation:: setVisitCount(vcount_t vc)
   {
   return (_visitCount = vc);
   }

vcount_t
OMR::Compilation::incVisitCount()
   {
   if (_visitCount == MAX_VCOUNT-1)
      {
      TR_ASSERT(0, "_visitCount equals MAX_VCOUNT-1");
      self()->failCompilation<TR::CompilationException>("_visitCount equals MAX_VCOUNT-1");
      }
   return ++_visitCount;
   }

vcount_t
OMR::Compilation::incOrResetVisitCount()
   {
   if (_visitCount > HIGH_VISIT_COUNT)
      self()->resetVisitCounts(0);
   return self()->incVisitCount();
   }


uint32_t
OMR::Compilation::getNumInlinedCallSites()
   {
   return _inlinedCallSites.size();
   }

TR_InlinedCallSite &
OMR::Compilation::getInlinedCallSite(uint32_t index)
   {
   return _inlinedCallSites[index].site();
   }

TR::ResolvedMethodSymbol  *
OMR::Compilation::getInlinedResolvedMethodSymbol(uint32_t index)
   {
   return _inlinedCallSites[index].resolvedMethodSymbol();
   }

TR_ResolvedMethod  *
OMR::Compilation::getInlinedResolvedMethod(uint32_t index)
   {
   return _inlinedCallSites[index].resolvedMethod();
   }

TR::SymbolReference *
OMR::Compilation::getInlinedCallerSymRef(uint32_t index)
   {
   return _inlinedCallSites[index].callSymRef();
   }

bool
OMR::Compilation::isInlinedDirectCall(uint32_t index)
   {
   return _inlinedCallSites[index].directCall();
   }

/*
 * Get the number of pending push slots for a caller, as this will be the size of
 * the OSRCallSiteRemat table. In the event that the table has not been initialized,
 * it will return 0.
 */
uint32_t
OMR::Compilation::getOSRCallSiteRematSize(uint32_t callSiteIndex)
   {
   if (!_inlinedCallSites[callSiteIndex].osrCallSiteRematTable())
      return 0;

   int32_t callerIndex = self()->getInlinedCallSite(callSiteIndex)._byteCodeInfo.getCallerIndex();
   return callerIndex < 0 ? self()->getMethodSymbol()->getNumPPSlots() :
      self()->getInlinedResolvedMethodSymbol(callerIndex)->getNumPPSlots();
   }

/*
 * Get the pending push symbol reference and the corresponding load, to later remat the pending push
 * within OSR code blocks inside the callee. To get a mapping, the call site index for the callee and
 * the caller's pending push slot should be provided.
 */
void
OMR::Compilation::getOSRCallSiteRemat(uint32_t callSiteIndex, uint32_t slot, TR::SymbolReference *&ppSymRef, TR::SymbolReference *&loadSymRef)
   {
   int32_t *table = _inlinedCallSites[callSiteIndex].osrCallSiteRematTable();
   if (!table)
      {
      ppSymRef = NULL;
      loadSymRef = NULL;
      return;
      }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   // Ensure the requested slot is valid, based on the total number of PP slots
   uint32_t callerNumPPSlots = self()->getOSRCallSiteRematSize(callSiteIndex);
   TR_ASSERT(slot < callerNumPPSlots, "can only perform call site remat for the caller's pending pushes");
#endif

   TR::SymbolReferenceTable *symRefTab = self()->getSymRefTab();
   ppSymRef = table[slot * 2] == 0 ? NULL : symRefTab->getSymRef(table[slot * 2]);
   loadSymRef = table[slot * 2 + 1] == 0 ? NULL : symRefTab->getSymRef(table[slot * 2 + 1]);
   }

/*
 * Set the pending push symbol reference and the corresponding load, to later remat the
 * pending push within OSR code blocks inside the callee. To correctly add the entry, the callee's
 * call site index and the caller's pending push with its matching load should be provided.
 *
 * It is necessary to provide the pending push symbol reference, rather than just its slot, as the slots
 * may be shared. The information necessary to find the symref from the slot is stored, as the call site
 * should be an OSR point, however the pending push symref is expected to be already known, so it is
 * cheaper to use directly.
 */
void
OMR::Compilation::setOSRCallSiteRemat(uint32_t callSiteIndex, TR::SymbolReference *ppSymRef, TR::SymbolReference *loadSymRef)
   {
   int32_t *table = _inlinedCallSites[callSiteIndex].osrCallSiteRematTable();
   int32_t slot = -ppSymRef->getCPIndex() - 1;

   // If no table exists, allocate it based on the number of PP slots
   if (!table)
      {
      int32_t callerIndex = self()->getInlinedCallSite(callSiteIndex)._byteCodeInfo.getCallerIndex();
      uint32_t callerNumPPSlots = callerIndex < 0 ? self()->getMethodSymbol()->getNumPPSlots() :
         self()->getInlinedResolvedMethodSymbol(callerIndex)->getNumPPSlots();
      table = (int32_t*) self()->trMemory()->allocateHeapMemory(callerNumPPSlots * 2 * sizeof(int32_t));
      memset(table, 0, callerNumPPSlots * 2 * sizeof(int32_t));
      _inlinedCallSites[callSiteIndex].setOSRCallSiteRematTable(table);
      }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   // Check the pending push is valid
   uint32_t callerNumPPSlots = self()->getOSRCallSiteRematSize(callSiteIndex);
   TR_ASSERT(ppSymRef->getSymbol()->isPendingPush(), "can only perform call site remat on pending pushes");
   TR_ASSERT(slot >= 0 && unsigned(slot) < callerNumPPSlots, "can only perform call site remat for the caller's pending pushes");
#endif

   table[slot * 2] = ppSymRef->getReferenceNumber();
   table[slot * 2 + 1] = loadSymRef ? loadSymRef->getReferenceNumber() : 0;
   }

/*
 * Check if it is not possible to perform an OSR transition during this call site.
 * A true result means transitioning within this method is definitely not possible, however,
 * a false result does not ensure OSR is possible.
 */
bool
OMR::Compilation::cannotAttemptOSRDuring(uint32_t index)
   {
   return _inlinedCallSites[index].cannotAttemptOSRDuring();
   }

/*
 * Store a known cannotAttemptOSRDuring result against this call site.
 */
void
OMR::Compilation::setCannotAttemptOSRDuring(uint32_t index, bool cannotOSR)
   {
   _inlinedCallSites[index].setCannotAttemptOSRDuring(cannotOSR);
   }

TR_AOTMethodInfo *
OMR::Compilation::getInlinedAOTMethodInfo(uint32_t index)
   {
   return _inlinedCallSites[index].aotMethodInfo();
   }

TR_InlinedCallSite *
OMR::Compilation::getCurrentInlinedCallSite()
   {
   return _inlinedCallStack.isEmpty() ? 0 : &_inlinedCallSites[_inlinedCallStack.top()].site();
   }

int32_t
OMR::Compilation::getCurrentInlinedSiteIndex()
   {
   return _inlinedCallStack.isEmpty() ? -1 : _inlinedCallStack.top();
   }

TR_PrexArgInfo *
OMR::Compilation::getCurrentInlinedCallArgInfo()
   {
   return _inlinedCallArgInfoStack.isEmpty() ? 0 : _inlinedCallArgInfoStack.top();
   }

TR_Stack<TR_PrexArgInfo *>&
OMR::Compilation::getInlinedCallArgInfoStack()
   {
   return _inlinedCallArgInfoStack;
   }

TR_Stack<TR_PeekingArgInfo *> *
OMR::Compilation::getPeekingArgInfo()
   {
   return (&_peekingArgInfo);
   }

TR_PeekingArgInfo *
OMR::Compilation::getCurrentPeekingArgInfo()
   {
   return _peekingArgInfo.isEmpty() ? 0 : _peekingArgInfo.top();
   }

void
OMR::Compilation::addPeekingArgInfo(TR_PeekingArgInfo *info)
  {
   _peekingArgInfo.push(info);
   }

void
OMR::Compilation::removePeekingArgInfo()
   {
   _peekingArgInfo.pop();
   }


TR::ResolvedMethodSymbol *
OMR::Compilation::getMethodSymbol()
   {
   static const bool disableReturnCalleeInIlgen = feGetEnv("TR_DisableReturnCalleeInIlgen") ? true: false;
   if (self()->getCurrentIlGenerator() && !disableReturnCalleeInIlgen)
      return self()->getCurrentIlGenerator()->methodSymbol();
   if (self()->getOptimizer())
      return self()->getOptimizer()->getMethodSymbol();
   return _methodSymbol;
   }


TR_ResolvedMethod *
OMR::Compilation::getCurrentMethod()
   {
   static const bool disableReturnCalleeInIlgen = feGetEnv("TR_DisableReturnCalleeInIlgen") ? true: false;
   if (self()->getCurrentIlGenerator() && !disableReturnCalleeInIlgen)
      return self()->getCurrentIlGenerator()->methodSymbol()->getResolvedMethod();
   if (self()->getOptimizer())
      return self()->getOptimizer()->getMethodSymbol()->getResolvedMethod();
   return _method;
   }

TR::CFG *
OMR::Compilation::getFlowGraph()
   {
   return self()->getMethodSymbol()->getFlowGraph();
   }

TR::TreeTop *
OMR::Compilation::getStartTree()
   {
   return self()->getMethodSymbol()->getFirstTreeTop();
   }

TR::TreeTop *
OMR::Compilation::findLastTree()
   {
   return self()->getMethodSymbol()->getLastTreeTop();
   }

bool
OMR::Compilation::mayHaveLoops()
   {
   return self()->getMethodSymbol()->mayHaveLoops();
   }

bool
OMR::Compilation::hasNews()
   {
   return self()->getMethodSymbol()->hasNews();
   }

bool
OMR::Compilation::hasExceptionHandlers()
   {
   return self()->getMethodSymbol()->hasExceptionHandlers();
   }

TR::HCRMode
OMR::Compilation::getHCRMode()
   {
   if (!self()->getOption(TR_EnableHCR))
      return TR::none;
   if (self()->isDLT() || (self()->isProfilingCompilation() && self()->getProfilingMode() != JProfiling) || self()->getOptLevel() <= cold)
      return TR::traditional;
   return self()->getOption(TR_EnableOSR) && !self()->getOption(TR_DisableNextGenHCR) ? TR::osr : TR::traditional;
   }

uint32_t
OMR::Compilation::getSymRefCount()
   {
   return self()->getSymRefTab()->getNumSymRefs();
   }

void
OMR::Compilation::setStartTree(TR::TreeTop * tt)
   {
   _methodSymbol->setFirstTreeTop(tt);
   }

bool
OMR::Compilation::isPICSite(TR::Instruction *instruction)
   {
   return (std::find(self()->getStaticPICSites()->begin(), self()->getStaticPICSites()->end(), instruction) != self()->getStaticPICSites()->end())
      ||  (std::find(self()->getStaticMethodPICSites()->begin(), self()->getStaticMethodPICSites()->end(), instruction) != self()->getStaticMethodPICSites()->end())
      ||  (std::find(self()->getStaticHCRPICSites()->begin(), self()->getStaticHCRPICSites()->end(), instruction) != self()->getStaticHCRPICSites()->end());
   }

bool
OMR::Compilation::hasLargeNumberOfLoops()
   {
   return (self()->getFlowGraph()->getStructure()->getNumberOfLoops()
           >=
           self()->getOptions()->getLargeNumberOfLoops());
   }



TR_Debug * OMR::Compilation::findOrCreateDebug()
   {
   if (!_debug) _debug = self()->fe()->createDebug(self()); return _debug;
   }

void OMR::Compilation::diagnosticImpl(const char *s, ...)
   {
   va_list ap;
   va_start(ap, s);
   self()->diagnosticImplVA(s, ap);
   va_end(ap);
   }

void OMR::Compilation::diagnosticImplVA(const char *s, va_list ap)
   {
   if (self()->getOutFile() != NULL)
      {
      va_list copy;
      va_copy(copy, ap);
      char buffer[256];
      TR::IO::vfprintf(self()->getOutFile(), self()->getDebug()->getDiagnosticFormat(s, buffer, sizeof(buffer)/sizeof(char)),
                 copy);
      trfflush(self()->getOutFile());
      va_end(copy);
      }
   }


#if defined(DEBUG)

// Limit on the size of the debug string
//
const int indebugLimit = 1023;

// The delimiter characters
//
static const char * delimiters = " ";

// The original value of the environment variable
//
static const char *TR_DEBUGValue = 0;

// The original string with embedded nulls between individual values
//
static char permDebugString[indebugLimit + 1];
static int  permDebugStrLen = 0;

struct DebugValue
   {
   DebugValue * next;
   DebugValue *prev;
   char       *value;
   };

static DebugValue *head = 0;

void addDebug(const char * debugString)
   {
   int32_t debugStringLen = (int32_t) strlen(debugString);
   char * newString = permDebugString + permDebugStrLen+1;
   strcpy(newString, debugString);
   permDebugStrLen += debugStringLen+1;

   // OK, now look for new debug options
   //
   char * t, * savedPointer;
   for (t = STRTOK(newString, delimiters, &savedPointer); t; t = STRTOK(0, " ", &savedPointer))
      {
      DebugValue * newValue = (DebugValue *)malloc(sizeof(DebugValue));
      if (head)
         {
         newValue->next = head;
         newValue->prev = head->prev;
         head->prev->next = newValue;
         head->prev = newValue;
         }
      else
         {
         newValue->next = newValue;
         newValue->prev = newValue;
         }
      newValue->value = t;
      head = newValue;
      }
   }

char * debug(const char *option)
   {

   // Get a pointer to the environment variable.
   if (!TR_DEBUGValue)
      {
      TR_DEBUGValue = feGetEnv("TR_DEBUG");
      if (!TR_DEBUGValue)
         TR_DEBUGValue = "";
      addDebug(TR_DEBUGValue);
      }

   // OK, now look for our option
   int32_t optionLen = (int32_t) strlen(option);
   if (head)
      {
      for (DebugValue *val = head; ; val = val->next)
         {
         if (!strncmp(option, val->value, optionLen) &&
             (val->value[optionLen] == 0 ||
              val->value[optionLen] == '='))
            {
            head = val;
            if (val->value[optionLen])
               ++optionLen;
            return val->value+optionLen;
            }
         if (val->next == head)
            break;
         }
      }

   return 0;
   }
#endif

BitVectorPool::BitVectorPool(TR::Compilation* c) : _comp(c), _pool (c->trMemory()) {};

TR_BitVector* BitVectorPool::get()
   {
   if (_pool.size() > 0)
      return _pool.pop();

   TR_BitVector* newBitVector =  new  (_comp->trHeapMemory()) TR_BitVector(_comp->getNodeCount() /*an estimate*/, _comp->trMemory(), heapAlloc, growable);
   return newBitVector;
   }

void BitVectorPool::release (TR_BitVector* v)
   {
   v->empty();
   _pool.push(v);
   }


namespace TR
{
Compilation& operator<< (Compilation &comp, const char *str)
   {
   traceMsg(&comp,"%s",str);
   return comp;
   }

Compilation& operator<< (Compilation &comp, const int n)
   {
   traceMsg(&comp,"%d",n);
   return comp;
   }

Compilation& operator<< (Compilation &comp, const ::TR_ByteCodeInfo& bcInfo)
   {
   char tmp[20];
   sprintf(tmp, "%d", bcInfo.getByteCodeIndex());
   comp << "{" << bcInfo.getCallerIndex() << ", " << tmp << "}";
   return comp;
   }
}


#define SAVE_COMPILATION_PHASE_NOT_IMPLEMENTED (0xB0FF0)

TR::Compilation::CompilationPhase
OMR::Compilation::saveCompilationPhase()
   {
   return SAVE_COMPILATION_PHASE_NOT_IMPLEMENTED;
   }


void
OMR::Compilation::restoreCompilationPhase(TR::Compilation::CompilationPhase phase)
   {
   if (phase != SAVE_COMPILATION_PHASE_NOT_IMPLEMENTED)
      {
      TR_ASSERT(0, "restoreCompilationPhase not implemented");
      }
   }


OMR::Compilation::CompilationPhaseScope::CompilationPhaseScope(TR::Compilation *comp) :
      _comp(comp),
      _savedPhase(comp->saveCompilationPhase()) {}

OMR::Compilation::CompilationPhaseScope::~CompilationPhaseScope()
   {
   _comp->restoreCompilationPhase(_savedPhase);
   }

TR::Region &OMR::Compilation::aliasRegion()
   {
   return self()->_aliasRegion;
   }

void OMR::Compilation::invalidateAliasRegion()
   {
   if (self()->_aliasRegion.bytesAllocated() > (ALIAS_REGION_LOAD_FACTOR) * TR::Region::initialSize())
      {
      TR::Region::reset(self()->_aliasRegion, _heapMemoryRegion);
      }
   }

bool OMR::Compilation::incompleteOptimizerSupportForReadWriteBarriers()
   {
   return false;
   }


bool OMR::Compilation::isRecursiveMethodTarget(TR_ResolvedMethod *targetResolvedMethod)
   {
   return targetResolvedMethod && targetResolvedMethod->isSameMethod(self()->getCurrentMethod()) && !self()->isDLT();
   }


bool OMR::Compilation::isRecursiveMethodTarget(TR::Symbol *targetSymbol)
   {
   bool isRecursive = false;

   if (targetSymbol)
      {
      TR::MethodSymbol *methodSymbol = targetSymbol->isMethod() ? targetSymbol->castToMethodSymbol() : NULL;
      TR::ResolvedMethodSymbol *resolvedSymbol = methodSymbol ? methodSymbol->getResolvedMethodSymbol() : NULL;
      TR_ResolvedMethod *resolvedMethod  = resolvedSymbol ? resolvedSymbol->getResolvedMethod() : NULL;
      isRecursive = self()->isRecursiveMethodTarget(resolvedMethod);
      }

   return isRecursive;
   }

const TR::TypeLayout* OMR::Compilation::typeLayout(TR_OpaqueClassBlock * clazz)
   {
   TR::Region& region = self()->region();
   auto it = _typeLayoutMap.find(clazz);
   if (it != _typeLayoutMap.end())
      {
      return it->second;
      }
   else
      {
      const TR::TypeLayout* layout = TR::Compiler->cls.enumerateFields(region, clazz, self());
      _typeLayoutMap.insert(std::make_pair(clazz, layout));
      return layout;
      }
   }

TR::list<TR::Snippet*> *
OMR::Compilation::getSnippetsToBePatchedOnClassRedefinition()
   {
   return self()->cg()->getSnippetsToBePatchedOnClassRedefinition();
   }

TR::Block *
OMR::Compilation::insertNewFirstBlock()
   {
   TR::Node *oldBBStart = self()->getStartTree()->getNode();
   TR::Block *oldFirstBlock = self()->getStartTree()->getNode()->getBlock();
   TR::Node *glRegDeps=NULL;
   if (oldBBStart->getNumChildren() == 1)
      glRegDeps = oldBBStart->getChild(0);

   TR::CFG *cfg = self()->getFlowGraph();
   TR::Block *newFirstBlock = TR::Block::createEmptyBlock(oldBBStart, self(), oldFirstBlock->getFrequency());

   newFirstBlock->takeGlRegDeps(self(), glRegDeps);
   cfg->addNode(newFirstBlock, (TR_RegionStructure *)cfg->getStructure());
   cfg->join(newFirstBlock, oldFirstBlock);
   cfg->addEdge(cfg->getStart(), newFirstBlock);
   cfg->removeEdge(cfg->getStart(), oldFirstBlock);
   self()->setStartTree(newFirstBlock->getEntry());

   return newFirstBlock;
   }
