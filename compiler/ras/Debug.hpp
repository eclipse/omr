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

/*  __   ___  __   __   ___  __       ___  ___  __
 * |  \ |__  |__) |__) |__  /  `  /\   |  |__  |  \
 * |__/ |___ |    |  \ |___ \__, /~~\  |  |___ |__/
 *
 * This file is now deprecated and its contents are slowly
 * being moved back to codegen and other directories. Please do not
 * add more interfaces here.
 */

#ifndef DEBUG_INCL
#define DEBUG_INCL

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/Machine.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/Method.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/RawAllocator.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/TRlist.hpp"
#include "optimizer/Optimizations.hpp"
#include "runtime/Runtime.hpp"
#include "infra/CfgNode.hpp"

#include "codegen/RegisterRematerializationInfo.hpp"

class TR_Debug;
class TR_BlockStructure;
class TR_CHTable;

namespace TR {
class CompilationFilters;
}
class TR_FilterBST;
class TR_FrontEnd;
class TR_GCStackMap;
class TR_InductionVariable;
class TR_PseudoRandomNumbersListElement;
class TR_RegionAnalysis;
class TR_RegionStructure;
class TR_RematerializationInfo;
class TR_ResolvedMethod;
class TR_Structure;
class TR_StructureSubGraphNode;

namespace OMR {
class Logger;
} // namespace OMR

namespace TR {
class VPConstraint;
class GCStackAtlas;
class AutomaticSymbol;
class Block;
class CFG;
class CFGEdge;
class CFGNode;
class CodeGenerator;
class Compilation;
class DebugCounterGroup;
class GCRegisterMap;
class InstOpCode;
class Instruction;
class LabelSymbol;
class Machine;
class MemoryReference;
class Node;
class OptionSet;
class Options;
class RealRegister;
class Register;
class RegisterDependencyGroup;
class RegisterDependency;
class RegisterDependencyConditions;
class RegisterMappedSymbol;
class ResolvedMethodSymbol;
class SimpleRegex;
class Snippet;
class Symbol;
class SymbolReference;
class SymbolReferenceTable;
class TreeTop;
struct OptionTable;
} // namespace TR
template<class T> class List;
template<class T> class ListIterator;

struct J9JITExceptionTable;
struct J9AnnotationInfo;
struct J9AnnotationInfoEntry;
struct J9PortLibrary;

namespace TR {
class X86NoOperandInstruction;
class X86LabelInstruction;
class X86PaddingInstruction;
class X86AlignmentInstruction;
class X86BoundaryAvoidanceInstruction;
class X86PatchableCodeAlignmentInstruction;
class X86FenceInstruction;
class X86VirtualGuardNOPInstruction;
class X86ImmInstruction;
class X86ImmSnippetInstruction;
class X86ImmSymInstruction;
class X86RegInstruction;
class X86RegRegInstruction;
class X86RegImmInstruction;
class X86RegRegImmInstruction;
class X86RegRegRegInstruction;
class X86RegMaskRegRegInstruction;
class X86RegMaskRegRegImmInstruction;
class X86RegMaskRegInstruction;
class X86RegMaskMemInstruction;
class X86MemInstruction;
class X86MemImmInstruction;
class X86MemRegInstruction;
class X86MemMaskRegInstruction;
class X86MemRegImmInstruction;
class X86RegMemInstruction;
class X86RegMemImmInstruction;
class X86RegRegMemInstruction;
class X86RestartSnippet;
class X86PicDataSnippet;
class X86DivideCheckSnippet;
class X86FPConvertToIntSnippet;
class X86FPConvertToLongSnippet;
class X86GuardedDevirtualSnippet;
class X86HelperCallSnippet;
class UnresolvedDataSnippet;
class AMD64Imm64Instruction;
class AMD64Imm64SymInstruction;
class AMD64RegImm64Instruction;
} // namespace TR

struct TR_VFPState;

namespace TR {
class X86VFPSaveInstruction;
class X86VFPRestoreInstruction;
class X86VFPDedicateInstruction;
class X86VFPReleaseInstruction;
class X86VFPCallCleanupInstruction;
} // namespace TR

#ifdef J9_PROJECT_SPECIFIC
namespace TR {
class X86CallSnippet;
class X86CheckFailureSnippet;
class X86CheckFailureSnippetWithResolve;
class X86BoundCheckWithSpineCheckSnippet;
class X86SpineCheckSnippet;
class X86ForceRecompilationSnippet;
class X86RecompilationSnippet;
} // namespace TR
#endif

namespace TR {
class PPCAlignmentNopInstruction;
class PPCDepInstruction;
class PPCLabelInstruction;
class PPCDepLabelInstruction;
class PPCConditionalBranchInstruction;
class PPCDepConditionalBranchInstruction;
class PPCAdminInstruction;
class PPCImmInstruction;
class PPCSrc1Instruction;
class PPCDepImmSymInstruction;
class PPCTrg1Instruction;
class PPCTrg1Src1Instruction;
class PPCTrg1ImmInstruction;
class PPCTrg1Src1ImmInstruction;
class PPCTrg1Src1Imm2Instruction;
class PPCSrc2Instruction;
class PPCSrc3Instruction;
class PPCTrg1Src2Instruction;
class PPCTrg1Src2ImmInstruction;
class PPCTrg1Src3Instruction;
class PPCMemSrc1Instruction;
class PPCMemInstruction;
class PPCTrg1MemInstruction;
class PPCControlFlowInstruction;
class PPCVirtualGuardNOPInstruction;
class PPCUnresolvedCallSnippet;
class PPCVirtualSnippet;
class PPCVirtualUnresolvedSnippet;
class PPCInterfaceCallSnippet;
class PPCHelperCallSnippet;
class PPCMonitorEnterSnippet;
class PPCMonitorExitSnippet;
class PPCReadMonitorSnippet;
} // namespace TR

namespace TR {
class PPCAllocPrefetchSnippet;
}

namespace TR {
class PPCLockReservationEnterSnippet;
class PPCLockReservationExitSnippet;
class PPCArrayCopyCallSnippet;
} // namespace TR

#ifdef J9_PROJECT_SPECIFIC
namespace TR {
class PPCInterfaceCastSnippet;
class PPCStackCheckFailureSnippet;
class PPCForceRecompilationSnippet;
class PPCRecompilationSnippet;
class PPCCallSnippet;
} // namespace TR
#endif

namespace TR {
class ARMLabelInstruction;
class ARMConditionalBranchInstruction;
class ARMVirtualGuardNOPInstruction;
class ARMAdminInstruction;
class ARMImmInstruction;
class ARMImmSymInstruction;
class ARMTrg1Src2Instruction;
class ARMTrg2Src1Instruction;
class ARMMulInstruction;
class ARMMemSrc1Instruction;
class ARMTrg1Instruction;
class ARMMemInstruction;
class ARMTrg1MemInstruction;
class ARMTrg1MemSrc1Instruction;
class ARMControlFlowInstruction;
class ARMMultipleMoveInstruction;
} // namespace TR
class TR_ARMMemoryReference;
class TR_ARMOperand2;
class TR_ARMRealRegister;

namespace TR {
class ARMCallSnippet;
class ARMUnresolvedCallSnippet;
class ARMVirtualSnippet;
class ARMVirtualUnresolvedSnippet;
class ARMInterfaceCallSnippet;
class ARMHelperCallSnippet;
class ARMMonitorEnterSnippet;
class ARMMonitorExitSnippet;
class ARMStackCheckFailureSnippet;
class ARMRecompilationSnippet;
} // namespace TR

