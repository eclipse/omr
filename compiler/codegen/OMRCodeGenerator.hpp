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

#ifndef OMR_CODEGENERATOR_INCL
#define OMR_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { class CodeGenerator; }
namespace OMR { typedef OMR::CodeGenerator CodeGeneratorConnector; }
#endif

#include <limits.h>                             // for INT_MAX, etc
#include <stddef.h>                             // for NULL, size_t
#include <stdint.h>                             // for uint8_t, etc
#include <map>
#include "codegen/CodeGenPhase.hpp"             // for CodeGenPhase
#include "codegen/FrontEnd.hpp"                 // for feGetEnv
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"              // for Compilation
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/hashtab.h"                        // for HashTable, etc
#include "env/CompilerEnv.hpp"                  // for TR::Host
#include "env/ObjectModel.hpp"                  // for ObjectModel
#include "env/TRMemory.hpp"                     // for Allocator, etc
#include "env/jittypes.h"
#include "il/DataTypes.hpp"                     // for DataTypes, etc
#include "il/ILOpCodes.hpp"                     // for ILOpCodes
#include "il/ILOps.hpp"                         // for TR::ILOpCode
#include "il/Node.hpp"                          // for vcount_t, etc
#include "infra/Array.hpp"                      // for TR_Array
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/Flags.hpp"                      // for flags32_t, etc
#include "infra/HashTab.hpp"                    // for TR_HashTab, etc
#include "infra/Link.hpp"
#include "infra/List.hpp"                       // for List, etc
#include "infra/TRlist.hpp"
#include "infra/Random.hpp"
#include "infra/Stack.hpp"                      // for TR_Stack
#include "optimizer/Dominators.hpp"             // for TR_Dominators
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"
#include "codegen/StaticRelocation.hpp"

#define OPT_DETAILS_CA "O^O COMPLETE ALIASING: "

#define NEED_CC(n) (n->nodeRequiresConditionCodes())

class TR_BackingStore;
class TR_BitVector;
class TR_GCStackMap;
class TR_InterferenceGraph;
class TR_LiveReference;
class TR_LiveRegisters;
class TR_OSRMethodData;
class TR_PseudoRegister;
class TR_RegisterCandidate;
class TR_RegisterCandidates;
namespace TR { class Relocation; }
namespace TR { class RelocationDebugInfo; }
class TR_ResolvedMethod;
class TR_ScratchRegisterManager;
namespace TR { class GCStackAtlas; }
namespace OMR { class RegisterUsage; }
namespace TR { class AheadOfTimeCompile; }
namespace TR { class AutomaticSymbol; }
namespace TR { class Block; }
namespace TR { class CodeCache; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Linkage; }
namespace TR { class MemoryReference; }
namespace TR { class RealRegister; }
namespace TR { class Recompilation; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterIterator; }
namespace TR { class RegisterPair; }
namespace TR { class Snippet; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class TreeTop; }

typedef TR::SparseBitVector SharedSparseBitVector;

enum TR_SpillKinds // For register pressure simulation
   {
   // Mandatory spill kinds are certain to cause a spill to memory
   //
   TR_hprSpill,       // zGryphon 390 All integer highword regs
   TR_gprSpill,       // All integer regs
   TR_fprSpill,       // All floating-point regs
   TR_vrfSpill,       // All vector regs
   TR_vmThreadSpill,  // The VMThread reg
   TR_numMandatorySpillKinds,

   // Probable spill kinds are likely to cause a spill to memory, but may spill
   // to another free register instead
   //
   TR_volatileSpill   // Regs not preserved across calls
      = TR_numMandatorySpillKinds,
#if defined(TR_TARGET_X86)
   TR_edxSpill,       // Used by multiply and divide
#endif
#if defined(TR_TARGET_S390)
   TR_litPoolSpill,
   TR_staticBaseSpill,
   TR_gpr0Spill,
   TR_gpr1Spill,
   TR_gpr2Spill,
   TR_gpr3Spill,
   TR_gpr4Spill,
   TR_gpr5Spill,
   TR_gpr6Spill,
   TR_gpr7Spill,
   TR_gpr8Spill,
   TR_gpr9Spill,
   TR_gpr10Spill,
   TR_gpr11Spill,
   TR_gpr12Spill,
   TR_gpr13Spill,
   TR_gpr14Spill,
   TR_gpr15Spill,
#endif
   TR_linkageSpill,    // Regs used to pass arguments
   TR_numProbableSpillKinds, // Includes the mandatory spills

   // The other spill kinds are more likely to cause register shuffling
   //
#if defined(TR_TARGET_X86)
   TR_eaxSpill       // Used by multiply and divide
      = TR_numProbableSpillKinds,
   TR_ecxSpill,       // Used by shifts
   TR_numSpillKinds
#else
   TR_numSpillKinds
      = TR_numProbableSpillKinds
#endif
   };

enum TR_NOPKind
   {
   TR_NOPStandard,     // Used to generate a normal NOP instruction
   TR_NOPEndGroup,     // Used to generate a NOP that ends an instruction group (P6 uses this)
   TR_ProbeNOP         // Used in P8 and higher for RI
   };

class TR_ClobberEvalData
   {

   // NOTE:
   // No TR_Memory type defined for this class
   // since original use was to use as a stack-alloc'd object
   // If you're thinking of using it more widely, please
   // update the class definition to include something like:
   //    TR_ALLOC(TR_Memory::ClobberEvalData)

   private:
   flags8_t  _flags;

   public:
   TR_ClobberEvalData()
      { }

   bool isPair()
      {
      return _flags.testAny(Pair);
      }

   bool canClobberLowWord()
      {
      return _flags.testAny(ClobberLowWord);
      };

   bool canClobberHighWord()
      {
      return _flags.testAny(ClobberHighWord);
      }

   void setPair()
      {
      _flags.set(Pair);
      }

   void setClobberLowWord()
      {
      _flags.set(ClobberLowWord);
      }

   void setClobberHighWord()
      {
      _flags.set(ClobberHighWord);
      }

   private:

   enum
      {
      Pair = 0x01,
      ClobberLowWord = 0x02,
      ClobberHighWord = 0x04
      };

   };


TR::Node* generatePoisonNode(TR::Compilation *comp, TR::Block *currentBlock, TR::SymbolReference *liveAutoSymRef);



namespace OMR
{

class /*OMR_EXTENSIBLE*/ CodeGenerator

   {
   private:

   TR::Compilation *_compilation;
   TR_Memory *_trMemory;

   TR::Machine *_machine;

   TR_BitVector *_liveLocals;
   TR::TreeTop *_currentEvaluationTreeTop;
   TR::Block *_currentEvaluationBlock;

   uint32_t _prePrologueSize;

   TR::Instruction *_implicitExceptionPoint;
   bool areMergeableGuards(TR::Instruction *earlierGuard, TR::Instruction *laterGuard);

   protected:

   TR_BitVector *_localsThatAreStored;
   int32_t _numLocalsWhenStoreAnalysisWasDone;
   TR_HashTabInt _uncommmonedNodes;               // uncommoned nodes keyed by the original nodes
   List<TR_Pair<TR::Node, int32_t> > _ialoadUnneeded;

   public:

   TR_ALLOC(TR_Memory::CodeGenerator)

   inline TR::CodeGenerator *self();

   TR_StackMemory trStackMemory();
   TR_Memory *trMemory() { return _trMemory; }
   TR_HeapMemory trHeapMemory() { return _trMemory; }
   TR::Machine *machine() { return _machine; }
   TR::Compilation *comp() { return _compilation; }
   TR_FrontEnd *fe();
   TR_Debug *getDebug();

   void uncommonCallConstNodes();

   OMR_API virtual void preLowerTrees();
   void postLowerTrees() {}

