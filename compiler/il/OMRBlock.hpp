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

#ifndef OMR_BLOCK_INCL
#define OMR_BLOCK_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_BLOCK_CONNECTOR
#define OMR_BLOCK_CONNECTOR
namespace OMR { class Block; }
namespace OMR { typedef OMR::Block BlockConnector; }
#endif

#define MIN_PROFILED_FREQUENCY (.75f)

#include "infra/CfgNode.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimizer.hpp"

class TR_BitVector;
class TR_BlockStructure;
class TR_Debug;
namespace TR { class GlobalRegister; }
class TR_GlobalRegisterAllocator;
class TR_Memory;
class TR_RegionStructure;
namespace TR { class RegisterCandidate; }
namespace TR { class RegisterCandidates; }
class TR_ResolvedMethod;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CFGNode; }
namespace TR { class DebugCounterAggregation; }
namespace TR { class Instruction; }
namespace TR { class Symbol; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

// Pseudo-safe downcast from a CFG node to a TR::Block
//
static inline TR::Block *toBlock(TR::CFGNode *node)
   {
   #if DEBUG
      if (node != NULL)
         {
         TR_ASSERT(node->asBlock() != NULL, "Bad downcast from TR::CFGNode to TR::Block");
         }
   #endif
   return (TR::Block *)node;
   }

namespace OMR
{
/**
 * Order of Blocks:
 * There are two types of orders for blocks, CFG and textual order.
 * CFG (Control flow graph) orders the block in a control flow manner.
 * Textual order is just a linear order of blocks. It does not represent the control flow.
 * --> At any given point during compilation, the code compiled is represented by a linked list of treetops, which are broken up into blocks.
 * --> And this is the textual order
 *
 * To traverse blocks in CFG order, use getSuccessors(), getPredecessors()
 * To traverse blocks in textual order, use getNextBlock(), getPrevBlock()
 *
 */
class OMR_EXTENSIBLE Block : public TR::CFGNode
   {
   public:

   /// Downcast to concrete type
   TR::Block * self();

   Block(TR_Memory * m);
   Block(TR::CFG &cfg);

   /// Create a block with the given entry and exit.
   Block(TR::TreeTop *entry, TR::TreeTop *exit, TR_Memory * m);
   Block(TR::TreeTop *entry, TR::TreeTop *exit, TR::CFG &cfg);


   /// Copy Constructor.
   ///
   /// Uses the same memory as `other`
   Block(TR::Block &other, TR::TreeTop *entry, TR::TreeTop *exit);

   ~Block()
      {
      TR_ASSERT(0,"blocks should not be freed until the destructor is updated to also free the catch block extension");
      // Currently blocks are allocated on the heap and we never free them.
      // In the future, if we start freeing blocks, we should update
      // ensureCatchBlockExtensionExists to allocate the catch block extension
      // somewhere other than the heap, and then free _catchBlockExtension here
      // if it is not null.
      }

   virtual const char * getName(TR_Debug *);
   static TR::Block * createBlock(TR::TreeTop *entry, TR::TreeTop *exit, TR::CFG &cfg);
   static TR::Block * createEmptyBlock(TR::Node *, TR::Compilation *, int32_t frequency = -1, TR::Block *block = NULL);
   static TR::Block * createEmptyBlock(TR::Compilation *comp, int32_t frequency = -1, TR::Block *block = NULL);

   TR::Block *breakFallThrough(TR::Compilation *comp, TR::Block *faller, TR::Block *fallee);
   static void insertBlockAsFallThrough(TR::Compilation *comp, TR::Block *block, TR::Block *newFallThroughBlock);
   static void redirectFlowToNewDestination(TR::Compilation *comp, TR::CFGEdge *origEdge, TR::Block *newTo, bool useGotoForFallThrough);

   virtual TR::Block *asBlock();

   // return the first treetop in the block, skipping past any fences
   TR::TreeTop * getFirstRealTreeTop();