namespace TR {
class S390LabelInstruction;
class S390BranchInstruction;
class S390BranchOnCountInstruction;
class S390VirtualGuardNOPInstruction;
class S390BranchOnIndexInstruction;
class S390AnnotationInstruction;
class S390PseudoInstruction;
class S390ImmInstruction;
class S390ImmSnippetInstruction;
class S390ImmSymInstruction;
class S390Imm2Instruction;
class S390RegInstruction;
class S390RRInstruction;
class S390TranslateInstruction;
class S390RRFInstruction;
class S390RRRInstruction;
class S390RXFInstruction;
class S390RIInstruction;
class S390RILInstruction;
class S390RSInstruction;
class S390RSLInstruction;
class S390RSLbInstruction;
class S390RXEInstruction;
class S390RXInstruction;
class S390MemInstruction;
class S390SS1Instruction;
class S390MIIInstruction;
class S390SMIInstruction;
class S390SS2Instruction;
class S390SS4Instruction;
class S390SSEInstruction;
class S390SSFInstruction;
class S390VRIInstruction;
class S390VRIaInstruction;
class S390VRIbInstruction;
class S390VRIcInstruction;
class S390VRIdInstruction;
class S390VRIeInstruction;
class S390VRRInstruction;
class S390VRRaInstruction;
class S390VRRbInstruction;
class S390VRRcInstruction;
class S390VRRdInstruction;
class S390VRReInstruction;
class S390VRRfInstruction;
class S390VRSaInstruction;
class S390VRSbInstruction;
class S390VRScInstruction;
class S390VRVInstruction;
class S390VRXInstruction;
class S390VStorageInstruction;
class S390OpCodeOnlyInstruction;
class S390IInstruction;
class S390SInstruction;
class S390SIInstruction;
class S390SILInstruction;
class S390NOPInstruction;
class S390AlignmentNopInstruction;
class S390RestoreGPR7Snippet;
class S390CallSnippet;
class S390ConstantDataSnippet;
class S390WritableDataSnippet;
class S390HelperCallSnippet;
class S390JNICallDataSnippet;
} // namespace TR

namespace TR {
class S390StackCheckFailureSnippet;
class S390HeapAllocSnippet;
class S390RRSInstruction;
class S390RIEInstruction;
class S390RISInstruction;
class S390IEInstruction;
} // namespace TR

#ifdef J9_PROJECT_SPECIFIC
namespace TR {
class S390ForceRecompilationSnippet;
class S390ForceRecompilationDataSnippet;
class S390J9CallSnippet;
class S390UnresolvedCallSnippet;
class S390VirtualSnippet;
class S390VirtualUnresolvedSnippet;
class S390InterfaceCallSnippet;
class J9S390InterfaceCallDataSnippet;
} // namespace TR
#endif

namespace TR {
class ARM64ImmInstruction;
class ARM64RelocatableImmInstruction;
class ARM64ImmSymInstruction;
class ARM64LabelInstruction;
class ARM64BranchInstruction;
class ARM64ConditionalBranchInstruction;
class ARM64CompareBranchInstruction;
class ARM64TestBitBranchInstruction;
class ARM64RegBranchInstruction;
class ARM64AdminInstruction;
class ARM64Trg1Instruction;
class ARM64Trg1CondInstruction;
class ARM64Trg1ImmInstruction;
class ARM64Trg1ImmShiftedInstruction;
class ARM64Trg1ImmSymInstruction;
class ARM64Trg1Src1Instruction;
class ARM64Trg1ZeroSrc1Instruction;
class ARM64Trg1ZeroImmInstruction;
class ARM64Trg1Src1ImmInstruction;
class ARM64Trg1Src2Instruction;
class ARM64CondTrg1Src2Instruction;
class ARM64Trg1Src2ImmInstruction;
class ARM64Trg1Src2ShiftedInstruction;
class ARM64Trg1Src2ExtendedInstruction;
class ARM64Trg1Src2IndexedElementInstruction;
class ARM64Trg1Src2ZeroInstruction;
class ARM64Trg1Src3Instruction;
class ARM64Trg1MemInstruction;
class ARM64Trg2MemInstruction;
class ARM64MemInstruction;
class ARM64MemImmInstruction;
class ARM64MemSrc1Instruction;
class ARM64MemSrc2Instruction;
class ARM64Trg1MemSrc1Instruction;
class ARM64Src1Instruction;
class ARM64ZeroSrc1ImmInstruction;
class ARM64Src2Instruction;
class ARM64ZeroSrc2Instruction;
class ARM64Src1ImmCondInstruction;
class ARM64Src2CondInstruction;
class ARM64SynchronizationInstruction;
class ARM64HelperCallSnippet;
} // namespace TR

namespace TR {
class LabelInstruction;
class AdminInstruction;
} // namespace TR
#ifdef J9_PROJECT_SPECIFIC
namespace TR {
class ARM64VirtualGuardNOPInstruction;
}

namespace TR {
class ARM64InterfaceCallSnippet;
class ARM64StackCheckFailureSnippet;
class ARM64ForceRecompilationSnippet;
class ARM64RecompilationSnippet;
class ARM64CallSnippet;
class ARM64UnresolvedCallSnippet;
class ARM64VirtualUnresolvedSnippet;
} // namespace TR
#endif

namespace TR {
class DataInstruction;
class RtypeInstruction;
class ItypeInstruction;
class StypeInstruction;
class BtypeInstruction;
class UtypeInstruction;
class JtypeInstruction;
class LoadInstruction;
class StoreInstruction;
class RVHelperCallSnippet;
} // namespace TR

TR_Debug *createDebugObject(TR::Compilation *);

class TR_Debug {
public:
    void *operator new(size_t s, TR_HeapMemory m);
    void *operator new(size_t s, TR::PersistentAllocator &allocator);

    TR_Debug(TR::Compilation *c);

    OMR::Logger *getLogger() { return _logger; }

    void setLogger(OMR::Logger *log) { _logger = log; }

    virtual void resetDebugData();
    virtual void newNode(TR::Node *);
    virtual void newLabelSymbol(TR::LabelSymbol *);
    virtual void newRegister(TR::Register *);
    virtual void newVariableSizeSymbol(TR::AutomaticSymbol *sym);
    virtual void newInstruction(TR::Instruction *);
    virtual void addInstructionComment(TR::Instruction *, char *, ...);

    /**
     * @brief Check whether to trigger debugger breakpoint or launch debugger
     *        when \c artifact name is created.
     *
     * @param[in] artifactName : name of artifact to break or debug on
     */
    void breakOrDebugOnCreate(char *artifactName);

    virtual TR::CompilationFilters *getInlineFilters() { return _inlineFilters; }

    virtual TR_FrontEnd *fe() { return _fe; }

    virtual TR::Compilation *comp() { return _comp; }

    virtual char *formattedString(char *buf, uint32_t bufLen, const char *format, va_list args,
        TR_AllocationKind = heapAlloc);

    virtual TR::Node *getCurrentParent() { return _currentParent; }

    virtual void setCurrentParentAndChildIndex(TR::Node *n, int32_t i)
    {
        _currentParent = n;
        _currentChildIndex = i;
    }

    virtual void setCurrentParent(TR::Node *n) { _currentParent = n; }

    virtual int32_t getCurrentChildIndex() { return _currentChildIndex; }

    virtual void setCurrentChildIndex(int32_t i) { _currentChildIndex = i; }

    // Print current stack back trace to standard error
    static void printStackBacktrace();

    // Print current stack back trace to trace log
    // Argument: comp is the current compilation object, which cannot be NULL
    static void printStackBacktraceToTraceLog(TR::Compilation *comp);

    void breakOn();

    void debugOnCreate();

    // Options processing
    //
    virtual void dumpOptionHelp(TR::OptionTable *jitOptions, TR::OptionTable *feOptions, TR::SimpleRegex *nameFilter);

    static void dumpOptions(const char *optionsType, const char *options, const char *envOptions,
        TR::Options *cmdLineOptions, TR::OptionTable *jitOptions, TR::OptionTable *feOptions, void *, TR_FrontEnd *);
    virtual const char *limitfileOption(const char *, void *, TR::OptionTable *, TR::Options *, bool loadLimit,
        TR_PseudoRandomNumbersListElement **pseudoRandomListHeadPtr = 0);
    virtual const char *inlinefileOption(const char *, void *, TR::OptionTable *, TR::Options *);
    virtual const char *limitOption(const char *, void *, TR::OptionTable *, TR::Options *, bool loadLimit);
    const char *limitOption(const char *, void *, TR::OptionTable *, TR::Options *, TR::CompilationFilters *&);
    virtual int32_t *loadCustomStrategy(char *optFileName);
    virtual bool methodCanBeCompiled(TR_Memory *mem, TR_ResolvedMethod *, TR_FilterBST *&);
    virtual bool methodCanBeRelocated(TR_Memory *mem, TR_ResolvedMethod *, TR_FilterBST *&);
    virtual bool methodSigCanBeCompiled(const char *, TR_FilterBST *&, TR::Method::Type methodType);
    virtual bool methodSigCanBeRelocated(const char *, TR_FilterBST *&);
    virtual bool methodSigCanBeCompiledOrRelocated(const char *, TR_FilterBST *&, bool isRelocation,
        TR::Method::Type methodType);
    virtual bool methodCanBeFound(TR_Memory *, TR_ResolvedMethod *, TR::CompilationFilters *, TR_FilterBST *&);
    virtual bool methodSigCanBeFound(const char *, TR::CompilationFilters *, TR_FilterBST *&,
        TR::Method::Type methodType);

