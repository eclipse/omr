/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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
 *******************************************************************************/

#ifndef OMR_OPTIMIZATIONMANAGER_INCL
#define OMR_OPTIMIZATIONMANAGER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_OPTIMIZATIONMANAGER_CONNECTOR
#define OMR_OPTIMIZATIONMANAGER_CONNECTOR
namespace OMR { class OptimizationManager; }
namespace OMR { typedef OMR::OptimizationManager OptimizationManagerConnector; }
#endif

#include <stddef.h>                         // for NULL
#include <stdint.h>                         // for int32_t
#include "compile/Compilation.hpp"          // for Compilation
#include "env/TRMemory.hpp"                 // for Allocator, Allocatable, etc
#include "infra/Flags.hpp"                  // for flags32_t
#include "infra/List.hpp"                   // for List
#include "optimizer/OptimizationData.hpp"
#include "optimizer/OptimizationPolicy.hpp"
#include "optimizer/OptimizationUtil.hpp"
#include "optimizer/Optimizations.hpp"  // for Optimizations
#include "optimizer/Optimizer.hpp"      // for Optimizer
#include "infra/Annotations.hpp"        // for OMR_EXTENSIBLE

class TR_Debug;
class TR_FrontEnd;
namespace TR { class SymbolReferenceTable; }
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Optimization; }
namespace TR { class OptimizationManager; }
struct OptimizationStrategy;

typedef TR::Optimization *(*OptimizationFactory)(TR::OptimizationManager *m);

