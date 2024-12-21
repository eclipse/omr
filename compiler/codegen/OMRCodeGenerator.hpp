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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <map>
#include "codegen/CodeGenPhase.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/HashTab.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/TRlist.hpp"
#include "infra/Random.hpp"
#include "infra/Stack.hpp"
#include "optimizer/Dominators.hpp"
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
namespace TR { class RegisterCandidate; }
namespace TR { class RegisterCandidates; }
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
namespace TR { class ObjectFormat; }
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

namespace TR
   {
   enum ExternalRelocationPositionRequest
      {
      ExternalRelocationAtFront,
      ExternalRelocationAtBack,
      };
   }


namespace OMR
{

typedef TR::Register *(* TreeEvaluatorFunctionPointer)(TR::Node *node, TR::CodeGenerator *cg);

class TreeEvaluatorFunctionPointerTable
   {
   private:
   static TreeEvaluatorFunctionPointer table[];

   static void checkTableSize();

   public:
   TreeEvaluatorFunctionPointer& operator[] (TR::ILOpCode opcode)
      {
      return table[opcode.getTableIndex()];
      }

   TreeEvaluatorFunctionPointer& operator[] (TR::VectorOperation operation)
      {
      return table[TR::ILOpCode::getTableIndex(operation)];
      }
   };

class OMR_EXTENSIBLE CodeGenerator
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

   /**
    * @brief Constructor
    *
    * @param[in] comp \c TR::Compilation object
    */
   CodeGenerator(TR::Compilation *comp);

public:

   TR_ALLOC(TR_Memory::CodeGenerator)

   /**
    * @brief Factory function to create and initialize a new \c TR::CodeGenerator object.
    *
    * @param[in] comp \c TR::Compilation object
    *
    * @return An allocated and initialized \c TR::CodeGenerator object
    */
   static TR::CodeGenerator *create(TR::Compilation *comp);

   /**
    * @brief Initialize a \c TR::CodeGenerator object
    */
   void initialize();

   inline TR::CodeGenerator *self();

   TR_StackMemory trStackMemory();
   TR_Memory *trMemory() { return _trMemory; }
   TR_HeapMemory trHeapMemory() { return _trMemory; }
   TR::Machine *machine() { return _machine; }
   TR::Compilation *comp() { return _compilation; }
   TR_FrontEnd *fe();
   TR_Debug *getDebug();

   void uncommonCallConstNodes();

   void preLowerTrees();
   void postLowerTrees();