    virtual TR::CompilationFilters *getCompilationFilters() { return _compilationFilters; }

    virtual TR::CompilationFilters *getRelocationFilters() { return _relocationFilters; }

    virtual void clearFilters(TR::CompilationFilters *);
    void clearFilters(bool loadLimit);
    virtual bool scanInlineFilters(FILE *, int32_t &, TR::CompilationFilters *);
    virtual TR_FilterBST *addFilter(const char *&, int32_t, int32_t, int32_t, TR::CompilationFilters *);
    virtual TR_FilterBST *addFilter(const char *&, int32_t, int32_t, int32_t, bool loadLimit);
    virtual TR_FilterBST *addExcludedMethodFilter(bool loadLimit);
    virtual int32_t scanFilterName(const char *, TR_FilterBST *);
    virtual void printFilters(TR::CompilationFilters *);
    virtual void printFilters();
    virtual void print(TR_FilterBST *filter);
    virtual void printHeader(OMR::Logger *log);
    virtual void printMethodHotness(OMR::Logger *log);
    virtual void printInstrDumpHeader(OMR::Logger *log, const char *title);

    virtual void printByteCodeAnnotations(OMR::Logger *log);
    virtual void printAnnotationInfoEntry(OMR::Logger *log, J9AnnotationInfo *, J9AnnotationInfoEntry *, int32_t);

    virtual void printOptimizationHeader(OMR::Logger *log, const char *, const char *, int32_t, bool mustBeDone);

    virtual const char *getName(TR::ILOpCode);
    virtual const char *getName(TR::ILOpCodes);
    virtual const char *getName(TR::DataType);
    virtual const char *getName(TR_RawBCDSignCode);
    virtual const char *getName(TR::LabelSymbol *);
    virtual const char *getName(TR::SymbolReference *);
    virtual const char *getName(TR::Register *, TR_RegisterSizes = TR_WordReg);

    /**
     * @brief Answers whether the type of data stored in a register should
     *     influence the register size being used. This is primarily
     *     useful when printing register names and ensuring the best
     *     register name is printed for the context.
     *
     * @return true if this platform should use the data type to refine the
     *     register size; false otherwise.
     */
    virtual bool refineRegisterSizeForDataType();
    virtual const char *getRealRegisterName(uint32_t regNum);
    virtual const char *getGlobalRegisterName(TR_GlobalRegisterNumber regNum, TR_RegisterSizes size = TR_WordReg);

    /**
     * @brief Returns a const char * string corresponding to the given register number.
     *     The choice of register name is further refined by the data type if desired
     *     by a particular platform.
     *
     * @param[in] regNum : the register to return the name for
     * @param[in] dt : the data type of the data to be stored in the register
     *
     * @return a const char * string for the name of the register
     */
    virtual const char *getGlobalRegisterName(TR_GlobalRegisterNumber regNum, TR::DataType dt);
    virtual const char *getName(TR::Node *);
    virtual const char *getName(TR::Symbol *);
    virtual const char *getName(TR::Instruction *);
    virtual const char *getName(TR_Structure *);
    virtual const char *getName(TR::CFGNode *);

    virtual const char *getName(TR_ResolvedMethod *m) { return getName((void *)m, "(TR_ResolvedMethod*)", 0, false); }

    virtual const char *getName(TR_OpaqueClassBlock *c)
    {
        return getName((void *)c, "(TR_OpaqueClassBlock*)", 0, false);
    }

    virtual const char *getName(void *, const char *, uint32_t, bool);

    virtual const char *getName(const char *s) { return s; }

    virtual const char *getName(const char *s, int32_t len) { return s; }

    virtual const char *getName(TR_YesNoMaybe value);
    virtual const char *getVSSName(TR::AutomaticSymbol *sym);
    virtual const char *getWriteBarrierKindName(int32_t);
    virtual const char *getSpillKindName(uint8_t);
    virtual const char *getLinkageConventionName(uint8_t);
    virtual const char *getVirtualGuardKindName(TR_VirtualGuardKind kind);
    virtual const char *getVirtualGuardTestTypeName(TR_VirtualGuardTestType testType);
    virtual const char *getRuntimeHelperName(int32_t);

    virtual void roundAddressEnumerationCounters(uint32_t boundary = 16);

    virtual void print(OMR::Logger *log, uint8_t *, uint8_t *);
    virtual void printIRTrees(OMR::Logger *log, const char *, TR::ResolvedMethodSymbol *);
    virtual void printBlockOrders(OMR::Logger *log, const char *, TR::ResolvedMethodSymbol *);
    virtual void print(OMR::Logger *log, TR::CFG *);
    virtual void print(OMR::Logger *log, TR_Structure *structure, uint32_t indentation);
    virtual void print(OMR::Logger *log, TR_RegionAnalysis *structure, uint32_t indentation);
    virtual int32_t print(OMR::Logger *log, TR::TreeTop *);
    virtual int32_t print(OMR::Logger *log, TR::Node *, uint32_t indentation = 0, bool printSubtree = true);
    virtual int32_t print(OMR::Logger *log, TR::SymbolReference *, bool hideHelperMethodInfo = false,
        bool verbose = false);
    virtual void print(OMR::Logger *log, TR::LabelSymbol *);
    virtual void print(OMR::Logger *log, TR_BitVector *);
    virtual void print(OMR::Logger *log, TR_SingleBitContainer *);
    virtual void print(OMR::Logger *log, TR::BitVector *bv);
    virtual void print(OMR::Logger *log, TR::SparseBitVector *sparse);
    virtual void print(OMR::Logger *log, TR::SymbolReferenceTable *);
    virtual void printAliasInfo(OMR::Logger *log, TR::SymbolReferenceTable *);
    virtual void printAliasInfo(OMR::Logger *log, TR::SymbolReference *);

    virtual int32_t printWithFixedPrefix(OMR::Logger *log, TR::Node *, uint32_t indentation, bool printChildren,
        bool printRefCounts, const char *prefix);
    virtual void printVCG(OMR::Logger *log, TR::CFG *, const char *);
    virtual void printVCG(OMR::Logger *log, TR::Node *, uint32_t indentation);

    virtual void print(OMR::Logger *log, J9JITExceptionTable *data, TR_ResolvedMethod *feMethod, bool fourByteOffsets);

    virtual void clearNodeChecklist();
    virtual void saveNodeChecklist(TR_BitVector &saveArea);
    virtual void restoreNodeChecklist(TR_BitVector &saveArea);
    virtual void dumpLiveRegisters(OMR::Logger *log);
    virtual void setupToDumpTreesAndInstructions(const char *);
    virtual void setupToDumpTreesAndInstructions(OMR::Logger *log, const char *);
    virtual void dumpSingleTreeWithInstrs(TR::TreeTop *, TR::Instruction *, bool, bool, bool, bool);
    virtual void dumpSingleTreeWithInstrs(OMR::Logger *log, TR::TreeTop *, TR::Instruction *, bool, bool, bool, bool);
    virtual void dumpMethodInstrs(OMR::Logger *log, const char *, bool, bool header = false);
    virtual void dumpMixedModeDisassembly(OMR::Logger *log);
    virtual void dumpInstructionComments(OMR::Logger *log, TR::Instruction *, bool needsStartComment = true);
    virtual void print(OMR::Logger *log, TR::Instruction *);
    virtual void print(OMR::Logger *log, TR::Instruction *, const char *);
    virtual void print(OMR::Logger *log, TR::list<TR::Snippet *> &);
    virtual void print(OMR::Logger *log, TR::Snippet *);

    virtual void print(OMR::Logger *log, TR::RegisterMappedSymbol *, bool);
    virtual void print(OMR::Logger *log, TR::GCStackAtlas *);
    virtual void print(OMR::Logger *log, TR_GCStackMap *, TR::GCStackAtlas *atlas = NULL);