   TR::TreeTop *lowerTree(TR::Node *root, TR::TreeTop *tt);
   void lowerTrees();
   void lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   OMR_API virtual void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   OMR_API virtual void lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);
   OMR_API virtual void lowerTreesPostTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);

   OMR_API virtual void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);
   OMR_API virtual void lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   OMR_API virtual void lowerTreesPropagateBlockToNode(TR::Node *node);

   OMR_API virtual void setUpForInstructionSelection();
   OMR_API virtual void doInstructionSelection();
   OMR_API virtual void createStackAtlas();

   OMR_API virtual void beginInstructionSelection() {}
   OMR_API virtual void endInstructionSelection() {}

   bool use64BitRegsOn32Bit();


   TR::Register *evaluate(TR::Node *node);

   // Main entry point for code generation from IL trees.  Override this function
   // to provide customized code generation functionality.
   //
   bool generateCodeFromIL();

   uint32_t getPrePrologueSize() {return _prePrologueSize;}
   uint32_t setPrePrologueSize(uint32_t s) {return (_prePrologueSize = s);}

   TR_BitVector *getLiveLocals() {return _liveLocals;}
   TR_BitVector *setLiveLocals(TR_BitVector *v) {return (_liveLocals = v);}

   /**
    * @brief Returns the first TR::Instruction in the stream of instructions for
    *        this method.  This instruction's "previous" link should be NULL.
    *
    * @return The first instruction in this method; NULL if not yet set.
    */
   TR::Instruction *getFirstInstruction() {return _firstInstruction;}

   /**
    * @brief Sets the first TR::Instruction in the stream of instructions for
    *        this method.  This instruction's "previous" link should be NULL.
    *
    * @return The instruction being set.
    */
   TR::Instruction *setFirstInstruction(TR::Instruction *fi) {return (_firstInstruction = fi);}

   /**
    * @brief Returns the last TR::Instruction in the stream of instructions for
    *        this method.  This instruction's "next" link should be NULL.
    *
    * @return The last instruction in this method; NULL if not yet set.
    */
   TR::Instruction *getAppendInstruction() {return _appendInstruction;}

   /**
    * @brief Sets the last TR::Instruction in the stream of instructions for
    *        this method.  This instruction's "next" link should be NULL.
    *
    * @return The instruction being set.
    */
   TR::Instruction *setAppendInstruction(TR::Instruction *ai) {return (_appendInstruction = ai);}

   TR::TreeTop *getCurrentEvaluationTreeTop() {return _currentEvaluationTreeTop;}
   TR::TreeTop *setCurrentEvaluationTreeTop(TR::TreeTop *tt) {return (_currentEvaluationTreeTop = tt);}

   TR::Block *getCurrentEvaluationBlock() {return _currentEvaluationBlock;}
   TR::Block *setCurrentEvaluationBlock(TR::Block *tt) {return (_currentEvaluationBlock = tt);}

   TR::Instruction *getImplicitExceptionPoint() {return _implicitExceptionPoint;}
   TR::Instruction *setImplicitExceptionPoint(TR::Instruction *p) {return (_implicitExceptionPoint = p);}

   void setNextAvailableBlockIndex(int32_t blockIndex) {}
   int32_t getNextAvailableBlockIndex() { return -1; }

   OMR_API virtual bool supportsMethodEntryPadding() { return true; }
   OMR_API virtual bool mustGenerateSwitchToInterpreterPrePrologue() { return false; }
   OMR_API virtual bool buildInterpreterEntryPoint() { return false; }
   OMR_API virtual void generateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction) { return; }
   OMR_API virtual bool supportsUnneededLabelRemoval() { return true; }
   OMR_API virtual bool allowSplitWarmAndColdBlocks() { return false; }

   TR_HasRandomGenerator randomizer;

   bool supportsAtomicAdd() {return false;}
   bool hasTMEvaluator()    {return false;}

   // --------------------------------------------------------------------------
   // Infrastructure
   //
   TR_PersistentMemory * trPersistentMemory();

   TR::SymbolReferenceTable * getSymRefTab() { return _symRefTab; }
   TR::SymbolReference * getSymRef(TR_RuntimeHelper h);

   TR::SymbolReferenceTable *symRefTab() { return _symRefTab; }
   TR::Linkage *linkage() {return _bodyLinkage;}


   // --------------------------------------------------------------------------
   // Code Generator Phases
   //
   void generateCode();
   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);  // no virt
   void doBinaryEncoding(); // no virt, no cast
   void doPeephole() { return; } // no virt, no cast, default avail
   bool hasComplexAddressingMode() { return false; } // no virt, default
   void removeUnusedLocals();

   void identifyUnneededByteConvNodes(TR::Node*, TR::TreeTop *, vcount_t, TR::DataType);
   void identifyUnneededByteConvNodes();

   bool afterRA() { return _afterRA; }
   TR::CodeGenPhase& getCodeGeneratorPhase() {return _codeGenPhase;}

   void prepareNodeForInstructionSelection(TR::Node*node);
   void remapGCIndicesInInternalPtrFormat();
   OMR_API virtual void processRelocations();

   void findAndFixCommonedReferences();
   void findCommonedReferences(TR::Node*node, TR::TreeTop *treeTop);
   void processReference(TR::Node*reference, TR::Node*parent, TR::TreeTop *treeTop);
   void spillLiveReferencesToTemps(TR::TreeTop *insertionTree, std::list<TR::SymbolReference*, TR::typed_allocator<TR::SymbolReference*, TR::Allocator> >::iterator firstAvailableSpillTemp);
   void needSpillTemp(TR_LiveReference * cursor, TR::Node*parent, TR::TreeTop *treeTop);

   friend void OMR::CodeGenPhase::performEmitSnippetsPhase(TR::CodeGenerator*, TR::CodeGenPhase *);
   friend void OMR::CodeGenPhase::performCleanUpFlagsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase);

   // --------------------------------------------------------------------------
   // Hardware profiling
   //
   OMR_API virtual void createHWPRecords() {}

   // --------------------------------------------------------------------------
   // Tree evaluation
   //
   static TR_TreeEvaluatorFunctionPointer *getTreeEvaluatorTable() {return _nodeToInstrEvaluators;}

   int32_t getEvaluationPriority(TR::Node*node);
   int32_t whichNodeToEvaluate(TR::Node*first, TR::Node* second);      // Decide which of two nodes should be evaluated first.
   int32_t whichChildToEvaluate(TR::Node*node);    // Decide which child of the given node should be evaluated first.

   // Convert a multiply tree node to a shift if possible.
   // Note that for a negative constant the negation of the shifted value is
   // not done, the caller must insert it after evaluating the shift node.
   // Returns "true" if the conversion was done.
   //
   bool convertMultiplyToShift(TR::Node*node);

   // See if the tree represents an operation on a memory location.
   //
   bool isMemoryUpdate(TR::Node*node);

   // Find the magic values for turning a divide by a constant into multiply and shift
   void compute32BitMagicValues(int32_t  d, int32_t *m, int32_t *s);
   void compute64BitMagicValues(int64_t  d, int64_t *m, int64_t *s);
   uint64_t computeUnsigned64BitMagicValues(uint64_t d, int32_t *s, int32_t* a);

   rcount_t incReferenceCount(TR::Node*node);
   rcount_t decReferenceCount(TR::Node*node);
   rcount_t recursivelyDecReferenceCount(TR::Node*node);
   void evaluateChildrenWithMultipleRefCount(TR::Node*node);

   OMR_API virtual void incRefCountForOpaquePseudoRegister(TR::Node * node, TR::CodeGenerator * cg, TR::Compilation * comp) {}

   void startUsingRegister(TR::Register *reg);
   void stopUsingRegister(TR::Register *reg);

   void setCurrentBlockIndex(int32_t blockIndex) { } // no virt, default, cast
   int32_t getCurrentBlockIndex() { return -1; } // no virt, default

   TR::Instruction *lastInstructionBeforeCurrentEvaluationTreeTop()
      {
      return _lastInstructionBeforeCurrentEvaluationTreeTop;
      }
   void setLastInstructionBeforeCurrentEvaluationTreeTop(TR::Instruction *instr)
      {
      _lastInstructionBeforeCurrentEvaluationTreeTop = instr;
      }

   bool useClobberEvaluate();

   bool canClobberNodesRegister(TR::Node* node, uint16_t count = 1,
                                 TR_ClobberEvalData * data = NULL, bool ignoreRefCount = false);
   bool isRegisterClobberable(TR::Register *reg, uint16_t count);

   // ilgen
   bool ilOpCodeIsSupported(TR::ILOpCodes); // no virt, default, cast



   OMR_API virtual TR::Recompilation *allocateRecompilationInfo() { return NULL; }

   // --------------------------------------------------------------------------
   // Capabilities
   //
   bool supports32bitAiadd() {return true;}  // no virt, default
   bool supportsMergingGuards() {return false;} // no virt, default

   // --------------------------------------------------------------------------
   // Z only
   //

   TR::list<uint8_t*>& getBreakPointList() {return _breakPointList;}
   void addBreakPointAddress(uint8_t *b) {_breakPointList.push_front(b);}

   bool AddArtificiallyInflatedNodeToStack(TR::Node* n);

   // Used to model register liveness without Future Use Count.
   bool isInternalControlFlowReg(TR::Register *reg) {return false;}  // no virt, default
   void startInternalControlFlow(TR::Instruction *instr) {} // no virt, default, cast
   void endInternalControlFlow(TR::Instruction *instr) {} // no virt, default, cast



   // --------------------------------------------------------------------------
   // P only
   //
   intptrj_t hiValue(intptrj_t address); // no virt, 1 impl

   // --------------------------------------------------------------------------
   // Lower trees
   //
   void rematerializeCmpUnderTernary(TR::Node*node);
   bool yankIndexScalingOp() {return false;} // no virt, default

   void cleanupFlags(TR::Node*node);

   bool shouldYankCompressedRefs() { return false; } // no virt, default, cast
   bool materializesHeapBase() { return true; } // no virt, default, cast
   bool canFoldLargeOffsetInAddressing() { return false; } // no virt, default, cast

   void insertDebugCounters();


   // --------------------------------------------------------------------------
   // Instruction selection
   //
   void setUpStackSizeForCallNode(TR::Node * node);

   // --------------------------------------------------------------------------
   // Debug counters
   //
   TR::Instruction *generateDebugCounter(const char *name, int32_t delta = 1, int8_t fidelity = TR::DebugCounter::Undetermined);
   TR::Instruction *generateDebugCounter(const char *name, TR::Register *deltaReg, int8_t fidelity = TR::DebugCounter::Undetermined);
   TR::Instruction *generateDebugCounter(TR::Instruction *cursor, const char *name, int32_t delta = 1, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1);
   TR::Instruction *generateDebugCounter(TR::Instruction *cursor, const char *name, TR::Register *deltaReg, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1);
   TR::Instruction *generateDebugCounter(const char *name, TR::RegisterDependencyConditions &cond, int32_t delta = 1, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1, TR::Instruction *cursor = NULL);
   TR::Instruction *generateDebugCounter(const char *name, TR::Register *deltaReg, TR::RegisterDependencyConditions &cond, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1, TR::Instruction *cursor = NULL);
   TR::Instruction *generateDebugCounter(const char *name, TR_ScratchRegisterManager &srm, int32_t delta = 1, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1, TR::Instruction *cursor = NULL);
   TR::Instruction *generateDebugCounter(const char *name, TR::Register *deltaReg, TR_ScratchRegisterManager &srm, int8_t fidelity = TR::DebugCounter::Undetermined, int32_t staticDelta = 1, TR::Instruction *cursor = NULL);

   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond){ return cursor; } // no virt, default, cast
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond){ return cursor; } // no virt, default, cast
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm){ return cursor; } // no virt, default, cast
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm){ return cursor; } // no virt, default, cast

   // NOT USED?
   bool supportsDebugCounters(TR::DebugCounterInjectionPoint injectionPoint){ return injectionPoint == TR::TR_BeforeCodegen; } // no virt, default

   void incrementEventCounter(TR::Node *node, TR::SymbolReference *symRef, TR::CodeGenerator *cg) { TR_ASSERT(0,"not implemented\n");} // no virt, default

   // --------------------------------------------------------------------------
   // Linkage
   //
   void initializeLinkage(); // no virt, default, cast
   OMR_API virtual TR::Linkage *createLinkage(TR_LinkageConventions lc); // no virt, default, cast
   OMR_API virtual TR::Linkage *createLinkageForCompilation();

   TR::Linkage *getLinkage() {return _bodyLinkage;}
   TR::Linkage *setLinkage(TR::Linkage * l) {return (_bodyLinkage = l);}

   TR::Linkage *getLinkage(TR_LinkageConventions lc);
   TR::Linkage *setLinkage(TR_LinkageConventions lc, TR::Linkage * l) {return _linkages[lc] = l;}

   // --------------------------------------------------------------------------
   // Optimizer, code generator capabilities
   //
   int32_t getPreferredLoopUnrollFactor() {return -1;} // no virt, default

   /**
    * @brief Answers whether the provided recognized method should be inlined by an
    *        inliner optimization.
    * @param method : the recognized method to consider
    * @return true if inlining should be suppressed; false otherwise
    */
   OMR_API virtual bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method) {return false;}

   // --------------------------------------------------------------------------
   // Optimizer, not code generator
   //
   bool getGRACompleted() { return _flags4.testAny(GRACompleted); }
   void setGRACompleted() { _flags4.set(GRACompleted); }

   bool getSupportsProfiledInlining() { return _flags4.testAny(SupportsProfiledInlining);}
   void setSupportsProfiledInlining() { _flags4.set(SupportsProfiledInlining);}
   bool supportsInliningOfIsInstance() {return false;} // no virt, default
   bool supportsPassThroughCopyToNewVirtualRegister() { return false; } // no virt, default

   uint8_t getSizeOfCombinedBuffer() {return 0;} // no virt, default

   bool doRematerialization() {return false;} // no virt, default

   // --------------------------------------------------------------------------
   // Architecture, not code generator
   //
   int16_t getMinShortForLongCompareNarrower() { return SHRT_MIN; } // no virt, default
   int8_t getMinByteForLongCompareNarrower() { return SCHAR_MIN; } // no virt, default

   bool branchesAreExpensive() { return true; } // no virt, default
   bool opCodeIsNoOp(TR::ILOpCode &opCode); // no virt, 1 impl
   bool opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode) {return false;} // no virt

   bool supportsSinglePrecisionSQRT() {return false;} // no virt
   bool supportsFusedMultiplyAdd() {return false;} // no virt
   bool supportsNegativeFusedMultiplyAdd() {return false;} // no virt

   bool supportsComplexAddressing() {return false;} // no virt
   bool canBeAffectedByStoreTagStalls() { return false; } // no virt, default

   bool isMaterialized(TR::Node *); // no virt, cast
   bool shouldValueBeInACommonedNode(int64_t) { return false; } // no virt, cast
   bool materializesLargeConstants() { return false; }

   bool canUseImmedInstruction(int64_t v) {return false;} // no virt
   bool needsNormalizationBeforeShifts() { return false; } // no virt, cast

   uint32_t getNumberBytesReadInaccessible() { return _numberBytesReadInaccessible; }
   uint32_t getNumberBytesWriteInaccessible() { return _numberBytesWriteInaccessible; }

   bool codegenSupportsUnsignedIntegerDivide() {return false;} // no virt
   bool mulDecompositionCostIsJustified(int numOfOperations, char bitPosition[], char operationType[], int64_t value); // no virt

   bool codegenSupportsLoadlessBNDCheck() {return false;} // no virt, cast

   // called to determine if multiply decomposition exists in platform codegen so that codegen sequences are used
   // instead of the IL transformed multiplies
   bool codegenMulDecomposition(int64_t multiplier) {return false;} // no virt

   // --------------------------------------------------------------------------
   // FrontEnd, not code generator
   //
   bool getSupportsNewObjectAlignment() { return false; } // no virt
   bool getSupportsTenuredObjectAlignment() { return false; } // no virt
   bool isObjectOfSizeWorthAligning(uint32_t size) { return false; } // no virt

   // J9
   int32_t getInternalPtrMapBit() { return 31;} // no virt

   uint32_t getMaxObjectSizeGuaranteedNotToOverflow() { return _maxObjectSizeGuaranteedNotToOverflow; }

   // --------------------------------------------------------------------------
   // OSR, not code generator
   //
   void addToOSRTable(TR::Instruction *);
   void addToOSRTable(int32_t instructionPC, TR_ByteCodeInfo &bcInfo);

   int genStoreForSymListArray(TR_Array<List<TR::SymbolReference> >* symListArray,
      TR_OSRMethodData* osrMethodData, TR::TreeTop*& insertionPoint, TR::Node* callNode,
      TR::Node* osrBufferNode, TR::Node* osrScratchBufferNode, TR::Node* osrFrameIndex,
      int32_t& scratchBufferOffset);

   void eliminateLoadsOfLocalsThatAreNotStored(TR::Node *node, int32_t childNum);

   // --------------------------------------------------------------------------
   // FE capability, not code generator
   //
   bool internalPointerSupportImplemented() {return false;} // no virt, cast
   bool supportsInternalPointers();

   // --------------------------------------------------------------------------
   // Behaviour on a particular arch, not code generator
   //
   bool supportsLongRegAllocation() {return false;}  // no virt

   // --------------------------------------------------------------------------
   // GC
   //
   TR::GCStackAtlas *getStackAtlas() {return _stackAtlas;}
   TR::GCStackAtlas *setStackAtlas(TR::GCStackAtlas *p) {return (_stackAtlas = p);}
   TR_GCStackMap *getMethodStackMap() {return _methodStackMap;}
   TR_GCStackMap *setMethodStackMap(TR_GCStackMap *m) {return (_methodStackMap = m);}
   void addToAtlas(TR::Instruction *);
   void buildGCMapsForInstructionAndSnippet(TR::Instruction *instr);
   TR_GCStackMap *buildGCMapForInstruction(TR::Instruction *instr);
   void buildRegisterMapForInstruction(TR_GCStackMap *map);
   // IA32 only?
   uint32_t getRegisterMapInfoBitsMask() {return 0;} // no virt, cast

   // --------------------------------------------------------------------------
   // Shrink wrapping
   //
   TR_BitVector *getPreservedRegsInPrologue() {return _preservedRegsInPrologue;}
   TR_BitVector *setPreservedRegsInPrologue(TR_BitVector *v) {return (_preservedRegsInPrologue = v);}

   int32_t getLowestSavedRegister() {return _lowestSavedReg;}
   void setLowestSavedRegister(int32_t v) {_lowestSavedReg = v;}

   bool processInstruction(TR::Instruction *instr, TR_BitVector **registerUsageInfo, int32_t &blockNum, int32_t &isFence, bool traceIt) {return false;} // no virt, cast
   uint32_t isPreservedRegister(int32_t regIndex) { return 0; } // no virt, cast
   bool isReturnInstruction(TR::Instruction *instr) { return false; } // no virt, cast
   bool isBranchInstruction(TR::Instruction *instr) { return false; } // no virt, cast
   int32_t isFenceInstruction(TR::Instruction *instr) { return false; } // no virt
   bool isAlignmentInstruction(TR::Instruction *instr) { return false; } // no virt
   bool isLabelInstruction(TR::Instruction *instr) { return false; } // no virt
   TR::Instruction *splitEdge(TR::Instruction *cursor, bool isFallThrough, bool needsJump, TR::Instruction *newSplitLabel, TR::list<TR::Instruction*> *jmpInstrs, bool firstJump = false) { return NULL; } // no virt
   TR::Instruction *splitBlockEntry(TR::Instruction *instr) { return NULL; } // no virt
   int32_t computeRegisterSaveDescription(TR_BitVector *regs, bool populateInfo = false) { return 0; } // no virt
   void processIncomingParameterUsage(TR_BitVector **registerUsageInfo, int32_t blockNum) { return; } // no virt
   void updateSnippetMapWithRSD(TR::Instruction *cur, int32_t rsd) { return; } // no virt
   bool isTargetSnippetOrOutOfLine(TR::Instruction *instr, TR::Instruction **start, TR::Instruction **end) { return false; }

   // --------------------------------------------------------------------------
   // Method frame building
   //
   uint32_t getLargestOutgoingArgSize()           {return _largestOutgoingArgSize;}
   uint32_t setLargestOutgoingArgSize(uint32_t s) {return (_largestOutgoingArgSize = s);}

   int32_t getFrameSizeInBytes() {return _frameSizeInBytes;}
   int32_t setFrameSizeInBytes(int32_t fs) {return (_frameSizeInBytes = fs);}

   int32_t getRegisterSaveDescription() {return _registerSaveDescription;}
   int32_t setRegisterSaveDescription(int32_t rsd) {return (_registerSaveDescription = rsd);}

   TR_InterferenceGraph *getLocalsIG() {return _localsIG;}
   TR_InterferenceGraph *setLocalsIG(TR_InterferenceGraph *ig) {return (_localsIG = ig);}

   bool isLeafMethod()      { return _flags1.testAny(IsLeafMethod); }
   void setIsLeafMethod()   { _flags1.set(IsLeafMethod); }
   void resetIsLeafMethod() { _flags1.reset(IsLeafMethod); }

   // --------------------------------------------------------------------------
   // Binary encoding code cache
   //
   uint32_t getEstimatedWarmLength()           {return _estimatedCodeLength;} // DEPRECATED
   uint32_t setEstimatedWarmLength(uint32_t l) {return (_estimatedCodeLength = l);} // DEPRECATED

   uint32_t getEstimatedCodeLength()           {return _estimatedCodeLength;}
   uint32_t setEstimatedCodeLength(uint32_t l) {return (_estimatedCodeLength = l);}

   uint8_t *getBinaryBufferStart()           {return _binaryBufferStart;}
   uint8_t *setBinaryBufferStart(uint8_t *b) {return (_binaryBufferStart = b);}

   uint8_t *getCodeStart();
   uint8_t *getCodeEnd()                  {return _binaryBufferCursor;}
   uint32_t getCodeLength();

   uint8_t *getBinaryBufferCursor() {return _binaryBufferCursor;}
   uint8_t *setBinaryBufferCursor(uint8_t *b) { return (_binaryBufferCursor = b); }

   uint8_t *alignBinaryBufferCursor();

   uint32_t getBinaryBufferLength() {return (uint32_t)(_binaryBufferCursor - _binaryBufferStart - _jitMethodEntryPaddingSize);} // cast explicitly

   int32_t getEstimatedSnippetStart() {return _estimatedSnippetStart;}
   int32_t setEstimatedSnippetStart(int32_t s) {return (_estimatedSnippetStart = s);}

   int32_t getAccumulatedInstructionLengthError() {return _accumulatedInstructionLengthError;}
   int32_t setAccumulatedInstructionLengthError(int32_t e) {return (_accumulatedInstructionLengthError = e);}
   int32_t addAccumulatedInstructionLengthError(int32_t e) {return (_accumulatedInstructionLengthError += e);}

   uint32_t getPreJitMethodEntrySize() {return _preJitMethodEntrySize;}
   uint32_t setPreJitMethodEntrySize(uint32_t s) {return (_preJitMethodEntrySize = s);}

   uint32_t getJitMethodEntryPaddingSize() {return _jitMethodEntryPaddingSize;}
   uint32_t setJitMethodEntryPaddingSize(uint32_t s) {return (_jitMethodEntryPaddingSize = s);}

   // --------------------------------------------------------------------------
   // Code cache
   //
   TR::CodeCache * getCodeCache() { return _codeCache; }
   void  setCodeCache(TR::CodeCache * codeCache) { _codeCache = codeCache; }
   void  reserveCodeCache();
   uint8_t * allocateCodeMemory(uint32_t size, bool isCold, bool isMethodHeaderNeeded=true);
   uint8_t * allocateCodeMemory(uint32_t warmSize, uint32_t coldSize, uint8_t **coldCode, bool isMethodHeaderNeeded=true);
   void  resizeCodeMemory();
   OMR_API virtual void  registerAssumptions() {}

   static void syncCode(uint8_t *codeStart, uint32_t codeSize);

   void commitToCodeCache() { _committedToCodeCache = true; }
   bool committedToCodeCache() { return _committedToCodeCache; }

   // --------------------------------------------------------------------------
   // Load extensions (Z)

   TR::SparseBitVector & getExtendedToInt64GlobalRegisters()  { return _extendedToInt64GlobalRegisters; }

   // --------------------------------------------------------------------------
   // Live registers
   //
   void checkForLiveRegisters(TR_LiveRegisters *);
   TR_LiveRegisters *getLiveRegisters(TR_RegisterKinds rk) {return _liveRegisters[rk];}
   TR_LiveRegisters *setLiveRegisters(TR_LiveRegisters *p, TR_RegisterKinds rk) {return (_liveRegisters[rk] = p);}

   void genLiveRealRegisters(TR_RegisterKinds rk, TR_RegisterMask r) {_liveRealRegisters[rk] |= r;}
   void killLiveRealRegisters(TR_RegisterKinds rk, TR_RegisterMask r) {_liveRealRegisters[rk] &= ~r;}
   TR_RegisterMask getLiveRealRegisters(TR_RegisterKinds rk) {return _liveRealRegisters[rk];}
   void resetLiveRealRegisters(TR_RegisterKinds rk) {_liveRealRegisters[rk] = 0;}

   uint8_t getSupportedLiveRegisterKinds() {return _supportedLiveRegisterKinds;}
   void addSupportedLiveRegisterKind(TR_RegisterKinds rk) {_supportedLiveRegisterKinds |= (uint8_t)(1<<rk);}

   // --------------------------------------------------------------------------
   // VMThread (shouldn't be common codegen)
   //
   TR::Register *getVMThreadRegister() {return _vmThreadRegister;}
   TR::Register *setVMThreadRegister(TR::Register *vmtr) {return (_vmThreadRegister = vmtr);}

   TR::RealRegister *getRealVMThreadRegister() {return _realVMThreadRegister;}
   void setRealVMThreadRegister(TR::RealRegister *defvmtr) {_realVMThreadRegister = defvmtr;}

   TR::Instruction *getVMThreadSpillInstruction() {return _vmThreadSpillInstr;}
   void setVMThreadSpillInstruction(TR::Instruction *i);

   // --------------------------------------------------------------------------
   // GRA
   //
   void addSymbolAndDataTypeToMap(TR::Symbol *symbol, TR::DataType dt);
   TR::DataType getDataTypeFromSymbolMap(TR::Symbol *symbol);

   bool prepareForGRA(); // no virt, cast

   uint32_t getGlobalRegister(TR_GlobalRegisterNumber n) {return _globalRegisterTable[n];}
   uint32_t *setGlobalRegisterTable(uint32_t *p) {return (_globalRegisterTable = p);}

   TR_GlobalRegisterNumber getGlobalRegisterNumber(uint32_t realReg) { return -1; } // no virt, cast

   TR_GlobalRegisterNumber getFirstGlobalGPR() {return 0;}
   TR_GlobalRegisterNumber getLastGlobalGPR()  {return _lastGlobalGPR;}
   TR_GlobalRegisterNumber setLastGlobalGPR(TR_GlobalRegisterNumber n) {return (_lastGlobalGPR = n);}

   TR_GlobalRegisterNumber getFirstGlobalHPR() {return _firstGlobalHPR;}
   TR_GlobalRegisterNumber setFirstGlobalHPR(TR_GlobalRegisterNumber n) {return (_firstGlobalHPR = n);}
   TR_GlobalRegisterNumber getLastGlobalHPR() {return _lastGlobalHPR;}
   TR_GlobalRegisterNumber setLastGlobalHPR(TR_GlobalRegisterNumber n) {return (_lastGlobalHPR = n);}

   TR_GlobalRegisterNumber getGlobalHPRFromGPR (TR_GlobalRegisterNumber n) {return 0;} // no virt, cast
   TR_GlobalRegisterNumber getGlobalGPRFromHPR (TR_GlobalRegisterNumber n) {return 0;} // no virt

   TR_GlobalRegisterNumber getFirstGlobalFPR() {return _lastGlobalGPR + 1;}
   TR_GlobalRegisterNumber setFirstGlobalFPR(TR_GlobalRegisterNumber n) {return (_firstGlobalFPR = n);}
   TR_GlobalRegisterNumber getLastGlobalFPR() {return _lastGlobalFPR;}
   TR_GlobalRegisterNumber setLastGlobalFPR(TR_GlobalRegisterNumber n)  {return (_lastGlobalFPR = n);}

   TR_GlobalRegisterNumber getFirstOverlappedGlobalFPR()                          { return _firstOverlappedGlobalFPR    ;}
   TR_GlobalRegisterNumber setFirstOverlappedGlobalFPR(TR_GlobalRegisterNumber n) { return _firstOverlappedGlobalFPR = n;}
   TR_GlobalRegisterNumber getLastOverlappedGlobalFPR()                           { return _lastOverlappedGlobalFPR     ;}
   TR_GlobalRegisterNumber setLastOverlappedGlobalFPR(TR_GlobalRegisterNumber n)  { return _lastOverlappedGlobalFPR = n ;}

   TR_GlobalRegisterNumber getFirstGlobalAR() {return _firstGlobalAR;}
   TR_GlobalRegisterNumber setFirstGlobalAR(TR_GlobalRegisterNumber n) {return (_firstGlobalAR = n);}
   TR_GlobalRegisterNumber getLastGlobalAR() {return _lastGlobalAR;}
   TR_GlobalRegisterNumber setLastGlobalAR(TR_GlobalRegisterNumber n) {return (_lastGlobalAR = n);}

   TR_GlobalRegisterNumber getFirstGlobalVRF() {return _firstGlobalVRF;}
   TR_GlobalRegisterNumber setFirstGlobalVRF(TR_GlobalRegisterNumber n) {return (_firstGlobalVRF = n);}
   TR_GlobalRegisterNumber getLastGlobalVRF() {return _lastGlobalVRF;}
   TR_GlobalRegisterNumber setLastGlobalVRF(TR_GlobalRegisterNumber n) {return (_lastGlobalVRF= n);}

   TR_GlobalRegisterNumber getFirstOverlappedGlobalVRF()                          {return _firstOverlappedGlobalVRF     ;}
   TR_GlobalRegisterNumber setFirstOverlappedGlobalVRF(TR_GlobalRegisterNumber n) {return _firstOverlappedGlobalVRF = n ;}
   TR_GlobalRegisterNumber getLastOverlappedGlobalVRF()                           {return _lastOverlappedGlobalVRF      ;}
   TR_GlobalRegisterNumber setLastOverlappedGlobalVRF(TR_GlobalRegisterNumber n)  {return _lastOverlappedGlobalVRF = n  ;}

   bool hasGlobalVRF() { return _firstGlobalVRF != -1 && _lastGlobalVRF != -1; }

   void setLast8BitGlobalGPR(TR_GlobalRegisterNumber n) { _last8BitGlobalGPR = n;}

   uint16_t getNumberOfGlobalRegisters();

