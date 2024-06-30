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

#ifndef OMR_POWER_CODEGENERATOR_INCL
#define OMR_POWER_CODEGENERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGENERATOR_CONNECTOR
#define OMR_CODEGENERATOR_CONNECTOR
namespace OMR { namespace Power { class CodeGenerator; } }
namespace OMR { typedef OMR::Power::CodeGenerator CodeGeneratorConnector; }
#else
#error OMR::Power::CodeGenerator expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRCodeGenerator.hpp"

#include "codegen/InstOpCode.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/ScratchRegisterManager.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CPU.hpp"
#include "env/PersistentInfo.hpp"
#include "env/Processors.hpp"
#include "env/TypedAllocator.hpp"
#include "env/jittypes.h"
#include "infra/BitVector.hpp"
#include "infra/TRlist.hpp"
#include "optimizer/DataFlowAnalysis.hpp"

#include "codegen/RegisterPair.hpp"
#include "codegen/MemoryReference.hpp"

class TR_BackingStore;
class TR_PPCLoadLabelItem;
class TR_PPCOutOfLineCodeSection;
class TR_PPCScratchRegisterManager;
namespace TR { class CodeGenerator; }
namespace TR { class ConstantDataSnippet; }
namespace TR { class PPCImmInstruction; }
namespace TR { class Snippet; }
namespace TR { struct PPCLinkageProperties; }

extern void loadFloatConstant(TR::CodeGenerator *cg,
                              TR::InstOpCode::Mnemonic loadOp,
                              TR::Node *node,
                              TR::DataType type,
                              void* value,
                              TR::Register *trgReg);

extern TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptr_t      address,
                                    TR::Register    *targetRegister,
                                    TR::Register    *tempRegister,
                                    TR::InstOpCode::Mnemonic  opCode,
                                    bool           isUnloadablePicSite=false,
                                    TR::Instruction *cursor=NULL);

extern TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptr_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false,
                                    int16_t         typeAddress = -1);


extern TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg,
                                    bool            isRelocatable,
                                    TR::Node        *node,
                                    intptr_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false,
                                    int16_t         typeAddress = -1);

extern TR::Instruction *loadActualConstant(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    intptr_t       value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false);

extern TR::Instruction *loadConstant(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    int32_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false);

extern TR::Instruction *loadConstant(TR::CodeGenerator *cg,
                                    TR::Node        *node,
                                    int64_t         value,
                                    TR::Register    *targetRegister,
                                    TR::Instruction *cursor=NULL,
                                    bool            isPicSite=false,
                                    bool            useTOC=true);

extern TR::Instruction *fixedSeqMemAccess(TR::CodeGenerator *cg,
                                         TR::Node          *node,
                                         intptr_t         addr,
                                         TR::Instruction  **nibbles,
                                         TR::Register      *srcOrTrg,
                                         TR::Register      *baseReg,
                                         TR::InstOpCode::Mnemonic     opCode,
                                         int32_t           opSize,
                                         TR::Instruction   *cursor=NULL,
                                         TR::Register      *tempReg=NULL);

extern uint8_t *storeArgumentItem(TR::InstOpCode::Mnemonic       op,
                                  uint8_t            *buffer,
                                  TR::RealRegister *reg,
                                  int32_t             offset,
                                  TR::CodeGenerator *cg);

extern uint8_t *loadArgumentItem(TR::InstOpCode::Mnemonic       op,
                                 uint8_t            *buffer,
                                 TR::RealRegister *reg,
                                 int32_t             offset,
                                 TR::CodeGenerator *cg);

extern intptr_t findCCLocalPPCHelperTrampoline(int32_t helperIndex, void *target, void*callSite, void*fe);

#if defined(TR_HOST_POWER)
void ppcCodeSync(uint8_t * start, uint32_t size);
#endif


struct TR_PPCBinaryEncodingData : public TR_BinaryEncodingData
   {
   int32_t estimate;
   TR::Instruction *cursorInstruction;
   TR::Instruction *jitTojitStart;
   TR::Instruction *preProcInstruction;
   TR::Recompilation *recomp;
   };

#include "il/Node.hpp"
#include "p/codegen/PPCOutOfLineCodeSection.hpp"


namespace OMR
{

namespace Power
{

class CodeGenerator;

class OMR_EXTENSIBLE CodeGenerator : public OMR::CodeGenerator
   {

public:

   List<TR_BackingStore> * conversionBuffer;
   ListIterator<TR_BackingStore> * conversionBufferIt;
   TR_BackingStore * allocateStackSlot();

protected:

   CodeGenerator(TR::Compilation *comp);

public:

   TR::Linkage *createLinkage(TR_LinkageConventions lc);

   void initialize();

   static bool supportsTransientPrefetch();

   bool is64BitProcessor();

   void generateBinaryEncodingPrologue(TR_PPCBinaryEncodingData *data);

   void beginInstructionSelection();
   void endInstructionSelection();
   void doBinaryEncoding();
   void processRelocations();
   void expandInstructions();
   virtual TR_RegisterPressureSummary *calculateRegisterPressure();
   TR::Instruction *generateNop(TR::Node *n, TR::Instruction *preced = 0, TR_NOPKind nopKind=TR_NOPStandard);
   TR::Instruction *generateGroupEndingNop(TR::Node *node , TR::Instruction *preced = 0);
   TR::Instruction *generateProbeNop(TR::Node *node , TR::Instruction *preced = 0);

   bool canEmitDataForExternallyRelocatableInstructions();

   bool inlineDirectCall(TR::Node *node, TR::Register *&resultReg);
   bool supportsInliningOfIsInstance();

   /**
    * Return the proper linkage for this call, especially for the case when the methodSymbol
    * doesn't capture the complete information.
    *
    * @param[in] node, this calling node
    * @param[in] isIndirect true if this call is an indirect call
    * @return    appropriate linkage for this call
    */
   TR::Linkage *deriveCallingLinkage(TR::Node *node, bool isIndirect);

   bool isSnippetMatched(TR::Snippet *, int32_t, TR::SymbolReference *);

   bool mulDecompositionCostIsJustified(int numOfOperations, char bitPosition[], char operationType[], int64_t value);

   void emitDataSnippets();
   bool hasDataSnippets();

   TR::Instruction *generateSwitchToInterpreterPrePrologue(TR::Instruction *cursor, TR::Node *node);

   int32_t setEstimatedLocationsForDataSnippetLabels(int32_t estimatedSnippetStart);

#ifdef DEBUG
   void dumpDataSnippets(TR::FILE *outFile);
#endif

   bool considerTypeForGRA(TR::Node *node);
   bool considerTypeForGRA(TR::DataType dt);
   bool considerTypeForGRA(TR::SymbolReference *symRef);

   void findOrCreateFloatConstant(void *v, TR::DataType t,
                  TR::Instruction *n0, TR::Instruction *n1,
                  TR::Instruction *n2, TR::Instruction *n3);
   void findOrCreateAddressConstant(void *v, TR::DataType t,
                  TR::Instruction *n0, TR::Instruction *n1,
                  TR::Instruction *n2, TR::Instruction *n3,
                  TR::Node *node, bool isUnloadablePicSite);

   // different from evaluateNode in that it returns a clobberable register
   TR::Register *gprClobberEvaluate(TR::Node *node);

   const TR::PPCLinkageProperties &getProperties() { return *_linkageProperties; }

   using OMR::CodeGenerator::apply16BitLabelRelativeRelocation;
   void apply16BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);
   void apply24BitLabelRelativeRelocation(int32_t * cursor, TR::LabelSymbol *);

   TR::RealRegister *getStackPointerRegister()
      {
      return _stackPtrRegister;
      }
   TR::RealRegister *setStackPointerRegister(TR::RealRegister *r) {return (_stackPtrRegister = r);}

   TR::RealRegister *getMethodMetaDataRegister()                       {return _methodMetaDataRegister;}
   TR::RealRegister *setMethodMetaDataRegister(TR::RealRegister *r) {return (_methodMetaDataRegister = r);}

   TR::RealRegister *getTOCBaseRegister()                       {return _tocBaseRegister;}
   TR::RealRegister *setTOCBaseRegister(TR::RealRegister *r)  {return (_tocBaseRegister = r);}

   uintptr_t *getTOCBase();

   TR_PPCScratchRegisterManager* generateScratchRegisterManager(int32_t capacity = 32);

   void buildRegisterMapForInstruction(TR_GCStackMap *map);

   int32_t getPreferredLoopUnrollFactor();

   bool canTransformUnsafeCopyToArrayCopy();
   bool canTransformUnsafeSetMemory();
   bool supportsMergingGuards() {return false;}

   virtual bool supportsAESInstructions();

   virtual bool getSupportsTLE()
      {
      return false;
      }

