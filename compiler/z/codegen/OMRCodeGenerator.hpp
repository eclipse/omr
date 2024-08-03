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

#ifndef OMR_Z_CODEGENERATOR_INCL
#define OMR_Z_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace Z { class CodeGenerator; } }
namespace OMR { typedef OMR::Z::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::Z::CodeGenerator expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/hashtab.h"
#include "env/CompilerEnv.hpp"
#include "env/CPU.hpp"
#include "env/jittypes.h"
#include "env/PersistentInfo.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Flags.hpp"
#include "infra/List.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "env/IO.hpp"


#include "z/codegen/S390OutOfLineCodeSection.hpp"
#include "codegen/BackingStore.hpp"
#include "codegen/Relocation.hpp"

#include "infra/TRlist.hpp"
class TR_BackingStore;
class TR_GCStackMap;
class TR_OpaquePseudoRegister;
class TR_PseudoRegister;
namespace TR { class RegisterCandidate; }
namespace TR { class S390ConstantDataSnippet; }
namespace TR { class S390ConstantInstructionSnippet; }
namespace TR { class S390EyeCatcherDataSnippet; }
namespace TR { class S390ImmInstruction; }
class TR_S390OutOfLineCodeSection;
class TR_S390ScratchRegisterManager;
namespace TR { class S390WritableDataSnippet; }
class TR_StorageReference;
namespace OMR { class Linkage; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Optimizer; }
namespace TR { class RegStarRef; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterIterator; }
namespace TR { class RegisterPair; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class SystemLinkage; }

extern int64_t getIntegralValue(TR::Node* node);

#if defined(TR_TARGET_64BIT)
#define UPPER_4_BYTES(x) ((x) >> 32)
#define LOWER_4_BYTES(x) ((x) & 0xffffffff)
#else
// fake it to allow warning-free compilation on 32 bit systems
#define UPPER_4_BYTES(x) (x)
#define LOWER_4_BYTES(x) (x)
#endif

#define TR_MAX_MVC_SIZE 256
#define TR_MAX_CLC_SIZE 256
#define TR_MIN_SS_DISP 0
#define TR_MAX_SS_DISP 4095
#define TR_MIN_RX_DISP 0
#define TR_MAX_RX_DISP 4095

#define TR_MEMCPY_PAD_DST_LEN_INDEX 1
#define TR_MEMCPY_PAD_SRC_LEN_INDEX 3
#define TR_MEMCPY_PAD_MAX_LEN_INDEX 5
#define TR_MEMCPY_PAD_EQU_LEN_INDEX 6
#define MVCL_THRESHOLD 16777216

enum TR_MemCpyPadTypes
   {
   OneByte,
   TwoByte,
   ND_OneByte,
   ND_TwoByte,
   InvalidType
   };

struct TR_S390BinaryEncodingData : public TR_BinaryEncodingData
   {
   int32_t estimate;
   TR::Instruction *cursorInstruction;
   TR::Instruction *jitTojitStart;
   TR::Instruction *preProcInstruction;
   int32_t loadArgSize;
   };


namespace OMR
{
namespace Z
{
class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

public:

   void preLowerTrees();

   struct TR_BranchPreloadCallData
      {
      TR_ALLOC(TR_Memory::BranchPreloadCallData)
      TR::Instruction *_callInstr;
      TR::LabelSymbol *_callLabel;
      TR::Block       *_callBlock;
      TR::SymbolReference * _callSymRef;
      int32_t        _frequency;

      TR_BranchPreloadCallData() { _frequency = -1;}
      TR_BranchPreloadCallData(TR::Instruction *i,TR::LabelSymbol *l,TR::Block *b,TR::SymbolReference *s, int32_t f)
      :_callInstr(i),_callLabel(l),_callBlock(b),_callSymRef(s),_frequency(f){}
      };

   struct TR_BranchPreloadData
      {
      TR::Instruction *_returnInstr;
      TR::LabelSymbol *_returnLabel;
      TR::Block       *_returnBlock;
      int32_t        _frequency;
      bool           _insertBPPInEpilogue;

      TR_BranchPreloadData() { _frequency = -1; _insertBPPInEpilogue = false; }
      };

   TR_BranchPreloadData _hottestReturn;
   TR_BranchPreloadCallData _outlineCall;
   TR_BranchPreloadCallData _outlineArrayCall;

   TR::list<TR_BranchPreloadCallData*> *_callsForPreloadList;

   TR::list<TR_BranchPreloadCallData*> * getCallsForPreloadList() { return _callsForPreloadList; }
   void createBranchPreloadCallData(TR::LabelSymbol *callLabel, TR::SymbolReference * callSymRef, TR::Instruction * instr);

