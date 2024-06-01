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

#ifndef OMR_COMPILATION_INCL
#define OMR_COMPILATION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_COMPILATION_CONNECTOR
#define OMR_COMPILATION_CONNECTOR
namespace OMR { class Compilation; }
namespace OMR { typedef OMR::Compilation CompilationConnector; }
#endif

/**
 * \file OMRCompilation.hpp
 * \brief Header for OMR::Compilation, root of the Compilation extensible class hierarchy.
 */


#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <set>
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/OSRData.hpp"
#include "compile/Method.hpp"
#include "compile/TLSCompilationManager.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Options.hpp"                // For Options
#include "control/Options_inlines.hpp"
#include "cs2/timer.h"
#include "env/PersistentInfo.hpp"
#include "env/TRMemory.hpp"
#include "env/Region.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/IL.hpp"
#include "il/Node.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Array.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/Stack.hpp"
#include "infra/ThreadLocal.hpp"
#include "optimizer/Optimizations.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "ras/ILValidationStrategies.hpp"



class TR_AOTGuardSite;
class TR_BitVector;
struct TR_CallTarget;
class TR_CHTable;
class TR_FrontEnd;
class TR_HWPRecord;
class TR_IlGenerator;
class TR_OptimizationPlan;
class TR_OSRCompilationData;
class TR_PersistentClassInfo;
class TR_PrexArgInfo;
class TR_RandomGenerator;
namespace TR { class RegisterCandidates; }
class TR_ResolvedMethod;
namespace OMR { class RuntimeAssumption; }
class TR_VirtualGuard;
class TR_VirtualGuardSite;
struct TR_VirtualGuardSelection;
namespace TR { class Block; }
namespace TR { class CFG; }
namespace TR { class CodeCache; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class IlGenRequest; }
namespace TR { class IlVerifier; }
namespace TR { class ILValidator; }
namespace TR { class Instruction; }
namespace TR { class KnownObjectTable; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class NodePool; }
namespace TR { class Options; }
namespace TR { class Optimizer; }
namespace TR { class Recompilation; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }
namespace TR { class TypeLayout; }
typedef TR::SparseBitVector SharedSparseBitVector;

#if _AIX
   #define STRTOK(a,b,c) strtok_r(a,b,c)
#else
   #define STRTOK(a,b,c) strtok(a,b)
#endif


// Return codes from the compilation. Any non-zero return code will abort the
// compilation.
enum CompilationReturnCodes
   {
   COMPILATION_SUCCEEDED = 0,
   COMPILATION_REQUESTED,
   COMPILATION_IL_GEN_FAILURE,
   COMPILATION_IL_VALIDATION_FAILURE,
   COMPILATION_UNIMPL_OPCODE,
   // Keep this the last one
   COMPILATION_FAILED
   };

enum ProfilingMode
   {
   DisabledProfiling,
   JitProfiling,
   JProfiling
   };

#if defined(DEBUG)
   // For a production build the body of of TR::Compilation::diagnostic is empty, so
   // the call will be inlined and become a NOP.  The problem is that unless the
   // compiler is very smart, it will continue to add the string (the first argument)
   // to the dll.  The use of these macros should ensure that the string is not added to
   // the dll.
   //
   #if defined(_MSC_VER)
      #define diagnostic( msg, ...) (TR::comp()->diagnosticImpl(msg, __VA_ARGS__))
   #else // XLC or GCC
      #define diagnostic(msg, ...) (TR::comp()->diagnosticImpl(msg, ##__VA_ARGS__))
   #endif

   /**
    * Returns true if 'option' exists inside the environment variable
    * TR_DEBUG or has been passsed as trDebug=, or an option has been
    * hooked up to setDebug.
    */
   char *debug(const char * option);
   void addDebug(const char * string);
#else
   #define diagnostic(msg, ...) ((void)0)
   #define debug(option)      NULL
   #define addDebug(string)   ((void)0)
#endif

/**
 * \def performTransformation(comp, msg, ...)
 *
 * \brief Macro that is used to guard optional steps in compilation in order to
 *        aid debuggability
 *
 *
 * Ostensibly, the purpose of performTransformation is to allow code
 * transformations to be selectively disabled during debugging in order to
 * isolate a buggy optimization. But this description fails to capture the
 * important effect that performTransformation has on the maintainability of
 * Testarossa's code base.
 *
 * Calls to performTransformation can (and should) be placed around any part of
 * the code that is optional; in an optimizer, that's a lot of code. Tons of
 * Testarossa code is there only to improve performance-not for correctness-and
 * can therefore be guarded by performTransformation.
 *
 * A call in a hypothetical dead code elimination might look like this:
 *
 *     if (performTransformation2c(comp(),
 *           "%sDeleting dead candidate [%p]\n", OPT_DETAILS, candidate->_node))
 *        {
 *        // Logic to delete some dead code.
 *        }
 *
 * When this opt runs on a method that is being logged, all calls to
 * performTransformation will be assigned a unique number. Running the test
 * case with lastOptTransformationIndex=n will prevent subsequent
 * transformations from occurring. By adjusting n and observing when the test
 * passes and fails, it is possible to narrow down the failing transformation
 * using a binary-search approach.
 *
 * But beyond just allowing this opt to be enabled and disabled, this idiom
 * also does the following:
 *
 *  -  It describes what the code does without needing a comment
 *  -  It reports the transformation in the log
 *  -  It gives people unfamiliar with the opt a way to locate the code that
 *     caused a particular transformation
 *  -  Combined with countOptTransformations, it allows you to locate methods
 *     in which this opt is occurring
 *
 * Most importantly, it identifies exactly the code that should be skipped if
 * someone wanted to prevent this opt from occurring in certain cases. Even if
 * you know nothing about an optimization, you can locate its
 * performTransformation call(s) and add an additional clause to this "if"
 * statement, secure in the knowledge that skipping this code will not leave
 * the optimization in some undefined state. The author of the optimization
 * has identified this code as "skippable", so you can be fairly certain that
 * skipping it will do just what you want.
 *
 * If you are developing code that has optional parts, it is strongly
 * recommended to guard them with performTransformation. It is likely to help
 * you debug your code immediately, and will certainly help people after you to
 * maintain and experiment with the code.
 */

#if defined(NO_OPT_DETAILS)

   #define traceMsg(comp, msg, ...)              ((void)0)
   #define dumpOptDetails(comp, msg, ...)        ((void)0)
   #define traceMsgVarArgs(comp, msg, arg)       ((void)0)
   #define performTransformation(comp, msg, ...) ((comp)->getOptimizer()? (comp)->getOptimizer()->incOptMessageIndex() > 0: true)
   #define performNodeTransformation0(comp,a)                (true)
   #define performNodeTransformation1(comp,a,b)              (true)
   #define performNodeTransformation2(comp,a,b,c)            (true)
   #define performNodeTransformation3(comp,a,b,c,d)          (true)