   static bool getSupportsOpCodeForAutoSIMD(TR::CPU *cpu, TR::ILOpCode opcode);
   bool getSupportsOpCodeForAutoSIMD(TR::ILOpCode opcode);

   bool getSupportsEncodeUtf16LittleWithSurrogateTest();

   bool getSupportsEncodeUtf16BigWithSurrogateTest();

   int32_t arrayInitMinimumNumberOfBytes() {return 32;}

   TR::SymbolReference &getDouble2LongSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCdouble2Long); }
   TR::SymbolReference &getDoubleRemainderSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCdoubleRemainder); }
   TR::SymbolReference &getInteger2DoubleSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinteger2Double); }
   TR::SymbolReference &getLong2DoubleSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPClong2Double); }
   TR::SymbolReference &getLong2FloatSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPClong2Float); }
   TR::SymbolReference &getLong2Float_mvSymbolReference()  { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPClong2Float_mv); }
   TR::SymbolReference &getLongMultiplySymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPClongMultiply); }
   TR::SymbolReference &getLongDivideSymbolReference()   { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPClongDivide); }
   TR::SymbolReference &getUnresolvedStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStaticGlue); }
   TR::SymbolReference &getUnresolvedSpecialGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedSpecialGlue); }
   TR::SymbolReference &getUnresolvedDirectVirtualGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedDirectVirtualGlue); }
   TR::SymbolReference &getUnresolvedClassGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedClassGlue); }
   TR::SymbolReference &getUnresolvedClassGlue2SymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedClassGlue2); }
   TR::SymbolReference &getUnresolvedStringGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStringGlue); }
   TR::SymbolReference &getUnresolvedStaticDataGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStaticDataGlue); }
   TR::SymbolReference &getUnresolvedStaticDataStoreGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedStaticDataStoreGlue); }
   TR::SymbolReference &getUnresolvedInstanceDataGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedInstanceDataGlue); }
   TR::SymbolReference &getUnresolvedInstanceDataStoreGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterUnresolvedInstanceDataStoreGlue); }
   TR::SymbolReference &getVirtualUnresolvedHelperSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCvirtualUnresolvedHelper); }
   TR::SymbolReference &getInterfaceCallHelperSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterfaceCallHelper); }
   TR::SymbolReference &getItrgSendVirtual0SymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtual0); }
   TR::SymbolReference &getItrgSendVirtual1SymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtual1); }
   TR::SymbolReference &getItrgSendVirtualJSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualJ); }
   TR::SymbolReference &getItrgSendVirtualFSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualF); }
   TR::SymbolReference &getItrgSendVirtualDSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualD); }
   TR::SymbolReference &getVoidStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterVoidStaticGlue); }
   TR::SymbolReference &getSyncVoidStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterSyncVoidStaticGlue); }
   TR::SymbolReference &getGPR3StaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterGPR3StaticGlue); }
   TR::SymbolReference &getSyncGPR3StaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterSyncGPR3StaticGlue); }
   TR::SymbolReference &getGPR3GPR4StaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterGPR3GPR4StaticGlue); }
   TR::SymbolReference &getSyncGPR3GPR4StaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterSyncGPR3GPR4StaticGlue); }
   TR::SymbolReference &getFPR0FStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterFPR0FStaticGlue); }
   TR::SymbolReference &getSyncFPR0FStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterSyncFPR0FStaticGlue); }
   TR::SymbolReference &getFPR0DStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterFPR0DStaticGlue); }
   TR::SymbolReference &getSyncFPR0DStaticGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinterpreterSyncFPR0DStaticGlue); }
   TR::SymbolReference &getNativeStaticHelperForUnresolvedGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCnativeStaticHelperForUnresolvedGlue); }
   TR::SymbolReference &getNativeStaticHelperSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCnativeStaticHelper); }
   TR::SymbolReference &getCollapseJNIReferenceFrameSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCcollapseJNIReferenceFrame); }
   TR::SymbolReference &getArrayCopySymbolReference();
   TR::SymbolReference &getWordArrayCopySymbolReference();
   TR::SymbolReference &getHalfWordArrayCopySymbolReference();
   TR::SymbolReference &getForwardArrayCopySymbolReference();
   TR::SymbolReference &getForwardWordArrayCopySymbolReference();
   TR::SymbolReference &getForwardHalfWordArrayCopySymbolReference();
   TR::SymbolReference &getReferenceArrayCopySymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCreferenceArrayCopy); }
   TR::SymbolReference &getGeneralArrayCopySymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCgeneralArrayCopy); }
   TR::SymbolReference &getSamplingPatchCallSiteSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCsamplingPatchCallSite); }
   TR::SymbolReference &getSamplingRecompileMethodSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCsamplingRecompileMethod); }
   TR::SymbolReference &getCountingPatchCallSiteSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCcountingPatchCallSite); }
   TR::SymbolReference &getCountingRecompileMethodSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCcountingRecompileMethod); }
   TR::SymbolReference &getInduceRecompilationSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCinduceRecompilation); }
   TR::SymbolReference &getRevertToInterpreterGlueSymbolReference() { return *_symRefTab->findOrCreateRuntimeHelper(TR_PPCrevertToInterpreterGlue); }

   bool hasCall()                 {return _flags.testAny(HasCall);}
   bool noStackFrame()            {return _flags.testAny(NoStackFrame);}
   bool canExceptByTrap()         {return _flags.testAny(CanExceptByTrap);}
   bool enableTLHPrefetching()    {return _flags.testAny(EnableTLHPrefetching);}
   bool isOutOfLineHotPath()      {return _flags.testAny(IsOutOfLineHotPath);}

   void setHasCall()              { _flags.set(HasCall);}
   void setNoStackFrame()         { _flags.set(NoStackFrame);}
   void setCanExceptByTrap()      { _flags.set(CanExceptByTrap);}
   void setEnableTLHPrefetching() { _flags.set(EnableTLHPrefetching);}
   void setIsOutOfLineHotPath(bool v)   { _flags.set(IsOutOfLineHotPath, v);}

   void setIsDualTLH()           { _flags.set(IsDualTLH);}
   bool isDualTLH()              {return _flags.testAny(IsDualTLH);}

   TR_BitVector  *getBlockCallInfo() {return _blockCallInfo;}
   TR_BitVector  *setBlockCallInfo(TR_BitVector *v) {return (_blockCallInfo = v);}

   TR_Array<TR::Register *> &getTransientLongRegisters() {return _transientLongRegisters;}
   void freeAndResetTransientLongs();

   TR_Array<TR::SymbolReference *> *getTrackStatics() {return _trackStatics;}
   void setTrackStatics(TR_Array<TR::SymbolReference *> *p) {_trackStatics = p;}
   void staticTracking(TR::SymbolReference *symRef);

   TR_Array<TR_PPCLoadLabelItem *> *getTrackItems() {return _trackItems;}
   void setTrackItems(TR_Array<TR_PPCLoadLabelItem *> *p) {_trackItems = p;}
   void itemTracking(int32_t offset, TR::LabelSymbol *sym);

   void setRealRegisterAssociation(TR::Register *vr, TR::RealRegister::RegNum rn);
   void addRealRegisterInterference(TR::Register *vr, TR::RealRegister::RegNum rn);

   TR_GlobalRegisterNumber pickRegister(TR::RegisterCandidate *, TR::Block * *, TR_BitVector & availableRegisters, TR_GlobalRegisterNumber &, TR_LinkHead<TR::RegisterCandidate> *candidates);
   bool allowGlobalRegisterAcrossBranch(TR::RegisterCandidate *, TR::Node * branchNode);
   using OMR::CodeGenerator::getMaximumNumberOfGPRsAllowedAcrossEdge;
   int32_t getMaximumNumberOfGPRsAllowedAcrossEdge(TR::Node *);
   int32_t getMaximumNumberOfFPRsAllowedAcrossEdge(TR::Node *);
   bool isGlobalRegisterAvailable(TR_GlobalRegisterNumber i, TR::DataType dt);

   TR_BitVector _globalRegisterBitVectors[TR_numSpillKinds];
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return &_globalRegisterBitVectors[kind]; }

   TR_GlobalRegisterNumber _gprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters], _fprLinkageGlobalRegisterNumbers[TR::RealRegister::NumRegisters]; // these could be smaller
   TR_GlobalRegisterNumber getLinkageGlobalRegisterNumber(int8_t linkageRegisterIndex, TR::DataType type);

   virtual void simulateNodeEvaluation(TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);

   bool internalPointerSupportImplemented() {return true;}

   bool materializesLargeConstants() { return true;}

   bool needsNormalizationBeforeShifts();

   bool getSupportsBitOpCodes() {return true;}

   bool supportsSinglePrecisionSQRT();
   bool supportsFusedMultiplyAdd() {return true;}
   bool supportsNegativeFusedMultiplyAdd() {return true;}

   bool supportsNonHelper(TR::SymbolReferenceTable::CommonNonhelperSymbol symbol);

   bool getSupportsTenuredObjectAlignment() { return true; }
   bool isObjectOfSizeWorthAligning(uint32_t size)
      {
      uint32_t lineSize = 64;
      return  ((size < (lineSize<<1)) && (size > (lineSize >> 2)));
      }

   uint32_t getJitMethodEntryAlignmentBoundary();

   using OMR::CodeGenerator::getSupportsConstantOffsetInAddressing;
   bool getSupportsConstantOffsetInAddressing(int64_t value) { return (value>=LOWER_IMMED) && (value<=UPPER_IMMED);}

   bool supportsPassThroughCopyToNewVirtualRegister() { return true; }

   bool branchesAreExpensive()
      {
      // Currently returning true
      // Should be changed on PPC to account for processors with good
      // branch prediction (return false in that case)
      //
      return true;
      }

   TR::RealRegister *regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk);

   static uint32_t registerBitMask(int32_t reg);

   int32_t getMaximumNumbersOfAssignableGPRs();
   int32_t getMaximumNumbersOfAssignableFPRs();
   int32_t getMaximumNumbersOfAssignableVRs();
   bool doRematerialization();

   bool isRotateAndMask(TR::Node * node);

   // PPC specific thresholds for constant re-materialization
   int64_t getLargestNegConstThatMustBeMaterialized() {return -32769;}  // minimum 16-bit signed int minus 1
   int64_t getSmallestPosConstThatMustBeMaterialized() {return 32768;}  // maximum 16-bit signed int plus 1
   bool shouldValueBeInACommonedNode(int64_t);

   static bool isILOpCodeSupported(TR::ILOpCodes);
   // Constant Data update
   bool checkAndFetchRequestor(TR::Instruction *instr, TR::Instruction **q);

   // OutOfLineCodeSection List functions
   TR::list<TR_PPCOutOfLineCodeSection*> &getPPCOutOfLineCodeSectionList() {return _outOfLineCodeSectionList;}
   TR_PPCOutOfLineCodeSection *findOutLinedInstructionsFromLabel(TR::LabelSymbol *label);
   TR::Snippet *findSnippetInstructionsFromLabel(TR::LabelSymbol *label);

   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR::RegisterDependencyConditions *cond);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, int32_t delta, TR_ScratchRegisterManager &srm);
   TR::Instruction *generateDebugCounterBump(TR::Instruction *cursor, TR::DebugCounterBase *counter, TR::Register *deltaReg, TR_ScratchRegisterManager &srm);

   int32_t arrayTranslateMinimumNumberOfElements(bool isByteSource, bool isByteTarget) { return 8; } //FIXME
   int32_t arrayTranslateAndTestMinimumNumberOfIterations() { return 8; } //FIXME

   // Provide codeGen-specific hooks for class unloading events
   static void ppcCGOnClassUnloading(void * loaderPtr);

   TR::Instruction *loadAddressConstantFixed(
      TR::Node        *node,
      intptr_t         value,
      TR::Register    *targetRegister,
      TR::Instruction *cursor=NULL,
      TR::Register    *tempReg=NULL,
      int16_t         typeAddress = -1,
      bool            doAOTRelocation = true);

   TR::Instruction *loadIntConstantFixed(
      TR::Node        *node,
      int32_t         value,
      TR::Register    *targetRegister,
      TR::Instruction *cursor=NULL,
      int16_t         typeAddress = -1);

   TR::Instruction *fixedLoadLabelAddressIntoReg(
      TR::Node          *node,
      TR::Register      *trgReg,
      TR::LabelSymbol    *label,
      TR::Instruction   *cursor=NULL,
      TR::Register      *tempReg=NULL,
      bool              useADDISFor32Bit = false);

   void addMetaDataForLoadIntConstantFixed(
      TR::Node *node,
      TR::Instruction *firstInstruction,
      TR::Instruction *secondInstruction,
      int16_t typeAddress,
      int32_t value);

   void addMetaDataForLoadAddressConstantFixed(
      TR::Node *node,
      TR::Instruction *firstInstruction,
      TR::Register *tempReg,
      int16_t typeAddress,
      intptr_t value);

   void addMetaDataFor32BitFixedLoadLabelAddressIntoReg(
      TR::Node *node,
      TR::LabelSymbol *label,
      TR::Instruction *firstInstruction,
      TR::Instruction *secondInstruction);

   void addMetaDataFor64BitFixedLoadLabelAddressIntoReg(
      TR::Node *node,
      TR::LabelSymbol *label,
      TR::Register *tempReg,
      TR::Instruction **q);

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
    * @brief Gets the preferred alignment of hot loops in bytes.
    *
    * In order to reduce the number of unnecessary instructions fetched when jumping back to the
    * start of a loop, hot loops should be aligned to match the boundary on which the CPU will
    * begin fetching instructions. This function determines what alignment should be used when
    * doing this.
    */
   uint32_t getHotLoopAlignment();

   TR::LabelSymbol *getStartPCLabel();

   private:

   enum // flags
      {
      HasCall               = 0x00000200,
      NoStackFrame          = 0x00000400,
      CanExceptByTrap       = 0x00000800,
      EnableTLHPrefetching  = 0x00001000,
      IsOutOfLineColdPath   = 0x00002000, // AVAILABLE
      IsOutOfLineHotPath    = 0x00004000,
      IsDualTLH             = 0x00008000,
      DummyLastFlag
      };


   TR_BitVector *computeCallInfoBitVector();




   TR_PPCBinaryEncodingData _binaryEncodingData;
   TR::RealRegister              *_stackPtrRegister;
   TR::RealRegister              *_methodMetaDataRegister;
   TR::RealRegister              *_tocBaseRegister;
   TR::PPCImmInstruction            *_returnTypeInfoInstruction;
   TR::ConstantDataSnippet       *_constantData;
   const TR::PPCLinkageProperties   *_linkageProperties;
   TR_BitVector                    *_blockCallInfo;
   TR_Array<TR::SymbolReference *>  *_trackStatics;
   TR_Array<TR_PPCLoadLabelItem *> *_trackItems;
   TR_Array<TR::Register *>          _transientLongRegisters;
   TR::list<TR_PPCOutOfLineCodeSection*> _outOfLineCodeSectionList;
   flags32_t                        _flags;

   uint32_t                         _numGPR;
   uint32_t                         _firstGPR;
   uint32_t                         _lastGPR;
   uint32_t                         _firstParmGPR;
   uint32_t                         _lastVolatileGPR;
   uint32_t                         _numFPR;
   uint32_t                         _firstFPR;
   uint32_t                         _lastFPR;
   uint32_t                         _firstParmFPR;
   uint32_t                         _lastVolatileFPR;
   uint32_t *                       _tbTableStart;
   uint32_t *                       _tbTabletbOff;
   uint32_t *                       _tbTableEnd;
   uint32_t                         _numVRF;

   TR::LabelSymbol *_startPCLabel;
   };

} // PPC

} // TR