   void insertInstructionPrefetches();
   void insertInstructionPrefetchesForCalls(TR_BranchPreloadCallData * data);
   bool hasWarmCallsBeforeReturn();

   /**
    * @brief Answers whether a method call is implemented using an internal runtime
    *           helper routine (ex. a j2iTransition)
    *
    * @param[in] sym : The symbol holding the call information
    *
    * @return : true if the call is represented by a helper; false otherwise.
    */
   bool callUsesHelperImplementation(TR::Symbol *sym) { return false; }

   /**
    * @brief Answers whether a trampoline is required for a direct call instruction to
    *           reach a target address.
    *
    * @param[in] targetAddress : the absolute address of the call target
    * @param[in] sourceAddress : the absolute address of the call instruction
    *
    * @return : true if a trampoline is required; false otherwise.
    */
   bool directCallRequiresTrampoline(intptr_t targetAddress, intptr_t sourceAddress);

   /**
    * \brief Tells the optimzers and codegen whether a load constant node should be rematerialized.
    *
    * \details Large constants should be materialized (constant node should be commoned up)
    * because loading them as immediate values is expensive.
    *
    * A constant is large if it's outside of the range specified by the largest negative
    * const and smallest positive const. And the ranges are hardware platform dependent.
    */
   bool materializesLargeConstants() { return true; }
   bool shouldValueBeInACommonedNode(int64_t value);
   int64_t getLargestNegConstThatMustBeMaterialized() { return (int64_t)(((~(uint64_t)0) << 31) - 1); } // min 32bit signed integer minus 1
   int64_t getSmallestPosConstThatMustBeMaterialized() {return ((int64_t)0x000000007FFFFFFF) + 1;}   // max 32bit signed integer plus 1

   void beginInstructionSelection();
   void endInstructionSelection();

   void checkIsUnneededIALoad(TR::Node *parent, TR::Node* node, TR::TreeTop *tt);
   void lowerTreesWalk(TR::Node * parent, TR::TreeTop * treeTop, vcount_t visitCount);


   void lowerTreeIfNeeded(TR::Node *node, int32_t childNumber, TR::Node *parent, TR::TreeTop *tt);

   void lowerTreesPropagateBlockToNode(TR::Node *node);

protected:

   CodeGenerator(TR::Compilation *comp);

public:

   void initialize();

   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   bool anyNonConstantSnippets();
   bool anyLitPoolSnippets();

   bool getSupportsEncodeUtf16BigWithSurrogateTest();

   TR_S390ScratchRegisterManager* generateScratchRegisterManager(int32_t capacity = 8);

   bool canTransformUnsafeCopyToArrayCopy();
   bool supportsInliningOfIsInstance();
   bool supports32bitAiadd() {return OMR::Z::CodeGenerator::comp()->target().is64Bit();}

   void addPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet, TR::list<TR_OpaqueClassBlock*> * PICSlist);
   TR::list<TR_OpaqueClassBlock*> * getPICsListForInterfaceSnippet(TR::S390ConstantDataSnippet * ifcSnippet);

   void doInstructionSelection();

   bool loadOrStoreAddressesMatch(TR::Node *node1, TR::Node *node2);

   bool reliesOnAParticularSignEncoding(TR::Node *node);

   void recordRegisterAssignment(TR::Register *assignedReg, TR::Register *virtualReg);

   void doBinaryEncoding();

   void AddFoldedMemRefToStack(TR::MemoryReference * mr);
   void RemoveMemRefFromStack(TR::MemoryReference * mr);
   void StopUsingEscapedMemRefsRegisters(int32_t topOfMemRefStackBeforeEvaluation);

   bool supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol);

   bool supportsDirectJNICallsForAOT() { return true;}

   bool shouldYankCompressedRefs() { return true; }

   bool supportsJITFreeSystemStackPointer() { return false; }
   TR::RegisterIterator *getVRFRegisterIterator()                           {return  _vrfRegisterIterator;        }
   TR::RegisterIterator *setVRFRegisterIterator(TR::RegisterIterator *iter)  {return _vrfRegisterIterator = iter;}

   bool supportsLengthMinusOneForMemoryOpts() {return true;}

   bool codegenSupportsLoadlessBNDCheck() {return OMR::Z::CodeGenerator::comp()->target().cpu.isAtLeast(OMR_PROCESSOR_S390_ZEC12);}
   TR::Register *evaluateLengthMinusOneForMemoryOps(TR::Node *,  bool , bool &lenMinusOne);