    virtual void printRegisterMask(OMR::Logger *log, TR_RegisterMask mask, TR_RegisterKinds rk);
    virtual void print(OMR::Logger *log, TR::Register *, TR_RegisterSizes size = TR_WordReg);
    virtual void printFullRegInfo(OMR::Logger *log, TR::Register *);
    virtual const char *getRegisterKindName(TR_RegisterKinds);
    virtual const char *toString(TR_RematerializationInfo *);

    virtual void print(OMR::Logger *log, TR::VPConstraint *);

#ifdef J9_PROJECT_SPECIFIC
    virtual void dump(OMR::Logger *log, TR_CHTable *);
#endif

    virtual void traceLnFromLogTracer(const char *);
    virtual bool performTransformationImpl(bool, const char *, ...);

    virtual void dumpGlobalRegisterTable(OMR::Logger *log);
    virtual void dumpSimulatedNode(OMR::Logger *log, TR::Node *node, char tagChar);

    virtual uint32_t getLabelNumber() { return _nextLabelNumber; }

    // Verification Passes
    //
    virtual void verifyTrees(TR::ResolvedMethodSymbol *s);
    virtual void verifyBlocks(TR::ResolvedMethodSymbol *s);
    virtual void verifyCFG(TR::ResolvedMethodSymbol *s);

    virtual TR::Node *verifyFinalNodeReferenceCounts(TR::ResolvedMethodSymbol *s);

    virtual void startTracingRegisterAssignment(OMR::Logger *log,
        TR_RegisterKinds kindsToAssign = TR_RegisterKinds(TR_GPR_Mask | TR_FPR_Mask));
    virtual void stopTracingRegisterAssignment(OMR::Logger *log);
    virtual void pauseTracingRegisterAssignment();
    virtual void resumeTracingRegisterAssignment();
    virtual void traceRegisterAssignment(OMR::Logger *log, const char *format, va_list args);
    virtual void traceRegisterAssignment(OMR::Logger *log, TR::Instruction *instr, bool insertedByRA = true,
        bool postRA = false);
    virtual void traceRegisterAssigned(OMR::Logger *log, TR_RegisterAssignmentFlags flags, TR::Register *virtReg,
        TR::Register *realReg);
    virtual void traceRegisterFreed(OMR::Logger *log, TR::Register *virtReg, TR::Register *realReg);
    virtual void traceRegisterInterference(OMR::Logger *log, TR::Register *virtReg, TR::Register *interferingVirtual,
        int32_t distance);
    virtual void traceRegisterWeight(OMR::Logger *log, TR::Register *realReg, uint32_t weight);

    virtual void printGPRegisterStatus(OMR::Logger *log, OMR::MachineConnector *machine);
    virtual void printFPRegisterStatus(OMR::Logger *log, OMR::MachineConnector *machine);