   TR::TreeTop *lowerTree(TR::Node *root, TR::TreeTop *tt);
   void lowerTrees();
   void lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   void lowerTreesPreTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);
   void lowerTreesPostTreeTopVisit(TR::TreeTop *tt, vcount_t visitCount);

   void lowerTreesPreChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);
   void lowerTreesPostChildrenVisit(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);

   void lowerTreesPropagateBlockToNode(TR::Node *node);

   /**
    * @brief Inserts goto into the last block if necessary
    */
   void insertGotoIntoLastBlock(TR::Block *lastBlock);

   /**
    * @brief Finds last warm block and inserts necessary gotos
    *        for splitting code into warm and cold
    */
   void prepareLastWarmBlockForCodeSplitting();

   void setUpForInstructionSelection();
   void doInstructionSelection();
   void createStackAtlas();

   void beginInstructionSelection() {}
   void endInstructionSelection() {}

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

   TR::Instruction *getLastWarmInstruction() {return _lastWarmInstruction;}
   TR::Instruction *setLastWarmInstruction(TR::Instruction *instr) {return (_lastWarmInstruction = instr);}

   TR::TreeTop *getCurrentEvaluationTreeTop() {return _currentEvaluationTreeTop;}
   TR::TreeTop *setCurrentEvaluationTreeTop(TR::TreeTop *tt) {return (_currentEvaluationTreeTop = tt);}

   TR::Block *getCurrentEvaluationBlock() {return _currentEvaluationBlock;}
   TR::Block *setCurrentEvaluationBlock(TR::Block *tt) {return (_currentEvaluationBlock = tt);}

   TR::Instruction *getImplicitExceptionPoint() {return _implicitExceptionPoint;}
   TR::Instruction *setImplicitExceptionPoint(TR::Instruction *p) {return (_implicitExceptionPoint = p);}

   bool mustGenerateSwitchToInterpreterPrePrologue() { return false; }
   bool buildInterpreterEntryPoint() { return false; }
   void generateCatchBlockBBStartPrologue(TR::Node *node, TR::Instruction *fenceInstruction) { return; }
   bool supportsUnneededLabelRemoval() { return true; }

   TR_HasRandomGenerator randomizer;

   /** \brief
    *     Determines whether the code generator supports inlining intrinsics for \p symbol.
    *
    *  \param symbol
    *     The symbol which to check.
    *
    *  \return
    *     \c true if intrinsics on \p symbol are supported; \c false otherwise.
    */
   bool supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol)
      {
      return false;
      }

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
   void doRegisterAssignment(TR_RegisterKinds kindsToAssign);
   void doBinaryEncoding();
   bool hasComplexAddressingMode() { return false; }
   void removeUnusedLocals();

   void identifyUnneededByteConvNodes(TR::Node*, TR::TreeTop *, vcount_t, TR::DataType);
   void identifyUnneededByteConvNodes();

   TR::CodeGenPhase& getCodeGeneratorPhase() {return _codeGenPhase;}

   void prepareNodeForInstructionSelection(TR::Node*node);
   void remapGCIndicesInInternalPtrFormat();
   void processRelocations();

   void findAndFixCommonedReferences();
   void findCommonedReferences(TR::Node*node, TR::TreeTop *treeTop);
   void processReference(TR::Node*reference, TR::Node*parent, TR::TreeTop *treeTop);
   void spillLiveReferencesToTemps(TR::TreeTop *insertionTree, std::list<TR::SymbolReference*, TR::typed_allocator<TR::SymbolReference*, TR::Allocator> >::iterator firstAvailableSpillTemp);
   void needSpillTemp(TR_LiveReference * cursor, TR::Node*parent, TR::TreeTop *treeTop);

   void expandInstructions() {}

   friend void OMR::CodeGenPhase::performEmitSnippetsPhase(TR::CodeGenerator*, TR::CodeGenPhase *);
   friend void OMR::CodeGenPhase::performCleanUpFlagsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase);

   // --------------------------------------------------------------------------
   // Hardware profiling
   //
   void createHWPRecords() {}

   // --------------------------------------------------------------------------
   // Tree evaluation
   //
   static TreeEvaluatorFunctionPointerTable getTreeEvaluatorTable() {return _nodeToInstrEvaluators;}

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

   void incRefCountForOpaquePseudoRegister(TR::Node * node) {}

   void startUsingRegister(TR::Register *reg);
   void stopUsingRegister(TR::Register *reg);

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

   /**
    * @brief Returns if an IL OpCode is supported by current CodeGen
    *
    * @param op The IL OpCode being checked.
    *
    * @return True if the IL OpCode is supported otherwise false.
    */
   static bool isILOpCodeSupported(TR::ILOpCodes op);

   /**
    * @brief Returns the corresponding IL OpCode for an intrinsic method
    *
    * This query maps an intrinsic method to an IL OpCode, with the requirement that
    * the method's child(ren) corresponds to the OpCode's child(ren) exactly.
    * It is usually used by the IL Gen transforming the intrinsic method to IL OpCode
    * so that it can leverage existing framework for better optimization.
    *
    * @param method The intrinsic method being checked.
    *
    * @return The corresponding IL OpCode for the intrinsic method.
    */
   static TR::ILOpCodes ilOpCodeForIntrinsicMethod(TR::RecognizedMethod method) { return TR::BadILOp; }

   /**
    * @brief Returns if an intrinsic method is supported by current CodeGen
    *
    * @param method The intrinsic method being checked.
    *
    * @return True if the intrinsic method is supported otherwise false.
    */
   static inline bool isIntrinsicMethodSupported(TR::RecognizedMethod method);


   TR::Recompilation *allocateRecompilationInfo() { return NULL; }

   /**
    * @brief This determines if it is necessary to emit a prefetch instruction.
    *        If so, it also emits the prefetch instruction.
    *
    * @param node The node being evaluated.
    * @param targetRegister A register holding the address where the prefetch location is generated from.
    */
   void insertPrefetchIfNecessary(TR::Node *node, TR::Register *targetRegister);

   // --------------------------------------------------------------------------
   // Capabilities
   //
   bool supports32bitAiadd() {return true;}

   // --------------------------------------------------------------------------
   // Z only
   //

   bool AddArtificiallyInflatedNodeToStack(TR::Node* n);

   // --------------------------------------------------------------------------
   // P only
   //
   intptr_t hiValue(intptr_t address);

   // --------------------------------------------------------------------------
   // Lower trees
   //
   void rematerializeCmpUnderSelect(TR::Node*node);
   bool yankIndexScalingOp() {return false;}

   void cleanupFlags(TR::Node*node);

   bool shouldYankCompressedRefs() { return false; }
   bool materializesHeapBase() { return true; }
   bool canFoldLargeOffsetInAddressing() { return false; }

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

   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond){ return cursor; }
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond){ return cursor; }
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm){ return cursor; }
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm){ return cursor; }

   // --------------------------------------------------------------------------
   // Linkage
   //
   void initializeLinkage();
   TR::Linkage *createLinkage(TR_LinkageConventions lc);
   TR::Linkage *createLinkageForCompilation();

   TR::Linkage *getLinkage() {return _bodyLinkage;}
   TR::Linkage *setLinkage(TR::Linkage * l) {return (_bodyLinkage = l);}

   TR::Linkage *getLinkage(TR_LinkageConventions lc);
   TR::Linkage *setLinkage(TR_LinkageConventions lc, TR::Linkage * l) {return _linkages[lc] = l;}

   // --------------------------------------------------------------------------
   // Optimizer, code generator capabilities
   //
   int32_t getPreferredLoopUnrollFactor() {return -1;}

   /**
    * @brief Answers whether the provided recognized method should be inlined by an
    *        inliner optimization.
    * @param method : the recognized method to consider
    * @return true if inlining should be suppressed; false otherwise
    */
   bool suppressInliningOfRecognizedMethod(TR::RecognizedMethod method);

   /**
    * @brief Answers whether iabs/labs evaluators are available or not
    * @return true if iabs/labs evaluators are available
    */
   bool supportsIntAbs() { return true; }
   /**
    * @brief Answers whether fabs/dabs evaluators are available or not
    * @return true if fabs/dabs evaluators are available
    */
   bool supportsFPAbs() { return true; }

   // --------------------------------------------------------------------------
   // Optimizer, not code generator
   //
   bool getGRACompleted() { return _flags4.testAny(GRACompleted); }
   void setGRACompleted() { _flags4.set(GRACompleted); }

   bool getSupportsProfiledInlining() { return _flags4.testAny(SupportsProfiledInlining);}
   void setSupportsProfiledInlining() { _flags4.set(SupportsProfiledInlining);}
   bool supportsInliningOfIsInstance() {return false;}
   bool supportsPassThroughCopyToNewVirtualRegister() { return false; }

   uint8_t getSizeOfCombinedBuffer() {return 0;}

   bool doRematerialization() {return false;}

   // --------------------------------------------------------------------------
   // Architecture, not code generator
   //
   int16_t getMinShortForLongCompareNarrower() { return SHRT_MIN; }
   int8_t getMinByteForLongCompareNarrower() { return SCHAR_MIN; }

   bool branchesAreExpensive() { return true; }
   bool opCodeIsNoOp(TR::ILOpCode &opCode);
   bool opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode) {return false;}

   bool supportsSinglePrecisionSQRT() {return false;}
   bool supportsFusedMultiplyAdd() {return false;}
   bool supportsNegativeFusedMultiplyAdd() {return false;}

   bool supportsComplexAddressing() {return false;}
   bool canBeAffectedByStoreTagStalls() { return false; }

   bool isMaterialized(TR::Node *);
   bool shouldValueBeInACommonedNode(int64_t) { return false; }
   bool materializesLargeConstants() { return false; }

   bool canUseImmedInstruction(int64_t v) {return false;}
   bool needsNormalizationBeforeShifts() { return false; }

   uint32_t getNumberBytesReadInaccessible() { return _numberBytesReadInaccessible; }
   uint32_t getNumberBytesWriteInaccessible() { return _numberBytesWriteInaccessible; }

   bool codegenSupportsUnsignedIntegerDivide() {return false;}
   bool mulDecompositionCostIsJustified(int numOfOperations, char bitPosition[], char operationType[], int64_t value);

   bool codegenSupportsLoadlessBNDCheck() {return false;}

   // called to determine if multiply decomposition exists in platform codegen so that codegen sequences are used
   // instead of the IL transformed multiplies
   bool codegenMulDecomposition(int64_t multiplier) {return false;}

   // --------------------------------------------------------------------------
   // FrontEnd, not code generator
   //
   bool getSupportsNewObjectAlignment() { return false; }
   bool getSupportsTenuredObjectAlignment() { return false; }
   bool isObjectOfSizeWorthAligning(uint32_t size) { return false; }

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
   bool internalPointerSupportImplemented() {return false;}
   bool supportsInternalPointers();

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
   uint32_t getRegisterMapInfoBitsMask() {return 0;}

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
   uint32_t getEstimatedWarmLength()           {return _estimatedWarmCodeLength;}
   uint32_t setEstimatedWarmLength(uint32_t l) {return (_estimatedWarmCodeLength = l);}

   uint32_t getEstimatedColdLength()           {return _estimatedColdCodeLength;}
   uint32_t setEstimatedColdLength(uint32_t l) {return (_estimatedColdCodeLength = l);}

   uint32_t getEstimatedCodeLength()           {return _estimatedCodeLength;}
   uint32_t setEstimatedCodeLength(uint32_t l) {return (_estimatedCodeLength = l);}

   uint8_t *getBinaryBufferStart()           {return _binaryBufferStart;}
   uint8_t *setBinaryBufferStart(uint8_t *b) {return (_binaryBufferStart = b);}

   uint8_t *getCodeStart();
   uint8_t *getCodeEnd()                  {return _binaryBufferCursor;}
   uint32_t getCodeLength();

   uint8_t *getWarmCodeEnd()              {return _coldCodeStart ? _warmCodeEnd : _binaryBufferCursor;}
   uint8_t *setWarmCodeEnd(uint8_t *c)    {return (_warmCodeEnd = c);}
   uint8_t *getColdCodeStart()            {return _coldCodeStart;}
   uint8_t *setColdCodeStart(uint8_t *c)  {return (_coldCodeStart = c);}

   uint32_t getWarmCodeLength() {return (uint32_t)(getWarmCodeEnd() - getCodeStart());} // cast explicitly
   uint32_t getColdCodeLength() {return (uint32_t)(_coldCodeStart ? getCodeEnd() - getColdCodeStart() : 0);} // cast explicitly

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

   /** \brief
    *     Determines whether the code generator supports or allows JIT-to-JIT method entry point alignment.
    */
   bool supportsJitMethodEntryAlignment();

   /** \brief
    *     Determines the byte boundary at which to align the JIT-to-JIT method entry point. If the boundary is
    *     specified to be \c x and the JIT-to-JIT method entry point to be \c y then <c>y & (x - 1) == 0</c>.
    */
   uint32_t getJitMethodEntryAlignmentBoundary();

   uint32_t getJitMethodEntryPaddingSize() {return _jitMethodEntryPaddingSize;}
   uint32_t setJitMethodEntryPaddingSize(uint32_t s) {return (_jitMethodEntryPaddingSize = s);}

   // --------------------------------------------------------------------------
   // Code cache
   //
   TR::CodeCache * getCodeCache() { return _codeCache; }
   void  setCodeCache(TR::CodeCache * codeCache) { _codeCache = codeCache; }
   void  reserveCodeCache();

   /**
    * \brief Allocates code memory of the specified size in the specified area of
    *        the code cache.  The compilation will fail if unsuccessful.
    *
    * \param[in]  codeSizeInBytes : the number of bytes to allocate
    * \param[in]  isCold : whether the allocation should be done in the cold area or not
    * \param[in]  isMethodHeaderNeeded : boolean indicating whether space for a
    *                method header must be allocated
    *
    * \return address of the allocated code (if allocated)
    */
   uint8_t *allocateCodeMemory(uint32_t codeSizeInBytes, bool isCold, bool isMethodHeaderNeeded=true);

   /**
    * \brief Allocates code memory of the specified size in the specified area of
    *        the code cache.  The compilation will fail if unsuccessful.
    *
    * \param[in]  warmCodeSizeInBytes : the number of bytes to allocate in the warm area
    * \param[in]  coldCodeSizeInBytes : the number of bytes to allocate in the cold area
    * \param[out] coldCode : address of the cold code (if allocated)
    * \param[in]  isMethodHeaderNeeded : boolean indicating whether space for a
    *                method header must be allocated
    *
    * \return address of the allocated warm code (if allocated)
    */
   uint8_t *allocateCodeMemory(
      uint32_t warmCodeSizeInBytes,
      uint32_t coldCodeSizeInBytes,
      uint8_t **coldCode,
      bool isMethodHeaderNeeded=true);

   /**
    * \brief Allocates code memory of the specified size in the specified area of
    *        the code cache.  The compilation will fail if unsuccessful.  This function
    *        provides a means of specialization in the allocation process for downstream
    *        consumers of this API.
    *
    * \param[in]  warmCodeSizeInBytes : the number of bytes to allocate in the warm area
    * \param[in]  coldCodeSizeInBytes : the number of bytes to allocate in the cold area
    * \param[out] coldCode : address of the cold code (if allocated)
    * \param[in]  isMethodHeaderNeeded : boolean indicating whether space for a
    *                method header must be allocated
    *
    * \return address of the allocated warm code (if allocated)
    */
   uint8_t *allocateCodeMemoryInner(
      uint32_t warmCodeSizeInBytes,
      uint32_t coldCodeSizeInBytes,
      uint8_t **coldCode,
      bool isMethodHeaderNeeded);

   /**
    * \brief Trim the size of code memory required by this method to match the
    *        actual code length required, allowing the reclaimed memory to be
    *        reused.  This is needed when the conservative length estimate
    *        exceeds the actual memory requirement.
    */
   void trimCodeMemoryToActualSize();

   void  registerAssumptions() {}

   static void syncCode(uint8_t *codeStart, uint32_t codeSize);

   void commitToCodeCache() { _committedToCodeCache = true; }
   bool committedToCodeCache() { return _committedToCodeCache; }

   /**
    * \brief Answers whether the CodeCache in the current compilation has been switched
    *        from the originally assigned CodeCache.
    *
    * \return true if the CodeCache has been switched; false otherwise.
    */
   bool hasCodeCacheSwitched() const { return _codeCacheSwitched; }

   /**
    * \brief Updates the state of whether the CodeCache has been switched.
    *
    * \param[in] s : bool indicating whether the CodeCache has been switched
    */
   void setCodeCacheSwitched(bool s) { _codeCacheSwitched = s; }

   /**
    * \brief Changes the current CodeCache to the provided CodeCache.
    *
    * \param[in] newCodeCache : the CodeCache to switch to
    */
   void switchCodeCacheTo(TR::CodeCache *newCodeCache);

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

   // --------------------------------------------------------------------------
   // GRA
   //
   void addSymbolAndDataTypeToMap(TR::Symbol *symbol, TR::DataType dt);
   TR::DataType getDataTypeFromSymbolMap(TR::Symbol *symbol);

   bool prepareForGRA();

   uint32_t getGlobalRegister(TR_GlobalRegisterNumber n) {return _globalRegisterTable[n];}
   uint32_t *setGlobalRegisterTable(uint32_t *p) {return (_globalRegisterTable = p);}

   TR_GlobalRegisterNumber getGlobalRegisterNumber(uint32_t realReg) { return -1; }

   TR_GlobalRegisterNumber getFirstGlobalGPR() {return 0;}
   TR_GlobalRegisterNumber getLastGlobalGPR()  {return _lastGlobalGPR;}
   TR_GlobalRegisterNumber setLastGlobalGPR(TR_GlobalRegisterNumber n) {return (_lastGlobalGPR = n);}

   TR_GlobalRegisterNumber getFirstGlobalFPR() {return _lastGlobalGPR + 1;}
   TR_GlobalRegisterNumber setFirstGlobalFPR(TR_GlobalRegisterNumber n) {return (_firstGlobalFPR = n);}
   TR_GlobalRegisterNumber getLastGlobalFPR() {return _lastGlobalFPR;}
   TR_GlobalRegisterNumber setLastGlobalFPR(TR_GlobalRegisterNumber n)  {return (_lastGlobalFPR = n);}

   TR_GlobalRegisterNumber getFirstOverlappedGlobalFPR()                          { return _firstOverlappedGlobalFPR    ;}
   TR_GlobalRegisterNumber setFirstOverlappedGlobalFPR(TR_GlobalRegisterNumber n) { return _firstOverlappedGlobalFPR = n;}
   TR_GlobalRegisterNumber getLastOverlappedGlobalFPR()                           { return _lastOverlappedGlobalFPR     ;}
   TR_GlobalRegisterNumber setLastOverlappedGlobalFPR(TR_GlobalRegisterNumber n)  { return _lastOverlappedGlobalFPR = n ;}

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

   uint16_t getNumberOfGlobalGPRs() {return _lastGlobalGPR + 1;}
   uint16_t getNumberOfGlobalFPRs() {return _lastGlobalFPR - _lastGlobalGPR;}
   uint16_t getNumberOfGlobalVRFs() {return _lastGlobalVRF - _firstGlobalVRF;}

   uint8_t getGlobalGPRPartitionLimit() {return _globalGPRPartitionLimit;}
   uint8_t setGlobalGPRPartitionLimit(uint8_t l) {return (_globalGPRPartitionLimit = l);}

   uint8_t getGlobalFPRPartitionLimit() {return _globalFPRPartitionLimit;}
   uint8_t setGlobalFPRPartitionLimit(uint8_t l) {return (_globalFPRPartitionLimit = l);}

   bool isGlobalGPR(TR_GlobalRegisterNumber n) {return n <= _lastGlobalGPR;}

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

   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type){ return -1; }
   TR_BitVector *getGlobalGPRsPreservedAcrossCalls(){ return NULL; }
   TR_BitVector *getGlobalFPRsPreservedAcrossCalls(){ return NULL; }

   int32_t getFirstBit(TR_BitVector &bv);
   TR_GlobalRegisterNumber pickRegister(TR::RegisterCandidate *, TR::Block * *, TR_BitVector & availableRegisters, TR_GlobalRegisterNumber & highRegisterNumber, TR_LinkHead<TR::RegisterCandidate> *candidates);
   TR::RegisterCandidate *findCoalescenceForRegisterCopy(TR::Node *node, TR::RegisterCandidate *rc, bool *isUnpreferred);
   TR_GlobalRegisterNumber findCoalescenceRegisterForParameter(TR::Node *callNode, TR::RegisterCandidate *rc, uint32_t childIndex, bool *isUnpreferred);
   TR::RegisterCandidate *findUsedCandidate(TR::Node *node, TR::RegisterCandidate *rc, TR_BitVector *visitedNodes);

   bool allowGlobalRegisterAcrossBranch(TR::RegisterCandidate *, TR::Node * branchNode);
   void removeUnavailableRegisters(TR::RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters) {}
   void setUnavailableRegistersUsage(TR_Array<TR_BitVector>  & liveOnEntryUsage, TR_Array<TR_BitVector>   & liveOnExitUsage) {}

   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *) { return INT_MAX; }
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *) { return INT_MAX; }
   int32_t getMaximumNumberOfVRFsAllowedAcrossEdge(TR::Node *) { return INT_MAX; }
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *block);
   int32_t getMaximumNumbersOfAssignableGPRs() { return INT_MAX; }
   int32_t getMaximumNumbersOfAssignableFPRs() { return INT_MAX; }
   int32_t getMaximumNumbersOfAssignableVRs()  { return INT_MAX; }
   virtual bool willBeEvaluatedAsCallByCodeGen(TR::Node *node, TR::Compilation *comp){ return true;}
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber, TR::DataType) { return true; }

   bool areAssignableGPRsScarce();

   TR_Array<TR::Register *>& getRegisterArray() {return _registerArray;}

   /**
   * \brief Checks if global register allocation is supported for the given node
   *
   * \return true if the node can support GRA, otherwise false
   */
   bool considerTypeForGRA(TR::Node *node) {return true;}

   /**
   * \brief Checks if global register allocation is supported for the given type
   *
   * \return true if the data-type can support GRA, otherwise false
   */
   bool considerTypeForGRA(TR::DataType dt) {return true;}

   /**
   * \brief Checks if global register allocation is supported for the symbol reference
   *
   * \return true if the symbol can support GRA, otherwise false
   */
   bool considerTypeForGRA(TR::SymbolReference *symRef) {return true;}

   void enableLiteralPoolRegisterForGRA () {}
   bool excludeInvariantsFromGRAEnabled() { return false; }

   TR_BitVector *getBlocksWithCalls();

   // --------------------------------------------------------------------------
   // Debug
   //
   TR::RegisterIterator *getGPRegisterIterator() {return _gpRegisterIterator;}
   TR::RegisterIterator *setGPRegisterIterator(TR::RegisterIterator *iter) {return (_gpRegisterIterator = iter);}

   TR::RegisterIterator *getFPRegisterIterator() {return _fpRegisterIterator;}
   TR::RegisterIterator *getVMRegisterIterator() {return _vmRegisterIterator;}
   TR::RegisterIterator *setFPRegisterIterator(TR::RegisterIterator *iter) {return (_fpRegisterIterator = iter);}
   TR::RegisterIterator *setVMRegisterIterator(TR::RegisterIterator *iter) {return (_vmRegisterIterator = iter);}

   // X86 only
   uint32_t estimateBinaryLength(TR::MemoryReference *) { return 0; }