   virtual TR_GlobalRegisterNumber getGlobalRegisterNumber(uint32_t realRegNum);

   TR::RegisterDependencyConditions* createDepsForRRMemoryInstructions(TR::Node *node, TR::RegisterPair * sourceReg, TR::RegisterPair * targetReg, uint8_t extraDeps=0);

   // Allocate a pair with inherent consecutive association
   virtual TR::RegisterPair* allocateConsecutiveRegisterPair();
   virtual TR::RegisterPair* allocateConsecutiveRegisterPair(TR::Register* lowRegister, TR::Register* highRegister);
   virtual TR::RegisterPair* allocateFPRegisterPair();
   virtual TR::RegisterPair* allocateFPRegisterPair(TR::Register* lowRegister, TR::Register* highRegister);

   void processUnusedNodeDuringEvaluation(TR::Node *node);

   void apply12BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *, bool isCheckDisp = true);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *, int8_t addressDifferenceDivisor, bool isInstrOffset = false);
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply32BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);

   void setUnavailableRegisters(TR::Block *b, TR_BitVector &unavailableRegisters);

   void resetGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters);
   void setGlobalRegister(TR::RealRegister::RegNum reg, TR_BitVector &globalRegisters);
   void setRegister(TR::RealRegister::RegNum reg, TR_BitVector &registers);
   void removeUnavailableRegisters(TR::RegisterCandidate * rc, TR::Block * * blocks, TR_BitVector & availableRegisters);
   void setUnavailableRegistersUsage(TR_Array<TR_BitVector>  &_liveOnEntryUsage, TR_Array<TR_BitVector>   &_liveOnExitUsage);

   void genMemCpy(TR::MemoryReference *targetMR, TR::Node *targetNode, TR::MemoryReference *sourceMR, TR::Node *sourceNode, int64_t copySize);
   void genMemClear(TR::MemoryReference *targetMR, TR::Node *targetNode, int64_t clearSize);

   void genCopyFromLiteralPool(TR::Node *node, int32_t bytesToCopy, TR::MemoryReference *targetMR, size_t litPoolOffset, TR::InstOpCode::Mnemonic op = TR::InstOpCode::MVC);

   bool useMVCLForMemcpyWithPad(TR::Node *node, TR_MemCpyPadTypes type);
   bool isValidCompareConst(int64_t compareConst);
   bool isIfFoldable(TR::Node *node, int64_t compareConst);

   TR::Instruction *genLoadAddressToRegister(TR::Register *reg, TR::MemoryReference *origMR, TR::Node *node, TR::Instruction *preced=NULL);


   bool useRippleCopyOrXC(size_t size, char *lit);

   bool isCompressedClassPointerOfObjectHeader(TR::Node * node);

   bool usesImplicit64BitGPRs(TR::Node *node);
   bool nodeMayCauseException(TR::Node *node);

   bool nodeRequiresATemporary(TR::Node *node);
   bool endHintOnOperation(TR::Node *node);
   bool endAccumulatorSearchOnOperation(TR::Node *node);
   bool isStorageReferenceType(TR::Node *node);

   TR::Register *allocateClobberableRegister(TR::Register *srcRegister);
   /** Different from evaluateNode in that it returns a clobberable register */
   TR::Register *gprClobberEvaluate(TR::Node *node, bool force_copy=false, bool ignoreRefCount = false);
   TR::Register *fprClobberEvaluate(TR::Node *node);
   TR_OpaquePseudoRegister *ssrClobberEvaluate(TR::Node *node, TR::MemoryReference *sourceMR);