    virtual const char *getPerCodeCacheHelperName(TR_CCPreLoadedCode helper);

#if defined(TR_TARGET_X86)
    virtual const char *getOpCodeName(TR::InstOpCode *);
    virtual const char *getMnemonicName(TR::InstOpCode *);
    virtual void printReferencedRegisterInfo(OMR::Logger *log, TR::Instruction *);
    virtual void dumpInstructionWithVFPState(OMR::Logger *log, TR::Instruction *instr, const TR_VFPState *prevState);

    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void printRegMemInstruction(OMR::Logger *log, const char *, TR::RealRegister *reg, TR::RealRegister *base = 0,
        int32_t = 0);
    void printRegRegInstruction(OMR::Logger *log, const char *, TR::RealRegister *reg1, TR::RealRegister *reg2 = 0);
    void printRegImmInstruction(OMR::Logger *log, const char *, TR::RealRegister *reg, int32_t imm);
    void printMemRegInstruction(OMR::Logger *log, const char *, TR::RealRegister *base, int32_t offset,
        TR::RealRegister * = 0);
    void printMemImmInstruction(OMR::Logger *log, const char *, TR::RealRegister *base, int32_t offset, int32_t imm);
#endif
#if defined(TR_TARGET_POWER)
    virtual const char *getOpCodeName(TR::InstOpCode *);
    const char *getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void print(OMR::Logger *log, TR::PPCHelperCallSnippet *);
#endif
#if defined(TR_TARGET_ARM)
    virtual void printARMDelayedOffsetInstructions(OMR::Logger *log, TR::ARMMemInstruction *instr);
    virtual void printARMHelperBranch(OMR::Logger *log, TR::SymbolReference *symRef, uint8_t *cursor,
        const char *opcodeName = "bl");
    virtual const char *getOpCodeName(TR::InstOpCode *);
    const char *getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    const char *getName(uint32_t realRegisterIndex, TR_RegisterSizes = (TR_RegisterSizes)-1);
    void print(OMR::Logger *log, TR::ARMHelperCallSnippet *);
#endif
#if defined(TR_TARGET_S390)
    virtual void printRegisterDependencies(OMR::Logger *log, TR::RegisterDependencyGroup *rgd, int numberOfRegisters);
    const char *getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
#endif
#if defined(TR_TARGET_ARM64)
    virtual const char *getOpCodeName(TR::InstOpCode *);
    const char *getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
#endif
#if defined(TR_TARGET_RISCV)
    virtual const char *getOpCodeName(TR::InstOpCode *);
    const char *getName(TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
#endif

#if defined(AIXPPC)
    virtual void setupDebugger(void *);
#elif defined(LINUX) || defined(J9ZOS390) || defined(OMR_OS_WINDOWS)
    virtual void setupDebugger(void *, void *, bool);
#endif /* defined(AIXPPC) */

    virtual void setSingleAllocMetaData(bool usesSingleAllocMetaData);

    const char *internalNamePrefix() { return "_"; }

    TR_OpaqueClassBlock *containingClass(TR::SymbolReference *);
    const char *signature(TR::ResolvedMethodSymbol *s);
    virtual int32_t nodePrintAllFlags(OMR::Logger *log, TR::Node *node);

    // used by DebugExt and may be overridden
    virtual void printDestination(OMR::Logger *log, TR::TreeTop *);
    virtual void printNodeInfo(OMR::Logger *log, TR::Node *);
    virtual void print(OMR::Logger *log, TR::CFGNode *, uint32_t indentation);
    virtual void printNodesInEdgeListIterator(OMR::Logger *log, TR::CFGEdgeList &li, bool fromNode);
    virtual void print(OMR::Logger *log, TR::Block *block, uint32_t indentation);
    virtual void print(OMR::Logger *log, TR_RegionStructure *regionStructure, uint32_t indentation);
    virtual void printSubGraph(OMR::Logger *log, TR_RegionStructure *regionStructure, uint32_t indentation);
    virtual void print(OMR::Logger *log, TR_InductionVariable *inductionVariable, uint32_t indentation);

    virtual void *dxMallocAndRead(uintptr_t size, void *remotePtr, bool dontAddToMap = false) { return remotePtr; }

    virtual void *dxMallocAndReadString(void *remotePtr, bool dontAddToMap = false) { return remotePtr; }

    virtual void dxFree(void *localPtr) { return; }

    int32_t printSymRefOffset(OMR::Logger *log, TR::SymbolReference *symRef);
    int32_t printSymRefObjIndex(OMR::Logger *log, TR::SymbolReference *symRef);
    int32_t printSymRefKind(OMR::Logger *log, TR::SymbolReference *symRef);
    int32_t printSymRefName(OMR::Logger *log, TR::SymbolReference *symRef);
    int32_t printSymRefOtherInfo(OMR::Logger *log, TR::SymbolReference *symRef);

    void printTopLegend(OMR::Logger *log);
    void printBottomLegend(OMR::Logger *log);
    void printSymRefTable(OMR::Logger *log, bool printFullTable = false);
    void printBasicPreNodeInfoAndIndent(OMR::Logger *log, TR::Node *, uint32_t indentation);
    void printBasicPostNodeInfo(OMR::Logger *log, TR::Node *, uint32_t indentation);

    bool isAlignStackMaps()
    {
#if defined(TR_HOST_ARM)
        return true;
#else
        return false;
#endif
    }

    void printNodeFlags(OMR::Logger *log, TR::Node *);
#ifdef J9_PROJECT_SPECIFIC
    void printBCDNodeInfo(OMR::Logger *log, TR::Node *node);
#endif

    int32_t *printStackAtlas(OMR::Logger *log, uintptr_t startPC, uint8_t *mapBits, int32_t numberOfSlotsMapped,
        bool fourByteOffsets, int32_t *sizeOfStackAtlas, int32_t frameSize);
    uint16_t printStackAtlasDetails(OMR::Logger *log, uintptr_t startPC, uint8_t *mapBits, int numberOfSlotsMapped,
        bool fourByteOffsets, int32_t *sizeOfStackAtlas, int32_t frameSize, int32_t *offsetInfo);
    uint8_t *printMapInfo(OMR::Logger *log, uintptr_t startPC, uint8_t *mapBits, int32_t numberOfSlotsMapped,
        bool fourByteOffsets, int32_t *sizeOfStackAtlas, TR_ByteCodeInfo *byteCodeInfo,
        uint16_t indexOfFirstInternalPtr, int32_t offsetInfo[], bool nummaps = false);
    void printStackMapInfo(OMR::Logger *log, uint8_t *&mapBits, int32_t numberOfSlotsMapped, int32_t *sizeOfStackAtlas,
        int32_t *offsetInfo, bool nummaps = false);
    void printJ9JITExceptionTableDetails(OMR::Logger *log, J9JITExceptionTable *data,
        J9JITExceptionTable *dbgextRemotePtr = NULL);

    /**
     * @brief Print a label and label offset in the code buffer in a format
     *     suitable for showing Snippet labels
     *
     * @param[in] log : \c OMR::Logger
     * @param[in] label : the \c LabelSymbol to print
     * @param[in] cursor : the code address of the label
     */
    void printSnippetLabel(OMR::Logger *log, TR::LabelSymbol *label, uint8_t *cursor);

    /**
     * @brief Print a snippet label and the label offset in the code buffer
     *
     * @param[in] log : \c OMR::Logger
     * @param[in] snippet : the snippet whose label to print
     * @param[in] cursor : the code address of the label
     */
    void printSnippetLabel(OMR::Logger *log, TR::Snippet *snippet, uint8_t *cursor);

    /**
     * @brief Print a label, label offset in the code buffer, and one or more
     *     comments in a a format suitable for showing Snippet labels
     *
     * @param[in] log : \c OMR::Logger
     * @param[in] label : the \c LabelSymbol to print
     * @param[in] cursor : the code address of the label
     * @param[in] comment1 : a comment to print after the label
     * @param[in] comment2 : a second, optional comment to print after comment1
     */
    void printSnippetLabel(OMR::Logger *log, TR::LabelSymbol *label, uint8_t *cursor, const char *comment1,
        const char *comment2 = 0);

    uint8_t *printPrefix(OMR::Logger *log, TR::Instruction *, uint8_t *cursor, uint8_t size);

    void printLabelInstruction(OMR::Logger *log, const char *, TR::LabelSymbol *label);
    int32_t printRestartJump(OMR::Logger *log, TR::X86RestartSnippet *, uint8_t *);
    int32_t printRestartJump(OMR::Logger *log, TR::X86RestartSnippet *, uint8_t *, int32_t, const char *);

    void printSymbolName(OMR::Logger *log, TR::Symbol *, TR::SymbolReference *, TR::MemoryReference *mr = NULL);
    bool isBranchToTrampoline(TR::SymbolReference *, uint8_t *, int32_t &);

    virtual void printDebugCounters(TR::DebugCounterGroup *counterGroup, const char *name);

    // ------------------------------

    void printFirst(OMR::Logger *log, int32_t i);
    void printCPIndex(OMR::Logger *log, int32_t i);
    void printConstant(OMR::Logger *log, int32_t i);
    void printConstant(OMR::Logger *log, double d);
    void printFirstAndConstant(OMR::Logger *log, int32_t i, int32_t j);

    void printLoadConst(OMR::Logger *log, TR::Node *);

    TR::CompilationFilters *findOrCreateFilters(TR::CompilationFilters *);
    TR::CompilationFilters *findOrCreateFilters(bool loadLimit);

    void printFilterTree(TR_FilterBST *root);

    TR::ResolvedMethodSymbol *getOwningMethodSymbol(TR::SymbolReference *);
    TR_ResolvedMethod *getOwningMethod(TR::SymbolReference *);

    const char *getAutoName(TR::SymbolReference *);
    const char *getParmName(TR::SymbolReference *);
    const char *getStaticName(TR::SymbolReference *);
    const char *getStaticName_ForListing(TR::SymbolReference *);

    virtual const char *getMethodName(TR::SymbolReference *);

    const char *getShadowName(TR::SymbolReference *);
    const char *getMetaDataName(TR::SymbolReference *);

    void printPreds(OMR::Logger *log, TR::CFGNode *);
    void printBaseInfo(OMR::Logger *log, TR_Structure *structure, uint32_t indentation);
    void print(OMR::Logger *log, TR_BlockStructure *blockStructure, uint32_t indentation);
    void print(OMR::Logger *log, TR_StructureSubGraphNode *node, uint32_t indentation);

    void printBlockInfo(OMR::Logger *log, TR::Node *node);

    void printVCG(OMR::Logger *log, TR_Structure *structure);
    void printVCG(OMR::Logger *log, TR_RegionStructure *regionStructure);
    void printVCG(OMR::Logger *log, TR_StructureSubGraphNode *node, bool isEntry);
    void printVCGEdges(OMR::Logger *log, TR_StructureSubGraphNode *node);
    void printVCG(OMR::Logger *log, TR::Block *block, int32_t vorder = -1, int32_t horder = -1);

    void printByteCodeStack(OMR::Logger *log, int32_t parentStackIndex, uint16_t byteCodeIndex, size_t *indentLen);
    void print(OMR::Logger *log, TR::GCRegisterMap *);

    void verifyTreesPass1(TR::Node *node);
    void verifyTreesPass2(TR::Node *node, bool isTreeTop);
    void verifyBlocksPass1(TR::Node *node);
    void verifyBlocksPass2(TR::Node *node);
    void verifyGlobalIndices(TR::Node *node, TR::Node **nodesByGlobalIndex);

    TR::Node *verifyFinalNodeReferenceCounts(TR::Node *node);
    uint32_t getIntLength(uint32_t num) const; // Number of digits in an integer
    // Number of spaces that must be inserted after index when index's length < maxIndexLength so the information
    // following it will be aligned
    uint32_t getNumSpacesAfterIndex(uint32_t index, uint32_t maxIndexLength) const;

#if defined(TR_TARGET_X86)
    void printPrefix(OMR::Logger *log, TR::Instruction *instr);
    int32_t printPrefixAndMnemonicWithoutBarrier(OMR::Logger *log, TR::Instruction *instr, int32_t barrier);
    void printPrefixAndMemoryBarrier(OMR::Logger *log, TR::Instruction *instr, int32_t barrier, int32_t barrierOffset);
    void dumpDependencyGroup(OMR::Logger *log, TR::RegisterDependencyGroup *group, int32_t numConditions, char *prefix,
        bool omitNullDependencies);
    void dumpDependencies(OMR::Logger *log, TR::Instruction *);
    void printRegisterInfoHeader(OMR::Logger *log, TR::Instruction *);
    void printBoundaryAvoidanceInfo(OMR::Logger *log, TR::X86BoundaryAvoidanceInstruction *);

    void printX86OOLSequences(OMR::Logger *log);

    void printx(OMR::Logger *log, TR::Instruction *);
    void print(OMR::Logger *log, TR::X86NoOperandInstruction *);
    void print(OMR::Logger *log, TR::X86LabelInstruction *);
    void print(OMR::Logger *log, TR::X86PaddingInstruction *);
    void print(OMR::Logger *log, TR::X86AlignmentInstruction *);
    void print(OMR::Logger *log, TR::X86BoundaryAvoidanceInstruction *);
    void print(OMR::Logger *log, TR::X86PatchableCodeAlignmentInstruction *);
    void print(OMR::Logger *log, TR::X86FenceInstruction *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::X86VirtualGuardNOPInstruction *);
#endif
    void print(OMR::Logger *log, TR::X86ImmInstruction *);
    void print(OMR::Logger *log, TR::X86ImmSnippetInstruction *);
    void print(OMR::Logger *log, TR::X86ImmSymInstruction *);
    void print(OMR::Logger *log, TR::X86RegInstruction *);
    void print(OMR::Logger *log, TR::X86RegRegInstruction *);
    void print(OMR::Logger *log, TR::X86RegImmInstruction *);
    void print(OMR::Logger *log, TR::X86RegRegImmInstruction *);
    void print(OMR::Logger *log, TR::X86RegRegRegInstruction *);
    void print(OMR::Logger *log, TR::X86RegMaskRegInstruction *);
    void print(OMR::Logger *log, TR::X86RegMaskRegRegInstruction *);
    void print(OMR::Logger *log, TR::X86RegMaskRegRegImmInstruction *);
    void print(OMR::Logger *log, TR::X86MemInstruction *);
    void print(OMR::Logger *log, TR::X86MemImmInstruction *);
    void print(OMR::Logger *log, TR::X86MemRegInstruction *);
    void print(OMR::Logger *log, TR::X86MemMaskRegInstruction *);
    void print(OMR::Logger *log, TR::X86MemRegImmInstruction *);
    void print(OMR::Logger *log, TR::X86RegMemInstruction *);
    void print(OMR::Logger *log, TR::X86RegMaskMemInstruction *);
    void print(OMR::Logger *log, TR::X86RegMemImmInstruction *);
    void print(OMR::Logger *log, TR::X86RegRegMemInstruction *);
    void print(OMR::Logger *log, TR::AMD64Imm64Instruction *);
    void print(OMR::Logger *log, TR::AMD64Imm64SymInstruction *);
    void print(OMR::Logger *log, TR::AMD64RegImm64Instruction *);
    void print(OMR::Logger *log, TR::X86VFPSaveInstruction *);
    void print(OMR::Logger *log, TR::X86VFPRestoreInstruction *);
    void print(OMR::Logger *log, TR::X86VFPDedicateInstruction *);
    void print(OMR::Logger *log, TR::X86VFPReleaseInstruction *);
    void print(OMR::Logger *log, TR::X86VFPCallCleanupInstruction *);

    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegRegInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86MemInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86MemRegInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegMemInstruction *);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegMemInstruction *);

    void printFullRegisterDependencyInfo(OMR::Logger *log, TR::RegisterDependencyConditions *conditions);
    void printDependencyConditions(OMR::Logger *log, TR::RegisterDependencyGroup *, uint8_t, char *);

    void print(OMR::Logger *log, TR::MemoryReference *, TR_RegisterSizes);
    void printReferencedRegisterInfo(OMR::Logger *log, TR::MemoryReference *);

    int32_t printIntConstant(OMR::Logger *log, int64_t value, int8_t radix, TR_RegisterSizes size = TR_WordReg,
        bool padWithZeros = false);
    int32_t printDecimalConstant(OMR::Logger *log, int64_t value, int8_t width, bool padWithZeros);
    int32_t printHexConstant(OMR::Logger *log, int64_t value, int8_t width, bool padWithZeros);
    void printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr);
    void printFPRegisterComment(OMR::Logger *log, TR::Register *target, TR::Register *source);
    void printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *mr);
    TR_RegisterSizes getTargetSizeFromInstruction(TR::Instruction *instr);
    TR_RegisterSizes getSourceSizeFromInstruction(TR::Instruction *instr);
    TR_RegisterSizes getImmediateSizeFromInstruction(TR::Instruction *instr);

    void printFullRegInfo(OMR::Logger *log, TR::RealRegister *);
    void printX86GCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);

    const char *getName(TR::RealRegister *, TR_RegisterSizes = TR_WordReg);
    const char *getName(uint32_t realRegisterIndex, TR_RegisterSizes = (TR_RegisterSizes)-1);

    void printx(OMR::Logger *log, TR::Snippet *);