#ifdef DEBUG
   static void shutdown(TR_FrontEnd *fe, TR::FILE *logFile);
#endif

   void dumpDataSnippets(TR::FILE *outFile) {}

   // --------------------------------------------------------------------------
   // Register assignment tracing
   //
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
   TR::list<TR_BackingStore*>& getSpill32FreeList() {return _spill32FreeList;}
   TR::list<TR_BackingStore*>& getSpill64FreeList() {return _spill64FreeList;}
   TR::list<TR_BackingStore*>& getInternalPointerSpillFreeList() {return _internalPointerSpillFreeList;}
   TR::list<TR_BackingStore*>& getCollectedSpillList() {return _collectedSpillList;}
   TR::list<TR_BackingStore*>& getAllSpillList() {return _allSpillList;}

   /**
    * @brief Returns the list of registers which is assigned first time in OOL cold path
    *
    * @return the list of registers which is assigned first time in OOL cold path
    */
   TR::list<TR::Register*> *getFirstTimeLiveOOLRegisterList() {return _firstTimeLiveOOLRegisterList;}

   /**
    * @brief Sets the list of registers which is assigned first time in OOL cold path
    *
    * @param r : the list of registers which is assigned first time in OOL cold path
    * @return the list of registers
    */
   TR::list<TR::Register*> *setFirstTimeLiveOOLRegisterList(TR::list<TR::Register*> *r) {return _firstTimeLiveOOLRegisterList = r;}

   TR::list<TR::Register*> *getSpilledRegisterList() {return _spilledRegisterList;}
   TR::list<TR::Register*> *setSpilledRegisterList(TR::list<TR::Register*> *r) {return _spilledRegisterList = r;}

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

   TR::RegisterPair * allocateRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);
   TR::RegisterPair * allocateSinglePrecisionRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);
   TR::RegisterPair * allocateFloatingPointRegisterPair(TR::Register * lo = 0, TR::Register * ho = 0);



   TR::SymbolReference * allocateLocalTemp(TR::DataType dt = TR::Int32, bool isInternalPointer = false);

   // --------------------------------------------------------------------------
   // Relocations
   //
   TR::list<TR::Relocation*>& getRelocationList() {return _relocationList;}
   TR::list<TR::Relocation*>& getExternalRelocationList() {return _externalRelocationList;}
   TR::list<TR::StaticRelocation>& getStaticRelocations() { return _staticRelocationList; }

   void addRelocation(TR::Relocation *r);
   void addExternalRelocation(TR::Relocation *r, const char *generatingFileName, uintptr_t generatingLineNumber, TR::Node *node, TR::ExternalRelocationPositionRequest where = TR::ExternalRelocationAtBack);
   void addExternalRelocation(TR::Relocation *r, TR::RelocationDebugInfo *info, TR::ExternalRelocationPositionRequest where = TR::ExternalRelocationAtBack);
   void addStaticRelocation(const TR::StaticRelocation &relocation);

   void addProjectSpecializedRelocation(uint8_t *location,
                                          uint8_t *target,
                                          uint8_t *target2,
                                          TR_ExternalRelocationTargetKind kind,
                                          const char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}
   void addProjectSpecializedPairRelocation(uint8_t *location1,
                                          uint8_t *location2,
                                          uint8_t *target,
                                          TR_ExternalRelocationTargetKind kind,
                                          const char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}
   void addProjectSpecializedRelocation(TR::Instruction *instr,
                                          uint8_t *target,
                                          uint8_t *target2,
                                          TR_ExternalRelocationTargetKind kind,
                                          const char *generatingFileName,
                                          uintptr_t generatingLineNumber,
                                          TR::Node *node) {}

   void apply8BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label);
   void apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label, bool isCheckDisp = true);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol * label,int8_t d, bool isInstrOffset = false);
   void apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   bool supportsMergingGuards();


   bool needClassAndMethodPointerRelocations();
   bool needRelocationsForStatics();
   bool needRelocationsForBodyInfoData();
   bool needRelocationsForPersistentInfoData();
   bool needRelocationsForPersistentProfileInfoData();
   bool needRelocationsForLookupEvaluationData();
   bool needRelocationsForCurrentMethodPC();

   // This query can be used if we need to decide whether data represented by TR_HelperAddress or TR_AbsoluteHelperAddress
   // relocation type needs a relocation record.
   bool needRelocationsForHelpers();

   // --------------------------------------------------------------------------
   // Snippets
   //
   int32_t setEstimatedLocationsForSnippetLabels(int32_t estimatedSnippetStart);
   TR::list<TR::Snippet*>& getSnippetList() {return _snippetList;}
   void addSnippet(TR::Snippet *s);

   // Local snippet sharing facility: most RISC platforms can make use of it. The platform
   // specific code generators should override isSnippetMatched if they choose to use it.
   TR::LabelSymbol * lookUpSnippet(int32_t snippetKind, TR::SymbolReference *symRef);
   bool isSnippetMatched(TR::Snippet *snippet, int32_t snippetKind, TR::SymbolReference *symRef) {return false;}

   // called to emit any constant data snippets.  The platform specific code generators
   // should override these methods if they use constant data snippets.
   //
   void emitDataSnippets() {}
   bool hasDataSnippets() {return false;}
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart) {return 0;}

   TR::list<TR::Snippet*> *getSnippetsToBePatchedOnClassUnload() { return &_snippetsToBePatchedOnClassUnload; }

   TR::list<TR::Snippet*> *getMethodSnippetsToBePatchedOnClassUnload() { return &_methodSnippetsToBePatchedOnClassUnload; }

   TR::list<TR::Snippet*> *getSnippetsToBePatchedOnClassRedefinition() { return &_snippetsToBePatchedOnClassRedefinition; }

   /**
   * \brief Calculates total size of all code snippets
   *
   * \return total size of all code snippets
   */
   uint32_t getCodeSnippetsSize();

   /**
   * \brief Calculates total size of all data snippets
   *
   * \return total size of all data snippets
   */
   uint32_t getDataSnippetsSize() { return 0; }

   /**
   * \brief Calculates total size of all out of line code
   *
   * \return total size of all out of line code
   */
   uint32_t getOutOfLineCodeSize() { return 0; }

   struct MethodStats
      {
      uint32_t codeSize;
      uint32_t warmBlocks;
      uint32_t coldBlocks;
      uint32_t prologue;
      uint32_t snippets;
      uint32_t outOfLine;
      uint32_t unaccounted;
      uint32_t blocksInColdCache;
      uint32_t overestimateInColdCache;
      };

   /**
   * \brief Fills in MethodStats structure with footprint data
   */
   void getMethodStats(MethodStats &methodStats);

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

   private:

   int32_t _internalControlFlowNestingDepth;
   int32_t _internalControlFlowSafeNestingDepth;

   public:

   /**
    * @brief initializeLinkageInfo
    *
    * The linkage info word is emitted to be right before the startPC. In OMR, there is no
    * interpreter entry point. Therefore, after the Linkage Info word is emitted, the next
    * instruction is the compiled method start PC. This means that when the code in
    * doBinaryEncoding tries to determine the offset of the interpreter entry point from the
    * start PC, it will always compute 0. Since the Linkage Info word is emitted as 32-bit
    * word of 0s, there is nothing to initialize.
    */
   uint32_t initializeLinkageInfo(void *linkageInfo) { return 0; }

   int32_t internalControlFlowNestingDepth() {return _internalControlFlowNestingDepth;}
   int32_t internalControlFlowSafeNestingDepth() { return _internalControlFlowSafeNestingDepth; }
   void incInternalControlFlowNestingDepth() {_internalControlFlowNestingDepth++;}
   void decInternalControlFlowNestingDepth() {_internalControlFlowNestingDepth--;}
   bool insideInternalControlFlow() {return (_internalControlFlowNestingDepth > _internalControlFlowSafeNestingDepth);}
   void setInternalControlFlowNestingDepth(int32_t depth) { _internalControlFlowNestingDepth = depth; }
   void setInternalControlFlowSafeNestingDepth(int32_t safeDepth) { _internalControlFlowSafeNestingDepth = safeDepth; }

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
   TR_RegisterKinds prepareRegistersForAssignment();
   void addToUnlatchedRegisterList(TR::RealRegister *reg);
   void freeUnlatchedRegisters();

   // --------------------------------------------------------------------------
   // Listing
   //
   uint32_t _indentation;


   // --------------------------------------------------------------------------
   // Code patching
   //
   // Used to find out whether there is an appropriate instruction space as vgnop space
   int32_t sizeOfInstructionToBePatched(TR::Instruction *vgnop);
   int32_t sizeOfInstructionToBePatchedHCRGuard(TR::Instruction *vgnop);
   // Used to find which instruction is an appropriate instruction space as vgnop space
   TR::Instruction* getInstructionToBePatched(TR::Instruction *vgnop);
   // Used to find the guard instruction where a given guard will actually patch
   // currently can only return a value other than vgnop for HCR guards
   TR::Instruction* getVirtualGuardForPatching(TR::Instruction *vgnop);

   void jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched) {}
   void jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched) {}
   void jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false) {}
   void jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved = false) {}
   void jitAddUnresolvedAddressMaterializationToPatchOnClassRedefinition(void *firstInstruction) {} //J9

   // --------------------------------------------------------------------------
   // Unclassified
   //

   TR::Instruction *generateNop(TR::Node *node, TR::Instruction *instruction=0, TR_NOPKind nopKind=TR_NOPStandard);
   bool isOutOfLineHotPath() { TR_ASSERT(0, "isOutOfLineHotPath is only implemented for 390 and ppc"); return false;}

   //Rather confusingly not used -only- in BCD related codegen.
   //... has leaked into non-BCD code.
   bool traceBCDCodeGen();
   void traceBCDEntry(const char *str, TR::Node *node);
   void traceBCDExit(const char *str, TR::Node *node);

   TR_BitVector *getLiveButMaybeUnreferencedLocals() {return _liveButMaybeUnreferencedLocals;}
   TR_BitVector *setLiveButMaybeUnreferencedLocals(TR_BitVector *v) {return (_liveButMaybeUnreferencedLocals = v);}

   TR::AheadOfTimeCompile *getAheadOfTimeCompile() {return _aheadOfTimeCompile;}
   TR::AheadOfTimeCompile *setAheadOfTimeCompile(TR::AheadOfTimeCompile *p) {return (_aheadOfTimeCompile = p);}

   // J9, X86
   bool canTransformUnsafeCopyToArrayCopy() { return false; }
   bool canTransformUnsafeSetMemory() { return false; }

   bool canNullChkBeImplicit(TR::Node *);
   bool canNullChkBeImplicit(TR::Node *, bool doChecks);

   bool IsInMemoryType(TR::DataType type) { return false; }

   bool nodeMayCauseException(TR::Node *node) { return false; }

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
   int32_t arrayTranslateTableRequiresAlignment(bool isByteSource, bool isByteTarget)  { return 0; }

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
   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget);

   // TO TransformUtil.  Make platform specific
   int32_t arrayTranslateAndTestMinimumNumberOfIterations();
   static int32_t defaultArrayTranslateMinimumNumberOfIterations(const char *methodName);
   static bool useOldArrayTranslateMinimumNumberOfIterations()
      {
      static bool useOldValue = feGetEnv("TR_oldArrayTranslateMinimumNumberOfIterations") != NULL;
      return useOldValue;
      }

   // the following functions evaluate whether a codegen for the node or for static
   // symbol reference requires entry in the literal pool
   bool bitwiseOpNeedsLiteralFromPool(TR::Node *parent, TR::Node *child) { return false; }
   void setOnDemandLiteralPoolRun(bool answer) {}
   bool isLiteralPoolOnDemandOn () { return false; }
   bool supportsOnDemandLiteralPool() { return false; }
   bool supportsDirectIntegralLoadStoresFromLiteralPool() { return false; }

   bool inlineDirectCall(TR::Node *node, TR::Register *&resultReg) { return false; }

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

   bool shouldBuildStructure() { return _enabledFlags.testAny(ShouldBuildStructure); }
   void setShouldBuildStructure() {_enabledFlags.set(ShouldBuildStructure);}


   bool enableRefinedAliasSets();
   void setEnableRefinedAliasSets() {_enabledFlags.set(EnableRefinedAliasSets);}

   /**
    * @brief Answers whether a trampoline is required for a direct call instruction to
    *           reach a target address.  This function should be overridden by an
    *           architecture-specific implementation.
    *
    * @param[in] targetAddress : the absolute address of the call target
    * @param[in] sourceAddress : the absolute address of the call instruction
    *
    * @return : true, but will assert fatally before returning.
    */
   bool directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress)
      {
      TR_ASSERT_FATAL(0, "An architecture specialization of this function must be provided.");
      return true;
      }

   /**
    * @brief
    *    Redo the trampoline reservation for a call target, if a trampoline might be
    *    required for the target.  This is typically required if a new code cache is
    *    allocated between the instruction selection and binary encoding phases.
    *
    * @details
    *    Note that the instructionSymRef cannot simply be read from the provided instruction
    *    because this function is shared across multiple architectures with incompatible
    *    instruction hierarchies.
    *
    * @param[in] callInstr : the call instruction to which this trampoline applies
    * @param[in] instructionSymRef : the TR::SymbolReference present on the instruction
    *
    */
   void redoTrampolineReservationIfNecessary(TR::Instruction *callInstr, TR::SymbolReference *instructionSymRef);

   /**
    * @brief
    *    Create the object format for this compilation.  If the code generator
    *    does not support the use of object formats then this function does nothing.
    */
   void createObjectFormat() { return; }

   /**
    * @return TR::ObjectFormat created for this \c TR::CodeGenerator.  If the value
    *         returned is NULL then ObjectFormats are not supported.
    */
   TR::ObjectFormat *getObjectFormat() { return _objectFormat; }