#ifdef TR_HOST_S390
   uint16_t getNumberOfGlobalGPRs();
#else
   uint16_t getNumberOfGlobalGPRs() {return _lastGlobalGPR + 1;}
#endif
   uint16_t getNumberOfGlobalFPRs() {return _lastGlobalFPR - _lastGlobalGPR;}
   uint16_t getNumberOfGlobalVRFs() {return _lastGlobalVRF - _firstGlobalVRF;}

   uint8_t getGlobalGPRPartitionLimit() {return _globalGPRPartitionLimit;}
   uint8_t setGlobalGPRPartitionLimit(uint8_t l) {return (_globalGPRPartitionLimit = l);}

   uint8_t getGlobalFPRPartitionLimit() {return _globalFPRPartitionLimit;}
   uint8_t setGlobalFPRPartitionLimit(uint8_t l) {return (_globalFPRPartitionLimit = l);}

   bool isGlobalGPR(TR_GlobalRegisterNumber n) {return n <= _lastGlobalGPR;}
   bool isGlobalHPR(TR_GlobalRegisterNumber n) {return (n >= _firstGlobalHPR && n <= _lastGlobalHPR);}

   bool isAliasedGRN(TR_GlobalRegisterNumber n);
   TR_GlobalRegisterNumber getOverlappedAliasForGRN(TR_GlobalRegisterNumber n);

   void setOverlapOffsetBetweenAliasedGRNs(TR_GlobalRegisterNumber n)
      {
      TR_ASSERT(n >= 0, "Offset for aliased global register numbers must be positive. Currently: %d", n);
      _overlapOffsetBetweenFPRandVRFgrns = n;
      }

   TR_GlobalRegisterNumber getOverlapOffsetBetweenAliasedGRNs()
      {
      return _overlapOffsetBetweenFPRandVRFgrns;
      }

   bool isGlobalVRF(TR_GlobalRegisterNumber n);
   bool isGlobalFPR(TR_GlobalRegisterNumber n);

   bool is8BitGlobalGPR(TR_GlobalRegisterNumber n) {return n <= _last8BitGlobalGPR;}

   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type){ return -1; } // no virt, cast
   TR_BitVector *getGlobalGPRsPreservedAcrossCalls(){ return NULL; } // no virt, cast
   TR_BitVector *getGlobalFPRsPreservedAcrossCalls(){ return NULL; } // no virt, cast

   int32_t getFirstBit(TR_BitVector &bv);
   TR_GlobalRegisterNumber pickRegister(TR_RegisterCandidate *, TR::Block * *, TR_BitVector & availableRegisters, TR_GlobalRegisterNumber & highRegisterNumber, TR_LinkHead<TR_RegisterCandidate> *candidates); // no virt
   TR_RegisterCandidate *findCoalescenceForRegisterCopy(TR::Node *node, TR_RegisterCandidate *rc, bool *isUnpreferred);
   TR_GlobalRegisterNumber findCoalescenceRegisterForParameter(TR::Node *callNode, TR_RegisterCandidate *rc, uint32_t childIndex, bool *isUnpreferred);
   TR_RegisterCandidate *findUsedCandidate(TR::Node *node, TR_RegisterCandidate *rc, TR_BitVector *visitedNodes);

   bool allowGlobalRegisterAcrossBranch(TR_RegisterCandidate *, TR::Node * branchNode); // no virt
   void removeUnavailableRegisters(TR_RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters) {} // no virt
   void setUnavailableRegistersUsage(TR_Array<TR_BitVector>  & liveOnEntryUsage, TR_Array<TR_BitVector>   & liveOnExitUsage) {} // no virt

   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *) { return INT_MAX; } // no virt
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *) { return INT_MAX; } // no virt
   int32_t getMaximumNumberOfVRFsAllowedAcrossEdge(TR::Node *) { return INT_MAX; } // no virt
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *block); // no virt
   int32_t getMaximumNumbersOfAssignableGPRs() { return INT_MAX; } // no virt, cast
   int32_t getMaximumNumbersOfAssignableFPRs() { return INT_MAX; } // no virt, cast
   int32_t getMaximumNumbersOfAssignableVRs()  { return INT_MAX; } // no virt, cast
   virtual bool willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp){ return true;}
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber, TR::DataType) { return true; } // no virt

   bool areAssignableGPRsScarce(); // no virt, 1 impl

   TR_Array<TR::Register *>& getRegisterArray() {return _registerArray;}

   bool needToAvoidCommoningInGRA() {return false;} // no virt

   bool considerTypeForGRA(TR::Node *node) {return true;} // no virt
   bool considerTypeForGRA(TR::DataType dt) {return true;} // no virt
   bool considerTypeForGRA(TR::SymbolReference *symRef) {return true;} // no virt

   void enableLiteralPoolRegisterForGRA () {} // no virt
   bool excludeInvariantsFromGRAEnabled() { return false; } // no virt

   TR_BitVector *getBlocksWithCalls();

   // --------------------------------------------------------------------------
   // Debug
   //
   TR::RegisterIterator *getGPRegisterIterator() {return _gpRegisterIterator;}
   TR::RegisterIterator *setGPRegisterIterator(TR::RegisterIterator *iter) {return (_gpRegisterIterator = iter);}

   TR::RegisterIterator *getFPRegisterIterator() {return _fpRegisterIterator;}
   TR::RegisterIterator *setFPRegisterIterator(TR::RegisterIterator *iter) {return (_fpRegisterIterator = iter);}

   // X86 only
   uint32_t estimateBinaryLength(TR::MemoryReference *) { return 0; } // no virt