#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::X86CallSnippet *);
    void print(OMR::Logger *log, TR::X86PicDataSnippet *);
    void print(OMR::Logger *log, TR::X86CheckFailureSnippet *);
    void print(OMR::Logger *log, TR::X86CheckFailureSnippetWithResolve *);
    void print(OMR::Logger *log, TR::X86BoundCheckWithSpineCheckSnippet *);
    void print(OMR::Logger *log, TR::X86SpineCheckSnippet *);
    void print(OMR::Logger *log, TR::X86ForceRecompilationSnippet *);
    void print(OMR::Logger *log, TR::X86RecompilationSnippet *);
#endif

    void print(OMR::Logger *log, TR::X86DivideCheckSnippet *);
    void print(OMR::Logger *log, TR::X86FPConvertToIntSnippet *);
    void print(OMR::Logger *log, TR::X86FPConvertToLongSnippet *);
    void print(OMR::Logger *log, TR::X86GuardedDevirtualSnippet *);
    void print(OMR::Logger *log, TR::X86HelperCallSnippet *);
    void printBody(OMR::Logger *log, TR::X86HelperCallSnippet *, uint8_t *bufferPos);

    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);

#ifdef TR_TARGET_64BIT
    uint8_t *printArgumentFlush(OMR::Logger *log, TR::Node *, bool, uint8_t *);
#endif
#endif
#ifdef TR_TARGET_POWER
    void printPrefix(OMR::Logger *log, TR::Instruction *);

    void print(OMR::Logger *log, TR::PPCAlignmentNopInstruction *);
    void print(OMR::Logger *log, TR::PPCDepInstruction *);
    void print(OMR::Logger *log, TR::PPCLabelInstruction *);
    void print(OMR::Logger *log, TR::PPCDepLabelInstruction *);
    void print(OMR::Logger *log, TR::PPCConditionalBranchInstruction *);
    void print(OMR::Logger *log, TR::PPCDepConditionalBranchInstruction *);
    void print(OMR::Logger *log, TR::PPCAdminInstruction *);
    void print(OMR::Logger *log, TR::PPCImmInstruction *);
    void print(OMR::Logger *log, TR::PPCSrc1Instruction *);
    void print(OMR::Logger *log, TR::PPCDepImmSymInstruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Instruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src1Instruction *);
    void print(OMR::Logger *log, TR::PPCTrg1ImmInstruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src1ImmInstruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src1Imm2Instruction *);
    void print(OMR::Logger *log, TR::PPCSrc2Instruction *);
    void print(OMR::Logger *log, TR::PPCSrc3Instruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src2Instruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src2ImmInstruction *);
    void print(OMR::Logger *log, TR::PPCTrg1Src3Instruction *);
    void print(OMR::Logger *log, TR::PPCMemSrc1Instruction *);
    void print(OMR::Logger *log, TR::PPCMemInstruction *);
    void print(OMR::Logger *log, TR::PPCTrg1MemInstruction *);
    void print(OMR::Logger *log, TR::PPCControlFlowInstruction *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::PPCVirtualGuardNOPInstruction *);
#endif

    TR::Instruction *getOutlinedTargetIfAny(TR::Instruction *instr);
    void printPPCOOLSequences(OMR::Logger *log);

    const char *getPPCRegisterName(uint32_t regNum, bool useVSR = false);

    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void print(OMR::Logger *log, TR::RegisterDependency *);
    void print(OMR::Logger *log, TR::RegisterDependencyConditions *);
    void printPPCGCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);

    void print(OMR::Logger *log, TR::MemoryReference *, bool d_form = true);

    uint8_t *printEmitLoadPPCHelperAddrToCtr(OMR::Logger *log, uint8_t *, int32_t, TR::RealRegister *);
    uint8_t *printEmitLoadIndirectPPCHelperAddrToCtr(OMR::Logger *log, uint8_t *, TR::RealRegister *,
        TR::RealRegister *, int32_t);
    uint8_t *printPPCArgumentsFlush(OMR::Logger *log, TR::Node *, uint8_t *, int32_t);
    void printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr);

    void printp(OMR::Logger *log, TR::Snippet *);
    void print(OMR::Logger *log, TR::PPCMonitorEnterSnippet *);
    void print(OMR::Logger *log, TR::PPCMonitorExitSnippet *);
    void print(OMR::Logger *log, TR::PPCReadMonitorSnippet *);
    void print(OMR::Logger *log, TR::PPCAllocPrefetchSnippet *);

    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);

#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::PPCStackCheckFailureSnippet *);
    void print(OMR::Logger *log, TR::PPCInterfaceCastSnippet *);
    void print(OMR::Logger *log, TR::PPCUnresolvedCallSnippet *);
    void print(OMR::Logger *log, TR::PPCVirtualSnippet *);
    void print(OMR::Logger *log, TR::PPCVirtualUnresolvedSnippet *);
    void print(OMR::Logger *log, TR::PPCInterfaceCallSnippet *);
    void print(OMR::Logger *log, TR::PPCForceRecompilationSnippet *);
    void print(OMR::Logger *log, TR::PPCRecompilationSnippet *);
    void print(OMR::Logger *log, TR::PPCCallSnippet *);