   // return the treetop before the exit, skipping past any fences
   TR::TreeTop * getLastRealTreeTop();

   // returns the treetop before any [un]conditional goto, return, etc.
   TR::TreeTop * getLastNonControlFlowTreeTop();

   int32_t getNumberOfRealTreeTops();

   /// getNextBlock and getPrevBlock return the next/previous block in the "textual" orders of blocks
   TR::Block * getNextBlock();
   TR::Block * getPrevBlock();

   TR::Block * getNextExtendedBlock();

   // append a node under a treetop and before the bbend
   TR::TreeTop *append(TR::TreeTop * tt);

   // preppend a node under a treetop immediately after the bbStart
   TR::TreeTop * prepend(TR::TreeTop * tt);

   TR::Block * split(TR::TreeTop * startOfNewBlock,  TR::CFG * cfg, bool fixupCommoning = false, bool copyExceptionSuccessors = true, TR::ResolvedMethodSymbol *methodSymbol = NULL);

   TR::Block * splitPostGRA(TR::TreeTop *startOfNewBlock, TR::CFG *cfg, bool copyExceptionSuccessors = true, TR::ResolvedMethodSymbol *methodSymbol = NULL);


   TR::Block * splitWithGivenMethodSymbol(TR::ResolvedMethodSymbol *methodSymbol, TR::TreeTop * startOfNewBlock,  TR::CFG * cfg, bool fixupCommoning = false, bool copyExceptionSuccessors = true);

   TR::Block *createConditionalSideExitBeforeTree(TR::TreeTop *tree,
                                                  TR::TreeTop *compareTree,
                                                  TR::TreeTop *exitTree,
                                                  TR::TreeTop *returnTree,
                                                  TR::CFG *cfg,
                                                  bool markCold = true);

   TR::Block * createConditionalBlocksBeforeTree(TR::TreeTop* tree,
                                                 TR::TreeTop* compareTree,
                                                 TR::TreeTop* ifTree,
                                                 TR::TreeTop* elseTree,
                                                 TR::CFG* cfg,
                                                 bool changeBlockExtensions = true,
                                                 bool markCold = true);

   TR::Block * splitEdge(TR::Block *, TR::Block *, TR::Compilation *, TR::TreeTop **lastTreeTop = NULL, bool findOptimalInsertionPoint = true);

   virtual void removeFromCFG(TR::Compilation *);

   // remove the branch treetop from the block as well as the associated branch edge.
   // removeBranch assumes the last nonfence node is a branch.
   //
   void removeBranch(TR::Compilation *);

   void takeGlRegDeps(TR::Compilation *comp, TR::Node *glRegDeps);

   TR::Block * findVirtualGuardBlock( TR::CFG *cfg);

   TR_RegionStructure *getParentStructureIfExists( TR::CFG *);
   TR_RegionStructure *getCommonParentStructureIfExists(TR::Block *,  TR::CFG *);

   void inheritBlockInfo(TR::Block * org, bool inheritFreq = true);

   bool isTargetOfJumpWhoseTargetCanBeChanged(TR::Compilation * comp);

   static int32_t getMaxColdFrequency(TR::Block *b1, TR::Block *b2);
   static int32_t getMinColdFrequency(TR::Block *b1, TR::Block *b2);
   static int32_t getScaledSpecializedFrequency(int32_t fastFrequency);

   TR::Block * startOfExtendedBlock();

   // change the target of the branch in the block, add an edge to the new block,
   // remove the edge from the previous destination block and update any GlRegDep Nodes
   //
   void changeBranchDestination(TR::TreeTop * newDestination,  TR::CFG *cfg, bool callerFixesRegdeps = false);

   TR::Node * findFirstReference(TR::Symbol * sym, vcount_t visitCount);
   void collectReferencedAutoSymRefsIn(TR::Compilation *comp, TR_BitVector *, vcount_t);
   void collectReferencedAutoSymRefsIn(TR::Compilation *comp, TR::Node *, TR_BitVector *, vcount_t);

