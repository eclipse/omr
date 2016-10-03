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

#include "compile/OMRCompilation.hpp"

#include <limits.h>                            // for UINT_MAX
#include <math.h>                              // for log, pow
#include <signal.h>                            // for sig_atomic_t
#include <stdarg.h>                            // for va_list
#include <stddef.h>                            // for NULL, size_t
#include <stdint.h>                            // for uint8_t, uint64_t, etc
#include <stdio.h>                             // for fprintf, stderr, etc
#include <stdlib.h>                            // for abs, atoi, malloc, etc
#include <string.h>                            // for strncmp, strlen, etc
#include <algorithm>                           // for std::find
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd, etc
#include "codegen/Instruction.hpp"             // for Instruction
#include "codegen/RecognizedMethods.hpp"       // for RecognizedMethod, etc
#include "compile/Compilation.hpp"             // for self(), etc
#include "compile/CompilationTypes.hpp"        // for TR_Hotness
#include "compile/Method.hpp"                  // for TR_Method, etc
#include "compile/OSRData.hpp"                 // for TR_OSRCompilationData, etc
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "compile/VirtualGuard.hpp"            // for TR_VirtualGuard
#include "control/OptimizationPlan.hpp"        // for TR_OptimizationPlan
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "cs2/allocator.h"                     // for heap_allocator
#include "cs2/sparsrbit.h"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                  // for IO (trfflush)
#include "env/ObjectModel.hpp"                 // for ObjectModel
#include "env/KnownObjectTable.hpp"            // for KnownObjectTable
#include "env/PersistentInfo.hpp"              // for PersistentInfo
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                    // for TR_Memory, etc
#include "env/defines.h"                       // for TR_HOST_X86
#include "env/jittypes.h"                      // for TR_ByteCodeInfo, etc
#include "il/Block.hpp"                        // for Block
#include "il/DataTypes.hpp"                    // for DataTypes::Address, etc
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::BBStart, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/NodePool.hpp"                     // for TR::NodePool
#include "il/Node_inlines.hpp"                 // for Node::getType, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/MethodSymbol.hpp"          // for MethodSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "ilgen/IlGenRequest.hpp"              // for IlGenRequest
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "infra/Array.hpp"                     // for TR_Array
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/Bit.hpp"                       // for leadingZeroes
#include "infra/BitVector.hpp"                 // for TR_BitVector, etc
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/Flags.hpp"                     // for flags32_t
#include "infra/Link.hpp"                      // for TR_Pair
#include "infra/List.hpp"                      // for List, ListIterator, etc
#include "infra/Random.hpp"                    // for TR_RandomGenerator
#include "infra/Stack.hpp"                     // for TR_Stack
#include "infra/TRCfgEdge.hpp"                 // for CFGEdge
#include "infra/Timer.hpp"                     // for TR_SingleTimer
#include "infra/ThreadLocal.h"         // for tlsDefine
#include "optimizer/DebuggingCounters.hpp"     // for TR_DebuggingCounters
#include "optimizer/Optimizations.hpp"         // for Optimizations, etc
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/RegisterCandidate.hpp"     // for TR_RegisterCandidates
#include "optimizer/Structure.hpp"             // for TR_RegionStructure, etc
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"                       // for TR_DebugBase
#include "ras/DebugCounter.hpp"                // for TR_DebugCounterGroup, etc
#include "control/Recompilation.hpp"           // for TR_Recompilation, etc

#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"           // for TR_Recompilation, etc
#include "runtime/RuntimeAssumptions.hpp"
#include "env/CHTable.hpp"                     // for TR_AOTGuardSite, etc
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
tlsDefine(TR::Compilation *, compilation);
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

bool OMR::Compilation::nodeNeeds2Regs(TR::Node*node)
   {
   return (node->getType().isInt64() && TR::Compiler->target.is32Bit() && !self()->cg()->use64BitRegsOn32Bit());
   }


static TR::CodeGenerator * allocateCodeGenerator(TR::Compilation * comp)
   {
   return new (comp->trHeapMemory()) TR::CodeGenerator();
   }



uint8_t OMR::Compilation::_NOPTranslateTable[TR_NOP_TRANSLATE_TABLE_SIZE] =
   {
   0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
   0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
   0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
   0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
   0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
   0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
   0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
   0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
   0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
   0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
   0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
   0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
   0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
   0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
   0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
   0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
   };


uint8_t *
OMR::Compilation::getNOPTranslateTable()
   {
   return _NOPTranslateTable;
   }




/* See comment below about operator new */
bool firstCompileStarted = false;