#endif

    void printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *mr);
    void print(OMR::Logger *log, TR::PPCLockReservationEnterSnippet *);
    void print(OMR::Logger *log, TR::PPCLockReservationExitSnippet *);
    uint8_t *print(OMR::Logger *log, TR::PPCArrayCopyCallSnippet *snippet, uint8_t *cursor);

#endif
#ifdef TR_TARGET_ARM
    char *fullOpCodeName(TR::Instruction *instr);
    void printPrefix(OMR::Logger *log, TR::Instruction *);
    void printBinaryPrefix(char *prefixBuffer, TR::Instruction *);
    void dumpDependencyGroup(OMR::Logger *log, TR::RegisterDependencyGroup *group, int32_t numConditions, char *prefix,
        bool omitNullDependencies);
    void dumpDependencies(OMR::Logger *log, TR::Instruction *);
    void print(OMR::Logger *log, TR::ARMLabelInstruction *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::ARMVirtualGuardNOPInstruction *);
#endif
    void print(OMR::Logger *log, TR::ARMAdminInstruction *);
    void print(OMR::Logger *log, TR::ARMImmInstruction *);
    void print(OMR::Logger *log, TR::ARMImmSymInstruction *);
    void print(OMR::Logger *log, TR::ARMTrg1Src2Instruction *);
    void print(OMR::Logger *log, TR::ARMTrg2Src1Instruction *);
    void print(OMR::Logger *log, TR::ARMMulInstruction *);
    void print(OMR::Logger *log, TR::ARMMemSrc1Instruction *);
    void print(OMR::Logger *log, TR::ARMTrg1Instruction *);
    void print(OMR::Logger *log, TR::ARMMemInstruction *);
    void print(OMR::Logger *log, TR::ARMTrg1MemInstruction *);
    void print(OMR::Logger *log, TR::ARMTrg1MemSrc1Instruction *);
    void print(OMR::Logger *log, TR::ARMControlFlowInstruction *);
    void print(OMR::Logger *log, TR::ARMMultipleMoveInstruction *);

    void print(OMR::Logger *log, TR::MemoryReference *);

    void print(OMR::Logger *log, TR_ARMOperand2 *op, TR_RegisterSizes size = TR_WordReg);

    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void printARMGCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);
    void printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr);

    void printa(OMR::Logger *log, TR::Snippet *);
    void print(OMR::Logger *log, TR::ARMCallSnippet *);
    void print(OMR::Logger *log, TR::ARMUnresolvedCallSnippet *);
    void print(OMR::Logger *log, TR::ARMVirtualSnippet *);
    void print(OMR::Logger *log, TR::ARMVirtualUnresolvedSnippet *);
    void print(OMR::Logger *log, TR::ARMInterfaceCallSnippet *);
    void print(OMR::Logger *log, TR::ARMMonitorEnterSnippet *);
    void print(OMR::Logger *log, TR::ARMMonitorExitSnippet *);
    void print(OMR::Logger *log, TR::ARMStackCheckFailureSnippet *);
    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);
    void print(OMR::Logger *log, TR::ARMRecompilationSnippet *);
#endif
#ifdef TR_TARGET_S390
    void printPrefix(OMR::Logger *log, TR::Instruction *);

    void printS390RegisterDependency(OMR::Logger *log, TR::Register *virtReg, int realReg, bool uses, bool defs);

    TR::Instruction *getOutlinedTargetIfAny(TR::Instruction *instr);
    void printS390OOLSequences(OMR::Logger *log);

    void dumpDependencies(OMR::Logger *log, TR::Instruction *, bool, bool);
    void printAssocRegDirective(OMR::Logger *log, TR::Instruction *instr);
    void printz(OMR::Logger *log, TR::Instruction *);
    void printz(OMR::Logger *log, TR::Instruction *, const char *);
    void print(OMR::Logger *log, TR::S390LabelInstruction *);
    void print(OMR::Logger *log, TR::S390BranchInstruction *);
    void print(OMR::Logger *log, TR::S390BranchOnCountInstruction *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::S390VirtualGuardNOPInstruction *);
#endif
    void print(OMR::Logger *log, TR::S390BranchOnIndexInstruction *);
    void print(OMR::Logger *log, TR::S390ImmInstruction *);
    void print(OMR::Logger *log, TR::S390ImmSnippetInstruction *);
    void print(OMR::Logger *log, TR::S390ImmSymInstruction *);
    void print(OMR::Logger *log, TR::S390Imm2Instruction *);
    void print(OMR::Logger *log, TR::S390RegInstruction *);
    void print(OMR::Logger *log, TR::S390TranslateInstruction *);
    void print(OMR::Logger *log, TR::S390RRInstruction *);
    void print(OMR::Logger *log, TR::S390RRFInstruction *);
    void print(OMR::Logger *log, TR::S390RRRInstruction *);
    void print(OMR::Logger *log, TR::S390RIInstruction *);
    void print(OMR::Logger *log, TR::S390RILInstruction *);
    void print(OMR::Logger *log, TR::S390RSInstruction *);
    void print(OMR::Logger *log, TR::S390RSLInstruction *);
    void print(OMR::Logger *log, TR::S390RSLbInstruction *);
    void print(OMR::Logger *log, TR::S390MemInstruction *);
    void print(OMR::Logger *log, TR::S390SS1Instruction *);
    void print(OMR::Logger *log, TR::S390SS2Instruction *);
    void print(OMR::Logger *log, TR::S390SMIInstruction *);
    void print(OMR::Logger *log, TR::S390MIIInstruction *);
    void print(OMR::Logger *log, TR::S390SS4Instruction *);
    void print(OMR::Logger *log, TR::S390SSFInstruction *);
    void print(OMR::Logger *log, TR::S390SSEInstruction *);
    void print(OMR::Logger *log, TR::S390SIInstruction *);
    void print(OMR::Logger *log, TR::S390SILInstruction *);
    void print(OMR::Logger *log, TR::S390SInstruction *);
    void print(OMR::Logger *log, TR::S390RXInstruction *);
    void print(OMR::Logger *log, TR::S390RXEInstruction *);
    void print(OMR::Logger *log, TR::S390RXFInstruction *);
    void print(OMR::Logger *log, TR::S390AnnotationInstruction *);
    void print(OMR::Logger *log, TR::S390PseudoInstruction *);
    void print(OMR::Logger *log, TR::S390NOPInstruction *);
    void print(OMR::Logger *log, TR::S390AlignmentNopInstruction *);
    void print(OMR::Logger *log, TR::MemoryReference *, TR::Instruction *);
    void print(OMR::Logger *log, TR::S390RRSInstruction *);
    void print(OMR::Logger *log, TR::S390RIEInstruction *);
    void print(OMR::Logger *log, TR::S390RISInstruction *);
    void print(OMR::Logger *log, TR::S390OpCodeOnlyInstruction *);
    void print(OMR::Logger *log, TR::S390IInstruction *);
    void print(OMR::Logger *log, TR::S390IEInstruction *);
    void print(OMR::Logger *log, TR::S390VRIInstruction *);
    void print(OMR::Logger *log, TR::S390VRRInstruction *);
    void print(OMR::Logger *log, TR::S390VStorageInstruction *);

    const char *getS390RegisterName(uint32_t regNum, bool isVRF = false);
    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void printFullRegInfo(OMR::Logger *log, TR::RealRegister *);
    void printS390GCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);

    uint8_t *printS390ArgumentsFlush(OMR::Logger *log, TR::Node *, uint8_t *, int32_t);

    void printz(OMR::Logger *log, TR::Snippet *);

    void print(OMR::Logger *log, TR::S390ConstantDataSnippet *);

    void print(OMR::Logger *log, TR::S390HelperCallSnippet *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::S390ForceRecompilationSnippet *);
    void print(OMR::Logger *log, TR::S390ForceRecompilationDataSnippet *);
    void print(OMR::Logger *log, TR::S390UnresolvedCallSnippet *);
    void print(OMR::Logger *log, TR::S390VirtualSnippet *);
    void print(OMR::Logger *log, TR::S390VirtualUnresolvedSnippet *);
    void print(OMR::Logger *log, TR::S390InterfaceCallSnippet *);
    void print(OMR::Logger *log, TR::J9S390InterfaceCallDataSnippet *);
