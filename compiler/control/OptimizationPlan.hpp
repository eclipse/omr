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

#ifndef OPTIMIZATION_PLAN_INCL
#define OPTIMIZATION_PLAN_INCL

#include "optimizer/Optimizer.hpp"

#include <stddef.h>
#include <stdint.h>
#include "compile/CompilationTypes.hpp"
#include "env/FilePointerDecl.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace TR { class CompilationInfo; }
class TR_MethodEvent;
namespace TR { class Recompilation; }
namespace TR { class Monitor; }
struct TR_MethodToBeCompiled;

#if defined(__IBMCPP__) || defined(__open_xl__) && defined(__cplusplus)
// GCC (and other compilers) may assume that operator new cannot return null
// unless a nothrow specification is provided. For other reasons, we build
// Testarossa with -qnoeh / -fnoeh to disable exceptions. This is all fine,
// except that XLC doesn't wan't to be using the nothrow specification
// when -qnoeh is effect. As a result, we use it everywhere exept on XLC
#define NOTHROW
#else
#define NOTHROW throw()
#endif

///////////////////////////////////////////////////////////////////////
// TR_OptimizationPlan
//   Will be used by Continuous Program Optimization (CPO) framework
//   to control the behaviour of TR optimizations.
///////////////////////////////////////////////////////////////////////