   // Map for which standard exceptions can be caught by this (catch) block
   //
   enum
      {
      CanCatchNullCheck           = 0x00000001,
      CanCatchResolveCheck        = 0x00000002,
      CanCatchDivCheck            = 0x00000004,
      CanCatchBoundCheck          = 0x00000008,
      CanCatchArrayStoreCheck     = 0x00000010,
      CanCatchCheckCast           = 0x00000020,
      CanCatchNew                 = 0x00000040,
      CanCatchArrayNew            = 0x00000080,
      CanCatchMonitorExit         = 0x00000100,
      CanCatchUserThrows          = 0x00000200,
      CanCatchOSR                 = 0x00000400,
      CanCatchOverflowCheck       = 0x00000800,
      CanCatchNewvalue            = 0x00001000,
      CanCatchEverything          = 0x00001FFF, // Mask for all of the above
      };

   bool     canCatchExceptions(uint32_t flags); // uses the above enum

   bool     canFallThroughToNextBlock();
   bool     doesNotNeedLabelAtStart();
   TR::CFGEdge * getFallThroughEdgeInEBB();

   void setByteCodeIndex(int32_t index, TR::Compilation *comp);

   bool endsInGoto();
   bool isGotoBlock(TR::Compilation * comp, bool allowPrecedingSnapshots=false);
   bool endsInBranch();
   bool isEmptyBlock();

   TR::TreeTop *getExceptingTree();

   bool hasExceptionPredecessors();
   bool hasExceptionSuccessors();

   /**
    * Field functions
    */

   TR::TreeTop *getEntry() { return _pEntry; }
   TR::TreeTop *setEntry(TR::TreeTop *p)
      {
      if (TR::comp()->getOptimizer() && !TR::comp()->isPeekingMethod())
         {
         TR::comp()->getOptimizer()->setCachedExtendedBBInfoValid(false);
         }
      return (_pEntry = p);
      }

   TR::TreeTop *getExit() { return _pExit; }
   TR::TreeTop *setExit(TR::TreeTop *p)
      {
      if (TR::comp()->getOptimizer() && !TR::comp()->isPeekingMethod())
         {
         TR::comp()->getOptimizer()->setCachedExtendedBBInfoValid(false);
         }
      return (_pExit = p);
      }

   TR_BitVector *getLiveLocals()                { return _liveLocals; }
   TR_BitVector *setLiveLocals(TR_BitVector* v) { return (_liveLocals = v); }

   TR_BlockStructure *getStructureOf()                     { return _pStructureOf; }
   TR_BlockStructure *setStructureOf(TR_BlockStructure *p) { return (_pStructureOf = p); }
   int32_t getNestingDepth();

   TR_Array<TR::GlobalRegister> & getGlobalRegisters(TR::Compilation *);
   void clearGlobalRegisters() { _globalRegisters = NULL; }

   struct InstructionBoundaries : TR_Link<InstructionBoundaries>
      {
      InstructionBoundaries(uint32_t s = UINT_MAX, uint32_t e = UINT_MAX) : _startPC(s), _endPC(e) { }

      uint32_t _startPC;
      uint32_t _endPC;
      };

   void setInstructionBoundaries(uint32_t startPC, uint32_t endPC);
   InstructionBoundaries & getInstructionBoundaries()  { return _instructionBoundaries; }

   void addExceptionRangeForSnippet(uint32_t startPC, uint32_t endPC);
   InstructionBoundaries * getFirstSnippetBoundaries() { return _snippetBoundaries.getFirst(); }

   TR::Instruction *getFirstInstruction()                         { return _firstInstruction; }
   TR::Instruction *setFirstInstruction(TR::Instruction *i)       { return (_firstInstruction = i); }

   TR::Instruction *getLastInstruction()                          { return _lastInstruction; }
   TR::Instruction *setLastInstruction(TR::Instruction *i)        { return (_lastInstruction = i); }