namespace OMR
{

class OMR_EXTENSIBLE OptimizationManager
   {
   public:

   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, size_t size)
      { ((OptimizationManager*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() must return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~OptimizationManager() {}

   TR::OptimizationManager *self();

   OptimizationManager(TR::Optimizer *o, OptimizationFactory factory, OMR::Optimizations optNum, const OptimizationStrategy *groupOfOpts = NULL);

   TR::Optimizer *           optimizer()                     { return _optimizer; }
   TR::Compilation *         comp()                          { return _optimizer->comp(); }

   TR::CodeGenerator *       cg();
   TR_FrontEnd *             fe();
   TR_Debug *            getDebug();
   TR::SymbolReferenceTable *getSymRefTab();

   TR_Memory *               trMemory();
   TR_StackMemory            trStackMemory();
   TR_HeapMemory             trHeapMemory();
   TR_PersistentMemory *     trPersistentMemory();

   TR::Allocator             allocator();

   OptimizationFactory       factory()                       { return _factory; }
   OMR::Optimizations        id()                            { return _id; }
   const char *              name()                          { return OMR::Optimizer::getOptimizationName(_id); }
   const OptimizationStrategy *    groupOfOpts()                   { return _groupOfOpts; }

   int32_t                   numPassesCompleted()            { return _numPassesCompleted; }
   void                      incNumPassesCompleted()         { ++_numPassesCompleted; }
   void                      setNumPassesCompleted(int32_t i){ _numPassesCompleted = i; }

   TR::OptimizationData *    getOptData()                    { return _optData; }
   void                      setOptData(TR::OptimizationData *optData) { _optData = optData; }

   TR::OptimizationPolicy *  getOptPolicy()                    { return _optPolicy; }
   void                      setOptPolicy(TR::OptimizationPolicy *optPolicy) { _optPolicy = optPolicy; }

   TR::OptimizationUtil *    getOptUtil()                    { return _optUtil; }
   void                      setOptUtil(TR::OptimizationUtil *optUtil) { _optUtil = optUtil; }

   flags32_t                 flags()                         { return _flags; }
   void                      setFlags(flags32_t flags)       { _flags.set(flags); }

   bool                      enabled()                       { return _enabled; }
   void                      setEnabled(bool enabled = true) { _enabled = enabled; }

   bool                      trace()                         { return _trace; }
   void                      setTrace(bool t = true)         { _trace = t; }

   bool requested(TR::Block *block = NULL);
   void setRequested(bool requested = true, TR::Block *block = NULL);

   List<TR::Block> *getRequestedBlocks()                        { return &_requestedBlocks; }

   void performChecks();

   enum
      {
      requiresStructure                    = 0x00000001,
      verifyTrees                          = 0x00000002,
      verifyBlocks                         = 0x00000004,
      checkTheCFG                          = 0x00000008,
      checkStructure                       = 0x00000010,
      alteredCode                          = 0x00000020,
      dumpStructure                        = 0x00000040,
      requiresGlobalsUseDefInfo            = 0x00000080,
      prefersGlobalsUseDefInfo             = 0x00000100,
      requiresLocalsUseDefInfo             = 0x00000200,
      requiresGlobalsValueNumbering        = 0x00000400,
      stronglyPrefersGlobalsValueNumbering = 0x00000800,
      prefersGlobalsValueNumbering         = 0x00001000,
      requiresLocalsValueNumbering         = 0x00002000,
      canAddSymbolReference                = 0x00004000,
      doesNotRequireAliasSets              = 0x00008000,
      performOnlyOnEnabledBlocks           = 0x00010000,
      supportsIlGenOptLevel                = 0x00020000,
      doesNotRequireTreeDumps              = 0x00040000,
      doesNotRequireLoadsAsDefs            = 0x00080000,
      lastRun                              = 0x00100000,
      cannotOmitTrivialDefs                = 0x00200000,
      maintainsUseDefInfo                  = 0x00400000,
      requiresAccurateNodeCount            = 0x00800000,
      doNotSetFrequencies                  = 0x01000000,
      dummyLastEnum
      };

   bool getRequiresStructure()           { return _flags.testAny(requiresStructure); }
   bool getRequiresUseDefInfo()          { return _flags.testAny(requiresGlobalsUseDefInfo | prefersGlobalsUseDefInfo | requiresLocalsUseDefInfo); }
   bool getRequiresGlobalsUseDefInfo()   { return _flags.testAny(requiresGlobalsUseDefInfo);}
   bool getPrefersGlobalsUseDefInfo()    { return _flags.testAny(prefersGlobalsUseDefInfo);}
   bool getRequiresLocalsUseDefInfo()    { return _flags.testAny(requiresLocalsUseDefInfo);}
   bool getDoesNotRequireLoadsAsDefsInUseDefs(){ return _flags.testAny(doesNotRequireLoadsAsDefs);}
   bool getRequiresValueNumbering()      { return _flags.testAny(requiresGlobalsValueNumbering | stronglyPrefersGlobalsValueNumbering | prefersGlobalsValueNumbering | requiresLocalsValueNumbering); }
   bool getRequiresGlobalsValueNumbering() { return _flags.testAny(requiresGlobalsValueNumbering);}
   bool getStronglyPrefersGlobalsValueNumbering()  { return _flags.testAny(stronglyPrefersGlobalsValueNumbering);}
   bool getPrefersGlobalsValueNumbering()  { return _flags.testAny(prefersGlobalsValueNumbering | stronglyPrefersGlobalsValueNumbering);}
   bool getRequiresLocalsValueNumbering()  { return _flags.testAny(requiresLocalsValueNumbering);}
   bool getRequiresAccurateNodeCount()  { return _flags.testAny(requiresAccurateNodeCount); }
   bool getVerifyTrees()                 { return _flags.testAny(verifyTrees); }
   bool getVerifyBlocks()                { return _flags.testAny(verifyBlocks); }
   bool getCheckTheCFG()                 { return _flags.testAny(checkTheCFG); }
   bool getCheckStructure()              { return _flags.testAny(checkStructure); }
   bool getDumpStructure()               { return _flags.testAny(dumpStructure); }
   bool getAlteredCode()                 { return _flags.testAny(alteredCode); }
   bool getCanAddSymbolReference()       { return _flags.testAny(canAddSymbolReference); }
   bool getDoesNotRequireAliasSets()     { return _flags.testAny(doesNotRequireAliasSets); }
   bool getPerformOnlyOnEnabledBlocks()  { return _flags.testAny(performOnlyOnEnabledBlocks); }
   bool getDoesNotRequireTreeDumps()     { return _flags.testAny(doesNotRequireTreeDumps); }
   bool getSupportsIlGenOptLevel()       { return _flags.testAny(supportsIlGenOptLevel); }
   bool getLastRun()                     { return _flags.testAny(lastRun); }
   bool getCannotOmitTrivialDefs()       { return _flags.testAny(cannotOmitTrivialDefs); }
   bool getMaintainsUseDefInfo()         { return _flags.testAny(maintainsUseDefInfo); }
   bool getDoNotSetFrequencies()         { return _flags.testAny(doNotSetFrequencies); }

   void setRequiresStructure(bool b)           { _flags.set(requiresStructure, b); }
   void setRequiresGlobalsUseDefInfo(bool b)   { _flags.set(requiresGlobalsUseDefInfo, b); }
   void setPrefersGlobalsUseDefInfo(bool b)    { _flags.set(prefersGlobalsUseDefInfo, b); }
   void setRequiresLocalsUseDefInfo(bool b)   { _flags.set(requiresLocalsUseDefInfo, b); }
   void setDoesNotRequireLoadsAsDefsInUseDefs(bool b){ _flags.set(doesNotRequireLoadsAsDefs, b);}
   void setRequiresGlobalsValueNumbering(bool b)   { _flags.set(requiresGlobalsValueNumbering, b); }
   void setPrefersGlobalsValueNumbering(bool b)    { _flags.set(prefersGlobalsValueNumbering, b); }
   void setStronglyPrefersGlobalsValueNumbering(bool b)    { _flags.set(stronglyPrefersGlobalsValueNumbering, b); }
   void setRequiresLocalsValueNumbering(bool b)   { _flags.set(requiresLocalsValueNumbering, b); }
   void setRequiresAccurateNodeCount(bool b)    { _flags.set(requiresAccurateNodeCount,b); }
   void setVerifyTrees(bool b)                 { _flags.set(verifyTrees, b); }
   void setVerifyBlocks(bool b)                { _flags.set(verifyBlocks, b); }
   void setCheckTheCFG(bool b)                 { _flags.set(checkTheCFG, b); }
   void setCheckStructure(bool b)              { _flags.set(checkStructure, b); }
   void setDumpStructure(bool b)               { _flags.set(dumpStructure, b); }
   void setAlteredCode(bool b)                 { _flags.set(alteredCode, b); }
   void setCanAddSymbolReference(bool b)       { _flags.set(canAddSymbolReference, b); }
   void setDoesNotRequireAliasSets(bool b)     { _flags.set(doesNotRequireAliasSets, b); }
   void setPerformOnlyOnEnabledBlocks(bool b)  { _flags.set(performOnlyOnEnabledBlocks, b); }
   void setSupportsIlGenOptLevel(bool b)       { _flags.set(supportsIlGenOptLevel, b); }
   void setLastRun(bool b)                     { _flags.set(lastRun,b); }
   void setCannotOmitTrivialDefs(bool b)       { _flags.set(cannotOmitTrivialDefs, b); }
   void setMaintainsUseDefInfo(bool b)         { _flags.set(maintainsUseDefInfo, b); }
   void setDoNotSetFrequencies(bool b)         { _flags.set(doNotSetFrequencies, b); }

   protected:

   TR::Optimizer *           _optimizer;
   OptimizationFactory       _factory;

   OMR::Optimizations        _id;
   const OptimizationStrategy *    _groupOfOpts;

   int32_t                   _numPassesCompleted;
   TR::OptimizationData *    _optData;          // opt specific data
   TR::OptimizationPolicy *  _optPolicy;        // opt specific policy for project specific optimization heuristic
   TR::OptimizationUtil *    _optUtil;          // opt specific utility for project specific optimization

   flags32_t                 _flags;
   bool                      _enabled;          // via command line
   bool                      _requested;        // by other opts
   List<TR::Block>           _requestedBlocks;  // by other opts
   bool                      _trace;
   };

}

#endif