OMR::Compilation::Compilation(
      int32_t id,
      OMR_VMThread *omrVMThread,
      TR_FrontEnd *fe,
      TR_ResolvedMethod *compilee,
      TR::IlGenRequest &ilGenRequest,
      TR::Options &options,
      const TR::Region &dispatchRegion,
      TR_Memory *m,
      TR_OptimizationPlan *optimizationPlan) :
   _trMemory(m),
   _knownObjectTable(NULL),
   _fe((firstCompileStarted = true, fe)),
   _ilGenRequest(ilGenRequest),
   _options(&options),
   _omrVMThread(omrVMThread),
   _currentOptIndex(0),
   _lastBegunOptIndex(0),
   _lastPerformedOptIndex(0),
   _currentOptSubIndex(0), // The transformation index within the current opt
   _lastPerformedOptSubIndex(0),
   _debug(0),
   _region(dispatchRegion),
   _compThreadID(id),
   _allocator(TRCS2MemoryAllocator(m)),
   _arenaAllocator(TR::Allocator(self()->allocator("Arena"))),
   _allocatorName(NULL),
   _method(compilee),
   _firstInstruction(NULL),
   _appendInstruction(NULL),
   _firstColdInstruction(NULL),
   _returnInfo(TR_VoidReturn),
   _methodSymbols(m, 10),
   _visitCount(0),
   _nodeCount(0),
   _accurateNodeCount(0),
   _lastValidNodeCount(0),
   _maxInlineDepth(0),
   _numNestingLevels(0),
   _numLivePendingPushSlots(0),
   _ilGenerator(0),
   _optimizer(0),
   _usesPreexistence(options.getOption(TR_ForceUsePreexistence)),
   _loopVersionedWrtAsyncChecks(false),
   _nextOptLevel(unknownHotness),
   _errorCode(COMPILATION_SUCCEEDED),
   _codeCacheSwitched(false),
   _commitedCallSiteInfo(false),
   _currentSymRefTab(NULL),
   _peekingSymRefTab(NULL),
   _recompilationInfo(0),
   _primaryRandom(NULL),
   _adhocRandom(NULL),
   _toNumberMap(self()->allocator("toNumberMap")),
   _toStringMap(self()->allocator("toStringMap")),
   _toCommentMap(self()->allocator("toCommentMap")),
   _signature(compilee->signature(m)),
   _containsBigDecimalLoad(false),
   _useLongRegAllocation(false),
   _resolvedMethodSymbolReferences(m),
   _inlinedCallSites(m),
   _monitorAutos(m),
   _peekingArgInfo(m),
   _inlinedCallStack(m),
   _inlinedCallArgInfoStack(m),
   _inlinedCalls(0),
   _inlinedFramesAdded(0),
   _staticPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _staticHCRPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _staticMethodPICSites(getTypedAllocator<TR::Instruction*>(self()->allocator())),
   _snippetsToBePatchedOnClassUnload(getTypedAllocator<TR::Snippet*>(self()->allocator())),
   _methodSnippetsToBePatchedOnClassUnload(getTypedAllocator<TR::Snippet*>(self()->allocator())),
   _snippetsToBePatchedOnClassRedefinition(getTypedAllocator<TR::Snippet*>(self()->allocator())),
   _snippetsToBePatchedOnRegisterNative(getTypedAllocator<TR_Pair<TR::Snippet,TR_ResolvedMethod> *>(self()->allocator())),
   _virtualGuards(getTypedAllocator<TR_VirtualGuard*>(self()->allocator())),
   _genILSyms(getTypedAllocator<TR::ResolvedMethodSymbol*>(self()->allocator())),
   _devirtualizedCalls(getTypedAllocator<TR_DevirtualizedCallInfo*>(self()->allocator())),
   _checkcastNullChkInfo(getTypedAllocator<TR_Pair<TR_ByteCodeInfo, TR::Node> *>(self()->allocator())),
   _nodesThatShouldPrefetchOffset(getTypedAllocator<TR_Pair<TR::Node,uint32_t> *>(self()->allocator())),
   _extraPrefetchInfo(getTypedAllocator<TR_PrefetchInfo*>(self()->allocator())),
   _noEarlyInline(true),
   _optimizationPlan(optimizationPlan),
   _verboseOptTransformationCount(0),
   _currentBlock(NULL),
   _unreferencedAutoOrParmOrExternals(self()->allocator("unreferenced external or auto or Parm")),
   _externalSparseBitVector(self()->allocator("external")),
   _monitorAutoSymRefsInCompiledMethod(getTypedAllocator<TR::SymbolReference*>(self()->allocator())),
   _aotMethodCodeStart(NULL),
   _failCHtableCommitFlag(false),
   _numReservedIPICTrampolines(0),
   _osrStateIsReliable(true),
   _canAffordOSRControlFlow(true),
   _phaseTimer("Compilation", self()->allocator("phaseTimer"), self()->getOption(TR_Timing)),
   _phaseMemProfiler("Compilation", self()->allocator("phaseMemProfiler"), self()->getOption(TR_LexicalMemProfiler)),
   _copyPropagationRematerializationCandidates(self()->allocator("CP rematerialization")),
   _prevSymRefTabSize(0),
   _compilationNodes(NULL),
   _nodeOpCodeLength(0),
   _gpuPtxList(m),
   _gpuKernelLineNumberList(m),
   _gpuPtxCount(0),
   _scratchSpaceLimit(TR::Options::_scratchSpaceLimit),
   _cpuTimeAtStartOfCompilation(-1),
   _bitVectorPool(self()),
   _tlsManager(*self())
   {
   _aotClassInfo = new (m->trHeapMemory()) TR::list<TR::AOTClassInfo*>(getTypedAllocator<TR::AOTClassInfo*>(self()->allocator()));

   if (TR::isJ9())
      {
      _ObjectClassPointer   = fe->getClassFromSignature("Ljava/lang/Object;", 18, _method);
      _RunnableClassPointer = fe->getClassFromSignature("Ljava/lang/Runnable;", 20, _method);
      _StringClassPointer   = fe->getClassFromSignature("Ljava/lang/String;", 18, _method);
      _SystemClassPointer   = fe->getClassFromSignature("Ljava/lang/System;", 18, _method);
      _ReferenceClassPointer = fe->getClassFromSignature("Ljava/lang/ref/Reference;", 25, _method);
      _JITHelpersClassPointer = fe->getClassFromSignature("Lcom/ibm/jit/JITHelpers;", 24, _method);
      }
   else
      {
      _ObjectClassPointer = _RunnableClassPointer = _StringClassPointer = _SystemClassPointer = _ReferenceClassPointer = _JITHelpersClassPointer = NULL;
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
   _compilationNodes = new (_trMemory->trHeapMemory()) TR::NodePool(self(), self()->allocator());

#ifdef J9_PROJECT_SPECIFIC
   // The following must stay before the first assumption gets created which could happen
   // while the PersistentMethodInfo is allocated during codegen creation
   // Access to this list must be performed with assumptionTableMutex in hand
   //
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableFastAssumptionReclamation))
      _metadataAssumptionList = new (m->trPersistentMemory()) TR_SentinelRuntimeAssumption();