class TR_PPCScratchRegisterManager : public TR_ScratchRegisterManager
   {
   public:
   TR_PPCScratchRegisterManager(int32_t capacity, TR::CodeGenerator *cg) : TR_ScratchRegisterManager(capacity, cg) {}
   using TR_ScratchRegisterManager::addScratchRegistersToDependencyList;
   void addScratchRegistersToDependencyList(TR::RegisterDependencyConditions *deps, bool excludeGPR0);
   };

   void mulConstant(
      TR::Node *,
      TR::Register *trgReg,
      TR::Register *sourceReg,
      int32_t value,
      TR::CodeGenerator *cg);

   void mulConstant(
      TR::Node *,
      TR::Register *trgReg,
      TR::Register *sourceReg,
      int64_t value,
      TR::CodeGenerator *cg);


   TR::Register *addConstantToLong(
      TR::Node * node,
      TR::Register *srcReg,
      int64_t value,
      TR::Register *trgReg,
      TR::CodeGenerator *cg);

   TR::Register *addConstantToLong(
      TR::Node *node,
      TR::Register *srcHigh,
      TR::Register *srcLow,
      int32_t valHigh,
      int32_t valLow,
      TR::CodeGenerator *cg);

   TR::Register *addConstantToInteger(
      TR::Node * node,
      TR::Register *srcReg,
      int32_t value,
      TR::CodeGenerator *cg);

   TR::Register *addConstantToInteger(
      TR::Node * node,
      TR::Register *trgReg,
      TR::Register *srcReg,
      int32_t value,
      TR::CodeGenerator *cg);

#endif