   class TR_CatchBlockExtension
      {
   public:
      TR_ALLOC(TR_Memory::CatchBlockExtension)

      TR_CatchBlockExtension()
         : _exceptionClass(NULL), _exceptionClassNameChars(NULL), _exceptionClassNameLength(0),
           _catchType(0), _exceptionsCaught(0), _handlerIndex(0), _inlineDepth(0), _owningMethod(NULL), _isSyntheticHandler(false) {}

      TR_CatchBlockExtension(TR_CatchBlockExtension &other)
         : _exceptionClass(other._exceptionClass), _exceptionClassNameChars(other._exceptionClassNameChars),
           _exceptionClassNameLength(other._exceptionClassNameLength), _catchType(other._catchType),
           _exceptionsCaught(other._exceptionsCaught), _handlerIndex(other._handlerIndex),
           _inlineDepth(other._inlineDepth), _owningMethod(other._owningMethod), _byteCodeInfo(other._byteCodeInfo), _isSyntheticHandler(other._isSyntheticHandler){}

      TR_OpaqueClassBlock *                 _exceptionClass;
      char                *                 _exceptionClassNameChars;
      int32_t                               _exceptionClassNameLength;
      uint32_t                              _exceptionsCaught;
      uint32_t                              _catchType;
      TR_ResolvedMethod *                   _owningMethod;
      TR_ByteCodeInfo                       _byteCodeInfo;
      int32_t                               _handlerIndex;
      uint8_t                               _inlineDepth;
      bool                                  _isSyntheticHandler; // indicate whether the exception handler is inserted by the compiler rather than existing in the source code
      };

   TR_CatchBlockExtension* getCatchBlockExtension()               { return _catchBlockExtension; }
   void setCatchBlockExtension(TR_CatchBlockExtension *extension) { _catchBlockExtension = extension; }

   void setHandlerInfo(uint32_t c, uint8_t d, int32_t i, TR_ResolvedMethod * m, TR::Compilation *comp);
   void setHandlerInfoWithOutBCInfo(uint32_t c, uint8_t d, int32_t i, TR_ResolvedMethod * m, TR::Compilation *comp); //also used for estimatecodesize dummy blocks

   bool                   isCatchBlock();
   uint32_t               getCatchType();
   uint8_t                getInlineDepth();
   int32_t                getHandlerIndex();
   TR_ByteCodeInfo        getByteCodeInfo();
   TR_OpaqueClassBlock *  getExceptionClass();
   char *                 getExceptionClassNameChars();
   int32_t                getExceptionClassNameLength();
   void                   setExceptionClassName(char *c, int32_t length, TR::Compilation *comp);
   TR_ResolvedMethod *    getOwningMethod();

   void setUnrollFactor(int factor)                               { _unrollFactor = factor; }
   uint16_t getUnrollFactor()                                     { return _unrollFactor; }

   void setBlockSize(int32_t s)                                   { _blockSize = s; }
   int32_t getBlockSize()                                         { return _blockSize; }

   // TODO: These access members that are only used in J9EstimateCodeSize and
   // should be deleted when the members are moved out.
   void     setBlockBCIndex(int32_t len)                          { _blockBCIndex = len; }
   int32_t  getBlockBCIndex()                                     {  return _blockBCIndex; }
   void     setJ9EstimateCodeSizeMethod(TR_ResolvedMethod *m)     { _j9EstimateSizeMethod = m; }
   TR_ResolvedMethod *getJ9EstimateCodeSizeMethod()               { return _j9EstimateSizeMethod; }

   TR::DebugCounterAggregation *getDebugCounters()                   { return _debugCounters; }
   void setDebugCounters(TR::DebugCounterAggregation *debugCounters) { _debugCounters = debugCounters; }