//Convenience accessor methods
   TR::Linkage *getS390Linkage();

   TR::RealRegister *getStackPointerRealRegister(TR::Symbol *symbol = NULL);
   TR::RealRegister *getEntryPointRealRegister();
   TR::RealRegister *getReturnAddressRealRegister();

   TR::RealRegister::RegNum getEntryPointRegister();
   TR::RealRegister::RegNum getReturnAddressRegister();

   TR::RealRegister *getMethodMetaDataRealRegister();
   TR::RealRegister *getLitPoolRealRegister();


   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   // BCDCHK node
   TR::Node * _currentCheckNode;
   void setCurrentCheckNodeBeingEvaluated(TR::Node * n) { _currentCheckNode = n; }
   TR::Node * getCurrentCheckNodeBeingEvaluated(){ return _currentCheckNode; }

   // BCDCHK node exception handler label
   TR::LabelSymbol* _currentBCDCHKHandlerLabel;
   void setCurrentBCDCHKHandlerLabel(TR::LabelSymbol * l) { _currentBCDCHKHandlerLabel = l; }
   TR::LabelSymbol* getCurrentBCDCHKHandlerLabel(){ return _currentBCDCHKHandlerLabel; }

   TR::RegisterDependencyConditions * _currentBCDRegDeps;
   void setCurrentCheckNodeRegDeps(TR::RegisterDependencyConditions * daaDeps) {_currentBCDRegDeps = daaDeps;}
   TR::RegisterDependencyConditions * getCurrentCheckNodeRegDeps() {return _currentBCDRegDeps;}

   int32_t arrayInitMinimumNumberOfBytes() {return 16;}

   bool directLoadAddressMatch(TR::Node *load1, TR::Node *load2, bool trace);
   bool isOutOf32BitPositiveRange(int64_t value, bool trace);
   int32_t getMaskSize(int32_t leftMostNibble, int32_t nibbleCount);

   void setAccumulatorNodeUsage(int32_t n) { _accumulatorNodeUsage = n; }
   int32_t getAccumulatorNodeUsage()       { return _accumulatorNodeUsage; }
   void incAccumulatorNodeUsage()          { _accumulatorNodeUsage++; }

   bool possiblyConflictingNode(TR::Node *node);
   bool foundConflictingNode(TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes);
   void collectConflictingAddressNodes(TR::Node *parent, TR::Node *node, TR::list<TR::Node*> *conflictingAddressNodes);


   template <class TR_AliasSetInterface> bool loadAndStoreMayOverlap(TR::Node *store, size_t storeSize, TR::Node *load, size_t loadSize, TR_AliasSetInterface &storeAliases);
   bool loadAndStoreMayOverlap(TR::Node *store, size_t storeSize, TR::Node *load, size_t loadSize);

   bool checkIfcmpxx(TR::Node *node);
   bool checkBitWiseChild(TR::Node *node);

   TR_StorageDestructiveOverlapInfo getStorageDestructiveOverlapInfo(TR::Node *src, size_t srcLength, TR::Node *dst, size_t dstLength);

   void setBranchOnCountFlag(TR::Node *node, vcount_t visitCount);

   bool needs64bitPrecision(TR::Node *node);

   virtual bool isUsing32BitEvaluator(TR::Node *node);
   virtual bool getSupportsBitPermute();
   int32_t getEstimatedExtentOfLitLoop()  {return _extentOfLitPool;}

   bool supportsBranchPreload()          {return _cgFlags.testAny(S390CG_enableBranchPreload);}
   void setEnableBranchPreload()          {_cgFlags.set(S390CG_enableBranchPreload);}
   void setDisableBranchPreload()          {_cgFlags.reset(S390CG_enableBranchPreload);}

   bool supportsBranchPreloadForCalls()          {return _cgFlags.testAny(S390CG_enableBranchPreloadForCalls);}
   void setEnableBranchPreloadForCalls()          {_cgFlags.set(S390CG_enableBranchPreloadForCalls);}
   void setDisableBranchPreloadForCalls()          {_cgFlags.reset(S390CG_enableBranchPreloadForCalls);}

   bool isOutOfLineHotPath() {return _cgFlags.testAny(S390CG_isOutOfLineHotPath);}
   void setIsOutOfLineHotPath(bool val) {_cgFlags.set(S390CG_isOutOfLineHotPath, val);}

   bool getExitPointsInMethod() {return _cgFlags.testAny(S390CG_doesExit);}
   void setExitPointsInMethod(bool val) {return _cgFlags.set(S390CG_doesExit, val);}

   bool isPrefetchNextStackCacheLine() {return _cgFlags.testAny(S390CG_prefetchNextStackCacheLine);}
   void setPrefetchNextStackCacheLine(bool val) {return _cgFlags.set(S390CG_prefetchNextStackCacheLine, val);}

   // Heap object prefetching
   bool enableTLHPrefetching()    {return _cgFlags.testAny(S390CG_enableTLHPrefetching);}
   void setEnableTLHPrefetching() { _cgFlags.set(S390CG_enableTLHPrefetching);}

   // Query to codegen to know if regs are available or not
   bool isLitPoolFreeForAssignment();

   // REG ASSOCIATION
   //
   bool enableRegisterAssociations();
   bool enableRegisterPairAssociation();

   bool canBeAffectedByStoreTagStalls() { return true; }

   // Local RA/GRA
   TR_RegisterKinds prepareRegistersForAssignment();

   // GRA
   //
   bool prepareForGRA();
   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }
   virtual void simulateNodeEvaluation(TR::Node * node, TR_RegisterPressureState * state, TR_RegisterPressureSummary * summary);

   TR_BitVector *getGlobalGPRsPreservedAcrossCalls(){ return &_globalGPRsPreservedAcrossCalls; }
   TR_BitVector *getGlobalFPRsPreservedAcrossCalls(){ return &_globalFPRsPreservedAcrossCalls; }

   bool considerTypeForGRA(TR::Node *node);
   bool considerTypeForGRA(TR::DataType dt);
   bool considerTypeForGRA(TR::SymbolReference *symRef);

   // Number of assignable GPRs
   int32_t getMaximumNumberOfAssignableGPRs();

   using OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfVRFsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Block *);

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();
   int32_t getMaximumNumbersOfAssignableVRs();
   bool allowGlobalRegisterAcrossBranch(TR::RegisterCandidate *, TR::Node *);
   void setRealRegisterAssociation(TR::Register     *reg,
                                   TR::RealRegister::RegNum realNum);
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt);

   bool doRematerialization() {return true;}

   void dumpDataSnippets(TR::FILE *outFile);

   bool getSupportsBitOpCodes() { return true;}

   bool getSupportsImplicitNullChecks() { return _cgFlags.testAny(S390CG_implicitNullChecks); }
   void setSupportsImplicitNullChecks(bool b) { _cgFlags.set(S390CG_implicitNullChecks, b); }

   bool getConditionalMovesEvaluationMode() { return _cgFlags.testAny(S390CG_conditionalMovesEvaluation); }
   void setConditionalMovesEvaluationMode(bool b) { _cgFlags.set(S390CG_conditionalMovesEvaluation, b); }

   bool getEnableRIOverPrivateLinkage() { return _cgFlags.testAny(S390CG_enableRIOverPrivateLinkage); }
   void setEnableRIOverPrivateLinkage(bool b) { _cgFlags.set(S390CG_enableRIOverPrivateLinkage, b); }

   bool getCondCodeShouldBePreserved() { return _cgFlags.testAny(S390CG_condCodeShouldBePreserved); }
   void setCondCodeShouldBePreserved(bool b) { _cgFlags.set(S390CG_condCodeShouldBePreserved, b); }

   uint8_t getFCondMoveBranchOpCond() { return fCondMoveBranchOpCond; }
   void setFCondMoveBranchOpCond(TR::InstOpCode::S390BranchCondition b) { fCondMoveBranchOpCond = (getMaskForBranchCondition(b)) & 0xF; }

   uint8_t getRCondMoveBranchOpCond() { return 0xF - fCondMoveBranchOpCond; }

   bool canUseImmedInstruction(int64_t v);

   virtual bool isAddMemoryUpdate(TR::Node * node, TR::Node * valueChild);

   bool afterRA() { return _afterRA; }

   TR_S390OutOfLineCodeSection *findS390OutOfLineCodeSectionFromLabel(TR::LabelSymbol *label);

   TR::Instruction *generateNop(TR::Node *node, TR::Instruction *preced=0, TR_NOPKind nopKind=TR_NOPStandard);

   /** \brief
    *     Inserts padding (NOP) instructions in the instruction stream.
    *
    *  \param node
    *     The node with which the generated instruction will be associated.
    *
    *  \param cursor
    *     The instruction following which the padding instructions will be inserted.
    *
    *  \param size
    *     The size in number of bytes of how much padding to insert. This value can be one of 0, 2, 4, or 6.
    *
    *  \param prependCursor
    *     Determines whether the padding instructions will be inserted before or after \param cursor.
    *
    *  \return
    *     The last padding instruction generated, or \param cursor if \param size is zero.
    */
   TR::Instruction* insertPad(TR::Node* node, TR::Instruction* cursor, uint32_t size, bool prependCursor);

    struct TR_S390ConstantDataSnippetKey
        {
           void * c;
           uint32_t size;
           TR::Node *node;
        };

    struct constantHashInfo
        {
          static CS2::HashValue Hash(const TR_S390ConstantDataSnippetKey & key, const CS2::HashValue hv = CS2::CS2_FNV_OFFSETBASIS)
              {
              return CS2::Hash_FNV((const unsigned char*)key.c,key.size, hv);
              }
          static bool Equal(const TR_S390ConstantDataSnippetKey & key1,const TR_S390ConstantDataSnippetKey & key2)
              {
              if(key1.size != key2.size)
                  return false;
              if (key1.node != key2.node)
                  return false;
              switch (key1.size)
                 {
                 case 4:
                    return *((int32_t *) key2.c) == *((int32_t *) key1.c);
                 case 8:
                    return *((int64_t *)key2.c) == *((int64_t *) key1.c);
                 case 2:
                    return *((int16_t *)key2.c) == *((int16_t *)key1.c);
                 case 256:
                    return memcmp(key1.c, key2.c, 256) == 0;
                 }
              return false;
              }
        };

    typedef CS2::HashTable<TR_S390ConstantDataSnippetKey,TR::S390ConstantDataSnippet *, TR::Allocator, constantHashInfo> TR_ConstantSnippetHash;
    typedef TR_ConstantSnippetHash::Cursor TR_ConstHashCursor;


   /** First Snippet (can be AddressSnippet or ConstantSnippet) */
   TR::Snippet *getFirstSnippet();
   // Constant Data List functions
   int32_t setEstimatedOffsetForConstantDataSnippets();
   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);
   void emitDataSnippets();
   bool hasDataSnippets() { return (_constantList.empty() && _writableList.empty() && _snippetDataList.empty() && _constantHash.IsEmpty()) ? false : true; }
   TR::list<TR::S390ConstantDataSnippet*> &getConstantInstructionSnippets() { return _constantList; }
   TR::list<TR::S390ConstantDataSnippet*> &getConstantDataStringSnippets() { return _constantList; }
   TR_ConstHashCursor getConstantDataSnippets() { return _constantHashCur;}
   TR::S390ConstantDataSnippet * getConstantDataSnippet(CS2::HashIndex hi) { return _constantHash.DataAt(hi);}


   TR::S390ConstantDataSnippet * create64BitLiteralPoolSnippet(TR::DataType dt, int64_t value);
   TR::S390ConstantDataSnippet * createLiteralPoolSnippet(TR::Node * node);
   TR::S390ConstantInstructionSnippet *createConstantInstruction(TR::Node *node, TR::Instruction * instr);
   TR::S390ConstantDataSnippet *findOrCreateConstant(TR::Node *, void *c, uint16_t size);
   TR::S390ConstantDataSnippet *findOrCreate2ByteConstant(TR::Node *, int16_t c);
   TR::S390ConstantDataSnippet *findOrCreate4ByteConstant(TR::Node *, int32_t c);
   TR::S390ConstantDataSnippet *findOrCreate8ByteConstant(TR::Node *, int64_t c);
   TR::S390ConstantDataSnippet *Create4ByteConstant(TR::Node *, int32_t c, bool writable);
   TR::S390ConstantDataSnippet *Create8ByteConstant(TR::Node *, int64_t c, bool writable);
   TR::S390ConstantDataSnippet *CreateConstant(TR::Node *, void *c, uint16_t size, bool writable);
   TR::S390ConstantDataSnippet *getFirstConstantData();

   // Writable Data List functions
   bool hasWritableDataSnippets() { return _writableList.empty() ? false : true; }
   TR::S390WritableDataSnippet *CreateWritableConstant(TR::Node *);

   // OutOfLineCodeSection List functions
   TR::list<TR_S390OutOfLineCodeSection*> &getS390OutOfLineCodeSectionList() {return _outOfLineCodeSectionList;}
   TR_S390OutOfLineCodeSection *findOutLinedInstructionsFromLabel(TR::LabelSymbol *label);



   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm);


   // Snippet Data functions
   void addDataConstantSnippet(TR::S390ConstantDataSnippet * snippet);

   // Transient Long Registers

   TR_Array<TR::Register *> &getTransientLongRegisters() {return _transientLongRegisters;}
   void freeAndResetTransientLongs();
   uint32_t registerBitMask(int32_t reg)
      {
      return 1 << (reg-1);
      }

   bool opCodeIsNoOpOnThisPlatform(TR::ILOpCode &opCode);

   bool internalPointerSupportImplemented();
   // list of branch unconditional relative instructions used for patching entry point
   // need to be updated with correct offset to sampling preprologue
   TR::list<TR::Instruction*> _recompPatchInsnList;

   bool supportsFusedMultiplyAdd() {return true;}
   bool supportsSinglePrecisionSQRT() {return true;}

   bool isAddressScaleIndexSupported(int32_t scale) { if (scale <= 2) return true; return false; }
   using OMR::CodeGenerator::getSupportsConstantOffsetInAddressing;
   bool getSupportsConstantOffsetInAddressing(int64_t value);

   void enableLiteralPoolRegisterForGRA ();
   void setOnDemandLiteralPoolRun(bool val)     { _cgFlags.set(S390CG_literalPoolOnDemandOnRun, val); }
   bool isLiteralPoolOnDemandOn () { return _cgFlags.testAny(S390CG_literalPoolOnDemandOnRun); }
   virtual void setGlobalStaticBaseRegisterOn(bool val)     { _cgFlags.set(S390CG_globalStaticBaseRegisterOn, val); }
   virtual bool isGlobalStaticBaseRegisterOn () { return _cgFlags.testAny(S390CG_globalStaticBaseRegisterOn); }
   virtual void setGlobalStaticBaseRegisterOnFlag();
   virtual void setGlobalPrivateStaticBaseRegisterOn(bool val)     { _cgFlags.set(S390CG_globalPrivateStaticBaseRegisterOn, val); }
   virtual bool isGlobalPrivateStaticBaseRegisterOn () { return _cgFlags.testAny(S390CG_globalPrivateStaticBaseRegisterOn); }

   bool canUseRelativeLongInstructions(int64_t value);
   bool supportsOnDemandLiteralPool();

   bool supportsDirectIntegralLoadStoresFromLiteralPool();

   void setCanExceptByTrap(bool val) { _cgFlags.set(S390CG_canExceptByTrap, val); }
   virtual bool canExceptByTrap()    { return _cgFlags.testAny(S390CG_canExceptByTrap); }

   TR_BackingStore *getLocalF2ISpill();

   TR_BackingStore *getLocalD2LSpill();

   // The reusable temp slot is an 8-byte scalar temp that gets mapped to the stack
   // the first time allocateReusableTempSlot() is called.  This temp is meant to
   // be used for short periods of time.
   TR::SymbolReference* allocateReusableTempSlot();
   void freeReusableTempSlot();

   bool isActiveCompareCC(TR::InstOpCode::Mnemonic opcd, TR::Register* tReg, TR::Register* sReg);
   bool isActiveArithmeticCC(TR::Register* tstReg);
   bool isActiveLogicalCC(TR::Node* ccNode, TR::Register* tstReg);

   bool mulDecompositionCostIsJustified(int32_t numOfOperations, char bitPosition[], char operationType[], int64_t value);

   bool canUseGoldenEagleImmediateInstruction( int32_t value )
     {
     return true;
     };

   bool canUseGoldenEagleImmediateInstruction( int64_t value )
     {
     return value >= GE_MIN_IMMEDIATE_VAL && value <= GE_MAX_IMMEDIATE_VAL;
     };

   /** Checks if the immediate value can fit in 32-bit immediate field that is unsigned. */
   bool canUseGoldenEagleUnsignedImmediateInstruction(int64_t value)
     {
     return value == (value & GE_MAX_UNSIGNED_IMMEDIATE_VAL);
     }

   /** LL: Sign extended the specified number of high order bits in the register. */
   bool signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits );

   /** LL: Sign extended the specified number of high order bits in the register, use for 64 bits. */
   bool signExtendedHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits );

   /** LL: Zero filled the specified number of high order bits in the register. */
   bool clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, uint32_t numberOfBits );

   /** LL: Zero filled the specified number of high order bits in the register, use for 64 bits. */
   bool clearHighOrderBits( TR::Node * node, TR::Register * targetRegister, TR::Register * srcRegister, uint32_t numberOfBits );

   void replaceInst(TR::Instruction* old, TR::Instruction* curr);

   static bool isILOpCodeSupported(TR::ILOpCodes);

   void setUsesZeroBasePtr( bool v = true );
   bool getUsesZeroBasePtr();

   int16_t getMinShortForLongCompareNarrower() { return 0; }
   int8_t getMinByteForLongCompareNarrower() { return 0; }

   bool IsInMemoryType(TR::DataType type);

   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget) { if (isByteSource) return 25; else return 5; }

   int32_t arrayTranslateAndTestMinimumNumberOfIterations() { return 4; }

   /** Yank the scaling opp up a tree in lowerTrees */
   bool yankIndexScalingOp() {return true;}

   bool excludeInvariantsFromGRAEnabled();

   /**
    * array translate on TRex and below requires that the translate table be page-aligned
    * for TRTO/TRTT and double-word aligned for TROT/TROO
    * array translate takes awhile to initialize - for very short translations, the original
    * loop of instructions should be driven instead of the translate instruction
    * LL: For Golden Eagle, TRTO/TRTT is also double-word aligned.
    */
   int32_t arrayTranslateTableRequiresAlignment(bool isByteSource, bool isByteTarget)
      {
      return 7;
      }

   uint32_t getJitMethodEntryAlignmentBoundary();

   // LL: move to .cpp
   bool bitwiseOpNeedsLiteralFromPool(TR::Node *parent, TR::Node *child);

   virtual bool isDispInRange(int64_t disp);

   static bool getSupportsOpCodeForAutoSIMD(TR::CPU *cpu, TR::ILOpCode opcode, bool supportsVectorRegisters = true);
   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode);

   TR::Instruction *_ccInstruction;
   TR::Instruction* ccInstruction() { return _ccInstruction; }
   void setCCInstruction(TR::Instruction* cc) { _ccInstruction = cc; }

   #define TR_DEFAULT_DATA_SNIPPET_EXPONENT 7
   int32_t constantDataSnippetExponent() { return TR_DEFAULT_DATA_SNIPPET_EXPONENT; } // 1 << 7 = 128 byte max size for each constantDataSnippet


 private:
   TR_BitVector _globalGPRsPreservedAcrossCalls;
   TR_BitVector _globalFPRsPreservedAcrossCalls;

   TR::S390ImmInstruction          *_returnTypeInfoInstruction;
   int32_t                        _extentOfLitPool;  // excludes snippets