protected:
   /**
    * @brief Sets an ObjectFormat.  Available to \c TR::CodeGenerator classes only.
    */
   void setObjectFormat(TR::ObjectFormat *of) { _objectFormat = of; }

public:

   // --------------------------------------------------------------------------

   bool constantAddressesCanChangeSize(TR::Node *node);
   bool profiledPointersRequireRelocation();

   // will a BCD left shift always leave the sign code unchanged and thus allow it to be propagated through and upwards
   bool propagateSignThroughBCDLeftShift(TR::DataType type) { return false; }

   bool supportsLengthMinusOneForMemoryOpts() {return false;}

   // Allows a platform code generator to assert that a particular node operation will use 64 bit values
   // that are not explicitly present in the node datatype.
   bool usesImplicit64BitGPRs(TR::Node *node) { return false; }

   // General utility?
   static bool treeContainsCall(TR::TreeTop * treeTop);

   // IA32 only?
   int32_t arrayInitMinimumNumberOfBytes() {return 8;}

   void addCountersToEdges(TR::Block *block);

   bool getSupportsBitOpCodes() {return false;}

   bool getMappingAutomatics() {return _flags1.testAny(MappingAutomatics);}
   void setMappingAutomatics() {_flags1.set(MappingAutomatics);}

   bool getSupportsDirectJNICalls() {return _flags1.testAny(SupportsDirectJNICalls);}
   bool supportsDirectJNICallsForAOT() { return false;}

   void setSupportsDirectJNICalls() {_flags1.set(SupportsDirectJNICalls);}

   bool getSupportsGlRegDeps() {return _flags1.testAny(SupportsGlRegDeps);}
   void setSupportsGlRegDeps() {_flags1.set(SupportsGlRegDeps);}

   /**
    * @brief Query whether this code generator supports recompilation
    * @return true if recompilation supported; false otherwise
    */
   bool getSupportsRecompilation() {return _flags1.testAny(SupportsRecompilation);}

   /**
    * @brief Indicate code generator support for recompilation
    */
   void setSupportsRecompilation() {_flags1.set(SupportsRecompilation);}

   bool getSupportsVectorRegisters() {return _flags1.testAny(SupportsVectorRegisters);}
   void setSupportsVectorRegisters() {_flags1.set(SupportsVectorRegisters);}

   bool getSupportsGlRegDepOnFirstBlock() {return _flags1.testAny(SupportsGlRegDepOnFirstBlock);}
   void setSupportsGlRegDepOnFirstBlock() {_flags1.set(SupportsGlRegDepOnFirstBlock);}

   bool getDisableLongGRA() {return _flags1.testAny(DisableLongGRA);}
   void setDisableLongGRA() {_flags1.set(DisableLongGRA);}

   bool getDisableFloatingPointGRA() {return _flags2.testAny(DisableFloatingPointGRA);}
   void setDisableFloatingPointGRA() {_flags2.set(DisableFloatingPointGRA);}

   bool usesRegisterMaps() {return _flags1.testAny(UsesRegisterMaps);}
   void setUsesRegisterMaps() {_flags1.set(UsesRegisterMaps);}

   bool getSupportsDivCheck() {return _flags1.testAny(SupportsDivCheck);}
   void setSupportsDivCheck() {_flags1.set(SupportsDivCheck);}

   bool getSupportsPrimitiveArrayCopy() {return _flags2.testAny(SupportsPrimitiveArrayCopy);}
   void setSupportsPrimitiveArrayCopy() {_flags2.set(SupportsPrimitiveArrayCopy);}

   bool getSupportsDynamicANewArray() {return _flags2.testAny(SupportsDynamicANewArray);}
   void setSupportsDynamicANewArray() {_flags2.set(SupportsDynamicANewArray);}

   bool getSupportsReferenceArrayCopy() {return _flags1.testAny(SupportsReferenceArrayCopy);}
   void setSupportsReferenceArrayCopy() {_flags1.set(SupportsReferenceArrayCopy);}

   bool getSupportsEfficientNarrowIntComputation() {return _flags2.testAny(SupportsEfficientNarrowIntComputation);}
   void setSupportsEfficientNarrowIntComputation() {_flags2.set(SupportsEfficientNarrowIntComputation);}

   bool getSupportsEfficientNarrowUnsignedIntComputation() {return _flags4.testAny(SupportsEfficientNarrowUnsignedIntComputation);}
   void setSupportsEfficientNarrowUnsignedIntComputation() {_flags4.set(SupportsEfficientNarrowUnsignedIntComputation);}

   bool getSupportsArrayTranslateAndTest() {return _flags2.testAny(SupportsArrayTranslateAndTest);}
   void setSupportsArrayTranslateAndTest() {_flags2.set(SupportsArrayTranslateAndTest);}

   bool getSupportsArrayTranslateTRxx() {return _flags2.testAny(SupportsArrayTranslate);}
   void setSupportsArrayTranslateTRxx() {_flags2.set(SupportsArrayTranslate);}
   void resetSupportsArrayTranslateTRxx() {_flags2.reset(SupportsArrayTranslate);}

   bool getSupportsSelect() {return _flags2.testAny(SupportsSelect);}
   void setSupportsSelect() {_flags2.set(SupportsSelect);}

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

   bool getSupportsArraySet() {return _flags1.testAny(SupportsArraySet);}
   void setSupportsArraySet() {_flags1.set(SupportsArraySet);}

   bool getSupportsArrayCmp() {return _flags1.testAny(SupportsArrayCmp);}
   void setSupportsArrayCmp() {_flags1.set(SupportsArrayCmp);}

   bool getSupportsArrayCmpLen() {return _flags1.testAny(SupportsArrayCmpLen);}
   void setSupportsArrayCmpLen() {_flags1.set(SupportsArrayCmpLen);}

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

   bool getSupportsBDLLHardwareOverflowCheck() { return _flags3.testAny(SupportsBDLLHardwareOverflowCheck);}
   void setSupportsBDLLHardwareOverflowCheck() { _flags3.set(SupportsBDLLHardwareOverflowCheck);}

   bool getSupportsDoubleWordCAS() { return _flags3.testAny(SupportsDoubleWordCAS);}
   void setSupportsDoubleWordCAS() { _flags3.set(SupportsDoubleWordCAS);}

   bool getSupportsDoubleWordSet() { return _flags3.testAny(SupportsDoubleWordSet);}
   void setSupportsDoubleWordSet() { _flags3.set(SupportsDoubleWordSet);}

   bool getSupportsAtomicLoadAndAdd() { return _flags4.testAny(SupportsAtomicLoadAndAdd);}
   void setSupportsAtomicLoadAndAdd() { _flags4.set(SupportsAtomicLoadAndAdd);}

   bool getSupportsTM() { return _flags4.testAny(SupportsTM);}
   void setSupportsTM() { _flags4.set(SupportsTM);}

   virtual bool getSupportsTLE();

   virtual bool getSupportsBitPermute();

   bool getSupportsAutoSIMD() { return _flags4.testAny(SupportsAutoSIMD);}
   void setSupportsAutoSIMD() { _flags4.set(SupportsAutoSIMD);}

   static bool getSupportsOpCodeForAutoSIMD(TR::CPU *cpu, TR::ILOpCode opcode) { return false; }
   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode) { return false; }

   bool usesMaskRegisters() { return false; }

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

   bool getSupportsMergedAllocations() {return _flags1.testAny(SupportsMergedAllocations);}
   void setSupportsMergedAllocations() {_flags1.set(SupportsMergedAllocations);}

   bool getSupportsInlinedAtomicLongVolatiles() {return _flags1.testAny(SupportsInlinedAtomicLongVolatiles);}
   void setSupportsInlinedAtomicLongVolatiles() {_flags1.set(SupportsInlinedAtomicLongVolatiles);}
   bool getInlinedGetCurrentThreadMethod() {return _flags3.testAny(InlinedGetCurrentThreadMethod);}
   void setInlinedGetCurrentThreadMethod() {_flags3.set(InlinedGetCurrentThreadMethod);}

   bool considerAllAutosAsTacticalGlobalRegisterCandidates()    {return _flags1.testAny(ConsiderAllAutosAsTacticalGlobalRegisterCandidates);}
   void setConsiderAllAutosAsTacticalGlobalRegisterCandidates() {_flags1.set(ConsiderAllAutosAsTacticalGlobalRegisterCandidates);}

   bool supportsByteswap() { return _flags1.testAny(SupportsByteswap); }
   void setSupportsByteswap() { _flags1.set(SupportsByteswap); }

   bool getSupportsScaledIndexAddressing() { return _flags1.testAny(SupportsScaledIndexAddressing); }
   void setSupportsScaledIndexAddressing() { _flags1.set(SupportsScaledIndexAddressing); }

   bool isAddressScaleIndexSupported(int32_t scale) { return false; }

   bool getSupportsConstantOffsetInAddressing(int64_t value);
   bool getSupportsConstantOffsetInAddressing() { return _flags3.testAny(SupportsConstantOffsetInAddressing); }
   void setSupportsConstantOffsetInAddressing() { _flags3.set(SupportsConstantOffsetInAddressing); }

   bool getSupportsCompactedLocals() {return _flags1.testAny(SupportsCompactedLocals);}
   void setSupportsCompactedLocals() {_flags1.set(SupportsCompactedLocals);}

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

   bool getSupportsLoweringConstIDiv() {return getSupportsIMulHigh();}

   bool getSupportsLoweringConstLDiv() {return getSupportsLMulHigh();}

   bool getSupportsIMulHigh() {return _flags2.testAny(SupportsIMulHigh);}
   void setSupportsIMulHigh() {_flags2.set(SupportsIMulHigh);}

   bool getSupportsLMulHigh() {return _flags2.testAny(SupportsLMulHigh);}
   void setSupportsLMulHigh() {_flags2.set(SupportsLMulHigh);}

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

   bool getSupportsTestUnderMask() {return _flags4.testAny(SupportsTestUnderMask);}
   void setSupportsTestUnderMask() {_flags4.set(SupportsTestUnderMask);}

   bool getSupportsRuntimeInstrumentation() {return _flags4.testAny(SupportsRuntimeInstrumentation);}
   void setSupportsRuntimeInstrumentation() {_flags4.set(SupportsRuntimeInstrumentation);}

   bool isOutOfLineColdPath() {return (_outOfLineColdPathNestedDepth > 0) ? true : false;}
   void incOutOfLineColdPathNestedDepth(){_outOfLineColdPathNestedDepth++;}
   void decOutOfLineColdPathNestedDepth(){_outOfLineColdPathNestedDepth--;}

   /**
    * @brief checks if instruction selection is in the state of generating
    *        instrucions for warm blocks in the case of warm and cold block splitting
    *
    * @return true if instruction selection is in the state of generating
    *         instrucions for warm blocks and false otherwise
    */
   bool getInstructionSelectionInWarmCodeCache() {return _flags2.testAny(InstructionSelectionInWarmCodeCache);}

   /**
    * @brief sets the state of the instruction selection to generating warm blocks
    *        in the case of warm and cold block splitting
    */
   void setInstructionSelectionInWarmCodeCache() {_flags2.set(InstructionSelectionInWarmCodeCache);}

   /**
    * @brief sets the state of the instruction selection to generating cold blocks
    *        in the case of warm and cold block splitting
    */
   void resetInstructionSelectionInWarmCodeCache() {_flags2.reset(InstructionSelectionInWarmCodeCache);}

   bool getMethodModifiedByRA() {return _flags2.testAny(MethodModifiedByRA);}
   void setMethodModifiedByRA() {_flags2.set(MethodModifiedByRA);}
   void resetMethodModifiedByRA() {_flags2.reset(MethodModifiedByRA);}

   bool getEnforceStoreOrder() {return _flags2.testAny(EnforceStoreOrder);}
   void setEnforceStoreOrder() {_flags2.set(EnforceStoreOrder);}

   bool getDisableNullCheckOfArrayLength() { return _flags3.testAny(CompactNullCheckOfArrayLengthDisabled); }
   void setDisableNullCheckOfArrayLength() { _flags3.set(CompactNullCheckOfArrayLengthDisabled); }

   bool getSupportsStackAllocationOfArraylets() {return _flags3.testAny(SupportsStackAllocationOfArraylets);}
   void setSupportsStackAllocationOfArraylets() {_flags3.set(SupportsStackAllocationOfArraylets);}

   bool multiplyIsDestructive() { return _flags3.testAny(MultiplyIsDestructive); }
   void setMultiplyIsDestructive() { _flags3.set(MultiplyIsDestructive); }

   bool getIsInOOLSection() { return _flags4.testAny(IsInOOLSection); }
   void toggleIsInOOLSection();

   bool isInMemoryInstructionCandidate(TR::Node * node);

   void setLmmdFailed() { _lmmdFailed = true;}

   protected:

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
      SupportsRecompilation                              = 0x00001000,
      SupportsVectorRegisters                            = 0x00002000,
      SupportsGlRegDepOnFirstBlock                       = 0x00004000,
      SupportsRemAsThirdChildOfDiv                       = 0x00008000,
      HasNodePointerInEachInstruction                    = 0x00010000,
      SupportsMergedAllocations                          = 0x00020000,
      SupportsInlinedAtomicLongVolatiles                 = 0x00040000,
      PerformsExplicitChecks                             = 0x00080000,
      SpillsFPRegistersAcrossCalls                       = 0x00100000,
      ConsiderAllAutosAsTacticalGlobalRegisterCandidates = 0x00200000,
      SupportsByteswap                                   = 0x00400000,
      SupportsScaledIndexAddressing                      = 0x00800000,
      SupportsCompactedLocals                            = 0x01000000,
      // AVAILABLE                                       = 0x02000000,
      UsesRegisterPairsForLongs                          = 0x04000000,
      SupportsArraySet                                   = 0x08000000,
      SupportsArrayCmpLen                                = 0x10000000,
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
      SupportsIMulHigh                                    = 0x00000040,
      SupportsLMulHigh                                    = 0x00000080,
      SupportsArrayTranslate                              = 0x00000100,
      HasDoubleWordAlignedStack                           = 0x00000200,
      SupportsReadOnlyLocks                               = 0x00000400,
      SupportsArrayTranslateAndTest                       = 0x00000800,
      SupportsDynamicANewArray                            = 0x00001000,
      SupportsSelect                                      = 0x00002000,
      // AVAILABLE                                        = 0x00004000,
      SupportsPostProcessArrayCopy                        = 0x00008000,
      // AVAILABLE                                        = 0x00010000,
      SupportsCurrentTimeMaxPrecision                     = 0x00020000,
      HasCCSigned                                         = 0x00040000,
      HasCCZero                                           = 0x00080000,
      HasCCOverflow                                       = 0x00100000,
      HasCCInfo                                           = 0x00200000,
      // AVAILABLE                                        = 0x00400000,
      SupportsLoweringConstLDivPower2                     = 0x00800000,
      DisableFloatingPointGRA                             = 0x01000000,
      InstructionSelectionInWarmCodeCache                = 0x02000000,
      MethodModifiedByRA                                  = 0x04000000,
      // AVAILABLE                                        = 0x08000000,
      // AVAILABLE                                        = 0x10000000,
      EnforceStoreOrder                                   = 0x20000000,
      // AVAILABLE                                        = 0x80000000,
      DummyLastEnum2
      };

   enum // _flags3
      {
      // AVAILABLE                                        = 0x00000001,
      SupportsConstantOffsetInAddressing                  = 0x00000002,
      SupportsAlignedAccessOnly                           = 0x00000004,
      // AVAILABLE                                        = 0x00000008,
      CompactNullCheckOfArrayLengthDisabled               = 0x00000010,
      SupportsArrayCmpSign                                = 0x00000020,
      SupportsSearchCharString                            = 0x00000040,
      SupportsTranslateAndTestCharString                  = 0x00000080,
      SupportsTestCharComparisonControl                   = 0x00000100,
      // AVAILABLE                                        = 0x00000200,
      // AVAILABLE                                        = 0x00000400,
      HasCCCarry                                          = 0x00000800,
      // AVAILABLE                                        = 0x00001000,
      // AVAILABLE                                        = 0x00002000,
      RemoveRegisterHogsInLowerTreesWalk                  = 0x00004000,
      SupportsBDLLHardwareOverflowCheck                   = 0x00008000,
      InlinedGetCurrentThreadMethod                       = 0x00010000,
      RequiresCarry                                       = 0x00020000,
      MethodContainsBinaryCodedDecimal                    = 0x00040000,  // wcode
      ComputesCarry                                       = 0x00080000,
      // AVAILABLE                                        = 0x00100000,
      // AVAILABLE                                        = 0x00200000,
      SupportsStackAllocationOfArraylets                  = 0x00400000,
      // AVAILABLE                                        = 0x00800000,
      SupportsDoubleWordCAS                               = 0x01000000,
      SupportsDoubleWordSet                               = 0x02000000,
      // AVAILABLE                                        = 0x04000000,
      // AVAILABLE                                        = 0x08000000,
      MultiplyIsDestructive                               = 0x10000000,
      // AVAILABLE                                        = 0x20000000,
      HasCCCompare                                        = 0x40000000,
      // AVAILABLE                                        = 0x80000000,
      DummyLastEnum
      };

   enum // flags4
      {
      // AVAILABLE                                        = 0x00000001,
      // AVAILABLE                                        = 0x00000002,
      // AVAILABLE                                        = 0x00000004,
      // AVAILABLE                                        = 0x00000008,
      // AVAILABLE                                        = 0x00000010,
      IsInOOLSection                                      = 0x00000020,
      // AVAILABLE                                        = 0x00000040,
      GRACompleted                                        = 0x00000080,
      SupportsTestUnderMask                               = 0x00000100,
      SupportsRuntimeInstrumentation                      = 0x00000200,
      SupportsEfficientNarrowUnsignedIntComputation       = 0x00000400,
      SupportsAtomicLoadAndAdd                            = 0x00000800,
      // AVAILABLE                                        = 0x00001000,
      // AVAILABLE                                        = 0x00002000,
      // AVAILABLE                                        = 0x00004000,
      SupportsArrayTranslateTRTO255                       = 0x00008000, //if (ca[i] < 256) ba[i] = (byte) ca[i]
      SupportsArrayTranslateTROTNoBreak                   = 0x00010000, //ca[i] = (char) ba[i]
      SupportsArrayTranslateTRTO                          = 0x00020000, //if (ca[i] < x) ba[i] = (byte) ca[i]; x is either 256 or 128
      SupportsArrayTranslateTROT                          = 0x00040000, //if (ba[i] >= 0) ca[i] = (char) ba[i];
      SupportsTM                                          = 0x00080000,
      SupportsProfiledInlining                            = 0x00100000,
      SupportsAutoSIMD                                    = 0x00200000,  //vector support for autoVectorizatoon
      // AVAILABLE                                        = 0x00400000,
      // AVAILABLE                                        = 0x00800000,
      // AVAILABLE                                        = 0x01000000,
      // AVAILABLE                                        = 0x02000000,
      // AVAILABLE                                        = 0x04000000,
      // AVAILABLE                                        = 0x08000000,
      // AVAILABLE                                        = 0x10000000,
      // AVAILABLE                                        = 0x20000000,

      DummyLastEnum4
      };

   enum // enabledFlags
      {
      // AVAILABLE                     = 0x0001,
      // AVAILABLE                     = 0x0002,
      // AVAILABLE                     = 0x0004,
      EnableRefinedAliasSets           = 0x0008,
      // AVAILABLE                     = 0x0010,
      ShouldBuildStructure             = 0x0020,
      LockFreeSpillList                = 0x0040,  // TAROK only (until it matures)
      UseNonLinearRegisterAssigner     = 0x0080,  // TAROK only (until it matures)
      TrackRegisterUsage               = 0x0100,  // TAROK only (until it matures)
      // AVAILABLE                     = 0x0200,
      // AVAILABLE                     = 0x0400,
      // AVAILABLE                     = 0x0800,
      // AVAILABLE                     = 0x1000,
      // AVAILABLE                     = 0x2000,
      };

   TR::SymbolReferenceTable *_symRefTab;
   TR::Linkage *_linkages[TR_NumLinkages];
   TR::Linkage *_bodyLinkage;
   TR::Register *_vmThreadRegister;
   TR::RealRegister *_realVMThreadRegister;
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

   TR::list<TR::SymbolReference*> _availableSpillTemps;
   TR::list<TR_LiveReference*> _liveReferenceList;
   TR::list<TR::Snippet*> _snippetList;
   TR_Array<TR::Register *> _registerArray;
   TR::list<TR_BackingStore*> _spill4FreeList;
   TR::list<TR_BackingStore*> _spill8FreeList;
   TR::list<TR_BackingStore*> _spill16FreeList;
   TR::list<TR_BackingStore*> _spill32FreeList;
   TR::list<TR_BackingStore*> _spill64FreeList;
   TR::list<TR_BackingStore*> _internalPointerSpillFreeList;
   TR::list<TR_BackingStore*> _collectedSpillList;
   TR::list<TR_BackingStore*> _allSpillList;
   TR::list<TR::Relocation *> _relocationList;
   TR::list<TR::Relocation *> _externalRelocationList;
   TR::list<TR::StaticRelocation> _staticRelocationList;

   TR::list<TR::SymbolReference*> _variableSizeSymRefPendingFreeList;
   TR::list<TR::SymbolReference*> _variableSizeSymRefFreeList;
   TR::list<TR::SymbolReference*> _variableSizeSymRefAllocList;

   int32_t _accumulatorNodeUsage;

   TR::list<TR::Register*> *_firstTimeLiveOOLRegisterList;
   TR::list<TR::Register*> *_spilledRegisterList;
   TR::list<OMR::RegisterUsage*> *_referencedRegistersList;
   int32_t _currentPathDepth;



   uint32_t _largestOutgoingArgSize;

   uint32_t _estimatedWarmCodeLength;
   uint32_t _estimatedColdCodeLength;
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
   TR::Instruction *_lastWarmInstruction;

   TR_RegisterMask _liveRealRegisters[NumRegisterKinds];
   TR_GlobalRegisterNumber _lastGlobalGPR;
   TR_GlobalRegisterNumber _firstGlobalFPR;
   TR_GlobalRegisterNumber _lastGlobalFPR;
   TR_GlobalRegisterNumber _firstOverlappedGlobalFPR;
   TR_GlobalRegisterNumber _lastOverlappedGlobalFPR;
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

   uint8_t *_warmCodeEnd;
   uint8_t *_coldCodeStart;

   public:

   static TreeEvaluatorFunctionPointerTable _nodeToInstrEvaluators;

   protected:

   /// Determines whether register allocation has been completed
   bool _afterRA;

   bool _disableInternalPointers;

   TR::RegisterIterator *_gpRegisterIterator;
   TR::RegisterIterator *_fpRegisterIterator;
   TR::RegisterIterator *_vmRegisterIterator;
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

   bool _codeCacheSwitched; ///< Has the CodeCache switched from the initially assigned CodeCache?

   TR_Stack<TR::Node *> _stackOfArtificiallyInflatedNodes;

   CS2::HashTable<TR::Symbol*, TR::DataType, TR::Allocator> _symbolDataTypeMap;

   /**
    * The binary object format to generate code for in this compilation
    */
   TR::ObjectFormat *_objectFormat;

   TR::list<TR::Snippet *> _snippetsToBePatchedOnClassUnload;
   TR::list<TR::Snippet *> _methodSnippetsToBePatchedOnClassUnload;
   TR::list<TR::Snippet *> _snippetsToBePatchedOnClassRedefinition;

   };

}

#define SPLIT_WARM_COLD_STRING  "SPLIT WARM AND COLD BLOCKS:"

#endif