   // getNormalizedFrequency returns a value between 0 and 100.  getGlobalNormalizedFrequency multiplies the
   // normalizedfrequency by 10 for hot methods and by 100 for scorching methods.
   //
   int32_t getNormalizedFrequency(TR::CFG *);
   int32_t getGlobalNormalizedFrequency(TR::CFG *);

   bool verifyOSRInduceBlock(TR::Compilation *);

   /**
    * Field functions end
    */

   /**
    * Flag functions
    */

   void setIsExtensionOfPreviousBlock(bool b = true);
   bool isExtensionOfPreviousBlock();

   void setIsCold(bool b = true)                      { _flags.set(_isCold, b); }
   bool isCold()                                      { return _flags.testAny(_isCold); }

   void setIsSuperCold(bool v = true);
   bool isSuperCold();

   void setDoNotProfile()                             { _flags.set(_doNotProfile); }
   bool doNotProfile()                                { return _flags.testAny(_doNotProfile); }

   void setSpecializedDesyncCatchBlock()              { _flags.set(_specializedDesyncCatchBlock); }
   bool specializedDesyncCatchBlock()                 { return _flags.testAny(_specializedDesyncCatchBlock); }

   void setFirstBlockInLoop()                         { _flags.set(_firstBlockInLoop); }
   bool firstBlockInLoop()                            { return _flags.testAny(_firstBlockInLoop); }

   void setBranchesBackwards()                        { _flags.set(_branchingBackwards); }
   bool branchesBackwards()                           { return _flags.testAny(_branchingBackwards); }

   void setIsSynchronizedHandler()                    { _flags.set(_isSynchronizedHandler); }
   bool isSynchronizedHandler()                       { return _flags.testAny(_isSynchronizedHandler); }

   void setIsPRECandidate(bool b)                     { _flags.set(_isPRECandidate, b); }
   bool isPRECandidate()                              { return _flags.testAny(_isPRECandidate); }

   void setIsAdded()                                  { _flags.set(_isAdded); }
   bool isAdded()                                     { return _flags.testAny(_isAdded); }

   void setIsOSRInduceBlock()                         { _flags.set(_isOSRInduceBlock); }
   bool isOSRInduceBlock()                            { return _flags.testAny(_isOSRInduceBlock); }

   void setIsOSRCodeBlock()                           { _flags.set(_isOSRCodeBlock); }
   bool isOSRCodeBlock()                              { return _flags.testAny(_isOSRCodeBlock); }

   void setIsOSRCatchBlock()                          { _flags.set(_isOSRCatchBlock); }
   bool isOSRCatchBlock()                             { return _flags.testAny(_isOSRCatchBlock); }

   void setIsCreatedAtCodeGen(bool b = true)          { _flags.set(_createdAtCodeGen, b); }
   bool isCreatedAtCodeGen()                          { return _flags.testAny(_createdAtCodeGen); }

   void setHasCalls(bool b)                           { _flags.set(_hasCalls, b); }
   bool hasCalls()                                    { return _flags.testAny(_hasCalls); }

   void setHasCallToSuperCold(bool b)                 { _flags.set(_hasCallToSuperCold, b); }
   bool hasCallToSuperCold()                          { return _flags.testAny(_hasCallToSuperCold); }

   void setIsSpecialized(bool b = true)               { _flags.set(_isSpecialized, b); }
   bool isSpecialized()                               { return _flags.testAny(_isSpecialized); }

   bool isLoopInvariantBlock()                        { return _flags.testAny(_isLoopInvariantBlock); }
   void setAsLoopInvariantBlock(bool b)               { _flags.set(_isLoopInvariantBlock, b); }

   bool isCreatedByVersioning()                       { return _flags.testAny(_isCreatedByVersioning); }
   void setCreatedByVersioning(bool b)                { _flags.set(_isCreatedByVersioning, b); }

   bool isEntryOfShortRunningLoop()                   { return _flags.testAny(_isEntryOfShortRunningLoop); }
   void setIsEntryOfShortRunningLoop()                { _flags.set(_isEntryOfShortRunningLoop, true); }