#endif
    void print(OMR::Logger *log, TR::S390StackCheckFailureSnippet *);
    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);
    void print(OMR::Logger *log, TR::S390HeapAllocSnippet *);
    void print(OMR::Logger *log, TR::S390JNICallDataSnippet *);
    void print(OMR::Logger *log, TR::S390RestoreGPR7Snippet *);

    // Assembly related snippet display
    void printGPRegisterStatus(OMR::Logger *log, TR::Machine *machine);
    void printFPRegisterStatus(OMR::Logger *log, TR::Machine *machine);
    uint32_t getBitRegNum(TR::RealRegister *reg);

    void printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr,
        bool needsStartComment = false);
    uint8_t *printLoadVMThreadInstruction(OMR::Logger *log, uint8_t *cursor);
    uint8_t *printRuntimeInstrumentationOnOffInstruction(OMR::Logger *log, uint8_t *cursor, bool isRION,
        bool isPrivateLinkage = false);
    const char *updateBranchName(const char *opCodeName, const char *brCondName);
#endif
#ifdef TR_TARGET_ARM64
    void printPrefix(OMR::Logger *log, TR::Instruction *);

    void print(OMR::Logger *log, TR::ARM64ImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64RelocatableImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64ImmSymInstruction *);
    void print(OMR::Logger *log, TR::ARM64LabelInstruction *);
    void print(OMR::Logger *log, TR::ARM64BranchInstruction *);
    void print(OMR::Logger *log, TR::ARM64ConditionalBranchInstruction *);
    void print(OMR::Logger *log, TR::ARM64CompareBranchInstruction *);
    void print(OMR::Logger *log, TR::ARM64TestBitBranchInstruction *);
    void print(OMR::Logger *log, TR::ARM64RegBranchInstruction *);
    void print(OMR::Logger *log, TR::ARM64AdminInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1CondInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1ImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1ImmShiftedInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1ImmSymInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src1Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1ZeroSrc1Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1ZeroImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src1ImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2Instruction *);
    void print(OMR::Logger *log, TR::ARM64CondTrg1Src2Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2ImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2ShiftedInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2ExtendedInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2IndexedElementInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src2ZeroInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1Src3Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1MemInstruction *);
    void print(OMR::Logger *log, TR::ARM64Trg2MemInstruction *);
    void print(OMR::Logger *log, TR::ARM64MemInstruction *);
    void print(OMR::Logger *log, TR::ARM64MemImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64MemSrc1Instruction *);
    void print(OMR::Logger *log, TR::ARM64MemSrc2Instruction *);
    void print(OMR::Logger *log, TR::ARM64Trg1MemSrc1Instruction *);
    void print(OMR::Logger *log, TR::ARM64Src1Instruction *);
    void print(OMR::Logger *log, TR::ARM64ZeroSrc1ImmInstruction *);
    void print(OMR::Logger *log, TR::ARM64Src2Instruction *);
    void print(OMR::Logger *log, TR::ARM64ZeroSrc2Instruction *);
    void print(OMR::Logger *log, TR::ARM64Src1ImmCondInstruction *);
    void print(OMR::Logger *log, TR::ARM64Src2CondInstruction *);
    void print(OMR::Logger *log, TR::ARM64SynchronizationInstruction *);
#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::ARM64VirtualGuardNOPInstruction *);
#endif
    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void print(OMR::Logger *log, TR::RegisterDependency *);
    void print(OMR::Logger *log, TR::RegisterDependencyConditions *);
    void print(OMR::Logger *log, TR::MemoryReference *);
    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);

    void printARM64OOLSequences(OMR::Logger *log);
    void printARM64GCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);
    void printInstructionComment(OMR::Logger *log, int32_t, TR::Instruction *);
    void printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *);
    void printAssocRegDirective(OMR::Logger *log, TR::Instruction *);

    const char *getARM64RegisterName(uint32_t, bool = true);

    /**
     * @brief Returns a const char * string for the name of the given
     *     register refined by the provided register size
     *
     * @param[in] regNum : register number
     * @param[in[ size : the size of register to output
     *
     * @return const char * string for the register name
     */
    const char *getName(uint32_t regNum, TR_RegisterSizes size);

    void printa64(OMR::Logger *log, TR::Snippet *);

#ifdef J9_PROJECT_SPECIFIC
    void print(OMR::Logger *log, TR::ARM64CallSnippet *);
    void print(OMR::Logger *log, TR::ARM64UnresolvedCallSnippet *);
    void print(OMR::Logger *log, TR::ARM64VirtualUnresolvedSnippet *);
    void print(OMR::Logger *log, TR::ARM64InterfaceCallSnippet *);
    void print(OMR::Logger *log, TR::ARM64StackCheckFailureSnippet *);
    void print(OMR::Logger *log, TR::ARM64ForceRecompilationSnippet *);
    void print(OMR::Logger *log, TR::ARM64RecompilationSnippet *);
    uint8_t *printARM64ArgumentsFlush(OMR::Logger *log, TR::Node *, uint8_t *, int32_t);
#endif

#endif
#ifdef TR_TARGET_RISCV
    void printPrefix(OMR::Logger *log, TR::Instruction *);

    void print(OMR::Logger *log, TR::LabelInstruction *);
    void print(OMR::Logger *log, TR::AdminInstruction *);
    void print(OMR::Logger *log, TR::DataInstruction *);

    void print(OMR::Logger *log, TR::RtypeInstruction *);
    void print(OMR::Logger *log, TR::ItypeInstruction *);
    void print(OMR::Logger *log, TR::StypeInstruction *);
    void print(OMR::Logger *log, TR::BtypeInstruction *);
    void print(OMR::Logger *log, TR::UtypeInstruction *);
    void print(OMR::Logger *log, TR::JtypeInstruction *);
    void print(OMR::Logger *log, TR::LoadInstruction *);
    void print(OMR::Logger *log, TR::StoreInstruction *);
    void print(OMR::Logger *log, TR::RVHelperCallSnippet *);

    void print(OMR::Logger *log, TR::RealRegister *, TR_RegisterSizes size = TR_WordReg);
    void print(OMR::Logger *log, TR::RegisterDependency *);
    void print(OMR::Logger *log, TR::RegisterDependencyConditions *);
    void print(OMR::Logger *log, TR::MemoryReference *);
    void print(OMR::Logger *log, TR::UnresolvedDataSnippet *);

    void printRVOOLSequences(OMR::Logger *log);
    void printRVGCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *);
    void printInstructionComment(OMR::Logger *log, int32_t, TR::Instruction *);
    void printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *);

    const char *getRVRegisterName(uint32_t, bool = true);
#endif

    friend class TR_CFGChecker;

protected:
    TR::Compilation *_comp;
    TR_FrontEnd *_fe;
    OMR::Logger *_logger;

    uint32_t _nextLabelNumber;
    uint32_t _nextRegisterNumber;
    uint32_t _nextNodeNumber;
    uint32_t _nextSymbolNumber;
    uint32_t _nextInstructionNumber;
    uint32_t _nextStructureNumber;
    uint32_t _nextVariableSizeSymbolNumber;
    TR::CompilationFilters *_compilationFilters;
    TR::CompilationFilters *_relocationFilters;
    TR::CompilationFilters *_inlineFilters;
    bool _usesSingleAllocMetaData;
    TR_BitVector _nodeChecklist;
    TR_BitVector _structureChecklist;

    TR::CodeGenerator *_cg;

    int32_t _lastFrequency;
    bool _isCold;
    bool _mainEntrySeen;
    TR::Node *_currentParent;
    int32_t _currentChildIndex;

#define TRACERA_IN_PROGRESS (0x0001)
#define TRACERA_INSTRUCTION_INSERTED (0x0002)
    uint16_t _registerAssignmentTraceFlags;
    uint16_t _pausedRegisterAssignmentTraceFlags;
    int16_t _registerAssignmentTraceCursor;
    TR_RegisterKinds _registerKindsToAssign;

    const static uint32_t DEFAULT_NODE_LENGTH
        = 82; // Tree information printed to the right of node OPCode will be aligned if node OPCode's length <=
              // DEFAULT_NODE_LENGTH, or will follow the node OPCode immediately (non-aligned) otherwise.
    const static uint32_t MAX_GLOBAL_INDEX_LENGTH = 5;
};

typedef TR_Debug *(*TR_CreateDebug_t)(TR::Compilation *);
#endif