#endif

   //Random fields must be set before allocating codegen
   _primaryRandom = new (m->trHeapMemory()) TR_RandomGenerator(options.getRandomSeed());
   _adhocRandom = new (m->trHeapMemory()) TR_RandomGenerator(options.getRandomSeed());
   if (_options->getOption(TR_RandomSeedSignatureHash))
      {
      int32_t hash = 0;
      for (const char *c = self()->signature(); *c; c++)
         hash = 33*hash + (int32_t)(*c);
      int32_t seed = _options->getRandomSeed();
      seed ^= hash;
      _primaryRandom->setSeed(seed);
      _adhocRandom->setSeed(_primaryRandom->getRandom());
      }

   if (ilGenRequest.details().isMethodInProgress())
      {
      _flags.set(IsDLTCompile);
      _options->setAllowRecompilation(false);
      }

   if (optimizationPlan)
      {
      if (optimizationPlan->isGPUCompilation())
          _flags.set(IsGPUCompilation);

      if (optimizationPlan->isGPUCompileCPUCode())
          _flags.set(IsGPUCompileCPUCode);


      self()->setGPUBlockDimX(optimizationPlan->getGPUBlockDimX());
      self()->setGPUParms(optimizationPlan->getGPUParms());
      }

   // if we are not in the selective NoOptServer mode
   _isOptServer = (!_options->getOption(TR_NoOptServer)) &&
         ( _options->getOption(TR_Server)
#ifdef J9_PROJECT_SPECIFIC
           || (self()->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold)
#endif
         );
   _isServerInlining = !_options->getOption(TR_NoOptServer);

   //_methodSymbol must be done after symRefTab, but before codegen
   // _methodSymbol must be initialized here because creating a jitted method symbol
   //   actually inspects TR::comp()->_methodSymbol (to compare against the new object)
   _methodSymbol = NULL;
      {
      _methodSymbol = TR::ResolvedMethodSymbol::createJittedMethodSymbol(self()->trHeapMemory(), compilee, self());
      }

   // initPersistentCPUField and createOpCode must be done after method symbol creation

   if (self()->getOption(TR_EnableNodeGC))
      {
      self()->getNodePool().enableNodeGC();
      }

   //codegen also needs _methodSymbol
   _codeGenerator = allocateCodeGenerator(self());
   _recompilationInfo = _codeGenerator->allocateRecompilationInfo();
   _globalRegisterCandidates = new (self()->trHeapMemory()) TR_RegisterCandidates(self());