   bool wasHeaderOfCanonicalizedLoop()                { return _flags.testAny(_wasHeaderOfCanonicalizedLoop); }
   void setWasHeaderOfCanonicalizedLoop(bool b)       { _flags.set(_wasHeaderOfCanonicalizedLoop, b); }

   bool isLastWarmBlock()                             { return _flags.testAny(_isLastWarmBlock); }
   void setIsLastWarmBlock(bool b = true)             { _flags.set(_isLastWarmBlock, b); }

   bool isSyntheticHandler()                          { return  _catchBlockExtension && _catchBlockExtension->_isSyntheticHandler; }
   void setIsSyntheticHandler();

   enum partialFlags // Stored in lowest 8 bits of _moreflags
      {
      _unsanitizeable         = 0x00000001,
      _containsCall           = 0x00000002,
      _restartBlock           = 0x00000004,
      _partialInlineBlock     = 0x00000008,
      _endBlock               = 0x00000010,
      _difficultBlock         = 0x00000020,
      // Available            = 0x00000040,
      // Available            = 0x00000080
      };

   flags16_t getPartialFlags() {return flags16_t(_moreflags.getValue(0xFF));} // J9

   void setIsUnsanitizeable(bool b = true)    { _moreflags.set(_unsanitizeable,b); } // J9
   bool isUnsanitizeable()                    { return _moreflags.testAny(_unsanitizeable); }

   void setContainsCall(bool b = true)        { _moreflags.set(_containsCall,b); }
   bool containsCall()                        { return _moreflags.testAny(_containsCall); }

   void setRestartBlock(bool b = true)        { _moreflags.set(_restartBlock,b); } // J9
   bool isRestartBlock()                      { return _moreflags.testAny(_restartBlock); }

   void setPartialInlineBlock(bool b = true)  { _moreflags.set(_partialInlineBlock,b); }
   bool isPartialInlineBlock()                { return _moreflags.testAny(_partialInlineBlock); }

   void setIsEndBlock(bool b = true)          { _moreflags.set(_endBlock,b); }
   bool isEndBlock()                          { return _moreflags.testAny(_endBlock); }

   void setIsDifficultBlock(bool b = true)    { _moreflags.set(_difficultBlock,b); }
   bool isDifficultBlock()                    { return _moreflags.testAny(_difficultBlock); }

   /**
    * Flag functions end
    */

   private:
   void init(TR::TreeTop *entry, TR::TreeTop *exit);

   void uncommonNodesBetweenBlocks(TR::Compilation *, TR::Block *, TR::ResolvedMethodSymbol *methodSymbol = NULL);

   TR::Block * splitBlockAndAddConditional(TR::TreeTop *tree,
                                           TR::TreeTop *compareTree,
                                           TR::CFG *cfg,
                                           bool newBlockShouldExtend);

   void ensureCatchBlockExtensionExists(TR::Compilation *comp);

   struct StandardException
      {
      int32_t     length;
      const char *name;
      uint32_t    exceptions;
      };

   static StandardException _standardExceptions[];

   enum // flag bits for _flags
      {
      _isExtensionOfPreviousBlock           = 0x00000001,
      _isCold                               = 0x00000002,
      _isSuperCold                          = 0x00040000,  // User specified cold/hotness by pragma or @ASSERT for PLX
      // AVAILABLE                          = 0x00000080,
      _doNotProfile                         = 0x00000004,
      _specializedDesyncCatchBlock          = 0x00000008,
      _firstBlockInLoop                     = 0x00000020,
      _branchingBackwards                   = 0x00000040,
      _isSynchronizedHandler                = 0x00000100,
      // Available                          = 0x00000400,
      _isPRECandidate                       = 0x00000800,
      _isAdded                              = 0x00001000,
      _isOSRInduceBlock                     = 0x00002000,
      _isOSRCodeBlock                       = 0x00004000,
      _isOSRCatchBlock                      = 0x00008000,
      _createdAtCodeGen                     = 0x00080000,
      _hasCalls                             = 0x00200000,
      _hasCallToSuperCold                   = 0x00400000,
      _isSpecialized                        = 0x00800000,  // Block has been specialized by loop specializer
      _isLoopInvariantBlock                 = 0x01000000,
      _isCreatedByVersioning                = 0x02000000,
      _isEntryOfShortRunningLoop            = 0x04000000,
      _wasHeaderOfCanonicalizedLoop         = 0x08000000,
      _isLastWarmBlock                      = 0x10000000,
      };