protected:
   TR::list<TR::S390ConstantDataSnippet*>  _constantList;
   TR::list<TR::S390ConstantDataSnippet*>  _snippetDataList;
   List<TR_Pair<TR::Node, int32_t> > _ialoadUnneeded;

private:

   // TODO: These should move into the base class. There also seems to be a little overlap between these and
   // _firstInstruction and _appendInstruction. We should likely expose a getLastInstruction API as well. The former
   // API should return _methodBegin and latter _methodEnd always. The append instruction will be the instruction
   // preceeding _methodEnd.
   TR::Instruction* _methodBegin;
   TR::Instruction* _methodEnd;

   TR::list<TR::S390WritableDataSnippet*>  _writableList;
   TR::list<TR_S390OutOfLineCodeSection*> _outOfLineCodeSectionList;

   CS2::HashTable<TR::Register *, TR::RealRegister::RegNum, TR::Allocator> _previouslyAssignedTo;

   TR_ConstantSnippetHash _constantHash;
   TR_ConstHashCursor     _constantHashCur;

   TR_HashTab * _interfaceSnippetToPICsListHashTab;

   TR::RegisterIterator            *_vrfRegisterIterator;

   TR_Array<TR::Register *>        _transientLongRegisters;

   /** For aggregate type GRA */
   bool considerAggregateSizeForGRA(int32_t size);

   bool  TR_LiteralPoolOnDemandOnRun;

   TR_BackingStore* _localF2ISpill;

   TR_BackingStore* _localD2LSpill;

   uint8_t fCondMoveBranchOpCond;

   TR::SymbolReference* _reusableTempSlot;

   CS2::HashTable<ncount_t, bool, TR::Allocator> _nodesToBeEvaluatedInRegPairs;

