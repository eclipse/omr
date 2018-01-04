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

#include "control/Options.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Options_inlines.hpp"

#include <algorithm>                     // for std::max, etc
#include <ctype.h>                       // for isdigit
#include <limits.h>                      // for INT_MAX, USHRT_MAX
#include <stddef.h>                      // for offsetof
#include <stdio.h>                       // for sprintf, printf
#include <stdlib.h>                      // for atoi, malloc, strtol
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"       // for Compilation, comp
#include "compile/CompilationTypes.hpp"  // for TR_Hotness
#include "compile/ResolvedMethod.hpp"    // for TR_ResolvedMethod
#include "control/OptimizationPlan.hpp"  // for TR_OptimizationPlan
#include "control/Recompilation.hpp"     // for TR_PersistentJittedBodyInfo, etc
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                    // for IO
#include "env/ObjectModel.hpp"           // for ObjectModel
#include "env/Processors.hpp"            // for TR_Processor, etc
#include "env/defines.h"                 // for TR_HOST_64BIT, TR_HOST_X86
#include "env/jittypes.h"                // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"              // for DataType, etc
#include "il/ILOps.hpp"                  // for TR::ILOpCode
#include "infra/SimpleRegex.hpp"
#include "ras/Debug.hpp"                 // for TR_Debug
#include "ras/IgnoreLocale.hpp"          // for stricmp_ignore_locale, etc

#if !defined(J9_PROJECT_SPECIFIC)
#include "env/JitConfig.hpp"
#endif

#ifdef J9_PROJECT_SPECIFIC
#include "control/RecompilationInfo.hpp"     // for TR_PersistentJittedBodyInfo, etc
#include "env/VMJ9.h"
#endif

using namespace OMR;

#define SET_OPTION_BIT(x)   TR::Options::setBit,   offsetof(OMR::Options,_options[(x)&TR_OWM]), ((x)&~TR_OWM)
#define RESET_OPTION_BIT(x) TR::Options::resetBit, offsetof(OMR::Options,_options[(x)&TR_OWM]), ((x)&~TR_OWM)
#define SET_TRACECG_BIT(x)  TR::Options::setBit,   offsetof(OMR::Options, _cgTrace), (x)

#define NoOptString                     "noOpt"
#define DisableAllocationInliningString "disableAllocationInlining"
#define DisableInlineCheckCastString    "disableInlineCheckCast"
#define DisableInlineIfInstanceOfString "disableInlineIfInstanceOf"
#define DisableInlineInstanceOfString   "disableInlineInstanceOf"
#define DisableInlineMonEntString       "disableInlineMonEnt"
#define DisableInlineMonExitString      "disableInlineMonExit"
#define DisableInliningOfNativesString  "disableInliningOfNatives"
#define DisableNewInstanceImplOptString "disableNewInstanceImplOpt"
#define DisableFastStringIndexOfString  "disableFastStringIndexOf"
#define DisableVirtualGuardNOPingString "disableVirtualGuardNOPing"
#define DisableAnnotations              "disableAnnotations"
#define EnableAnnotations               "enableAnnotations"

#define TR_VSS_ALIGNMENT 8
#define TR_AGGR_CONST_DISPLAY_SIZE 16
#define TR_STORAGE_ALIGNMENT_DISPLAY_SIZE 128
#define TR_LABEL_TARGET_NOP_LIMIT 232

#define TR_MAX_BUCKET_INDEX_COUNT 20
#define TR_MAX_LIMITED_GRA_REGS 5
#define TR_MAX_LIMITED_GRA_CANDIDATES (INT_MAX)
#define TR_MAX_LIMITED_GRA_BLOCKS (INT_MAX)
#define TR_MAX_LIMITED_GRA_REPRIORITIZE (2)
#define TR_MAX_LIMITED_GRA_PERFORM_CELLS (INT_MAX)

#define TR_PERFORM_INLINE_TREE_EXPANSION 0
#define TR_PERFORM_INLINE_BLOCK_EXPANSION 0


static char * EXCLUDED_METHOD_OPTIONS_PREFIX = "ifExcluded";

// The following options must be placed in alphabetical order for them to work properly
TR::OptionTable OMR::Options::_jitOptions[] = {

   { "abstractTimeGracePeriodInliningAggressiveness=", "O<nnn>Time to maintain full inlining aggressiveness\t",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_abstractTimeGracePeriod, 0, "F%d", NOT_IN_SUBSET },
   { "abstractTimeToReduceInliningAggressiveness=", "O<nnn>Time to lower inlining aggressiveness from highest to lowest level\t",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_abstractTimeToReduceInliningAggressiveness, 0, "F%d", NOT_IN_SUBSET },
   {"acceptHugeMethods",     "O\tallow processing of really large methods", SET_OPTION_BIT(TR_ProcessHugeMethods), "F" },
   {"activateCompThreadWhenHighPriReqIsBlocked", "M\tactivate another compilation thread when high priority request is blocked",  SET_OPTION_BIT(TR_ActivateCompThreadWhenHighPriReqIsBlocked), "F", NOT_IN_SUBSET},
   {"aggressiveRecompilationChances=", "O<nnn>\tnumber of chances per method to recompile with the "
                                       "aggressive recompilation mechanism",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_aggressiveRecompilationChances, 0, "F%d", NOT_IN_SUBSET},
   {"allowVPRangeNarrowingBasedOnDeclaredType",
      "I\tallow value propagation to assume that integers declared "
      "narrower than 32-bits (boolean, byte, char, short) are in-range",
      SET_OPTION_BIT(TR_AllowVPRangeNarrowingBasedOnDeclaredType), "F" },
   {"alwaysFatalAssert",       "I\tAlways execute fatal assertion for testing purposes",           SET_OPTION_BIT(TR_AlwaysFatalAssert), "F"},
   {"alwaysSafeFatalAssert", "I\tAlways issue a safe fatal assertion for testing purposes",      SET_OPTION_BIT(TR_AlwaysSafeFatal), "F"},
   {"alwaysWorthInliningThreshold=", "O<nnn>\t", TR::Options::set32BitNumeric, offsetof(OMR::Options, _alwaysWorthInliningThreshold), 0, " %d" },
   {"aot",                "O\tahead-of-time compilation",
        SET_OPTION_BIT(TR_AOT), NULL, NOT_IN_SUBSET},
   {"aotOnlyFromBootstrap", "O\tahead-of-time compilation allowed only for methods from bootstrap classes",
        SET_OPTION_BIT(TR_AOTCompileOnlyFromBootstrap), "F", NOT_IN_SUBSET },
   {"aotrtDebugLevel=", "R<nnn>\tprint aotrt debug output according to level", TR::Options::set32BitNumeric, offsetof(OMR::Options,_newAotrtDebugLevel), 0, " %d"},
   {"aotSecondRunDetection",  "M\tperform second run detection for AOT", RESET_OPTION_BIT(TR_NoAotSecondRunDetection), "F", NOT_IN_SUBSET},
   {"assignEveryGlobalRegister", "I\tnever refuse to assign any possible register for GRA in spite of the resulting potential spills", SET_OPTION_BIT(TR_AssignEveryGlobalRegister), "F"},
   {"assumeStartupPhaseUntilToldNotTo", "M\tUse compiler.Command(""endOfStartup"") to exit startup phase",
                 SET_OPTION_BIT(TR_AssumeStartupPhaseUntilToldNotTo), "F" },
   {"assumeStrictFP",     "C\talways assume strictFP semantics",
        SET_OPTION_BIT(TR_StrictFP), "F" },
   {"bcount=",            "O<nnn>\tnumber of invocations before compiling methods with loops",
        TR::Options::setCount, offsetof(OMR::Options,_initialBCount), 0, " %d"},
   {"bestAvailOpt",       "O\tdeprecated; equivalent to optLevel=warm",
        TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), warm},
   {"bigAppThreshold=", "R<nnn>\tNumber of loaded classes used to determine if crt app is 'big'",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_bigAppThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"bigCalleeFreqCutoffAtHot=", "O<nnn>\tInliner threshold for block frequncy for cold callees for opt level higher then warm",
        TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeFreqCutoffAtHot), 40, " %d"},
   {"bigCalleeFreqCutoffAtWarm=", "O<nnn>\tInliner threshold for block frequncy for cold callees for opt level less of equal to warm",
        TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeFreqCutoffAtWarm), 40, " %d"},
   {"bigCalleeHotOptThreshold=", "O<nnn>\tInliner threshold", TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeHotOptThreshold), 0, " %d" },
   {"bigCalleeScorchingOptThreshold=", "O<nnn>\tInliner threshold", TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeScorchingOptThreshold), 0, " %d" },
   {"bigCalleeThreshold=", "O<nnn>\tInliner threshold", TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeThreshold), 0, " %d" },
   {"bigCalleeThresholdForColdCallsAtHot=", "O<nnn>\tInliner threshold for cold calls for opt level higher then warm",
        TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeThresholdForColdCallsAtHot), 500, " %d"},
   {"bigCalleeThresholdForColdCallsAtWarm=", "O<nnn>\tInliner threshold for cold calls for opt level less or equal to warm",
        TR::Options::set32BitNumeric, offsetof(OMR::Options, _bigCalleeThresholdForColdCallsAtWarm), 100, " %d"},
   {"blockShufflingSequence=",  "D<string>\tDescription of the particular block shuffling operations to perform; see source code for more details",
        TR::Options::setString,  offsetof(OMR::Options,_blockShufflingSequence), 0, "P%s"},
   {"breakAfterCompile",  "D\traise trap when method compilation ends",   SET_OPTION_BIT(TR_BreakAfterCompile),  "F" },
   {"breakBeforeCompile", "D\traise trap when method compilation begins", SET_OPTION_BIT(TR_BreakBeforeCompile), "F" },

   {"breakOnBBStart",     "D\traise trap on BBStarts of method", SET_OPTION_BIT(TR_BreakBBStart),     "F" },
   {"breakOnCompile",     "D\tdeprecated; equivalent to breakBeforeCompile",    SET_OPTION_BIT(TR_BreakBeforeCompile), "F" },
   {"breakOnCreate=",     "D{regex}\traise trap when creating an item whose name matches regex", TR::Options::setRegex, offsetof(OMR::Options,_breakOnCreate), 0, "P"},
   {"breakOnEntry",       "D\tinsert entry breakpoint instruction in generated code",
        SET_OPTION_BIT(TR_EntryBreakPoints), "F" },
   {"breakOnJ2IThunk",    "D\tbreak before executing a jit-to-interpreter thunk", SET_OPTION_BIT(TR_BreakOnJ2IThunk), "P", NOT_IN_SUBSET},
   {"breakOnLoad",        "D\tbreak after the options have been processed", TR::Options::breakOnLoad, 0, 0, "P", NOT_IN_SUBSET},
   {"breakOnNew",         "D\tbreak before an inlined object allocation", SET_OPTION_BIT(TR_BreakOnNew), "F"},
   {"breakOnOpts=",        "D{regex}\traise trap when performing opts with matching regex",
        TR::Options::setRegex, offsetof(OMR::Options, _breakOnOpts), 0, "P"},
   {"breakOnPrint=",     "D{regex}\traise trap when print an item whose name matches regex", TR::Options::setRegex, offsetof(OMR::Options,_breakOnPrint), 0, "P"},
   {"breakOnThrow=",       "D{regex}\traise trap when throwing an exception whose class name matches regex", TR::Options::setRegex, offsetof(OMR::Options,             _breakOnThrow), 0, "P"},
   {"breakOnWriteBarrier", "D\tinsert breakpoint instruction ahead of inline write barrier", SET_OPTION_BIT(TR_BreakOnWriteBarrier), "F" },
   {"breakOnWriteBarrierSnippet", "D\tinsert breakpoint instruction at beginning of write barrier snippet", SET_OPTION_BIT(BreakOnWriteBarrierSnippet), "F" },
   {"checkGRA",                "D\tPreserve stores that would otherwise be removed by GRA, and then verify that the stored value matches the global register", SET_OPTION_BIT(TR_CheckGRA), "F"},
   {"checkStructureDuringExitExtraction", "D\tCheck structure after each step of exit extraction", SET_OPTION_BIT(TR_CheckStructureDuringExitExtraction), "F"},
   {"classesWithFoldableFinalFields=",   "O{regex}\tAllow hard-coding of values of final fields in the specified classes.  Default is to fold anything considered safe.", TR::Options::setRegex, offsetof(OMR::Options, _classesWithFolableFinalFields), 0, "F"},
   {"classExtendRatSize=",   "M<nnn>\tsize of runtime assumption table for class extend",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_classExtendRatSize, 0, "F%d", NOT_IN_SUBSET},
   {"classRedefinitionUPICRatSize=", "M<nnn>\tsize of runtime assumption table for classRedefinitionUPIC",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_classRedefinitionUPICRatSize, 0, "F%d", NOT_IN_SUBSET},
   {"coldRunBCount=",          "O<nnn>\tnumber of invocations before compiling methods with loops in AOT cold runs",
                               TR::Options::setCount, offsetof(OMR::Options,_initialColdRunBCount), 0, " %d", NOT_IN_SUBSET},
   {"coldRunCount=",           "O<nnn>\tnumber of invocations before compiling methods with loops in AOT cold runs",
                               TR::Options::setCount, offsetof(OMR::Options,_initialColdRunCount), 0, " %d", NOT_IN_SUBSET},
   {"coldUpgradeSampleThreshold=", "O<nnn>\tnumber of samples a method needs to get in order "
                                   "to be upgraded from cold to warm. Default 30. ",
                                    TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_coldUpgradeSampleThreshold, 0, "P%d", NOT_IN_SUBSET},
   {"compilationStrategy=",    "O<strategyname>\tname of the compilation strategy to use",
                               TR::Options::setStaticString,  (intptrj_t)(&OMR::Options::_compilationStrategyName), 0, "F%s", NOT_IN_SUBSET},
   {"compilationThreads=",   "R<nnn>\tnumber of compilation threads to use",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_numUsableCompilationThreads, 0, "F%d", NOT_IN_SUBSET},
   {"compile",                "D\tCompile these methods immediately. Primarily for use with Compiler.command",  SET_OPTION_BIT(TR_CompileBit),  "F" },
   {"compThreadCPUEntitlement=", "M<nnn>\tThreshold for CPU utilization of compilation threads",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_compThreadCPUEntitlement, 0, "F%d", NOT_IN_SUBSET },
   {"concurrentLPQ", "M\tCompilations from low priority queue can go in parallel with compilations from main queue", SET_OPTION_BIT(TR_ConcurrentLPQ), "F", NOT_IN_SUBSET },
   {"conservativeCompilation","O\tmore conservative decisions regarding compilations", SET_OPTION_BIT(TR_ConservativeCompilation), "F"},
   {"continueAfterILValidationError", "O\tDo not abort compilation upon encountering an ILValidation failure.", SET_OPTION_BIT(TR_ContinueAfterILValidationError), "F"},
   {"count=",             "O<nnn>\tnumber of invocations before compiling methods without loops",
        TR::Options::setCount, offsetof(OMR::Options,_initialCount), 0, " %d"},

   {"countOptTransformations=", "D\treport number of matching opt transformations in verbose log", TR::Options::configureOptReporting, TR_VerboseOptTransformations, 0, "F"},
   {"countPercentageForEarlyCompilation=", "M<nnn>\tNumber 1..100",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_countPercentageForEarlyCompilation, 0, "F%d", NOT_IN_SUBSET },
   {"counts=",            "Oc0 b0 m0 c1 b1 m1 ...\trecompilation counts, where cN, "
                          "bN and mN are the count, bcount and milcount values to "
                          "recompile at level N. If a value is '-' that opt level "
                          "is skipped.\n"
                          "Overrides count, bcount, milcount and optLevel options.",
        TR::Options::setString,  offsetof(OMR::Options,_countString), 0, "P%s", NOT_IN_SUBSET},
   {"countWriteBarriersRT", "D\tcount how many fast and slow RT write barriers occur per thread", SET_OPTION_BIT(TR_CountWriteBarriersRT), "F" },
   {"cpuExpensiveCompThreshold=", "M<nnn>\tthreshold for when compilations are considered cpu expensive",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_cpuExpensiveCompThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"cpuUsageCircularBufferSize=", "O<nnn>\tSet the size of the CPU Usage Circular Buffer; Set it to 0 to disable",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_cpuUsageCircularBufferSize, 0, "F%d", NOT_IN_SUBSET},
   {"cpuUsageCircularBufferUpdateFrequencySec=", "O<nnn>\tFrequency of the CPU Usage Array update",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_cpuUsageCircularBufferUpdateFrequencySec, 0, "F%d", NOT_IN_SUBSET},
   {"crashDuringCompile", "M\tforce a crash during compilation", SET_OPTION_BIT(TR_CrashDuringCompilation), "F" },
   {"debugBeforeCompile", "D\tinvoke the debugger when method compilation begins", SET_OPTION_BIT(TR_DebugBeforeCompile), "F" },
   {"debugCounterBucketGranularity=", "D<nnn>\tNumber of buckets per power of two for histogram debug counters", TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_counterBucketGranularity), 0, "F%d"},
   {"debugCounterBuckets=",           "D<nnn>\tAlias for counterBucketGranularity", TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_counterBucketGranularity), 0, "F%d"},
   {"debugCounterFidelity=",          "D<nnn>\tDisable dynamic debug counters with fidelity rating below this", TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_minCounterFidelity), 0, "F%d"},
   {"debugCounterHistograms=",        "D{regex}\tEnable these debug counters broken down into value buckets", TR::Options::setRegex, offsetof(OMR::Options, _counterHistogramNames), 0, "F"},
   {"debugCounters=",                 "D{regex}\tEnable dynamic debug counters with names matching regex (unless they fail to meet some other criterion)", TR::Options::setRegex, offsetof(OMR::Options, _enabledDynamicCounterNames), 0, "F"},
   {"debugCounterWarmupSeconds=",     "D<nnn>\tDebug counters will be reset to zero after nnn seconds, so only increments after this point will end up in the final report", TR::Options::set64BitSignedNumeric, offsetof(OMR::Options,_debugCounterWarmupSeconds), 0, "F%d"},
   {"debugInliner",     "O\ttrace statements to debug the Inliner",             SET_OPTION_BIT(TR_DebugInliner), "F" },
   {"debugOnCompile",     "D\tdeprecated; equivalent to debugBeforeCompile",             SET_OPTION_BIT(TR_DebugBeforeCompile), "F" },
   {"debugOnCreate=",     "D{regex}\tinvoke the debugger when creating an item whose name matches regex", TR::Options::setRegex, offsetof(OMR::Options,_debugOnCreate), 0, "P"},
   {"debugOnEntry",       "D\tinvoke the debugger at the entry of a method",       SET_OPTION_BIT(TR_DebugOnEntry),       "F" },
   {"debugRedundantMonitorElimination",     "O\ttrace statements to debug Monitor Elimination",             SET_OPTION_BIT(TR_DebugRedundantMonitorElimination), "F" },
   {"deferReferenceManipulations", "I\tdefer object reference manipulations to the host runtime.", SET_OPTION_BIT(TR_DeferReferenceManipulations), "F"},
   {"delayCompile=",      "I<nnn>\tAmount of time in ms before compile is started",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_delayCompile), 0, "F%d"},
   {"delayToEnableIdleCpuExploitation=", "M<nnn>\t",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_delayToEnableIdleCpuExploitation, 0, "F%d", NOT_IN_SUBSET },
   {"disableAbstractInlining",            "O\tdisable inlining of abstract methods with a single implementor", SET_OPTION_BIT(TR_DisableAbstractInlining), "F"},
   {"disableAdaptiveDumbInliner",         "O\tdisable adaptive dumbInliner strategy", SET_OPTION_BIT(TR_DisableAdaptiveDumbInliner), "F"},
   {"disableAESInHardware",               "O\tdo not use native AES instructions", SET_OPTION_BIT(TR_DisableAESInHardware), "F"},
   {"disableAggressiveRecompilations",     "R\trecompilation to higher opt levels is not anymore twice as probable", SET_OPTION_BIT(TR_DisableAggressiveRecompilations), "F"},
   {DisableAllocationInliningString,      "O\tdisable ANewArray    inline fast helper",        SET_OPTION_BIT(TR_DisableAllocationInlining)   , "F"},
   {"disableAllocationOfScratchBTL",      "M\tRefuse to allocate scratch memory below the line (zOS 31-bit)", SET_OPTION_BIT(TR_DontAllocateScratchBTL), "F", NOT_IN_SUBSET },
   {"disableAllocationSinking",           "O\tdon't delay object allocations until immediately before the corresponding constructor calls", TR::Options::disableOptimization, allocationSinking, 0, "P"},
   {"disableAndSimplification",           "O\tdisable and simplification",                     TR::Options::disableOptimization, andSimplification, 0, "P"},
   {DisableAnnotations,                   "O\tdisable annotation support",                     RESET_OPTION_BIT(TR_EnableAnnotations), "F"},
   {"disableAOTCheckCastInlining",        "O\tdisable AOT check cast inlining",                SET_OPTION_BIT(TR_DisableAOTCheckCastInlining), "F"},
   {"disableAOTColdCheapTacticalGRA",   "O\tdisable AOT cold cheap tactical GRA",                      SET_OPTION_BIT(TR_DisableAOTColdCheapTacticalGRA), "F"},
   {"disableAOTInstanceFieldResolution",   "O\tdisable AOT instance field resolution",                      SET_OPTION_BIT(TR_DisableAOTInstanceFieldResolution), "F"},
   {"disableAOTInstanceOfInlining",        "O\tdisable AOT instance of inlining",               SET_OPTION_BIT(TR_DisableAOTInstanceOfInlining), "F"},
   {"disableAOTResolutionPeeking",        "O\tdo not use resolved state at AOT compile time for performance decisions", SET_OPTION_BIT(TR_DisablePeekAOTResolutions), "F"},
   {"disableAOTResolveDiffCLMethods",     "O\tdo not resolve AOT methods from different class loaders", SET_OPTION_BIT(TR_DisableAOTResolveDiffCLMethods), "F"},
   {"disableAOTStaticField",               "O\tdisable AOT static field inlining",                      SET_OPTION_BIT(TR_DisableAOTStaticField), "F"},
   {"disableAOTValidationOpts",           "O\tdisable AOT optimizations with validations",                      SET_OPTION_BIT(TR_DisableAOTCheckCastInlining | TR_DisableAOTInstanceOfInlining | TR_DisableAOTInstanceFieldResolution | TR_DisableAOTStaticField), "F"},
   {"disableAOTWarmRunThroughputImprovement", "O\tdisable change iprofiler entry choosing heuristic to improve aot warm run throughput",                      SET_OPTION_BIT(TR_DisableAOTWarmRunThroughputImprovement), "F"},
   {"disableArch11PackedToDFP",           "O\tdisable arch(11) packed to DFP conversion instructions",            SET_OPTION_BIT(TR_DisableArch11PackedToDFP), "F",},
   {"disableArrayCopyOpts",               "O\tdisable array copy optimiations",                SET_OPTION_BIT(TR_DisableArrayCopyOpts), "F"},
   {"disableArraySetOpts",                "O\tdisable array set optimiations",                 SET_OPTION_BIT(TR_DisableArraySetOpts), "F"},
   {"disableArraySetStoreElimination",     "O\tdisable arrayset store elimination",                SET_OPTION_BIT(TR_DisableArraysetStoreElimination), "F"},
   {"disableArrayStoreCheckOpts",          "O\tdisable array store check optimizations",SET_OPTION_BIT(TR_DisableArrayStoreCheckOpts), "F"},
   {"disableAsyncCheckInsertion",         "O\tdisable insertion of async checks in large loopless methods", TR::Options::disableOptimization, asyncCheckInsertion, 0, "F" },
   {"disableAsyncCheckVersioning",        "O\tdisable versioning of loops wrt async checks",   SET_OPTION_BIT(TR_DisableAsyncCheckVersioning), "F"},
   {"disableAsyncCompilation",            "M\tdisable asynchronous compilation",               SET_OPTION_BIT(TR_DisableAsyncCompilation), "F"},
   {"disableAutoSIMD",            "M\tdisable automatic vectorization of loops",               SET_OPTION_BIT(TR_DisableAutoSIMD), "F"},
   {"disableBasicBlockExtension",         "O\tdisable basic block extension",                  TR::Options::disableOptimization, basicBlockExtension, 0, "P"},
   {"disableBasicBlockPeepHole",          "O\tdisable basic blocks peepHole",                  SET_OPTION_BIT(TR_DisableBasicBlockPeepHole), "F"},
   {"disableBCDArithChildOrdering",       "O\tstress testing option -- do not reorder children of BCD arithmetic nodes", SET_OPTION_BIT(TR_DisableBCDArithChildOrdering), "F" },
   {"disableBCDOppTracing",               "O\tdisable tracing of various BCD perf opportunities", SET_OPTION_BIT(TR_DisableBCDOppTracing), "F"},

   {"disableBDLLVersioning",              "O\tdisable BigDecimal long lookaside versioning",   SET_OPTION_BIT(TR_DisableBDLLVersioning), "F"},
   {"disableBitOpcode",                   "O\tdisable converting calling bit operation java method to bitOpcode",   SET_OPTION_BIT(TR_DisableBitOpcode), "F"},
   {"disableBlockShuffling",              "O\tdisable random rearrangement of blocks",         TR::Options::disableOptimization, blockShuffling, 0, "P"},
   {"disableBlockSplitter",               "O\tdisable block splitter",                         TR::Options::disableOptimization, blockSplitter, 0, "P"},
   {"disableBlockVersioner",              "O\tdisable block versioner",                        SET_OPTION_BIT(TR_DisableBlockVersioner), "P"},
   {"disableBranchOnCount",               "O\tdisable branch on count instructions for s390",  SET_OPTION_BIT(TR_DisableBranchOnCount), "F"},
   {"disableBranchPreload",               "O\tdisable return branch preload",                  SET_OPTION_BIT(TR_DisableBranchPreload), "F"},
   {"disableCallConstUncommoning",        "O\tdisable uncommon call constant node phase",  SET_OPTION_BIT(TR_DisableCallConstUncommoning), "F"},
   {"disableCallGraphInlining",           "O\tdisable Interpreter Profiling based inlining and code size estimation",  SET_OPTION_BIT(TR_DisableCallGraphInlining), "P"},
   {"disableCatchBlockRemoval",           "O\tdisable catch block removal",                    TR::Options::disableOptimization, catchBlockRemoval, 0, "P"},
   {"disableCFGSimplification",           "O\tdisable Control Flow Graph simplification",      TR::Options::disableOptimization, CFGSimplification, 0, "P"},
   {"disableCheapWarmOpts",               "O\tenable cheap warm optimizations",               SET_OPTION_BIT(TR_DisableCheapWarmOpts), "F"},
   {"disableCheckcastAndProfiledGuardCoalescer", "O\tdisable checkcast and profiled guard  coalescion optimization ",   SET_OPTION_BIT(TR_DisableCheckcastAndProfiledGuardCoalescer), "F"},
   {"disableCHOpts",                      "O\tdisable CHTable based optimizations",            SET_OPTION_BIT(TR_DisableCHOpts), "F"},
   {"disableClearCodeCacheFullFlag",      "I\tdisable the re-enabling of full code cache when a method body is freed.", SET_OPTION_BIT(TR_DisableClearCodeCacheFullFlag),"F", NOT_IN_SUBSET},
   {"disableCodeCacheConsolidation",      "M\tdisable code cache consolidation", RESET_OPTION_BIT(TR_EnableCodeCacheConsolidation), "F", NOT_IN_SUBSET},
   {"disableCodeCacheReclamation",        "I\tdisable the freeing of compilations.", SET_OPTION_BIT(TR_DisableCodeCacheReclamation),"F", NOT_IN_SUBSET},
   {"disableCodeCacheSnippets",           "O\tdisable code cache snippets (e.g. allocation prefetch snippet) ", SET_OPTION_BIT(TR_DisableCodeCacheSnippets), "F"},
   {"disableColdBlockMarker",             "O\tdisable detection of cold blocks",               TR::Options::disableOptimization, coldBlockMarker, 0, "P"},
   {"disableColdBlockOutlining",          "O\tdisable outlining of cold blocks",               TR::Options::disableOptimization, coldBlockOutlining, 0, "P"},
   {"disableCompactLocals",               "O\tdisable compact locals",                         TR::Options::disableOptimization, compactLocals, 0, "P"},
   {"disableCompactNullChecks",           "O\tdisable compact null checks",                    TR::Options::disableOptimization, compactNullChecks, 0, "P"},
   {"disableCompareAndBranchInstruction", "O\tdisable compareAndBranch instruction",           SET_OPTION_BIT(TR_DisableCompareAndBranchInstruction), "F"},
   {"disableCompilationAfterDLT",         "O\tdisable queueing of normal compilation for method that has been DLT compiled.", SET_OPTION_BIT(TR_DisableCompilationAfterDLT), "F"},
   {"disableCompilationThread",           "M\tdisable compilation on a separate thread",       SET_OPTION_BIT(TR_DisableCompilationThread), "F"},
   {"disableConservativeColdInlining",   "O\tDo not be conservative with inlining at cold", SET_OPTION_BIT(TR_DisableConservativeColdInlining), "F" },
   {"disableConservativeHotRecompForServerMode", "R\tDo not be more conservative in server mode", SET_OPTION_BIT(TR_DisableConservativeHotRecompilationForServerMode), "F", NOT_IN_SUBSET},
   {"disableConservativeInlining",        "O\tDo not be conservative with inlining", SET_OPTION_BIT(TR_DisableConservativeInlining), "F" },
   {"disableConverterReducer",            "O\tdisable reuducing converters methods to intrinisic arrayTranslate",SET_OPTION_BIT(TR_DisableConverterReducer), "F"},
   {"disableCPUUtilization",              "M\tdisable tracking of cpu utilization", SET_OPTION_BIT(TR_DisableCPUUtilization), "F", NOT_IN_SUBSET},
   {"disableCrackedEdit",                 "O\tdisable cracking of edit/edit-and-mark", SET_OPTION_BIT(TR_DisableCrackedEditOptimization), "F" },
   {"disableCustomMethodHandleThunks",    "R\tdisable creation of custom invokeExact thunks for MethodHandles", SET_OPTION_BIT(TR_DisableCustomMethodHandleThunks), "F", NOT_IN_SUBSET},
   {"disableDAATrailingZeros",            "O\tdisable DAA trailing zero in byte array acceleration", SET_OPTION_BIT(TR_DisableDAATrailingZero), "F"},
   {"disableDataCacheReclamation",        "I\tdisable the reaping of data caches when they are no longer needed.", SET_OPTION_BIT(TR_DisableDataCacheReclamation),"F", NOT_IN_SUBSET},
   {"disableDeadStoreBailOut",            "O\tdisable bail out of dead store", SET_OPTION_BIT(TR_DisableDeadStoreBailOut), "F"},
   {"disableDeadTreeElimination",         "O\tdisable dead tree elimination",                  TR::Options::disableOptimization, deadTreesElimination, 0, "P"},
   {"disableDecimalFormatPeephole",       "O\tdisable optimizing DecimalFormat.format(BigDecimal.doubleValue())", SET_OPTION_BIT(TR_DisableDecimalFormatPeephole), "P"},
   {"disableDelayRelocationForAOTCompilations", "M\tDo not relocate code for AOT compilations right away", SET_OPTION_BIT(TR_DisableDelayRelocationForAOTCompilations), "F" },
   {"disableDememoization",               "O\talways call \"memoizing\" getters (like Integer.valueOf) rather than having Escape Analysis turn them into noncontiguous stack allocations", SET_OPTION_BIT(TR_DisableDememoization), "F"},
   {"disableDirectMemoryOps",             "O\tdisable generation of direct memory instructions",SET_OPTION_BIT(TR_DisableDirectMemoryOps), "F"},
   {"disableDirectStaticAccessOnZ",  "O\tsupport relative load instructions for c and c++", SET_OPTION_BIT(TR_DisableDirectStaticAccessOnZ), "F"},
   {"disableDirectToJNI",                 "O\tdisable all JNI linkage dispatch sequences including thunks", SET_OPTION_BIT(TR_DisableDirectToJNI), "F"},
   {"disableDirectToJNIInline",           "O\tdisable direct calls to JNI methods from jitted methods (but still create thunks)", SET_OPTION_BIT(TR_DisableDirectToJNIInline), "F"},
   {"disableDLTBytecodeIndex=",           "O<nnn>\tprevent DLT compilation at the specified bytecode index", TR::Options::set32BitNumeric, offsetof(OMR::Options,_disableDLTBytecodeIndex), 0, " %d"},
   {"disableDLTrecompilationPrevention",  "M\tdisable the prevention of DLT bogus recompilations", SET_OPTION_BIT(TR_DisableDLTrecompilationPrevention), "F", NOT_IN_SUBSET},
   {"disableDoubleWordStackAlignment",    "O\tdisable double word java stack alignement on z", SET_OPTION_BIT(TR_DisableDoubleWordStackAlignment), "F"},
   {"disableDowngradeToColdOnVMPhaseStartup", "M\tdisable downgrading optLevel to cold during STARTUP VM phase", SET_OPTION_BIT( TR_DisableDowngradeToColdOnVMPhaseStartup), "F", NOT_IN_SUBSET},
   {"disableDualTLH",                     "D\tDisable use of non-zero initialized TLH", SET_OPTION_BIT(TR_DisableDualTLH), "F"},
   {"disableDumpFlowGraph",               "L\tDisable dumping of the flow graph into trace file", SET_OPTION_BIT(TR_DisableDumpFlowGraph), "P"},
   {"disableDynamicLoopTransfer",         "O\tdisable dynamic loop transfer",                  SET_OPTION_BIT(TR_DisableDynamicLoopTransfer), "F"},
   {"disableDynamicRIBufferProcessing",   "O\tprevent disabling buffer processing", SET_OPTION_BIT(TR_DisableDynamicRIBufferProcessing), "F", NOT_IN_SUBSET},
   {"disableDynamicSamplingWindow",       "M\t", SET_OPTION_BIT(TR_DisableDynamicSamplingWindow), "F", NOT_IN_SUBSET},
   {"disableEarlyCompilationDuringIdleCpu","M\t", RESET_OPTION_BIT(TR_EnableEarlyCompilationDuringIdleCpu), "F", NOT_IN_SUBSET},
   {"disableEDO",                         "O\tdisable exception directed optimizations",       SET_OPTION_BIT(TR_DisableEDO), "F"},
   {"disableEmptyPreHeaderCheck",         "O\tdisable Empty pre-header check in loop canonicalization", SET_OPTION_BIT(TR_DisableEmptyPreHeaderCheck), "F"},
   {"disableEnhancedClobberEval",         "O\tdisable passthrough clobber eval",               SET_OPTION_BIT(TR_DisableEnhancedClobberEval), "F"},
   {"disableEscapeAnalysis",              "O\tdisable escape analysis",                        TR::Options::disableOptimization, escapeAnalysis, 0, "P"},
   {"disableExitExtraction",              "O\tdisable extraction of structure nodes that unconditionally exit to outer regions", SET_OPTION_BIT(TR_DisableExitExtraction), "F"},
   {"disableExplicitNewInitialization",   "O\tdisable explicit new initialization",            TR::Options::disableOptimization, explicitNewInitialization, 0, "P"},
   {"disableFastAssumptionReclamation",   "O\tdisable fast assumption reclamation",            SET_OPTION_BIT(TR_DisableFastAssumptionReclamation), "F", NOT_IN_SUBSET},
   {"disableFastDLTOnLongRunningInterpreter",   "O\tdisable logic to trigger DLT when a compiled body exists, but we're receiving interpreter ticks",
                                                 SET_OPTION_BIT(TR_DisableFastDLTOnLongRunningInterpreter), "F", NOT_IN_SUBSET},
   {DisableFastStringIndexOfString,       "O\tdisable fast String.indexOf",                    SET_OPTION_BIT(TR_DisableFastStringIndexOf), "F"},
   {"disableFieldPrivatization",          "O\tdisable field privatization",                    TR::Options::disableOptimization, fieldPrivatization, 0, "P"},
   {"disableForcedEXInlining",            "O\tdisable forced EX inlining",                     SET_OPTION_BIT(TR_DisableForcedEXInlining), "F"},
   {"disableFPCodeGen",                   "O\tdisable floating point code generation",               SET_OPTION_BIT(TR_DisableFPCodeGen), "F"},
   {"disableFPE",                         "C\tdisable FPE",                                    SET_OPTION_BIT(TR_DisableFPE), "F"},
   {"disableGCRPatching",                 "R\tdisable patching of the GCR guard",              RESET_OPTION_BIT(TR_EnableGCRPatching), "F"},
   {"disableGlobalCopyPropagation",       "O\tdisable global copy propagation",                TR::Options::disableOptimization, globalCopyPropagation, 0, "P"},
   {"disableGlobalDSE",                   "O\tdisable global dead store elimination",          TR::Options::disableOptimization, globalDeadStoreElimination, 0, "P"},
   {"disableGlobalLiveVariablesForGC",    "O\tdisable global live variables for GC",           TR::Options::disableOptimization, globalLiveVariablesForGC, 0, "P"},
   {"disableGlobalStaticBaseRegister",    "O\tdisable global static base register ",           SET_OPTION_BIT(TR_DisableGlobalStaticBaseRegister), "F"},
   {"disableGlobalVP",                    "O\tdisable global value propagation",               TR::Options::disableOptimization, globalValuePropagation, 0, "P"},
   {"disableGLU",                         "O\tdisable general loop unroller",                  TR::Options::disableOptimization, generalLoopUnroller, 0, "P"},

   {"disableGLUColdRedirection",          "O\tdisable general loop unroller redirection of cold edges to loop header", SET_OPTION_BIT(TR_DisableGLUColdRedirection), "F"},
   {"disableGRA",                         "O\tdisable IL based global register allocator",     TR::Options::disableOptimization, tacticalGlobalRegisterAllocator, 0, "P"},
   {"disableGRACostBenefitModel",         "O\tdisable GRA cost/benefit model",                 RESET_OPTION_BIT(TR_EnableGRACostBenefitModel), "F" },
   {"disableGuardedCallArgumentRemat",    "O\tdon't rematerialize a guarded virtual call's arguments on the cold path; instead, leave the expressions on the mainline path", SET_OPTION_BIT(TR_DisableGuardedCallArgumentRemat), "F"},
   {"disableGuardedCountingRecompilation","O\tdisable GCR.  If you don't know what that is, I don't have room to explain it here.", SET_OPTION_BIT(TR_DisableGuardedCountingRecompilations), "F"},
   {"disableGuardedCountingRecompilations","O\tdeprecated.  Same as disableGuardedCountingRecompilation", SET_OPTION_BIT(TR_DisableGuardedCountingRecompilations), "F"},
   {"disableHalfSlotSpills",              "O\tdisable sharing of a single 8-byte spill temp for two 4-byte values",  SET_OPTION_BIT(TR_DisableHalfSlotSpills), "P"},
   {"disableHardwareProfilerDataCollection",    "O\tdisable the collection of hardware profiler information while maintaining the framework", SET_OPTION_BIT(TR_DisableHWProfilerDataCollection), "F", NOT_IN_SUBSET},
   {"disableHardwareProfilerDuringStartup", "O\tdisable hardware profiler during startup", SET_OPTION_BIT(TR_DisableHardwareProfilerDuringStartup), "F", NOT_IN_SUBSET},
   {"disableHardwareProfileRecompilation","O\tdisable hardware profile recompilation", RESET_OPTION_BIT(TR_EnableHardwareProfileRecompilation), "F", NOT_IN_SUBSET},
   {"disableHardwareProfilerReducedWarm", "O\tdisable hardware profiler reduced warm recompilation", SET_OPTION_BIT(TR_DisableHardwareProfilerReducedWarm), "F", NOT_IN_SUBSET},
   {"disableHardwareProfilerReducedWarmUpgrades", "O\tdisable hardware profiler reduced warm recompilation Upgrades", SET_OPTION_BIT(TR_DisableHardwareProfilerReducedWarmUpgrades), "F", NOT_IN_SUBSET},
   {"disableHardwareProfilingThread",     "O\tdo not create a separate thread for hardware profiling", SET_OPTION_BIT(TR_DisableHWProfilerThread), "F", NOT_IN_SUBSET},
   {"disableHeapAllocOOL",                "O\tdisable heap alloc OOL and replace with heap alloc snippet",  SET_OPTION_BIT(TR_DisableHeapAllocOOL), "F"},
   {"disableHierarchyInlining",           "O\tdisable inlining of overridden methods not overridden in subclass of the type of this pointer",  SET_OPTION_BIT(TR_DisableHierarchyInlining), "F"},
   {"disableHighWordRA",                  "O\tdisable High Word register allocations on z196 or newer", SET_OPTION_BIT(TR_DisableHighWordRA), "F"},
   {"disableHPRSpill",                    "O\tdisable spilling 31-bit values into High Word registers on z196 or newer", SET_OPTION_BIT(TR_DisableHPRSpill), "F"},
   {"disableHPRUpgrade",                  "O\tdisable upgrading 31-bit instructions to use High Word registers on z196 or newer", SET_OPTION_BIT(TR_DisableHPRUpgrade), "F"},
   {"disableHWAcceleratedStringCaseConv", "O\tdisable SIMD case conversion for toUpperCase and toLowerCase in Java", SET_OPTION_BIT(TR_DisableSIMDStringCaseConv), "F"},
#ifdef J9_PROJECT_SPECIFIC
   {"disableIdiomPatterns=",              "I{regex}\tlist of idiom patterns to disable",
                                          TR::Options::setRegex, offsetof(OMR::Options, _disabledIdiomPatterns), 0, "P"},
   {"disableIdiomRecognition",            "O\tdisable idiom recognition",                       TR::Options::disableOptimization, idiomRecognition, 0, "P"},
#endif
   {"disableIncrementalCCR",              "O\tdisable incremental ccr",      SET_OPTION_BIT(TR_DisableIncrementalCCR), "F" ,NOT_IN_SUBSET},

   {DisableInlineCheckCastString,         "O\tdisable CheckCast    inline fast helper",        SET_OPTION_BIT(TR_DisableInlineCheckCast)   , "F"},
   {"disableInlineCheckIfFinalizeObject", "M\tdisable CheckIfFinalizeObject inline helper",               SET_OPTION_BIT(TR_DisableInlineCheckIfFinalizeObject), "F"},
   {"disableInlineEXTarget",              "O\tdisable inlining of EX target for arraycopy and arraycmp",      SET_OPTION_BIT(TR_DisableInlineEXTarget), "F"},
   {DisableInlineIfInstanceOfString,      "O\tdisable IfInstanceOf inline fast helper",        SET_OPTION_BIT(TR_DisableInlineIfInstanceOf), "F"},
   {DisableInlineInstanceOfString,        "O\tdisable InstanceOf   inline fast helper",        SET_OPTION_BIT(TR_DisableInlineInstanceOf)  , "F"},
   {"disableInlineIsInstance",            "O\tdisable isInstance   inline fast helper",        SET_OPTION_BIT(TR_DisableInlineIsInstance)  , "F"},
   {DisableInlineMonEntString,            "O\tdisable MonEnt       inline fast helper",        SET_OPTION_BIT(TR_DisableInlineMonEnt)      , "F"},
   {DisableInlineMonExitString,           "O\tdisable MonExit      inline fast helper",        SET_OPTION_BIT(TR_DisableInlineMonExit)     , "F"},
   {"disableInlinerArgsPropagation",       "O\tenable argument propagation in inliner",      SET_OPTION_BIT(TR_DisableInlinerArgsPropagation), "F"},
   {"disableInlinerFanIn",                 "O\tdisable fan in as a consideration for inlining",      SET_OPTION_BIT(TR_DisableInlinerFanIn), "F"},
   {"disableInlineSites=",                "O{regex}\tlist of inlined sites to disable",
                                          TR::Options::setRegex, offsetof(OMR::Options, _disabledInlineSites), 0, "P"},
   {"disableInlineWriteBarriersRT",       "O\tdisable write barrier inline fast helper for real-time", SET_OPTION_BIT(TR_DisableInlineWriteBarriersRT) , "F"},
   {"disableInlining",                    "O\tdisable IL inlining",                            TR::Options::disableOptimization, inlining, 0, "P"},
   {"disableInliningDuringVPAtWarm",       "O\tdisable inlining during VP for warm bodies",    SET_OPTION_BIT(TR_DisableInliningDuringVPAtWarm), "F"},
   {DisableInliningOfNativesString,       "O\tdisable inlining of natives",                    SET_OPTION_BIT(TR_DisableInliningOfNatives), "F"},
   {"disableInnerPreexistence",           "O\tdisable inner preexistence",                     TR::Options::disableOptimization, innerPreexistence, 0, "P"},
   {"disableIntegerCompareSimplification",      "O\tdisable byte/short/int/long compare simplification  ",      SET_OPTION_BIT(TR_DisableIntegerCompareSimplification), "F"},
   {"disableInterfaceCallCaching",                          "O\tdisable interfaceCall caching   ",      SET_OPTION_BIT(TR_disableInterfaceCallCaching), "F"},
   {"disableInterfaceInlining",           "O\tdisable merge new",                              SET_OPTION_BIT(TR_DisableInterfaceInlining), "F"},
   {"disableInternalPointers",            "O\tdisable internal pointer creation",              SET_OPTION_BIT(TR_DisableInternalPointers), "F"},
   {"disableInterpreterProfiling",        "O\tdisable Interpreter Profiling hooks   ",         SET_OPTION_BIT(TR_DisableInterpreterProfiling), "F"},
   {"disableInterpreterProfilingThread",  "O\tdo not create a separate thread for interpreter profiling", SET_OPTION_BIT(TR_DisableIProfilerThread), "F", NOT_IN_SUBSET},
   {"disableInterpreterSampling",         "O\tdisable sampling of interpreted methods",        SET_OPTION_BIT(TR_DisableInterpreterSampling), "F"},
   {"disableIntrinsics",                   "O\tdisable inlining of packed decimal intrinsic functions", SET_OPTION_BIT(TR_DisableIntrinsics), "F"},
   {"disableInvariantArgumentPreexistence", "O\tdisable invariable argument preexistence",     TR::Options::disableOptimization, invariantArgumentPreexistence, 0, "P"},
   {"disableInvariantCodeMotion",           "O\tdisable invariant code motion.", SET_OPTION_BIT(TR_DisableInvariantCodeMotion), "P"},
   {"disableIPA",                          "O\tdisable inter procedural analysis.", SET_OPTION_BIT(TR_DisableIPA), "P"},
   {"disableIprofilerDataCollection",    "M\tdisables the collection of iprofiler information while maintaining the framework", SET_OPTION_BIT(TR_DisableIProfilerDataCollection), "F", NOT_IN_SUBSET},
   {"disableIprofilerDataPersistence",     "M\tdisable storage of iprofile information in the shared cache", SET_OPTION_BIT(TR_DisablePersistIProfile), "F"},
   {"disableIsolatedSE",                  "O\tdisable isolated store elimination",             TR::Options::disableOptimization, isolatedStoreElimination, 0, "P"},
   {"disableIterativeSA",       "O\trevert back to a recursive version of Structural Analysis", SET_OPTION_BIT(TR_DisableIterativeSA), "P"},
   {"disableIVTT",                        "O\tdisable IV Type transformation",                 TR::Options::disableOptimization, IVTypeTransformation, 0, "P"},
   {"disableJavaEightStartupHeuristics", "M\t", SET_OPTION_BIT(TR_DisableJava8StartupHeuristics), "F", NOT_IN_SUBSET },
   {"disableJProfiling",                  "O\tdisable JProfiling", RESET_OPTION_BIT(TR_EnableJProfiling), "F"},
   {"disableJProfilingThread",            "O\tdisable separate thread for JProfiling", SET_OPTION_BIT(TR_DisableJProfilerThread), "F", NOT_IN_SUBSET},
   {"disableKnownObjectTable",            "O\tdisable support for including heap object info in symbol references", SET_OPTION_BIT(TR_DisableKnownObjectTable), "F"},
   {"disableLastITableCache",             "C\tdisable using class lastITable cache for interface dispatches",  SET_OPTION_BIT(TR_DisableLastITableCache), "F"},
   {"disableLateEdgeSplitting",           "C\tconservatively add regdeps for the vmthread on any edge that might need it",  SET_OPTION_BIT(TR_DisableLateEdgeSplitting), "F"},
   {"disableLeafRoutineDetection",        "O\tdisable lleaf routine detection on zlinux", SET_OPTION_BIT(TR_DisableLeafRoutineDetection), "F"},
   {"disableLinkageRegisterAllocation",   "O\tdon't turn parm loads into RegLoads in first basic block",  SET_OPTION_BIT(TR_DisableLinkageRegisterAllocation), "F"},
   {"disableLiveMonitorMetadata",         "O\tdisable the creation of live monitor metadata", SET_OPTION_BIT(TR_DisableLiveMonitorMetadata), "F"},
   {"disableLiveRangeSplitter",          "O\tdisable live range splitter",                    SET_OPTION_BIT(TR_DisableLiveRangeSplitter), "F"},
   {"disableLocalCSE",                    "O\tdisable local common subexpression elimination", TR::Options::disableOptimization, localCSE, 0, "P"},
   {"disableLocalCSEVolatileCommoning",   "O\tdisable local common subexpression elimination volatile commoning", SET_OPTION_BIT(TR_DisableLocalCSEVolatileCommoning), "F"},
   {"disableLocalDSE",                    "O\tdisable local dead store elimination",           TR::Options::disableOptimization, localDeadStoreElimination, 0, "P"},
   {"disableLocalLiveRangeReduction",      "O\tdisable local live range reduction",            TR::Options::disableOptimization, localLiveRangeReduction, 0,"P"},
   {"disableLocalLiveVariablesForGC",     "O\tdisable local live variables for GC",            TR::Options::disableOptimization, localLiveVariablesForGC, 0, "P"},
   {"disableLocalReordering",             "O\tdisable local reordering",                       TR::Options::disableOptimization, localReordering, 0, "P"},
   {"disableLocalVP",                     "O\tdisable local value propagation",                TR::Options::disableOptimization, localValuePropagation, 0, "P"},
   {"disableLocalVPSkipLowFreqBlock",     "O\tDo not skip processing of low frequency blocks in localVP", RESET_OPTION_BIT(TR_EnableLocalVPSkipLowFreqBlock), "F" },
   {"disableLockReservation",             "O\tdisable lock reservation",                       SET_OPTION_BIT(TR_DisableLockResevation), "F"},
   {"disableLongDispNodes",               "C\tdisable 390 long-displacement fixup using TR Instructions", SET_OPTION_BIT(TR_DisableLongDispNodes), "F"},
   {"disableLongDispStackSlot",           "O\tdisable use of stack slot for handling long displacements on 390", SET_OPTION_BIT(TR_DisableLongDispStackSlot), "F"},
   // For PLX debug use
   {"disableLongRegAllocation",           "O\tdisable allocation of 64-bit regs on 32-bit",    SET_OPTION_BIT(TR_Disable64BitRegsOn32Bit), "F"},
   {"disableLongRegAllocationHeuristic",  "O\tdisable heuristic for long register allocation", SET_OPTION_BIT(TR_Disable64BitRegsOn32BitHeuristic), "F"},
   {"disableLookahead",                   "O\tdisable class lookahead",                        SET_OPTION_BIT(TR_DisableLookahead), "P"},
   {"disableLoopAliasRefiner",            "O\tdisable loop alias refinement",                         TR::Options::disableOptimization, loopAliasRefiner, 0, "P"},
   {"disableLoopCanonicalization",        "O\tdisable loop canonicalization",                  TR::Options::disableOptimization, loopCanonicalization, 0, "P"},
   {"disableLoopEntryAlignment",            "O\tdisable loop Entry alignment",                          SET_OPTION_BIT(TR_DisableLoopEntryAlignment), "F"},
   {"disableLoopInversion",               "O\tdisable loop inversion",                         TR::Options::disableOptimization, loopInversion, 0, "P"},
   {"disableLoopReduction",               "O\tdisable loop reduction",                         TR::Options::disableOptimization, loopReduction, 0, "P"},
   {"disableLoopReplicator",              "O\tdisable loop replicator",                        TR::Options::disableOptimization, loopReplicator, 0, "P"},
   {"disableLoopReplicatorColdSideEntryCheck","I\tdisable cold side-entry check for replicating loops containing hot inner loops", SET_OPTION_BIT(TR_DisableLoopReplicatorColdSideEntryCheck), "P"},
   {"disableLoopStrider",                 "O\tdisable loop strider",                           TR::Options::disableOptimization, loopStrider, 0, "P"},
   {"disableLoopTransfer",                "O\tdisable the loop transfer part of loop versioner", SET_OPTION_BIT(TR_DisableLoopTransfer), "F"},
   {"disableLoopVersioner",               "O\tdisable loop versioner",                         TR::Options::disableOptimization, loopVersioner, 0, "P"},
   {"disableMarkingOfHotFields",          "O\tdisable marking of Hot Fields",                  SET_OPTION_BIT(TR_DisableMarkingOfHotFields), "F"},
   {"disableMarshallingIntrinsics",       "O\tDisable packed decimal to binary marshalling and un-marshalling optimization. They will not be inlined.", SET_OPTION_BIT(TR_DisableMarshallingIntrinsics), "F"},
   {"disableMaskVFTPointers",             "O\tdisable masking of VFT Pointers",                SET_OPTION_BIT(TR_DisableMaskVFTPointers), "F"},
   {"disableMaxMinOptimization",          "O\tdisable masking of VFT Pointers",                SET_OPTION_BIT(TR_DisableMaxMinOptimization), "F"},
   {"disableMccFreeBlockRecycling",       "O\tdo not reuse code cache freed blocks",           SET_OPTION_BIT(TR_DisableFreeCodeCacheBlockRecycling), "F", NOT_IN_SUBSET},
   {"disableMCSBypass",                   "O\tdisable allocating JNI global references to skip some levels of indirection when accessing a MutableCallSite's target MethodHandle in jitted code", SET_OPTION_BIT(TR_DisableMCSBypass), "F"},
   {"disableMergeNew",                    "O\tdisable merge new",                              SET_OPTION_BIT(TR_DisableMergeNew), "F"},
   {"disableMergeStackMaps",              "O\tdisable stack map merging",                      SET_OPTION_BIT(TR_DisableMergeStackMaps), "P"},
   {"disableMetadataReclamation",         "I\tdisable J9JITExceptionTable reclamation", SET_OPTION_BIT(TR_DisableMetadataReclamation), "F", NOT_IN_SUBSET},
   {"disableMethodHandleInvokeOpts",      "O\tdo not perform any special optimizations on calls to MethodHandle.invoke",   SET_OPTION_BIT(TR_DisableMethodHandleInvokeOpts), "F", NOT_IN_SUBSET},
   {"disableMethodHandleThunks",          "D\tdo not produce jitted bodies to accelerate JSR292 MethodHandle invocation",   SET_OPTION_BIT(TR_DisableMethodHandleThunks), "F", NOT_IN_SUBSET},
   {"disableMethodIsCold",                "O\tdo not use heuristics to determine whether whole methods are cold based on how many times they have been interpreted",   SET_OPTION_BIT(TR_DisableMethodIsCold), "F"},
   {"disableMHCustomizationLogicCalls",   "C\tdo not insert calls to MethodHandle.doCustomizationLogic for handle invocations outside of thunks", RESET_OPTION_BIT(TR_EnableMHCustomizationLogicCalls), "F"},
   {"disableMonitorCoarsening",           "O\tdisable monitor coarsening",                     SET_OPTION_BIT(TR_DisableMonitorCoarsening), "F"},
   {"disableMoreOpts",                    "O\tapply noOpt optimization level and disable codegen optimizations", TR::Options::disableMoreOpts, offsetof(OMR::Options, _optLevel), noOpt, "P"},
   {"disableMultiLeafArrayCopy",          "O\tdisable multi-leaf arraycopy for real-time",   SET_OPTION_BIT(TR_DisableMultiLeafArrayCopy), "F"},
   {"disableMultiTargetInlining",         "O\tdisable multi-target inlining",   SET_OPTION_BIT(TR_DisableMultiTargetInlining), "F"},
   {"disableMutableCallSiteGuards",       "O\tdisable virtual guards for calls to java.lang.invoke.MutableCallSite.getTarget().invokeExact(...) (including invokedynamic)",   SET_OPTION_BIT(TR_DisableMutableCallSiteGuards), "F"},
   {"disableNewBlockOrdering",            "O\tdisable new block ordering, instead use basic block extension", SET_OPTION_BIT(TR_DisableNewBlockOrdering), "P"},
   {"disableNewBVA",                      "O\tdisable structure based bit vector analysis",SET_OPTION_BIT(TR_DisableNewBVA), "F"},
   {"disableNewInliningInfrastructure",  "O\tdisable new inlining infrastructure ",        SET_OPTION_BIT(TR_DisableNewInliningInfrastructure), "F"},
   {DisableNewInstanceImplOptString,      "O\tdisable newInstanceImpl opt",                    SET_OPTION_BIT(TR_DisableNewInstanceImplOpt), "F"},
   {"disableNewLoopTransfer",             "O\tdisable loop transfer for virtual guards",       SET_OPTION_BIT(TR_DisableNewLoopTransfer), "F"},
   {"disableNewMethodOverride",           "O\tdisable replacement for jitUpdateInlineAttribute", SET_OPTION_BIT(TR_DisableNewMethodOverride), "F"},
   {"disableNewStoreHint",                "O\tdisable re-initializing BCD nodes to a new store hint when one is available", SET_OPTION_BIT(TR_DisableNewStoreHint), "F"},
   {"disableNewX86VolatileSupport",        "O\tdisable new X86 Volatile Support", SET_OPTION_BIT(TR_DisableNewX86VolatileSupport), "F"},
   {"disableNextGenHCR",                  "O\tdisable HCR implemented with on-stack replacement",  SET_OPTION_BIT(TR_DisableNextGenHCR), "F"},

   {"disableNonvirtualInlining",          "O\tdisable inlining of non virtual methods",        SET_OPTION_BIT(TR_DisableNonvirtualInlining), "F"},
   {"disableNopBreakpointGuard",          "O\tdisable nop of breakpoint guards",        SET_OPTION_BIT(TR_DisableNopBreakpointGuard), "F"},
   {"disableNoServerDuringStartup",       "M\tDo not use NoServer during startup",  SET_OPTION_BIT(TR_DisableNoServerDuringStartup), "F"},
   {"disableNoVMAccess",                  "O\tdisable compilation without holding VM access",  SET_OPTION_BIT(TR_DisableNoVMAccess), "F"},
   {"disableOnDemandLiteralPoolRegister", "O\tdisable on demand literal pool register",        SET_OPTION_BIT(TR_DisableOnDemandLiteralPoolRegister), "F"},
   {"disableOOL",                         "O\tdisable out of line instruction selection",      SET_OPTION_BIT(TR_DisableOOL), "F"},
   {"disableOpts=",                       "O{regex}\tlist of optimizations to disable",
                                          TR::Options::setRegex, offsetof(OMR::Options, _disabledOpts), 0, "P"},
   {"disableOptTransformations=",         "O{regex}\tlist of optimizer transformations to disable",
                                          TR::Options::setRegex, offsetof(OMR::Options, _disabledOptTransformations), 0, "P"},
   {"disableOSR",                         "O\tdisable support for on-stack replacement", SET_OPTION_BIT(TR_DisableOSR), "F"},
   {"disableOSRCallSiteRemat",            "O\tdisable use of the call stack remat table in on-stack replacement", SET_OPTION_BIT(TR_DisableOSRCallSiteRemat), "F"},
   {"disableOSRExceptionEdgeRemoval",     "O\tdon't trim away unused on-stack replacement points", TR::Options::disableOptimization, osrExceptionEdgeRemoval, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"disableOSRGuardRemoval",             "O\tdisable OSR guard removal",                      TR::Options::disableOptimization, osrGuardRemoval, 0, "P"},
#endif
   {"disableOSRLiveRangeAnalysis",        "O\tdisable live range analysis for on-stack replacement", SET_OPTION_BIT(TR_DisableOSRLiveRangeAnalysis), "F"},
   {"disableOSRLocalRemat",               "O\tdisable use of remat when inserting guards for on-stack replacement", SET_OPTION_BIT(TR_DisableOSRLocalRemat), "F"},
   {"disableOSRSharedSlots",              "O\tdisable support for shared slots in on-stack replacement", SET_OPTION_BIT(TR_DisableOSRSharedSlots), "F"},
   {"disableOutlinedNew",                 "O\tdo object allocation logic inline instead of using a fast jit helper",  SET_OPTION_BIT(TR_DisableOutlinedNew), "F"},
   {"disableOutlinedPrologues",           "O\tdo all prologue logic in-line",      RESET_OPTION_BIT(TR_EnableOutlinedPrologues), "F"},
   {"disablePackedDecimalIntrinsics",     "O\tDisables packed decimal function optimizations and avoid generating exception triggering packed decimal instructions on z/Architecture.", SET_OPTION_BIT(TR_DisablePackedDecimalIntrinsics), "F"},
   {"disablePackedLongConversionOn32Bit", "O\t prevent pd2l/l2pd/pd2lu/lu2pd conversions from using the 64 bit IL (instead call out or use 32 bit operations)", SET_OPTION_BIT(TR_DisablePackedLongConversion), "F"},
   {"disablePartialInlining",             "O\tdisable  partial Inlining ",        SET_OPTION_BIT(TR_DisablePartialInlining), "F"},
   {"disablePostProfileCompPriorityBoost","M\tdisable boosting the priority of post profiling compilations",  SET_OPTION_BIT(TR_DisablePostProfileCompPriorityBoost), "F"},
   {"disablePRBE",                        "O\tdisable partial redundancy branch elimination",  SET_OPTION_BIT(TR_DisablePRBE), "F"},
   {"disablePRE",                         "O\tdisable partial redundancy elimination",         TR::Options::disableOptimization, partialRedundancyElimination, 0, "P"},
   {"disablePreexistenceDuringGracePeriod","O\tdisable preexistence during CLP grace period",  SET_OPTION_BIT(TR_DisablePrexistenceDuringGracePeriod), "F"},
   {"disablePrefetchInsertion",           "O\tdisable prefetch insertion",                     TR::Options::disableOptimization, prefetchInsertion, 0, "P"},
   {"disableProfiledInlining",            "O\tdisable inlining based on profiled this values", SET_OPTION_BIT(TR_DisableProfiledInlining), "F"},
   {"disableProfiledMethodInlining",      "O\tdisable inlining based on profiled methods", SET_OPTION_BIT(TR_DisableProfiledMethodInlining), "F"},
   {"disableProfiledNodeVersioning",      "O\tdisable profiled node versioning",               TR::Options::disableOptimization, profiledNodeVersioning, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"disableProfileGenerator",            "O\tdisable profile generator",                      TR::Options::disableOptimization, profileGenerator, 0, "P"},
#endif
   {"disableProfiling",                   "O\tdisable profiling",                              SET_OPTION_BIT(TR_DisableProfiling), "P"},
   {"disableProfilingDataReclamation",    "O\tdisable reclamation for profiling data",         SET_OPTION_BIT(TR_DisableProfilingDataReclamation), "F", NOT_IN_SUBSET},
   {"disableProloguePushes",              "O\tuse stores instead of pushes in x86 prologues",  SET_OPTION_BIT(TR_DisableProloguePushes), "P"},
   {"disableRampupImprovements",          "M\tDisable various changes that improve rampup",    SET_OPTION_BIT(TR_DisableRampupImprovements), "F", NOT_IN_SUBSET},
   {"disableReadMonitors",                 "O\tdisable read monitors",                         SET_OPTION_BIT(TR_DisableReadMonitors), "F"},
   {"disableRecognizedCallTransformer",    "O\tdisable recognized call transformer",           TR::Options::disableOptimization, recognizedCallTransformer, 0, "P"},
   {"disableRecognizedMethods",            "O\tdisable recognized methods",                    SET_OPTION_BIT(TR_DisableRecognizedMethods), "F"},
   {"disableReducedPriorityForCustomMethodHandleThunks",  "R\tcompile custom MethodHandle invoke exact thunks at the same priority as normal java methods", SET_OPTION_BIT(TR_DisableReducedPriorityForCustomMethodHandleThunks), "F", NOT_IN_SUBSET},
   {"disableRedundantAsyncCheckRemoval",  "O\tdisable redundant async check removal",          TR::Options::disableOptimization, redundantAsyncCheckRemoval, 0, "P"},
   {"disableRedundantBCDSignElimination",  "I\tdisable redundant BCD sign elimination", SET_OPTION_BIT(TR_DisableRedundantBCDSignElimination),"F"},
   {"disableRedundantGotoElimination",    "O\tdisable redundant goto elimination",             TR::Options::disableOptimization, redundantGotoElimination, 0, "P"},
   {"disableRedundantMonitorElimination", "O\tdisable redundant monitor elimination",          TR::Options::disableOptimization, redundantMonitorElimination, 0, "P"},
   {"disableRefArraycopyRT",              "O\tdisable reference arraycopy for real-time gc",   SET_OPTION_BIT(TR_DisableRefArraycopyRT), "F"},
   {"disableRefinedAliases",              "O\tdisable collecting side-effect summaries from compilations to improve aliasing info in subsequent compilations", SET_OPTION_BIT(TR_DisableRefinedAliases), "F"},
   {"disableRefinedBCDClobberEval",       "O\tdisable trying to minimize the number of BCD clobber evaluate copies ", SET_OPTION_BIT(TR_DisableRefinedBCDClobberEval), "F"},
   {"disableRegDepCopyRemoval",           "O\tdisable register dependency copy removal", TR::Options::disableOptimization, regDepCopyRemoval, 0, "P"},
   {"disableRegisterPressureSimulation",  "O\tdon't walk the trees to estimate register pressure during global register allocation", SET_OPTION_BIT(TR_DisableRegisterPressureSimulation), "F"},
   {"disableRematerialization",           "O\tdisable rematerialization",                      TR::Options::disableOptimization, rematerialization, 0, "P"},
   {"disableReorderArrayIndexExpr",       "O\tdisable reordering of index expressions",        TR::Options::disableOptimization, reorderArrayExprGroup, 0, "P"},
   {"disableRMODE64",                     "O\tDisable residence mode of compiled bodies on z/OS to reside above the 2-gigabyte bar", RESET_OPTION_BIT(TR_EnableRMODE64), "F"},
   {"disableRXusage",                     "O\tdisable increased usage of RX instructions",     SET_OPTION_BIT(TR_DisableRXusage), "F"},
   {"disableSamplingJProfiling",          "O\tDisable profiling in the jitted code", SET_OPTION_BIT(TR_DisableSamplingJProfiling), "F" },
   {"disableScorchingSampleThresholdScalingBasedOnNumProc", "M\t", SET_OPTION_BIT(TR_DisableScorchingSampleThresholdScalingBasedOnNumProc), "F", NOT_IN_SUBSET},
   {"disableSelectiveNoServer",           "D\tDisable turning on noServer selectively",        SET_OPTION_BIT(TR_DisableSelectiveNoOptServer), "F" },
   {"disableSeparateInitFromAlloc",        "O\tdisable separating init from alloc",            SET_OPTION_BIT(TR_DisableSeparateInitFromAlloc), "F"},
   {"disableSequenceSimplification",      "O\tdisable arithmetic sequence simplification",     TR::Options::disableOptimization, expressionsSimplification, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"disableSequentialStoreSimplification","O\tdisable sequential store simplification phase", TR::Options::disableOptimization, sequentialStoreSimplification, 0, "P"},
#endif
   {"disableShareableMethodHandleThunks",  "R\tdisable creation of shareable invokeExact thunks for MethodHandles", SET_OPTION_BIT(TR_DisableShareableMethodHandleThunks), "F", NOT_IN_SUBSET},
   {"disableSharedCacheHints",             "R\tdisable storing and loading hints from shared cache", SET_OPTION_BIT(TR_DisableSharedCacheHints), "F"},
   {"disableShrinkWrapping",               "M\tdisable shrink wrapping of callee saved registers", SET_OPTION_BIT(TR_DisableShrinkWrapping), "F"},
   {"disableSIMD",                         "O\tdisable SIMD exploitation and infrastructure on platforms supporting vector register and instructions", SET_OPTION_BIT(TR_DisableSIMD), "F"},
   {"disableSIMDArrayCompare",            "O\tDisable vectorized array comparison using SIMD instruction", SET_OPTION_BIT(TR_DisableSIMDArrayCompare), "F"},
   {"disableSIMDArrayCopy",                "O\tDisable vectorized array copying using SIMD instruction", SET_OPTION_BIT(TR_DisableSIMDArrayCopy), "F"},
   {"disableSIMDArrayTranslate",           "O\tdisable SIMD instructions for array translate", SET_OPTION_BIT(TR_DisableSIMDArrayTranslate), "F"},
   {"disableSIMDDoubleMaxMin",             "O\tdisable SIMD instructions for double max min", SET_OPTION_BIT(TR_DisableSIMDDoubleMaxMin), "F"},
   {"disableSIMDStringHashCode",           "O\tdisable vectorized java/lang/String.hashCode implementation", SET_OPTION_BIT(TR_DisableSIMDStringHashCode), "F"},
   {"disableSIMDUTF16BEEncoder",           "M\tdisable inlining of SIMD UTF16 Big Endian encoder", SET_OPTION_BIT(TR_DisableSIMDUTF16BEEncoder), "F"},
   {"disableSIMDUTF16LEEncoder",           "M\tdisable inlining of SIMD UTF16 Little Endian encoder", SET_OPTION_BIT(TR_DisableSIMDUTF16LEEncoder), "F"},
   {"disableSmartPlacementOfCodeCaches",   "O\tdisable placement of code caches in memory so they are near each other and the DLLs",  SET_OPTION_BIT(TR_DisableSmartPlacementOfCodeCaches), "F", NOT_IN_SUBSET},
   {"disableStoreAnchoring",              "O\tin trivialStoreSinking disable store child anchoring and therefore more aggressively duplicate trees",
                                          SET_OPTION_BIT(TR_DisableStoreAnchoring), "F"},
   {"disableStoreOnCondition",                 "O\tdisable store on condition (STOC) code gen",                         SET_OPTION_BIT(TR_DisableStoreOnCondition), "F"},
   {"disableStoreSinking",                 "O\tdisable store sinking",                         SET_OPTION_BIT(TR_DisableStoreSinking), "F"},
   {"disableStringBuilderTransformer",     "O\tenable transforming StringBuilder constructor to preallocate a buffer for String concatenation operations", SET_OPTION_BIT(TR_DisableStringBuilderTransformer), "F"},
   {"disableStringPeepholes",             "O\tdisable stringPeepholes",                        SET_OPTION_BIT(TR_DisableStringPeepholes), "F"},
   {"disableStripMining",                 "O\tdisable loop strip mining",                      SET_OPTION_BIT(TR_DisableStripMining), "F"},
   {"disableSuffixLogs",                  "O\tdo not add the date/time/pid suffix to the file name of the logs", RESET_OPTION_BIT(TR_EnablePIDExtension), "F"},
   {"disableSupportForCpuSpentInCompilation", "M\tdo not provide CPU spent in compilation",    SET_OPTION_BIT(TR_DisableSupportForCpuSpentInCompilation), "F" },
   {"disableSwitchAnalyzer",              "O\tdisable switch analyzer",                        TR::Options::disableOptimization, switchAnalyzer, 0, "P"},
   {"disableSwitchAwayFromProfilingForHotAndVeryhot", "O\tdisable switch away from profiling for hot and veryhot", SET_OPTION_BIT(TR_DisableSwitchAwayFromProfilingForHotAndVeryhot), "F"},
   {"disableSynchronizedFieldLoad",       "O\tDisable the use of hardware optimized synchronized field load intrinsics",         SET_OPTION_BIT(TR_DisableSynchronizedFieldLoad), "F"},
   {"disableSyncMethodInlining",          "O\tdisable inlining of synchronized methods",       SET_OPTION_BIT(TR_DisableSyncMethodInlining), "F"},
   {"disableTailRecursion",               "O\tdisable tail recursion",                         SET_OPTION_BIT(TR_DisableTailRecursion), "F"},
   {"disableTarokInlineArrayletAllocation", "O\tdisable Tarok inline Arraylet Allocation in genHeapAlloc", SET_OPTION_BIT(TR_DisableTarokInlineArrayletAllocation), "F"},
   {"disableThrowToGoto",                 "O\tdisable throw to goto",                          SET_OPTION_BIT(TR_DisableThrowToGoto), "F"},
   {"disableThunkTupleJ2I",               "D\tdo not replace initialInvokeExactThunk with J2I thunk / helper address in ThunkTuple",   SET_OPTION_BIT(TR_DisableThunkTupleJ2I), "F", NOT_IN_SUBSET},
   {"disableTLE",                         "O\tdisable transactional lock elision", SET_OPTION_BIT(TR_DisableTLE), "F"},
   {"disableTlhPrefetch",                 "O\tdisable software prefetch on allocation", SET_OPTION_BIT(TR_DisableTLHPrefetch), "F"},
   {"disableTM",                          "O\tdisable transactional memory support", SET_OPTION_BIT(TR_DisableTM), "F"},
   {"disableTOCForConsts",                "O\tdisable use of the TOC for constants and floats materialization", SET_OPTION_BIT(TR_DisableTOCForConsts), "F"},
   {"disableTraceRegDeps",                "O\tdisable printing of register dependancies for each instruction in trace file",              SET_OPTION_BIT(TR_DisableTraceRegDeps), "F"},
   {"disableTraps",                       "C\tdisable trap instructions",                       SET_OPTION_BIT(TR_DisableTraps), "F"},
   {"disableTreeCleansing",               "O\tdisable tree cleansing",                         TR::Options::disableOptimization, treesCleansing, 0, "P"},
   {"disableTreeSimplification",          "O\tdisable tree simplification",                    TR::Options::disableOptimization, treeSimplification, 0, "P"},
   {"disableTrivialBlockExtension",       "O\tdisable trivial block extension",                TR::Options::disableOptimization, trivialBlockExtension, 0, "P"},
   {"disableTrivialDeadBlockRemoval", "O\tdisable trivial dead block removal ",   SET_OPTION_BIT(TR_DisableTrivialDeadBlockRemover), "F"},
   {"disableTrivialDeadTreeRemoval",      "O\tdisable trivial dead tree removal",              TR::Options::disableOptimization, trivialDeadTreeRemoval, 0, "P"},
   {"disableTrivialStoreSinking",         "O\tdisable trivial store sinking", RESET_OPTION_BIT(TR_EnableTrivialStoreSinking), "F"},
   {"disableTrueRegisterModel",           "C\tdisable use of true liveness model in local RA instead of Future Use Count (Dangerous use unless no Global Virtual Regs used)", RESET_OPTION_BIT(TR_EnableTrueRegisterModel), "F"},
   {"disableUncountedUnrolls",            "O\tdisable GLU from unrolling uncoutned loops ",SET_OPTION_BIT(TR_DisableUncountedUnrolls), "F"},
   {"disableUnsafe",                      "O\tdisable code to inline Unsafe natives",          SET_OPTION_BIT(TR_DisableUnsafe), "F"},
   {"disableUnsafeFastPath",              "O\tdisable unsafe fast path",               TR::Options::disableOptimization, unsafeFastPath, 0, "P"},  // Java specific option
   {"disableUpdateAOTBytesSize",          "M\tDon't send VM size of bodies that could have been AOT'd if the SCC wasn't full", SET_OPTION_BIT(TR_DisableUpdateAOTBytesSize), "F", NOT_IN_SUBSET},
   {"disableUpdateJITBytesSize",          "M\tDon't send VM size of IProfiler Entires and Hints that could have been persisted if the SCC wasn't full", SET_OPTION_BIT(TR_DisableUpdateJITBytesSize), "F", NOT_IN_SUBSET},
   {"disableUpgradeBootstrapAtWarm",      "R\tRecompile bootstrap AOT methods at warm instead of cold", RESET_OPTION_BIT(TR_UpgradeBootstrapAtWarm), "F", NOT_IN_SUBSET},
   {"disableUpgrades",                    "O\tPrevent Jit Sampling from upgrading cold compilations",
                                          SET_OPTION_BIT(TR_DisableUpgrades), "F", NOT_IN_SUBSET},
   {"disableUpgradingColdCompilations",   "R\tdisable upgrading to warm those methods compiled at cold due to classLoadPhase", SET_OPTION_BIT(TR_DisableUpgradingColdCompilations), "F", NOT_IN_SUBSET},
   {"disableUseDefForShadows",            "I\ttemporary, disables usedef for shadows.", SET_OPTION_BIT(TR_DisableUseDefForShadows),"F"},
   {"disableUseRIOnlyForLargeQSZ",        "M\t", RESET_OPTION_BIT(TR_UseRIOnlyForLargeQSZ), "F", NOT_IN_SUBSET },
   {"disableUTF16BEEncoder",              "M\tdisable inlining of UTF16 Big Endian encoder", SET_OPTION_BIT(TR_DisableUTF16BEEncoder), "F"},
   {"disableValueProfiling",              "O\tdisable value profiling",                        SET_OPTION_BIT(TR_DisableValueProfiling), "F"},
   {"disableVariablePrecisionDAA",        "O\tdisable variable precision DAA optimizations",   SET_OPTION_BIT(TR_DisableVariablePrecisionDAA), "F"},
   {"disableVectorBCD",                   "O\tdisable vector instructions for DAA BCD intrinsics ", SET_OPTION_BIT(TR_DisableVectorBCD), "F"},
   {"disableVectorRegGRA",                "O\tdisable global register allocation for vector regs",   SET_OPTION_BIT(TR_DisableVectorRegGRA), "F"},
   {"disableVerification",                "O\tdisable verification of internal data structures between passes", SET_OPTION_BIT(TR_DisableVerification), "F"},
   {"disableVirtualGuardHeadMerger",      "O\tdisable virtual guard head merger",              TR::Options::disableOptimization, virtualGuardHeadMerger, 0, "P"},
   {DisableVirtualGuardNOPingString,      "O\tdisable virtual guard NOPing",                   SET_OPTION_BIT(TR_DisableVirtualGuardNOPing), "F"},
   {"disableVirtualGuardTailSplitter",    "O\tdisable virtual guard tail splitter",            TR::Options::disableOptimization, virtualGuardTailSplitter, 0, "P"},
   {"disableVirtualInlining",             "O\tdisable inlining of virtual methods",            SET_OPTION_BIT(TR_DisableVirtualInlining), "F"},
   {"disableVirtualScratchMemory",        "M\tdisable scratch memory to be allocated using virtual memory allocators",
                                          RESET_OPTION_BIT(TR_EnableVirtualScratchMemory), "F", NOT_IN_SUBSET},
   {"disableVMCSProfiling",               "O\tdisable VM data for virtual call sites", SET_OPTION_BIT(TR_DisableVMCSProfiling), "F", NOT_IN_SUBSET},
   {"disableVMThreadGRA",                 "O\tdisable reuse of the vmThread's real register as a global register", SET_OPTION_BIT(TR_DisableVMThreadGRA), "F"},
   {"disableVSSStackCompaction",          "O\tdisable VariableSizeSymbol stack compaction", SET_OPTION_BIT(TR_DisableVSSStackCompaction), "F"},
   {"disableWriteBarriersRangeCheck",     "O\tdisable adding range check to write barriers",   SET_OPTION_BIT(TR_DisableWriteBarriersRangeCheck), "F"},
   {"disableWrtBarSrcObjCheck",           "O\tdisable to not check srcObj location for wrtBar in gc", SET_OPTION_BIT(TR_DisableWrtBarSrcObjCheck), "F"},
   {"disableZ10",                         "O\tdisable z10 support",                            SET_OPTION_BIT(TR_DisableZ10), "F"},
   {"disableZ13",                         "O\tdisable z13 support",                        SET_OPTION_BIT(TR_DisableZ13), "F"},
   {"disableZ13LoadAndMask",              "O\tdisable load-and-mask instruction generation on z13",   SET_OPTION_BIT(TR_DisableZ13LoadAndMask), "F"},
   {"disableZ13LoadImmediateOnCond",      "O\tdisable load halfword immediate on condition instruction generation on z13",   SET_OPTION_BIT(TR_DisableZ13LoadImmediateOnCond), "F"},
   {"disableZ14",                         "O\tdisable z14 support",                            SET_OPTION_BIT(TR_DisableZ14), "F"},
   {"disableZ196",                        "O\tdisable z196 support",                           SET_OPTION_BIT(TR_DisableZ196), "F"},
   {"disableZArraySetUnroll",             "O\tdisable arraySet unrolling on 390.",             SET_OPTION_BIT(TR_DisableZArraySetUnroll), "F"},
   {"disableZealousCodegenOpts",          "O\tdisable use of zealous codegen optimizations.", SET_OPTION_BIT(TR_DisableZealousCodegenOpts), "F"},
   {"disableZHelix",                      "O\tdisable zEC12 support",                          SET_OPTION_BIT(TR_DisableZHelix), "F"},
   {"disableZImplicitNullChecks",         "O\tdisable implicit null checks on 390",            SET_OPTION_BIT(TR_DisableZImplicitNullChecks), "F"},
   {"disableZNext",                       "O\tdisable zNext support",                        SET_OPTION_BIT(TR_DisableZNext), "F"},
   {"disableZonedToDFPReduction",         "O\tdisable strength reduction of zoned decimal arithmetic to DFP arithmetic",   SET_OPTION_BIT(TR_DisableZonedToDFPReduction), "F"},
   {"dltMostOnce",                        "O\tprevent DLT compilation of a method at more than one bytecode index.", SET_OPTION_BIT(TR_DLTMostOnce), "F"},
   {"dltOptLevel=cold",                   "O\tforce DLT compilation at cold level",            TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), cold, "P"},
   {"dltOptLevel=hot",                    "O\tforce DLT compilation at hot level",             TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), hot, "P"},
   {"dltOptLevel=noOpt",                  "O\tforce DLT compilation at noOpt level",           TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), noOpt, "P"},
   {"dltOptLevel=scorching",              "O\tforce DLT compilation at scorching level",       TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), scorching, "P"},
   {"dltOptLevel=veryHot",                "O\tforce DLT compilation at veryHot level",         TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), veryHot, "P"},
   {"dltOptLevel=warm",                   "O\tforce DLT compilation at warm level",            TR::Options::set32BitValue, offsetof(OMR::Options, _dltOptLevel), warm, "P"},
   {"dontActivateCompThreadWhenHighPriReqIsBlocked", "M\tdo not activate another compilation thread when high priority request is blocked",  RESET_OPTION_BIT(TR_ActivateCompThreadWhenHighPriReqIsBlocked), "F", NOT_IN_SUBSET},
   {"dontAddHWPDataToIProfiler",          "O\tDont add HW Data to IProfiler", SET_OPTION_BIT(TR_DontAddHWPDataToIProfiler), "F", NOT_IN_SUBSET},
   {"dontDowngradeToCold",                "M\tdon't downgrade first time compilations from warm to cold", SET_OPTION_BIT(TR_DontDowngradeToCold), "F", NOT_IN_SUBSET},
   {"dontDowngradeWhenRIIsTemporarilyOff","M\t", SET_OPTION_BIT(TR_DontDowngradeWhenRIIsTemporarilyOff), "F", NOT_IN_SUBSET },
   {"dontIncreaseCountsForNonBootstrapMethods", "M\t", RESET_OPTION_BIT(TR_IncreaseCountsForNonBootstrapMethods), "F", NOT_IN_SUBSET }, // Xjit: option
   {"dontInline=",                        "O{regex}\tlist of callee methods to not inline",
                                          TR::Options::setRegex, offsetof(OMR::Options, _dontInline), 0, "P"},
   {"dontJitIfSlotsSharedByRefAndNonRef", "O\tfail the compilation (in FSD mode) if a slot needs to be shared between an address and a nonaddress.",     SET_OPTION_BIT(TR_DontJitIfSlotsSharedByRefAndNonRef), "F"},
   {"dontRestrictInlinerDuringStartup",   "O\tdo not restrict trivial inliner during startup", RESET_OPTION_BIT(TR_RestrictInlinerDuringStartup), "F", NOT_IN_SUBSET},
   {"dontRIUpgradeAOTWarmMethods",         "M\tdon't RI upgrade AOT warm methods", SET_OPTION_BIT(TR_DontRIUpgradeAOTWarmMethods), "F", NOT_IN_SUBSET},
   {"dontSuspendCompThreadsEarly",        "M\tDo not suspend compilation threads when QWeight drops under a threshold", RESET_OPTION_BIT(TR_SuspendEarly), "F", NOT_IN_SUBSET },
   {"dontTurnOffSelectiveNoOptServerIfNoStartupHint", "M\t", RESET_OPTION_BIT(TR_TurnOffSelectiveNoOptServerIfNoStartupHint), "F", NOT_IN_SUBSET },
   {"dontUseFastStackwalk",                "I\tDo not use accelerated stackwalking algorithm", SET_OPTION_BIT(TR_DoNotUseFastStackwalk), "P"},
   {"dontUseHigherCountsForNonSCCMethods", "M\tDo not use the default high counts for methods belonging to classes not in SCC", RESET_OPTION_BIT(TR_UseHigherCountsForNonSCCMethods), "F", NOT_IN_SUBSET },
   {"dontUseHigherMethodCountsAfterStartup", "M\tDo not use the default high counts for methods after startup in AOT mode", RESET_OPTION_BIT(TR_UseHigherMethodCountsAfterStartup), "F", NOT_IN_SUBSET },
   {"dontUseIdleTime",                    "M\tDo not use cpu idle time to compile more aggressively", RESET_OPTION_BIT(TR_UseIdleTime), "F", NOT_IN_SUBSET },
   {"dontUsePersistentIprofiler",         "M\tdon't use iprofiler data stored int he shared cache, even if it is available", SET_OPTION_BIT(TR_DoNotUsePersistentIprofiler), "F"},
   {"dontUseRIOnlyForLargeQSZ",           "M\tUse RI regardless of the compilation queue size", RESET_OPTION_BIT(TR_UseRIOnlyForLargeQSZ), "F", NOT_IN_SUBSET },
   {"dontVaryInlinerAggressivenessWithTime", "M\tDo not vary inliner aggressiveness with abstract time", RESET_OPTION_BIT(TR_VaryInlinerAggressivenessWithTime), "F", NOT_IN_SUBSET },
   {"dumbInlinerBytecodeSizeDivisor=",    "O<nnn>\thigher values will allow more inlining", TR::Options::set32BitNumeric, offsetof(OMR::Options,_dumbInlinerBytecodeSizeDivisor), 0, " %d"},
   {"dumbInlinerBytecodeSizeMaxCutoff=",  "O<nnn>\tmethods above the threshold will not inline other methods", TR::Options::set32BitNumeric, offsetof(OMR::Options,_dumbInlinerBytecodeSizeMaxCutoff), 0, " %d"},
   {"dumbInlinerBytecodeSizeMinCutoff=",  "O<nnn>\tmethods below the threshold will inline other methods", TR::Options::set32BitNumeric, offsetof(OMR::Options,_dumbInlinerBytecodeSizeMinCutoff), 0, " %d"},
   {"dumpFinalMethodNamesAndCounts",      "O\tPrinting of Method Names and Final Counts", SET_OPTION_BIT(TR_DumpFinalMethodNamesAndCounts), "F"},
   {"dumpInitialMethodNamesAndCounts",    "O\tDebug Printing of Method Names and Initial Counts.", SET_OPTION_BIT(TR_DumpInitialMethodNamesAndCounts), "F"},
   {"dumpIprofilerMethodNamesAndCounts",  "O\tDebug Printing of Method Names and Persisted Counts.", SET_OPTION_BIT(TR_DumpPersistedIProfilerMethodNamesAndCounts), "F"},
   {"dynamicThreadPriority",              "M\tenable dynamic changing of compilation thread priority", SET_OPTION_BIT(TR_DynamicThreadPriority), "F", NOT_IN_SUBSET},
   {"earlyLPQ",                           "M\tAllow compilations from low priority queue to happen early, during startup", SET_OPTION_BIT(TR_EarlyLPQ), "F", NOT_IN_SUBSET },
   {"enableAggressiveLiveness",           "I\tenable globalLiveVariablesForGC below warm", SET_OPTION_BIT(TR_EnableAggressiveLiveness), "F"},
   {"enableAggressiveLoopVersioning", "O\tOptions and thresholds that result in loop versioning occurring in more cases", SET_OPTION_BIT(TR_EnableAggressiveLoopVersioning), "F" },
   {"enableAllocationOfScratchBTL",       "M\tAllow the allocation scratch memory below the line (zOS 31-bit)", RESET_OPTION_BIT(TR_DontAllocateScratchBTL), "F", NOT_IN_SUBSET },
   {"enableAllocationSinking",            "O\tdelay object allocations until immediately before the corresponding constructor calls", TR::Options::enableOptimization, allocationSinking, 0, "P"},
   {EnableAnnotations,                    "O\tenable annotation support",                      SET_OPTION_BIT(TR_EnableAnnotations), "F"},
   {"enableAOTCacheReclamation",          "O\tenable AOT reclamation of code and data cache on AOT relocation failures", SET_OPTION_BIT(TR_EnableAOTCacheReclamation), "F"},
   {"enableAOTInlineSystemMethod",        "O\tenable AOT inline methods from system classes", SET_OPTION_BIT(TR_EnableAOTInlineSystemMethod), "F"},
   {"enableAOTMethodEnter",               "O\tenable AOT method enter", SET_OPTION_BIT(TR_EnableAOTMethodEnter), "F" },
   {"enableAOTMethodExit",                "O\tenable AOT method exit", SET_OPTION_BIT(TR_EnableAOTMethodExit), "F" },
   {"enableAOTRelocationTiming",          "M\tenable timing stats for relocating AOT methods", SET_OPTION_BIT(TR_EnableAOTRelocationTiming), "F"},
   {"enableAOTStats",                     "O\tenable AOT statistics",                      SET_OPTION_BIT(TR_EnableAOTStats), "F"},
   {"enableApplicationThreadYield",       "O\tinsert yield points in application threads", SET_OPTION_BIT(TR_EnableAppThreadYield), "F", NOT_IN_SUBSET},
   {"enableBasicBlockHoisting",           "O\tenable basic block hoisting",                    TR::Options::enableOptimization, basicBlockHoisting, 0, "P"},
   {"enableBlockShuffling",               "O\tenable random rearrangement of blocks",         TR::Options::enableOptimization, blockShuffling, 0, "P"},
   {"enableBranchPreload",                "O\tenable return branch preload for each method (for func testing)",  SET_OPTION_BIT(TR_EnableBranchPreload), "F"},
   {"enableCFGEdgeCounters",              "O\tenable CFG edge counters to keep track of taken and non taken branches in compiled code",      SET_OPTION_BIT(TR_EnableCFGEdgeCounters), "F"},
   {"enableCheapWarmOpts",                "O\tenable cheap warm optimizations", RESET_OPTION_BIT(TR_DisableCheapWarmOpts), "F"},
   {"enableCodeCacheConsolidation",       "M\tenable code cache consolidation", SET_OPTION_BIT(TR_EnableCodeCacheConsolidation), "F", NOT_IN_SUBSET},
   {"enableColdCheapTacticalGRA",         "O\tenable cold cheap tactical GRA", SET_OPTION_BIT(TR_EnableColdCheapTacticalGRA), "F"},
   {"enableCompilationSpreading",         "C\tenable adding spreading invocations to methods before compiling", SET_OPTION_BIT(TR_EnableCompilationSpreading), "F", NOT_IN_SUBSET},
   {"enableCompilationThreadThrottlingDuringStartup", "M\tenable compilation thread throttling during startup", SET_OPTION_BIT(TR_EnableCompThreadThrottlingDuringStartup), "F", NOT_IN_SUBSET },
   {"enableCompilationYieldStats",        "M\tenable statistics on time between 2 consecutive yield points", SET_OPTION_BIT(TR_EnableCompYieldStats), "F", NOT_IN_SUBSET},
   {"enableCopyingTROTInduction1Idioms",  "O\tenable CopyingTROTInduction1 idiom patterns", SET_OPTION_BIT(TR_EnableCopyingTROTInduction1Idioms), "F", NOT_IN_SUBSET},
   {"enableDataCacheStatistics",          "I\tenable the collection and display of data cache related statistics.", SET_OPTION_BIT(TR_EnableDataCacheStatistics),"F", NOT_IN_SUBSET},
   {"enableDeterministicOrientedCompilation", "O\tenable deteministic oriented compilation", SET_OPTION_BIT(TR_EnableDeterministicOrientedCompilation), "F"},
   {"enableDLTBytecodeIndex=",            "O<nnn>\tforce attempted DLT compilation to use specified bytecode index", TR::Options::set32BitNumeric, offsetof(OMR::Options,_enableDLTBytecodeIndex), 0, " %d"},
   {"enableDowngradeOnHugeQSZ",           "M\tdowngrade first time compilations when the compilation queue is huge (1000+ entries)", SET_OPTION_BIT(TR_EnableDowngradeOnHugeQSZ), "F", NOT_IN_SUBSET},
   {"enableDualTLH",                      "D\tEnable use of non-zero initialized TLH. TR_EnableBatchClear must be set too.", RESET_OPTION_BIT(TR_DisableDualTLH), "F"},
   {"enableDupRetTree",                   "O\tEnable duplicate return tree",                  SET_OPTION_BIT(TR_EnableDupRetTree), "F"},
   {"enableDynamicRIBufferProcessing",    "O\tenable disabling buffer processing", RESET_OPTION_BIT(TR_DisableDynamicRIBufferProcessing), "F", NOT_IN_SUBSET},
   {"enableDynamicSamplingWindow",        "M\t", RESET_OPTION_BIT(TR_DisableDynamicSamplingWindow), "F", NOT_IN_SUBSET},
   {"enableEarlyCompilationDuringIdleCpu","M\t", SET_OPTION_BIT(TR_EnableEarlyCompilationDuringIdleCpu), "F", NOT_IN_SUBSET},
   {"enableEBBCCInfo",                    "C\tenable tracking CCInfo in Extended Basic Block scope",  SET_OPTION_BIT(TR_EnableEBBCCInfo), "F"},
   {"enableElementPrivatization",         "O\tenable privatization of stack declared elements accessed by const indices\n", SET_OPTION_BIT(TR_EnableElementPrivatization), "F"},
   {"enableExpensiveOptsAtWarm",          "O\tenable store sinking, shrink wrapping and OSR at warm and below", SET_OPTION_BIT(TR_EnableExpensiveOptsAtWarm), "F" },
   {"enableFastHotRecompilation",         "R\ttry to recompile at hot sooner", SET_OPTION_BIT(TR_EnableFastHotRecompilation), "F"},
   {"enableFastScorchingRecompilation",   "R\ttry to recompile at scorching sooner", SET_OPTION_BIT(TR_EnableFastScorchingRecompilation), "F"},
   {"enableFpreductionAnnotation",        "O\tenable fpreduction annotation", SET_OPTION_BIT(TR_EnableFpreductionAnnotation), "F"},
   {"enableFSDGRA",                       "O\tenable basic GRA in FSD mode", SET_OPTION_BIT(TR_FSDGRA), "F"},
   {"enableGCRPatching",                  "R\tenable patching of the GCR guard", SET_OPTION_BIT(TR_EnableGCRPatching), "F"},
   {"enableGPU",                          "L\tenable GPU support  (basic)",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _enableGPU), TR_EnableGPU, "F"},
   {"enableGPU=",                          "L{regex}\tlist of additional GPU options: enforce, verbose, details, safeMT, enableMath",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _enableGPU), 0, "F"},
   {"enableGRACostBenefitModel",          "O\tenable GRA cost/benefit model", SET_OPTION_BIT(TR_EnableGRACostBenefitModel), "F" },
   {"enableGuardedCountingRecompilation", "O\tenable GCR.  If you don't know what that is, I don't have room to explain it here.", RESET_OPTION_BIT(TR_DisableGuardedCountingRecompilations), "F"},
   {"enableHalfSlotSpills",               "O\tenable sharing of a single 8-byte spill temp for two 4-byte values",  RESET_OPTION_BIT(TR_DisableHalfSlotSpills), "P"},
   {"enableHardwareProfileIndirectDispatch","O\tenable hardware profile indirect dispatch profiling", SET_OPTION_BIT(TR_EnableHardwareProfileIndirectDispatch), "F", NOT_IN_SUBSET},
   {"enableHardwareProfilerDuringStartup", "O\tenable hardware profiler during startup", RESET_OPTION_BIT(TR_DisableHardwareProfilerDuringStartup), "F", NOT_IN_SUBSET},
   {"enableHardwareProfileRecompilation", "O\tenable hardware profile recompilation", SET_OPTION_BIT(TR_EnableHardwareProfileRecompilation), "F", NOT_IN_SUBSET},
   {"enableHCR",                          "O\tenable hot code replacement", SET_OPTION_BIT(TR_EnableHCR), "F", NOT_IN_SUBSET},
#ifdef J9_PROJECT_SPECIFIC
   {"enableIdiomRecognition",             "O\tenable Idiom Recognition", TR::Options::enableOptimization, idiomRecognition, 0, "P"},
#endif
   {"enableInlineProfilingStats",         "O\tenable stats about profile based inlining",      SET_OPTION_BIT(TR_VerboseInlineProfiling), "F"},
   {"enableInliningDuringVPAtWarm",       "O\tenable inlining during VP for warm bodies",    RESET_OPTION_BIT(TR_DisableInliningDuringVPAtWarm), "F"},
   {"enableInliningOfUnsafeForArraylets", "O\tenable inlining of Unsafe calls when arraylets are enabled",                    SET_OPTION_BIT(TR_EnableInliningOfUnsafeForArraylets), "F"},
   {"enableInterfaceCallCachingSingleDynamicSlot",                          "O\tenable interfaceCall caching with one slot storing J9MethodPtr   ",      SET_OPTION_BIT(TR_enableInterfaceCallCachingSingleDynamicSlot), "F"},
   {"enableIprofilerChanges",             "O\tenable iprofiler changes", SET_OPTION_BIT(TR_EnableIprofilerChanges), "F"},
   {"enableIVTT",                         "O\tenable IV Type Transformation", TR::Options::enableOptimization, IVTypeTransformation, 0, "P"},
   {"enableJCLInline",                    "O\tenable JCL Integer and Long methods inlining", SET_OPTION_BIT(TR_EnableJCLInline), "F"},
   {"enableJITHelpershashCodeImpl",       "O\tenalbe java version of object hashCode()", SET_OPTION_BIT(TR_EnableJITHelpershashCodeImpl), "F"},
   {"enableJITHelpersoptimizedClone",     "O\tenalbe java version of object clone()", SET_OPTION_BIT(TR_EnableJITHelpersoptimizedClone), "F"},
   {"enableJProfiling",                   "O\tenable JProfiling", SET_OPTION_BIT(TR_EnableJProfiling), "F"},
   {"enableJProfilingInProfilingCompilations","O\tuse jprofiling instrumentation in profiling compilations", SET_OPTION_BIT(TR_EnableJProfilingInProfilingCompilations), "F"},
   {"enableJVMPILineNumbers",            "M\tenable output of line numbers via JVMPI",       SET_OPTION_BIT(TR_EnableJVMPILineNumbers), "F"},
   {"enableLabelTargetNOPs",             "O\tenable inserting NOPs before label targets", SET_OPTION_BIT(TR_EnableLabelTargetNOPs),  "F"},
   {"enableLargeCodePages",              "C\tenable large code pages",  SET_OPTION_BIT(TR_EnableLargeCodePages), "F"},
   {"enableLastRetrialLogging",          "O\tenable fullTrace logging for last compilation attempt. Needs to have a log defined on the command line", SET_OPTION_BIT(TR_EnableLastCompilationRetrialLogging), "F"},
   {"enableLateCleanFolding",            "O\tfold pdclean flags into pdstore nodes right before codegen",  SET_OPTION_BIT(TR_EnableLateCleanFolding), "F"},
   {"enableLateEdgeSplitting",           "C\trematerialize the vmthread at codegen time only on paths that need it",  RESET_OPTION_BIT(TR_DisableLateEdgeSplitting), "F"},
   {"enableLinkagePreserveStrategy2",              "O\tenable linkage strategy 2", SET_OPTION_BIT(TR_LinkagePreserveStrategy2), "F"},
   {"enableLocalVPSkipLowFreqBlock",     "O\tSkip processing of low frequency blocks in localVP", SET_OPTION_BIT(TR_EnableLocalVPSkipLowFreqBlock), "F" },
   {"enableLongRegAllocation",            "O\tenable allocation of 64-bit regs on 32-bit",      SET_OPTION_BIT(TR_Enable64BitRegsOn32Bit), "F"},
   {"enableLongRegAllocationHeuristic",   "O\tenable heuristic for long register allocation",   SET_OPTION_BIT(TR_Enable64BitRegsOn32BitHeuristic), "F"},
   {"enableLoopEntryAlignment",            "O\tenable loop Entry alignment",                          SET_OPTION_BIT(TR_EnableLoopEntryAlignment), "F"},
   {"enableLoopVersionerCountAllocFences", "O\tallow loop versioner to count allocation fence nodes on PPC toward a profiled guard's block total", SET_OPTION_BIT(TR_EnableLoopVersionerCountAllocationFences), "F"},
   {"enableLowerCompilationLimitsDecisionMaking", "O\tenable the piece of code that lowers compilation limits when low on virtual memory (on Linux and z/OS)",
       SET_OPTION_BIT(TR_EnableLowerCompilationLimitsDecisionMaking), "F", NOT_IN_SUBSET},
   {"enableMetadataBytecodePCToIAMap",   "O\tenable bytecode pc to IA map in the metadata", SET_OPTION_BIT(TR_EnableMetadataBytecodePCToIAMap), "F", NOT_IN_SUBSET},
   {"enableMetadataReclamation",         "I\tenable J9JITExceptionTable reclamation", RESET_OPTION_BIT(TR_DisableMetadataReclamation), "F", NOT_IN_SUBSET},
   {"enableMethodTrampolineReservation", "O\tReserve method trampolines even if they are not needed; only applicable on x86 and zLinux", SET_OPTION_BIT(TR_EnableMethodTrampolineReservation), "F"},
   {"enableMHCustomizationLogicCalls",   "C\tinsert calls to MethodHandle.doCustomizationLogic for handle invocations outside of thunks", SET_OPTION_BIT(TR_EnableMHCustomizationLogicCalls), "F"},
   {"enableMonitorCacheLookup",          "O\tenable  monitor cache lookup under lock nursery ",                       SET_OPTION_BIT(TR_EnableMonitorCacheLookup), "F"},
   {"enableMultipleGCRPeriods",          "M\tallow JIT to get in and out of GCR", SET_OPTION_BIT(TR_EnableMultipleGCRPeriods), "F", NOT_IN_SUBSET},
   {"enableMutableCallSiteGuards",       "O\tenable virgual guards for calls to java.lang.invoke.MutableCallSite.getTarget().invokeExact(...) (including invokedynamic)",   RESET_OPTION_BIT(TR_DisableMutableCallSiteGuards), "F"},
   {"enableNewAllocationProfiling",      "O\tenable profiling of new allocations", SET_OPTION_BIT(TR_EnableNewAllocationProfiling), "F"},
   {"enableNewCheckCastInstanceOf",      "O\tenable new Checkcast/InstanceOf evaluator", SET_OPTION_BIT(TR_EnableNewCheckCastInstanceOf), "F"},
   {"enableNewX86PrefetchTLH",           "O\tenable new X86 TLH prefetch algorithm", SET_OPTION_BIT(TR_EnableNewX86PrefetchTLH), "F"},
   {"enableNodeGC",                      "M\tenable node recycling", SET_OPTION_BIT(TR_EnableNodeGC), "F"},
   {"enableObjectFileGeneration",        "I\tenable the generation of object files use for static linking", SET_OPTION_BIT(TR_EnableObjectFileGeneration), "F", NOT_IN_SUBSET},
   {"enableOnsiteCacheForSuperClassTest", "O\tenable onsite cache for super class test",       SET_OPTION_BIT(TR_EnableOnsiteCacheForSuperClassTest), "F"},
   {"enableOSR",                          "O\tenable on-stack replacement", SET_OPTION_BIT(TR_EnableOSR), "F", NOT_IN_SUBSET},
   {"enableOSROnGuardFailure",            "O\tperform a decompile using on-stack replacement every time a virtual guard fails", SET_OPTION_BIT(TR_EnableOSROnGuardFailure), "F"},
   {"enableOSRSharedSlots",               "O\tenable support for shared slots in OSR", RESET_OPTION_BIT(TR_DisableOSRSharedSlots), "F"},
   {"enableOutlinedNew",                 "O\tdo object allocation logic with a fast jit helper",  SET_OPTION_BIT(TR_EnableOutlinedNew), "F"},
   {"enableOutlinedPrologues",            "O\tcall a helper routine to do some prologue logic",  SET_OPTION_BIT(TR_EnableOutlinedPrologues), "F"},
   {"enableParanoidRefCountChecks",      "O\tenable extra reference count verification", SET_OPTION_BIT(TR_EnableParanoidRefCountChecks), "F"},
   {"enablePerfAsserts",                 "O\tenable asserts for serious performance problems found during compilation",   SET_OPTION_BIT(TR_EnablePerfAsserts), "F"},
   {"enableProfiledDevirtualization",     "O\tenable devirtualization based on interpreter profiling", SET_OPTION_BIT(TR_enableProfiledDevirtualization), "F"},
   {"enableRampupImprovements",           "M\tEnable various changes that improve rampup",    SET_OPTION_BIT(TR_EnableRampupImprovements), "F", NOT_IN_SUBSET},
   {"enableRangeSplittingGRA",            "O\tenable GRA splitting of live ranges to reduce register pressure   ",  SET_OPTION_BIT(TR_EnableRangeSplittingGRA), "F"},
   {"enableRATPurging",                   "O\tpurge the RAT table of assumptions after X registered assumptions",         SET_OPTION_BIT(TR_EnableRATPurging), "F"},
   {"enableReassociation",                "O\tapply reassociation rules in Simplifier",         SET_OPTION_BIT(TR_EnableReassociation), "F"},
   {"enableRecompilationPushing",         "O\tenable pushing methods to be recompiled",         SET_OPTION_BIT(TR_EnableRecompilationPushing), "F"},
   {"enableRefinedAliases",               "O\tenable collecting side-effect summaries from compilations to improve aliasing info in subsequent compilations", RESET_OPTION_BIT(TR_DisableRefinedAliases), "F"},
   {"enableRegisterPressureEstimation",   "O\tdeprecated; same as enableRegisterPressureSimulation", RESET_OPTION_BIT(TR_DisableRegisterPressureSimulation), "F"},
   {"enableRegisterPressureSimulation",   "O\twalk the trees to estimate register pressure during global register allocation", RESET_OPTION_BIT(TR_DisableRegisterPressureSimulation), "F"},
   {"enableReorderArrayIndexExpr",        "O\treorder array index expressions to encourage hoisting", TR::Options::enableOptimization, reorderArrayExprGroup, 0, "P"},
   {"enableRIEMIT",                       "O\tAllows the z Codegen to emit RIEMIT instructions", SET_OPTION_BIT(TR_EnableRIEMIT), "F", NOT_IN_SUBSET},
   {"enableRMODE64",                      "O\tEnable residence mode of compiled bodies on z/OS to reside above the 2-gigabyte bar", SET_OPTION_BIT(TR_EnableRMODE64), "F"},
   {"enableRubyCodeCacheReclamation",     "O\tEnable Tiered Compilation on Ruby", SET_OPTION_BIT(TR_EnableRubyCodeCacheReclamation), "F", NOT_IN_SUBSET},
   {"enableRubyTieredCompilation",        "O\tEnable Tiered Compilation on Ruby", SET_OPTION_BIT(TR_EnableRubyTieredCompilation), "F", NOT_IN_SUBSET},
   {"enableSamplingJProfiling=",          "R\tenable generation of profiling code by the JIT", TR::Options::setSamplingJProfilingBits, 0, 0, "F", NOT_IN_SUBSET},
   {"enableSCHint=","R<nnn>\tOverride default SC Hints to user-specified hints", TR::Options::set32BitHexadecimal, offsetof(OMR::Options, _enableSCHintFlags), 0, "F%d"},
   {"enableScorchInterpBlkFreqProfiling",   "R\tenable profiling blocks in the jit", SET_OPTION_BIT(TR_EnableScorchInterpBlockFrequencyProfiling), "F"},
   {"enableScratchMemoryDebugging",          "I\tUse the debug segment provider for allocating region memory segments.", SET_OPTION_BIT(TR_EnableScratchMemoryDebugging),"F", NOT_IN_SUBSET},
   {"enableSelectiveEnterExitHooks",      "O\tadd method-specific test to JVMTI method enter and exit hooks", SET_OPTION_BIT(TR_EnableSelectiveEnterExitHooks), "F"},
   {"enableSelfTuningScratchMemoryUsageBeforeCompile", "O\tEnable self tuning scratch memory usage", SET_OPTION_BIT(TR_EnableSelfTuningScratchMemoryUsageBeforeCompile), "F", NOT_IN_SUBSET},
   {"enableSelfTuningScratchMemoryUsageInTrMemory", "O\tEnable self tuning scratch memory usage", SET_OPTION_BIT(TR_EnableSelfTuningScratchMemoryUsageInTrMemory), "F", NOT_IN_SUBSET},
   {"enableSeparateInitFromAlloc",        "O\tenable separating init from alloc",            RESET_OPTION_BIT(TR_DisableSeparateInitFromAlloc), "F"},
   {"enableSequentialLoadStoreCold",      "O\tenable sequential store/load opt at cold level", SET_OPTION_BIT(TR_EnableSequentialLoadStoreCold), "F"},
   {"enableSequentialLoadStoreWarm",      "O\tenable sequential store/load opt at warm level", SET_OPTION_BIT(TR_EnableSequentialLoadStoreWarm), "F"},
   {"enableSharedCacheTiming",            "M\tenable timing stats for accessing the shared cache", SET_OPTION_BIT(TR_EnableSharedCacheTiming), "F"},
   {"enableSIMDLibrary",                  "M\tEnable recognized methods for SIMD library", SET_OPTION_BIT(TR_EnableSIMDLibrary), "F"},
   {"enableSnapshotBlockOpts",            "O\tenable block ordering/redirecting optimizations in the presences of snapshot nodes", SET_OPTION_BIT(TR_EnableSnapshotBlockOpts), "F"},
   {"enableTailCallOpt",                  "R\tenable tall call optimization in peephole", SET_OPTION_BIT(TR_EnableTailCallOpt), "F"},
   {"enableThisLiveRangeExtension",       "R\tenable this live range extesion to the end of the method", SET_OPTION_BIT(TR_EnableThisLiveRangeExtension), "F"},
   {"enableTraps",                        "C\tenable trap instructions",                     RESET_OPTION_BIT(TR_DisableTraps), "F"},
   {"enableTreePatternMatching",          "O\tEnable opts that use the TR_Pattern framework", RESET_OPTION_BIT(TR_DisableTreePatternMatching), "F"},
   {"enableTrivialStoreSinking",          "O\tenable trivial store sinking", SET_OPTION_BIT(TR_EnableTrivialStoreSinking), "F"},
   {"enableTrueRegisterModel",            "C\tUse true liveness model in local RA instead of Future Use Count", SET_OPTION_BIT(TR_EnableTrueRegisterModel), "F"},
   {"enableUpgradesByJitSamplingWhenHWProfilingEnabled", "O\tAllow Jit Sampling to upgrade cold compilations when HW Profiling is on",
                                          SET_OPTION_BIT(TR_EnableJitSamplingUpgradesDuringHWProfiling), "F", NOT_IN_SUBSET},
   {"enableUpgradingAllColdCompilations", "O\ttry to upgrade to warm all cold compilations", SET_OPTION_BIT(TR_EnableUpgradingAllColdCompilations), "F"},
   {"enableValueTracing",                 "O\tenable runtime value tracing (experimental)", SET_OPTION_BIT(TR_EnableValueTracing), "F"},
   {"enableVirtualPersistentMemory",      "M\tenable persistent memory to be allocated using virtual memory allocators",
                                          SET_OPTION_BIT(TR_EnableVirtualPersistentMemory), "F", NOT_IN_SUBSET},
   {"enableVpicForResolvedVirtualCalls",  "O\tenable PIC for resolved virtual calls",         SET_OPTION_BIT(TR_EnableVPICForResolvedVirtualCalls), "F"},
   {"enableYieldVMAccess",                "O\tenable yielding of VM access when GC is waiting", SET_OPTION_BIT(TR_EnableYieldVMAccess), "F"},
   {"enableZAccessRegs",                "O\tenable use of access regs as spill area on 390.", SET_OPTION_BIT(TR_Enable390AccessRegs), "F"},
   {"enableZEpilogue",                  "O\tenable 64-bit 390 load-multiple breakdown.", SET_OPTION_BIT(TR_Enable39064Epilogue), "F"},
   {"enableZFreeVMThreadReg",           "O\tenable use of vm thread reg as assignable reg on 390.", SET_OPTION_BIT(TR_Enable390FreeVMThreadReg), "F"},
   {"enumerateAddresses=", "D\tselect kinds of addresses to be replaced by unique identifiers in trace file", TR::Options::setAddressEnumerationBits, offsetof(OMR::Options, _addressToEnumerate), 0, "F"},
   {"estimateRegisterPressure",           "O\tdeprecated; equivalent to enableRegisterPressureSimulation", RESET_OPTION_BIT(TR_DisableRegisterPressureSimulation), "F"},
   {"experimentalClassLoadPhase",         "O\tenable the experimental class load phase algorithm", SET_OPTION_BIT(TR_ExperimentalClassLoadPhase), "F"},
   {"extractExitsByInvalidatingStructure", "O\tInstead of running exit extraction normally, detect nodes that would be extracted, and invalidate structure if there are any", SET_OPTION_BIT(TR_ExtractExitsByInvalidatingStructure), "F"},
   {"failPreXRecompile",                  "I\tfail prexistance based recompilatoins", SET_OPTION_BIT(TR_FailPreXRecompile), "F"},
   {"failRecompile",                      "I\tfail the compile whenever recompiling a method", SET_OPTION_BIT(TR_FailRecompile), "F"},
   {"fanInCallGraphFactor=", "R<nnn>\tFactor by which the weight of the callgraph for a particular caller is multiplied",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::INLINE_fanInCallGraphFactor, 0, " %d", NOT_IN_SUBSET},
   {"firstLevelProfiling",           "O\tProfile first time compilations", SET_OPTION_BIT(TR_FirstLevelProfiling), "F"},
   {"firstOptIndex=",     "O<nnn>\tindex of the first optimization to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_firstOptIndex), 0, "F%d"},
   {"firstOptTransformationIndex=", "O<nnn>\tindex of the first optimization transformation to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_firstOptTransformationIndex), 0, "F%d"},
   {"firstRun", "O\tTell the compiler this is the first run (for count setting and persistence).", SET_OPTION_BIT(TR_FirstRun), "F"},
   {"floatMAF", "O\tEnable fused multiply add (FMA) operations.", SET_OPTION_BIT(TR_FloatMAF), "F"},
   {"forceAOT", "M\tForce compilations to be done in AOT mode",
    SET_OPTION_BIT(TR_ForceAOT), "P", NOT_IN_SUBSET},
   {"forceBCDInit", "O\tForce Binary Coded Decimal (BCD) loads to be initialized by forcing the field to a temporary", SET_OPTION_BIT(TR_ForceBCDInit), "F"},
   {"forceFullSpeedDebug", "M\tForce JIT to pretend that debug mode is activated",
    SET_OPTION_BIT(TR_FullSpeedDebug), "P", NOT_IN_SUBSET},
   {"forceIEEEDivideByZeroException", "O\tForce IEEE divide by zero exception bit on when performing DFP division", SET_OPTION_BIT(TR_ForceIEEEDivideByZeroException), "F"},
   {"forceLargeRAMoves", "O\tAlways use 64 bit register moves in RA", SET_OPTION_BIT(TR_ForceLargeRAMoves), "F"},
   {"forceLoadAOT", "M\tForce loading of relocatable code outside of class load phase from the shared cache",
    SET_OPTION_BIT(TR_ForceLoadAOT), "P", NOT_IN_SUBSET},
   {"forceNonSMP",                           "D\tforce UniP code generation.", SET_OPTION_BIT(TR_ForceNonSMP), "F"},
   {"forceUsePreexistence", "D\tPretend methods are using pre-existence. RAS feature.", SET_OPTION_BIT(TR_ForceUsePreexistence), "F"},
   {"forceVSSStackCompaction", "O\tAlways compact VariableSizeSymbols on the stack", SET_OPTION_BIT(TR_ForceVSSStackCompaction), "F"},
   {"fullInliningUnderOSRDebug", "O\tDo full inlining under OSR based debug (new FSD)", SET_OPTION_BIT(TR_FullInlineUnderOSRDebug), "F"},
   {"GCRCount=",       "R<nnn>\tthe value to which the counter is set for a method containing GCR body. The default is the upgrade compilation count",
    TR::Options::setCount, offsetof(OMR::Options,_GCRCount), 0, " %d"},
   {"GCRdecCount=",       "R<nnn>\tthe value by which the counter is decremented by guarded counting recompilations (postitive value)",
    TR::Options::setCount, offsetof(OMR::Options,_GCRDecCount), 0, " %d"},
   {"GCRresetCount=",       "R<nnn>\tthe value to which the counter is reset to after being tripped by guarded counting recompilations (postive value)",
    TR::Options::setCount, offsetof(OMR::Options,_GCRResetCount), 0, " %d"},
   {"generateCompleteInlineRanges", "O\tgenerate meta data ranges for each change in inliner depth", SET_OPTION_BIT(TR_GenerateCompleteInlineRanges), "F"},
   {"hcrPatchClassPointers", "I\tcreate runtime assumptions for patching pointers to classes, even though they are now updated in-place", SET_OPTION_BIT(TR_HCRPatchClassPointers), "F"},
   {"help",               " \tdisplay this help information", TR::Options::helpOption, 0, 0, NULL, NOT_IN_SUBSET},
   {"help=",              " {regex}\tdisplay help for options whose names match {regex}", TR::Options::helpOption, 1, 0, NULL, NOT_IN_SUBSET},
   {"highOpt",            "O\tdeprecated; equivalent to optLevel=hot", TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), hot},
   {"hotFieldThreshold=", "M<nnn>\t The normalized frequency of a reference to a field to be marked as hot.   Values are 0 to 10000.  Default is 10",
                          TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_hotFieldThreshold, 0, " %d", NOT_IN_SUBSET},
   {"hotMaxStaticPICSlots=", " <nnn>\tmaximum number of polymorphic inline cache slots pre-populated from profiling info for hot and above.  A negative value -N means use N times the maxStaticPICSlots setting.",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_hotMaxStaticPICSlots), 0, "F%d"},


   {"ignoreAssert",         "Ignore any failing assertions", SET_OPTION_BIT(TR_IgnoreAssert), "F"},
   {"ignoreIEEE",           "O\tallow non-IEEE compliant optimizations",  SET_OPTION_BIT(TR_IgnoreIEEERestrictions), "F"},
#ifdef DEBUG
   {"ignoreUnimp",        "D\tdo not fail compilation for unimpemented opcodes",
        TR::Options::setDebug, (intptrj_t)"continueWithUnimplementedOpCode"},
#endif
   {"immediateCountingRecompilation",  "D\tRecompile GCR methods as soon as possible",  SET_OPTION_BIT(TR_ImmediateCountingRecompilation), "F", NOT_IN_SUBSET},
   {"induceOSR=",  "L<induceOSRspec>\tinject induceOSR at specified locations",
        TR::Options::setString,  offsetof(OMR::Options,_induceOSR), 0, "P%s"},
   {"inhibitRecompilation",        "R\tInhibit (but don't disable) recompilation. For diagnostic only.",  SET_OPTION_BIT(TR_InhibitRecompilation), "F", NOT_IN_SUBSET},
   {"inhibitRIBufferProcessingDuringDeepSteady", "R\tInhibit RI during DeepSteady state", SET_OPTION_BIT(TR_InhibitRIBufferProcessingDuringDeepSteady), "F", NOT_IN_SUBSET },
   {"initialOptLevel=cold", "O\thint to perform first time compilations at cold optlevel",
        TR::Options::set32BitValue, offsetof(OMR::Options, _initialOptLevel), cold, "F", NOT_IN_SUBSET },
   {"initialOptLevel=hot", "O\thint to perform first time compilations at hot optlevel",
        TR::Options::set32BitValue, offsetof(OMR::Options, _initialOptLevel), hot, "F", NOT_IN_SUBSET },
   {"initialOptLevel=noOpt", "O\thint to perform first time compilations at noOpt optlevel",
        TR::Options::set32BitValue, offsetof(OMR::Options, _initialOptLevel), noOpt, "F", NOT_IN_SUBSET },
   {"initialOptLevel=scorching", "O\thint to perform first time compilations at scorching optlevel",
        TR::Options::set32BitValue, offsetof(OMR::Options, _initialOptLevel), scorching, "F", NOT_IN_SUBSET },
   {"initialOptLevel=warm", "O\thint to perform first time compilations at warm optlevel",
        TR::Options::set32BitValue, offsetof(OMR::Options, _initialOptLevel), warm, "F", NOT_IN_SUBSET },
   {"inlineCntrAllBucketSize=",    "R<nnn>\tThe unit used to categorize all uninlined calls at once",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrAllBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrCalleeTooBigBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the callee is too big",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrCalleeTooBigBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrCalleeTooDeepBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the recursively computed size of the callee is too big",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrCalleeTooDeepBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrColdAndNotTinyBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the callee is cold and not tiny",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrColdAndNotTinyBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrDepthExceededBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the depth in the call chain is too much",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrDepthExceededBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrRanOutOfBudgetBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the caller ran out of budget",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrRanOutOfBudgetBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrWarmCalleeHasTooManyNodesBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the warm callee has too many nodes",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrWarmCalleeHasTooManyNodesBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrWarmCalleeTooBigBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the warm callee is too big",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrWarmCalleeTooBigBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineCntrWarmCallerHasTooManyNodesBucketSize=",    "R<nnn>\tThe unit used to categorize uninlined calls when the warm caller has too many nodes",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlineCntrWarmCallerHasTooManyNodesBucketSize), 0, " %d", NOT_IN_SUBSET},
   {"inlineNativeOnly", "O\tinliner only inline native methods and do not inline other Java methods", SET_OPTION_BIT(TR_InlineNativeOnly), "F" },
   {"inlinerArgumentHeuristicFractionBeyondWarm=", "O<nnn>\tAt hotness above warm, decreases estimated size by 1/N when the inliner's argument heuristic kicks in", TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlinerArgumentHeuristicFractionBeyondWarm), 0, " %d"},
   {"inlinerArgumentHeuristicFractionUpToWarm=",   "O<nnn>\tAt hotness up to and including warm, decreases estimated size by 1/N when the inliner's argument heuristic kicks in", TR::Options::set32BitNumeric, offsetof(OMR::Options,_inlinerArgumentHeuristicFractionUpToWarm), 0, " %d"},
   {"inlinerBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerBorderFrequency), 0, " %d" },
   {"inlinerCGBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerCGBorderFrequency), 0, " %d" },
   {"inlinerCGColdBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerCGColdBorderFrequency), 0, " %d" },
   {"inlinerCGVeryColdBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerCGVeryColdBorderFrequency), 0, " %d" },
   {"inlinerColdBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerColdBorderFrequency), 0, " %d" },
   {"inlinerFanInUseCalculatedSize", "O\tUse calculated size for fanin method size threshold", SET_OPTION_BIT(TR_InlinerFanInUseCalculatedSize), "F" },
   {"inlinerVeryColdBorderFrequency=", "O<nnn>\tblock frequency threshold for not inlining at warm", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerVeryColdBorderFrequency), 0, " %d" },
   {"inlinerVeryColdBorderFrequencyAtCold=", "O<nnn>\tblock frequency threshold for not inlining at cold", TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerVeryColdBorderFrequencyAtCold), 0, " %d" },
   {"inlinerVeryLargeCompiledMethodAdjustFactor=", "O<nnn>\tFactor to multiply the perceived size of the method",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_inlinerVeryLargeCompiledMethodAdjustFactor, 0, "F%d", NOT_IN_SUBSET },
   {"inlinerVeryLargeCompiledMethodFaninThreshold=", "O<nnn>\tFanin threshold to disallow inlining of very large compiled methods",
       TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerVeryLargeCompiledMethodFaninThreshold), 0, "F%d", NOT_IN_SUBSET },
   {"inlinerVeryLargeCompiledMethodThreshold=", "O<nnn>\tSize threshold to disallow inlining of very large compiled methods",
       TR::Options::set32BitNumeric, offsetof(OMR::Options, _inlinerVeryLargeCompiledMethodThreshold), 0, "F%d", NOT_IN_SUBSET },
   {"inlineVeryLargeCompiledMethods", "O\tAllow inlining of very large compiled methods", SET_OPTION_BIT(TR_InlineVeryLargeCompiledMethods), "F" },
   {"insertInliningCounters=", "O<nnn>\tInsert instrumentation for debugging counters",TR::Options::set32BitNumeric, offsetof(OMR::Options,_insertDebuggingCounters), 0, " %d", NOT_IN_SUBSET},
   {"interpreterSamplingDivisorInStartupMode=",   "R<nnn>\tThe divisor used to decrease the invocation count when an interpreted method is sampled",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_interpreterSamplingDivisorInStartupMode, 0, " %d", NOT_IN_SUBSET},
   {"iprofilerPerformTimestampCheck", "O\tInterpreter Profiling will perform some validity checks based on timestamps",
                                 SET_OPTION_BIT(TR_IProfilerPerformTimestampCheck), "F"},
   {"iprofilerVerbose",          "O\tEnable Interpreter Profiling output messages",           SET_OPTION_BIT(TR_VerboseInterpreterProfiling), "F"},

#if defined(AIXPPC)
   {"j2prof",             "D\tenable profiling",
        SET_OPTION_BIT(TR_CreatePCMaps), "F" },
#endif
   {"jitAllAtMain",          "D\tjit all loaded methods when main is called", SET_OPTION_BIT(TR_jitAllAtMain), "F" },

   {"jitMethodEntryAlignmentBoundary=",      "C<nnn>\tAlignment boundary (in bytes) for JIT method entry",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_jitMethodEntryAlignmentBoundary), 0, "F%d"},
   {"keepBCDWidening",       "O\tstress testing option -- do not remove widening BCD operations", SET_OPTION_BIT(TR_KeepBCDWidening), "F" },

   {"labelTargetNOPLimit=", "C<nnn>\t(labelTargetAddress&0xff) > _labelTargetNOPLimit are padded out with NOPs until the next 256 byte boundary",
      TR::Options::set32BitNumeric, offsetof(OMR::Options,_labelTargetNOPLimit), TR_LABEL_TARGET_NOP_LIMIT , "F%d"},
   {"largeCompiledMethodExemptionFreqCutoff=", "O<nnn>\tInliner threshold",
      TR::Options::set32BitNumeric, offsetof(OMR::Options, _largeCompiledMethodExemptionFreqCutoff), 0, "F%d" },
   {"largeNumberOfLoops=", "O<nnn>\tnumber of loops to consider 'a large number'", TR::Options::set32BitNumeric, offsetof(OMR::Options,_largeNumberOfLoops), 0, "F%d"},
   {"lastIpaOptTransformationIndex=",      "O<nnn>\tindex of the last ipa optimization to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastIpaOptTransformationIndex), 0, "F%d"},
   {"lastOptIndex=",      "O<nnn>\tindex of the last optimization to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastOptIndex), 0, "F%d"},
   {"lastOptSubIndex=",   "O<nnn>\tindex of the last opt transformation to perform within the last optimization (see lastOptIndex)",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastOptSubIndex), 0, "F%d"},
   {"lastOptTransformationIndex=", "O<nnn>\tindex of the last optimization transformation to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastOptTransformationIndex), 0, "F%d"},
   {"lnl=", "C<nnn>\t(labelTargetAddress&0xff) > _labelTargetNOPLimit are padded out with NOPs until the next 256 byte boundary",
      TR::Options::set32BitNumeric, offsetof(OMR::Options,_labelTargetNOPLimit), TR_LABEL_TARGET_NOP_LIMIT , "F%d"},
   {"lockReserveClass=",  "O{regex}\tenable reserving locks for specified classes", TR::Options::setRegex, offsetof(OMR::Options, _lockReserveClass), 0, "P"},
   {"lockVecRegs=",    "M<nn>\tThe number of vector register to lock (from end) Range: 0-32", TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_numVecRegsToLock, 0, "F%d", NOT_IN_SUBSET},
   {"log=",               "L<filename>\twrite log output to filename",
        TR::Options::setString,  offsetof(OMR::Options,_logFileName), 0, "P%s"},
   {"loi=", "O<nnn>\tindex of the last optimization transformation to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastOptTransformationIndex), 0, "F%d"},
   {"loopyAsyncCheckInsertionMaxEntryFreq=",
        "O<nnn>\tmaximum entry block frequency for which asynccheck insertion "
        "considers a method to contain a very frequent block",
        TR::Options::set32BitNumeric, offsetof(OMR::Options, _loopyAsyncCheckInsertionMaxEntryFreq), 0, " %d" },
   {"lowCodeCacheThreshold=", "M<nnn>\tthreshold for available code cache contiguous space",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_lowCodeCacheThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"lowerCountsForAotCold", "M\tLower counts for cold aot runs", SET_OPTION_BIT(TR_LowerCountsForAotCold), "F", NOT_IN_SUBSET},
   {"lowOpt",             "O\tdeprecated; equivalent to optLevel=cold",
        TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), cold},
   {"maskAddresses",       "D\tremove addresses from trace file",                   SET_OPTION_BIT(TR_MaskAddresses), "F" },
   {"maxBytesToLeaveAllocatedInSharedPool=","R<nnn>\tnumber of memory segments to leave allocated",
                                         TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_maxBytesToLeaveAllocatedInSharedPool, 0, "F%d", NOT_IN_SUBSET},
   {"maxInlinedCalls=",    "O<nnn>\tmaximum number of calls to be inlined",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_maxInlinedCalls), 0, "F%d"},
   {"maxLimitedGRACandidates=", "C<nnn>\tThe max number of candidates to consider for limited GRA", TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxLimitedGRACandidates), TR_MAX_LIMITED_GRA_CANDIDATES , "F%d"},
   {"maxLimitedGRARegs=", "C<nnn>\tThe max number of registers to assign for limited GRA", TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxLimitedGRARegs), TR_MAX_LIMITED_GRA_REGS , "F%d"},
   {"maxNumPrexAssumptions=", "R<nnn>\tmaximum number of preexistence assumptions allowed per class",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_maxNumPrexAssumptions, 0, "P%d", NOT_IN_SUBSET},
   {"maxNumVisitedSubclasses=", "R<nnn>\tmaximum number of subclasses allowed per chtable lookup for a given call site",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_maxNumVisitedSubclasses, 0, "P%d", NOT_IN_SUBSET},
   {"maxPeekedBytecodeSize=", "O<nnn>\tmaximum number of bytecodes that can be peeked into",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_maxPeekedBytecodeSize, 0, "F%d", NOT_IN_SUBSET},
   {"maxSizeForVPInliningAtWarm=", "O<nnn>\tMax size for methods inlined during VP", TR::Options::set32BitNumeric, offsetof(OMR::Options, _maxSzForVPInliningWarm), 0, " %d" },
   {"maxSleepTimeMsForCompThrottling=", "M<nnn>\tUpper bound for sleep time during compilation throttling (ms)",
                       TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_maxSleepTimeMsForCompThrottling, 0, "F%d", NOT_IN_SUBSET },
   {"maxSpreadCountLoopless=", "O<nnn>\tnumber of maximum additional invocations before compiling methods without loops",
                        TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxSpreadCountLoopless), 0, "F%d", NOT_IN_SUBSET},
   {"maxSpreadCountLooply=",   "O<nnn>\tnumber of maximum additional invocations before compiling methods with loops",
                        TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxSpreadCountLoopy), 0, "F%d", NOT_IN_SUBSET},
   {"maxStaticPICSlots=", " <nnn>\tmaximum number of polymorphic inline cache slots pre-populated from profiling info",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_maxStaticPICSlots), 0, "F%d"},
   {"maxUnloadedAddressRanges=", " <nnn>\tmaximum number of entries in arrays of unloaded class/method address ranges",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_maxUnloadedAddressRanges), 0, "F%d"},
   {"mbc=", "C<nnn>\tThe max number of candidates to consider for limited GRA", TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxLimitedGRACandidates), TR_MAX_LIMITED_GRA_CANDIDATES , "F%d"},
   {"mbr=", "C<nnn>\tThe max number of registers to assign for limited GRA", TR::Options::set32BitNumeric, offsetof(OMR::Options,_maxLimitedGRARegs), TR_MAX_LIMITED_GRA_REGS , "F%d"},
   {"mccSanityCheck",       "M\tEnable multi-code-cache sanity checking. High overhead", SET_OPTION_BIT(TR_CodeCacheSanityCheck), "F", NOT_IN_SUBSET},
   {"memExpensiveCompThreshold=", "M<nnn>\tthreshold for when compilations are considered memory expensive",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_memExpensiveCompThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"memUsage",                "D\tgather lexical memory profiling statistics of all memory types: stack, heap and persistent",
        SET_OPTION_BIT(TR_LexicalMemProfiler), "F"},
   {"memUsage=",               "D\tgather lexical memory profiling statistics of the list of memory types: stack, heap or persistent",
        TR::Options::setRegex, offsetof(OMR::Options, _memUsage), 0, "P"},
   {"methodOverrideRatSize=", "M<nnn>\tsize of runtime assumption table for method override ops",
                               TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_methodOverrideRatSize, 0, "F%d", NOT_IN_SUBSET},
   {"milcount=",           "O<nnn>\tnumber of invocations before compiling methods with many iterations loops",
        TR::Options::setCount, offsetof(OMR::Options,_initialMILCount), 0, " %d"},
   {"mimicInterpreterFrameShape", "O\tMake sure all locals are laid out in the stack frame just as they would be for the interpreter", SET_OPTION_BIT(TR_MimicInterpreterFrameShape), "F"},
   {"minBytesToLeaveAllocatedInSharedPool=","R<nnn>\tnumber of memory segments to leave allocated",
                                         TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_minBytesToLeaveAllocatedInSharedPool, 0, "F%d", NOT_IN_SUBSET},
   {"minNumberOfTreeTopsInsideTMMonitor=", "R<nnn>\tMinimal number of tree tops needed for TM monitor",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_minimalNumberOfTreeTopsInsideTMMonitor, 0, "P%d", NOT_IN_SUBSET},
   {"minProfiledCheckcastFrequency=", "O<nnn>\tmin profiled frequency for which we generate checkcast test."
                                      "Percentage:  0-100",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_minProfiledCheckcastFrequency, 0, "F%d", NOT_IN_SUBSET},
   {"minSleepTimeMsForCompThrottling=", "M<nnn>\tLower bound for sleep time during compilation throttling (ms)",
                                         TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_minSleepTimeMsForCompThrottling, 0, "F%d", NOT_IN_SUBSET },
   {"noAotSecondRunDetection", "M\tdo not do second run detection for AOT", SET_OPTION_BIT(TR_NoAotSecondRunDetection), "F", NOT_IN_SUBSET },
#ifdef DEBUG
   {"noExceptions",       "C\tfail compilation for methods with exceptions",
        TR::Options::setDebug, (intptrj_t)"noExceptions"},
#endif
   {"noIProfilerDuringStartupPhase", "R\tturn off iprofiler during first startup phase", SET_OPTION_BIT(TR_NoIProfilerDuringStartupPhase), "F", NOT_IN_SUBSET},
   {"noJitDuringBootstrap",  "D\tdon't jit methods during bootstrap", SET_OPTION_BIT(TR_noJitDuringBootstrap), "F" },
   {"noJitUntilMain",        "D\tdon't jit methods until main has been called", SET_OPTION_BIT(TR_noJitUntilMain), "F" },
   {"noload",                "M\tdo not load AOT code from the shared cache (-Xaot option)", SET_OPTION_BIT(TR_NoLoadAOT), "F"},
   {NoOptString,          "O\tdeprecated; equivalent to optLevel=noOpt",
        TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), noOpt, "F"},
   {"noRecompile",        "D\tdo not recompile even when counts allow it", SET_OPTION_BIT(TR_NoRecompile), "F" },
   {"noregmap",           "C\tgenerate GC maps without register maps",  RESET_OPTION_BIT(TR_RegisterMaps), NULL, NOT_IN_SUBSET},
   {"noResumableTrapHandler", "C\tdo not generate traps for exception detections",
        SET_OPTION_BIT(TR_NoResumableTrapHandler), "F" },
   {"noServer",      "D\tDisable compilation strategy for large scale applications (e.g. WebSphere)",    SET_OPTION_BIT(TR_NoOptServer), "P" },
   {"nostore",            "M\tdo not store AOT code into shared cache (-Xaot option)", SET_OPTION_BIT(TR_NoStoreAOT), "F"},

   {"numInterfaceCallCacheSlots=",    "C<nnn>\tThe number of cache slots allocated per interface callpoint, 64 bit zseries only, default is 4",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_numInterfaceCallCacheSlots), 4, "F%d"},
   {"numInterfaceCallStaticSlots=",    "C<nnn>\tThe number of static slots allocated per interface callpoint, 64 bit zseries only, default is 1",
                   TR::Options::set32BitNumeric, offsetof(OMR::Options,_numInterfaceCallStaticSlots), 1, "F%d"},
   {"numIProfiledCallsToTriggerLowPriComp=", "M<nnn>",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_numIProfiledCallsToTriggerLowPriComp, 0, "F%d", NOT_IN_SUBSET },
   {"numRestrictedGPRs=",      "O<nnn>\tnumber of restricted GPRS (0-5). Currently 390 only",
        TR::Options::set32BitNumeric, offsetof(OMR::Options,_numRestrictedGPRs), 0, "F%d"},
   {"objectFile=", "L<filename>\twrite object file to filename", TR::Options::setString, offsetof(OMR::Options,_objectFileName), 0, "P%s", NOT_IN_SUBSET},
   {"oldDataCacheImplementation",         "I\trevert to old data cache implementation.", SET_OPTION_BIT(TR_OldDataCacheImplementation),"F", NOT_IN_SUBSET},
   {"oldJVMPI",           "D\told way of determining which jit options to use with JVMPI",    SET_OPTION_BIT(TR_OldJVMPI), "P" },
   {"omitFramePointer",         "I\tdo not dedicate a frame pointer register for X86 system linkage.", SET_OPTION_BIT(TR_OmitFramePointer),"F", NOT_IN_SUBSET},
   {"onlyInline=",                        "O{regex}\tlist of methods that can be inlined",
                                          TR::Options::setRegex, offsetof(OMR::Options, _onlyInline), 0, "P"},
   {"optDetails",         "L\tlog all optimizer transformations",
        SET_OPTION_BIT(TR_TraceOptDetails), "F" },

   {"optFile=",           "O<filename>\tRead in 'Performing' statements from <filename> and perform those opts instead of the usual ones",
        TR::Options::setString,  offsetof(OMR::Options,_optFileName), 0, "P%s"},
   {"optLevel=cold",      "O\tcompile all methods at cold level",      TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), cold, "P"},
   {"optLevel=hot",       "O\tcompile all methods at hot level",       TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), hot, "P"},
   {"optLevel=noOpt",     "O\tcompile all methods at noOpt level",     TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), noOpt, "P"},
   {"optLevel=scorching", "O\tcompile all methods at scorching level", TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), scorching, "P"},
   {"optLevel=veryHot",   "O\tcompile all methods at veryHot level",   TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), veryHot, "P"},
   {"optLevel=warm",      "O\tcompile all methods at warm level",      TR::Options::set32BitValue, offsetof(OMR::Options, _optLevel), warm, "P"},
   {"orderCompiles",      "C\tcompile methods in limitfile order", SET_OPTION_BIT(TR_OrderCompiles), "P" , NOT_IN_SUBSET},
   {"packedTest=",        "D{regex}\tforce particular code paths to test Java Packed Object",
        TR::Options::setRegex, offsetof(OMR::Options, _packedTest), 0, "P"},
   {"paintAllocatedFrameSlotsDead",   "C\tpaint all slots allocated in method prologue with deadf00d",    SET_OPTION_BIT(TR_PaintAllocatedFrameSlotsDead), "F"},
   {"paintAllocatedFrameSlotsFauxObject",   "C\tpaint all slots allocated in method prologue with faux object pointer",    SET_OPTION_BIT(TR_PaintAllocatedFrameSlotsFauxObject), "F"},
   {"paintDataCacheOnFree",     "I\tpaint data cache allocations that are being returned to the pool", SET_OPTION_BIT(TR_PaintDataCacheOnFree), "F"},
   {"paranoidOptCheck",   "O\tcheck the trees and cfgs after every optimization phase", SET_OPTION_BIT(TR_EnableParanoidOptCheck), "F"},
   {"performLookaheadAtWarmCold", "O\tallow lookahead to be performed at cold and warm", SET_OPTION_BIT(TR_PerformLookaheadAtWarmCold), "F"},
   {"perfTool", "M\tenable PerfTool", SET_OPTION_BIT(TR_PerfTool), "F", NOT_IN_SUBSET },
   {"poisonDeadSlots",    "O\tpaints all dead slots with deadf00d", SET_OPTION_BIT(TR_PoisonDeadSlots), "F"},
   {"prepareForOSREvenIfThatDoesNothing",   "O\temit the call to prepareForOSR even if there is no slot sharing", SET_OPTION_BIT(TR_EnablePrepareForOSREvenIfThatDoesNothing), "F"},
   {"printAbsoluteTimestampInVerboseLog", "O\tPrint Absolute Timestamp in vlog", SET_OPTION_BIT(TR_PrintAbsoluteTimestampInVerboseLog), "F", NOT_IN_SUBSET},
   {"printErrorInfoOnCompFailure",        "O\tPrint compilation error info to stderr", SET_OPTION_BIT(TR_PrintErrorInfoOnCompFailure), "F", NOT_IN_SUBSET},
   {"privatizeOverlaps",  "O\tif BCD storageRefs are going to overlap then do the move through a temp", SET_OPTION_BIT(TR_PrivatizeOverlaps), "F"},
   {"profile",            "O\tcompile a profiling method body", SET_OPTION_BIT(TR_Profile), "F"},
   {"profileCompileTime",   "I\tgenerate a perf report for a specific compilation", SET_OPTION_BIT(TR_CompileTimeProfiler), "F" },
   {"profileMemoryRegions", "I\tenable the collection of scratch memory profiling data", SET_OPTION_BIT(TR_ProfileMemoryRegions), "F" },
   {"profilingCompNodecountThreshold=", "M<nnn>\tthreshold for doubling the method to do a profiling compile is considered expensive",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_profilingCompNodecountThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"profilingCount=",    "R<nnn>\tOverride JIT profiling count", TR::Options::set32BitNumeric, offsetof(OMR::Options, _profilingCount), 0, "F%d"},
   {"profilingFrequency=","R<nnn>\tOverride JIT profiling frequency", TR::Options::set32BitNumeric, offsetof(OMR::Options, _profilingFrequency), 0, "F%d"},
   {"pseudoRandomVerbose","O\twrite info at non determinism points to vlog ", SET_OPTION_BIT(TR_VerbosePseudoRandom), "F"},
   {"qsziMaxToTrackLowPriComp=", "M<nnn>",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_qsziMaxToTrackLowPriComp, 0, "F%d", NOT_IN_SUBSET },
   {"queueWeightThresholdForAppThreadYield=", "M<nnn>\tCompilation queue weight threshold to apply yields to app threads",
                           TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_queueWeightThresholdForAppThreadYield , 0, "F%d", NOT_IN_SUBSET},
   {"queueWeightThresholdForStarvation=", "M<nnn>\tThreshold for applying (or not) starvation decisions",
                           TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_queueWeightThresholdForStarvation , 0, "F%d", NOT_IN_SUBSET},
   {"quickProfile",       "O\tmake online-profile-gathering quick (and less precise)", SET_OPTION_BIT(TR_QuickProfile), "F"},
   {"randomGen",          "D\tDeprecated; same as randomize", SET_OPTION_BIT(TR_Randomize),  "F" },
   {"randomize",          "D\tRandomize certain decisions and thresholds to improve test coverage", SET_OPTION_BIT(TR_Randomize),  "F" },
   {"randomSeed=",        "R<nnn>\tExplicit random seed value; zero (the default) picks the random seed randomly", TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_randomSeed), 0, "F%d"},
   {"randomSeedRaw",      "R\tUses the supplied random seed as-is; see also randomSeedSignatureHash", RESET_OPTION_BIT(TR_RandomSeedSignatureHash),  "F" },
   {"randomSeedSignatureHash","R\tSet random seed value based on a hash of the method's signature, in order to get varying seeds while maintaining reproducibility", SET_OPTION_BIT(TR_RandomSeedSignatureHash),  "F" },
   {"reduceCountsForMethodsCompiledDuringStartup", "M\tNeeds SCC compilation hints\t", SET_OPTION_BIT(TR_ReduceCountsForMethodsCompiledDuringStartup), "F", NOT_IN_SUBSET },
   {"regmap",             "C\tgenerate GC maps with register maps", SET_OPTION_BIT(TR_RegisterMaps), NULL, NOT_IN_SUBSET},
   {"reportEvents",       "C\tcompile event reporting hooks into code", SET_OPTION_BIT(TR_ReportMethodEnter | TR_ReportMethodExit)},
   {"reportMethodEnterExit", "D\treport method enter and exit",                   SET_OPTION_BIT(TR_ReportMethodEnter | TR_ReportMethodExit), "F"},
   {"reserveAllLocks",    "O\tenable reserving locks for all classes and methods (DEBUG Only)", SET_OPTION_BIT(TR_ReserveAllLocks), "F"},
   {"reservingLocks",     "O\tenable reserving locks for hot methods on classes that can be reserved", SET_OPTION_BIT(TR_ReservingLocks), "F"},
   {"restrictInlinerDuringStartup", "O\trestrict trivial inliner during startup", SET_OPTION_BIT(TR_RestrictInlinerDuringStartup), "F", NOT_IN_SUBSET },
   {"restrictStaticFieldFolding", "O\trestrict instance field folding", SET_OPTION_BIT(TR_RestrictStaticFieldFolding), "F"},
   {"rtGCMapCheck", "D\tEnable runtime GC Map checking at every async check.", SET_OPTION_BIT(TR_RTGCMapCheck), "F"},
   {"sampleDensityBaseThreshold=", "M<nnn>\t", TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_sampleDensityBaseThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"sampleDensityIncrementThreshold=", "M<nnn>\t", TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_sampleDensityIncrementThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"sampleInterval=",    "R<nnn>\tThe number of samples taken on a method between times when it is considered for recompilation",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_sampleInterval, 0, " %d", NOT_IN_SUBSET},

   {"sampleThreshold=",    "R<nnn>\tThe maximum number of global samples taken during a sample interval for which the method will be recompiled",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_sampleThreshold, 0, " %d", NOT_IN_SUBSET},
   {"samplingFrequency=", "R<nnn>\tnumber of milliseconds between samples for hotness",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_samplingFrequency, 0, " %d", NOT_IN_SUBSET},
   {"scalarizeSSOps",                      "O\tsupport o-type optimization via scalarization of storega to storage operations",
                                          SET_OPTION_BIT(TR_ScalarizeSSOps), "F"},
   {"scount=",            "O<nnn>\tnumber of invocations before loading relocatable method in shared cache",
        TR::Options::setCount, offsetof(OMR::Options,_initialSCount), 1, " %d"},
   {"scratchSpaceLimit=",    "C<nnn>\ttotal heap and stack memory limit, in KB",
                                         TR::Options::setStaticNumericKBAdjusted, (intptrj_t)&OMR::Options::_scratchSpaceLimit, 0, " %d (KB)"},
   {"scratchSpaceLowerBound=",    "C<nnn>\tlower bound of total heap and stack memory limit, in KB",
                                         TR::Options::setStaticNumericKBAdjusted, (intptrj_t)&OMR::Options::_scratchSpaceLowerBound, 0, " %d (KB)"},
   {"searchCount=",      "O<nnn>\tcount of the max search to perform",
        TR::Options::set32BitSignedNumeric, offsetof(OMR::Options,_lastSearchCount), 0, "F%d"},
   {"sinkAllBlockedStores",               "O\tin trivialStoreSinking sink all stores that are blocked by a killed sym by creating an anchor",
                                          SET_OPTION_BIT(TR_SinkAllBlockedStores), "F"},
   {"sinkAllStores",                      "O\tin trivialStoreSinking sink all stores possible by agressively creating anchors for indirect loads and killed syms",
                                          SET_OPTION_BIT(TR_SinkAllStores), "F"},
   {"sinkOnlyCCStores",                   "O\tin trivialStoreSinking only sink stores that are to the psw.cc symbol",
                                          SET_OPTION_BIT(TR_SinkOnlyCCStores), "F"},
   {"slipTrap=",                          "O{regex}\trecord entry/exit for slit/trap for methods listed",
                                          TR::Options::setRegex, offsetof(OMR::Options, _slipTrap), 0, "P"},
   {"softFailOnAssume",   "M\tfail the compilation quietly and use the interpreter if an assume fails", SET_OPTION_BIT(TR_SoftFailOnAssume), "P"},
   {"stackPCDumpNumberOfBuffers=",            "O<nnn>\t The number of gc cycles for which we collect top stack pcs", TR::Options::setCount, offsetof(OMR::Options,_stackPCDumpNumberOfBuffers), 0, " %d"},
   {"stackPCDumpNumberOfFrames=",            "O<nnn>\t The number of top stack pcs we collect during each cycle", TR::Options::setCount, offsetof(OMR::Options,_stackPCDumpNumberOfFrames), 0, " %d"},
   {"startThrottlingTime=", "M<nnn>\tTime when compilation throttling should start (ms since JVM start)",
                             TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_startThrottlingTime, 0, "F%d", NOT_IN_SUBSET },
   {"startupMethodDontDowngradeThreshold=", "O<nnn> Certain methods below this threshold will not be downgraded during startup",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_startupMethodDontDowngradeThreshold, 0, "F%d", NOT_IN_SUBSET},
   {"staticDebugCounters",     "D\tEnable static versions of all enabled dynamic debug counters (unless staticDebugCounters={regex} is specified)", SET_OPTION_BIT(TR_StaticDebugCountersRequested), "F" },
   {"staticDebugCounters=",    "D{regex}\tEnable static debug counters with names matching regex", TR::Options::setRegex, offsetof(OMR::Options, _enabledStaticCounterNames), 0, "F"},
   {"stopThrottlingTime=", "M<nnn>\tTime when compilation throttling should stop (ms since JVM start)",
                             TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_stopThrottlingTime, 0, "F%d", NOT_IN_SUBSET },
   {"storeSinkingLastOpt=", "C<nnn>\tLast store sinking optimization to perform", TR::Options::set32BitNumeric, offsetof(OMR::Options, _storeSinkingLastOpt), static_cast<uintptrj_t>(-1) , "F%d"},
   {"stressTrampolines", "O\tenables trampolines to always be used for method and helper calls", SET_OPTION_BIT(TR_StressTrampolines), "F"},
   {"strictFPCompares",   "C\tassume strictFP semantics for floating point compares only", SET_OPTION_BIT(TR_StrictFPCompares), "F" },
   {"subtractLoopyMethodCounts",   "C\tSubtract loopy method counts instead of dividing", SET_OPTION_BIT(TR_SubtractLoopyMethodCounts), "F", NOT_IN_SUBSET},
   {"subtractMethodCountsWhenIprofilerIsOff",   "C\tSubtract method counts instead of dividing when Iprofiler is off", SET_OPTION_BIT(TR_SubtractMethodCountsWhenIprofilerIsOff), "F", NOT_IN_SUBSET},
   {"suffixLogs",          "O\tadd the date/time/pid suffix to the file name of the logs", SET_OPTION_BIT(TR_EnablePIDExtension), "F"},
   {"suffixLogsFormat=",   "O\tadd the suffix in specified format to the file name of the logs", TR::Options::setString,  offsetof(OMR::Options, _suffixLogsFormat), 0, "P%s"},
   {"supportSwitchToInterpeter", "C\tGenerate code to allow each method to switch to the interpreter", SET_OPTION_BIT(TR_SupportSwitchToInterpreter), "P"},
   {"suspendCompThreadsEarly", "M\tSuspend compilation threads when QWeight drops under a threshold", SET_OPTION_BIT(TR_SuspendEarly), "F", NOT_IN_SUBSET },
   {"terseRegisterPressureTrace","L\tinclude only summary info about register pressure tracing when traceGRA is enabled", SET_OPTION_BIT(TR_TerseRegisterPressureTrace), "P" },
   {"test390LitPoolBufferSize=", "L\tInsert 8byte elements into Lit Pool to force testing of large lit pool sizes",
        TR::Options::set32BitNumeric,offsetof(OMR::Options,_test390LitPoolBuffer), 0, "F%d"},
   {"test390StackBufferSize=", "L\tInsert buffer in stack to force testing of large stack sizes",
        TR::Options::set32BitNumeric,offsetof(OMR::Options,_test390StackBuffer), 0, "F%d"},
   {"timing", "M\ttime individual phases and optimizations", SET_OPTION_BIT(TR_Timing), "F" },
   {"timingCumulative", "M\ttime cumulative phases (ILgen,Optimizer,codegen)", SET_OPTION_BIT(TR_CummTiming), "F" },
#if defined(TR_HOST_X86) || defined(TR_HOST_POWER)
   {"tlhPrefetch",  "D\tenable software prefetch on allocation ",   SET_OPTION_BIT(TR_TLHPrefetch),  "F" },
#endif // defined(TR_HOST_X86) || defined(TR_HOST_POWER)

   {"tocSize=", "C<nnn>\tnumber of KiloBytes allocated for table of constants",
      TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_tocSizeInKB, 0, "P%d (KB)", NOT_IN_SUBSET},
   {"traceAddAndRemoveEdge",            "L\ttrace edge addition and removal",              SET_OPTION_BIT(TR_TraceAddAndRemoveEdge), "P" },
   {"traceAliases",                     "L\ttrace alias set generation",                   SET_OPTION_BIT(TR_TraceAliases), "P" },
   {"traceAllocationSinking",           "L\ttrace allocation sinking",                     TR::Options::traceOptimization, allocationSinking, 0, "P"},
   {"traceAndSimplification",           "L\ttrace and simplification",                     TR::Options::traceOptimization, andSimplification, 0, "P"},
   {"traceArraycopyTransformation",     "L\ttrace arraycopy transformation",               TR::Options::traceOptimization, arraycopyTransformation, 0, "P"},
   {"traceAsyncCheckInsertion",         "L\ttrace redundant insertion of async checks",    TR::Options::traceOptimization, asyncCheckInsertion, 0, "P" },
   {"traceAutoSIMD",                    "L\ttrace autoVectorization ",                     TR::Options::traceOptimization, SPMDKernelParallelization, 0, "P"},
   {"traceBasicBlockExtension",         "L\ttrace basic block extension",                  TR::Options::traceOptimization, basicBlockExtension, 0, "P"},
   {"traceBasicBlockHoisting",          "L\ttrace basic block hoisting",                   TR::Options::traceOptimization, basicBlockHoisting, 0, "P"},
   {"traceBasicBlockPeepHole",          "L\ttrace basic blocks peepHole",                  TR::Options::traceOptimization, basicBlockPeepHole, 0, "P"},
   {"traceBBVA",                        "L\ttrace backward bit vector analysis",           SET_OPTION_BIT(TR_TraceBBVA), "P" },
   {"traceBC",                          "L\tdump bytecodes",                               SET_OPTION_BIT(TR_TraceBC), "P" },
   {"traceBCDCodeGen",                  "L\ttrace binary coded decimal code generations",  SET_TRACECG_BIT(TR_TraceCGBinaryCodedDecimal), "P"},
   {"traceBin",                         "L\tdump binary instructions",                     SET_TRACECG_BIT(TR_TraceCGPostBinaryEncoding|TR_TraceCGMixedModeDisassembly), "P" },
   {"traceBlockFrequencyGeneration",    "L\ttrace block frequency generation",             SET_OPTION_BIT(TR_TraceBFGeneration), "P"},
   {"traceBlockShuffling",              "L\ttrace random rearrangement of blocks",         TR::Options::traceOptimization, blockShuffling, 0, "P"},
   {"traceBlockSplitter",               "L\ttrace block splitter",                         TR::Options::traceOptimization, blockSplitter, 0, "P"},
   {"traceBVA",                         "L\ttrace bit vector analysis",                    SET_OPTION_BIT(TR_TraceBVA), "P" },
   {"traceCatchBlockRemoval",           "L\ttrace catch block removal",                    TR::Options::traceOptimization, catchBlockRemoval, 0, "P"},
   {"traceCFGSimplification",           "L\ttrace Control Flow Graph simplification",      TR::Options::traceOptimization, CFGSimplification, 0, "P"},
   {"traceCG",                          "L\tdump output of code generation passes",        SET_OPTION_BIT(TR_TraceCG), "P" },
   {"traceCodeGen",                     "L\tdump output of code generation passes",        SET_OPTION_BIT(TR_TraceCG), "P" },
   {"traceColdBlockMarker",             "L\ttrace detection of cold blocks",               TR::Options::traceOptimization, coldBlockMarker, 0, "P"},
   {"traceColdBlockOutlining",          "L\ttrace outlining of cold blocks",               TR::Options::traceOptimization, coldBlockOutlining, 0, "P"},
   {"traceCompactLocals",               "L\ttrace compact locals",                         TR::Options::traceOptimization, compactLocals, 0, "P"},
   {"traceCompactNullChecks",           "L\ttrace compact null checks",                    TR::Options::traceOptimization, compactNullChecks, 0, "P"},
   {"traceDeadTreeElimination",         "L\ttrace dead tree elimination",                  TR::Options::traceOptimization, deadTreesElimination, 0, "P"},
   {"traceDominators",                  "L\ttrace dominators and post-dominators",         SET_OPTION_BIT(TR_TraceDominators), "P" },
   {"traceEarlyStackMap",               "L\ttrace early stack map",                        SET_TRACECG_BIT(TR_TraceEarlyStackMap), "P"},
   {"traceEscapeAnalysis",              "L\ttrace escape analysis",                        TR::Options::traceOptimization, escapeAnalysis, 0, "P"},
   {"traceEvaluation",                  "L\tdump output of tree evaluation passes",        SET_TRACECG_BIT(TR_TraceCGEvaluation), "P" },
   {"traceExitExtraction",              "L\ttrace extraction of structure nodes that unconditionally exit to outer regions", SET_OPTION_BIT(TR_TraceExitExtraction), "F"},
   {"traceExplicitNewInitialization",   "L\ttrace explicit new initialization",            TR::Options::traceOptimization, explicitNewInitialization, 0, "P"},
   {"traceFieldPrivatization",          "L\ttrace field privatization",                    TR::Options::traceOptimization, fieldPrivatization, 0, "P"},
   {"traceForCodeMining=",              "L{regex}\tadd instruction annotations for code mining",
                                         TR::Options::setRegex, offsetof(OMR::Options, _traceForCodeMining), 0, "P"},
   {"traceFull",                        "L\tturn on all trace options",                    SET_OPTION_BIT(TR_TraceAll), "P"},
   {"traceGeneralStoreSinking",         "L\ttrace general store sinking",                  TR::Options::traceOptimization, generalStoreSinking, 0, "P"},
   {"traceGlobalCopyPropagation",       "L\ttrace global copy propagation",                TR::Options::traceOptimization, globalCopyPropagation, 0, "P"},
   {"traceGlobalDSE",                   "L\ttrace global dead store elimination",          TR::Options::traceOptimization, globalDeadStoreElimination, 0, "P"},
   {"traceGlobalLiveVariablesForGC",    "L\ttrace global live variables for GC",           TR::Options::traceOptimization, globalLiveVariablesForGC, 0, "P"},
   {"traceGlobalVP",                    "L\ttrace global value propagation",               TR::Options::traceOptimization, globalValuePropagation, 0, "P"},
   {"traceGLU",                         "L\ttrace general loop unroller",                  TR::Options::traceOptimization, generalLoopUnroller, 0, "P"},
   {"traceGRA",                         "L\ttrace tree based global register allocator",     TR::Options::traceOptimization, tacticalGlobalRegisterAllocator, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"traceIdiomRecognition",            "L\ttrace idiom recognition",                       TR::Options::traceOptimization, idiomRecognition, 0, "P"},
#endif
   {"traceILDeadCode",                  "L\ttrace Instruction Level Dead Code (basic)",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceILDeadCode), TR_TraceILDeadCodeBasic, "F"},
   {"traceILDeadCode=",                 "L{regex}\tlist of additional traces to enable: basic, listing, details, live, progress",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceILDeadCode), 0, "P"},
   {"traceILGen",                       "L\ttrace IL generator",                           SET_OPTION_BIT(TR_TraceILGen), "F"},
   {"traceILValidator",                 "L\ttrace validation over intermediate language constructs",SET_OPTION_BIT(TR_TraceILValidator), "F" },
   {"traceILWalk",                      "L\tsynonym for traceILWalks",                              SET_OPTION_BIT(TR_TraceILWalks), "P" },
   {"traceILWalks",                     "L\ttrace iteration over intermediate language constructs", SET_OPTION_BIT(TR_TraceILWalks), "P" },
   {"traceInductionVariableAnalysis",   "L\ttrace Induction Variable Analysis",            TR::Options::traceOptimization, inductionVariableAnalysis,       0, "P"},
   {"traceInlining",                    "L\ttrace IL inlining",                            TR::Options::traceOptimization, inlining, 0, "P"},
   {"traceInnerPreexistence",           "L\ttrace inner preexistence",                     TR::Options::traceOptimization, innerPreexistence, 0, "P"},
   {"traceInvariantArgumentPreexistence", "L\ttrace invariable argument preexistence",     TR::Options::traceOptimization, invariantArgumentPreexistence, 0, "P"},
   {"traceIsolatedSE",                  "L\ttrace isolated store elimination",             TR::Options::traceOptimization, isolatedStoreElimination, 0, "P"},
   {"traceIVTT",                        "L\ttrace IV Type transformation",                 TR::Options::traceOptimization, IVTypeTransformation, 0, "P"},
   {"traceKnownObjectGraph",            "L\ttrace the relationships between objects in the known-object table", SET_OPTION_BIT(TR_TraceKnownObjectGraph), "P" },
   {"traceLabelTargetNOPs",             "L\ttrace inserting of NOPs before label targets", SET_OPTION_BIT(TR_TraceLabelTargetNOPs), "F"},
   {"traceLastOpt",                     "L\textra tracing for the opt corresponding to lastOptIndex; usually used with traceFull", SET_OPTION_BIT(TR_TraceLastOpt), "F"},
   {"traceLiveMonitorMetadata",         "L\ttrace live monitor metadata",                  SET_OPTION_BIT(TR_TraceLiveMonitorMetadata), "F" },
   {"traceLiveness",                     "L\ttrace liveness analysis",                     SET_OPTION_BIT(TR_TraceLiveness), "P" },
   {"traceLiveRangeSplitter",           "L\ttrace live-range splitter for global register allocator",     TR::Options::traceOptimization, liveRangeSplitter, 0, "P"},
   {"traceLocalCSE",                    "L\ttrace local common subexpression elimination", TR::Options::traceOptimization, localCSE, 0, "P"},
   {"traceLocalDSE",                    "L\ttrace local dead store elimination",           TR::Options::traceOptimization, localDeadStoreElimination, 0, "P"},
   {"traceLocalLiveRangeReduction",     "L\ttrace local live range reduction",             TR::Options::traceOptimization, localLiveRangeReduction, 0, "P"},
   {"traceLocalLiveVariablesForGC",     "L\ttrace local live variables for GC",            TR::Options::traceOptimization, localLiveVariablesForGC, 0, "P"},
   {"traceLocalReordering",             "L\ttrace local reordering",                       TR::Options::traceOptimization, localReordering, 0, "P"},
   {"traceLocalVP",                     "L\ttrace local value propagation",                TR::Options::traceOptimization, localValuePropagation, 0, "P"},
   {"traceLongRegAllocationHeuristic",  "L\ttrace long register allocation",               TR::Options::traceOptimization, longRegAllocation, 0, "P"},
   {"traceLookahead",                   "O\ttrace class lookahead",                        SET_OPTION_BIT(TR_TraceLookahead), "P"},
   {"traceLoopAliasRefiner",            "L\ttrace loop alias refiner",                     TR::Options::traceOptimization, loopAliasRefiner, 0, "P"},
   {"traceLoopCanonicalization",        "L\ttrace loop canonicalization",                  TR::Options::traceOptimization, loopCanonicalization, 0, "P"},
   {"traceLoopInversion",               "L\ttrace loop inversion",                         TR::Options::traceOptimization, loopInversion, 0, "P"},
   {"traceLoopReduction",               "L\ttrace loop reduction",                         TR::Options::traceOptimization, loopReduction, 0, "P"},
   {"traceLoopReplicator",              "L\ttrace loop replicator",                        TR::Options::traceOptimization, loopReplicator, 0, "P"},
   {"traceLoopStrider",                 "L\ttrace loop strider",                           TR::Options::traceOptimization, loopStrider,   0, "P"},
   {"traceLoopVersioner",               "L\ttrace loop versioner",                          TR::Options::traceOptimization, loopVersioner, 0, "P"},
   {"traceMarkingOfHotFields",          "M\ttrace marking of Hot Fields",                 SET_OPTION_BIT(TR_TraceMarkingOfHotFields), "F"},
   {"traceMethodIndex",                 "L\treport every method symbol that gets created and consumes a methodIndex", SET_OPTION_BIT(TR_TraceMethodIndex), "F"},
   {"traceMixedModeDisassembly",        "L\tdump generated assembly with bytecodes",       SET_TRACECG_BIT(TR_TraceCGMixedModeDisassembly), "P"},
   {"traceNewBlockOrdering",            "L\ttrace new block ordering",                     TR::Options::traceOptimization, basicBlockOrdering, 0, "P"},
   {"traceNodeFlags",                   "L\ttrace setting/resetting of node flags",        SET_OPTION_BIT(TR_TraceNodeFlags), "F"},
   {"traceNonLinearRA",                 "L\ttrace non-linear RA",                          SET_OPTION_BIT(TR_TraceNonLinearRegisterAssigner), "F"},
   {"traceObjectFileGeneration",        "I\ttrace the creation of object files used for static linking", SET_OPTION_BIT(TR_TraceObjectFileGeneration), "F", NOT_IN_SUBSET},
   {"traceOpts",                        "L\tdump each optimization name",                 SET_OPTION_BIT(TR_TraceOpts), "P" },
   {"traceOpts=",                       "L{regex}\tlist of optimizations to trace", TR::Options::setRegex, offsetof(OMR::Options, _optsToTrace), 0, "P"},
   {"traceOptTrees",                    "L\tdump trees after each optimization",           SET_OPTION_BIT(TR_TraceOptTrees), "P" },
   {"traceOSR",                         "L\ttrace OSR",                                    SET_OPTION_BIT(TR_TraceOSR), "P"},
   {"traceOSRDefAnalysis",              "L\ttrace OSR reaching defintions analysis",       TR::Options::traceOptimization, osrDefAnalysis, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"traceOSRGuardInsertion",           "L\ttrace HCR guard insertion",                    TR::Options::traceOptimization, osrGuardInsertion, 0, "P"},
   {"traceOSRGuardRemoval",             "L\ttrace HCR guard removal",                      TR::Options::traceOptimization, osrGuardRemoval, 0, "P"},
#endif
   {"tracePartialInlining",             "L\ttrace partial inlining heuristics",            SET_OPTION_BIT(TR_TracePartialInlining), "P" },
   {"tracePostBinaryEncoding",          "L\tdump instructions (code cache addresses, real registers) after binary encoding", SET_TRACECG_BIT(TR_TraceCGPostBinaryEncoding), "P"},
   {"tracePostInstructionSelection",    "L\tdump instructions (virtual registers) after instruction selection", SET_TRACECG_BIT(TR_TraceCGPostInstructionSelection), "P"},
   {"tracePostRegisterAssignment",      "L\tdump instructions (real registers) after register assignment", SET_TRACECG_BIT(TR_TraceCGPostRegisterAssignment), "P"},
   {"tracePRE",                         "L\ttrace partial redundancy elimination",        TR::Options::traceOptimization, partialRedundancyElimination, 0, "P"},
   {"tracePrefetchInsertion",           "L\ttrace prefetch insertion",                     TR::Options::traceOptimization, prefetchInsertion, 0, "P"},
   {"tracePREForSubNodeReplacement",    "L\ttrace partial redundancy elimination focussed on optimal subnode replacement", SET_OPTION_BIT(TR_TracePREForOptimalSubNodeReplacement), "P" },
   {"tracePreInstructionSelection",     "L\tdump trees prior to instruction selection",    SET_TRACECG_BIT(TR_TraceCGPreInstructionSelection), "P"},
   {"traceProfiledNodeVersioning",      "L\ttrace profiled node versioning",               TR::Options::traceOptimization, profiledNodeVersioning, 0, "P"},
#ifdef J9_PROJECT_SPECIFIC
   {"traceProfileGenerator",            "L\ttrace profile generator",                      TR::Options::traceOptimization, profileGenerator, 0, "P"},
#endif
   {"traceRA",                          "L\ttrace register assignment (basic)",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _raTrace), TR_TraceRABasic, "F"},
   {"traceRA=",                         "L{regex}\tlist of additional register assignment traces to enable: deps, details, preRA, states",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _raTrace), 0, "F"},
   {"traceReachability",                "L\ttrace all analyses based on the reachability engine",     SET_OPTION_BIT(TR_TraceReachability), "P"},
   {"traceRecognizedCallTransformer",   "L\ttrace recognized call transformer",            TR::Options::traceOptimization, recognizedCallTransformer, 0, "P"},
   {"traceRedundantAsyncCheckRemoval",  "L\ttrace redundant async check removal",          TR::Options::traceOptimization, redundantAsyncCheckRemoval, 0, "P"},
   {"traceRedundantGotoElimination",    "L\ttrace redundant goto elimination",             TR::Options::traceOptimization, redundantGotoElimination, 0, "P"},
   {"traceRedundantMonitorElimination", "L\ttrace redundant monitor elimination",          TR::Options::traceOptimization, redundantMonitorElimination, 0, "P"},
   {"traceRegDepCopyRemoval",           "L\ttrace register dependency copy removal", TR::Options::traceOptimization, regDepCopyRemoval, 0, "P"},
   {"traceRegisterITF",                 "L\ttrace register interference graph (basic)",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceRegisterITF), TR_TraceRegisterITFBasic, "F"},
   {"traceRegisterITF=",                "L{regex}\tlist of additional register interference graph build options: basic, listing, results, build, details, live, colourability",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceRegisterITF), 0, "P"},
   {"traceRegisterPressureDetails",     "L\tinclude extra register pressure annotations in register pressure simulation and tree evaluation traces", SET_OPTION_BIT(TR_TraceRegisterPressureDetails), "P" },
   {"traceRegisterState",               "L\ttrace bit vector denoting assigned registers after register allocation", SET_OPTION_BIT(TR_TraceRegisterState), "P"},
   {"traceRelocatableDataCG",           "L\ttrace relocation data when generating relocatable code", SET_OPTION_BIT(TR_TraceRelocatableDataCG), "P"},
   {"traceRelocatableDataDetailsCG",    "L\ttrace relocation data details when generating relocatable code", SET_OPTION_BIT(TR_TraceRelocatableDataDetailsCG), "P"},
   {"traceRelocatableDataDetailsRT",    "L\ttrace relocation data details when relocating relocatable code", SET_OPTION_BIT(TR_TraceRelocatableDataDetailsRT), "P"},
   {"traceRelocatableDataRT",           "L\ttrace relocation data when relocating relocatable code", SET_OPTION_BIT(TR_TraceRelocatableDataRT), "P"},
   {"traceReloCG",                      "L\ttrace relocation data details with generation info when generating relocatable code", SET_OPTION_BIT(TR_TraceReloCG),"P"},
   {"traceRematerialization",           "L\ttrace rematerialization",                      TR::Options::traceOptimization, rematerialization, 0, "P"},
   {"traceReorderArrayIndexExpr",       "L\ttrace reorder array index expressions",        TR::Options::traceOptimization, reorderArrayIndexExpr, 0, "P"},
   {"traceSamplingJProfiling",          "L\ttrace samplingjProfiling",                     TR::Options::traceOptimization, samplingJProfiling, 0, "P"},
   {"traceScalarizeSSOps",              "L\ttrace scalarization of array/SS ops", SET_OPTION_BIT(TR_TraceScalarizeSSOps), "P"},
   {"traceSEL",                         "L\ttrace sign extension load", SET_OPTION_BIT(TR_TraceSEL), "P"},
   {"traceSequenceSimplification",      "L\ttrace arithmetic sequence simplification",     TR::Options::traceOptimization, expressionsSimplification, 0, "P"},
   {"traceShrinkWrapping",              "L\ttrace shrink wrapping of callee saved registers ", SET_OPTION_BIT(TR_TraceShrinkWrapping), "P"},
   {"traceSpillCosts",                 "L\ttrace spill costs (basic) only show its activation",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceSpillCosts), TR_TraceSpillCostsBasic, "F"},
   {"traceSpillCosts=",                "L{regex}\tlist of additional spill costs options: basic, results, build, details",
        TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceSpillCosts), 0, "P"},
   {"traceStringBuilderTransformer",    "L\ttrace StringBuilder tranfsofermer optimization", TR::Options::traceOptimization, stringBuilderTransformer, 0, "P"},
   {"traceStringPeepholes",             "L\ttrace string peepholes",                       TR::Options::traceOptimization, stringPeepholes, 0, "P"},
   {"traceStripMining",                 "L\ttrace strip mining",                           TR::Options::traceOptimization, stripMining, 0, "P"},
   {"traceStructuralAnalysis",          "L\ttrace structural analysis", SET_OPTION_BIT(TR_TraceSA), "P"},
   {"traceSwitchAnalyzer",              "L\ttrace switch analyzer",                        TR::Options::traceOptimization, switchAnalyzer, 0, "P"},
   {"traceTempUsage",                   "L\ttrace number of temps used",                   SET_OPTION_BIT(TR_TraceTempUsage), "P"},
   {"traceTempUsageMore",               "L\ttrace usage of temps, showing each temp used", SET_OPTION_BIT(TR_TraceTempUsageMore), "P"},
   {"traceTreeCleansing",               "L\ttrace tree cleansing",                         TR::Options::traceOptimization, treesCleansing, 0, "P"},
   {"traceTreePatternMatching",         "L\ttrace the functioning of the TR_Pattern framework", SET_OPTION_BIT(TR_TraceTreePatternMatching), "F"},
   {"traceTrees",                       "L\tdump trees after each compilation phase", SET_OPTION_BIT(TR_TraceTrees), "P" },
   {"traceTreeSimplification",          "L\ttrace tree simplification",                    TR::Options::traceOptimization, treeSimplification, 0, "P"},
   {"traceTreeSimplification=",         "L{regex}\tlist of additional options: mulDecompose",
         TR::Options::setBitsFromStringSet, offsetof(OMR::Options, _traceSimplifier), 0, "P"},
   {"traceTrivialBlockExtension",       "L\ttrace trivial block extension",                TR::Options::traceOptimization, trivialBlockExtension, 0, "P"},
   {"traceTrivialDeadTreeRemoval",      "L\ttrace trivial dead tree removal", SET_OPTION_BIT(TR_TraceTrivialDeadTreeRemoval), "P"},
   {"traceTrivialStoreSinking",         "L\ttrace trivial store sinking",                  TR::Options::traceOptimization, trivialStoreSinking, 0, "P"},
   {"traceUnsafeFastPath",              "L\ttrace unsafe fast path",                       TR::Options::traceOptimization, unsafeFastPath, 0, "P"},  // Java specific option
   {"traceUnsafeInlining",              "L\ttrace unsafe inlining",                        SET_OPTION_BIT(TR_TraceUnsafeInlining), "F"},
   {"traceUseDefs",                     "L\ttrace use def info",                           SET_OPTION_BIT(TR_TraceUseDefs), "F"},
   {"traceValueNumbers",                "L\ttrace value number info",                      SET_OPTION_BIT(TR_TraceValueNumbers), "F"},
   {"traceVarHandleTransformer",        "L\ttrace VarHandle transformer",                  TR::Options::traceOptimization, varHandleTransformer, 0, "P"},  // Java specific option
   {"traceVFPSubstitution",             "L\ttrace replacement of virtual frame pointer with actual register in memrefs", SET_OPTION_BIT(TR_TraceVFPSubstitution), "F"},
   {"traceVIP",                         "L\ttrace variable initializer propagation (constant propagation of read-only variables)", SET_OPTION_BIT(TR_TraceVIP), "P" },
   {"traceVirtualGuardHeadMerger",      "L\ttrace virtual head merger",                    TR::Options::traceOptimization, virtualGuardHeadMerger, 0, "P"},
   {"traceVirtualGuardTailSplitter",    "L\ttrace virtual guard tail splitter",            TR::Options::traceOptimization, virtualGuardTailSplitter, 0, "P"},
   {"traceVPConstraints",               "L\ttrace the execution of value propagation merging and intersecting", SET_OPTION_BIT(TR_TraceVPConstraints), "F"},
   {"trampolineSpacePercentage=",       "R<nnn>\tpercent of code cache space to reserve for tranmpolines",
                                         TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_trampolineSpacePercentage, 0, "F%d", NOT_IN_SUBSET},
   {"transactionalMemoryRetryCount=",   "R<nnn>\tthe time of retries when transactions get abort",
                                         TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_TransactionalMemoryRetryCount, 0, "F%d", NOT_IN_SUBSET},
#if defined(DEBUG)
   {"trdebug=", "D{option,option,...}\tadd debug_options to the debug list",TR::Options::setDebug},
#endif
   {"trivialInlinerMaxSize=",           "O<nnn>\tmax size of a method that will be inlined by trivial inliner", TR::Options::set32BitNumeric, offsetof(OMR::Options,_trivialInlinerMaxSize), 0, " %d"},
   {"trustAllInterfaceTypeInfo",        "O\tGive Java interface type information the same level of trust afforded to class info.  The Java spec is much more lackadasical about interface type safety, and requires us to be conservative.", SET_OPTION_BIT(TR_TrustAllInterfaceTypeInfo), "F"},
   {"tryToInline=",                     "O{regex}\tlist of callee methods to be inlined if possible", TR::Options::setRegex, offsetof(OMR::Options, _tryToInline), 0, "P"},
   {"turnOffSelectiveNoOptServerIfNoStartupHint", "M\t", SET_OPTION_BIT(TR_TurnOffSelectiveNoOptServerIfNoStartupHint), "F", NOT_IN_SUBSET },
   {"unleashStaticFieldFolding",        "O\tbypass the class white-list, and allow static final fields to be folded aggressively", RESET_OPTION_BIT(TR_RestrictStaticFieldFolding), "F"},
   {"unresolvedSymbolsAreNotColdAtCold", "R\tMark unresolved symbols as cold blocks at cold or lower", SET_OPTION_BIT(TR_UnresolvedAreNotColdAtCold), "F"},
   {"upgradeBootstrapAtWarm",           "R\tRecompile bootstrap AOT methods at warm instead of cold", SET_OPTION_BIT(TR_UpgradeBootstrapAtWarm), "F", NOT_IN_SUBSET},
   {"useGlueIfMethodTrampolinesAreNotNeeded", "O\tSafety measure; return to the old behaviour of always going through the glue", SET_OPTION_BIT(TR_UseGlueIfMethodTrampolinesAreNotNeeded), "F"},
   {"useHigherCountsForNonSCCMethods", "M\tuse the default high counts for methods belonging to classes not in SCC", SET_OPTION_BIT(TR_UseHigherCountsForNonSCCMethods), "F", NOT_IN_SUBSET },
   {"useHigherMethodCounts", "M\tuse the default high counts for methods even for AOT", SET_OPTION_BIT(TR_UseHigherMethodCounts), "F" },
   {"useHigherMethodCountsAfterStartup", "M\tuse the default high counts for methods after startup in AOT mode", SET_OPTION_BIT(TR_UseHigherMethodCountsAfterStartup), "F", NOT_IN_SUBSET },
   {"useIdleTime", "M\tuse cpu idle time to compile more aggressively", SET_OPTION_BIT(TR_UseIdleTime), "F", NOT_IN_SUBSET },
   {"useILValidator", "O\tuse the new ILValidator to check IL instead of the old TreeVerifier", SET_OPTION_BIT(TR_UseILValidator), "F"},
   {"useLowerMethodCounts",          "M\tuse the original initial counts for methods", SET_OPTION_BIT(TR_UseLowerMethodCounts), "F"},
   {"useLowPriorityQueueDuringCLP",  "O\tplace cold compilations due to classLoadPhase "
                                     "in the low priority queue to be compiled later",
                                      SET_OPTION_BIT(TR_UseLowPriorityQueueDuringCLP), "F", NOT_IN_SUBSET},
   {"useOldHCRGuardAOTRelocations", "I\tcreate apparently ineffective AOT relocations for HCR guards", SET_OPTION_BIT(TR_UseOldHCRGuardAOTRelocations), "F"},
   {"useOldIProfilerDeactivationLogic", "M\tUse Old Iprofiler Deactivation Logic", SET_OPTION_BIT(TR_UseOldIProfilerDeactivationLogic), "F", NOT_IN_SUBSET},
   {"useOptLevelAdjustment",            "M\tEnable decreasing the opt level based on load", SET_OPTION_BIT(TR_UseOptLevelAdjustment), "F", NOT_IN_SUBSET},
   {"useRIOnlyForLargeQSZ", "M\tUse RI only when the compilation queue size grows too large", SET_OPTION_BIT(TR_UseRIOnlyForLargeQSZ), "F", NOT_IN_SUBSET },
   {"userSpaceVirtualMemoryMB=", "O<nnn>\tsize of the virtual memory that is user space in MB (not used on Windows, AIX, or 64 bit systems)",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_userSpaceVirtualMemoryMB, 0, "F%d", NOT_IN_SUBSET},
   {"useSamplingJProfilingForAllFirstTimeComps","M\tHeuristic", SET_OPTION_BIT(TR_UseSamplingJProfilingForAllFirstTimeComps), "F", NOT_IN_SUBSET },
   {"useSamplingJProfilingForDLT",              "M\tHeuristic. Use samplingJProfiling for DLT methods", SET_OPTION_BIT(TR_UseSamplingJProfilingForDLT), "F", NOT_IN_SUBSET },
   {"useSamplingJProfilingForInterpSampledMethods","M\tHeuristic. Use samplingJProfiling for methods sampled by interpreter", SET_OPTION_BIT(TR_UseSamplingJProfilingForInterpSampledMethods), "F", NOT_IN_SUBSET },
   {"useSamplingJProfilingForLPQ",              "M\tHeuristic. Use samplingJProfiling for methods from low priority queue", SET_OPTION_BIT(TR_UseSamplingJProfilingForLPQ), "F", NOT_IN_SUBSET },
   {"useStrictStartupHints",        "M\tStartup hints from application obeyed strictly", SET_OPTION_BIT(TR_UseStrictStartupHints), "F", NOT_IN_SUBSET},
   {"useVmTotalCpuTimeAsAbstractTime", "M\tUse VmTotalCpuTime as abstractTime", SET_OPTION_BIT(TR_UseVmTotalCpuTimeAsAbstractTime), "F", NOT_IN_SUBSET },
   {"varyInlinerAggressivenessWithTime", "M\tVary inliner aggressiveness with abstract time", SET_OPTION_BIT(TR_VaryInlinerAggressivenessWithTime), "F", NOT_IN_SUBSET },
   {"verifyReferenceCounts", "I\tverify the sanity of object reference counts before manipulation", SET_OPTION_BIT(TR_VerifyReferenceCounts), "F"},
   {"virtualMemoryCheckFrequencySec=", "O<nnn>\tFrequency of the virtual memory check (only applicable for 32 bit systems)",
        TR::Options::setStaticNumeric, (intptrj_t)&OMR::Options::_virtualMemoryCheckFrequencySec, 0, "F%d", NOT_IN_SUBSET},
   {"waitOnCompilationQueue",        "M\tPerform synchronous wait until compilation queue empty. Primarily for use with Compiler.command", SET_OPTION_BIT(TR_WaitBit), "F", NOT_IN_SUBSET},
   {"x86HLE",         "C\tEnable haswell hardware lock elision", SET_OPTION_BIT(TR_X86HLE), "F"},
   {"x86UseMFENCE",   "M\tEnable to use mfence to handle volatile store", SET_OPTION_BIT(TR_X86UseMFENCE), "F", NOT_IN_SUBSET},
   {NULL}
};



int64_t
OMR::Options::getNumericValue(char * & option)
   {
   /* The natural way to implement operations might be to recurse to process
    * the right-hand number.  However, in a sequence of operations, that would
    * make them right-associative, which is probably just too weird and
    * unexpected to tolerate.  Hence we must do a slightly more complex thing
    * here to make the operations left-associative.
    */
   int64_t result  = 0;
   char pendingOperation = '+';
   while (pendingOperation)
      {
      int64_t current = 0;
      while (isdigit(*option))
         {
         current = 10*current + *option - '0';
         option++;
         }
      switch (pendingOperation)
         {
         case '+': result += current; break;
         case '-': result -= current; break;
         case '*': result *= current; break;
         case '/': result /= current; break;
         case '%': result %= current; break;
         }
      switch (*option)
         {
         case '+':
         case '-':
         case '*':
         case '/':
         case '%':
            pendingOperation = *option++;
            break;
         default:
            pendingOperation = 0;
            break;
         }
      }
   return result;
   }


static uintptrj_t getHexadecimalValue(char * & option)
   {
   uintptrj_t value=0;
   char *endLocation;
   value = strtol((const char *)option, &endLocation, 16);
   option = endLocation;
   return value;
   }


char *
OMR::Options::setNumeric(char *option, void *base, TR::OptionTable *entry)
   {
   *((intptrj_t*)((char*)base+entry->parm1)) = (intptrj_t)TR::Options::getNumericValue(option);
   return option;
   }


char *
OMR::Options::set32BitNumeric(char *option, void *base, TR::OptionTable *entry)
   {
   *((int32_t*)((char*)base+entry->parm1)) = (int32_t)TR::Options::getNumericValue(option);
   return option;
   }


char *
OMR::Options::set32BitNumericInJitConfig(char *option, void *base, TR::OptionTable *entry)
   {
   return TR::Options::set32BitNumeric(option, _feBase, entry);
   }


char *
OMR::Options::set64BitSignedNumeric(char *option, void *base, TR::OptionTable *entry)
   {
   int64_t sign = 1;
   if (*option == '-')
      {
      sign = -1;
      option++;
      }
   *((int64_t*)((char*)base+entry->parm1)) = sign * TR::Options::getNumericValue(option);
   return option;
   }


char *
OMR::Options::set32BitHexadecimal(char *option, void *base, TR::OptionTable *entry)
   {
   *((int32_t*)((char*)base+entry->parm1)) = getHexadecimalValue(option);
   return option;
   }


char *
OMR::Options::set32BitSignedNumeric(char *option, void *base, TR::OptionTable *entry)
   {
   int32_t sign = 1;
   if (*option == '-')
      {
      sign = -1;
      option++;
      }
   *((int32_t*)((char*)base+entry->parm1)) = sign * TR::Options::getNumericValue(option);
   return option;
   }


char *
OMR::Options::setStaticNumeric(char *option, void *base, TR::OptionTable *entry)
   {
   *((int32_t*)entry->parm1) = (int32_t)TR::Options::getNumericValue(option);
   return option;
   }


char *
OMR::Options::setStaticNumericKBAdjusted(char *option, void *base, TR::OptionTable *entry)
   {
   *((size_t*)entry->parm1) = (size_t) (TR::Options::getNumericValue(option) * 1024);
   return option;
   }


char *
OMR::Options::setStaticHexadecimal(char *option, void *base, TR::OptionTable *entry)
   {
   *((uintptrj_t*)entry->parm1) = getHexadecimalValue(option);
   return option;
   }


char *
OMR::Options::setStatic32BitValue(char *option, void *base, TR::OptionTable *entry)
   {
   *((int32_t*)((char*)entry->parm1)) = (int32_t)entry->parm2;
   return option;
   }


static char *dummy_string = "dummy";

char *
OMR::Options::setString(char *option, void *base, TR::OptionTable *entry)
   {
   char *p;
   int32_t parenNest = 0;
   for (p = option; *p; p++)
      {
      if (*p == ',')
         break;
      if (*p == '(')
         parenNest++;
      else if (*p == ')')
         {
         if ((--parenNest) < 0)
            break;
         }
      }
   int32_t len = p - option;
   p = (char *)TR::Options::jitPersistentAlloc(len+1);
   if (p)
      {
      memcpy(p, option, len);
      p[len] = 0;
      *((char**)((char*)base+entry->parm1)) = p;
      return option+len;
      }

   return dummy_string;
   }

char *
OMR::Options::setStringInJitConfig(char *option, void *base, TR::OptionTable *entry)
   {
   return TR::Options::setString(option, _feBase, entry);
   }

char *
OMR::Options::setStringForPrivateBase(char *option, void *base, TR::OptionTable *entry)
   {
#ifdef J9_PROJECT_SPECIFIC
   base = TR_J9VMBase::getPrivateConfig(_feBase);
   return TR::Options::setString(option, base, entry);
#else
   return 0;
#endif
   }


char *
OMR::Options::setStaticString(char *option, void *base, TR::OptionTable *entry)
   {
   return TR::Options::setString(option, 0, entry);
   }


char *
OMR::Options::setDebug(char *option, void *base, TR::OptionTable *entry)
   {
   if(strcmp((char*)entry->name,"trdebug="))
      {
      addDebug((char*)entry->parm1);
      return option;
      }
   else
      {
      char * position = option;
      if(*position == '{')
         {
         for(;*position;++position)
            {
            if(*position == '}')
               {
               ++position;
               break;
               }
            else if(*position == ',')
               {//replace ',' seperators with spaces so addDebug can parse them properly
               *position = ' ';
               }
            }
         }
      int32_t len = position - option-2;
      if(len > 0)
         {
         entry->parm1 = (intptrj_t)TR::Options::jitPersistentAlloc(len+1);
         if(entry->parm1)
            {
            memcpy((char*)(entry->parm1),(option+1), len);
            ((char*)entry->parm1)[len] = 0;
            addDebug((char*)entry->parm1);
            }
         }
      return position;
      }
   }


char *
OMR::Options::setRegex(char *option, void *base, TR::OptionTable *entry)
   {
   TR::SimpleRegex * regex = TR::SimpleRegex::create(option);
   *((TR::SimpleRegex**)((char*)base+entry->parm1)) = regex;
   if (!regex)
      TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
   return option;
   }


char *
OMR::Options::setStaticRegex(char *option, void *base, TR::OptionTable *entry)
   {
   TR::SimpleRegex * regex = TR::SimpleRegex::create(option);
   *((TR::SimpleRegex**)entry->parm1) = regex;
   if (!regex)
      TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
   return option;
   }


// -----------------------------------------------------------------------------
// Static data initialization
// -----------------------------------------------------------------------------

void *OMR::Options::_feBase = 0;
TR_FrontEnd *OMR::Options::_fe     = 0;
TR::Options *OMR::Options::_cmdLineOptions = 0;
TR::Options *OMR::Options::_jitCmdLineOptions = 0;
TR::Options *OMR::Options::_aotCmdLineOptions = 0;

bool OMR::Options::_hasLogFile = false;
bool OMR::Options::_suppressLogFileBecauseDebugObjectNotCreated = false;
TR_Debug *OMR::Options::_debug = 0;

bool OMR::Options::_canJITCompile = false;
bool OMR::Options::_fullyInitialized = false;

OMR::Options::VerboseOptionFlagArray OMR::Options::_verboseOptionFlags;
bool          OMR::Options::_quickstartDetected = false;

OMR::Options::SamplingJProfilingOptionFlagArray OMR::Options::_samplingJProfilingOptionFlags;

int32_t       OMR::Options::_samplingFrequency = 2; // ms

int32_t       OMR::Options::_sampleInterval = 30;
int32_t       OMR::Options::_sampleThreshold = 3000;
int32_t       OMR::Options::_startupMethodDontDowngradeThreshold = -1;

int32_t       OMR::Options::_tocSizeInKB = 256;

int32_t       OMR::Options::_aggressiveRecompilationChances = 4;

// J9, but one dependence in Compilation
int32_t       OMR::Options::_bigAppThreshold = 2000; // loaded classes



int32_t       OMR::Options::_profilingCompNodecountThreshold = 30000;	// this number should be smaller than USHRT_MAX
                                                        		// and larger than a desired size
                                                       			// to avoid cloning in more cases for hot and very hot compilation

int32_t       OMR::Options::_coldUpgradeSampleThreshold = TR_DEFAULT_COLD_UPGRADE_SAMPLE_THRESHOLD;

int32_t       OMR::Options::_interpreterSamplingDivisorInStartupMode = -1; // undefined; will be updated later
int32_t       OMR::Options::_numJitEntries = 0;
int32_t       OMR::Options::_numVmEntries = 0;
int32_t       OMR::Options::_numVecRegsToLock=0;
int32_t       OMR::Options::_hotFieldThreshold = 100;
int32_t       OMR::Options::_maxNumPrexAssumptions = 209;
int32_t       OMR::Options::_maxNumVisitedSubclasses = 500;

int32_t       OMR::Options::_minProfiledCheckcastFrequency = 20; // as a percentage
int32_t       OMR::Options::_lowCodeCacheThreshold = 256*1024; // 256KB Turn off Iprofiler if available code cache space is lower than this value

// If the compilation queue weight increases too much, the JIT
// may make the application thread to sleep()
// Used only on LINUX where lack of thread priorities can lead to compilation thread starvation
// Use a large value to disable the feature. Suggested default == 3200
int32_t       OMR::Options::_queueWeightThresholdForAppThreadYield = -1; // not yet set
int32_t       OMR::Options::_queueWeightThresholdForStarvation = -1; // not yet set

uint32_t       OMR::Options::_memExpensiveCompThreshold = 0; // not initialized
uint32_t       OMR::Options::_cpuExpensiveCompThreshold = 0; // not initialized
int32_t       OMR::Options::_deterministicMode = -1; // -1 means we're not in any deterministic mode
int32_t       OMR::Options::_maxPeekedBytecodeSize = 100000;

int32_t       OMR::Options::INLINE_failedToDevirtualize          = 0;
int32_t       OMR::Options::INLINE_failedToDevirtualizeInterface = 0;
int32_t       OMR::Options::INLINE_fanInCallGraphFactor          = 90;
int32_t       OMR::Options::INLINE_calleeToBig                   = 0;
int32_t       OMR::Options::INLINE_calleeToDeep                  = 0;
int32_t       OMR::Options::INLINE_calleeHasTooManyNodes         = 0;
int32_t       OMR::Options::INLINE_ranOutOfBudget                = 0;
int64_t       OMR::Options::INLINE_calleeToBigSum                = 0;
int64_t       OMR::Options::INLINE_calleeToDeepSum               = 0;
int64_t       OMR::Options::INLINE_calleeHasTooManyNodesSum      = 0;

int32_t       OMR::Options::_inlinerVeryLargeCompiledMethodAdjustFactor = 20;

int32_t       OMR::Options::_numUsableCompilationThreads = -1; // -1 means not initialized

int32_t       OMR::Options::_trampolineSpacePercentage = 0; // 0 means no change from default

bool          OMR::Options::_realTimeGC=false;

bool          OMR::Options::_countsAreProvidedByUser = false;
TR_YesNoMaybe OMR::Options::_startupTimeMatters = TR_maybe;

bool          OMR::Options::_sharedClassCache=false;

TR::OptionSet *OMR::Options::_currentOptionSet = NULL;
char *        OMR::Options::_compilationStrategyName = "default";

bool          OMR::Options::_optionsTablesValidated = false;

bool          OMR::Options::_dualLogging = false; // a log file can be used in two different option sets, or in
                                                // in the master TR::Options object and in an option set
bool          OMR::Options::_logsForOtherCompilationThreadsExist = false;

int32_t       OMR::Options::_aggressivenessLevel = -1; // -1 means not set

int32_t       OMR::Options::_numIProfiledCallsToTriggerLowPriComp = 250;
int32_t       OMR::Options::_qsziMaxToTrackLowPriComp = 40; // If number of queued first time compilations is lower we allow triggering of low priority compilations
int32_t       OMR::Options::_delayToEnableIdleCpuExploitation = 100; // ms
int32_t       OMR::Options::_countPercentageForEarlyCompilation = 50;

int32_t       OMR::Options::_sampleDensityBaseThreshold = 200;
int32_t       OMR::Options::_sampleDensityIncrementThreshold = 400;

int32_t       OMR::Options::_abstractTimeGracePeriod = -1; // -1 mean not set
int32_t       OMR::Options::_abstractTimeToReduceInliningAggressiveness = -1; // -1 means not set;

int32_t       OMR::Options::_processOptionsStatus = TR_Unprocessed;
#ifdef TR_HOST_X86
int32_t       OMR::Options::_TransactionalMemoryRetryCount = 2048;
#else
int32_t       OMR::Options::_TransactionalMemoryRetryCount = 1;
#endif

int32_t       OMR::Options::_minimalNumberOfTreeTopsInsideTMMonitor = 6;

TR::SimpleRegex *OMR::Options::_debugCounterInsertByteCode = NULL;
TR::SimpleRegex *OMR::Options::_debugCounterInsertJittedBody = NULL;
TR::SimpleRegex *OMR::Options::_debugCounterInsertMethod = NULL;

size_t OMR::Options::_scratchSpaceLimit = 0;
size_t OMR::Options::_scratchSpaceLowerBound = 0;

uint32_t OMR::Options::_minBytesToLeaveAllocatedInSharedPool = 1024*512; // 512kb
uint32_t OMR::Options::_maxBytesToLeaveAllocatedInSharedPool = 1024*1024*25; //25MB


// -----------------------------------------------------------------------------
// Move to J9

int32_t       OMR::Options::_classExtendRatSize = -1; // -1 means not set
int32_t       OMR::Options::_methodOverrideRatSize = -1; // -1 means not set
int32_t       OMR::Options::_classRedefinitionUPICRatSize = -1; // -1 means not set

// needs Option init work
int32_t       OMR::Options::_userSpaceVirtualMemoryMB = 1;

uint32_t      OMR::Options::_virtualMemoryCheckFrequencySec = 60;
uint32_t      OMR::Options::_cpuUsageCircularBufferUpdateFrequencySec = 10;
uint32_t      OMR::Options::_cpuUsageCircularBufferSize = 0;

int32_t       OMR::Options::_compThreadCPUEntitlement = -1; // -1 means not set or feature disabled
int32_t       OMR::Options::_minSleepTimeMsForCompThrottling = 1; // ms
int32_t       OMR::Options::_maxSleepTimeMsForCompThrottling = 50; // ms
int32_t       OMR::Options::_startThrottlingTime = 0; // ms
int32_t       OMR::Options::_stopThrottlingTime = 0; // ms. 0 means no expiration time

//
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// Startup / initialization
// -----------------------------------------------------------------------------

bool OMR::Options::createDebug()
   {
   _debug = _fe->createDebug();
   return _debug != 0;
   }


bool
OMR::Options::requiresDebugObject()
   {
   #if defined(DEBUG)
      return true;
   #endif

   if (OMR::Options::hasSomeLogFile() ||
       OMR::Options::isOptionSetForAnyMethod(TR_EntryBreakPoints) ||
       OMR::Options::isOptionSetForAnyMethod(TR_DebugBeforeCompile) ||
       OMR::Options::isOptionSetForAnyMethod(TR_DebugOnEntry))
      {
      return true;
      }

   static const char *TR_DEBUGisSet = feGetEnv("TR_DEBUG");
   if (TR_DEBUGisSet)
      {
      return true;
      }

   return false;
   }


#if DEBUG
static char  categories[] = // Must match optionCategories
   {
   ' ', // General options
   'C', // Codegen
   'O', // Optimizer
   'L', // Logging
   'D', // Debug
   'R', // Recompilation
   'I', // Internal (ie. not displayed with -Xjit:help)
   'M', // Miscellaneous
   0    // (null terminator)
   };
#endif



OMR::Options::Options(
      TR_Memory * trMemory,
      int32_t index,
      int32_t lineNum,
      TR_ResolvedMethod *compilee,
      void *oldStartPC,
      TR_OptimizationPlan *optimizationPlan,
      bool isAOT,
      int32_t compThreadID) :
   _optionSets(0),
   _logListForOtherCompThreads(0)
   {
   TR_ASSERT(optimizationPlan, "Must have an optimization plan when calling this method");

   // if this is the first compile -- try to figure out the hotness from the optionsets...
   //
   TR::OptionSet *optionSet = TR::Options::findOptionSet(trMemory, index, lineNum, compilee, optimizationPlan->getOptLevel(), isAOT);

   TR::Options *masterOptions;
   if (optionSet)
      masterOptions = optionSet->getOptions();
   else if (isAOT)
      masterOptions = _aotCmdLineOptions;
   else
      masterOptions = _jitCmdLineOptions;
   *this = *masterOptions;

   // At this point this object contains the log for compThreadId==0
   // If this is a different compilation thread we need to find the right log
   //
   if (_logFileName && compThreadID > 0 && !_suppressLogFileBecauseDebugObjectNotCreated)
      {
      self()->setLogForCompilationThread(compThreadID, masterOptions);
      }

   if (optimizationPlan->disableCHOpts())
      {
      self()->setOption(TR_DisableCHOpts);
      }

   // preexistence is based on CHTable, so if we don't want to use CHTable,
   // don't use preexistence either
   //
   if (self()->getOption(TR_DisableCHOpts))
      {
      _disabledOptimizations[invariantArgumentPreexistence] = true;
      self()->setOption(TR_DisableIPA, true);
      }

#ifdef J9_PROJECT_SPECIFIC
   if (oldStartPC)
      {
      // The following assume may fail when we specify a fixed opt level for the JIT options
      // but not for the AOT options and the method is AOTed. The solution is to keep the
      // JIT and AOT options in sync
      TR_ASSERT((self()->getOptLevel() == -1) || optimizationPlan->isLogCompilation(),
             "cannot be recompiling a method when a fixed opt level has been specified");

      // For methods that get repeatedly invalidated, disable preexistence
      //
      TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(oldStartPC);
      if (bodyInfo->getIsInvalidated())
         {
         if (bodyInfo->getMethodInfo()->getNumberOfInvalidations() >= 2)
            {
            this->_disabledOptimizations[invariantArgumentPreexistence] = true;
            }
         }
      }
#endif

   if(_optLevel != -1) // has been set intentionally--respect it
      {
      optimizationPlan->setOptLevel((TR_Hotness)_optLevel);
      if (self()->allowRecompilation())
         self()->setAllowRecompilation(false); // disable recompilation for this method
      optimizationPlan->setOptLevelDowngraded(false);
      }

   if (self()->getOption(TR_FullSpeedDebug))
      {
      if (self()->getOption(TR_MimicInterpreterFrameShape))
         {
         optimizationPlan->setOptLevel(noOpt);
         self()->setOption (TR_DisableInterpreterProfiling, true);
         if (self()->allowRecompilation())
            self()->setAllowRecompilation(false); // disable recompilation for this method
         optimizationPlan->setOptLevelDowngraded(false);
         }
      }

   // now set the current opt level in TR::Options
   //
   _optLevel = optimizationPlan->getOptLevel();
   _optLevelDowngraded = optimizationPlan->isOptLevelDowngraded();
   if(optimizationPlan->isLogCompilation())
      {
      if (_debug || TR::Options::createDebug())
         _logFile = optimizationPlan->getLogCompilation();
      }

   /*
    * If there is no debug, _suppressLogFileBecauseDebugObjectNotCreated is set in rossa.
    * However for recompiling after crash we create the debug object then,
    * and thus we don't want to suppress the log.
    */
   if (_suppressLogFileBecauseDebugObjectNotCreated && !optimizationPlan->isLogCompilation())
      _logFile = NULL;

   }


OMR::Options::Options(TR::Options &other) :
   _optionSets(0),
   _logListForOtherCompThreads(0)
   {
   *this = other;
   if (_suppressLogFileBecauseDebugObjectNotCreated)
      _logFile = NULL;
   }


char *
OMR::Options::latePostProcessJIT(void *jitConfig)
   {
   if (_jitCmdLineOptions)
      return TR::Options::latePostProcess(_jitCmdLineOptions, jitConfig, false);
   return dummy_string;
   }


char *
OMR::Options::latePostProcessAOT(void *jitConfig)
   {
   if (_aotCmdLineOptions)
      return TR::Options::latePostProcess(_aotCmdLineOptions, jitConfig, true);
   return dummy_string;
   }


char *
OMR::Options::latePostProcess(TR::Options *options, void *jitConfig, bool isAOT)
   {
   char * rc = 0;

   // If nobody has set the number of compilation threads by now, use a value of 1
   if (_numUsableCompilationThreads <= 0)
      _numUsableCompilationThreads = 1;

   // feLatePostProcess has to be called before jitLatePostProcess as the fe call may effect the opt level
   //
   if (!options->feLatePostProcess(_feBase, NULL))
      /* Returns false when any of the hooks cannot be disabled and FSD is disabled */
      rc = (char*) 1;

   if (!options->jitLatePostProcess(NULL, jitConfig))
      return options->_startOptions;

   // Now process any option sets that we have saved
   //
   TR::OptionSet *optionSet;
   for (optionSet = options->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      // Get the option string before clobbering it with the options object
      //
      _currentOptionSet = optionSet;
      char *subOpts = optionSet->getOptionString();
      TR::Options *newOptions = new (PERSISTENT_NEW) TR::Options(*options);
      if (newOptions)
         {
         optionSet->setOptions(newOptions);
         subOpts = TR::Options::processOptionSet(subOpts, optionSet, optionSet->getOptions(), isAOT);
         if (*subOpts != ')')
            return subOpts;
         if (!optionSet->getOptions()->jitLatePostProcess(optionSet, jitConfig))
            return options->_startOptions;
         if (!optionSet->getOptions()->feLatePostProcess(_feBase, optionSet))
            return options->_startOptions;
         if (optionSet->getIndex() == TR_EXCLUDED_OPTIONSET_INDEX)
            TR::Options::findOrCreateDebug()->addExcludedMethodFilter(isAOT); // Not sure isAOT is exactly the right thing to pass here
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   // Print PID number and date
   //
   if (options->showPID())
      TR::Options::printPID();
#endif

   // If required, print the options in effect
   //
   if (options->showOptionsInEffect())
      {
      options->printOptions(options->_startOptions, options->_envOptions);
      }

   return rc;
   }


bool
OMR::Options::jitLatePostProcess(TR::OptionSet *optionSet, void * jitConfig)
   {
   if (_sampleInterval == 0) // sampleInterval==0 does make much sense
      _sampleInterval = 1;

#if defined(TR_HOST_ARM)
   // OSR is not available for ARM yet
   self()->setOption(TR_DisableOSR);
   self()->setOption(TR_EnableOSR, false);
   self()->setOption(TR_EnableOSROnGuardFailure, false);
#endif

#ifndef J9_PROJECT_SPECIFIC
   self()->setOption(TR_DisableNextGenHCR);
#endif

   static const char *ccr = feGetEnv("TR_DisableCCR");
   if (ccr)
      {
      self()->setOption(TR_DisableCodeCacheReclamation);
      }
   static const char *disableCCCF = feGetEnv("TR_DisableClearCodeCacheFullFlag");
   if (disableCCCF)
      {
      self()->setOption(TR_DisableClearCodeCacheFullFlag);
      }

   if (self()->getOption(TR_FullSpeedDebug))
      {
      if (!self()->getOption(TR_DisableOSR))
         self()->setOption(TR_EnableOSR); // Make OSR the default for FSD
      self()->setOption(TR_DisableShrinkWrapping);
      self()->setOption(TR_DisableMethodHandleThunks); // Can't yet transition a MH thunk frame into equivalent interpreter frames
      }

   if (self()->getOption(TR_EnableOSROnGuardFailure) && !self()->getOption(TR_DisableOSR))
      self()->setOption(TR_EnableOSR);

   if (TR::Compiler->om.mayRequireSpineChecks())
      {
      OMR::Options::getCmdLineOptions()->setOption(TR_DisableInternalPointers);
      OMR::Options::getCmdLineOptions()->setDisabled(idiomRecognition, true);
      OMR::Options::getCmdLineOptions()->setDisabled(rematerialization, true);

      if (OMR::Options::getAOTCmdLineOptions())
         {
         OMR::Options::getAOTCmdLineOptions()->setOption(TR_DisableInternalPointers);
         OMR::Options::getAOTCmdLineOptions()->setDisabled(idiomRecognition, true);
         OMR::Options::getAOTCmdLineOptions()->setDisabled(rematerialization, true);
         }
      }

   static const char *ipm = feGetEnv("TR_IProfileMore");
   if (ipm)
      {
#ifdef J9_PROJECT_SPECIFIC
      TR::Options::_maxIprofilingCount = 3000;
      TR::Options::_maxIprofilingCountInStartupMode = 3000;
      TR::Options::_iProfilerMemoryConsumptionLimit = 50000000;
      TR::Options::_profileAllTheTime = 1;
#endif

      OMR::Options::_interpreterSamplingDivisorInStartupMode = 1;
      self()->setOption(TR_DisableDynamicLoopTransfer);
      }

   if (!optionSet)
      {
      if (self()->getFixedOptLevel() == -1 && self()->getOption(TR_InhibitRecompilation))
         {
         self()->setOption(TR_DisableUpgradingColdCompilations);
         self()->setOption(TR_DisableGuardedCountingRecompilations);
         self()->setOption(TR_DisableDynamicLoopTransfer);
         self()->setOption(TR_DisableEDO);
         self()->setOption(TR_DisableAggressiveRecompilations);
         self()->setOption(TR_EnableHardwareProfileRecompilation, false);
         OMR::Options::_sampleThreshold = 0;

#ifdef J9_PROJECT_SPECIFIC
         TR::Options::_scorchingSampleThreshold = 0;
         TR::Options::_resetCountThreshold = 0;
#endif
         }
      // If the intent was to start with warm, disable downgrades
      if (_initialOptLevel == warm)
         self()->setOption(TR_DontDowngradeToCold);

#ifdef J9_PROJECT_SPECIFIC
      if (TR::Options::_qszThresholdToDowngradeOptLevel == -1) // not yet set
         {
         if (TR::Compiler->target.numberOfProcessors() <= 2)
            TR::Options::_qszThresholdToDowngradeOptLevel = 500;
         else
            TR::Options::_qszThresholdToDowngradeOptLevel = 3000;
         }
#endif

      if (_queueWeightThresholdForAppThreadYield == -1)
         _queueWeightThresholdForAppThreadYield = TR::Compiler->target.numberOfProcessors() <= 2 ? 10000 : 22000;
      if (_queueWeightThresholdForStarvation == -1)
         _queueWeightThresholdForStarvation = TR::Compiler->target.numberOfProcessors() <= 2 ? 1600 : 3200;

      // enable by default rampup improvements
      if (! (TR::Options::getCmdLineOptions()    && TR::Options::getCmdLineOptions()->getOption(TR_DisableRampupImprovements) ||
             TR::Options::getAOTCmdLineOptions() && TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableRampupImprovements)))
         {
         self()->setOption(TR_EnableDowngradeOnHugeQSZ);
         self()->setOption(TR_EnableMultipleGCRPeriods);
         // enable GCR filtering

#ifdef J9_PROJECT_SPECIFIC
         if (TR::Options::_smallMethodBytecodeSizeThresholdForCold == -1) // not yet set
            // 8, 16, 32 depending on how many processors we have
            TR::Options::_smallMethodBytecodeSizeThresholdForCold = std::max<int32_t>(8, (TR::Compiler->target.numberOfProcessors() != 0 ? 32/TR::Compiler->target.numberOfProcessors() : 8));
#endif


#ifdef LINUX // On Linux compilation threads can be starved
         self()->setOption(TR_EnableAppThreadYield);
#endif
#if defined(TR_TARGET_X86)
         // Currently GCR patching only works correctly on x86
         self()->setOption(TR_EnableGCRPatching);
#else

#ifdef J9_PROJECT_SPECIFIC
         TR::Options::_GCRQueuedThresholdForCounting = 400; // stop GCR counting if too many GCR requests are queued
#endif

#endif
         }
      if (self()->getOption(TR_DisableRampupImprovements))
         {
         self()->setOption(TR_DisableConservativeHotRecompilationForServerMode);
         }

      if (OMR::Options::_memExpensiveCompThreshold == 0) // not yet set
         OMR::Options::_memExpensiveCompThreshold = TR::Options::isQuickstartDetected() ? TR_QUICKSTART_MEM_EXPENSIVE_COMP_THRESHOLD : TR_DEFAULT_MEM_EXPENSIVE_COMP_THRESHOLD;

      if (OMR::Options::_cpuExpensiveCompThreshold == 0) // not yet set
         OMR::Options::_cpuExpensiveCompThreshold = TR::Options::isQuickstartDetected() ? TR_QUICKSTART_CPU_EXPENSIVE_COMP_THRESHOLD : TR_DEFAULT_CPU_EXPENSIVE_COMP_THRESHOLD;

      // TODO: make cold inliner less aggressive for 2P or less

#ifdef J9_PROJECT_SPECIFIC
      if (TR::Options::_catchSamplingSizeThreshold == -1) // not yet set
         {
         TR::Options::_catchSamplingSizeThreshold = 1100; // in number of nodes
         if (TR::Compiler->target.numberOfProcessors() <= 2)
            TR::Options::_catchSamplingSizeThreshold = 850;
         }
#endif

      if (_startupMethodDontDowngradeThreshold == -1) // not yet set
         {
         _startupMethodDontDowngradeThreshold = 300;
         if (TR::Compiler->target.numberOfProcessors() <= 2)
            {
            _startupMethodDontDowngradeThreshold = 100; // avoid being aggressive on small number of processors
            }
         }

      if (TR::Compiler->target.numberOfProcessors() <= 2)
         self()->setOption(TR_ConservativeCompilation, true);

      if (TR::Options::isQuickstartDetected()
#if defined(J9ZOS390)
         || sharedClassCache()  // Disable GCR for zOS if SharedClasses/AOT is used
#endif
         )
         {
         // Disable GCR in quickstart mode, but provide the option to enable it
         static char *gcr = feGetEnv("TR_EnableGuardedCountingRecompilations");
         if (!gcr)
            self()->setOption(TR_DisableGuardedCountingRecompilations);
         }

      if (TR::Options::sharedClassCache()) // If AOT shared classes is used
         {
         // If interpreter profiling is not used then we can become more aggressive with scount
         if (self()->getOption(TR_DisableInterpreterProfiling))
            {
            // If scount has not been changed on the command line, adjust it here
            if (self()->getInitialSCount() == TR_INITIAL_SCOUNT)
               {
               _initialSCount = TR_QUICKSTART_INITIAL_SCOUNT;
               }
            // disable DelayRelocationForAOTCompilations feature because the whole
            // purpose of the feature is to gather more Iprofiler information
            self()->setOption(TR_DisableDelayRelocationForAOTCompilations, true);
            }
         else // IProfiler is enabled
            {
            if (TR::Options::getAOTCmdLineOptions()->getOption(TR_DisablePersistIProfile) ||
               TR::Options::getCmdLineOptions()->getOption(TR_DisablePersistIProfile))
               {
               self()->setOption(TR_DisablePersistIProfile);
               }
            }
         // In quickstart + AOT mode we want to change the count string to something more aggressive
         if (TR::Options::isQuickstartDetected())
            {
            if (_coldUpgradeSampleThreshold == TR_DEFAULT_COLD_UPGRADE_SAMPLE_THRESHOLD)
               _coldUpgradeSampleThreshold = 2;
            }
         }
      else // No AOT
         {
         // To minimize risk, don't use this feature for non-AOT cases
         // Note that information about AOT is only available in late processing stages
         self()->setOption(TR_ActivateCompThreadWhenHighPriReqIsBlocked, false);
         }

      if (OMR::Options::_abstractTimeGracePeriod == -1) // not set
         {
         OMR::Options::_abstractTimeGracePeriod = self()->getOption(TR_UseVmTotalCpuTimeAsAbstractTime) ?
            DEFAULT_ABSTRACT_TIME_MS_CPU_GRACE_PERIOD : DEFAULT_ABSTRACT_TIME_SAMPLES_GRACE_PERIOD;
         }
      if (OMR::Options::_abstractTimeToReduceInliningAggressiveness == -1) // not set
         {
         OMR::Options::_abstractTimeToReduceInliningAggressiveness = self()->getOption(TR_UseVmTotalCpuTimeAsAbstractTime) ?
            DEFAULT_ABSTRACT_TIME_MS_CPU_TO_REDUCE_INLINING_AGGRESSIVENESS : DEFAULT_ABSTRACT_TIME_SAMPLES_TO_REDUCE_INLINING_AGGRESSIVENESS;
         }

      if (self()->getOption(TR_MimicInterpreterFrameShape))
         {
         if (self()->getFixedOptLevel() != -1 && self()->getFixedOptLevel() != noOpt)
            TR_VerboseLog::write("<JIT: FullSpeedDebug: ignoring user specified optLevel>\n");
         if (_countString)
            {
            //if quickstart is enabled, then message saying it is incompatable with fsdb
            if (self()->isVerboseFileSet())
               {
               if (TR::Options::isQuickstartDetected())
                  {
                  TR_VerboseLog::write("<JIT: FullSpeedDebug: ignoring -Xquickstart option>\n");
                  }
               else
                  {
                  TR_VerboseLog::write("<JIT: FullSpeedDebug: ignoring countString>\n");
                  }
               }
            }

         _countString = 0;
         }

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseOptimizer)
         || _lastOptSubIndex != INT_MAX)
         {
         self()->setOption(TR_CountOptTransformations);
         if(!_debug)
            TR::Options::createDebug();
         }

      if (self()->setCounts())
         return false; // bad string count

      // If Iprofiler is disabled we will not have block frequencies so we should
      // disable the logic that makes inlining more conservative based on block frequencies
      if (self()->getOption(TR_DisableInterpreterProfiling))
         {
         TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableConservativeInlining);
         TR::Options::getCmdLineOptions()->setOption(TR_DisableConservativeInlining);
         TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableConservativeColdInlining);
         TR::Options::getCmdLineOptions()->setOption(TR_DisableConservativeColdInlining);
         }

      if (self()->getOption(TR_DisableCompilationThread))
         self()->setOption(TR_DisableNoVMAccess);

      // YieldVMAccess and NoVMAccess are incompatible. If the user enables YieldVMAccess
      // make sure NoVMAccess is disabled
      //
      if (self()->getOption(TR_EnableYieldVMAccess) && !self()->getOption(TR_DisableNoVMAccess))
         self()->setOption(TR_DisableNoVMAccess);
      }
   else // option set processing
      {
      _logFile = NULL;
      if (_logFileName)
         {
         if (!_debug)
            TR::Options::createDebug();

         if (_debug)
            {
            _logFile = _debug->findLogFile(TR::Options::getAOTCmdLineOptions(), TR::Options::getJITCmdLineOptions(), optionSet, _logFileName);
            if (_logFile == NULL)
               self()->openLogFile();
            else
               OMR::Options::_dualLogging = true; // a log file is used in two different option sets, or in
                                                // in the master TR::Options object and in an option set
            }
         }
      else if (self()->requiresLogFile())
         {
         if (this == TR::Options::getAOTCmdLineOptions())
            TR_VerboseLog::write("<AOT");
         else
            TR_VerboseLog::write("<JIT");
         TR_VerboseLog::write(": trace options require a log file to be specified: log=<filename>)>\n");
         return false;
         }

#ifdef J9_PROJECT_SPECIFIC
      // Compiler.compile() interface
      //
      if (self()->getOption(TR_CompileBit))
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
         fej9->compileMethods(optionSet, jitConfig);
         if (self()->getOption(TR_WaitBit))
            {
            TR_VerboseLog::write("Will call waitOnCompiler\n");
            fej9->waitOnCompiler(jitConfig);
            }
         }
#endif

      }

   // The adding of the enumeration of register names takes up a lot
   // of heap storage (as much as 100+ MBs) for very large programs. Here
   // we choose when we want to see these names

   if (self()->requiresLogFile() || self()->getBreakOnCreate() || self()->getDebugOnCreate())
      _addressToEnumerate |= TR_EnumerateRegister;

   if (self()->getOption(TR_ImmediateCountingRecompilation))
      self()->setOption(TR_EnableGCRPatching, false);

   if (self()->getOption(TR_DisableLockResevation))
      {
         self()->setOption(TR_ReservingLocks, false);
      }
#if defined(TR_HOST_S390)
   // Lock reservation without OOL has not implemented on Z
   if (self()->getOption(TR_DisableOOL))
      self()->setOption(TR_ReservingLocks,false);
#endif

   return true;
   }


// The string passed in as options should be from the -Xjit option
char *
OMR::Options::processOptionsJIT(char *jitOptions, void *feBase, TR_FrontEnd *fe)
   {
   // Create the default JIT command line options object
   // only do this if this the first time around we're processing options
   //
   if (!_jitCmdLineOptions)
      {
      _jitCmdLineOptions = new (PERSISTENT_NEW) TR::Options();
      _cmdLineOptions = _jitCmdLineOptions;
      }

   if (_jitCmdLineOptions)
      memset(_jitCmdLineOptions, 0, sizeof(TR::Options));

   _feBase = feBase;
   _fe     = fe;

   if (_jitCmdLineOptions)
      {

      if (!(_jitCmdLineOptions->fePreProcess(feBase)))
         {
         _processOptionsStatus |= TR_JITProcessErrorFE;

         return dummy_string;
         }

      static char *envOptions = feGetEnv("TR_Options");

      _jitCmdLineOptions->jitPreProcess();

      char *rc = TR::Options::processOptions(jitOptions, envOptions, feBase, fe, _jitCmdLineOptions);

      _processOptionsStatus |= (NULL == rc)?TR_JITProcessedOK : TR_JITProcessErrorJITOpts;
      return rc;
      }

   _processOptionsStatus |= TR_JITProcessErrorUnknown;
   return dummy_string;
   }


// The string passed in as options should be from the -Xaot option
char *
OMR::Options::processOptionsAOT(char *aotOptions, void *feBase, TR_FrontEnd *fe)
   {
   // Create the default AOT command line options object
   // only do this if this the first time around we're processing options
   //
   if (!_aotCmdLineOptions)
      _aotCmdLineOptions = new (PERSISTENT_NEW) TR::Options();

   if (_aotCmdLineOptions)
      memset(_aotCmdLineOptions, 0, sizeof(TR::Options));

   _feBase = feBase;
   _fe     = fe;

   if (_aotCmdLineOptions)
      {

      if (!(_aotCmdLineOptions->fePreProcess(feBase)))
         {
         _processOptionsStatus |= TR_AOTProcessErrorFE;
         return dummy_string;
         }

      _aotCmdLineOptions->jitPreProcess();

      static char *envOptions = feGetEnv("TR_OptionsAOT");
      char* rc = TR::Options::processOptions(aotOptions, envOptions, feBase, fe, _aotCmdLineOptions);

      _processOptionsStatus |= (NULL == rc)?TR_AOTProcessedOK : TR_AOTProcessErrorAOTOpts;
      return rc;
      }

   _processOptionsStatus |= TR_AOTProcessErrorUnknown;
   return dummy_string;
   }


void
OMR::Options::jitPreProcess()
   {

   // --------------------------------------------------------------------------
   // All projects
   //

   //Disabling Shrink Wrapping on all platforms (functional issues)
   self()->setOption(TR_DisableShrinkWrapping);

   self()->setOption(TR_RestrictStaticFieldFolding);

   if (TR::Compiler->target.cpu.isPower())
      self()->setOption(TR_DisableRegisterPressureSimulation);

#if defined(TR_HOST_ARM)
   // alignment problem for float/double
   self()->setOption(TR_DisableIntrinsics);
#endif

#if defined(RUBY_PROJECT_SPECIFIC)
   // Ruby has been known to spawn other Ruby VMs.  Log filenames must be unique
   // or corruption will occur.
   //
   bool forceSuffixLogs = true;
#else
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   bool forceSuffixLogs = false;
#else
   bool forceSuffixLogs = true;
#endif
#endif

   if (forceSuffixLogs)
      self()->setOption(TR_EnablePIDExtension);


   // The signature-hashing seed algorithm it the best default.
   // Unless the user specifies randomSeed=nosignature, we want to override the
   // default seed we just set above in order to improve reproducibility.
   //
   self()->setOption(TR_RandomSeedSignatureHash);
   self()->setOption(TR_DisableRefinedAliases);


   _disabledOptimizations[blockShuffling]    = true;
   _disabledOptimizations[IVTypeTransformation] = true;
   _disabledOptimizations[basicBlockHoisting] = true;

   self()->setOption(TR_DisableTreePatternMatching);
   self()->setOption(TR_DisableHalfSlotSpills);

   _optLevel = -1;
   _initialOptLevel = _quickstartDetected ? cold : -1; // Not initialized
   _initialCount  = -1;
   _initialBCount = -1;
   _initialMILCount = -1;
   _initialColdRunCount = -1;
   _initialColdRunBCount = -1;
   _initialSCount = TR_INITIAL_SCOUNT;
   _lastOptIndex = INT_MAX;
   _lastOptSubIndex = INT_MAX;
   _lastSearchCount = INT_MAX;
   _firstOptTransformationIndex = self()->getMinFirstOptTransformationIndex();
   _lastOptTransformationIndex = self()->getMaxLastOptTransformationIndex();

   _cgTrace = 0;
   _raTrace = 0;
   _storeSinkingLastOpt = -1;
#ifdef J9_PROJECT_SPECIFIC
   _profilingCount = DEFAULT_PROFILING_COUNT;
   _profilingFrequency = DEFAULT_PROFILING_FREQUENCY;
#endif

#if defined(TR_HOST_POWER)
   _bigCalleeThreshold = 300;
#elif defined(TR_HOST_S390)
   _bigCalleeThreshold = 600;
#else
   _bigCalleeThreshold = 400;
#endif
   _bigCalleeThresholdForColdCallsAtWarm = 400;
   _bigCalleeThresholdForColdCallsAtHot = 500;
   _bigCalleeFreqCutoffAtWarm = 0;
   _bigCalleeHotOptThreshold = 600;
   _bigCalleeFreqCutoffAtHot = 40;
   _bigCalleeScorchingOptThreshold = 800;
#if defined(TR_HOST_S390)
   _inlinerVeryLargeCompiledMethodThreshold = 230;
#elif defined(TR_HOST_X86)
   _inlinerVeryLargeCompiledMethodThreshold = 150;
#elif defined(TR_HOST_POWER)
   _inlinerVeryLargeCompiledMethodThreshold = 210;
#else
   _inlinerVeryLargeCompiledMethodThreshold = 300;
#endif
   _inlinerVeryLargeCompiledMethodFaninThreshold = 1;
   _maxSzForVPInliningWarm = 8;
   _largeCompiledMethodExemptionFreqCutoff = 10000;
   _inlineCntrCalleeTooBigBucketSize = INT_MAX;
   _inlineCntrColdAndNotTinyBucketSize = INT_MAX;
   _inlineCntrWarmCalleeTooBigBucketSize = INT_MAX;
   _inlineCntrRanOutOfBudgetBucketSize = INT_MAX;
   _inlineCntrCalleeTooDeepBucketSize = INT_MAX;
   _inlineCntrWarmCallerHasTooManyNodesBucketSize = INT_MAX;
   _inlineCntrWarmCalleeHasTooManyNodesBucketSize = INT_MAX;
   _inlineCntrDepthExceededBucketSize = INT_MAX;
   _inlineCntrAllBucketSize = INT_MAX;
   _maxInlinedCalls = INT_MAX;
   _dumbInlinerBytecodeSizeMaxCutoff = 800; // 800 bytecodes; higher value means more inlining
   _dumbInlinerBytecodeSizeMinCutoff = 200; // 200 bytecodes; higher value means more inlining
   _dumbInlinerBytecodeSizeCutoff = _dumbInlinerBytecodeSizeMaxCutoff; // may be changed dynamically; cannot be changed directly with an option
   _dumbInlinerBytecodeSizeDivisor = 4; // higher value means more inlining
   _trivialInlinerMaxSize = TRIVIAL_INLINER_MAX_SIZE;
   _inlinerArgumentHeuristicFractionUpToWarm = 999999; // (Huge value here essentially disables this heuristic at warm)
   _inlinerArgumentHeuristicFractionBeyondWarm = 2;
   _inlinerVeryColdBorderFrequencyAtCold = -1; // -1 means not set
   _inlinerBorderFrequency = -1;
   _inlinerColdBorderFrequency = -1;
   _inlinerVeryColdBorderFrequency = -1;
   _inlinerCGBorderFrequency = -1;
   _inlinerCGColdBorderFrequency = -1;
   _inlinerCGVeryColdBorderFrequency = -1;
   _alwaysWorthInliningThreshold = 15;
   _maxLimitedGRACandidates = TR_MAX_LIMITED_GRA_CANDIDATES;
   _maxLimitedGRARegs = TR_MAX_LIMITED_GRA_REGS;
   _counterBucketGranularity = 2;
   _minCounterFidelity = INT_MIN;
   _labelTargetNOPLimit = TR_LABEL_TARGET_NOP_LIMIT;
   _lastIpaOptTransformationIndex = INT_MAX;
#if defined(TR_HOST_POWER)
   _jitMethodEntryAlignmentBoundary = 128;
#else
   _jitMethodEntryAlignmentBoundary = 0;
#endif
   _blockShufflingSequence = "S";
   _delayCompile = 0;
   _largeNumberOfLoops = 6500;

   // The entry block is under this threshold in methods containing a block
   // expected to run >=100 times per method entry, which should be sure to
   // catch loops that run thousands of times.
   _loopyAsyncCheckInsertionMaxEntryFreq = 100;

#if defined(TR_TARGET_64BIT)
   self()->setOption(TR_EnableCodeCacheConsolidation);
#endif

   // --------------------------------------------------------------------------
   // J9-only
   //
   if (TR::isJ9())
      {
      self()->setOption(TR_DisableInliningDuringVPAtWarm, true); // This reduces compilation overhead

      // Only J9 distinguishes how it allocates scratch memory segments
      self()->setOption(TR_EnableVirtualScratchMemory, true);

      self()->setOption(TR_UseHigherCountsForNonSCCMethods);
      self()->setOption(TR_UseHigherMethodCountsAfterStartup);

      // On Linux we use this option so that, if the application doesn't send a startup hint
      // in the first second, we are allowed to disableSelectiveNoServer
      if (((TR::Compiler->target.cpu.isPower() && TR::Compiler->target.isLinux()) && TR::Compiler->target.numberOfProcessors() >= 8) ||
          ((TR::Compiler->target.cpu.isX86() && TR::Compiler->target.isLinux()) && TR::Compiler->target.numberOfProcessors() >= 4))
          self()->setOption(TR_TurnOffSelectiveNoOptServerIfNoStartupHint);

      self()->setOption(TR_DisableHeapAllocOOL);
      if (!(TR::Compiler->target.cpu.isZ() && TR::Compiler->target.isLinux()))
         self()->setOption(TR_UseIdleTime);

#if defined(TR_HOST_X86) || defined(TR_HOST_S390)
      // Dual TLH disabled on default on X and Z
      self()->setOption(TR_DisableDualTLH);
#endif

      if (self()->realTimeGC())
         {
         // Outlined-new requires heapAlloc and heapTop fields in the vmthread,
         // which metronome doesn't have.
         //
         self()->setOption(TR_DisableOutlinedNew);
         self()->setOption(TR_EnableOutlinedPrologues, false);
         }

      self()->setOption(TR_EnableAnnotations);

      TR::Compilation* comp = TR::comp();
      if (comp && TR::Compiler->om.canGenerateArraylets())
         {
         _disabledOptimizations[prefetchInsertion] = true;
         }

      self()->setOption(TR_DisableThunkTupleJ2I); // JSR292:TODO: Figure out how to do this without confusing startPCIfAlreadyCompiled

      self()->setOption(TR_DisableSeparateInitFromAlloc);

   #ifdef J9ZOS390
   #if defined(TR_TARGET_32BIT)

   #ifdef J9_PROJECT_SPECIFIC
       TR::Options::_weightOfJSR292 = 20; // set a large value for zOS 31-bit to force sequential compilation of these expensive guys
   #endif
       self()->setOption(TR_DontAllocateScratchBTL);
   #endif
   #endif

   #if defined(TR_TARGET_32BIT) && defined(PROD_WITH_ASSUMES)
      #if defined(LINUX)
         #if defined(S390)
            _userSpaceVirtualMemoryMB = 2048; // 2 GB = 2048 MB
         #else
            _userSpaceVirtualMemoryMB = 3072; // 3 GB = 3072 MB
         #endif //#if defined(S390)
      #elif defined(J9ZOS390)
         _userSpaceVirtualMemoryMB = -1; // Compute userspace dynamically on z/OS
      #endif //#if defined(LINUX)
   #elif !defined(WINDOWS)
      _userSpaceVirtualMemoryMB = 0; // Disabled for 64 bit or non-windows production
   #endif //#if defined(TR_TARGET_32BIT) && defined(PROD_WITH_ASSUMES)

      self()->setOption(TR_LowerCountsForAotCold);
      self()->setOption(TR_NoAotSecondRunDetection);

      // Now that we have set the default values for the options let's see if we need to override some of them
      // The aggressivenessLevel is set as a VM option. JIT options have not been processed at this point
      // When JIT processing is taking place, some of these decisions can be overidden
      //
      if (this == OMR::Options::getJITCmdLineOptions() || this == OMR::Options::getAOTCmdLineOptions())
         {
         if (_aggressivenessLevel >= 0 && _aggressivenessLevel <= 5)
            {
            // Set some default values for JIT and AOT master command line options
            //
            switch (_aggressivenessLevel)
               {
               case OMR::Options::DEFAULT: // default behavior
                  break;
               case OMR::Options::CONSERVATIVE_DEFAULT: // conservative default
                  self()->setConservativeDefaultBehavior();
                  break;
               case OMR::Options::AGGRESSIVE_AOT: // aggressive AOT (Rely on AOT as much as possible)
                  self()->setGlobalAggressiveAOT();
                  break;
               case OMR::Options::AGGRESSIVE_QUICKSTART: // aggressive quickstart (Quickstart with interpreter profiler)
                  self()->setAggressiveQuickStart();
                  break;
               case OMR::Options::QUICKSTART: // quickstart or -client
                  self()->setQuickStart();
                  break;
               case OMR::Options::CONSERVATIVE_QUICKSTART: // conservative quickstart
                  self()->setConservativeQuickStart();
                  break;
               } // end switch
            }
         else  // Some message that the aggressivenessLevel is invalid
            {
            if (_aggressivenessLevel != -1) // -1 means not set
               {
               if (OMR::Options::isAnyVerboseOptionSet())
                  TR_VerboseLog::write("\n<JIT: _aggressivenessLevel=%d; must be between 0 and 5; Option ignored\n", _aggressivenessLevel);
               _aggressivenessLevel = -1;
               }
            }
         }

      _enableSCHintFlags = TR_HintFailedValidation;

      // Enable the expensive compilation hints by default
      _enableSCHintFlags |= TR_HintLargeMemoryMethodW | TR_HintLargeMemoryMethodC | TR_HintLargeCompCPUW | TR_HintLargeCompCPUC;

      // Enable upgrade hints only if we have an SMP
      if (TR::Compiler->target.numberOfProcessors() > 1
   #if defined(J9ZOS390) // do not enable upgrade hints, hot/scorching hints on zOS because of compilation costs
          && _quickstartDetected
   #endif
          )
         {
         _enableSCHintFlags |= TR_HintUpgrade;
         self()->setOption(TR_UpgradeBootstrapAtWarm);
         // Hot and scorching upgrades are expensive and they might create a large
         // backlog of JNI or AOTloads; if more than two CPUs are available we can
         // rely on a secondary compilation thread to solve the backlog
         if (TR::Compiler->target.numberOfProcessors() > 2)
            {
            // Use only on quickstart; on non-quickstart we have GCR as a replacement
            // while during startup we want none of this
            if (_quickstartDetected)
               _enableSCHintFlags |= TR_HintHot | TR_HintScorching;
            }
         }

      if (TR::isJ9() && !_quickstartDetected && (TR::Compiler->target.is64Bit() && TR::Compiler->target.isLinux()))
         self()->setOption(TR_IncreaseCountsForNonBootstrapMethods);

      _newAotrtDebugLevel = 0;
      _disableDLTBytecodeIndex = -1;
      _enableDLTBytecodeIndex = -1;
      _dltOptLevel = -1;
      _maxStaticPICSlots = 4;
      _hotMaxStaticPICSlots = -2; // -N means it will default to N*_maxStaticPICSlots
      _insertGCRTrees = false;
      _GCRCount=-1; // indicates to use the default value
      _GCRResetCount = INT_MAX;
      _GCRDecCount = TR_DEFAULT_GCR_DEC_COUNT;
      _numInterfaceCallCacheSlots = 4;
      _numInterfaceCallStaticSlots = 1;
      _stackPCDumpNumberOfBuffers=4;
      _stackPCDumpNumberOfFrames=5;
      _maxSpreadCountLoopless = TR_MAX_SPREAD_COUNT_LOOPLESS;
      _maxSpreadCountLoopy = TR_MAX_SPREAD_COUNT_LOOPY;

      // This call needs to stay at the end of jitPreProcess() because
      // it changes the default values for some options
      self()->setDefaultsForDeterministicMode();
      }

   // --------------------------------------------------------------------------
   // Non-J9 only
   //
   else
      {

      // disable the fanin heuristics & virtual scratch memory for non-java frontends!
      self()->setOption(TR_DisableInlinerFanIn);
      self()->setOption(TR_DisableInlineEXTarget);
      }

   }


// -----------------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------------

void
OMR::Options::shutdown(TR_FrontEnd * fe)
   {
   if (TR::Options::isFullyInitialized())
      {
      if (TR::Options::getAOTCmdLineOptions() && TR::Options::getAOTCmdLineOptions()->getLogFile())
         TR::Options::closeLogFile(fe, TR::Options::getAOTCmdLineOptions()->getLogFile());

      if (TR::Options::getAOTCmdLineOptions())
         {
         TR::OptionSet *optionSet, *prev;
         for (optionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
            {
            TR::FILE *logFile = optionSet->getOptions()->getLogFile();
            if (logFile == NULL || logFile == TR::Options::getAOTCmdLineOptions()->getLogFile())
               continue;
            for (prev = TR::Options::getAOTCmdLineOptions()->_optionSets; prev != optionSet; prev = prev->getNext())
               {
               if (prev->getOptions()->getLogFile() == logFile)
                  {
                  logFile = NULL;
                  break;
                  }
               }
            if (logFile != NULL)
               TR::Options::closeLogFile(fe, logFile);
            }
         }

      if (TR::Options::getJITCmdLineOptions())
         {
         TR::OptionSet *optionSet, *prev;

         if (TR::Options::getJITCmdLineOptions()->getLogFile())
            {
            TR::FILE *logFile = TR::Options::getJITCmdLineOptions()->getLogFile();
            if (TR::Options::getAOTCmdLineOptions())
               {
               if (logFile ==TR::Options::getAOTCmdLineOptions()->getLogFile())
                  {
                  logFile = NULL;
                  }
               else
                  {
                  TR::OptionSet *aotOptionSet;

                  for (aotOptionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; aotOptionSet; aotOptionSet = aotOptionSet->getNext())
                     {
                     TR::FILE *aotLogFile = aotOptionSet->getOptions()->getLogFile();
                     if (aotLogFile == logFile)
                        {
                        logFile = NULL;
                        break;
                        }
                     }
                  }
               }
            if (logFile != NULL)
               {
               TR::Options::closeLogFile(fe, logFile);
               }
            }
         for (optionSet = TR::Options::getJITCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
            {
            TR::FILE *logFile = optionSet->getOptions()->getLogFile();
            if (logFile == NULL || logFile == TR::Options::getJITCmdLineOptions()->getLogFile())
               continue;
            for (prev = TR::Options::getJITCmdLineOptions()->_optionSets; prev != optionSet; prev = prev->getNext())
               {
               if (prev->getOptions()->getLogFile() == logFile)
                  {
                  logFile = NULL;
                  break;
                  }
               }
            if (TR::Options::getAOTCmdLineOptions() && (logFile != NULL))
               {
               TR::OptionSet *aotOptionSet;
               if (logFile == TR::Options::getAOTCmdLineOptions()->getLogFile())
                  {
                  logFile = NULL;
                  continue;
                  }
               for (aotOptionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; aotOptionSet; aotOptionSet = aotOptionSet->getNext())
                  {
                  TR::FILE *aotLogFile = aotOptionSet->getOptions()->getLogFile();
                  if (aotLogFile == logFile)
                     {
                     logFile = NULL;
                     break;
                     }
                  }
               }
            if (logFile != NULL)
               {
               TR::Options::closeLogFile(fe, logFile);
               }
            }
         }
      if (_logsForOtherCompilationThreadsExist)
         TR::Options::closeLogsForOtherCompilationThreads(fe);
      }

   }


void OMR::Options::safelyCloseLogs(TR::Options *options, TR_MCTLogs * &closedLogs, TR_FrontEnd * fe)
   {
   TR_MCTLogs *logEntry = options->getLogListForOtherCompThreads();
   while (logEntry)
      {
      TR_MCTLogs *nextLogEntry = logEntry->next();
      // search
      TR_MCTLogs *logEntry1 = closedLogs;
      for (; logEntry1; logEntry1 = logEntry1->next())
         {
         if (logEntry->getLogFile() == logEntry1->getLogFile())
            {
            // Log already closed; delete entry
            TR::Options::jitPersistentFree(logEntry);
            break;
            }
         }
      if (!logEntry1)
         {
         // This log was not yet closed
         TR::Options::closeLogFile(fe, logEntry->getLogFile());
         // Add to list of closed logs
         logEntry->setNext(closedLogs);
         closedLogs = logEntry;
         }
      logEntry = nextLogEntry;
      }
   }


void OMR::Options::closeLogsForOtherCompilationThreads(TR_FrontEnd * fe)
   {
   TR::OptionSet *optionSet;
   TR_MCTLogs *listOfClosedLogs = NULL;
   fe->acquireLogMonitor();
   TR::Options::safelyCloseLogs(TR::Options::getAOTCmdLineOptions(), listOfClosedLogs, fe);
   for (optionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      TR::Options::safelyCloseLogs(optionSet->getOptions(), listOfClosedLogs, fe);

   TR::Options::safelyCloseLogs(TR::Options::getJITCmdLineOptions(), listOfClosedLogs, fe);
   for (optionSet = TR::Options::getJITCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      TR::Options::safelyCloseLogs(optionSet->getOptions(), listOfClosedLogs, fe);

   // Now free all the log entries that have been closed
   TR_MCTLogs *logEntry = listOfClosedLogs;
   while (logEntry)
      {
      TR_MCTLogs *nextLogEntry = logEntry->next();
      TR::Options::jitPersistentFree(logEntry);
      logEntry = nextLogEntry;
      }
   fe->releaseLogMonitor();
   }


// -----------------------------------------------------------------------------
// Option sets
// -----------------------------------------------------------------------------

/**
 * Negate the option function passed in
 *
 * (Curiously, not able to use a switch here)
 */
TR::OptionFunctionPtr OMR::Options::negateProcessingMethod(TR::OptionFunctionPtr function)
   {
   if (function ==  OMR::Options::setBit)
      return OMR::Options::resetBit;
   if (function ==  OMR::Options::resetBit)
      return OMR::Options::setBit;
   if (function ==  OMR::Options::enableOptimization)
      return OMR::Options::disableOptimization;
   if (function ==  OMR::Options::disableOptimization)
      return OMR::Options::enableOptimization;
   if (function ==  OMR::Options::traceOptimization)
      return OMR::Options::dontTraceOptimization;

   return 0;
   }


TR::OptionSet *
OMR::Options::findOptionSet(TR_Memory * trMemory, TR_ResolvedMethod * vmMethod, bool isAOT)
   {
   TR_FilterBST * filter = 0;
   if (OMR::Options::getDebug() && OMR::Options::getDebug()->getCompilationFilters())
      OMR::Options::getDebug()->methodCanBeCompiled(trMemory, vmMethod, filter);

   int32_t optionSetIndex = filter ? filter->getOptionSet() : 0;
   int32_t lineNumber = filter ? filter->getLineNumber() : 0;
   return OMR::Options::findOptionSet(trMemory, optionSetIndex, lineNumber, vmMethod, TR::Options::getInitialHotnessLevel(vmMethod->hasBackwardBranches()), isAOT);
   }


TR::OptionSet *
OMR::Options::findOptionSet(TR_Memory * trMemory, int32_t index, int32_t lineNum, TR_ResolvedMethod * vmMethod, TR_Hotness hotnessLevel, bool isAOT)
   {
   const char *methodSignature = vmMethod->signature(trMemory);
   return TR::Options::findOptionSet(index, lineNum, methodSignature, hotnessLevel, isAOT);
   }


TR::OptionSet *
OMR::Options::findOptionSet(int32_t index, int32_t lineNum, const char *methodSignature, TR_Hotness hotnessLevel, bool isAOT)
   {
   TR::OptionSet * optionSet;
   TR::Options *cmdLineOptions = (isAOT) ? _aotCmdLineOptions : _jitCmdLineOptions;
   for (optionSet = cmdLineOptions->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      if (index && optionSet->getIndex() == index)
         {
         // Matched on option set index
         break;
         }
      else if (lineNum && optionSet->getStart() <= lineNum && optionSet->getEnd() >= lineNum)
         {
         // Matched on limtfile line range
         break;
         }
      else if (optionSet->getMethodRegex())
         {
         if (TR::SimpleRegex::match(optionSet->getMethodRegex(), methodSignature))
            {
            // Matched on method signature - see if a match on hotness level is
            // also required
            //
            if (!optionSet->getOptLevelRegex())
               break;
            if (TR::SimpleRegex::matchIgnoringLocale(optionSet->getOptLevelRegex(), TR::Compilation::getHotnessName(hotnessLevel)))
               break;
            char hotnessLevelString[2];
            hotnessLevelString[0] = hotnessLevel + '0';
            hotnessLevelString[1] = 0;
            if (TR::SimpleRegex::matchIgnoringLocale(optionSet->getOptLevelRegex(), hotnessLevelString))
               break;
            }
         }
      }

   return optionSet;
   }


void OMR::Options::setOptionInAllOptionSets(uint32_t mask, bool b)
   {
   if (TR::Options::getAOTCmdLineOptions())
      {
      TR::Options::getAOTCmdLineOptions()->setOption(mask, b);
      TR::OptionSet *optionSet;
      for (optionSet = TR::Options::getAOTCmdLineOptions()->getFirstOptionSet(); optionSet; optionSet = optionSet->getNext())
         {
         optionSet->getOptions()->setOption(mask, b);
         }
      }

   if (TR::Options::getJITCmdLineOptions())
      {
      TR::Options::getJITCmdLineOptions()->setOption(mask, b);
      TR::OptionSet *optionSet;
      for (optionSet = TR::Options::getJITCmdLineOptions()->getFirstOptionSet(); optionSet; optionSet = optionSet->getNext())
         {
         optionSet->getOptions()->setOption(mask, b);
         }
      }
   }


char *
OMR::Options::getDefaultOptions()
   {
   if (TR::Compiler->target.cpu.isX86() || TR::Compiler->target.cpu.isPower() || TR::Compiler->target.cpu.isARM())
      return (char *) ("samplingFrequency=2");

   if (TR::Compiler->target.cpu.isZ())
      return "samplingFrequency=2,numInterfaceCallCacheSlots=4";

   return "optLevel=cold,count=1000,bcount=1,milcount=1";
   }


void
OMR::Options::setTarget()
   {
   TR::ILOpCode::setTarget();
   if (TR::Compiler->target.is64Bit())
      TR::DataType::setSize(TR::Address, 8);
   else
      TR::DataType::setSize(TR::Address, 4);
   }


bool
OMR::Options::validateOptionsTables(void *feBase, TR_FrontEnd *fe)
   {
   TR::OptionTable *opt;

   _optionsTablesValidated = false;
   _numJitEntries = 0; // ensure value is initialized to 0 when processOptions is called
   _numVmEntries = 0; // ensure value is initialized to 0 when processOptions is called

   int32_t i;
   for (opt = _jitOptions; opt->name; opt++)
      {
#if DEBUG
      if (opt->helpText)
         {
         for (i = 0; categories[i]; i++)
            {
            if (opt->helpText[0] == categories[i])
               break;
            }
         TR_ASSERT(categories[i], "No category for help text");
         }
      if (_numJitEntries > 0 && stricmp_ignore_locale((opt-1)->name, opt->name) >= 0)
         {
         TR_VerboseLog::writeLine(TR_Vlog_FAILURE,"JIT option table entries out of order: ");
         TR_VerboseLog::write((opt-1)->name);
         TR_VerboseLog::write(", ");
         TR_VerboseLog::write(opt->name);
         return false;
         }
#endif
      _numJitEntries++;
      }

   for (opt = TR::Options::_feOptions; opt->name; opt++)
      {
#if DEBUG
      if (opt->helpText)
         {
         for (i = 0; categories[i]; i++)
            {
            if (opt->helpText[0] == categories[i])
               break;
            }
         TR_ASSERT(categories[i], "No category for help text");
         }
      if (_numVmEntries > 0 && stricmp_ignore_locale((opt-1)->name, opt->name) >= 0)
         {
         TR_VerboseLog::writeLine(TR_Vlog_FAILURE,"FE option table entries out of order: ");
         TR_VerboseLog::write((opt-1)->name);
         TR_VerboseLog::write(", ");
         TR_VerboseLog::write(opt->name);
         return false;
         }
#endif
      _numVmEntries++;
      }

   _optionsTablesValidated = true;
   return true;
   }


char *
OMR::Options::processOptions(
      char *options,
      char *envOptions,
      void *feBase,
      TR_FrontEnd *fe,
      TR::Options *cmdLineOptions)
   {
   if (!_optionsTablesValidated)
      {
      if (!TR::Options::validateOptionsTables(feBase, fe))
         return options;
      }

   // Get the environment variable and command lineoptions.
   // If none are specified, then get the default options.
   //
   if (strlen(options) == 0 && !envOptions)
      {
      options = OMR::Options::getDefaultOptions();
      }

   return TR::Options::processOptions(options, envOptions, cmdLineOptions);
   }


char *
OMR::Options::processOptions(char * options, char * envOptions, TR::Options *cmdLineOptions)
   {
   // Process the main options - option sets found will be skipped and their
   // addresses saved in the following array, then processed after the main
   // options have been processed.
   //
   if (cmdLineOptions == NULL)
      cmdLineOptions = _jitCmdLineOptions;

   cmdLineOptions->_startOptions = options;
   cmdLineOptions->_envOptions = envOptions;

   options = TR::Options::processOptionSet(options, envOptions, cmdLineOptions, (cmdLineOptions == TR::Options::getAOTCmdLineOptions()));
   if (*options)
      return options;

   if (!cmdLineOptions->jitPostProcess())
      return cmdLineOptions->_startOptions;

   if (cmdLineOptions == _aotCmdLineOptions)
      {
      if (!cmdLineOptions->fePostProcessAOT(_feBase))
         return cmdLineOptions->_startOptions;
      }
   else
      {
      if (!cmdLineOptions->fePostProcessJIT(_feBase))
         return cmdLineOptions->_startOptions;
      }

   return options;
   }


char *
OMR::Options::processOptionSet(
      char *options,
      char *envOptions,
      TR::Options *jitBase,
      bool isAOT)
   {
   options = TR::Options::processOptionSet(options, NULL, (void*) jitBase, isAOT);
   if (!*options && envOptions)
      options = TR::Options::processOptionSet(envOptions, NULL, (void*) jitBase, isAOT);

   return options;
   }


char *
OMR::Options::processOptionSet(
      char *options,
      char *envOptions,
      TR::OptionSet *optionSet)
   {
   TR::Options *jitBase = optionSet ? optionSet->getOptions() : _cmdLineOptions;

   options = TR::Options::processOptionSet(options, optionSet, (void*) jitBase, false);
   if (!*options && envOptions)
      options = TR::Options::processOptionSet(envOptions, optionSet, (void*) jitBase, false);

   return options;
   }


char *
OMR::Options::processOptionSet(
      char *options,
      TR::OptionSet *optionSet,
      void *jitBase,
      bool isAOT)
   {
   while (*options && *options != ')')
      {
      char *endOpt = NULL;
      char *filterHeader = NULL;
      TR::SimpleRegex *methodRegex = NULL, *optLevelRegex = NULL;
      int32_t startLine = 0, endLine = 0;

      // If this not an option subset, look for an option subset and scan it
      // off before looking for real options. The option subset will be
      // processed after all the main options are processed.
      //
      if (!optionSet)
         {
         //filter starts with a regular expression
         if (*options == '{')
            {
            filterHeader = options;
            // Option subset represented by a regular expression
            endOpt = options;

            methodRegex = TR::SimpleRegex::create(endOpt);
            if (!methodRegex)
               {
               TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", endOpt);
               return options;
               }

            // Look for a regular expression representing an opt level
            //
            if (*endOpt == '{')
               {
               optLevelRegex = TR::SimpleRegex::create(endOpt);
               if (!optLevelRegex)
                  {
                  TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", endOpt);
                  return options;
                  }
               }
            else
               {
               optLevelRegex = NULL;
               }

            if (!_debug)
               TR::Options::createDebug();
            }
         //filter starts with a limitfile linenumber range
         else if (*options == '[')
            {
            filterHeader = options;
            // Option subset represented by a range (as in limitfile line number)
            int32_t value = 0;
            options++;
            //assume this is a digit
            value=0;
            while (isdigit(*options))
               {
               value = 10*value + *options - '0';
               options++;
               }

            // set start
            startLine=value;

            if (*options != ']')
               {
               // pickup the end line number

               //assume this is a ',', you can put '-' if you want :)
               options++;
               //assume this is a digit
               value=0;
               while (isdigit(*options))
                  {
                  value = 10*value + *options - '0';
                  options++;
                  }
               }
            else
               {
               // use the start value as the end of the range
               }

            // set end
            endLine=value;

            //assume this is the ending ']';
            endOpt = options+1;
            }
         else if (!strnicmp_ignore_locale(options, EXCLUDED_METHOD_OPTIONS_PREFIX, strlen(EXCLUDED_METHOD_OPTIONS_PREFIX)))
            {
            // Option subset that applies to excluded methods
            //
            filterHeader = options;
            endOpt = options + strlen(EXCLUDED_METHOD_OPTIONS_PREFIX);
            }
         //filter starts with a limitfile subset index
         else if (*options >= '0' && *options <= '9')
            {
            filterHeader = options;
            // Option subset represented by an index (used in limitfiles)
            //
            endOpt = options+1;
            }
         }

      // If an option subset was found, save the information for later
      // processing
      //
      if (endOpt)
         {
         if (*endOpt != '(')
            return options;
         char *startOptString = ++endOpt;
         int32_t parenNest = 1;
         for (; *endOpt; endOpt++)
            {
            if (*endOpt == '(')
               parenNest++;
            else if (*endOpt == ')')
               {
               if (--parenNest == 0)
                  {
                  endOpt++;
                  break;
                  }
               }
            }
         if (parenNest)
            return options;

         // Save the option set - its option string will be processed after
         // the main options have been finished.
         //
         TR::OptionSet *newSet = new (PERSISTENT_NEW) TR::OptionSet(startOptString);

         if (newSet)
            {
            if (*filterHeader == '{')
               {
               newSet->setMethodRegex(methodRegex);
               newSet->setOptLevelRegex(optLevelRegex);
               }
            else if (*filterHeader == '[')
               {
               newSet->setStart(startLine);
               newSet->setEnd(endLine);
               }
            else if (!strnicmp_ignore_locale(filterHeader, EXCLUDED_METHOD_OPTIONS_PREFIX, strlen(EXCLUDED_METHOD_OPTIONS_PREFIX)))
               {
               newSet->setIndex(TR_EXCLUDED_OPTIONSET_INDEX);
               if (isAOT)
                  TR::Options::getAOTCmdLineOptions()->_compileExcludedmethods = true;
               else
                  TR::Options::getJITCmdLineOptions()->_compileExcludedmethods = true;
               }
            else
               {
               newSet->setIndex(*filterHeader - '0');
               }

            if (isAOT)
               TR::Options::getAOTCmdLineOptions()->addOptionSet(newSet);
            else
               TR::Options::getJITCmdLineOptions()->addOptionSet(newSet);
            }
         }

      // If an option subset was not found process the option by option table lookup
      //
      else
         {
         endOpt = TR::Options::processOption(options, _jitOptions, jitBase, _numJitEntries, optionSet);

         if (!endOpt)
            {
            TR_VerboseLog::write("<JIT: Unable to allocate option string>\n");
            return options;
            }

         char *feEndOpt = TR::Options::processOption(options, TR::Options::_feOptions, _feBase, _numVmEntries, optionSet);

         if (!feEndOpt)
            {
            TR_VerboseLog::write("<JIT: Unable to allocate option string>\n");
            return options;
            }

         // FE options must only appear in the main options. If this is an
         // option subset others and a FE option was found, treat it as an unrecognized
         // option.
         //
         if (feEndOpt != options && optionSet)
            {
            TR_VerboseLog::write("<JIT: Option not allowed in option subset>\n");
            return options;
            }

         if (feEndOpt > endOpt)
            endOpt = feEndOpt;
         if (endOpt == options)
            {
            return options; // Unrecognized option
            }
         }

      // Look for the separator
      //
      if (*endOpt == ',')
         {
         options = endOpt+1;
         continue;
         }
      else if (*endOpt && *endOpt != ')')
         {
         return options;  // Missing separator
         }
      options = endOpt;
      break;
      }
   return options;
   }

/**
 * Custom comparison function to compare two options.
 * Used for STL binary search.
 *
 * @param a The first option.
 * @param b The second option.
 */

bool
OMR::Options::compareOptionsForBinarySearch(const TR::OptionTable &a, const TR::OptionTable &b)
   {
   int32_t lengthOfTableEntry;

   // Check which option is the one passed in (optionToFind). We want to match
   // only up to the number of characters of the option in the table, not the one
   // passed in.
   if (a.isOptionToFind)
      lengthOfTableEntry = b.length;
   else
      lengthOfTableEntry = a.length;

   return (strnicmp_ignore_locale(a.name, b.name, lengthOfTableEntry) < 0);
   }

char *
OMR::Options::processOption(
      char *startOption,
      TR::OptionTable *table,
      void *base,
      int32_t numEntries,
      TR::OptionSet *optionSet)
   {
   char * option = startOption;
   bool negate = false;
   if (*option == '!')
      {
      negate = true;
      ++option;
      }

   // Set the length of all options in the table and set the isOptionToFind
   // field to false.
   for (TR::OptionTable* i = table; i < (table + numEntries); i++)
      {
      i->isOptionToFind = false;
      if (!i->length)
         i->length = strlen(i->name);
      }

   // Find the entry for this option string using a binary search

   // Create an object for the option to find in the table and set attributes
   TR::OptionTable optionToFind = TR::OptionTable();
   optionToFind.name = startOption;
   optionToFind.length = strlen(optionToFind.name);

   // Since STL binary search requires total ordering, there need to be a way to differentiate
   // between the option to find and an option in the table. The isOptionToFind field is used
   // here to specify which option is the option to find.

   optionToFind.isOptionToFind = true;

   TR::OptionTable *first, *last, *opt;

   auto equalRange = std::equal_range(table, (table + numEntries), optionToFind, compareOptionsForBinarySearch);

   first = equalRange.first;
   last = equalRange.second;

   if (first == last)
      {
      // optionToFind not found in the table
      return startOption;
      }
   else
      {
      // This is the best (longest) match for the specified option in the table
      opt = last - 1;
      }

   // If we are in an option subset and the option has been marked as not
   // available in an option subset. Make this an error.
   //
   if (optionSet)
      {
      if (opt->msgInfo & NOT_IN_SUBSET)
         {
         TR_VerboseLog::write("<JIT: option not allowed in option subset>\n");
         opt->msgInfo = 0;
         return startOption;
         }
      }
   else
      {
      // Remember we have seen this option at the outermost level
      //
      opt->msgInfo |= OPTION_FOUND;
      opt->enabled = true;
      // the following prevents use from being able to negate an option in a subset
      // that was specified at the outermost level.
      //
      //opt->msgInfo = NOT_IN_SUBSET;
      }

   TR::OptionFunctionPtr processingMethod;

   if (negate)
      {
      processingMethod = TR::Options::negateProcessingMethod(opt->fcn);
      if (!processingMethod)
         {
         TR_VerboseLog::write("<JIT: '!' is not supported for this option>\n");
         opt->msgInfo = 0;
         return startOption;
         }
      }
   else
      processingMethod = opt->fcn;

   // Process this entry
   //
   return processingMethod(option+opt->length, base, opt);
   }


bool
OMR::Options::jitPostProcess()
   {
   _enableDLTBytecodeIndex = -1;
   _disableDLTBytecodeIndex = -1;

   if (_logFileName)
      {
      if (_logFileName[0])
         _hasLogFile = true; // non-null log file name
      else
         _logFileName = NULL;// null log file name ... treat as no log file
      }

   if (self()->getOption(TR_ForceNonSMP))
      {
      TR::Compiler->host.setSMP(false);
      TR::Compiler->target.setSMP(false);
      }

   if (_logFileName)
      {
      if (!_debug)
         TR::Options::createDebug();

      if (_debug)
         self()->openLogFile();
      }
   else if (self()->requiresLogFile())
      {
      TR_VerboseLog::write("<JIT: the log file option must be specified when a trace options is used: log=<filename>)>\n");
      return false;
      }

   if (_optFileName)
      {
      if (!_debug)
         TR::Options::createDebug();

      if (_debug)
         {
         _customStrategy = _debug->loadCustomStrategy(_optFileName);
         if (_customStrategy)
            {
            for (_customStrategySize = 0; _customStrategy[_customStrategySize] != endOpts; _customStrategySize++);
            _customStrategySize++; // Include the endOpts terminator itself
            }
         else
            {
            TR_VerboseLog::write("<JIT: WARNING: ignoring optFile option; unable to read opts from '%s'\n", _optFileName);
            }
         }
      }

   if (debug("dumpOptDetails"))
      self()->setOption(TR_TraceOptDetails);

   if (self()->getOption(TR_Randomize))
      {
      // Options to reduce nondeterminism
      //
      // TODO: Do these belong here?  Perhaps -Xjit:randomize,deterministic=N should be the standard way to achieve this.
      //
      self()->setOption (TR_DisableInterpreterProfiling, true);
      self()->setOption (TR_DisableInterpreterSampling, true);
      TR::Options::disableSamplingThread();
      }


   if (self()->getOption(TR_StaticDebugCountersRequested) && !_enabledStaticCounterNames)
      _enabledStaticCounterNames = _enabledDynamicCounterNames;

   if (!_debug && (_enabledStaticCounterNames || _enabledDynamicCounterNames))
       TR::Options::createDebug();

   if (self()->getOption(TR_MimicInterpreterFrameShape) && self()->getOption(TR_FSDGRA))  // If we are enabling FSDGRA, we must disable VmThreadGRA or bad things will happen.
      self()->setOption(TR_DisableVMThreadGRA,true);


   uint8_t memUsageEnabled = 0;
   if (self()->getOption(TR_LexicalMemProfiler))
      {
      memUsageEnabled = heapAlloc | stackAlloc | persistentAlloc;
      }

   if (_memUsage)
      {
      self()->setOption(TR_LexicalMemProfiler);
      if (TR::SimpleRegex::match(_memUsage, "heap"))
         {
         memUsageEnabled |= heapAlloc;
         }
      if (TR::SimpleRegex::match(_memUsage, "stack"))
         {
         memUsageEnabled |= stackAlloc;
         }
      if (TR::SimpleRegex::match(_memUsage, "persistent"))
         {
         memUsageEnabled |= persistentAlloc;
         }
      }
   TR::AllocatedMemoryMeter::_enabled = memUsageEnabled;

   if (_hotMaxStaticPICSlots < 0)
      _hotMaxStaticPICSlots = -_hotMaxStaticPICSlots * _maxStaticPICSlots;

   if (self()->getOption(TR_EnableLargePages))
      {
      self()->setOption(TR_EnableLargeCodePages);
      }

#if defined(TR_TARGET_64BIT) && defined(J9ZOS390)
   // We allocate code cache memory on z/OS by asking the port library for typically small (~2MB) code cache chunks.
   // This is done because the port library can typically only allocate executable memory (code caches) below the
   // 2GB bar. When RMODE(64) is enabled however we are able to allocate executable memory above the 2GB bar. This
   // means two code caches could be very far apart from one another which is something we want to avoid as it may
   // cause performance issues. The solution is to enable the code cache consolidation which will allocate larger
   // (~256MB) code caches at startup. The code caches will then have locality which improves performance, but
   // moreover the size of the allocated larger code cache will ensure no trampolines are needed in JIT private
   // linkage.
   //
   // Code cache consolidation is enabled on all 64-bit platforms by default. However if RMODE64 is not available
   // on the OS or if the user specified to disable it we must also disable code cache consolidation.
   if (!self()->getOption(TR_EnableRMODE64))
      {
      self()->setOption(TR_EnableCodeCacheConsolidation, false);
      }
#endif

   return true;
   }


// -----------------------------------------------------------------------------
// Log file processing
// -----------------------------------------------------------------------------

#ifdef J9ZOS390
// TODO: remove and use port library to fetch PID
#include "unistd.h"
#endif

bool
OMR::Options::requiresLogFile()
   {
   if (self()->getOptsToTrace())
      return true;

   // note: enumerators with different word maps can't be or'ed together
   //
   if (self()->getAnyOption(TR_TraceAll) ||
       self()->getAnyOption(TR_TraceBBVA) ||
       self()->getAnyOption(TR_TraceBVA) ||
       self()->getAnyOption(TR_TraceUseDefs) ||
       self()->getAnyOption(TR_TraceNodeFlags) ||
       self()->getAnyOption(TR_TraceValueNumbers) ||
       self()->getAnyOption(TR_TraceLiveness) ||
       self()->getAnyOption(TR_TraceILGen) ||
       self()->getAnyOption(TR_TraceILValidator))
      return true;

   if (self()->tracingOptimization() || self()->getAnyTraceCGOption())
      return true;

   if (self()->getRegisterAssignmentTraceOption(0xffffffff))
      return true;

   return false;
   }


void getTimeInSeconds(char *buf);
void getTRPID(char *buf);


void OMR::Options::openLogFile(int32_t idSuffix)
   {
   _logFile = NULL;

   if (_logFile == NULL)
      {
      TR_ASSERT(_logFileName, "assertion failure");

      if (_suffixLogsFormat)
         self()->setOption(TR_EnablePIDExtension);

      char filename[1025];
      char *fn = _logFileName;

      if (idSuffix >= 0) // Must add the suffix to the name
         {
         int32_t len = strlen(_logFileName);
         if (len+12 > 1025)
            return; // may overflow the buffer
         sprintf(filename, "%s.%d", _logFileName, idSuffix);
         fn = filename;
         }

      char *fmodeString = "wb";
      if (self()->getOption(TR_EnablePIDExtension))
         {
         if (!_suffixLogsFormat)
            {
            // Append time id. TPO may invoke TR multiple times with different partition
            int32_t len = strlen(fn);
            if (len+12 > 1025)
               return; // may overflow the buffer
            char buf[20];
            memset(buf, 0, 20);
            getTRPID(buf);
            sprintf(filename, "%s.%s", fn, buf);
            getTimeInSeconds(buf);
            strncat(filename, ".", 1);
            strncat(filename, buf, 20);
            fn = filename;
            }

          char tmp[1025];
          fn = _fe->getFormattedName(tmp, 1025, fn, _suffixLogsFormat, true);
         _logFile = trfopen(fn, fmodeString, false);
         }
      else
         {
         char tmp[1025];
         fn = _fe->getFormattedName(tmp, 1025, fn, NULL, false);
         _logFile = trfopen(fn, fmodeString, false);
         }
      }

   if (_logFile != NULL)
      {
      trfprintf(_logFile,
                "<?xml version=\"1.0\" standalone=\"no\"?>\n"
                "<jitlog>\n");

      if (_numUsableCompilationThreads > 1)
         {
         trfprintf(_logFile,
                "<!--\n"
                "MULTIPLE LOG FILES MAY EXIST\n"
                "Please check for ADDITIONAL log files named:");
         for (int i = 1; i < _numUsableCompilationThreads; i++)
             trfprintf(_logFile, "  %s.%d", _logFileName, i);
         trfprintf(_logFile, "\n-->\n");
         }
      }
   }


void OMR::Options::closeLogFile(TR_FrontEnd *fe, TR::FILE *file)
   {
   if (file != NULL)
      trfprintf(file, "</jitlog>\n");

   trfclose(file);
   }


void
OMR::Options::printOptions(char *options, char *envOptions)
   {
   char *optionsType = "JIT";
   if (this == TR::Options::getAOTCmdLineOptions())
      optionsType = "AOT";
   TR_Debug::dumpOptions(optionsType, options, envOptions, self(), _jitOptions, TR::Options::_feOptions, _feBase, _fe);
   if (_aggressivenessLevel > 0)
       TR_VerboseLog::write("\naggressivenessLevel=%u\n", _aggressivenessLevel);
   }


TR_MCTLogs *OMR::Options::findLogFileForCompilationThread(int32_t compThreadID)
   {
   for (TR_MCTLogs *logInfo = self()->getLogListForOtherCompThreads(); logInfo; logInfo = logInfo->next())
      if (logInfo->getID() == compThreadID)
         return logInfo;
   return NULL;
   }


// Side effect this->_logFile is set
// compThreadID must be greater than 0
void OMR::Options::setLogForCompilationThread(int32_t compThreadID, TR::Options *masterOptions)
   {
   _fe->acquireLogMonitor();

   // Is the log file for this compilation thread already open?
   //
   TR_MCTLogs *optionLogEntry = self()->findLogFileForCompilationThread(compThreadID);
   if (optionLogEntry)
      {
      _logFile = optionLogEntry->getLogFile(); // overwrite the file descriptor for the log
      _fe->releaseLogMonitor();
      return;
      }

   if (OMR::Options::_dualLogging) // this named log could be open in another option set
      {
      if (!_debug)
         TR::Options::createDebug();

      if (_debug)
         {
         // find all the options objects that have a log file with given name
         #define OPTIONS_ARRAY_SIZE 256
         TR::Options *optionsArray[OPTIONS_ARRAY_SIZE];
         int32_t reqSize = _debug->findLogFile(_logFileName, OMR::Options::getAOTCmdLineOptions(),
                                               OMR::Options::getJITCmdLineOptions(), optionsArray, OPTIONS_ARRAY_SIZE);
         if (reqSize <= OPTIONS_ARRAY_SIZE)
            {
            for (int32_t i=0; i < reqSize; i++)
               {
               // check if there is any log open for that compilation thread
               optionLogEntry = optionsArray[i]->findLogFileForCompilationThread(compThreadID);
               if (optionLogEntry)
                  {
                  _logFile = optionLogEntry->getLogFile(); // overwrite the file descriptor for the log
                  _fe->releaseLogMonitor();
                  return;
                  }
               }
            }
         else // TODO: try to allocate an array of reqSize entries
            {
            _logFile = NULL; // error case
            }
         }
      else
         {
         _logFile = NULL; // error case
         _fe->releaseLogMonitor();
         return;
         }
      }

   // We should open a new log for this compilation thread
   optionLogEntry = new (PERSISTENT_NEW) TR_MCTLogs(compThreadID, self());
   if (optionLogEntry)
      {
      self()->openLogFile(compThreadID); // side effect: the open file will be set in this object
      if (_logFile != NULL)
         {
         // Cache the open log file in the masterOptions
         optionLogEntry->setLogFile(_logFile);
         // Attach the new logInfo to the list
         optionLogEntry->setNext(masterOptions->getLogListForOtherCompThreads());
         masterOptions->setLogListForOtherCompThreads(optionLogEntry);
         OMR::Options::_logsForOtherCompilationThreadsExist = true;
         }
      else
         {
         // delete optionLogEntry
         TR::Options::jitPersistentFree(optionLogEntry);
         }
      }
   else
      {
      _logFile = NULL; // error
      }

   _fe->releaseLogMonitor();
   }


char * TR_MCTLogs::getLogFileName()
   {
   return _options->getLogFileName();
   }


#include <ctime>
void getTimeInSeconds(char *buf)
   {
   time_t timer = time(NULL);
   sprintf(buf, "%i", (int32_t)timer % 100000);
   buf[6] = '\0';
   }


#ifdef _MSC_VER
#include <process.h>
void getTRPID(char *buf)
   {
   auto pid = _getpid();
   sprintf(buf, "%i", (int32_t)pid);
   }
#else
#include <sys/types.h>
#include <unistd.h>
void getTRPID(char *buf)
   {
   pid_t pid = getpid();
   sprintf(buf, "%i", (int32_t)pid);
   }
#endif


// -----------------------------------------------------------------------------
// Optlevels and counts
// -----------------------------------------------------------------------------

int32_t
OMR::Options::getFixedOptLevel()
   {
   TR_ASSERT(_jitCmdLineOptions == this || _aotCmdLineOptions == this, "getFixedOptLevel should be called on cmdlineoptions");
   return _optLevel;
   }


// J9 only
void
OMR::Options::setFixedOptLevel(int32_t optLevel)
   {
   TR_ASSERT(_jitCmdLineOptions == this || _aotCmdLineOptions == this, "setFixedOptLevel should be called on cmdlineoptions");
   _optLevel = optLevel;
   }


int32_t
OMR::Options::getOptLevel() const
   {
   TR_ASSERT(this != _jitCmdLineOptions && this != _aotCmdLineOptions, "use getFixedOptLevel instead");
   return _optLevel;
   }


// Same as setFixedOptLevel
void
OMR::Options::setOptLevel(int32_t o)
   {
   TR_ASSERT(this != _jitCmdLineOptions && this != _aotCmdLineOptions, "use setFixedOptLevel instead");
   _optLevel = o;
   }

static int32_t    count[numHotnessLevels] = {-2};
static int32_t   bcount[numHotnessLevels] = {-2};
static int32_t milcount[numHotnessLevels] = {-2};

char*
OMR::Options::setCounts()
   {
   if (_countString)
      {
      // Use the count string in preference to any specified fixed opt level
      //
      _optLevel = -1;

      _countsAreProvidedByUser = true; // so that we don't try to change counts later on

      // caveat: if the counts string is provided we should also not forget that
      // interpreterSamplingDivisorInStartupMode is at the default level of 16
      if (_interpreterSamplingDivisorInStartupMode == -1) // unchanged
         _interpreterSamplingDivisorInStartupMode = TR_DEFAULT_INTERPRETER_SAMPLING_DIVISOR;
      }
   else // no counts string specified
      {
      // No need for sampling thread if only one level of compilation and
      // interpreted methods are not to be sampled. Also, methods with loops
      // will need a smaller initial count since we won't know if they are hot.
      //
      if (_optLevel >= 0 && self()->getOption(TR_DisableInterpreterSampling))
         TR::Options::disableSamplingThread();

      // useLowerMethodCounts sets the count/bcount to the old values of 1000,250 resp.
      // those are what TR_QUICKSTART_INITIAL_COUNT and TR_QUICKSTART_INITIAL_BCOUNT are defined to.
      // if these defines are updated in the context of -Xquickstart,
      // please update this option accordingly
      //

      if (self()->getOption(TR_FirstRun)) // This overrides everything
         {
         _startupTimeMatters = TR_no;
         }

      if (_startupTimeMatters == TR_maybe) // not yet set
         {
         if (TR::Options::getJITCmdLineOptions()->getOption(TR_UseLowerMethodCounts) ||
            (TR::Options::getAOTCmdLineOptions() && TR::Options::getAOTCmdLineOptions()->getOption(TR_UseLowerMethodCounts)))
            _startupTimeMatters = TR_yes;
         else if (TR::Options::getJITCmdLineOptions()->getOption(TR_UseHigherMethodCounts) ||
                 (TR::Options::getAOTCmdLineOptions() && TR::Options::getAOTCmdLineOptions()->getOption(TR_UseHigherMethodCounts)))
            _startupTimeMatters = TR_no;
         else if (TR::Options::isQuickstartDetected())
            _startupTimeMatters = TR_yes;
         }

      bool startupTimeMatters = (_startupTimeMatters == TR_yes ||
                                (_startupTimeMatters == TR_maybe && TR::Options::sharedClassCache()));

      // Determine the counts for first time compilations
      if (_initialCount == -1) // Count was not set by user
         {
         if (startupTimeMatters)
            {
            // Select conditions under which we want even smaller counts
            if (TR::Compiler->target.isWindows() && TR::Compiler->target.is32Bit() && TR::Options::isQuickstartDetected() && TR::Options::sharedClassCache())
               _initialCount = TR_QUICKSTART_SMALLER_INITIAL_COUNT;
            else
               _initialCount = TR_QUICKSTART_INITIAL_COUNT;
            }
         else // Use higher count
            {
            _initialCount = TR_DEFAULT_INITIAL_COUNT;
            }
         }
      else
         {
         _countsAreProvidedByUser = true;
         }

      if (_initialBCount == -1)
         {
         if (_samplingFrequency == 0 || self()->getOption(TR_DisableInterpreterSampling))
            _initialBCount = std::min(1, _initialCount); // If no help from sampling, then loopy methods need a smaller count
         else
            {
            if (startupTimeMatters)
               {
               if (TR::Compiler->target.isWindows() && TR::Compiler->target.is32Bit() && TR::Options::isQuickstartDetected() && TR::Options::sharedClassCache())
                  _initialBCount = TR_QUICKSTART_SMALLER_INITIAL_BCOUNT;
               else
                  _initialBCount = TR_QUICKSTART_INITIAL_BCOUNT;
               }
            else
               {
               _initialBCount = TR_DEFAULT_INITIAL_BCOUNT;
               }
            _initialBCount = std::min(_initialBCount, _initialCount);
            }
         }
      else
         {
         _countsAreProvidedByUser = true;
         }

      if (_initialMILCount == -1)
         _initialMILCount = std::min(startupTimeMatters? TR_QUICKSTART_INITIAL_MILCOUNT : TR_DEFAULT_INITIAL_MILCOUNT, _initialBCount);

      if (_interpreterSamplingDivisorInStartupMode == -1) // unchanged
         _interpreterSamplingDivisorInStartupMode = startupTimeMatters ? TR_DEFAULT_INTERPRETER_SAMPLING_DIVISOR : 64;
      }

   // Prevent increasing the counts if lowerMethodCounts or quickstart is used
   if (_startupTimeMatters == TR_yes || _countsAreProvidedByUser)
      {
      TR::Options::getCmdLineOptions()->setOption(TR_IncreaseCountsForNonBootstrapMethods, false);
      TR::Options::getCmdLineOptions()->setOption(TR_IncreaseCountsForMethodsCompiledOutsideStartup, false);
      TR::Options::getCmdLineOptions()->setOption(TR_UseHigherCountsForNonSCCMethods, false);
      TR::Options::getCmdLineOptions()->setOption(TR_UseHigherMethodCountsAfterStartup, false);
      }
   if (_countsAreProvidedByUser)
      {
      TR::Options::getCmdLineOptions()->setOption(TR_ReduceCountsForMethodsCompiledDuringStartup, false);
      }

   // Set up default count string if none was specified
   //
   if (!_countString)
      _countString = self()->getDefaultCountString(); // _initialCount and _initialBCount have been set above

   if (_countString)
      {
      // The counts string is set up as:
      //
      //    counts=c0 b0 m0 c1 b1 m1 c2 b2 m2 c3 b3 m3 c4 b4 m4 ... etc.
      //
      // where "cn" is the count to get to recompile at level n
      //       "bn" is the bcount to get to recompile at level n
      //       "mn" is the milcount to get to recompile at level n
      // If a value is '-' or is an omitted trailing value, that opt level is
      // skipped. For levels other than 0, a zero value also skips the opt level.
      int32_t initialCount  = -1;
      int32_t initialBCount = -1;
      int32_t initialMILCount = -1;
      bool allowRecompilation = false;

      count[0] = 0;

      const char *s = _countString;
      if (s[0] == '"') ++s; // eat the leading quote
      int32_t i;
      for (i = minHotness; i <= maxHotness; ++i)
         {
         while (s[0] == ' ')
            ++s;
         if (isdigit(s[0]))
            {
            count[i] = atoi(s);
            while(isdigit(s[0]))
               ++s;
            if (initialCount >= 0)
               {
               allowRecompilation = true;
               if (count[i] == 0)
                  count[i] = -1;
               }
            else
               {
               initialCount = count[i];
               }
            }
         else if (s[0] == '-')
            {
            count[i] = -1;
            ++s;
            }
         else
            count[i] = -1;
         while (s[0] == ' ')
            ++s;
         if (isdigit(s[0]))
            {
            bcount[i] = atoi(s);
            while(isdigit(s[0]))
               ++s;
            if (initialBCount >= 0)
               {
               allowRecompilation = true;
               if (bcount[i] == 0)
                  bcount[i] = -1;
               }
            else
               initialBCount = bcount[i];
            }
         else if (s[0] == '-')
            {
            bcount[i] = -1;
            ++s;
            }
         else
         bcount[i] = -1;
         while (s[0] == ' ')
            ++s;
         if (isdigit(s[0]))
            {
            milcount[i] = atoi(s);
            while(isdigit(s[0]))
               ++s;
            if (initialMILCount >= 0)
               {
               allowRecompilation = true;
               if (milcount[i] == 0)
                  milcount[i] = -1;
               }
            else
               initialMILCount = milcount[i];
            }
         else if (s[0] == '-')
            {
            milcount[i] = -1;
            ++s;
            }
         else
            milcount[i] = -1;
         }

      _initialCount = initialCount;
      _initialBCount = initialBCount;
      _initialMILCount = initialMILCount;
      _allowRecompilation = allowRecompilation;
      }

   // The following need to stay after the count string has been processed
   if (_initialColdRunCount == -1) // not yet set
      _initialColdRunCount = std::min(TR_INITIAL_COLDRUN_COUNT, _initialCount);
   if (_initialColdRunBCount == -1) // not yet set
      _initialColdRunBCount = std::min(TR_INITIAL_COLDRUN_BCOUNT, _initialBCount);


   if (!_countString)
      {
      TR_VerboseLog::write("<JIT: Count string could not be allocated>\n");
      return dummy_string;
      }

   if (_initialCount == -1 || _initialBCount == -1 || _initialMILCount == -1)
      {
      TR_VerboseLog::write("<JIT: Bad string count: %s>\n", _countString);
      return _countString;
      }

   if ((TR::Options::getAOTCmdLineOptions() == this || TR::Options::getJITCmdLineOptions() == this) &&
       (_initialCount == 0) &&
       (_initialBCount == 0))
      self()->setOption (TR_DisableInterpreterProfiling, true);

   return 0;
   }


TR_Hotness
OMR::Options::getNextHotnessLevel(bool methodHasLoops, TR_Hotness current)
   {
   int32_t *values = methodHasLoops ? bcount : count;
   int32_t level;
   int32_t nextCount = -1;
   for (level = current+1; level <= maxHotness; ++level)
      {
      nextCount = values[level];
      if (nextCount > 0)
         break;
      }
   if (nextCount == -1)
      return unknownHotness;
   return (TR_Hotness) level;
   }


int32_t
OMR::Options::getCountValue(bool methodHasLoops, TR_Hotness hotness)
   {
   if (hotness < minHotness && hotness >= maxHotness)
      return -1;
   else
      return methodHasLoops ? bcount[hotness] : count[hotness];
   }


TR_Hotness
OMR::Options::getInitialHotnessLevel(bool methodHasLoops)
   {
   int32_t *values = methodHasLoops ? bcount : count;
   int32_t level;

   for (level = minHotness ; level <= maxHotness; ++level)
      {
      if (values[level] >= 0)
         return (TR_Hotness) level;
      }

   TR_ASSERT(false, "No initial level found in count string");
   return (TR_Hotness) 0;
   }


char *
OMR::Options::getDefaultCountString()
   {
   TR_ASSERT(this == _jitCmdLineOptions || this == _aotCmdLineOptions, "assertion failure");

   // If a fixed opt level was not given, assign the default count string
   //
   bool bcountFirst = false;

   char * str = 0;

   if (self()->getFixedOptLevel() == -1) // Not a fixed opt level
      {
      if (self()->getOption(TR_MimicInterpreterFrameShape))
         {
         str = "%d %d %d - - - - - - - - - - - -";
         }
      else
         {
         if (_samplingFrequency > 0)
            {
            if (self()->getOption(TR_DisableInterpreterSampling))
               {
               bcountFirst = true;
               str = "- - - - %d %d %d - - 1000 500 500 - - - 10000 10000 10000";
               }
            else
               {
               // If the user or the JVM heuristics have set _initialOptLevel, use that
               switch (_initialOptLevel)
                  {
                  case noOpt: str = "%d %d %d - - - - - - 1000 500 500 - - - 10000 10000 10000"; break;
                  case cold: str = "- - - %d %d %d - - - 1000 500 500 - - - 10000 10000 10000"; break;
                  case hot: str = "- - - - - - - - - %d %d %d - - - 10000 10000 10000"; break;
                  case scorching:
                      // If profiling can be done, then use very-hot as first level
                     if (!self()->getOption(TR_DisableProfiling))
                        str = "- - - - - - - - - - - - %d %d %d 10000 10000 10000";
                     else
                        str = "- - - - - - - - - - - - - - - %d %d %d";
                     break;
                  case -1: // fall through
                  default: str = "- - - - - - %d %d %d 1000 500 500 - - - 10000 10000 10000";
                  }
               }
            }
         else // no sampling
            {
            str = "- - - - - - %d %d %d - - - - - -"; // why do we use fixed optlevel if no sampling?
            }
         }
      }

   // If a fixed opt level was given, build a count string from the initial
   // count, bcount and milcount values.
   //
   else
      switch (self()->getFixedOptLevel())
         {
         case noOpt:
            str =  "%d %d %d";
            break;
         case cold:
            str =  "- - - %d %d %d";
            break;
         case warm:
            str =  "- - - - - - %d %d %d";
            break;
         case hot:
            str =  "- - - - - - - - - %d %d %d";
            break;
         case veryHot:
            str =  "- - - - - - - - - - - - %d %d %d";
            break;
         case scorching:
            str =  "- - - - - - - - - - - - - - - %d %d %d";
            break;
         default:
            TR_ASSERT(false, "should be unreachable");
            break;
         }

   char *p = (char*)TR::Options::jitPersistentAlloc(100);

   if (p)
      {
      if (bcountFirst)
         sprintf(p, str, _initialBCount, _initialMILCount, _initialCount);
      else
         sprintf(p, str, _initialCount, _initialBCount, _initialMILCount);
      }

   return p;
   }


char *
OMR::Options::setCount(char *option, void *base, TR::OptionTable *entry)
   {
   int32_t offset = entry->parm1;
   int32_t countValue = (int32_t)TR::Options::getNumericValue(option);

   *((int32_t*)((char*)base+offset)) = countValue;

   if ((offset == offsetof(OMR::Options, _initialCount)) && (((TR::Options *)base)->_initialSCount > countValue))
      {
      ((TR::Options *)base)->_initialSCount = countValue;
      }

   if ((base != _jitCmdLineOptions && base != _aotCmdLineOptions)) // the option is specified in a subset.
      {
      _aotCmdLineOptions->setAnOptionSetContainsACountValue(true);
      _jitCmdLineOptions->setAnOptionSetContainsACountValue(true);

      // make sure that the bcount value remains <= to the count value
      //
      if (offset == offsetof(OMR::Options, _initialCount) && ((TR::Options *)base)->_initialBCount > countValue)
         ((TR::Options *)base)->_initialBCount = countValue;

      // make sure that the milcount value remains <= to the count value and to the bcount value
      //
      if ((offset == offsetof(OMR::Options, _initialCount) || offset == offsetof(OMR::Options, _initialBCount)) &&
          ((TR::Options *)base)->_initialMILCount > countValue)
         ((TR::Options *)base)->_initialMILCount = countValue;
      }

   // make sure that the count reset value is positive
   if (offset == offsetof(OMR::Options, _GCRResetCount) && countValue <= 0)
      ((TR::Options *)base)->_GCRResetCount = TR_DEFAULT_GCR_RESET_COUNT;
   // make sure that the count dec value is positive
   else if (offset == offsetof(OMR::Options, _GCRDecCount) && countValue <= 0)
      ((TR::Options *)base)->_GCRResetCount = TR_DEFAULT_GCR_DEC_COUNT;

   return option;
   }


// -----------------------------------------------------------------------------

bool
OMR::Options::isOptionSetForAnyMethod(TR_CompilationOptions x)
   {
   if (TR::Options::getAOTCmdLineOptions()->getOption(x))
      return true;

   if (TR::Options::getJITCmdLineOptions()->getOption(x))
      return true;

   TR::OptionSet *optionSet;
   for (optionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      if (optionSet->getOptions()->getOption(x))
         return true;
      }
   for (optionSet = TR::Options::getJITCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      if (optionSet->getOptions()->getOption(x))
         return true;
      }

   return false;
   }


void
OMR::Options::setForAllMethods(TR_CompilationOptions x)
   {
   TR::Options::getAOTCmdLineOptions()->setOption(x);
   TR::Options::getJITCmdLineOptions()->setOption(x);

   TR::OptionSet *os;
   for (os = TR::Options::getAOTCmdLineOptions()->_optionSets; os; os = os->getNext())
      os->getOptions()->setOption(x);
   for (os = TR::Options::getJITCmdLineOptions()->_optionSets; os; os = os->getNext())
      os->getOptions()->setOption(x);
   }


void
OMR::Options::disableForAllMethods(OMR::Optimizations x)
   {
   TR::Options::getAOTCmdLineOptions()->setDisabled(x, true);
   TR::Options::getAOTCmdLineOptions()->setDisabled(x, true);

   TR::OptionSet *os;
   for (os = TR::Options::getAOTCmdLineOptions()->_optionSets; os; os = os->getNext())
      os->getOptions()->setDisabled(x, true);
   for (os = TR::Options::getJITCmdLineOptions()->_optionSets; os; os = os->getNext())
      os->getOptions()->setDisabled(x, true);
   }


bool
OMR::Options::checkDisableFlagForAllMethods(OMR::Optimizations o, bool b)
   {
   if (TR::Options::getAOTCmdLineOptions()->isDisabled(o) == b)
      return b;

   if (TR::Options::getJITCmdLineOptions()->isDisabled(o) == b)
      return b;

   TR::OptionSet *optionSet;
   for (optionSet = TR::Options::getAOTCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      if (optionSet->getOptions()->isDisabled(o) == b)
         return b;
      }
   for (optionSet = TR::Options::getJITCmdLineOptions()->_optionSets; optionSet; optionSet = optionSet->getNext())
      {
      if (optionSet->getOptions()->isDisabled(o) == b)
         return b;
      }

   return !b;
   }


char *
OMR::Options::setStaticBool(char *option, void *base, TR::OptionTable *entry)
   {
   *((bool*)entry->parm1) = (bool)entry->parm2;
   return option;
   }


char *
OMR::Options::setBit(char *option, void *base, TR::OptionTable *entry)
   {
   *((uint32_t*)((char*)base+entry->parm1)) |= entry->parm2;
   return option;
   }


char *
OMR::Options::setAddressEnumerationBits(char *option, void *base, TR::OptionTable *entry)
   {
   if (!_debug)
      TR::Options::createDebug();

   if (entry->parm2 != 0)
      {
     *((int32_t*)((char*)base+entry->parm1)) = (int32_t)entry->parm2;
      }
   else
      {
      *((int32_t*)((char*)base+entry->parm1)) = 0;  //Reset all bits to zero when this function is called

      TR::SimpleRegex * regex = _debug ? TR::SimpleRegex::create(option) : 0;
      if (!regex)
         TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
      else
         {
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "block"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateBlock;
            }
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "instruction"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateInstruction;
            }
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "node"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateNode;
            }
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "register"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateRegister;
            }
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "symbol"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateSymbol;
            }
         if (TR::SimpleRegex::matchIgnoringLocale(regex, "structure"))
            {
            *((int32_t*)((char*)base+entry->parm1)) |= TR_EnumerateStructure;
            }
         if (*((int32_t*)((char*)base+entry->parm1)) == 0x00000000)
            TR_VerboseLog::write("<JIT: Address enumeration option not found.  No address enumeration option was set.>");
         }
      }

   return option;
   }


OMR::Options::TR_OptionStringToBit OMR::Options::_optionStringToBitMapping[] = {
// Names cannot be reused otherwise all matching bits will be set
// bit 0x00000001 is set if any option is present

// Simplifier trace options
{ "mulDecompose", TR_TraceMulDecomposition},

// Debug Enable flags
{ "enableUnneededNarrowIntConversion", TR_EnableUnneededNarrowIntConversion },

// Local RA named trace options
{ "deps", TR_TraceRADependencies },
{ "details", TR_TraceRADetails },
{ "lateEdgeSplitting", TR_TraceRALateEdgeSplitting },
{ "preRA", TR_TraceRAPreAssignmentInstruction },
{ "spillTemps", TR_TraceRASpillTemps },
{ "states", TR_TraceRARegisterStates },
{ "listing", TR_TraceRAListing},


// Instruction Level GRA named trace Options
{ "basic", TR_TraceGRABasic},
{ "listing", TR_TraceGRAListing},

// Live Register Analysis named trace Options
{ "results", TR_TraceLRAResults },

// Register Interference Graph named trace Options
{ "basic", TR_TraceRegisterITFBasic},
{ "build", TR_TraceRegisterITFBuild },
{ "colour", TR_TraceRegisterITFColour },

// Register Spill Costs named trace Options
{ "basic", TR_TraceSpillCostsBasic},

// Instruction Level Dead Code named trace Options
{ "basic", TR_TraceILDeadCodeBasic},

// GPU Options
{ "default", TR_EnableGPU},
{ "enforce", TR_EnableGPUForce},
{ "verbose", TR_EnableGPUVerbose},
{ "details", TR_EnableGPUDetails},
{ "safeMT", TR_EnableSafeMT},
{ "enableMath", TR_EnableGPUEnableMath},
{ "DisableTransferHoist",  TR_EnableGPUDisableTransferHoist},

{ "", 0} // End of list indicator
};


char *
OMR::Options::setBitsFromStringSet(char *option, void *base, TR::OptionTable *entry)
   {
   int i;
   if (!_debug)
      TR::Options::createDebug();

   if (entry->parm2 != 0)
      {
     *((int32_t*)((char*)base+entry->parm1)) = (intptrj_t)entry->parm2;
      }
   else
      {
      *((int32_t*)((char*)base+entry->parm1)) = 0x1; // enable basic tracing is any set is provided

      TR::SimpleRegex * regex = _debug ? TR::SimpleRegex::create(option) : 0;
      if (!regex)
         TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
      else
         {
         for(i=0;_optionStringToBitMapping[i].bitValue != 0;i++)
           {
           if (TR::SimpleRegex::matchIgnoringLocale(regex, _optionStringToBitMapping[i].bitName))
             {
             *((int32_t*)((char*)base+entry->parm1)) |= _optionStringToBitMapping[i].bitValue;
             }
           }
         if (*((int32_t*)((char*)base+entry->parm1)) == 0x00000000)
            TR_VerboseLog::write("<JIT: Register assignment tracing options not found.  No additional tracing option was set.>");
         }
      }

   return option;
   }


char *OMR::Options::clearBitsFromStringSet(char *option, void *base, TR::OptionTable *entry)
   {
   int i;

   if (entry->parm2 != 0)
      {
     *((int32_t*)((char*)base+entry->parm1)) = (intptrj_t)entry->parm2;
      }
   else
      {
      TR::SimpleRegex * regex = TR::SimpleRegex::create(option);
      if (!regex)
         TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
      else
         {
         for(i=0;_optionStringToBitMapping[i].bitValue != 0;i++)
           {
           if (TR::SimpleRegex::matchIgnoringLocale(regex, _optionStringToBitMapping[i].bitName))
             {
             *((int32_t*)((char*)base+entry->parm1)) &= ~(_optionStringToBitMapping[i].bitValue);
             }
           }
         if (*((int32_t*)((char*)base+entry->parm1)) == 0x00000000)
            TR_VerboseLog::write("<JIT: Register assignment tracing options not found.  No additional tracing option was set.>");
         }
      }

   return option;
   }



char *
OMR::Options::configureOptReporting(char *option, void *base, TR::OptionTable *entry)
   {
   if (!_debug)
      TR::Options::createDebug();

   TR::Options *options = (TR::Options*)base;
   TR_CompilationOptions co = (TR_CompilationOptions)entry->parm1;
   options->setOption(co);

   switch(co)
      {
      case TR_VerboseOptTransformations:
         {
         options->setOption(TR_CountOptTransformations);
         TR::SimpleRegex * regex = _debug ? TR::SimpleRegex::create(option) : 0;
         if (!regex)
            TR_VerboseLog::write("<JIT: Bad regular expression --> '%s'>\n", option);
         else
            options->_verboseOptTransformationsRegex = regex;
         break;
         }
      default:
         {
         TR_ASSERT(0, "Unrecognized parm1 passed to configureOptReporting");
         break;
         }
      }

   return option;
   }


char *OMR::Options::_verboseOptionNames[TR_NumVerboseOptions] =
   {
   "options",
   "compileStart",
   "compileEnd",
   "compileRequest",
   "gc",
   "recompile",
   "compilePerformance",
   "filters",
   "sampling",
   "mmap",
   "compileExclude",
   "link",
   "classLoadPhase",
   "GCcycle",
   "compilationYieldStats",
   "heartbeat",
   "Extended",
   "SChints",
   "counts",
   "failures",
   "jitState",
   "jitMemory",
   "compilationThreads",
   "compilationThreadsDetails",
   "MCCReclamation",
   "dump",
   "hooks",
   "hookDetails",
   "runtimeAssumptions",
   "methodHandles",
   "methodHandleDetails",
   "j2iThunks",
   "vmemAvailable", // currently used only for win32
   "codecache",
   "precompile",
   "osr",
   "osrDetails",
   "optimizer",
   "hwprofiler",
   "inlining",
   "classUnloading",
   "patching",
   "compilationDispatch",
   "reclamation",
   "hookDetailsClassLoading",
   "hookDetailsClassUnloading",
   "sampleDensity",
   "profiling"
   };


char *
OMR::Options::setVerboseBitsInJitPrivateConfig(char *option, void *base, TR::OptionTable *entry)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR_JitPrivateConfig *privateConfig = *(TR_JitPrivateConfig**)((char*)_feBase+entry->parm1);
   TR_ASSERT(sizeof(VerboseOptionFlagArray) <= sizeof(privateConfig->verboseFlags), "TR_JitPrivateConfig::verboseFlags field is too small");
   VerboseOptionFlagArray *verboseOptionFlags = (VerboseOptionFlagArray*)((char*)privateConfig + offsetof(TR_JitPrivateConfig, verboseFlags));
   return TR::Options::setVerboseBitsHelper(option, verboseOptionFlags, entry->parm2);
#else
   return NULL;
#endif
   }


char *
OMR::Options::setVerboseBits(char *option, void *base, TR::OptionTable *entry)
   {
   VerboseOptionFlagArray *verboseOptionFlags = (VerboseOptionFlagArray*)((char*)_feBase+entry->parm1);
   return TR::Options::setVerboseBitsHelper(option, verboseOptionFlags, entry->parm2);
   }


char *
OMR::Options::setVerboseBitsHelper(char *option, VerboseOptionFlagArray *verboseOptionFlags, uintptrj_t defaultVerboseFlags)
   {
   if (defaultVerboseFlags != 0) // This is used for -Xjit:verbose without any options
      {
      // Since no verbose options are specified, add the default options
      verboseOptionFlags->maskWord(0, defaultVerboseFlags);
      }
   else // This is used for -Xjit:verbose={}  construct
      {
      TR::SimpleRegex * regex = TR::SimpleRegex::create(option);
      if (!regex)
         TR_VerboseLog::write("<JIT: Bad regular expression at --> '%s'>\n", option);
      else
         {
         bool foundMatch = false;
         for (int32_t i = 0; i < TR_NumVerboseOptions; i++)
            {
            if (TR::SimpleRegex::matchIgnoringLocale(regex, _verboseOptionNames[i], false))
               {
               verboseOptionFlags->set(i);
               foundMatch = true;
               if (i == TR_VerbosePerformance)  // If verbose={performance} is specified, I really want to see the options
                   verboseOptionFlags->set(TR_VerboseOptions);
               }
            }

         if (!foundMatch)
            TR_VerboseLog::write("<JIT: Verbose option not found.  No verbose option was set.>");
         }
      }
      return option;
   }


bool OMR::Options::isVerboseFileSet()
   {
#ifdef J9_PROJECT_SPECIFIC
   //check to see if a file has been opened for the vlog file
   TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
   TR_JitPrivateConfig* privateConfig = (TR_JitPrivateConfig*)(fej9->getPrivateConfig());
   return privateConfig->vLogFile ? true : false;
#else
   return false;
#endif
   }


char *
OMR::Options::resetBit(char *option, void *base, TR::OptionTable *entry)
   {
   *((uint32_t*)((char*)base+entry->parm1)) &= ~entry->parm2;
   return option;
   }


char *
OMR::Options::setValue(char *option, void *base, TR::OptionTable *entry)
   {
   *((intptrj_t*)((char*)base+entry->parm1)) = entry->parm2;
   return option;
   }


char *
OMR::Options::set32BitValue(char *option, void *base, TR::OptionTable *entry)
   {
   *((int32_t*)((char*)base+entry->parm1)) = (int32_t)entry->parm2;
   return option;
   }


char *
OMR::Options::enableOptimization(char *option, void *base, TR::OptionTable *entry)
   {
   ((TR::Options *)base)->_disabledOptimizations[entry->parm1] = false;
   return option;
   }


char *
OMR::Options::dontTraceOptimization(char *option, void *base, TR::OptionTable *entry)
   {
   ((TR::Options *)base)->_traceOptimizations[entry->parm1] = false;
   return option;
   }


char *
OMR::Options::disableOptimization(char *option, void *base, TR::OptionTable *entry)
   {
   ((TR::Options *)base)->_disabledOptimizations[entry->parm1] = true;
   return option;
   }


char *
OMR::Options::traceOptimization(char *option, void *base, TR::OptionTable *entry)
   {
   ((TR::Options *)base)->_traceOptimizations[entry->parm1] = true;
   ((TR::Options *)base)->_tracingOptimization = true;
   return option;
   }


char *
OMR::Options::helpOption(char *option, void *base, TR::OptionTable *entry)
   {
   if (!_debug)
      TR::Options::createDebug();

   TR::SimpleRegex * nameFilter = NULL;

   if (entry->parm1 == 0)
      {
      // Check for valid separator
      //
      if ((*option && *option != ',') )
         return option;
      }
   else
      {
      nameFilter = _debug ? TR::SimpleRegex::create(option) : 0;
      if (!nameFilter)
         return option;
      }

#if DEBUG
   if (_debug)
       _debug->dumpOptionHelp(_jitOptions, TR::Options::_feOptions, nameFilter);
#endif

   return option;
   }


char *
OMR::Options::disableMoreOpts(char *option, void *base, TR::OptionTable *entry)
   {
   TR::Options::processOptionSet(NoOptString "," DisableAllocationInliningString "," DisableInlineCheckCastString ","
          DisableInlineIfInstanceOfString "," DisableInlineInstanceOfString ","
          DisableInlineMonEntString "," DisableInlineMonExitString ","
          DisableInliningOfNativesString ","
          DisableNewInstanceImplOptString "," DisableFastStringIndexOfString,
          0, _currentOptionSet);

   return option;
   }


char *OMR::Options::_samplingJProfilingOptionNames[TR_NumSamplingJProfilingFlags] =
   {
   "invokevirtual",
   "invokeinterface",
   "invokestatic",
   "branches",
   "checkcast",
   "instanceof",
   };

char *OMR::Options::setSamplingJProfilingBits(char *option, void *base, TR::OptionTable *entry)
   {
   TR::SimpleRegex * regex = TR::SimpleRegex::create(option);

   if (regex)
      {
      bool foundMatch = false;

      for (int32_t i = 0; i < TR_NumSamplingJProfilingFlags; i++)
         {
         if (TR::SimpleRegex::matchIgnoringLocale(regex, _samplingJProfilingOptionNames[i], false))
            {
            _samplingJProfilingOptionFlags.set(i);
            foundMatch = true;
            }
         }
      }

   return option;
   }

char *
OMR::Options::breakOnLoad(char *option, void *base, TR::OptionTable *entry)
   {
   TR::Compiler->debug.breakPoint();
   return option;
   }


void OMR::Options::setConservativeQuickStart()
   {
   self()->setQuickStart();
   self()->setOption(TR_NoOptServer);
   // more conservative hot thresholds (already enabled on big apps)
   _sampleThreshold = 1000; // 3%


#ifdef J9_PROJECT_SPECIFIC
   TR::Options::_veryHotSampleThreshold = 240; // 12.5 %
   TR::Options::_scorchingSampleThreshold = 120; // 25% CPU
   TR::Options::_resetCountThreshold = 0; // No recompilations through sample counter reaching 0
#endif
   // low scratch memory threshold
   OMR::Options::setScratchSpaceLimit(64*1024*1024);
   // conservative upgrades
   _coldUpgradeSampleThreshold = 30; // instead of 3 or even 2
   // no AOT upgrade hints, unless specified by command line option
   // ...
   // more conservative cold inliner?
   // ...
   }


void OMR::Options::setQuickStart()
   {
   _quickstartDetected = true;
   _initialOptLevel = cold;
   _tocSizeInKB = 64;
   self()->setOption (TR_DisableInterpreterProfiling, true);
   self()->setOption (TR_ForceAOT, true);
   self()->setOption (TR_DisableAggressiveRecompilations, true);
   self()->setOption (TR_ConservativeCompilation, true);
   self()->setOption(TR_RestrictInlinerDuringStartup, true);

#ifdef J9_PROJECT_SPECIFIC
   TR::Options::_aotMethodThreshold = 1000000000; // disable detection of second run for shared relo code
#endif

   _minBytesToLeaveAllocatedInSharedPool = 0; // reduce the number of segments to keep in quickstart mode
   }


void OMR::Options::setAggressiveQuickStart()
   {
   self()->setQuickStart();
   self()->setOption(TR_DisableInterpreterProfiling, false);
   // Hot recompilations should become conservative only for bigApps
   // more aggressive cold inliner (allow bigger callees)
   // ...
   // more aggressive AOT expensive method hints
   // ...
   }


void OMR::Options::setLocalAggressiveAOT()
   {
   // disable GCR (AOT supposedly is good enough)
   self()->setOption(TR_DisableGuardedCountingRecompilations);

   // More conservative recompilation through sampling
   self()->setOption(TR_ConservativeCompilation, true);

   _bigCalleeThreshold = 150; // use a lower value to inline less and save compilation time

#ifdef J9ZOS390
   _inlinerVeryLargeCompiledMethodThreshold = 200; // down from 230
#else
   _inlinerVeryLargeCompiledMethodThreshold = 100; // down from 150/210
   _inlinerVeryLargeCompiledMethodFaninThreshold = 0; // down from 1
#endif
   }

void OMR::Options::setGlobalAggressiveAOT()
   {
   // Generate as much AOT code as possible, at warm opt level
   self()->setOption(TR_ForceAOT, true);
   // Setting the counts??
   // Allow IProfiler ... (already enabled)

   // setOption(TR_NoOptServer); // questionable

   // conservative upgrades
   _coldUpgradeSampleThreshold = 10; // instead of 3 or even 2

   //setOption(TR_VaryInlinerAggressivenessWithTime); // aggressiveness will go gradually down; for non-AOT warm compilations

   // more aggressive cold inliner for AOT compilations
   // ...
   // more aggressive AOT expensive method hints
   // ...

   // Since the HWP Reduced Warm is based off of -Xtune:virtualized
   // there is no need for this when -Xtune:virtualized is used globally
   self()->setOption(TR_DisableHardwareProfilerReducedWarm);

   // Set Non-static fields
   self()->setLocalAggressiveAOT();
   }


void OMR::Options::setConservativeDefaultBehavior()
   {
   // conservative recompilations
#ifdef J9_PROJECT_SPECIFIC
   TR::Options::_scorchingSampleThreshold = 120; //TR_EnableAppThreadYield 25% CPU
   TR::Options::_veryHotSampleThreshold = 240; // 12.5 %
#endif
   _sampleThreshold = 1000; // 3%

   self()->setOption(TR_DisableGuardedCountingRecompilations);
   self()->setOption(TR_NoOptServer);
   self()->setOption(TR_ConservativeCompilation, true);
   // conservative upgrades ?
   _coldUpgradeSampleThreshold = 30; // instead of 3 or even 2
   // query for Xaggressive must return false
   }


bool OMR::Options::counterIsEnabled(const char *name, int8_t fidelity, TR::SimpleRegex *nameRegex)
   {
   return (
         nameRegex
      && fidelity >= _minCounterFidelity
      && TR::SimpleRegex::match(nameRegex, name, false)
      );
   }


void OMR::Options::disableCHOpts()
   {
   self()->setOption(TR_DisableCHOpts);
   _disabledOptimizations[invariantArgumentPreexistence] = true;
   self()->setOption(TR_DisableIPA, true);
   }


TR::Options *OMR::Options::getCmdLineOptions()
   {
   return _jitCmdLineOptions;
   }


TR::Options *OMR::Options::getAOTCmdLineOptions()
   {
   return _aotCmdLineOptions;
   }


TR::Options *OMR::Options::getJITCmdLineOptions()
   {
   return _jitCmdLineOptions;
   }


bool OMR::Options::useCompressedPointers()
   {
   return false;
   }


char *OMR::Options::limitOption(char *option, void *base, TR::OptionTable *entry)
   {
   // TODO: this is identical to the J9 frontend
   if (!TR::Options::getDebug() && !TR::Options::createDebug())
      return 0;
   return TR::Options::getDebug()->limitOption(option, base, entry, TR::Options::getCmdLineOptions(), false);
   }


char *OMR::Options::limitfileOption(char* option, void* base, TR::OptionTable* entry)
   {
   if (!TR::Options::getDebug() && !TR::Options::createDebug())
      return 0;
   return TR::Options::getDebug()->limitfileOption(option, base, entry, TR::Options::getCmdLineOptions(), false);
   }


char *OMR::Options::inlinefileOption(char* option, void* base, TR::OptionTable* entry)
   {
   if (!TR::Options::getDebug() && !TR::Options::createDebug())
      return 0;
   return TR::Options::getDebug()->inlinefileOption(option, base, entry, TR::Options::getCmdLineOptions());
   }


bool OMR::Options::showOptionsInEffect()
   {
   return (TR::Options::getVerboseOption(TR_VerboseOptions));
   }


bool OMR::Options::fePreProcess(void*)
   {
#if !defined(J9_PROJECT_SPECIFIC)
   TR::Compiler->host.setNumberOfProcessors(2);
   TR::Compiler->target.setNumberOfProcessors(2);
   TR::Compiler->host.setSMP(true);
   TR::Compiler->target.setSMP(true);
#endif
   return true;
   }


bool OMR::Options::feLatePostProcess(void*, TR::OptionSet *)
   {
   return true;
   }


bool OMR::Options::fePostProcessAOT(void *base)
   {
   return true;
   }


bool  OMR::Options::fePostProcessJIT(void *base)
   {
#if !defined(J9_PROJECT_SPECIFIC)
   auto jitConfig = TR::JitConfig::instance();
   if (jitConfig->options.vLogFileName)
      {
      char *fileName;
      char tmp[1025];
      fileName = _fe->getFormattedName(tmp, 1025, jitConfig->options.vLogFileName, NULL, TR::Options::getCmdLineOptions()->getOption(TR_EnablePIDExtension));
      jitConfig->options.vLogFile = trfopen(fileName, "w", false);
      }
   else
      {
      jitConfig->options.vLogFile = OMR::IO::Stderr;
      }
   self()->setVerboseOptions(jitConfig->options.verboseFlags);
#endif

   return true;
   }


char *OMR::Options::versionOption(char * option, void * base, TR::OptionTable *entry)
   {
   fprintf(stdout, "JIT version: %s\n", TR_BUILD_NAME);
   return  option;
   }


bool OMR::Options::showPID() { return false; } // this option is needed for verbose={mmap}


void OMR::Options::setMoreAggressiveInlining()
   {
   self()->setOption(TR_RestrictInlinerDuringStartup, false);

   _trivialInlinerMaxSize = 100;
   self()->setOption(TR_DisableConservativeColdInlining, true);

   self()->setOption(TR_DisableConservativeInlining, true);
   self()->setOption(TR_InlineVeryLargeCompiledMethods, true);
   _bigCalleeFreqCutoffAtWarm = 0;
   _bigCalleeFreqCutoffAtHot = 0;
   _bigCalleeThreshold = 600;
   _bigCalleeHotOptThreshold = 600;
   _bigCalleeThresholdForColdCallsAtWarm = 600;
   _bigCalleeThresholdForColdCallsAtHot = 600;
   _maxSzForVPInliningWarm = 600;
   self()->setOption(TR_DisableInliningDuringVPAtWarm, false);
   }

void OMR::Options::setDefaultsForDeterministicMode()
   {
   if (TR::Options::getDeterministicMode() != -1 // if deterministic mode was set
       && OMR::Options::_aggressivenessLevel == -1 // quistart/Xtune:virtualized etc were note set
       && !self()->getOption(TR_AggressiveOpts))
      {
      // Set options common to all deterministic modes
      if (_initialOptLevel == -1) // If not yet set
         _initialOptLevel = warm;
      _initialCount = 1000;
      _initialBCount = 250;
      OMR::Options::_interpreterSamplingDivisorInStartupMode = 1;
      self()->setOption(TR_UseIdleTime, false);
      self()->setOption(TR_EnableEarlyCompilationDuringIdleCpu, false);
      self()->setOption(TR_DisableDynamicLoopTransfer, true);
      OMR::Options::_scratchSpaceLimit = 2147483647;
      self()->setOption(TR_ProcessHugeMethods);
      self()->setOption(TR_DisableScorchingSampleThresholdScalingBasedOnNumProc, true);
      self()->setOption(TR_DisableDynamicSamplingWindow, true);
      self()->setOption(TR_DisableAsyncCompilation, true);
      self()->setOption(TR_EnableHardwareProfileRecompilation, false);
      self()->setOption(TR_DisableSelectiveNoOptServer, true);
      self()->setOption(TR_RestrictInlinerDuringStartup, false);
      self()->setOption(TR_DisableSharedCacheHints, true); // AOT specific
      self()->setOption(TR_ForceAOT, true); // AOT specific
      self()->setOption(TR_DisablePersistIProfile, true); // AOT specific
      OMR::Options::_bigAppThreshold = 1;
      if (TR::Options::getNumUsableCompilationThreads() == -1) // not yet set
         OMR::Options::_numUsableCompilationThreads = 7;
#ifdef J9_PROJECT_SPECIFIC
      TR::Options::_veryHotSampleThreshold = 240; // 12.5 %
      TR::Options::_scorchingSampleThreshold = 120; // 25% CPU
      TR::Options::_resetCountThreshold = 0; // No recompilations through sample counter reaching 0
      TR::Options::_cpuEntitlementForConservativeScorching = 512000; // 512 CPUs
      TR::Options::_interpreterSamplingDivisor = 1;
      TR::Options::_interpreterSamplingThreshold = 10000;
      TR::Options::_interpreterSamplingThresholdInStartupMode = 10000;
      TR::Options::_interpreterSamplingThresholdInJSR292 = 10000;
      TR::Options::_iProfilerMemoryConsumptionLimit = 100000000;
      TR::Options::_profileAllTheTime = 1;
      TR::Options::_scratchSpaceFactorWhenJSR292Workload = 1; // since we have maximum scratchSpaceLimit
      TR::Options::_scratchSpaceLimitKBWhenLowVirtualMemory = 2147483647;
      TR::Options::_catchSamplingSizeThreshold = 10000000;
      TR::Options::_smallMethodBytecodeSizeThresholdForCold = 0; // Don't try filter out small methods from using GCR trees
#endif
      switch (TR::Options::getDeterministicMode())
         {
         case 0: // Fixed opt level
            self()->setFixedOptLevel(warm);
            break;

         case 1: // Inhibited recompilation
            self()->setOption(TR_InhibitRecompilation);
            break;

         case 2: // Lots of GCRs, no hot/scorching
            _initialOptLevel = cold;
            self()->setOption(TR_ImmediateCountingRecompilation, true);
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_sampleThreshold = 0;
            TR::Options::_veryHotSampleThreshold = 0;
            TR::Options::_scorchingSampleThreshold = 0;
            TR::Options::_resetCountThreshold = 0;
#endif
            break;

         case 3: // many hot recompilations without profiling
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_scorchingSampleThreshold = 0;
#endif
            self()->setOption(TR_EnableFastHotRecompilation, true);
            self()->setOption(TR_DisableProfiling, true);
            break;

         case 4: // many scorching recompilations without profiling
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_sampleThreshold = 0;
            TR::Options::_veryHotSampleThreshold = 0;
            TR::Options::_resetCountThreshold = 0;
#endif
            self()->setOption(TR_EnableFastScorchingRecompilation, true);
            self()->setOption(TR_DisableProfiling, true);
            break;

         case 5: // many high scorching recompilations with quick profiling
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_sampleThreshold = 0;
            TR::Options::_veryHotSampleThreshold = 0;
            TR::Options::_resetCountThreshold = 0;
#endif
            self()->setOption(TR_EnableFastScorchingRecompilation, true);
            self()->setOption(TR_QuickProfile, true);
            break;

         case 6:  // fixed opt level with lots of inlining
            self()->setFixedOptLevel(warm);
            self()->setMoreAggressiveInlining();
            break;

         case 7: // Inhibit recomp; aggressive inlining
            self()->setOption(TR_InhibitRecompilation);
            self()->setMoreAggressiveInlining();
            break;

         case 8: // Aggressive inlining, lots of GCRs, no high level compilations
            _initialOptLevel = cold;
            self()->setOption(TR_ImmediateCountingRecompilation, true);
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_sampleThreshold = 0;
            TR::Options::_veryHotSampleThreshold = 0;
            TR::Options::_scorchingSampleThreshold = 0;
            TR::Options::_resetCountThreshold = 0;
#endif
            self()->setMoreAggressiveInlining();
            break;

         case 9: // many hot recompilations without profiling; aggressive inlining
#ifdef J9_PROJECT_SPECIFIC
            TR::Options::_scorchingSampleThreshold = 0;
#endif
            self()->setOption(TR_EnableFastHotRecompilation, true);
            self()->setOption(TR_DisableProfiling, true);
            self()->setMoreAggressiveInlining();
            break;

         default: break;
         }
      }
   }


int32_t
OMR::Options::getJitMethodEntryAlignmentBoundary(TR::CodeGenerator *cg)
   {
   if (cg->supportsMethodEntryPadding())
      {
      return _jitMethodEntryAlignmentBoundary;
      }
   return 0;
   }