#else // of NO_OPT_DETAILS

   #define DumpOptDetailsDefined 1
   #define TRACE_OPT_DETAILS(comp) (comp)->getOptions()->getAnyOption(TR_TraceOptDetails|TR_CountOptTransformations)

   #define traceMsgVarArgs(comp, msg, arg) \
      ((comp)->getDebug() ? \
            (comp)->getDebug()->vtrace(msg, arg) : \
            (void)0)

   #if defined(_MSC_VER)
      #define traceMsg(comp, msg, ...) \
      ((comp)->getDebug() ? \
            (comp)->getDebug()->trace(msg, __VA_ARGS__) : \
            (void)0)
         #define dumpOptDetails(comp, msg, ...) \
      (TRACE_OPT_DETAILS(comp) ? \
            (comp)->getDebug()->performTransformationImpl(false, msg, __VA_ARGS__) : \
            true)
         #define performTransformation(comp, msg, ...) \
      (TRACE_OPT_DETAILS(comp) ? \
            (comp)->getDebug()->performTransformationImpl(true, msg, __VA_ARGS__) : \
            ((comp)->getOptimizer() ? (comp)->getOptimizer()->incOptMessageIndex() > 0 : true))
   #else // XLC or GCC
         #define traceMsg(comp, msg, ...) \
      ((comp)->getDebug() ? \
            (comp)->getDebug()->trace(msg, ##__VA_ARGS__) : \
            (void)0)
         #define dumpOptDetails(comp, msg, ...) \
      (TRACE_OPT_DETAILS(comp) ? \
            (comp)->getDebug()->performTransformationImpl(false, msg, ##__VA_ARGS__) : \
            true)
         #define performTransformation(comp, msg, ...) \
      (TRACE_OPT_DETAILS(comp) ? \
            (comp)->getDebug()->performTransformationImpl(true, msg, ##__VA_ARGS__) : \
            ((comp)->getOptimizer() ? (comp)->getOptimizer()->incOptMessageIndex() > 0 : true))
   #endif // compiler checks

   #define performNodeTransformation0(comp,a)       (comp->getOption(TR_TraceNodeFlags) ? performTransformation(comp,a) : true)
   #define performNodeTransformation1(comp,a,b)     (comp->getOption(TR_TraceNodeFlags) ? performTransformation(comp,a,b) : true)
   #define performNodeTransformation2(comp,a,b,c)   (comp->getOption(TR_TraceNodeFlags) ? performTransformation(comp,a,b,c) : true)
   #define performNodeTransformation3(comp,a,b,c,d) (comp->getOption(TR_TraceNodeFlags) ? performTransformation(comp,a,b,c,d) : true)

#endif



namespace OMR
{

class OMR_EXTENSIBLE Compilation
   {
protected:

   inline TR::Compilation *self();

public:

   TR_ALLOC(TR_Memory::Compilation)

   Compilation(
         int32_t compThreadId,
         OMR_VMThread *omrVMThread,
         TR_FrontEnd *,
         TR_ResolvedMethod *,
         TR::IlGenRequest &,
         TR::Options &,
         TR::Region &heapMemoryRegion,
         TR_Memory *,
         TR_OptimizationPlan *optimizationPlan,
         TR::Environment *target = NULL
         );

   TR::SymbolReference * getSymbolReferenceByReferenceNumber(int32_t referenceNumber);

   ~Compilation() throw();



   TR::Region &region() { return _heapMemoryRegion; }

   TR::IL il;

   TR::IlGenRequest &ilGenRequest()     { return _ilGenRequest; }
   TR::CodeGenerator *cg()              { return _codeGenerator; }
   TR_FrontEnd *fe()                    { return _fe; }
   TR::Options *getOptions()            { return _options; }

   TR_Memory *trMemory()                { return _trMemory; }
   TR_StackMemory trStackMemory()       { return _trMemory; }
   TR_HeapMemory trHeapMemory()         { return _trMemory; }
   TR_PersistentMemory *trPersistentMemory() { return _trMemory->trPersistentMemory(); }

   bool getOption(TR_CompilationOptions o) {return _options->getOption(o);}
   void setOption(TR_CompilationOptions o) { _options->setOption(o); }

   bool trace(OMR::Optimizations o)             { return _options->trace(o); }
   bool isDisabled(OMR::Optimizations o)        { return _options->isDisabled(o); }


   // Table of heap objects whose identity is known at compile-time
   //
   TR::KnownObjectTable *getKnownObjectTable() { return _knownObjectTable; }
   TR::KnownObjectTable *getOrCreateKnownObjectTable();
   void freeKnownObjectTable();

   // Is this compilation producing relocatable code?  This should generally
   // return true, for example, for ahead-of-time compilations.
   //
   bool compileRelocatableCode() { return false; }

   // Is this compilation producing portable code?  This should generally
   // return true, for example, for compilations to be used in a
   // snapshot/restore setting.
   //
   bool compilePortableCode() { return false; }

   // Maximum number of internal pointers that can be managed.
   //
   int32_t maxInternalPointers();

   // The OMR thread on which this compilation is occurring.
   //
   OMR_VMThread *omrVMThread() { return _omrVMThread; }

   bool compilationShouldBeInterrupted(TR_CallingContext) { return false; }

   /**
    * @brief denotes the start of a region wherein decisions do not need to be
    *        remembered (for example, in relocatable compilations)
    */
   void enterHeuristicRegion() {}

   /**
    * @brief denotes the end of a region wherein decisions do not need to be
    *        remembered (for example, in relocatable compilations)
    */
   void exitHeuristicRegion() {}

   /* Can be used to ensure that a implementer chosen for inlining is valid;
    * for example, to ensure that the implementer can be used for inlining
    * in a relocatable compilation
    */
   bool validateTargetToBeInlined(TR_ResolvedMethod *implementer) { return true; }

   // ..........................................................................
   // Optimizer mechanics
   int16_t getOptIndex()        { return _currentOptIndex; }
   void setOptIndex(int16_t i)  { _currentOptIndex = i; }
   void incOptIndex()           { _currentOptIndex++; _currentOptSubIndex = 0; }

   int32_t getOptSubIndex()     { return _currentOptSubIndex; }
   void incOptSubIndex()        { _currentOptSubIndex++; }

   void recordBegunOpt()        { _lastBegunOptIndex = _currentOptIndex; }
   void recordPerformedOptTransformation()
      {
      _lastPerformedOptIndex = _currentOptIndex;
      _lastPerformedOptSubIndex = _currentOptSubIndex;
      }

   int16_t getLastBegunOptIndex()       { return _lastBegunOptIndex; }
   int16_t getLastPerformedOptIndex()   { return _lastPerformedOptIndex; }
   int32_t getLastPerformedOptSubIndex(){ return _lastPerformedOptSubIndex; }

   bool isOutermostMethod();
   bool ilGenTrace();

   // ==========================================================================
   // Debug
   //
   void setDebug(TR_Debug * d) { _debug = d; }
   TR_Debug * getDebug() { return _debug; }
   TR_Debug * findOrCreateDebug();

   // Do not use the following directly; use diagnostic macro instead
   void diagnosticImpl(const char *s, ...);
   void diagnosticImplVA(const char *s, va_list ap);

   // RAS methods.

   TR::FILE *getOutFile()  { return _options->getLogFile(); }
   void  setOutFile(TR::FILE *pf) { _options->setLogFile(pf); }

   // --------------------------------------------------------------------------

   bool allowRecompilation()   {return _options->allowRecompilation();}

   bool allocateAtThisOptLevel();

   TR::Allocator allocator(const char *name = NULL) { return TR::Allocator(_allocator); }

   TR_ArenaAllocator  *arenaAllocator() { return &_arenaAllocator; }

   TR_OpaqueMethodBlock *getMethodFromNode(TR::Node * node);
   int32_t getLineNumber(TR::Node *);
   int32_t getLineNumberInCurrentMethod(TR::Node * node);

   bool useRegisterMaps();

   bool suppressAllocationInlining()      {return _options->getOption(TR_DisableAllocationInlining);}

   // ==========================================================================
   // OMR utility
   //

   TR_IlGenerator *getCurrentIlGenerator() { return _ilGenerator; }
   void setCurrentIlGenerator(TR_IlGenerator * il) { _ilGenerator = il; }

   TR::Optimizer *getOptimizer() { return _optimizer; }
   void setOptimizer(TR::Optimizer * o) { _optimizer = o; }

   TR::ResolvedMethodSymbol *getMethodSymbol();
   TR_ResolvedMethod *getCurrentMethod();

   TR_ResolvedMethod *getMethodBeingCompiled() { return _method; }

   TR::PersistentInfo *getPersistentInfo();

   int32_t getCompThreadID() const { return _compThreadID; }

   const char * signature() { return _signature; }
   const char * externalName() { return _method->externalName(_trMemory); }

   TR::ResolvedMethodSymbol *getJittedMethodSymbol() { return _methodSymbol;}
   TR::CFG *getFlowGraph();
   TR::TreeTop *getStartTree();
   TR::TreeTop *findLastTree();
   TR::Block *getStartBlock();

   TR::Block * getCurrentBlock() {return _currentBlock;}
   void setCurrentBlock(TR::Block * block) {_currentBlock=block; }

   vcount_t getVisitCount();
   vcount_t setVisitCount(vcount_t vc);
   vcount_t incVisitCount();

   void resetVisitCounts(vcount_t);
   void resetVisitCounts(vcount_t, TR::TreeTop *);
   vcount_t incOrResetVisitCount();

   // common, compilation error code
   int32_t getErrorCode() { return _errorCode; }
   void setErrorCode(int32_t errorCode) { _errorCode = errorCode; }

   TR::HCRMode getHCRMode();

   // ==========================================================================
   // Symbol reference
   //
   uint32_t getSymRefCount();

   TR::SymbolReferenceTable *getSymRefTab()
      {
      if (_currentSymRefTab)
         return _currentSymRefTab;
      else
         return _symRefTab;
      }

   void setCurrentSymRefTab(TR::SymbolReferenceTable *t) {_currentSymRefTab = t;}
   TR::SymbolReferenceTable *getCurrentSymRefTab() {return _currentSymRefTab;}

   // ==========================================================================
   // Aliasing
   //

   int32_t getMaxAliasIndex();

   // ==========================================================================
   // Method
   //
   TR_ReturnInfo getReturnInfo() {return _returnInfo;}
   void setReturnInfo(TR_ReturnInfo i);

   mcount_t addOwningMethod(TR::ResolvedMethodSymbol * p);
   TR::ResolvedMethodSymbol *getOwningMethodSymbol(mcount_t i);
   TR::ResolvedMethodSymbol *getOwningMethodSymbol(TR_ResolvedMethod * method);
   TR::ResolvedMethodSymbol *getOwningMethodSymbol(TR_OpaqueMethodBlock * method);

   void registerResolvedMethodSymbolReference(TR::SymbolReference *);
   TR::SymbolReference * getResolvedMethodSymbolReference(mcount_t index) { return _resolvedMethodSymbolReferences[index.value()]; }

   bool isPeekingMethod();
   // J9
   void setContainsBigDecimalLoad(bool b) { _containsBigDecimalLoad = b; }
   bool containsBigDecimalLoad() { return _containsBigDecimalLoad; }

   bool osrStateIsReliable() { return _osrStateIsReliable;}
   void setOsrStateIsReliable(bool b) { _osrStateIsReliable = b;}

   bool osrInfrastructureRemoved() { return _osrInfrastructureRemoved; }
   void setOSRInfrastructureRemoved(bool b) { _osrInfrastructureRemoved = b; }

   bool mayHaveLoops();
   bool hasLargeNumberOfLoops();
   bool hasNews();
   bool hasExceptionHandlers();

   ToNumberMap  &getToNumberMap()  { return _toNumberMap; }
   ToStringMap  &getToStringMap()  { return _toStringMap; }
   ToCommentMap &getToCommentMap() { return _toCommentMap; }

   /**
    * @brief Answers whether a call to the provided resolved method would represent a
    *        recursive method call from the method currently being compiled.
    *
    * @param[in] targetResolvedMethod : the method symbol to check
    *
    * @return true if calling the targetResolvedMethod is a recursive call from the
    *         method being compiled; false otherwise.
    */
   bool isRecursiveMethodTarget(TR_ResolvedMethod *targetResolvedMethod);

   /**
    * @brief Answers whether a call to the provided target symbol would represent a
    *        recursive method call from the method currently being compiled.
    *
    * @param[in] targetSymbol : the method symbol to check
    *
    * @return true if calling the targetSymbol is a recursive call from the method
    *         being compiled; false otherwise.
    */
   bool isRecursiveMethodTarget(TR::Symbol *targetSymbol);

   // ==========================================================================
   // Should be in Code Generator
   //
   // J9

   TR::list<TR::Instruction*> *getStaticPICSites() {return &_staticPICSites;}
   TR::list<TR::Instruction*> *getStaticHCRPICSites() {return &_staticHCRPICSites;}
   TR::list<TR::Instruction*> *getStaticMethodPICSites() {return &_staticMethodPICSites;}

   bool isPICSite(TR::Instruction *instruction);

   TR::list<TR::Snippet*> *getSnippetsToBePatchedOnClassRedefinition();

  /*
   * \brief
   *    Inserts new first block into the IL
   *
   * \note
   *    Inserts an empty block before the current first block
   *    Moves glRegDeps from the current first block to the new one
   *
   */
   TR::Block *insertNewFirstBlock();

   TR::RegisterCandidates *getGlobalRegisterCandidates() { return _globalRegisterCandidates; }
   void setGlobalRegisterCandidates(TR::RegisterCandidates *t) { _globalRegisterCandidates = t; }

   bool hasNativeCall()                         { return _flags.testAny(HasNativeCall); }
   void setHasNativeCall()                      { _flags.set(HasNativeCall); }

   /*
   * \brief
   *    This query tells whether the trees might contain rdbar/wrtbar opcodes
   *    that are not fully supported by optimizer yet
   *
   * \note
   *    This query is for temporarily fixing up unanchored rdbars or disable
   *    opts not supporting the newly added rdbar/wrtbar opcodes. Subprojects
   *    can override the answer. This query should be deleted eventually if
   *    all the optimizations support the opcodes.
   */
   bool incompleteOptimizerSupportForReadWriteBarriers();

   // P codegen
   TR::list<TR_PrefetchInfo*> &getExtraPrefetchInfo() { return _extraPrefetchInfo; }
   TR_PrefetchInfo *findExtraPrefetchInfo(TR::Node * node, bool use = true);
   TR_PrefetchInfo *removeExtraPrefetchInfo(TR::Node * node);

   // J9
   // for TR_checkcastANDNullCHK
   TR::list<TR_Pair<TR_ByteCodeInfo, TR::Node> *> &getCheckcastNullChkInfo() { return _checkcastNullChkInfo; }

   // ==========================================================================
   // Should be in Optimizer
   //

   // J9
   bool getLoopWasVersionedWrtAsyncChecks() { return _loopVersionedWrtAsyncChecks; }
   void setLoopWasVersionedWrtAsyncChecks(bool v) { _loopVersionedWrtAsyncChecks = v; }

   // J9, used by VP to decide if a sync is needed at a monexit
   bool syncsMarked() { return _flags.testAny(SyncsMarked); }
   void setSyncsMarked() { _flags.set(SyncsMarked); }

   void setLoopTransferDone() {  _flags.set(LoopTransferDone); }
   bool isLoopTransferDone() { return  _flags.testAny(LoopTransferDone); }

   // ..........................................................................
   // Inliner
   BitVectorPool& getBitVectorPool () { return _bitVectorPool; }
   TR_Stack<int32_t> & getInlinedCallStack() {return _inlinedCallStack; }
   uint16_t getInlineDepth() {return _inlinedCallStack.size();}
   uint16_t getMaxInlineDepth() {return _maxInlineDepth;}
   bool incInlineDepth(TR::ResolvedMethodSymbol *, TR::Node *callNode, bool directCall, TR_VirtualGuardSelection *guard, TR_OpaqueClassBlock *receiverClass, TR_PrexArgInfo *argInfo = 0);
   bool incInlineDepth(TR::ResolvedMethodSymbol *, TR_ByteCodeInfo &, int32_t cpIndex, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo = 0);
   void decInlineDepth(bool removeInlinedCallSitesEntries = false);
   int16_t  adjustInlineDepth(TR_ByteCodeInfo & bcInfo);
   void     resetInlineDepth();
   int16_t restoreInlineDepth(TR_ByteCodeInfo &existingInfo);
   int16_t  matchingCallStackPrefixLength(TR_ByteCodeInfo &bcInfo);
   bool foundOnTheStack(TR_ResolvedMethod *, int32_t);
   int32_t getInlinedCalls() { return _inlinedCalls; }
   void incInlinedCalls() { _inlinedCalls++; }


protected:
   bool incInlineDepth(TR_OpaqueMethodBlock *, TR::ResolvedMethodSymbol *, TR_ByteCodeInfo &, TR::SymbolReference *callSymRef, bool directCall, TR_PrexArgInfo *argInfo, TR_AOTMethodInfo *aotMethodInfo);

public:
   TR_ExternalRelocationTargetKind getReloTypeForMethodToBeInlined(TR_VirtualGuardSelection *guard, TR::Node *callNode, TR_OpaqueClassBlock *receiverClass) { return TR_NoRelocation; }

   class TR_InlinedCallSiteInfo
      {
      TR_InlinedCallSite _site;
      TR::ResolvedMethodSymbol *_resolvedMethod;
      TR::SymbolReference *_callSymRef;
      int32_t *_osrCallSiteRematTable;
      bool _directCall;
      bool _cannotAttemptOSRDuring;
      TR_AOTMethodInfo *_aotMethodInfo;

      public:

      TR_InlinedCallSiteInfo(TR_OpaqueMethodBlock *method,
                             TR_ByteCodeInfo &bcInfo,
                             TR::ResolvedMethodSymbol *resolvedMethod,
                             TR::SymbolReference *callSymRef,
                             bool directCall,
                             TR_AOTMethodInfo *aotMethodInfo = NULL):
         _resolvedMethod(resolvedMethod),
         _callSymRef(callSymRef),
         _directCall(directCall),
         _osrCallSiteRematTable(0),
         _cannotAttemptOSRDuring(false),
         _aotMethodInfo(aotMethodInfo)
         {
         _site._methodInfo = method;
         _site._byteCodeInfo = bcInfo;
         }

      TR_InlinedCallSite &site() { return _site; }
      TR_ResolvedMethod *resolvedMethod() { return _resolvedMethod->getResolvedMethod(); }
      TR::ResolvedMethodSymbol *resolvedMethodSymbol() { return _resolvedMethod; }
      TR::SymbolReference *callSymRef(){ return _callSymRef; }
      int32_t *osrCallSiteRematTable() { return _osrCallSiteRematTable; }
      void setOSRCallSiteRematTable(int32_t *array) { _osrCallSiteRematTable = array; }
      bool directCall() { return _directCall; }
      bool cannotAttemptOSRDuring() { return _cannotAttemptOSRDuring; }
      void setCannotAttemptOSRDuring(bool cannotOSR) { _cannotAttemptOSRDuring = cannotOSR; }
      TR_AOTMethodInfo *aotMethodInfo() { return _aotMethodInfo; }
      };

   uint32_t getNumInlinedCallSites();

   TR_InlinedCallSite &getInlinedCallSite(uint32_t index);
   TR_ResolvedMethod  *getInlinedResolvedMethod(uint32_t index);
   TR::ResolvedMethodSymbol  *getInlinedResolvedMethodSymbol(uint32_t index);
   TR::SymbolReference *getInlinedCallerSymRef(uint32_t index);
   uint32_t getOSRCallSiteRematSize(uint32_t callSiteIndex);
   void getOSRCallSiteRemat(uint32_t callSiteIndex, uint32_t slot, TR::SymbolReference *&ppSymRef, TR::SymbolReference *&loadSymRef);
   void setOSRCallSiteRemat(uint32_t callSiteIndex, TR::SymbolReference *ppSymRef, TR::SymbolReference *loadSymRef);
   bool isInlinedDirectCall(uint32_t index);
   bool cannotAttemptOSRDuring(uint32_t index);
   void setCannotAttemptOSRDuring(uint32_t index, bool cannot);
   TR_AOTMethodInfo *getInlinedAOTMethodInfo(uint32_t index);

   TR_InlinedCallSite *getCurrentInlinedCallSite();
   int32_t getCurrentInlinedSiteIndex();
   TR_PrexArgInfo *getCurrentInlinedCallArgInfo();
   TR_Stack<TR_PrexArgInfo *>& getInlinedCallArgInfoStack();

   void incNumLivePendingPushSlots(int32_t n) { _numLivePendingPushSlots = _numLivePendingPushSlots + n; }
   int32_t getNumLivePendingPushSlots() { return _numLivePendingPushSlots; }

   void incNumLoopNestingLevels(int32_t n) { _numNestingLevels = _numNestingLevels + n; }
   int32_t getNumLoopNestingLevels() { return _numNestingLevels; }


   //...........................................................................
   // Peeking
   TR_Stack<TR_PeekingArgInfo *> *getPeekingArgInfo();
   TR_PeekingArgInfo *getCurrentPeekingArgInfo();
   void addPeekingArgInfo(TR_PeekingArgInfo *info);
   void removePeekingArgInfo();
   TR::SymbolReferenceTable *getPeekingSymRefTab() { return _peekingSymRefTab; }
   void setPeekingSymRefTab(TR::SymbolReferenceTable *symRefTab) { _peekingSymRefTab = symRefTab; }
   int32_t getMaxPeekedBytecodeSize() const { return TR::Options::getMaxPeekedBytecodeSize() >> (_optimizationPlan->getReduceMaxPeekedBytecode());}

   void AddCopyPropagationRematerializationCandidate(TR::SymbolReference * sr);
   void RemoveCopyPropagationRematerializationCandidate(TR::SymbolReference * sr);
   bool IsCopyPropagationRematerializationCandidate(TR::SymbolReference * sr);

   // ==========================================================================
   // RAS
   //
   int32_t getPrevSymRefTabSize() { return _prevSymRefTabSize; }
   void setPrevSymRefTabSize( int32_t prevSize ) { _prevSymRefTabSize = prevSize; }
   void dumpMethodTrees(const char *title, TR::ResolvedMethodSymbol * = 0);
   void dumpMethodTrees(const char *title1, const char *title2, TR::ResolvedMethodSymbol * = 0);
   void dumpFlowGraph( TR::CFG * = 0);

   bool getAddressEnumerationOption(TR_CompilationOptions o) {return _options->getAddressEnumerationOption(o);}

   TR::ILValidator *getILValidator() { return _ilValidator; }
   void setILValidator(TR::ILValidator *ilValidator) { _ilValidator = ilValidator; }
   void validateIL(TR::ILValidationContext ilValidationContext);

   void verifyTrees(TR::ResolvedMethodSymbol *s = 0);
   void verifyBlocks(TR::ResolvedMethodSymbol *s = 0);
   void verifyCFG(TR::ResolvedMethodSymbol *s = 0);

   /*
   * \brief
   *    Check to make sure the rdbars are anchored and anchor a rdbar if it's found unanchored
   *
   * \note
   *    Ideally all optimizations should anchor a rdbar when it's created.
   *    This API is for fixing the unanchored rdbars in case some optimizations forget to
   *    anchor the node by mistake. Optimizations should still try to generate correct rdbar
   *    trees instead of relying on this API.
   */
   void verifyAndFixRdbarAnchors();

   void setIlVerifier(TR::IlVerifier *ilVerifier) { _ilVerifier = ilVerifier; }

   typedef std::pair<const void * const, TR::DebugCounterBase *> DebugCounterEntry;
   typedef TR::typed_allocator<DebugCounterEntry, TR::Allocator> DebugCounterMapAllocator;
   typedef std::map<const void *, TR::DebugCounterBase *, std::less<const void *>, DebugCounterMapAllocator> DebugCounterMap;

   /**
    * @brief mapStaticAddressToCounter
    * @param symRef the symref containing the static address of the memory used to
    *               store the count value of the debug counter
    * @param counter pointer to the debug counter
    *
    * Maps the staticAddress of the symRef to the debug counter. The map has to be between the static address
    * and the debug counter, instead of the symref of the static address and the debug counter, because sometimes,
    * transformations can create a new symref that will not exist in this map; however because the static
    * address is constant, it is guaranteed to exist in the map.
    */
   void mapStaticAddressToCounter(TR::SymbolReference *symRef, TR::DebugCounterBase *counter);

   /**
    * @brief getCounterFromStaticAddress
    * @param symRef the symref containingthe static address of the memory used to
    *               store the count value of the debug counter
    * @return the debug counter associated with the static address
    */
   TR::DebugCounterBase *getCounterFromStaticAddress(TR::SymbolReference *symRef);


#ifdef DEBUG
   void dumpMethodGraph(int index, TR::ResolvedMethodSymbol * = 0);
#endif

   int32_t getVerboseOptTransformationCount() { return _verboseOptTransformationCount; }
   void incVerboseOptTransformationCount(int32_t delta=1) { _verboseOptTransformationCount += delta; }

   int32_t getNodeOpCodeLength() { return _nodeOpCodeLength; }
   void setNodeOpCodeLength( int32_t opCodeLen ) { _nodeOpCodeLength = opCodeLen; }
   void incrNodeOpCodeLength( int32_t incr ) { _nodeOpCodeLength += incr; }

   // ==========================================================================
   // CHTable
   //

   TR_DevirtualizedCallInfo * getFirstDevirtualizedCall();
   TR_DevirtualizedCallInfo * createDevirtualizedCall(TR::Node *,
                        TR_OpaqueClassBlock *);
   TR_DevirtualizedCallInfo * findDevirtualizedCall(TR::Node *);
   TR_DevirtualizedCallInfo * findOrCreateDevirtualizedCall(TR::Node *,
                         TR_OpaqueClassBlock *);

   typedef TR::typed_allocator<TR_VirtualGuard*, TR::Region&> GuardSetAlloc;
   typedef std::less<TR_VirtualGuard*> GuardSetCmp;
   typedef std::set<TR_VirtualGuard*, GuardSetCmp, GuardSetAlloc> GuardSet;

   const GuardSet &getVirtualGuards() { return _virtualGuards; }
   TR_VirtualGuard *findVirtualGuardInfo(TR::Node *);
   void addVirtualGuard   (TR_VirtualGuard *guard);
   void removeVirtualGuard(TR_VirtualGuard *guard);

   TR::Node *createDummyOrSideEffectGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *);
   TR::Node *createSideEffectGuard(TR::Compilation *, TR::Node *, TR::TreeTop *);
   TR::Node *createAOTInliningGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *, TR_VirtualGuardKind);
   TR::Node *createAOTGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *, TR_VirtualGuardKind);
   TR::Node *createDummyGuard(TR::Compilation *, int16_t, TR::Node *, TR::TreeTop *);

   TR_LinkHead<TR_ClassLoadCheck> *getClassesThatShouldNotBeLoaded() { return &_classesThatShouldNotBeLoaded; }
   TR_LinkHead<TR_ClassExtendCheck> *getClassesThatShouldNotBeNewlyExtended() { return &_classesThatShouldNotBeNewlyExtended;}

   void setHasMethodOverrideAssumptions(bool v = true) { _flags.set(HasMethodOverrideAssumptions); }
   bool hasMethodOverrideAssumptions() { return _flags.testAny(HasMethodOverrideAssumptions); }

   void setHasClassExtendAssumptions(bool v = true) { _flags.set(HasClassExtendAssumptions); }
   bool hasClassExtendAssumptions() { return _flags.testAny(HasClassExtendAssumptions); }

   // ==========================================================================
   // Compilation
   //

   int32_t compile();

   static void shutdown(TR_FrontEnd *);

   void performOptimizations();

   bool isOptServer() const { return _isOptServer;}
   bool isServerInlining() const { return _isServerInlining; }

   bool isRecompilationEnabled() { return false; }

   int32_t getOptLevel();

   TR_Hotness getNextOptLevel() { return _nextOptLevel; }
   void setNextOptLevel(TR_Hotness nextOptLevel) { _nextOptLevel = nextOptLevel; }
   TR_Hotness getMethodHotness();
   static const char *getHotnessName(TR_Hotness t);
   const char *getHotnessName();

   template<typename Exception>
   void failCompilation(const char *format, ...)
      {
      char buffer[512];

      va_list args;
      va_start(args, format);
      vsnprintf (buffer, sizeof(buffer), format, args);
      va_end(args);

      OMR::Compilation::reportFailure(buffer);
      throw Exception();
      }

   void setOptimizationPlan(TR_OptimizationPlan *optimizationPlan ) {_optimizationPlan = optimizationPlan;}
   TR_OptimizationPlan * getOptimizationPlan() {return _optimizationPlan;}

   bool isProfilingCompilation();
   ProfilingMode getProfilingMode();
   bool isJProfilingCompilation();

   TR::Recompilation *getRecompilationInfo() { return _recompilationInfo; }
   void setRecompilationInfo(TR::Recompilation * i) { _recompilationInfo = i;    }

   /// Profiler info
   /// @{
   bool haveCommittedCallSiteInfo()                         { return _commitedCallSiteInfo; }
   void setCommittedCallSiteInfo(bool b)                    { _commitedCallSiteInfo = b; }
   /// @}

   ncount_t getNodeCount();
   ncount_t generateAccurateNodeCount();
   ncount_t getAccurateNodeCount();

   PhaseTimingSummary  &phaseTimer()        { return _phaseTimer; }
   TR::PhaseMemSummary &phaseMemProfiler()  { return _phaseMemProfiler; }
   TR::NodePool        &getNodePool()       { return *_compilationNodes; }

   bool mustNotBeRecompiled();
   bool shouldBeRecompiled();
   bool couldBeRecompiled();

   bool hasBlockFrequencyInfo();
   bool usesPreexistence() { return _usesPreexistence; }
   void setUsesPreexistence(bool v);

   bool hasUnsafeSymbol()                       { return _flags.testAny(HasUnsafeSymbol); }
   void setHasUnsafeSymbol()                    { _flags.set(HasUnsafeSymbol); }

   bool areSlotsSharedByRefAndNonRef()          { return _flags.testAny(SlotsSharedByRefAndNonRef); }
   void setSlotsSharedByRefAndNonRef(bool b)    { _flags.set(SlotsSharedByRefAndNonRef, b); }

   bool performVirtualGuardNOPing();
   bool isVirtualGuardNOPingRequired(TR_VirtualGuard *virtualGuard = NULL);

   // check if using compressed pointers
   //
   bool useCompressedPointers();
   bool useAnchors();

   void addGenILSym(TR::ResolvedMethodSymbol *s) { _genILSyms.push_front(s); }

   // Arraylet availability in this compilation unit.
   //
   bool generateArraylets();

   // Are spine checks for array element accesses required in this compilation unit.
   //
   bool requiresSpineChecks();

   TR::list<TR_Pair<TR::Node, uint32_t> *> &getNodesThatShouldPrefetchOffset()
      { return _nodesThatShouldPrefetchOffset; }
   int32_t findPrefetchInfo(TR::Node * node);

   bool canTransformUnsafeCopyToArrayCopy();
   bool canTransformUnsafeSetMemory();

   bool supportsInduceOSR();
   bool canAffordOSRControlFlow();
   void setCanAffordOSRControlFlow(bool b) { _canAffordOSRControlFlow = b; }
   bool penalizePredsOfOSRCatchBlocksInGRA();

   /*
    * Return true if the supplied method in the inlining table is known to not run
    * for very long and so not require asyncchecks or other yields
    */
   bool isShortRunningMethod(int32_t callerIndex);

   /*
    * isPotentialOSRPoint - used to check if a given treetop could be a point
    * where we would make an OSR transition from the JIT'd code to the interpreter.
    * Normal usage requires you to pass in a TreeTop and the transition node will
    * be distilled from that. Supplying a node directly is a very special operation
    * and is primarily intended for ILGen before blocks are created - you risk
    * incorrect answers if you fail to supply the TreeTop.
    *
    * osrPointNode can be used to identify the node that is believed to be the
    * potential OSR point. Multiple OSR points are not expected within a tree.
    *
    * The ignoreInfra flag will result in the call checking the node even if
    * OSR infrastructure has been removed. By default, if infrastructure has been
    * removed, this call will always return false.
    */
   bool isPotentialOSRPoint(TR::Node *node, TR::Node **osrPointNode=NULL, bool ignoreInfra=false);
   bool isPotentialOSRPointWithSupport(TR::TreeTop *tt);

   TR::OSRMode getOSRMode();
   TR::OSRTransitionTarget getOSRTransitionTarget();
   bool isOSRTransitionTarget(TR::OSRTransitionTarget target);
   int32_t getOSRInductionOffset(TR::Node *node);
   bool requiresAnalysisOSRPoint(TR::Node *node);

   bool pendingPushLivenessDuringIlgen();

   // for OSR
   TR_OSRCompilationData* getOSRCompilationData() {return _osrCompilationData;}
   void setSeenClassPreventingInducedOSR();

   // for assumptions Flags

   void setHasClassUnloadAssumptions(bool v = true) { _flags.set(HasClassUnloadAssumptions); }
   bool hasClassUnloadAssumptions() { return _flags.testAny(HasClassUnloadAssumptions); }

   void setHasClassRedefinitionAssumptions(bool v = true) { _flags.set(HasClassRedefinitionAssumptions); }
   bool hasClassRedefinitionAssumptions() { return _flags.testAny(HasClassRedefinitionAssumptions); }

   void setHasClassPreInitializeAssumptions(bool v = true) { _flags.set(HasClassPreInitializeAssumptions); }
   bool hasClassPreInitializeAssumptions() { return _flags.testAny(HasClassPreInitializeAssumptions); }

   void setUsesBlockFrequencyInGRA() { _flags.set(UsesBlockFrequencyInGRA); }
   bool getUsesBlockFrequencyInGRA() { return _flags.testAny(UsesBlockFrequencyInGRA); }

   void setScratchSpaceLimit(size_t val) { _scratchSpaceLimit = val; }
   size_t getScratchSpaceLimit() { return _scratchSpaceLimit; }

   void setHasMethodHandleInvoke() { _flags.set(HasMethodHandleInvoke); }
   bool getHasMethodHandleInvoke() { return _flags.testAny(HasMethodHandleInvoke); }

   void setHasColdBlocks()
      {
      _flags.set(HasColdBlocks);
      }

   bool hasColdBlocks()
      {
      return _flags.testAny(HasColdBlocks);
      }

   /**
    * @brief Specify that the code cache should only contain read only code.
    */
   void setGenerateReadOnlyCode()
      {
      _flags.set(GenerateReadOnlyCode);
      }

   /**
    * @brief Query whether the code cache should only contain read only code.
    * @return true if generating read only code; false otherwise.
    */
   bool getGenerateReadOnlyCode()
      {
      return _flags.testAny(GenerateReadOnlyCode);
      }

   bool isGPUCompilation() { return _flags.testAny(IsGPUCompilation);}

#ifdef J9_PROJECT_SPECIFIC
   bool hasIntStreamForEach() { return _flags.testAny(HasIntStreamForEach);}
   void setHasIntStreamForEach() { _flags.set(HasIntStreamForEach); }
#endif

   bool isGPUCompileCPUCode() { return _flags.testAny(IsGPUCompileCPUCode);}
   ListHeadAndTail<char*>& getGPUPtxList() { return _gpuPtxList; }
   ListHeadAndTail<int32_t>& getGPUKernelLineNumberList() { return _gpuKernelLineNumberList; } //TODO: fix to get real line numbers
   void incGPUPtxCount() { _gpuPtxCount++; }
   int32_t getGPUPtxCount() { return _gpuPtxCount; }

   bool isDLT() { return _flags.testAny(IsDLTCompile);}

   // surely J9 specific
   void * getRelocatableMethodCodeStart() const { return _relocatableMethodCodeStart; }
   void setRelocatableMethodCodeStart(void *p) { _relocatableMethodCodeStart = p; }

   bool getFailCHTableCommit() const { return _failCHtableCommitFlag; }
   void setFailCHTableCommit(bool v) { _failCHtableCommitFlag = v; }

   TR_RandomGenerator &adhocRandom(){ return *_adhocRandom; } // Not recommended if you care about reproducibility
   TR_RandomGenerator &primaryRandom() { return *_primaryRandom; }
   int32_t convertNonDeterministicInput(int32_t i, int32_t max, TR_RandomGenerator *randomGen = 0, int32_t min = 0, bool emitVerbose = true);

   int64_t getCpuTimeSpentInCompilation(); // coarse value (~0.5 sec granularity). Result in ns
   TR_YesNoMaybe isCpuExpensiveCompilation(int64_t threshold); // threshold in ns

   // set a 32 bit field that will be printed if the VM crashes
   // typically, this should be used to represent the state of the
   // compilation
   //
   void reportILGeneratorPhase() {}
   void reportAnalysisPhase(uint8_t id) {}
   void reportOptimizationPhase(OMR::Optimizations) {}
   void reportOptimizationPhaseForSnap(OMR::Optimizations) {}

   typedef int32_t CompilationPhase;
   CompilationPhase saveCompilationPhase();
   void restoreCompilationPhase(CompilationPhase phase);
   class CompilationPhaseScope
      {
      TR::Compilation *_comp;
      CompilationPhase _savedPhase;

      public:

      CompilationPhaseScope(TR::Compilation *comp);
      ~CompilationPhaseScope();
      };

   /**
    * @brief getDebugCounterMap
    * @return reference to the TR::DebugCounterMap map
    */
   DebugCounterMap &getDebugCounterMap() { return _debugCounterMap; }

   /**
    * @brief Answers whether the compilation is an out of process compilation
    * @return true if the compilation is an out of process compilation
    */
   static bool isOutOfProcessCompilation() { return false; }

public:
#ifdef J9_PROJECT_SPECIFIC
   // Access to this list must be performed with assumptionTableMutex in hand
   OMR::RuntimeAssumption** getMetadataAssumptionList() { return &_metadataAssumptionList; }
   void setMetadataAssumptionList(OMR::RuntimeAssumption *a) { _metadataAssumptionList = a; }
#endif

   // To TransformUtil
   void setStartTree(TR::TreeTop * tt);

   /**
    * \brief
    *    Answers whether the fact that a method has not been executed yet implies
    *    that the method is cold.
    *
    * \return
    *    true if the fact that a method has not been executed implies it is cold;
    *    false otherwise
    */
   bool notYetRunMeansCold();

   TR::Region &aliasRegion();
   void invalidateAliasRegion();

   /** \brief
    *	    Requests the layout of a type. The layout here means how the fields
    *     are laid out in an object of the given type.
    *
    *  \param clazz
    *     Class of the type whose layout is requested.
    *
    *  \return
    *     Returns a TypeLayout object pointer.
    */
   const TR::TypeLayout* typeLayout(TR_OpaqueClassBlock * clazz);

   /**
    * \brief
    *    Returns a copy of the target env singleton that has been specialized
    *    for the current compilation
    *
    * \return
    *    reference to the copy of the target env
    */
   TR::Environment& target() { return _target; }

   /**
    * \brief
    *    The inline call target, if any, for which IL is currently being generated.
    *
    * \return
    *    the call target
    */
   TR_CallTarget *currentILGenCallTarget() { return _currentILGenCallTarget; }

   /**
    * \brief
    *    Set the inline call target for which IL is currently being generated.
    *
    * This should be set in the inliner just prior to generating IL to inline,
    * and it should be reset to null afterward.
    */
   void setCurrentILGenCallTarget(TR_CallTarget *x) { _currentILGenCallTarget = x; }

private:
   void resetVisitCounts(vcount_t, TR::ResolvedMethodSymbol *);
   int16_t restoreInlineDepthUntil(int32_t stopIndex, TR_ByteCodeInfo &currentInfo);
   void reportFailure(const char *reason);

protected:

   const char * _signature;
   TR::Options *_options;
   flags32_t _flags;
   TR::Region & _heapMemoryRegion;
   TR_Memory * _trMemory;

   TR_FrontEnd                       *_fe; // must be declared before _flowGraph
   TR::IlGenRequest                  &_ilGenRequest;
   TR::CodeGenerator                 *_codeGenerator;


   int16_t                            _currentOptIndex;
   int16_t                            _lastBegunOptIndex;
   int16_t                            _lastPerformedOptIndex;
   int32_t                            _currentOptSubIndex;
   int32_t                            _lastPerformedOptSubIndex;

   TR_Debug                      *_debug;
protected:
   TR::KnownObjectTable *_knownObjectTable;

   OMR_VMThread *_omrVMThread;

protected:
   enum // flags
      {
      HasUnsafeSymbol                   = 0x0000001,
      // AVAILABLE                      = 0x0000002,
      HasNativeCall                     = 0x0000004,
      // AVAILABLE                      = 0x0000008,
      SyncsMarked                       = 0x0000010,
      SlotsSharedByRefAndNonRef         = 0x0000020,
      IsGPUCompileCPUCode               = 0x0000040,
      LoopTransferDone                  = 0x0000080,
      HasMethodOverrideAssumptions      = 0x0000100,
      HasClassExtendAssumptions         = 0x0000200,
      HasClassUnloadAssumptions         = 0x0000400,
      HasClassPreInitializeAssumptions  = 0x0000800,
      // AVAILABLE                      = 0x0001000,
      IsDLTCompile                      = 0x0002000,
      HasClassRedefinitionAssumptions   = 0x0004000,
      // AVAILABLE                      = 0x0008000,
      HasIntStreamForEach               = 0x0010000,
      // AVAILABLE                      = 0x0020000,
      UsesBlockFrequencyInGRA           = 0x0040000,
      HasMethodHandleInvoke             = 0x0080000, // JSR292
      HasSuperColdBlock                 = 0x0100000,
      HasColdBlocks                     = 0x0200000,
      GenerateReadOnlyCode              = 0x0400000,
      // AVAILABLE                      = 0x0800000,
      // AVAILABLE                      = 0x1000000,
      // AVAILABLE                      = 0x2000000,
      // AVAILABLE                      = 0x4000000,
      IsGPUCompilation                  = 0x8000000,
      dummyLastEnum,

      AssumptionFlagMask                = 0x0005F00,
      };

   TR::ThreadLocalAllocator _allocator;
   TR::ResolvedMethodSymbol *_methodSymbol;

private:
   TR_ResolvedMethod                 *_method; // must be declared before _flowGraph
   TR_ArenaAllocator                 _arenaAllocator;
   TR::Region                        _aliasRegion;


   TR_IlGenerator                    *_ilGenerator;
   TR::ILValidator                    *_ilValidator;
   TR::Optimizer                      *_optimizer;
   TR::RegisterCandidates             *_globalRegisterCandidates;
   TR::SymbolReferenceTable          *_currentSymRefTab;
   TR::Recompilation                  *_recompilationInfo;
   TR_OptimizationPlan               *_optimizationPlan;

   TR_RandomGenerator*                 _primaryRandom; // Used to spawn other RandomGenerators to keep nondeterminism contained
   TR_RandomGenerator*                 _adhocRandom;   // Used by callers who can't be bothered to maintain their own TR_RandomGenerator

   TR_Array<TR::ResolvedMethodSymbol*> _methodSymbols;
   TR_Array<TR::SymbolReference*>      _resolvedMethodSymbolReferences;
   TR_Array<TR_InlinedCallSiteInfo>   _inlinedCallSites;
   TR_Stack<int32_t>                  _inlinedCallStack;
   TR_Stack<TR_PrexArgInfo *>         _inlinedCallArgInfoStack;
   TR::list<TR_DevirtualizedCallInfo*>     _devirtualizedCalls;
   int32_t                            _inlinedCalls;
   int16_t                           _inlinedFramesAdded;

   GuardSet _virtualGuards;

   TR_LinkHead<TR_ClassLoadCheck>     _classesThatShouldNotBeLoaded;
   TR_LinkHead<TR_ClassExtendCheck>   _classesThatShouldNotBeNewlyExtended;

   TR::list<TR::Instruction*>               _staticPICSites;
   TR::list<TR::Instruction*>               _staticHCRPICSites;
   TR::list<TR::Instruction*>               _staticMethodPICSites;
   TR::list<TR::ResolvedMethodSymbol*>      _genILSyms;

   TR::SymbolReferenceTable*            _symRefTab;

   TR_ReturnInfo                      _returnInfo;

   vcount_t                           _visitCount;
   ncount_t                           _nodeCount;               // _nodeCount is a global count of number of created nodes
   ncount_t                           _accurateNodeCount;       // this number is the current number of nodes in the trees. it can go stale
   ncount_t                           _lastValidNodeCount;      // set to _nodeCount when _accurateNumberOfNodes is set. Used to tell how many nodes have been created since

   uint16_t                           _maxInlineDepth;
   int32_t                            _numLivePendingPushSlots;
   int32_t                            _numNestingLevels;

   bool                              _usesPreexistence;
   bool                              _loopVersionedWrtAsyncChecks;
   bool                              _commitedCallSiteInfo;
   bool                              _containsBigDecimalLoad;
   bool                              _isOptServer;
   bool                              _isServerInlining;

   bool                              _ilGenSuccess;

   bool                              _osrStateIsReliable;
   bool                              _canAffordOSRControlFlow;
   bool                              _osrInfrastructureRemoved;

   TR_Hotness                        _nextOptLevel;
   int32_t                           _errorCode;

   TR_Stack<TR_PeekingArgInfo *>     _peekingArgInfo;
   TR::SymbolReferenceTable          *_peekingSymRefTab;

   // for TR_checkcastANDNullCHK
   TR::list<TR_Pair<TR_ByteCodeInfo, TR::Node> *> _checkcastNullChkInfo;

   TR::list<TR_Pair<TR::Node,uint32_t> *>        _nodesThatShouldPrefetchOffset;
   TR::list<TR_PrefetchInfo*>                   _extraPrefetchInfo;

   // for OSR
   TR_OSRCompilationData                *_osrCompilationData;

   TR::Block *                         _currentBlock;

   // for TR_Debug usage
   ToNumberMap    _toNumberMap; // maps TR::LabelSymbol, etc. objects to numbers
   ToStringMap    _toStringMap; // maps TR_SymbolReference, etc. objects to strings
   ToCommentMap   _toCommentMap; // maps list of strings, etc. objects to list of strings

   /**
    * @brief _debugCounterMap
    *
    * A map between the static address of the memory used to store the count value of
    * the debug counter and the debug counter
    */
   DebugCounterMap _debugCounterMap;



   int32_t                           _verboseOptTransformationCount;

protected:
#ifdef J9_PROJECT_SPECIFIC
   TR_CHTable *                      _transientCHTable;   // per compilation CHTable
   OMR::RuntimeAssumption *          _metadataAssumptionList; // A special OMR::RuntimeAssumption to play the role of a sentinel for a linked list
#endif
   TR::Environment                   _target;

private:
   void *                            _relocatableMethodCodeStart;
   const int32_t                     _compThreadID; // The ID of the supporting compilation thread; 0 for compilation an application thread
   volatile bool                     _failCHtableCommitFlag;

   PhaseTimingSummary                _phaseTimer;
   TR::PhaseMemSummary               _phaseMemProfiler;
   TR::NodePool                      *_compilationNodes;

   TR::SparseBitVector _copyPropagationRematerializationCandidates;

   // Length of the last node OpCode that was printed. This should get reset to 0 at the beginning of a line where it is needed, e.g. for printing optimization trees
   // it gets set to 0 in TR_Debug::printBasicPreNodeInfoAndIndent() which preceeds node opCode printouts. Any methods that print parts of node opCodes should
   // always increment this as there may be information following opCodes that need to be aligned as in the case of trees or in future development.
   int32_t                           _nodeOpCodeLength;

   // Size of the previous symbol refernce table- used to keep track of symbols recently added to the table
   int32_t                           _prevSymRefTabSize;

   size_t                            _scratchSpaceLimit;
   int64_t                           _cpuTimeAtStartOfCompilation;

   TR::IlVerifier                    *_ilVerifier;

   ListHeadAndTail<char*> _gpuPtxList;
   ListHeadAndTail<int32_t> _gpuKernelLineNumberList; //TODO: fix to get real line numbers
   int32_t _gpuPtxCount;

   BitVectorPool _bitVectorPool; //MUST be declared after _trMemory

   typedef TR::typed_allocator<std::pair<TR_OpaqueClassBlock* const, const TR::TypeLayout *>, TR::Region &> LayoutAllocator;
   typedef std::less<TR_OpaqueClassBlock*> LayoutComparator;
   typedef std::map<TR_OpaqueClassBlock *, const TR::TypeLayout *, LayoutComparator, LayoutAllocator> TypeLayoutMap;
   TypeLayoutMap _typeLayoutMap;

   TR_CallTarget *_currentILGenCallTarget;

   /*
    * This must be last
    * NOTE: TLS for Compilation needs to be set before any object that may use it is initialized.
    */
   TR::TLSCompilationManager _tlsManager;

   };


// extern DWORD compilation
//
TR_TLS_DECLARE(TR::Compilation *, compilation);

}
namespace TR {
   /// Returns the thread local compilation object.
   ///
   /// \note Inlined for performance
   inline TR::Compilation *comp()
      {
      return TR_TLS_GET(OMR::compilation, TR::Compilation *);
      }
}

namespace TR {
   Compilation & operator<< (Compilation &comp, const char *str);
   Compilation & operator<< (Compilation &comp, const int n);
   Compilation & operator<< (Compilation & comp, const ::TR_ByteCodeInfo& bcInfo);
}

#endif