#ifdef J9_PROJECT_SPECIFIC
   if (_recompilationInfo && _options->getOptLevelDowngraded())
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

   if (self()->isDLT() || (((self()->getMethodHotness() < warm) || self()->compileRelocatableCode() || self()->isProfilingCompilation()) && !enableOSRAtAllOptLevels && !_options->getOption(TR_FullSpeedDebug)))
      {
      self()->setOption(TR_DisableOSR);
      _options->setOption(TR_EnableOSR, false);
      _options->setOption(TR_EnableOSROnGuardFailure, false);
      }

   if (_options->getOption(TR_EnableHCR))
      {
      if (!self()->getOption(TR_DisableOSR))
         self()->setOption(TR_EnableOSR); // Make OSR the default for HCR
      if (_options->getOption(TR_EnableNextGenHCR))
         {
         self()->setOption(TR_EnableOSR);
         _options->setOption(TR_DisableOSR, false);
         }
      }

   if (_options->getOption(TR_EnableOSR))
      {
      //TODO: investigate the memory footprint of this allocation
      _osrCompilationData = new (self()->trHeapMemory()) TR_OSRCompilationData(self());

      if (((self()->getMethodHotness() < warm) || self()->compileRelocatableCode() || self()->isProfilingCompilation()) && !enableOSRAtAllOptLevels && !_options->getOption(TR_FullSpeedDebug)) // Off for two reasons : 1) not sure if we can afford the increase in compile time due to the extra OSR control flow at cold and 2) not sure at this stage in 727 whether OSR can work with AOT (will try to find out soon) but disabling till I do find out
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
#ifdef J9_PROJECT_SPECIFIC
   if ((self()->getInlineDepth() != 0) || self()->isPeekingMethod())
      return false;
#endif
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

TR_Hotness
OMR::Compilation::getDeFactoHotness()
   {
   return self()->isProfilingCompilation() ? warm : self()->getMethodHotness();
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

TR::Compilation *
OMR::Compilation::self()
   {
   return static_cast<TR::Compilation *>(this);
   }


TR::ResolvedMethodSymbol * OMR::Compilation::createJittedMethodSymbol(TR_ResolvedMethod *resolvedMethod)
   {
   return TR::ResolvedMethodSymbol::createJittedMethodSymbol(self()->trHeapMemory(), resolvedMethod, self());
   }


bool OMR::Compilation::canAffordOSRControlFlow()
   {
   if (self()->getOption(TR_DisableOSR) || !self()->getOption(TR_EnableOSR))
      {
      if (self()->getOption(TR_TraceOSR))
         {
         traceMsg(self(), "canAffordOSRControlFlow returning false due to OSR options: disableOSR %d, enableOSR %d\n", self()->getOption(TR_DisableOSR), self()->getOption(TR_EnableOSR));
         }
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
   if (!self()->canAffordOSRControlFlow())
      {
      if (self()->getOption(TR_TraceOSR))
         traceMsg(self(), "canAffordOSRControlFlow is false - OSR induction is not supported\n");
      return false;
      }

   if (self()->getOption(TR_MimicInterpreterFrameShape) /* && areSlotsSharedByRefAndNonRef() */)
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

bool OMR::Compilation::isPotentialOSRPoint(TR::TreeTop *tt, TR::Node *ttNode)
   {
   static char *disableAsyncCheckOSR = feGetEnv("TR_disableAsyncCheckOSR");
   static char *disableGuardedCallOSR = feGetEnv("TR_disableGuardedCallOSR");
   TR::Node *node;
   if (tt)
      node = tt->getNode();
   else
      node = ttNode;

   bool potentialOSRPoint = false;
   if (self()->getHCRMode() == TR::osr)
      {
      if (node->getOpCodeValue() == TR::asynccheck)
         {
         if (disableAsyncCheckOSR == NULL)
            potentialOSRPoint = !self()->isShortRunningMethod(node->getByteCodeInfo().getCallerIndex());
         }
      else if ((node->getOpCodeValue() == TR::treetop || node->getOpCode().isCheck()) && node->getFirstChild()->getOpCode().isCall())
         {
         TR::Node *callNode = node->getFirstChild();
         TR::SymbolReference *callSymRef = callNode->getSymbolReference();
         if (callSymRef->getReferenceNumber() >
             self()->getSymRefTab()->getNonhelperIndex(self()->getSymRefTab()->getLastCommonNonhelperSymbol()))
            potentialOSRPoint = disableGuardedCallOSR == NULL;
         }
      }
   else if (node->canGCandReturn())
      potentialOSRPoint = true;

   if (potentialOSRPoint && tt && !self()->getOption(TR_FullSpeedDebug))
      {
      TR_ByteCodeInfo &bci = node->getByteCodeInfo();
      TR::ResolvedMethodSymbol *method = bci.getCallerIndex() == -1 ?
         self()->getMethodSymbol() : self()->getInlinedResolvedMethodSymbol(bci.getCallerIndex());
      potentialOSRPoint = method->supportsInduceOSR(bci, node, tt->getEnclosingBlock(), NULL, self());
      }

   return potentialOSRPoint;
   }


bool
OMR::Compilation::isProfilingCompilation()
   {
   return _recompilationInfo ? _recompilationInfo->isProfilingCompilation() : false;
   }

TR_OpaqueClassBlock *
OMR::Compilation::getClassClassPointer()
   {
   return _ObjectClassPointer ? self()->fe()->getClassClassPointer(_ObjectClassPointer) : 0;
   }

#if defined(AIXPPC) || defined(LINUX) || defined(J9ZOS390) || defined(WIN32)
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
#endif

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

   bool printCodegenTime = TR::Options::getCmdLineOptions()->getOption(TR_CummTiming);

   if (self()->isOptServer())
      {
      // Temporarily exclude PPC due to perf regression
      if( (self()->getMethodHotness() <= warm))
         {
         if (!TR::Compiler->target.cpu.isPower())
            TR::Options::getCmdLineOptions()->setOption(TR_DisableInternalPointers);
         if (TR::Compiler->target.cpu.isX86())
            TR::Options::getCmdLineOptions()->setOption(TR_DisableLateEdgeSplitting);  // TR_DisableLateEdgeSplitting only used on X86
         }
      self()->getOptions()->setOption(TR_DisablePartialInlining);
      }

#ifdef J9_PROJECT_SPECIFIC
   if (self()->getOptions()->getDelayCompile())
      {
      uint64_t limit = (uint64_t)self()->getOptions()->getDelayCompile();
      uint64_t starttime = self()->getPersistentInfo()->getElapsedTime();
      fprintf(stderr,"\nDelayCompile: Starting a delay of length %d for method %s at time %d.",limit,self()->signature(),starttime);
      fflush(stderr);
      uint64_t temp=0;
      while(1)
         {
         temp = self()->getPersistentInfo()->getElapsedTime();
         if( ( temp - starttime ) > limit)
            {
            fprintf(stderr,"\nDelayCompile: Finished delay at time  = %d, elapsed time = %d\n",temp,(temp-starttime));
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
#elif defined(LINUX) || defined(J9ZOS390) || (defined(WIN32))
   if (self()->getOption(TR_DebugBeforeCompile))
      {
#if defined(LINUXPPC64)
      self()->getDebug()->setupDebugger((void *) *((long*)&(stopBeforeCompile)),(void *) *((long*)&(stopBeforeCompile)), true);
#else
      self()->getDebug()->setupDebugger((void *) &stopBeforeCompile,(void *) &stopBeforeCompile,true);
#endif
      stopBeforeCompile();
      }
#endif

   if (self()->getOutFile() != NULL && (self()->getOption(TR_TraceAll) || debug("traceStartCompile") || self()->getOptions()->getAnyTraceCGOption() || self()->getOption(TR_Timing)))
      {
      self()->getDebug()->printHeader();
      static char *randomExercisePeriodStr = feGetEnv("TR_randomExercisePeriod");
      if (self()->getOption(TR_Randomize) || randomExercisePeriodStr > 0)
         traceMsg(self(), "Random seed is %d%s\n", _options->getRandomSeed(), self()->getOption(TR_RandomSeedSignatureHash)? " hashed with signature":"");
      if (randomExercisePeriodStr > 0)
         TR_RandomGenerator::exercise(atoi(randomExercisePeriodStr), self());
      }

   if (printCodegenTime) compTime.startTiming(self());
   if (_recompilationInfo)
      _recompilationInfo->startOfCompilation();

#ifdef J9_PROJECT_SPECIFIC
   TR_PersistentMethodInfo * methodInfo = TR_PersistentMethodInfo::get(self()->getCurrentMethod());
   if (methodInfo &&
       self()->isProfilingCompilation())
      methodInfo->setProfileInfo(NULL);
#endif

   self()->printMemStatsBefore(self()->signature());

   {
     if (printCodegenTime) genILTime.startTiming(self());
     _ilGenSuccess = _methodSymbol->genIL(self()->fe(), self(), self()->getSymRefTab(), _ilGenRequest);
     if (printCodegenTime) genILTime.stopTiming(self());
   }

   // Force a crash during compilation if the crashDuringCompile option is set
   TR_ASSERT_FATAL(!self()->getOption(TR_CrashDuringCompilation), "crashDuringCompile option is set");

   int32_t optRtn = 0;
   int32_t cgRtn = 0;

   {
   LexicalTimer t("compile", self()->signature(), self()->phaseTimer());
   TR::LexicalMemProfiler mp("compile", self()->signature(), self()->phaseMemProfiler());

   if (_ilGenSuccess)
      {
      _methodSymbol->detectInternalCycles(_methodSymbol->getFlowGraph(), self());

      //detect catch blocks that could have normal predecessors
      //if so, fail the compile
      //
      if (_methodSymbol->catchBlocksHaveRealPredecessors(_methodSymbol->getFlowGraph(), self()))
         self()->fe()->outOfMemory(self(), "Catch blocks have real predecessors");

      if ((debug("dumpInitialTrees") || self()->getOption(TR_TraceTrees)) && self()->getOutFile() != NULL)
         {
         self()->dumpMethodTrees("Initial Trees");
         self()->getDebug()->print(self()->getOutFile(), self()->getSymRefTab());
         }
#ifndef DISABLE_CFG_CHECK
      self()->verifyTrees (_methodSymbol);
      self()->verifyBlocks(_methodSymbol);
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

      optRtn = self()->performOptimizations();

      self()->printMemStatsAfter("optimization");

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

      if (optRtn == 0)
         {
         static char *abortafterilgen = feGetEnv("TR_TOSS_IL");
         if(abortafterilgen)
            self()->fe()->outOfMemory(self(), "Aborting after IL Gen due to TR_TOSS_IL");


         if (_recompilationInfo)
            _recompilationInfo->beforeCodeGen();

         {
           if (printCodegenTime) codegenTime.startTiming(self());
           cgRtn = self()->cg()->generateCode();

           self()->printMemStatsAfter("all codegen");

           if (printCodegenTime) codegenTime.stopTiming(self());
         }

         if (_recompilationInfo && (cgRtn == 0))
            _recompilationInfo->endOfCompilation();

         }

#ifdef J9_PROJECT_SPECIFIC
      if (self()->getOptions()->getVerboseOption(TR_VerboseInlining))
         {
         int32_t jittedBodyHash = strHash(self()->signature());
         char callerBuf[501], calleeBuf[501];
         TR_VerboseLog::vlogAcquire();
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
               strHash(calleeBuf), self()->fej9()->getMethodSize(site._methodInfo), calleeBuf);
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
                  TR_Method *method = node->getSymbol()->getMethodSymbol()->getMethod();
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
                        calleeSize = self()->fej9()->getMethodSize(node->getSymbol()->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier());
                     TR_VerboseLog::writeLine(TR_Vlog_INL, "%x %s %x@%d -> %x bcsz=%d %s",
                        jittedBodyHash,
                        curBlock->isCold()? "coldCalled":"called",
                        strHash(callerSig), bcInfo.getByteCodeIndex(),
                        strHash(calleeSig), calleeSize, calleeSig);
                     }
                  }
               }
            }
         TR_VerboseLog::vlogRelease();
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
      return COMPILATION_UNIMPL_OPCODE;

   if (!_ilGenSuccess)
      return COMPILATION_IL_GEN_FAILURE;

   if (optRtn != COMPILATION_SUCCEEDED)
     return optRtn;

   if (cgRtn != COMPILATION_SUCCEEDED)
     return cgRtn;

#ifdef J9_PROJECT_SPECIFIC
   if (self()->getOption(TR_TraceCG))
      {
      TR_CHTable * chTable = self()->getCHTable();
      if (chTable)
         self()->getDebug()->dump(self()->getOutFile(), chTable);
      }
#endif

#if defined(AIXPPC) || defined(LINUXPPC)
   if (self()->getOption(TR_DebugOnEntry))
      {
      intptrj_t jitTojitStart = (intptrj_t) self()->cg()->getCodeStart();
      jitTojitStart += ((*(int32_t *)(jitTojitStart - 4)) >> 16) & 0x0000ffff;
#if defined(AIXPPC)
      self()->getDebug()->setupDebugger((void *)jitTojitStart);
#else
      self()->getDebug()->setupDebugger((void *)jitTojitStart, self()->cg()->getCodeEnd(), false);
#endif
      }
#elif defined(LINUX) || defined(J9ZOS390) || (defined(WIN32))
   if (self()->getOption(TR_DebugOnEntry))
      {
      self()->getDebug()->setupDebugger(self()->cg()->getCodeStart(),self()->cg()->getCodeEnd(),false);
      }
#endif

   self()->printMemStatsAfter(self()->signature());

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

int32_t OMR::Compilation::performOptimizations()
   {
   // *this    swipeable for debugging purposes

   int32_t optRtn = 0;
   _optimizer = TR::Optimizer::createOptimizer(self(), self()->getJittedMethodSymbol(), false);

   // This opt is needed if certain ilgen input is seen but there is no optimizer created at this point.
   // So the block are tracked during ilgen and the opt is turned on here.
   //
   ListIterator<TR::Block> listIt(&_methodSymbol->getTrivialDeadTreeBlocksList());
   for (TR::Block *block = listIt.getFirst(); block; block = listIt.getNext())
      {
      ((TR::Optimizer*)(_optimizer))->setRequestOptimization(OMR::trivialDeadTreeRemoval, true, block);
      }

   if (_methodSymbol->hasUnkilledTemps())
      {
      ((TR::Optimizer*)(_optimizer))->setRequestOptimization(OMR::globalDeadStoreElimination);
      }

   if (_optimizer)
      optRtn = _optimizer->optimize();

   if (!self()->getOption(TR_EnableSpecializedEpilogues) &&
       self()->getOption(TR_DisableShrinkWrapping) &&
       !TR::Compiler->target.cpu.isZ() && // 390 now uses UseDefs in CodeGenPrep
       !self()->getOptions()->getVerboseOption(TR_VerboseCompYieldStats))
      _optimizer = NULL;

   return optRtn;
   }

bool OMR::Compilation::incInlineDepth(TR::ResolvedMethodSymbol * method, TR_ByteCodeInfo & bcInfo, int32_t cpIndex, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo)
   {
   if (self()->compileRelocatableCode())
      {
      TR_AOTMethodInfo *aotMethodInfo = (TR_AOTMethodInfo *)self()->trMemory()->allocateHeapMemory(sizeof(TR_AOTMethodInfo));
      aotMethodInfo->resolvedMethod = method->getResolvedMethod();
      aotMethodInfo->cpIndex = cpIndex;
      return self()->incInlineDepth((TR_OpaqueMethodBlock*)aotMethodInfo, method, bcInfo, callSymRef, directCall, argInfo);
      }
   else
      {
      return self()->incInlineDepth(method->getResolvedMethod()->getPersistentIdentifier(), method, bcInfo, callSymRef, directCall, argInfo);
      }
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

bool OMR::Compilation::incInlineDepth(TR_OpaqueMethodBlock *methodInfo, TR::ResolvedMethodSymbol * method, TR_ByteCodeInfo & bcInfo, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo)
   {
   int32_t maxCallerIndex = TR_ByteCodeInfo::maxCallerIndex;
   //This restriction is due to a limited number of bits allocated to callerIndex in TR_ByteCodeInfo
   //For example, in Java TR_ByteCodeInfo::maxCallerIndex is set to 4095 (12 bits and one used for signness)
	if (self()->getNumInlinedCallSites() >= maxCallerIndex)
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


   uint32_t callSiteIndex = _inlinedCallSites.add( TR_InlinedCallSiteInfo(methodInfo, bcInfo, method, callSymRef, directCall) );
   _inlinedCallStack.push(callSiteIndex);
   _inlinedCallArgInfoStack.push(argInfo);

   int16_t inlinedCallStackSize = self()->getInlineDepth();

   if (inlinedCallStackSize >= TR_ByteCodeInfo::maxCallerIndex)
      {
      TR_ASSERT(0, "max number of inlined calls exceeded");
      self()->fe()->outOfMemory(self(), "max number of inlined calls exceeded");
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
      while (self()->getCurrentInlinedSiteIndex() < _inlinedCallSites.size())
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

void OMR::Compilation::addMonitorAuto(TR::RegisterMappedSymbol * a, int32_t callerIndex)
   {
   List<TR::RegisterMappedSymbol> * autos = _monitorAutos[callerIndex + 1];
   if (!autos)
      _monitorAutos[callerIndex + 1] = autos = new (self()->trHeapMemory()) List<TR::RegisterMappedSymbol>(self()->trMemory());

   autos->add(a);
   }


void OMR::Compilation::addAsMonitorAuto(TR::SymbolReference* symRef, bool dontAddIfDLT)
   {
   symRef->getSymbol()->setHoldsMonitoredObject();
   int32_t siteIndex = self()->getCurrentInlinedSiteIndex();
   if (!self()->isPeekingMethod())
      {
      self()->addMonitorAuto(symRef->getSymbol()->castToRegisterMappedSymbol(), siteIndex);
      if (!dontAddIfDLT)
         {
         if (siteIndex == -1)
            self()->getMonitorAutoSymRefsInCompiledMethod()->push_front(symRef);
         }
      else
         {
         // only add the symref into the list for initialization when not in DLT and not peeking.
         // in DLT, we already use the corresp. slot to store the locked object from the interpreter
         // so initializing the symRef later in the block can overwrite the first store.
         if (!self()->isDLT() && siteIndex == -1)
            self()->getMonitorAutoSymRefsInCompiledMethod()->push_front(symRef);
         }
      }
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
   _virtualGuards.push_front(guard);
   }

TR_VirtualGuard *
OMR::Compilation::findVirtualGuardInfo(TR::Node*guardNode)
   {
   TR_ASSERT(guardNode->isTheVirtualGuardForAGuardedInlinedCall() || guardNode->isHCRGuard() || guardNode->isProfiledGuard() || guardNode->isMethodEnterExitGuard(), "@node %p\n", guardNode);

   TR_VirtualGuardKind guardKind = TR_NoGuard;
   if (guardNode->isSideEffectGuard())
      guardKind = TR_SideEffectGuard;
   else if (guardNode->isHCRGuard())
      guardKind = TR_HCRGuard;
   else if (guardNode->isMethodEnterExitGuard())
      guardKind = TR_MethodEnterExitGuard;
   else if (guardNode->isMutableCallSiteTargetGuard())
      guardKind = TR_MutableCallSiteTargetGuard;

   TR_VirtualGuard *guard = NULL;

   if (self()->getOption(TR_TraceRelocatableDataDetailsCG))
      traceMsg(self(), "Looking for a guard for node %p with kind %d bcindex %d calleeindex %d\n", guardNode, guardKind, guardNode->getByteCodeInfo().getByteCodeIndex(), guardNode->getByteCodeInfo().getCallerIndex());

   if (guardKind != TR_NoGuard)
      {
      for (auto current = _virtualGuards.begin(); current != _virtualGuards.end(); ++current)
         {
         //traceMsg(self(), "Looking at guard %p with kind %d bcindex %d calleeindex %d\n", current, current->getKind(), current->getByteCodeIndex(), current->getCalleeIndex());
         if ((*current)->getKind() == guardKind &&
             (*current)->getByteCodeIndex() == guardNode->getByteCodeInfo().getByteCodeIndex() &&
             (*current)->getCalleeIndex() == guardNode->getByteCodeInfo().getCallerIndex())
            {
            guard = *current;
            if (self()->getOption(TR_TraceRelocatableDataDetailsCG))
               traceMsg(self(), "found guard %p, guardkind = %d\n", guard, guardKind);
            break;
            }
         }
      }
   else
      {
      for (auto current = _virtualGuards.begin(); current != _virtualGuards.end(); ++current)
         {
         //traceMsg(self(), "Looking at guard %p kind %d bcindex %d calleeindex %d\n", current, current->getKind(), current->getByteCodeIndex(), current->getCalleeIndex());
         if ((*current)->getByteCodeIndex() == guardNode->getByteCodeInfo().getByteCodeIndex() &&
             (*current)->getCalleeIndex() == guardNode->getByteCodeInfo().getCallerIndex() &&
             (*current)->getKind() != TR_SideEffectGuard &&
             (*current)->getKind() != TR_HCRGuard &&
             (*current)->getKind() != TR_MethodEnterExitGuard &&
             (*current)->getKind() != TR_MutableCallSiteTargetGuard)
            {
            guard = *current;
            if (self()->getOption(TR_TraceRelocatableDataDetailsCG))
               traceMsg(self(), "found guard %p, guardkind = %d\n", guard, (*current)->getKind());
            break;
            }
         }
      }

   TR_ASSERT(guard, "Can't find the virtual guard @ node %p \n", guardNode);

   return guard;
   }

void
OMR::Compilation::removeVirtualGuard(TR_VirtualGuard *guard)
   {
   for (auto current = _virtualGuards.begin(); current != _virtualGuards.end(); ++current)
      {
      if ((*current)->getKind() != TR_HCRGuard &&
    ((*current)->getCalleeIndex() == guard->getCalleeIndex()) &&
          ((*current)->getByteCodeIndex() == guard->getByteCodeIndex()))
         {
         if (self()->getOption(TR_TraceRelocatableDataDetailsCG))
            traceMsg(self(), "removeVirtualGuard %p, kind %d bcindex %d calleeindex %d\n", *current, (*current)->getKind(), (*current)->getByteCodeIndex(), (*current)->getCalleeIndex());
         _virtualGuards.erase(current);
         break;
         }
      }
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
         uintptrj_t value = (uintptrj_t)(*pair)->getValue();
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
       self()->cg()->canTransformUnsafeCopyToArrayCopy())
      return true;

   return false;
   }

bool OMR::Compilation::canTransformUnsafeSetMemory()
   {
   if (!self()->getOptions()->realTimeGC() &&
       !TR::Compiler->om.canGenerateArraylets() &&
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
      method = self()->compileRelocatableCode() ? ((TR_ResolvedMethod *)node->getAOTMethod())->getPersistentIdentifier() : self()->getInlinedCallSite(bcInfo.getCallerIndex())._methodInfo;
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

int32_t OMR::Compilation::getOptMessageIndex()
   {
   return (self()->getOptimizer()? self()->getOptimizer()->getOptMessageIndex():0);
   }

// Dump the trees for the method and return the number of nodes in the trees.
//
void OMR::Compilation::dumpMethodTrees(char *title, TR::ResolvedMethodSymbol * methodSymbol)
   {
   // *this    swipeable for debugging purposes
   if (self()->getOutFile() == NULL)
      return;

   if (methodSymbol == 0) methodSymbol = _methodSymbol;

   self()->getDebug()->printIRTrees(self()->getOutFile(), title, methodSymbol);

   if (!self()->getOptions()->getOption(TR_DisableDumpFlowGraph))
      self()->dumpFlowGraph(methodSymbol->getFlowGraph());

   if (self()->isOutermostMethod() && self()->getKnownObjectTable()) // This is pretty verbose.  Let's just dump it when we're dumping the whole method.
      self()->getKnownObjectTable()->dumpTo(self()->getOutFile(), self());

   trfflush(self()->getOutFile());
   }

void OMR::Compilation::dumpMethodTrees(char *title1, const char *title2, TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());
   char *title = (char*)self()->trMemory()->allocateStackMemory(20 + strlen(title1) + strlen(title2));
   sprintf(title, "%s%s", title1, title2);
   self()->dumpMethodTrees(title, methodSymbol);
   }

void OMR::Compilation::dumpFlowGraph(TR::CFG * cfg)
   {
   // *this    swipeable for debugging purposes
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

TR::CodeCache *
OMR::Compilation::getCurrentCodeCache()
   {
   return _codeGenerator ? _codeGenerator->getCodeCache() : 0;
   }

void
OMR::Compilation::setCurrentCodeCache(TR::CodeCache * codeCache)
   {
   if (_codeGenerator) _codeGenerator->setCodeCache(codeCache);
   }

void OMR::Compilation::switchCodeCache(TR::CodeCache *newCodeCache)
   {
   TR_ASSERT( self()->getCurrentCodeCache() != newCodeCache, "Attempting to switch to the currently held code cache");
   self()->setCurrentCodeCache(newCodeCache);  // Even if we signal, we need to update the reserved code cache for recompilations.
   _codeCacheSwitched = true;
   _numReservedIPICTrampolines = 0;
   if ( self()->cg()->committedToCodeCache() || !newCodeCache )
      {
      if (self()->getErrorCode() == COMPILATION_SUCCEEDED)
         {
         if (newCodeCache)
            self()->setErrorCode(COMPILATION_ILLEGAL_CODE_CACHE_SWITCH);
         else
            self()->setErrorCode(COMPILATION_NULL_SUBSTITUTE_CODE_CACHE);
         }
      self()->fe()->outOfMemory(self(), "Already committed to current code cache");
      }
   }

void OMR::Compilation::verifyTrees(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOptions()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
         methodSymbol = _methodSymbol;
      self()->getDebug()->verifyTrees(methodSymbol);
      }
   }

void OMR::Compilation::verifyBlocks(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOptions()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
         methodSymbol = _methodSymbol;
      self()->getDebug()->verifyBlocks(methodSymbol);
      }
   }

void OMR::Compilation::verifyCFG(TR::ResolvedMethodSymbol *methodSymbol)
   {
   if (self()->getDebug() && !self()->getOptions()->getOption(TR_DisableVerification) && !self()->isPeekingMethod())
      {
      if (!methodSymbol)
    methodSymbol = _methodSymbol;
      self()->getDebug()->verifyCFG(methodSymbol);
      }
   }

#ifdef DEBUG
void OMR::Compilation::dumpMethodGraph(int index, TR::ResolvedMethodSymbol *methodSymbol)
   {
   // *this    swipeable for debugging purposes
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
      fprintf(stderr, "Compilation Time   = %s\n", compTime.timeTakenString(TR::comp()));
      fprintf(stderr, "Gen IL Time        = %s\n", genILTime.timeTakenString(TR::comp()));
      fprintf(stderr, "Optimization Time  = %s\n", optTime.timeTakenString(TR::comp()));
      fprintf(stderr, "Code Gen Time      = %s\n", codegenTime.timeTakenString(TR::comp()));
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


bool OMR::Compilation::notYetRunMeansCold()
   {
   if (_optimizer && !((TR::Optimizer*)_optimizer)->isIlGenOpt())
      return false;

   TR_ResolvedMethod *currentMethod = self()->getJittedMethodSymbol()->getResolvedMethod();

   intptrj_t initialCount = currentMethod->hasBackwardBranches() ?
                             self()->getOptions()->getInitialBCount() :
                             self()->getOptions()->getInitialCount();

   if (currentMethod->convertToMethod()->isBigDecimalMethod() ||
       currentMethod->convertToMethod()->isBigDecimalConvertersMethod())
       initialCount = 0;

#ifdef J9_PROJECT_SPECIFIC
    switch (currentMethod->getRecognizedMethod())
      {
      case TR::com_ibm_jit_DecimalFormatHelper_formatAsDouble:
      case TR::com_ibm_jit_DecimalFormatHelper_formatAsFloat:
         initialCount = 0;
         break;
      default:
      	break;
      }

    if (currentMethod->containingClass() == self()->getStringClassPointer())
       {
       if (currentMethod->isConstructor())
          {
          char *sig = currentMethod->signatureChars();
          if (!strncmp(sig, "([CIIII)", 8) ||
              !strncmp(sig, "([CIICII)", 9) ||
              !strncmp(sig, "(II[C)", 6))
             initialCount = 0;
          }
       else
          {
          char *sig = "isRepeatedCharCacheHit";
          if (strncmp(currentMethod->nameChars(), sig, strlen(sig)) == 0)
             initialCount = 0;
          }
       }
#endif

   if (
      self()->isDLT()
      || (initialCount < TR_UNRESOLVED_IMPLIES_COLD_COUNT)
      || ((self()->getOption(TR_UnresolvedAreNotColdAtCold) && self()->getMethodHotness() == cold) || self()->getMethodHotness() < cold)
      || currentMethod->convertToMethod()->isArchetypeSpecimen()
      || (  self()->getCurrentMethod()
         && self()->getCurrentMethod()->convertToMethod()->isArchetypeSpecimen())
      )
      return false;
   else
      return true;
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
      self()->fe()->outOfMemory(self(), "_visitCount equals MAX_VCOUNT-1");
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
   if (self()->getOptimizer())
      return self()->getOptimizer()->getMethodSymbol();
   return _methodSymbol;
   }


TR_ResolvedMethod *
OMR::Compilation::getCurrentMethod()
   {
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

TR::HCRMode
OMR::Compilation::getHCRMode()
   {
   if (!self()->getOption(TR_EnableHCR))
      return TR::none;
   if (self()->isDLT() || self()->isProfilingCompilation() || self()->getOptLevel() <= cold)
      return TR::traditional;
   return self()->getOption(TR_EnableOSR) && self()->getOption(TR_EnableNextGenHCR) ? TR::osr : TR::traditional;
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

void OMR::Compilation::printMemStats(const char *name)
   {
   }

void OMR::Compilation::printMemStatsBefore(const char *name)
   {
   }

void OMR::Compilation::printMemStatsAfter(const char *name)
   {
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
#if defined(DEBUG)
   va_list ap;
   va_start(ap, s);
   self()->diagnosticImplVA(s, ap);
   va_end(ap);
#endif
   }

void OMR::Compilation::diagnosticImplVA(const char *s, va_list ap)
   {
#if defined(DEBUG)
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
#endif
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


#if !defined(JITTEST) // TLP for the delete in checkStore
// There should be no allocations that use the global operator new, since
// all allocations should go through the JitMemory allocation routines.
// To catch cases that we miss, we define global operator new here.
// (This isn't safe on static platforms, since we don't want to affect user
// natives which might legitimately use new and delete.  Also, xlC
// won't link staticly with the -noe flag when we override these.)
//
void *operator new(size_t size)
   {
   #if defined(DEBUG)
   #if LINUX
   // glibc allocates something at dl_init; check if a method is being compiled to avoid
   // getting assumes at _dl_init
   if (firstCompileStarted)
   #endif
      {
      printf( "\n*** ERROR *** Invalid use of global operator new\n");
      TR_ASSERT(0,"Invalid use of global operator new");
      }
   #endif
   return malloc(size);
   }

// Since we are using GC, heap deletions must be a no-op
//
void operator delete(void *)
   {
   TR_ASSERT(0, "Invalid use of global operator delete");
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