protected:

   flags32_t  _cgFlags;

   /** Miscellaneous S390CG boolean flags. */
   typedef enum
      {
      // Available                       = 0x00000001,
      // Available                       = 0x00000002,
      // Available                       = 0x00000004,
      S390CG_addStorageReferenceHints    = 0x00000008,
      S390CG_isOutOfLineHotPath          = 0x00000010,
      S390CG_literalPoolOnDemandOnRun    = 0x00000020,
      S390CG_prefetchNextStackCacheLine  = 0x00000040,
      S390CG_doesExit                    = 0x00000080,
      // Available                       = 0x00000100,
      S390CG_implicitNullChecks          = 0x00000200,
      S390CG_reusableSlotIsFree          = 0x00000400,
      S390CG_conditionalMovesEvaluation  = 0x00000800,
      S390CG_usesZeroBasePtr             = 0x00001000,
      S390CG_enableRIOverPrivateLinkage  = 0x00002000,
      S390CG_condCodeShouldBePreserved   = 0x00004000,
      S390CG_enableBranchPreload         = 0x00008000,
      S390CG_globalStaticBaseRegisterOn  = 0x00010000,
      // Available                       = 0x00020000,
      S390CG_canExceptByTrap             = 0x00040000,
      S390CG_enableTLHPrefetching        = 0x00080000,
      S390CG_enableBranchPreloadForCalls = 0x00100000,
      S390CG_globalPrivateStaticBaseRegisterOn = 0x00200000
      } TR_S390CGFlags;
   };
}
}

class TR_S390ScratchRegisterManager : public TR_ScratchRegisterManager
   {
   public:

   TR_S390ScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) : TR_ScratchRegisterManager(capacity, cg) {}
   };

#ifdef J9_PROJECT_SPECIFIC
uint32_t CalcCodeSize(TR::Instruction *start,TR::Instruction *end);
#endif
#endif