   TR::TreeTop *                         _pEntry;
   TR::TreeTop *                         _pExit;

   TR_BitVector *                        _liveLocals;
   TR_BlockStructure *                   _pStructureOf;

   // TODO: This member is only used during GRA and should be moved out.
   TR_Array<TR::GlobalRegister> *         _globalRegisters;

   InstructionBoundaries                 _instructionBoundaries;
   TR_LinkHead<InstructionBoundaries>    _snippetBoundaries;

   TR::Instruction *                     _firstInstruction;
   TR::Instruction *                     _lastInstruction;

   TR_CatchBlockExtension *              _catchBlockExtension;

   uint16_t                              _unrollFactor;

   // TODO: These members are only used in J9EstimateCodeSize and should be moved out.
   int32_t                               _blockSize;
   int32_t                               _blockBCIndex;
   TR_ResolvedMethod *                   _j9EstimateSizeMethod;

   TR::DebugCounterAggregation *         _debugCounters;

   flags32_t                             _flags;
   flags32_t                             _moreflags;

   };

}


struct BlockMapper : TR_Link<BlockMapper>
   {
   BlockMapper(TR::Block * f, TR::Block * t) : _from(f), _to(t) { }
   TR::Block * _from;
   TR::Block * _to;
   };


class TR_BlockCloner
   {
public:
   TR_ALLOC(TR_Memory::BlockCloner)

   TR_BlockCloner( TR::CFG * cfg, bool cloneBranchesExactly = false, bool cloneSuccessorsOfLastBlock = false)
      : _cfg(cfg),
        _cloneBranchesExactly(cloneBranchesExactly),
        _cloneSuccessorsOfLastBlock(cloneSuccessorsOfLastBlock)
        { }

   TR::Block * cloneBlocks(TR::Block * firstBlock, TR::Block * lastBlock);
   TR::Block * cloneBlocks(TR_LinkHeadAndTail<BlockMapper>* bMap);

   TR::Block * getLastClonedBlock() { return _lastToBlock; }

   TR::Block * getToBlock(TR::Block *);

   void replaceTrees(TR::TreeTop *);

private:
   TR::Node  * cloneNode(TR::Node *);
   TR::Block * doBlockClone(TR_LinkHeadAndTail<BlockMapper>* bMap);

   TR::CFG *                       _cfg;
   TR::Block *                     _lastToBlock;
   TR_LinkHeadAndTail<BlockMapper> _blockMappings;
   TR_NodeMappings                 _nodeMappings;
   bool                            _cloneBranchesExactly;
   bool                            _cloneSuccessorsOfLastBlock;
   };


class TR_ExtendedBlockSuccessorIterator
   {
public:
   TR_ALLOC(TR_Memory::ExtendedBlockSuccessorIterator)

   TR_ExtendedBlockSuccessorIterator(TR::Block * b,  TR::CFG *cfg)
      : _firstBlock(b), _cfg(cfg)
      { }

   TR::Block * getFirst();
   TR::Block * getNext();

private:
   void setCurrentBlock(TR::Block * b);

   TR::Block * _firstBlock;
   TR::Block * _nextBlockInExtendedBlock;
   TR::CFG   * _cfg;
   TR::CFGEdgeList::iterator _iterator;
   TR::CFGEdgeList* _list;
   };

#endif