class TR_OptimizationPlan
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::OptimizationPlan)

   // Eliminate explicit constructors so that we do not crash
   // when calling new TR_OptimizationPlan(..) and don't have the memory

   static TR_OptimizationPlan *alloc(TR_Hotness optLevel)
      {
      TR_ASSERT(optLevel != unknownHotness, "Cannot create a plan with unknown hotness");
      TR_OptimizationPlan *plan = new TR_OptimizationPlan();
      if (plan)
         plan->init(optLevel);
      return plan;
      }
   static TR_OptimizationPlan *alloc(TR_Hotness optLevel, bool insertInstrumentation, bool useSampling)
      {
      TR_ASSERT(optLevel != unknownHotness, "Cannot create a plan with unknown hotness");
      TR_OptimizationPlan *plan = alloc(optLevel);
      if (plan)
         {
         plan->setInsertInstrumentation(insertInstrumentation);
         plan->setUseSampling(useSampling);
         }
      return plan;
      }
   void clone(const TR_OptimizationPlan* otherPlan)
      {
      _optLevel      = otherPlan->_optLevel;
      _flags         = otherPlan->_flags;
      _perceivedCPUUtil = otherPlan->_perceivedCPUUtil;
      _hwpDoReducedWarm = otherPlan->_hwpDoReducedWarm;
      // no need to copy the _next pointer
      }
   void init(TR_Hotness optLevel)
      {
      _next = 0;
      setOptLevel(optLevel);
      _flags.clear();
      setUseSampling(true);
      setInUse(true);
      _perceivedCPUUtil = 0;
      //getInsertEpilogueYieldpoints();
      _hwpDoReducedWarm = false;
      }

   TR_Hotness getOptLevel() const { return _optLevel; }
   void setOptLevel(TR_Hotness optLevel) { _optLevel = optLevel; TR_ASSERT(optLevel != unknownHotness, "Cannot set unknown hotness in a plan");}

   int insertInstrumentation() const { return _flags.testAny(InsertInstrumentation); }
   void setInsertInstrumentation(bool b) { _flags.set(InsertInstrumentation, b); }

   int getUseSampling() const  { return _flags.testAny(UseSampling); }
   void setUseSampling(bool b) {  _flags.set(UseSampling, b); }

   bool isOptLevelDowngraded() const { return _flags.testAny(OptLevelDowngraded); }
   void setOptLevelDowngraded(bool b) { _flags.set(OptLevelDowngraded, b); }

   bool isDowngradedDueToSamplingJProfiling() const { return _flags.testAny(DowngradedDueToSamplingJProfiling); }
   void setDowngradedDueToSamplingJProfiling(bool b) { _flags.set(DowngradedDueToSamplingJProfiling, b); }

   // Insert epilogue yieldpoints if the method is being sampled
   bool getInsertEpilogueYieldpoints() const { return _flags.testAny(UseSampling); }

   bool isLogCompilation() const { return _flags.testAny(LogCompilation); }
   void setLogCompilation(TR::FILE *f) { _flags.set(LogCompilation); _logFile = f;  }
   TR::FILE *getLogCompilation() { return _logFile;  }

   bool shouldAddToUpgradeQueue() const { return _flags.testAny(AddToUpgradeQueue); }
   void setAddToUpgradeQueue() { _flags.set(AddToUpgradeQueue); }
   void resetAddToUpgradeQueue() { _flags.set(AddToUpgradeQueue, false); }

   bool inUse() const { return _flags.testAny(InUse); }
   void setInUse(bool b) { _flags.set(InUse, b); }

   int32_t getPerceivedCPUUtil() const { return _perceivedCPUUtil; }
   void setPerceivedCPUUtil(int32_t v) { _perceivedCPUUtil = v; }

   void setDisableCHOpts(bool b = true) { _flags.set(DisableCHOpts, b); }
   bool disableCHOpts() { return _flags.testAny(DisableCHOpts); }
   void setDisableGCR(bool b = true) { _flags.set(DisableGCR, b); }
   bool disableGCR() { return _flags.testAny(DisableGCR); }
   void setIsUpgradeRecompilation(bool b = true) { _flags.set(IsUpgradeRecompilation, b); }
   bool isUpgradeRecompilation() { return _flags.testAny(IsUpgradeRecompilation); }
   void setIsExplicitCompilation(bool b = true) { _flags.set(IsExplicitCompilation, b); }
   bool isExplicitCompilation() { return _flags.testAny(IsExplicitCompilation); }
   void setReduceMaxPeekedBytecode(uint32_t val) { val &= ReduceMaxPeekedBytecodeMask;
                                                   _flags.setValue(ReduceMaxPeekedBytecodeMask, val); }
   uint32_t getReduceMaxPeekedBytecode() const { return _flags.getValue(ReduceMaxPeekedBytecodeMask); }
   bool getDoNotSwitchToProfiling() const  { return _flags.testAny(doNotSwitchToProfiling); }
   void setDoNotSwitchToProfiling(bool b) {  _flags.set(doNotSwitchToProfiling, b); }
   bool getIsStackAllocated() const  { return _flags.testAny(IsStackAllocated); }
   void setIsStackAllocated() {  _flags.set(IsStackAllocated, true); }
   bool getIsAotLoad() const  { return _flags.testAny(IsAotLoad); }
   void setIsAotLoad(bool b) {  _flags.set(IsAotLoad, b); }

   bool getDontFailOnPurpose() const  { return _flags.testAny(DontFailOnPurpose); }
   void setDontFailOnPurpose(bool b) { _flags.set(DontFailOnPurpose, b); }

   bool getRelaxedCompilationLimits() const  { return _flags.testAny(RelaxedCompilationLimits); }
   void setRelaxedCompilationLimits(bool b) { _flags.set(RelaxedCompilationLimits, b); }

   bool isHwpDoReducedWarm() {return _hwpDoReducedWarm; }
   void setIsHwpDoReducedWarm (bool b) { _hwpDoReducedWarm = b; }

   bool isInducedByDLT() const { return _flags.testAny(InducedByDLT); }
   void setInducedByDLT(bool b) { _flags.set(InducedByDLT, b); }

   bool getDisableEDO() const { return _flags.testAny(DisableEDO); }
   void setDisableEDO(bool b) { _flags.set(DisableEDO, b); }

   // --------------------------------------------------------------------------
   // GPU
   //
   void setIsGPUCompilation(bool b = true) { _flags.set(IsGPUCompilation, b); }
   bool isGPUCompilation() { return _flags.testAny(IsGPUCompilation); }

   void setIsGPUParallelStream(bool b = true) { _flags.set(IsGPUParallelStream, b); }
   bool isGPUParallelStream() { return _flags.testAny(IsGPUParallelStream); }

   void setIsGPUCompileCPUCode(bool b = true) { _flags.set(IsGPUCompileCPUCode, b); }
   bool isGPUCompileCPUCode() { return _flags.testAny(IsGPUCompileCPUCode); }

   void setGPUBlockDimX(int32_t dim) { _gpuBlockDimX = dim; }
   int32_t getGPUBlockDimX() { return _gpuBlockDimX; }

   void setGPUParms(void * parms) { _gpuParms = parms; }
   void *getGPUParms() { return _gpuParms; }

   void setGPUIR(char * ir) { _gpuIR = ir; }
   char *getGPUIR() { return _gpuIR; }

   void setGPUResult(int32_t result) { _gpuResult = result; }
   int32_t getGPUResult() { return _gpuResult; }

   enum {  // list of flags
      // First 2 bits are used as a value to divide the MAX_PEEKED_BYTECODE_SIZE
      // to avoid future compilation failures. This allows us a better implementation of setter/getter
      // NOTE: If we change the position of the mask, then we need to change the setter/getter above
      ReduceMaxPeekedBytecodeMask = 0x0003,
      InsertInstrumentation   = 0x00000010,
      UseSampling             = 0x00000020,
      OptLevelDowngraded      = 0x00000040, // set if we downgrade the compilation to a lower level
      LogCompilation          = 0x00000080, // special compilation for method that crashed (to produce log)
      AddToUpgradeQueue       = 0x00000100, // flag to add request to the low priority upgrade queue
      InUse                   = 0x00000200, // set this flag when allocating a plan; reset when dealocating it
      DisableGCR              = 0x00000400, // Do not generate a GCR body for this method
      DisableCHOpts           = 0x00000800, // do not use CHTable opts because it's likely to fail this compilation
      IsUpgradeRecompilation  = 0x00001000, // This is a recompilation request which upgrades a
                                            // previously downgraded method
      doNotSwitchToProfiling  = 0x00002000, // Prevent the optimizer to switch to profiling on the fly
      IsExplicitCompilation   = 0x00004000,
      IsStackAllocated        = 0x00008000, // Plan is allocated on the stack. Only used in emergency situations
                                            // when we run out of native memory and cannot allocate a plan
                                            // using persistent memory, yet the compilation must be performed
      IsAotLoad               = 0x00010000, // AOT loads have a compilation object now
                                            // This flag will allow the compilation object know that it is only
                                            // used for relocation, so that some of the structures that are not
                                            // needed will be skipped (e.g. jittedBodyInfo, persistentMethodInfo)
      IsGPUCompilation        = 0x00020000, // requested by GPU library
      IsGPUParallelStream     = 0x00040000, // requested by parallel stream
      IsGPUCompileCPUCode     = 0x00080000, // compile CPU code using system linkage
      DontFailOnPurpose       = 0x00100000, // Mostly needed to avoid failing for the purpose of upgrading to a higher opt level
      RelaxedCompilationLimits= 0x00200000, // Compilation can use larger limits because method is very very hot
      DowngradedDueToSamplingJProfiling=0x00400000, // Compilation was downgraded to cold just because we wanted to do JProfiling
      InducedByDLT            = 0x00800000, // Compilation that follows a DLT compilation
      DisableEDO              = 0x01000000, // Do not insert EDO profiling trees for this compilation
   };
   private:
   TR_OptimizationPlan  *_next;       // to link events in the pool
   TR_Hotness _optLevel;
   flags32_t  _flags;
   int32_t    _perceivedCPUUtil;      // percentage*10 (so that we can use fractional values)
                                      // Will be set only for recompilations. <=0 means no info

   bool                        _hwpDoReducedWarm;
   TR::FILE *_logFile;
   static TR_OptimizationPlan *_pool;
   static unsigned long        _totalNumAllocatedPlans; // number of elements in the system
   static unsigned long        _poolSize;   // number of elements currently in the pool
   static unsigned long        _numAllocOp; // statistics
   static unsigned long        _numFreeOp;  // statistics
   #define POOL_THRESHOLD 32
   public:
   static TR::Monitor *_optimizationPlanMonitor;
   static void freeOptimizationPlan(TR_OptimizationPlan *plan);
   static int32_t freeEntirePool();
   void * operator new(size_t) NOTHROW;

   int32_t _gpuBlockDimX;
   void *  _gpuParms;
   char *  _gpuIR;
   int32_t _gpuResult;
   };

#endif