#ifdef DEBUG
   static void shutdown(TR_FrontEnd *fe, TR::FILE *logFile);
   static void dumpSpillStats(TR_FrontEnd *fe);
   static void incNumSpilledRegisters()        {_totalNumSpilledRegisters++;}
   static void incNumRematerializedConstants() {_totalNumRematerializedConstants++;}
   static void incNumRematerializedLocals()    {_totalNumRematerializedLocals++;}
   static void incNumRematerializedStatics()   {_totalNumRematerializedStatics++;}
   static void incNumRematerializedIndirects() {_totalNumRematerializedIndirects++;}
   static void incNumRematerializedAddresses() {_totalNumRematerializedAddresses++;}
   static void incNumRematerializedXMMRs()     {_totalNumRematerializedXMMRs++;}
#endif

   void dumpDataSnippets(TR::FILE *outFile) {}
   void dumpTargetAddressSnippets(TR::FILE *outFile) {}

   // --------------------------------------------------------------------------
   // Register assignment tracing
   //
   bool getTraceRAOption(uint32_t mask);
   void traceRAInstruction(TR::Instruction *instr);
   void tracePreRAInstruction(TR::Instruction *instr);
   void tracePostRAInstruction(TR::Instruction *instr);
   void traceRegAssigned(TR::Register *virtReg, TR::Register *realReg);
   void traceRegAssigned(TR::Register *virtReg, TR::Register *realReg, TR_RegisterAssignmentFlags flags);
   void traceRegFreed(TR::Register *virtReg, TR::Register *realReg);
   void traceRegInterference(TR::Register *virtReg, TR::Register *interferingVirtual, int32_t distance);
   void traceRegWeight(TR::Register *realReg, uint32_t weight);
   void traceRegisterAssignment(const char *format, ...);

   void setRegisterAssignmentFlag(TR_RegisterAssignmentFlagBits b) {_regAssignFlags.set(b);}
   void resetRegisterAssignmentFlag(TR_RegisterAssignmentFlagBits b) {_regAssignFlags.set(b);}
   bool testAnyRegisterAssignmentFlags(TR_RegisterAssignmentFlagBits b) {return _regAssignFlags.testAny(b);}
   void clearRegisterAssignmentFlags() {_regAssignFlags.clear();}

   // --------------------------------------------------------------------------
   // Spill
   //
   TR::list<TR_BackingStore*>& getSpill4FreeList() {return _spill4FreeList;}
   TR::list<TR_BackingStore*>& getSpill8FreeList() {return _spill8FreeList;}
   TR::list<TR_BackingStore*>& getSpill16FreeList() {return _spill16FreeList;}
   TR::list<TR_BackingStore*>& getInternalPointerSpillFreeList() {return _internalPointerSpillFreeList;}
   TR::list<TR_BackingStore*>& getCollectedSpillList() {return _collectedSpillList;}
   TR::list<TR_BackingStore*>& getAllSpillList() {return _allSpillList;}

   TR::list<TR::Register*> *getSpilledRegisterList() {return _spilledRegisterList;}
   TR::list<TR::Register*> *setSpilledRegisterList(TR::list<TR::Register*> *r) {return _spilledRegisterList = r;}
   TR::list<TR::Register*> *getFirstTimeLiveOOLRegisterList() {return _firstTimeLiveOOLRegisterList;}
   TR::list<TR::Register*> *setFirstTimeLiveOOLRegisterList(TR::list<TR::Register*> *r) {return _firstTimeLiveOOLRegisterList = r;}

   TR_BackingStore * allocateVMThreadSpill();
   TR_BackingStore *allocateSpill(bool containsCollectedReference, int32_t *offset, bool reuse=true);
   TR_BackingStore *allocateSpill(int32_t size, bool containsCollectedReference, int32_t *offset, bool reuse=true);
   TR_BackingStore *allocateInternalPointerSpill(TR::AutomaticSymbol *pinningArrayPointer);
   void freeSpill(TR_BackingStore *spill, int32_t size, int32_t offset);
   void jettisonAllSpills(); // Call between register assigner passes

   // --------------------------------------------------------------------------
   // Out of line register allocation (x86)
   //
   TR::list<OMR::RegisterUsage*> *getReferencedRegisterList() {return _referencedRegistersList;}
   TR::list<OMR::RegisterUsage*> *setReferencedRegisterList(TR::list<OMR::RegisterUsage*> *r) {return _referencedRegistersList = r;}

   void recordSingleRegisterUse(TR::Register *reg);
   void startRecordingRegisterUsage();
   TR::list<OMR::RegisterUsage*> *stopRecordingRegisterUsage();

   int32_t setCurrentPathDepth(int32_t d) { return _currentPathDepth = d; }
   int32_t getCurrentPathDepth() { return _currentPathDepth; }

   // --------------------------------------------------------------------------
   // Virtual register allocation
   //
   TR::Register * allocateRegister(TR_RegisterKinds rk = TR_GPR);
   TR::Register * allocateCollectedReferenceRegister();
   TR::Register * allocateSinglePrecisionRegister(TR_RegisterKinds rk = TR_FPR);
   TR::Register * allocate64bitRegister();

   TR::RegisterPair * allocate64bitRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);
   TR::RegisterPair * allocateRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);
   TR::RegisterPair * allocateSinglePrecisionRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);
   TR::RegisterPair * allocateFloatingPointRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);



   TR::SymbolReference * allocateLocalTemp(TR::DataType dt = TR::Int32, bool isInternalPointer = false);

   // --------------------------------------------------------------------------
   // Relocations
   //
   TR::list<TR::Relocation*>& getRelocationList() {return _relocationList;}
   TR::list<TR::Relocation*>& getAOTRelocationList() {return _aotRelocationList;}
   TR::list<TR::StaticRelocation>& getStaticRelocations() { return _staticRelocationList; }

   void addRelocation(TR::Relocation *r);
   void addAOTRelocation(TR::Relocation *r, const char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node);
   void addAOTRelocation(TR::Relocation *r, TR::RelocationDebugInfo *info);
   void addStaticRelocation(const TR::StaticRelocation &relocation);

   OMR_API virtual void addProjectSpecializedRelocation(uint8_t *location,
                                          uint8_t *target,
                                          uint8_t *target2,
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}
   OMR_API virtual void addProjectSpecializedPairRelocation(uint8_t *location1,
                                          uint8_t *location2,
                                          uint8_t *target,
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}
   OMR_API virtual void addProjectSpecializedRelocation(TR::Instruction *instr,
                                          uint8_t *target,
                                          uint8_t *target2,
                                          TR_ExternalRelocationTargetKind kind,
                                          char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}

   void apply8BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label); // no virt
   void apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, bool isCheckDisp = true); // no virt
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label); // no virt
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label,int8_t d, bool isInstrOffset = false); // no virt
   void apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *); // no virt
   void apply16BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *, TR::LabelSymbol *, int32_t); // no virt
   void apply32BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *, TR::LabelSymbol *, int32_t);  // no virt
   void apply64BitLoadLabelRelativeRelocation(TR::Instruction *, TR::LabelSymbol *); // no virt
   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *); // no virt
   void apply32BitLabelTableRelocation(int32_t * cursor, TR::LabelSymbol *); // no virt

   TR::list<TR_Pair<TR_ResolvedMethod,TR::Instruction> *> &getJNICallSites() { return _jniCallSites; }  // registerAssumptions()

   bool needClassAndMethodPointerRelocations() { return false; }
   OMR_API virtual bool needRelocationsForStatics() { return false; }

   // --------------------------------------------------------------------------
   // Snippets
   //
   int32_t setEstimatedLocationsForSnippetLabels(int32_t estimatedSnippetStart);
   TR::list<TR::Snippet*>& getSnippetList() {return _snippetList;}
   void addSnippet(TR::Snippet *s);

   // Local snippet sharing facility: most RISC platforms can make use of it. The platform
   // specific code generators should override isSnippetMatched if they choose to use it.
   TR::LabelSymbol * lookUpSnippet(int32_t snippetKind, TR::SymbolReference *symRef);
   bool isSnippetMatched(TR::Snippet *snippet, int32_t snippetKind, TR::SymbolReference *symRef) {return false;} // no virt, cast

   // called to emit any constant data snippets.  The platform specific code generators
   // should override these methods if they use constant data snippets.
   //
   void emitDataSnippets() {}
   bool hasDataSnippets() {return false;} // no virt, cast
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart) {return 0;}

   // called to emit any target address snippets.  The platform specific code generators
   // should override these methods if they use target address snippets.
   //
   void emitTargetAddressSnippets() {}
   bool hasTargetAddressSnippets() {return false;} // no virt, cast
   int32_t setEstimatedLocationsForTargetAddressSnippetLabels(int32_t estimatedSnippetStart) {return 0;}

   // --------------------------------------------------------------------------
   // Register pressure
   //
   void estimateRegisterPressure(TR::Node *node, int32_t &registerPressure, int32_t &maxRegisterPressure, int32_t maxRegisters, TR_BitVector *valuesInGlobalRegs, bool isCold, vcount_t visitCount, TR::SymbolReference *symRef, bool &symRefIsLive, bool checkForIMuls, bool &vmThreadUsed);
   int32_t estimateRegisterPressure(TR::Block *block, vcount_t visitCount, int32_t maxStaticFrequency, int32_t maxFrequency, bool &vmThreadUsed, int32_t numGlobalRegs = 0, TR_BitVector *valuesInGlobalRegs = NULL, TR::SymbolReference *symRef = NULL, bool checkForIMuls = false);

   #include "codegen/RegisterPressureSimulatorInner.hpp"

   // --------------------------------------------------------------------------
   // Internal control flow
   //
   // Used to track the internal control flow depth level while compiling.
   // Updated the CodeGenerator.hpp in x86, s390, and PPC so they reference this common code.
   int32_t _internalControlFlowNestingDepth;
   int32_t _internalControlFlowSafeNestingDepth;
   TR::Instruction *_instructionAtEndInternalControlFlow;
   int32_t internalControlFlowNestingDepth() {return _internalControlFlowNestingDepth;}
   int32_t internalControlFlowSafeNestingDepth() { return _internalControlFlowSafeNestingDepth; }
   void incInternalControlFlowNestingDepth() {_internalControlFlowNestingDepth++;}
   void decInternalControlFlowNestingDepth() {_internalControlFlowNestingDepth--;}
   bool insideInternalControlFlow() {return (_internalControlFlowNestingDepth > _internalControlFlowSafeNestingDepth);}
   void setInternalControlFlowNestingDepth(int32_t depth) { _internalControlFlowNestingDepth = depth; }
   void setInternalControlFlowSafeNestingDepth(int32_t safeDepth) { _internalControlFlowSafeNestingDepth = safeDepth; }
   TR::Instruction* getInstructionAtEndInternalControlFlow() { return _instructionAtEndInternalControlFlow; }

   // --------------------------------------------------------------------------
   // Non-linear register assigner

   bool getUseNonLinearRegisterAssigner() { return _enabledFlags.testAny(UseNonLinearRegisterAssigner); }
   void setUseNonLinearRegisterAssigner() { _enabledFlags.set(UseNonLinearRegisterAssigner); }

   bool isFreeSpillListLocked() { return _enabledFlags.testAny(LockFreeSpillList); }
   void lockFreeSpillList() {_enabledFlags.set(LockFreeSpillList);}
   void unlockFreeSpillList() {_enabledFlags.reset(LockFreeSpillList);}

   bool getEnableRegisterUsageTracking() { return _enabledFlags.testAny(TrackRegisterUsage); }
   void setEnableRegisterUsageTracking() { _enabledFlags.set(TrackRegisterUsage); }
   void resetEnableRegisterUsageTracking() {_enabledFlags.reset(TrackRegisterUsage);}

   // --------------------------------------------------------------------------
   // Register assignment
   //
   TR_RegisterKinds prepareRegistersForAssignment(); // no virt
   void addToUnlatchedRegisterList(TR::RealRegister *reg);
   void freeUnlatchedRegisters();

   // --------------------------------------------------------------------------
   // Listing
   //
   uint32_t _indentation;


   // --------------------------------------------------------------------------
   // Code patching
   //
   // Used to find out whether there is an appropriate instruction space as vgdnop space
   int32_t sizeOfInstructionToBePatched(TR::Instruction *vgdnop);
   int32_t sizeOfInstructionToBePatchedHCRGuard(TR::Instruction *vgdnop);
   // Used to find which instruction is an appropriate instruction space as vgdnop space
   TR::Instruction* getInstructionToBePatched(TR::Instruction *vgdnop);
   // Used to find the guard instruction where a given guard will actually patch
   // currently can only return a value other than vgdnop for HCR guards
   TR::Instruction* getVirtualGuardForPatching(TR::Instruction *vgdnop);

   OMR_API virtual void jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched) {}
   OMR_API virtual void jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched) {}
   OMR_API virtual void jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false) {}
   OMR_API virtual void jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false) {}
   OMR_API virtual void jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(void *firstInstruction) {} //J9
   OMR_API virtual bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const TR::Node *forNode) { return false; } //J9
   OMR_API virtual bool wantToPatchClassPointer(const TR_OpaqueClassBlock *allegedClassPointer, const uint8_t *inCodeAt) { return false; } //J9

   // --------------------------------------------------------------------------
   // Unclassified
   //

   // P now
   bool isRotateAndMask(TR::Node *node) { return false; } // no virt

   TR::Instruction *generateNop(TR::Node *node, TR::Instruction *instruction=0, TR_NOPKind nopKind=TR_NOPStandard); // no virt, cast
   bool isOutOfLineHotPath() { TR_ASSERT(0, "isOutOfLineHotPath is only implemented for 390 and ppc"); return false;} // no virt

   //Rather confusingly not used -only- in BCD related codegen.
   //... has leaked into non-BCD code.
   bool traceBCDCodeGen();
   void traceBCDEntry(char *str, TR::Node *node);
   void traceBCDExit(char *str, TR::Node *node);

   TR_BitVector *getLiveButMaybeUnreferencedLocals() {return _liveButMaybeUnreferencedLocals;}
   TR_BitVector *setLiveButMaybeUnreferencedLocals(TR_BitVector *v) {return (_liveButMaybeUnreferencedLocals = v);}

   TR::AheadOfTimeCompile *getAheadOfTimeCompile() {return _aheadOfTimeCompile;}
   TR::AheadOfTimeCompile *setAheadOfTimeCompile(TR::AheadOfTimeCompile *p) {return (_aheadOfTimeCompile = p);}

   // J9, X86
   bool canTransformUnsafeCopyToArrayCopy() { return false; } // no virt
   bool canTransformUnsafeSetMemory() { return false; }

   bool canNullChkBeImplicit(TR::Node *); // no virt, cast
   bool canNullChkBeImplicit(TR::Node *, bool doChecks);

   bool IsInMemoryType(TR::DataType type) { return false; }

   bool nodeMayCauseException(TR::Node *node) { return false; } // no virt

   // Should these be in codegen?
   bool isSupportedAdd(TR::Node *addr);
   bool nodeMatches(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop=false);
   bool additionsMatch(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop=false);
   bool addressesMatch(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop=false);
   // Z codegen
   bool uniqueAddressOccurrence(TR::Node *addr1, TR::Node *addr2, bool addressesUnderSameTreeTop);

   TR_StorageOverlapKind storageMayOverlap(TR::Node *node1, size_t length1, TR::Node *node2, size_t length2);

   // arrayTranslateTableRequiresAlignment returns a mask if alignent required, otherwise 0
   // For example, if a page is 4096 bytes and array translation required page alignment, 4095 would
   // be returned.
   // arrayTranslateMinimumNumberOfElements returns the minimum number of elements that need
   // to be processed for it to be worth it to execute the 'translate' built-in function
   // arrayTranslateAndTestMinimumNumberOfIterations returns the minimum number of iterations
   // that the loop must run for the transformation to be worthwhile.
   int32_t arrayTranslateTableRequiresAlignment(bool isByteSource, bool isByteTarget)  { return 0; } // no virt

   // These methods used to return a default value of INT_MAX. However, in at least one place,
   // and quite possibly elsewhere, the optimizer tests for
   //
   //    arrayTranslateAndTestMinimumNumberOfIterations >= freq1 / freq2
   //
   // by instead using integer arithmetic like this:
   //
   //    arrayTranslateAndTestMinimumNumberOfIterations * freq2 >= freq1.
   //
   // In such a test, the default INT_MAX causes a signed overflow, which is undefined behaviour.
   // We appear to have gotten relatively benign wrapping behaviour, but the wrapped result is
   // -freq2 when freq2 is even, and INT_MAX - freq2 + 1 when freq2 is odd, which means that we
   // would make a decision based solely on the low bit of freq2.
   //
   // Rather than ensure that all callers use floating-point division for such tests, we can
   // make sure here that the default may be safely multiplied by a block frequency. The new
   // default of 10001 is still unmeasurably large as a frequency ratio.
   //
   // The proper thing should be for the optimizer to perform these sorts of transformations
   // regardless of the expected number of iterations, because the transformed IL provides
   // strictly more information to codegen, which can then generate the best sequence at its
   // discretion, fabricating a loop if necessary. But at least in the case of idiom
   // recognition's MemCpy pattern, we want Design 94472 for this.
   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget); // no virt

   // TO TransformUtil.  Make platform specific
   int32_t arrayTranslateAndTestMinimumNumberOfIterations(); // no virt
   static int32_t defaultArrayTranslateMinimumNumberOfIterations(const char *methodName);
   static bool useOldArrayTranslateMinimumNumberOfIterations()
      {
      static bool useOldValue = feGetEnv("TR_oldArrayTranslateMinimumNumberOfIterations") != NULL;
      return useOldValue;
      }

   // the following functions evaluate whether a codegen for the node or for static
   // symbol reference requires entry in the literal pool
   bool arithmeticNeedsLiteralFromPool(TR::Node *node) { return false; } // no virt
   bool bitwiseOpNeedsLiteralFromPool(TR::Node *parent, TR::Node *child) { return false; } // no virt
   bool bndsChkNeedsLiteralFromPool(TR::Node *node) { return false; } // no virt
   bool constLoadNeedsLiteralFromPool(TR::Node *node) { return false; } // no virt, cast
   void setOnDemandLiteralPoolRun(bool answer) {} // no virt, cast
   bool isLiteralPoolOnDemandOn () { return false; } // no virt, cast
   bool supportsOnDemandLiteralPool() { return false; } // no virt, cast
   bool supportsDirectIntegralLoadStoresFromLiteralPool() { return false; } // no virt
   bool supportsHighWordFacility() { return false; } // no virt, default, cast

   OMR_API virtual bool inlineDirectCall(TR::Node *node, TR::Register *&resultReg) { return false; }

   // J9 only, move to trj9
   TR_OpaqueClassBlock* getMonClass(TR::Node* monNode);

   void setCurrentBlock(TR::Block *b);
   TR::Block *getCurrentBlock() { return _currentBlock; }

   bool hasCCInfo()                          { return (_flags2.testAny(HasCCInfo));}
   void setHasCCInfo(bool v)                 { _flags2.set(HasCCInfo, v); }
   bool hasCCOverflow()                      { return (_flags2.testAny(HasCCOverflow));}
   void setHasCCOverflow(bool v)             { _flags2.set(HasCCOverflow, v); }
   bool hasCCSigned()                        { return (_flags2.testAny(HasCCSigned));}
   void setHasCCSigned(bool v)               { _flags2.set(HasCCSigned, v); }
   bool hasCCZero()                          { return (_flags2.testAny(HasCCZero));}
   void setHasCCZero(bool v)                 { _flags2.set(HasCCZero, v); }
   bool hasCCCarry()                         { return (_flags2.testAny(HasCCCarry));}
   void setHasCCCarry(bool v)                { _flags2.set(HasCCCarry, v); }

   bool hasCCCompare()                       { return (_flags3.testAny(HasCCCompare));}
   void setHasCCCompare(bool v)                { _flags3.set(HasCCCompare, v); }

   bool requiresCarry()                      { return (_flags3.testAny(RequiresCarry));}
   void setRequiresCarry(bool v)             { _flags3.set(RequiresCarry, v); }
   bool computesCarry()                      { return (_flags3.testAny(ComputesCarry));}
   void setComputesCarry(bool v)             { _flags3.set(ComputesCarry, v); }

   TR::RealRegister **_unlatchedRegisterList; // dynamically allocated

   bool alwaysUseTrampolines() { return _enabledFlags.testAny(AlwaysUseTrampolines); }
   void setAlwaysUseTrampolines() {_enabledFlags.set(AlwaysUseTrampolines);}

   bool shouldBuildStructure() { return _enabledFlags.testAny(ShouldBuildStructure); }
   void setShouldBuildStructure() {_enabledFlags.set(ShouldBuildStructure);}


   bool enableRefinedAliasSets();
   void setEnableRefinedAliasSets() {_enabledFlags.set(EnableRefinedAliasSets);}

   // --------------------------------------------------------------------------

   TR::Node *createOrFindClonedNode(TR::Node *node, int32_t numChildren);

   void zeroOutAutoOnEdge(TR::SymbolReference * liveAutoSym, TR::Block *block, TR::Block *succBlock, TR::list<TR::Block*> *newBlocks, TR_ScratchList<TR::Node> *fsdStores);

   bool constantAddressesCanChangeSize(TR::Node *node);
   bool profiledPointersRequireRelocation();
   bool needGuardSitesEvenWhenGuardRemoved();
   bool supportVMInternalNatives();
   bool supportsNativeLongOperations();

   TR::DataType IntJ() { return TR::Compiler->target.is64Bit() ? TR::Int64 : TR::Int32; }

   // will a BCD left shift always leave the sign code unchanged and thus allow it to be propagated through and upwards
   OMR_API virtual bool propagateSignThroughBCDLeftShift(TR::DataType type) { return false; } // no virt

   bool supportsLengthMinusOneForMemoryOpts() {return false;} // no virt, cast

   // Java, likely Z
   bool supportsTrapsInTMRegion() { return true; } // no virt

   // Allows a platform code generator to assert that a particular node operation will use 64 bit values
   // that are not explicitly present in the node datatype.
   bool usesImplicit64BitGPRs(TR::Node *node) { return false; } // no virt

   // General utility?
   static bool treeContainsCall(TR::TreeTop * treeTop);

   // IA32 only?
   int32_t getVMThreadGlobalRegisterNumber() {return -1;} // no virt
   int32_t arrayInitMinimumNumberOfBytes() {return 8;} // no virt

   TR::Instruction *saveOrRestoreRegisters(TR_BitVector *regs, TR::Instruction *cursor, bool doSaves);

   void addCountersToEdges(TR::Block *block);

   bool getSupportsBitOpCodes() {return false;} // no virt, default

   bool getMappingAutomatics() {return _flags1.testAny(MappingAutomatics);}
   void setMappingAutomatics() {_flags1.set(MappingAutomatics);}

   bool getSupportsDirectJNICalls() {return _flags1.testAny(SupportsDirectJNICalls);} // no virt
   bool supportsDirectJNICallsForAOT() { return false;} // no virt, default

   void setSupportsDirectJNICalls() {_flags1.set(SupportsDirectJNICalls);}

   bool getSupportsGlRegDeps() {return _flags1.testAny(SupportsGlRegDeps);}
   void setSupportsGlRegDeps() {_flags1.set(SupportsGlRegDeps);}

   bool getSupportsVectorRegisters() {return _flags1.testAny(SupportsVectorRegisters);}
   void setSupportsVectorRegisters() {_flags1.set(SupportsVectorRegisters);}

   bool getSupportsGlRegDepOnFirstBlock() {return _flags1.testAny(SupportsGlRegDepOnFirstBlock);}
   void setSupportsGlRegDepOnFirstBlock() {_flags1.set(SupportsGlRegDepOnFirstBlock);}

   bool getDisableLongGRA() {return _flags1.testAny(DisableLongGRA);}
   void setDisableLongGRA() {_flags1.set(DisableLongGRA);}

   bool getDisableFpGRA() {return _flags2.testAny(DisableFpGRA);}
   void setDisableFpGRA() {_flags2.set(DisableFpGRA);}

   bool getSupportsVMThreadGRA() {return _flags2.testAny(SupportsVMThreadGRA);}
   void setSupportsVMThreadGRA() {_flags2.set(SupportsVMThreadGRA);}
   void resetSupportsVMThreadGRA() {_flags2.reset(SupportsVMThreadGRA);}

   bool usesRegisterMaps() {return _flags1.testAny(UsesRegisterMaps);}
   void setUsesRegisterMaps() {_flags1.set(UsesRegisterMaps);}

   bool getSupportsDivCheck() {return _flags1.testAny(SupportsDivCheck);}
   void setSupportsDivCheck() {_flags1.set(SupportsDivCheck);}

   bool getSupportsPrimitiveArrayCopy() {return _flags2.testAny(SupportsPrimitiveArrayCopy);}
   void setSupportsPrimitiveArrayCopy() {_flags2.set(SupportsPrimitiveArrayCopy);}

   bool getSupportsReferenceArrayCopy() {return _flags1.testAny(SupportsReferenceArrayCopy);}
   void setSupportsReferenceArrayCopy() {_flags1.set(SupportsReferenceArrayCopy);}

   bool getSupportsEfficientNarrowIntComputation() {return _flags2.testAny(SupportsEfficientNarrowIntComputation);}
   void setSupportsEfficientNarrowIntComputation() {_flags2.set(SupportsEfficientNarrowIntComputation);}

   bool getSupportsEfficientNarrowUnsignedIntComputation() {return _flags4.testAny(SupportsEfficientNarrowUnsignedIntComputation);}
   void setSupportsEfficientNarrowUnsignedIntComputation() {_flags4.set(SupportsEfficientNarrowUnsignedIntComputation);}

   bool getSupportsArrayTranslateAndTest() {return _flags2.testAny(SupportsArrayTranslateAndTest);}
   void setSupportsArrayTranslateAndTest() {_flags2.set(SupportsArrayTranslateAndTest);}

   bool getSupportsReverseLoadAndStore() {return _flags2.testAny(SupportsReverseLoadAndStore);}
   void setSupportsReverseLoadAndStore() {_flags2.set(SupportsReverseLoadAndStore);}

   bool getSupportsArrayTranslateTRxx() {return _flags2.testAny(SupportsArrayTranslate);}
   void setSupportsArrayTranslateTRxx() {_flags2.set(SupportsArrayTranslate);}

   bool getSupportsArrayTranslateTRTO255() {return _flags4.testAny(SupportsArrayTranslateTRTO255);}
   void setSupportsArrayTranslateTRTO255() {_flags4.set(SupportsArrayTranslateTRTO255);}

   bool getSupportsArrayTranslateTRTO() {return _flags4.testAny(SupportsArrayTranslateTRTO);}
   void setSupportsArrayTranslateTRTO() {_flags4.set(SupportsArrayTranslateTRTO);}

   bool getSupportsArrayTranslateTROTNoBreak() {return _flags4.testAny(SupportsArrayTranslateTROTNoBreak);}
   void setSupportsArrayTranslateTROTNoBreak() {_flags4.set(SupportsArrayTranslateTROTNoBreak);}

   bool getSupportsArrayTranslateTROT() {return _flags4.testAny(SupportsArrayTranslateTROT);}
   void setSupportsArrayTranslateTROT() {_flags4.set(SupportsArrayTranslateTROT);}

   bool getSupportsEncodeUtf16LittleWithSurrogateTest() { return false; }
   bool getSupportsEncodeUtf16BigWithSurrogateTest() { return false; }

   bool supportsZonedDFPConversions() {return _enabledFlags.testAny(SupportsZonedDFPConversions);}
   void setSupportsZonedDFPConversions() {_enabledFlags.set(SupportsZonedDFPConversions);}

   bool supportsIntDFPConversions() {return _enabledFlags.testAny(SupportsIntDFPConversions);}
   void setSupportsIntDFPConversions() {_enabledFlags.set(SupportsIntDFPConversions);}

   bool supportsFastPackedDFPConversions() {return _enabledFlags.testAny(SupportsFastPackedDFPConversions);}
   void setSupportsFastPackedDFPConversions() {_enabledFlags.set(SupportsFastPackedDFPConversions);}

   bool getSupportsArraySet() {return _flags1.testAny(SupportsArraySet);}
   void setSupportsArraySet() {_flags1.set(SupportsArraySet);}

   bool getSupportsArrayCmp() {return _flags1.testAny(SupportsArrayCmp);}
   void setSupportsArrayCmp() {_flags1.set(SupportsArrayCmp);}

   bool getSupportsArrayCmpSign() {return _flags3.testAny(SupportsArrayCmpSign);}
   void setSupportsArrayCmpSign() {_flags3.set(SupportsArrayCmpSign);}

   bool getSupportsSearchCharString() {return _flags3.testAny(SupportsSearchCharString);}
   void setSupportsSearchCharString() {_flags3.set(SupportsSearchCharString);}

   bool getSupportsTranslateAndTestCharString() {return _flags3.testAny(SupportsTranslateAndTestCharString);}
   void setSupportsTranslateAndTestCharString() {_flags3.set(SupportsTranslateAndTestCharString);}

   bool getSupportsTestCharComparisonControl() {return _flags3.testAny(SupportsTestCharComparisonControl);}
   void setSupportsTestCharComparisonControl() {_flags3.set(SupportsTestCharComparisonControl);}

   bool getMethodContainsBinaryCodedDecimal() { return _flags3.testAny(MethodContainsBinaryCodedDecimal);}
   void setMethodContainsBinaryCodedDecimal() { _flags3.set(MethodContainsBinaryCodedDecimal);}

   bool getAccessStaticsIndirectly() {return _flags1.testAny(AccessStaticsIndirectly);}
   void setAccessStaticsIndirectly(bool b) {_flags1.set(AccessStaticsIndirectly, b);}

   bool getSupportsBigDecimalLongLookasideVersioning() { return _flags3.testAny(SupportsBigDecimalLongLookasideVersioning);}
   void setSupportsBigDecimalLongLookasideVersioning() { _flags3.set(SupportsBigDecimalLongLookasideVersioning);}

   bool getSupportsBDLLHardwareOverflowCheck() { return _flags3.testAny(SupportsBDLLHardwareOverflowCheck);}
   void setSupportsBDLLHardwareOverflowCheck() { _flags3.set(SupportsBDLLHardwareOverflowCheck);}

   bool getSupportsDoubleWordCAS() { return _flags3.testAny(SupportsDoubleWordCAS);}
   void setSupportsDoubleWordCAS() { _flags3.set(SupportsDoubleWordCAS);}

   bool getSupportsDoubleWordSet() { return _flags3.testAny(SupportsDoubleWordSet);}
   void setSupportsDoubleWordSet() { _flags3.set(SupportsDoubleWordSet);}

   bool getSupportsTMDoubleWordCASORSet() { return _flags3.testAny(SupportsTMDoubleWordCASORSet);}
   void setSupportsTMDoubleWordCASORSet() { _flags3.set(SupportsTMDoubleWordCASORSet);}

   bool getSupportsTMHashMapAndLinkedQueue() { return _flags4.testAny(SupportsTMHashMapAndLinkedQueue);}
   void setSupportsTMHashMapAndLinkedQueue() { _flags4.set(SupportsTMHashMapAndLinkedQueue);}

   bool getSupportsAtomicLoadAndAdd() { return _flags4.testAny(SupportsAtomicLoadAndAdd);}
   void setSupportsAtomicLoadAndAdd() { _flags4.set(SupportsAtomicLoadAndAdd);}

   bool getSupportsTM() { return _flags4.testAny(SupportsTM);}
   void setSupportsTM() { _flags4.set(SupportsTM);}

   bool getSupportsLM() { return _flags4.testAny(SupportsLM);}
   void setSupportsLM() { _flags4.set(SupportsLM);}

   virtual bool getSupportsTLE();

   virtual bool getSupportsIbyteswap();

   virtual bool getSupportsBitPermute();

   bool getSupportsAutoSIMD() { return _flags4.testAny(SupportsAutoSIMD);}
   void setSupportsAutoSIMD() { _flags4.set(SupportsAutoSIMD);}

   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode, TR::DataType) { return false; }

   bool removeRegisterHogsInLowerTreesWalk() { return _flags3.testAny(RemoveRegisterHogsInLowerTreesWalk);}
   void setRemoveRegisterHogsInLowerTreesWalk() { _flags3.set(RemoveRegisterHogsInLowerTreesWalk);}
   void resetRemoveRegisterHogsInLowerTreesWalk() {_flags3.reset(RemoveRegisterHogsInLowerTreesWalk);}

   bool getJNILinkageCalleeCleanup() {return _flags1.testAny(JNILinkageCalleeCleanup);}
   void setJNILinkageCalleeCleanup() {_flags1.set(JNILinkageCalleeCleanup);}

   bool getHasResumableTrapHandler() {return _flags1.testAny(HasResumableTrapHandler);}
   void setHasResumableTrapHandler() {_flags1.set(HasResumableTrapHandler);}

   bool performsChecksExplicitly() {return _flags1.testAny(PerformsExplicitChecks);}
   void setPerformsChecksExplicitly() {_flags1.set(PerformsExplicitChecks);}

   bool spillsFPRegistersAcrossCalls() {return _flags1.testAny(SpillsFPRegistersAcrossCalls);}
   void setSpillsFPRegistersAcrossCalls() {_flags1.set(SpillsFPRegistersAcrossCalls);}

   bool getSupportsJavaFloatSemantics() {return _flags1.testAny(SupportsJavaFloatSemantics);}
   void setSupportsJavaFloatSemantics() {_flags1.set(SupportsJavaFloatSemantics);}

   bool getSupportsInliningOfTypeCoersionMethods() {return _flags1.testAny(SupportsInliningOfTypeCoersionMethods);}
   void setSupportsInliningOfTypeCoersionMethods() {_flags1.set(SupportsInliningOfTypeCoersionMethods);}

   bool getSupportsPartialInlineOfMethodHooks() {return _flags1.testAny(SupportsPartialInlineOfMethodHooks);}
   void setSupportsPartialInlineOfMethodHooks() {_flags1.set(SupportsPartialInlineOfMethodHooks);}

   bool getVMThreadRequired() {return _flags1.testAny(VMThreadRequired);}
   void setVMThreadRequired(bool v);

   bool getSupportsMergedAllocations() {return _flags1.testAny(SupportsMergedAllocations);}
   void setSupportsMergedAllocations() {_flags1.set(SupportsMergedAllocations);}

   bool getSupportsInlinedAtomicLongVolatiles() {return _flags1.testAny(SupportsInlinedAtomicLongVolatiles);}
   void setSupportsInlinedAtomicLongVolatiles() {_flags1.set(SupportsInlinedAtomicLongVolatiles);}
   bool getInlinedGetCurrentThreadMethod() {return _flags3.testAny(InlinedGetCurrentThreadMethod);}
   void setInlinedGetCurrentThreadMethod() {_flags3.set(InlinedGetCurrentThreadMethod);}

   bool considerAllAutosAsTacticalGlobalRegisterCandidates()    {return _flags1.testAny(ConsiderAllAutosAsTacticalGlobalRegisterCandidates);}
   void setConsiderAllAutosAsTacticalGlobalRegisterCandidates() {_flags1.set(ConsiderAllAutosAsTacticalGlobalRegisterCandidates);}

   bool getSupportsScaledIndexAddressing() { return _flags1.testAny(SupportsScaledIndexAddressing); }
   void setSupportsScaledIndexAddressing() { _flags1.set(SupportsScaledIndexAddressing); }

   bool isAddressScaleIndexSupported(int32_t scale) { return false; } // no virt

   bool getSupportsConstantOffsetInAddressing(int64_t value);
   bool getSupportsConstantOffsetInAddressing() { return _flags3.testAny(SupportsConstantOffsetInAddressing); }
   void setSupportsConstantOffsetInAddressing() { _flags3.set(SupportsConstantOffsetInAddressing); }

   bool getSupportsCompactedLocals() {return _flags1.testAny(SupportsCompactedLocals);}
   void setSupportsCompactedLocals() {_flags1.set(SupportsCompactedLocals);}

   bool getSupportsFastCTM() {return _flags1.testAny(SupportsFastCTM);}
   void setSupportsFastCTM() {_flags1.set(SupportsFastCTM);}

   bool getSupportsCurrentTimeMaxPrecision() {return _flags2.testAny(SupportsCurrentTimeMaxPrecision);}
   void setSupportsCurrentTimeMaxPrecision() {_flags2.set(SupportsCurrentTimeMaxPrecision);}

   bool getSupportsAlignedAccessOnly() { return _flags3.testAny(SupportsAlignedAccessOnly); }
   void setSupportsAlignedAccessOnly() { _flags3.set(SupportsAlignedAccessOnly); }

   bool usesRegisterPairsForLongs()    {return _flags1.testAny(UsesRegisterPairsForLongs);}
   void setUsesRegisterPairsForLongs() {_flags1.set(UsesRegisterPairsForLongs);}

   bool getSupportsIDivAndIRemWithThreeChildren() {return _flags2.testAny(SupportsIDivAndIRemWithThreeChildren);}
   void setSupportsIDivAndIRemWithThreeChildren() {_flags2.set(SupportsIDivAndIRemWithThreeChildren);}

   bool getSupportsLDivAndLRemWithThreeChildren() {return _flags2.testAny(SupportsLDivAndLRemWithThreeChildren);}
   void setSupportsLDivAndLRemWithThreeChildren() {_flags2.set(SupportsLDivAndLRemWithThreeChildren);}

   bool getSupportsLoweringConstIDiv() {return _flags2.testAny(SupportsLoweringConstIDiv);}
   void setSupportsLoweringConstIDiv() {_flags2.set(SupportsLoweringConstIDiv);}

   bool getSupportsLoweringConstLDiv() {return _flags2.testAny(SupportsLoweringConstLDiv);}
   void setSupportsLoweringConstLDiv() {_flags2.set(SupportsLoweringConstLDiv);}

   bool getSupportsLoweringConstLDivPower2() {return _flags2.testAny(SupportsLoweringConstLDivPower2);}
   void setSupportsLoweringConstLDivPower2() {_flags2.set(SupportsLoweringConstLDivPower2);}

   bool getSupportsVirtualGuardNOPing() { return _flags2.testAny(SupportsVirtualGuardNOPing); }
   void setSupportsVirtualGuardNOPing() { _flags2.set(SupportsVirtualGuardNOPing); }

   bool getSupportsNewInstanceImplOpt() { return _flags2.testAny(SupportsNewInstanceImplOpt); }
   void setSupportsNewInstanceImplOpt() { _flags2.set(SupportsNewInstanceImplOpt); }

   bool getHasDoubleWordAlignedStack() { return _flags2.testAny(HasDoubleWordAlignedStack); }
   void setHasDoubleWordAlignedStack() { _flags2.set(HasDoubleWordAlignedStack); }

   bool getSupportsReadOnlyLocks() {return _flags2.testAny(SupportsReadOnlyLocks);}
   void setSupportsReadOnlyLocks() {_flags2.set(SupportsReadOnlyLocks);}

   bool getSupportsPostProcessArrayCopy() {return _flags2.testAny(SupportsPostProcessArrayCopy);}
   void setSupportsPostProcessArrayCopy() {_flags2.set(SupportsPostProcessArrayCopy);}

   bool getOptimizationPhaseIsComplete() {return _flags4.testAny(OptimizationPhaseIsComplete);}
   void setOptimizationPhaseIsComplete() {_flags4.set(OptimizationPhaseIsComplete);}

   bool getSupportsBCDToDFPReduction() {return _flags4.testAny(SupportsBCDToDFPReduction);}
   void setSupportsBCDToDFPReduction() {_flags4.set(SupportsBCDToDFPReduction);}

   bool getSupportsTestUnderMask() {return _flags4.testAny(SupportsTestUnderMask);}
   void setSupportsTestUnderMask() {_flags4.set(SupportsTestUnderMask);}

   bool getSupportsRuntimeInstrumentation() {return _flags4.testAny(SupportsRuntimeInstrumentation);}
   void setSupportsRuntimeInstrumentation() {_flags4.set(SupportsRuntimeInstrumentation);}

   bool isOutOfLineColdPath() {return (_outOfLineColdPathNestedDepth > 0) ? true : false;}
   void incOutOfLineColdPathNestedDepth(){_outOfLineColdPathNestedDepth++;}
   void decOutOfLineColdPathNestedDepth(){_outOfLineColdPathNestedDepth--;}

   bool getMethodModifiedByRA() {return _flags2.testAny(MethodModifiedByRA);}
   void setMethodModifiedByRA() {_flags2.set(MethodModifiedByRA);}
   void resetMethodModifiedByRA() {_flags2.reset(MethodModifiedByRA);}

   bool getEnforceStoreOrder() {return _flags2.testAny(EnforceStoreOrder);}
   void setEnforceStoreOrder() {_flags2.set(EnforceStoreOrder);}

   bool getDisableNullCheckOfArrayLength() { return _flags3.testAny(CompactNullCheckOfArrayLengthDisabled); }
   void setDisableNullCheckOfArrayLength() { _flags3.set(CompactNullCheckOfArrayLengthDisabled); }

   bool getSupportsShrinkWrapping() { return _flags3.testAny(SupportsShrinkWrapping); }
   void setSupportsShrinkWrapping() { _flags3.set(SupportsShrinkWrapping); }

   bool getShrinkWrappingDone() { return _flags3.testAny(ShrinkWrappingDone); }
   void setShrinkWrappingDone() { _flags3.set(ShrinkWrappingDone); }

   bool getUsesLoadStoreMultiple() { return _flags3.testAny(UsesLoadStoreMultiple); }
   void setUsesLoadStoreMultiple() { _flags3.set(UsesLoadStoreMultiple); }

   bool getSupportsStackAllocationOfArraylets() {return _flags3.testAny(SupportsStackAllocationOfArraylets);}
   void setSupportsStackAllocationOfArraylets() {_flags3.set(SupportsStackAllocationOfArraylets);}

   bool expandExponentiation() { return _flags3.testAny(ExpandExponentiation); }
   void setExpandExponentiation() { _flags3.set(ExpandExponentiation); }

   bool multiplyIsDestructive() { return _flags3.testAny(MultiplyIsDestructive); }
   void setMultiplyIsDestructive() { _flags3.set(MultiplyIsDestructive); }

   bool getIsInOOLSection() { return _flags4.testAny(IsInOOLSection); }
   void toggleIsInOOLSection();

   bool isInMemoryInstructionCandidate(TR::Node * node);

   bool trackingInMemoryKilledLoads() {return _flags4.testAny(TrackingInMemoryKilledLoads);}
   void setTrackingInMemoryKilledLoads() {_flags4.set(TrackingInMemoryKilledLoads);}
   void resetTrackingInMemoryKilledLoads() {_flags4.reset(TrackingInMemoryKilledLoads);}

   void setLmmdFailed() { _lmmdFailed = true;}

   protected:

   CodeGenerator();

   enum // _flags1
      {
      MappingAutomatics                                  = 0x00000001,
      SupportsDirectJNICalls                             = 0x00000002,
      SupportsGlRegDeps                                  = 0x00000004,
      SupportsDivCheck                                   = 0x00000008,
      UsesRegisterMaps                                   = 0x00000010,
      JNILinkageCalleeCleanup                            = 0x00000020,
      HasResumableTrapHandler                            = 0x00000040,
      IsLeafMethod                                       = 0x00000080,
      SupportsPartialInlineOfMethodHooks                 = 0x00000100,
      SupportsReferenceArrayCopy                         = 0x00000200,
      SupportsJavaFloatSemantics                         = 0x00000400,
      SupportsInliningOfTypeCoersionMethods              = 0x00000800,
      VMThreadRequired                                   = 0x00001000,
      SupportsVectorRegisters                            = 0x00002000,
      SupportsGlRegDepOnFirstBlock                       = 0x00004000,
      SupportsRemAsThirdChildOfDiv                       = 0x00008000,
      HasNodePointerInEachInstruction                    = 0x00010000,
      SupportsMergedAllocations                          = 0x00020000,
      SupportsInlinedAtomicLongVolatiles                 = 0x00040000,
      PerformsExplicitChecks                             = 0x00080000,
      SpillsFPRegistersAcrossCalls                       = 0x00100000,
      ConsiderAllAutosAsTacticalGlobalRegisterCandidates = 0x00200000,
      //                                                 = 0x00400000,   // Available
      SupportsScaledIndexAddressing                      = 0x00800000,
      SupportsCompactedLocals                            = 0x01000000,
      SupportsFastCTM                                    = 0x02000000,
      UsesRegisterPairsForLongs                          = 0x04000000,
      SupportsArraySet                                   = 0x08000000,
      AccessStaticsIndirectly                            = 0x10000000,
      SupportsArrayCmp                                   = 0x20000000,
      DisableLongGRA                                     = 0x40000000,
      DummyLastEnum1
      };

   enum // _flags2
      {
      SupportsIDivAndIRemWithThreeChildren                = 0x00000001,
      SupportsLDivAndLRemWithThreeChildren                = 0x00000002,
      SupportsPrimitiveArrayCopy                          = 0x00000004,
      SupportsVirtualGuardNOPing                          = 0x00000008,
      SupportsEfficientNarrowIntComputation               = 0x00000010,
      SupportsNewInstanceImplOpt                          = 0x00000020,
      SupportsLoweringConstIDiv                           = 0x00000040,
      SupportsLoweringConstLDiv                           = 0x00000080,
      SupportsArrayTranslate                              = 0x00000100,
      HasDoubleWordAlignedStack                           = 0x00000200,
      SupportsReadOnlyLocks                               = 0x00000400,
      SupportsArrayTranslateAndTest                       = 0x00000800,
      // AVAILABLE                                        = 0x00001000,
      // AVAILABLE                                        = 0x00002000,
      SupportsVMThreadGRA                                 = 0x00004000,
      SupportsPostProcessArrayCopy                        = 0x00008000,
      //                                                  = 0x00010000,   AVAILABLE FOR USE!!!
      SupportsCurrentTimeMaxPrecision                     = 0x00020000,
      HasCCSigned                                         = 0x00040000,
      HasCCZero                                           = 0x00080000,
      HasCCOverflow                                       = 0x00100000,
      HasCCInfo                                           = 0x00200000,
      SupportsReverseLoadAndStore                         = 0x00400000,
      SupportsLoweringConstLDivPower2                     = 0x00800000,
      DisableFpGRA                                        = 0x01000000,
      // Available                                        = 0x02000000,
      MethodModifiedByRA                                  = 0x04000000,
      SchedulingInstrCleanupNeeded                        = 0x08000000,
      // Available                                        = 0x10000000,
      EnforceStoreOrder                                   = 0x20000000,
      SupportsNewReferenceArrayCopy                       = 0x80000000,   // AVAILABLE FOR USE!!!!!!
      DummyLastEnum2
      };

   enum // _flags3
      {
      //                                                  = 0x00000001,   AVAILABLE FOR USE!!!!!!
      SupportsConstantOffsetInAddressing                  = 0x00000002,
      SupportsAlignedAccessOnly                           = 0x00000004,
      //                                                  = 0x00000008,   AVAILABLE FOR USE!!!!!!
      CompactNullCheckOfArrayLengthDisabled               = 0x00000010,
      SupportsArrayCmpSign                                = 0x00000020,
      SupportsSearchCharString                            = 0x00000040,
      SupportsTranslateAndTestCharString                  = 0x00000080,
      SupportsTestCharComparisonControl                   = 0x00000100,
      //                                                  = 0x00000200,   AVAILABLE FOR USE!!!!!!
      //                                                  = 0x00000400,   AVAILABLE FOR USE!!!!!!
      HasCCCarry                                          = 0x00000800,
      //                                                  = 0x00001000,  AVAILABLE FOR USE!!!!!!
      SupportsBigDecimalLongLookasideVersioning           = 0x00002000,
      RemoveRegisterHogsInLowerTreesWalk                  = 0x00004000,
      SupportsBDLLHardwareOverflowCheck                   = 0x00008000,
      InlinedGetCurrentThreadMethod                       = 0x00010000,
      RequiresCarry                                       = 0x00020000,
      MethodContainsBinaryCodedDecimal                    = 0x00040000,  // wcode
      ComputesCarry                                       = 0x00080000,
      SupportsShrinkWrapping                              = 0x00100000,
      ShrinkWrappingDone                                  = 0x00200000,
      SupportsStackAllocationOfArraylets                  = 0x00400000,
      //                                                  = 0x00800000,  AVAILABLE FOR USE!
      SupportsDoubleWordCAS                               = 0x01000000,
      SupportsDoubleWordSet                               = 0x02000000,
      UsesLoadStoreMultiple                               = 0x04000000,
      ExpandExponentiation                                = 0x08000000,
      MultiplyIsDestructive                               = 0x10000000,
      //                                                  = 0x20000000,  AVAILABLE FOR USE!
      HasCCCompare                                        = 0x40000000,
      SupportsTMDoubleWordCASORSet                        = 0x80000000,
      DummyLastEnum
      };

   enum // flags4
      {
      //                                                  = 0x00000001,  AVAILABLE FOR USE!
      //                                                  = 0x00000002,  AVAILABLE FOR USE!
      //                                                  = 0x00000004,  AVAILABLE FOR USE!
      OptimizationPhaseIsComplete                         = 0x00000008,
      RequireRAPassAR                                     = 0x00000010,
      IsInOOLSection                                      = 0x00000020,
      SupportsBCDToDFPReduction                           = 0x00000040,
      GRACompleted                                        = 0x00000080,
      SupportsTestUnderMask                               = 0x00000100,
      SupportsRuntimeInstrumentation                      = 0x00000200,
      SupportsEfficientNarrowUnsignedIntComputation       = 0x00000400,
      SupportsAtomicLoadAndAdd                            = 0x00000800,
      //                                                  = 0x00001000, AVAILABLE FOR USE!
      // AVAILABLE                                        = 0x00002000,
      HasSignCleans                                       = 0x00004000,
      SupportsArrayTranslateTRTO255                       = 0x00008000, //if (ca[i] < 256) ba[i] = (byte) ca[i]
      SupportsArrayTranslateTROTNoBreak                   = 0x00010000, //ca[i] = (char) ba[i]
      SupportsArrayTranslateTRTO                          = 0x00020000, //if (ca[i] < x) ba[i] = (byte) ca[i]; x is either 256 or 128
      SupportsArrayTranslateTROT                          = 0x00040000, //if (ba[i] >= 0) ca[i] = (char) ba[i];
      SupportsTM                                          = 0x00080000,
      SupportsProfiledInlining                            = 0x00100000,
      SupportsAutoSIMD                                = 0x00200000,  //vector support for autoVectorizatoon
      // AVAILABLE                                        = 0x00400000,
      // AVAILABLE                                        = 0x00800000,
      //                                                  = 0x01000000,  NOW AVAILABLE
      //                                                  = 0x02000000,  NOW AVAILABLE
      //                                                  = 0x04000000,  NOW AVAILABLE
      TrackingInMemoryKilledLoads                         = 0x08000000,
      SupportsTMHashMapAndLinkedQueue                     = 0x10000000,
	  SupportsLM                                          = 0x20000000,

      DummyLastEnum4
      };

   enum // enabledFlags
      {
      SupportsZonedDFPConversions      = 0x0001,
      // Available                             ,
      // Available                             ,
      EnableRefinedAliasSets           = 0x0008,
      AlwaysUseTrampolines             = 0x0010,
      ShouldBuildStructure             = 0x0020,
      LockFreeSpillList                = 0x0040,  // TAROK only (until it matures)
      UseNonLinearRegisterAssigner     = 0x0080,  // TAROK only (until it matures)
      TrackRegisterUsage               = 0x0100,  // TAROK only (until it matures)
      //                               = 0x0200,  // AVAILABLE FOR USE!
      //                               = 0x0400,  // AVAILABLE FOR USE!
      SupportsIntDFPConversions        = 0x0800,
      // Available                             ,
      SupportsFastPackedDFPConversions = 0x2000,
      // Available
      };

   TR::SymbolReferenceTable *_symRefTab;
   TR::Linkage *_linkages[TR_NumLinkages];
   TR::Linkage *_bodyLinkage;
   TR::Register *_vmThreadRegister;
   TR::RealRegister *_realVMThreadRegister;
   TR::Instruction *_vmThreadSpillInstr;
   TR::GCStackAtlas *_stackAtlas;
   TR_GCStackMap *_methodStackMap;
   TR::list<TR::Block*> _counterBlocks;
   uint8_t *_binaryBufferStart;
   uint8_t *_binaryBufferCursor;
   TR::SparseBitVector _extendedToInt64GlobalRegisters;

   TR_BitVector *_liveButMaybeUnreferencedLocals;
   bool _lmmdFailed;
   TR_BitVector *_assignedGlobalRegisters;

   TR_LiveRegisters *_liveRegisters[NumRegisterKinds];
   TR::AheadOfTimeCompile *_aheadOfTimeCompile;
   uint32_t *_globalRegisterTable;

   TR_InterferenceGraph *_localsIG;
   TR_BitVector *_currentGRABlockLiveOutSet;
   TR::Block *_currentBlock;
   TR_BitVector *_preservedRegsInPrologue;

   TR::list<TR::SymbolReference*> _availableSpillTemps;
   TR::list<TR_LiveReference*> _liveReferenceList;
   TR::list<TR::Snippet*> _snippetList;
   TR_Array<TR::Register *> _registerArray;
   TR::list<TR_BackingStore*> _spill4FreeList;
   TR::list<TR_BackingStore*> _spill8FreeList;
   TR::list<TR_BackingStore*> _spill16FreeList;
   TR::list<TR_BackingStore*> _internalPointerSpillFreeList;
   TR::list<TR_BackingStore*> _collectedSpillList;
   TR::list<TR_BackingStore*> _allSpillList;
   TR::list<TR::Relocation *> _relocationList;
   TR::list<TR::Relocation *> _aotRelocationList;
   TR::list<TR::StaticRelocation> _staticRelocationList;
   TR::list<uint8_t*> _breakPointList;

   TR::list<TR::SymbolReference*> _variableSizeSymRefPendingFreeList;
   TR::list<TR::SymbolReference*> _variableSizeSymRefFreeList;
   TR::list<TR::SymbolReference*> _variableSizeSymRefAllocList;

   int32_t _accumulatorNodeUsage;

   TR::list<TR::Register*> *_spilledRegisterList;
   TR::list<TR::Register*> *_firstTimeLiveOOLRegisterList;
   TR::list<OMR::RegisterUsage*> *_referencedRegistersList;
   int32_t _currentPathDepth;
   TR::list<TR::Node*> _nodesUnderComputeCCList;
   TR::list<TR::Node*> _nodesToUncommonList;
   TR::list<TR::Node*> _nodesSpineCheckedList;

   TR::list<TR_Pair<TR_ResolvedMethod, TR::Instruction> *> _jniCallSites; // list of instrutions representing direct jni call sites

   TR_Array<void *> _monitorMapping;

   TR::list<TR::Node*> _compressedRefs;

   int32_t _lowestSavedReg;

   uint32_t _largestOutgoingArgSize;

   uint32_t _estimatedCodeLength;
   int32_t _estimatedSnippetStart;
   int32_t _accumulatedInstructionLengthError;
   int32_t _frameSizeInBytes;
   int32_t _registerSaveDescription;
   flags32_t _flags1;
   flags32_t _flags2;
   flags32_t _flags3;
   flags32_t _flags4;
   uint32_t _numberBytesReadInaccessible;
   uint32_t _numberBytesWriteInaccessible;
   uint32_t _maxObjectSizeGuaranteedNotToOverflow;

   int32_t _outOfLineColdPathNestedDepth;

   TR::CodeGenPhase _codeGenPhase;

   TR::Instruction *_firstInstruction;
   TR::Instruction *_appendInstruction;

   TR_RegisterMask _liveRealRegisters[NumRegisterKinds];
   TR_GlobalRegisterNumber _lastGlobalGPR;
   TR_GlobalRegisterNumber _firstGlobalHPR;
   TR_GlobalRegisterNumber _lastGlobalHPR;
   TR_GlobalRegisterNumber _firstGlobalFPR;
   TR_GlobalRegisterNumber _lastGlobalFPR;
   TR_GlobalRegisterNumber _firstOverlappedGlobalFPR;
   TR_GlobalRegisterNumber _lastOverlappedGlobalFPR;
   TR_GlobalRegisterNumber _firstGlobalAR;
   TR_GlobalRegisterNumber _lastGlobalAR;
   TR_GlobalRegisterNumber _last8BitGlobalGPR;
   TR_GlobalRegisterNumber _firstGlobalVRF;
   TR_GlobalRegisterNumber _lastGlobalVRF;
   TR_GlobalRegisterNumber _firstOverlappedGlobalVRF;
   TR_GlobalRegisterNumber _lastOverlappedGlobalVRF;
   TR_GlobalRegisterNumber _overlapOffsetBetweenFPRandVRFgrns;
   uint8_t _supportedLiveRegisterKinds;
   uint8_t _globalGPRPartitionLimit;
   uint8_t _globalFPRPartitionLimit;
   flags16_t _enabledFlags;

   bool _afterRA;

   // MOVE TO J9 Z CodeGenerator
   // isTemporaryBased storageReferences just have a symRef but some other routines expect a node so use the below to fill in this symRef on this node
   TR::Node *_dummyTempStorageRefNode;

   public:
   static TR_TreeEvaluatorFunctionPointer _nodeToInstrEvaluators[];

   protected:

#ifdef DEBUG
   static int _totalNumSpilledRegisters;         // For collecting statistics on spilling
   static int _totalNumRematerializedConstants;
   static int _totalNumRematerializedLocals;
   static int _totalNumRematerializedStatics;
   static int _totalNumRematerializedIndirects;
   static int _totalNumRematerializedAddresses;
   static int _totalNumRematerializedXMMRs;
#endif

   bool _disableInternalPointers;

   void addMonClass(TR::Node* monNode, TR_OpaqueClassBlock* clazz);

   TR::RegisterIterator *_gpRegisterIterator;
   TR::RegisterIterator *_fpRegisterIterator;
   TR_RegisterAssignmentFlags _regAssignFlags;

   uint32_t _preJitMethodEntrySize;
   uint32_t _jitMethodEntryPaddingSize;

   TR::Instruction *_lastInstructionBeforeCurrentEvaluationTreeTop;

   TR_Stack<TR::MemoryReference *> _stackOfMemoryReferencesCreatedDuringEvaluation;

   uint8_t *emitSnippets();

   void addAllocatedRegisterPair(TR::RegisterPair * temp);
   void addAllocatedRegister(TR::Register * temp);

   private:

   TR_BitVector *_blocksWithCalls;
   void computeBlocksWithCalls();

   TR::CodeCache * _codeCache;
   bool _committedToCodeCache;

   TR_Stack<TR::Node *> _stackOfArtificiallyInflatedNodes;

   CS2::HashTable<TR::Symbol*, TR::DataType, TR::Allocator> _symbolDataTypeMap;
   };

}

#endif
