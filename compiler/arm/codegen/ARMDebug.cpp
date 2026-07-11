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

#if !defined(TR_TARGET_ARM)
int jitDebugARM;
#else

#include "arm/codegen/ARMInstruction.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "arm/codegen/ARMRecompilationSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Recompilation.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "arm/codegen/ARMHelperCallSnippet.hpp"
#endif

// change the following lines to not get the instruction addr during
// prints (useful for diff-ing output)
#define INSTR_OR_NULL instr

static const char *ARMConditionNames[]
    = { "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "al", "nv" };

#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
static const char *opCodeToVFPMap[] = {
    ".f64", // fabsd (vabs<c>.f64)
    ".f32", // fabss (vabs<c>.f32)
    ".f64", // faddd (vadd<c>.f64)
    ".f32", // fadds (vadd<c>.f32)
    ".f64", // fcmpd (vcmp<c>.f64)
    ".f32", // fcmps (vcmp<c>.f32)
    ".f64", // fcmpzd (vcmp<c>.f64)
    ".f32", // fcmpzs (vcmp<c>.f32)
    ".f64", // fcpyd (vmov<c>.f64)
    ".f32", // fcpys (vmov<c>.f32)
    ".f64.f32", // fcvtds (vcvt<c>.f64.f32)
    ".f32.f64", // fcvtsd (vcvt<c>.f32.f64)
    ".f64", // fdivd (vdiv<c>.f64)
    ".f32", // fdivs (vdiv<c>.f32)
    "", // fldd  (vldr)
    "", // fldmd (vldm)
    "", // fldms (vldm)
    "", // flds (vldr)
    ".f64", // fmacd (vmla.f64)
    ".f32", // fmacs (vmla.f32)
    "", // fmdrr (vmov)
    "", // fmrrd (vmov)
    "", // fmrrs (vmov)
    "", // fmrs (vmov)
    "", // fmrx (vmrs)
    ".f64", // fmscd (vmls.f64)
    ".f32", // fmscs (vmls.f32)
    "", // fmsr (vmov)
    "", // fmsrr (vmov)
    "", // fmstat (vmrs APSR_nzcv, FPSCR)
    ".f64", // fmuld (vmul.f64)
    ".f32", // fmuls (vmul.f32)
    "", // fmxr (vmsr)
    ".f64", // fnegd (vneg.f64)
    ".f32", // fnegs (vneg.f32)
    ".f64", // fnmacd (vnmla.f64)
    ".f32", // fnmacs (vnmla.f32)
    ".f64", // fnmscd (vnmls.f64)
    ".f32", // fnmscs (vnmls.f32)
    ".f64", // fnmuld (vnmul.f64)
    ".f32", // fnmuls (vnmul.f32)
    ".f64.s32", // fsitod (vcvt.f64.s32)
    ".f32.s32", // fsitos (vcvt.f32.s32)
    ".f64", // fsqrtd (vsqrt.f64)
    ".f32", // fsqrts (vsqrt.f32)
    "", // fstd (vstr)
    "", // fstmd (vstm)
    "", // fstms (vstm)
    "", // fsts (vstr)
    ".f64", // fsubd (vsub.f64)
    ".f32", // fsubs (vsub.f32)
    ".s32.f64", // ftosizd (vcvt.s32.f64)
    ".s32.f32", // ftosizs (vcvt.s32.f32)
    ".u32.f64", // ftouizd (vcvt.u32.f64)
    ".u32.f32", // ftouizs (vcvt.u32.f32)
    ".f64,u32", // fuitod (vcvt.f64,u32)
    ".f32.u32" // fuitos (vcvt.f32.u32)
};
#endif

static const char *getExtraVFPInstrSpecifiers(TR::InstOpCode *opCode)
{
#define FIRST_VFP_INSTR TR::InstOpCode::fabsd

#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
    uint32_t index = (uint32_t)opCode->getOpCodeValue() - (uint32_t)(FIRST_VFP_INSTR);
    return opCodeToVFPMap[index];
#else
    return "";
#endif
#undef FIRST_VFP_INSTR
}

char *TR_Debug::fullOpCodeName(TR::Instruction *instr)
{
    const size_t nameBufSize = 64;
    static char nameBuf[nameBufSize];
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
    snprintf(nameBuf, nameBufSize, "%s%s%s", getOpCodeName(&instr->getOpCode()),
        (instr->getConditionCode() != ARMConditionCodeAL) ? ARMConditionNames[instr->getConditionCode()] : "",
        instr->getOpCode().isVFPOp() ? getExtraVFPInstrSpecifiers(&instr->getOpCode()) : "");
#else
    snprintf(nameBuf, nameBufSize, "%s%s", getOpCodeName(&instr->getOpCode()),
        (instr->getConditionCode() != ARMConditionCodeAL) ? ARMConditionNames[instr->getConditionCode()] : "");
#endif
    return nameBuf;
}

void TR_Debug::printPrefix(OMR::Logger *log, TR::Instruction *instr)
{
    printPrefix(log, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
}

void TR_Debug::printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr)
{
    while (tabStops-- > 0)
        log->printc('\t');

    dumpInstructionComments(log, instr);
}

void TR_Debug::print(OMR::Logger *log, TR::Instruction *instr)
{
    switch (instr->getKind()) {
        case OMR::Instruction::IsImm:
            print(log, (TR::ARMImmInstruction *)instr);
            break;
        case OMR::Instruction::IsImmSym:
            print(log, (TR::ARMImmSymInstruction *)instr);
            break;
        case OMR::Instruction::IsLabel:
            print(log, (TR::ARMLabelInstruction *)instr);
            break;
        case OMR::Instruction::IsConditionalBranch:
            print(log, (TR::ARMConditionalBranchInstruction *)instr);
            break;
#ifdef J9_PROJECT_SPECIFIC
        case OMR::Instruction::IsVirtualGuardNOP:
            print(log, (TR::ARMVirtualGuardNOPInstruction *)instr);
            break;
#endif
        case OMR::Instruction::IsAdmin:
            print(log, (TR::ARMAdminInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Src2:
        case OMR::Instruction::IsSrc2:
        case OMR::Instruction::IsTrg1Src1:
            print(log, (TR::ARMTrg1Src2Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1:
            print(log, (TR::ARMTrg1Instruction *)instr);
            break;
        case OMR::Instruction::IsMem:
            print(log, (TR::ARMMemInstruction *)instr);
            break;
        case OMR::Instruction::IsMemSrc1:
            print(log, (TR::ARMMemSrc1Instruction *)instr);
            break;
        case OMR::Instruction::IsTrg1Mem:
            print(log, (TR::ARMTrg1MemInstruction *)instr);
            break;
        case OMR::Instruction::IsTrg1MemSrc1:
            print(log, (TR::ARMTrg1MemSrc1Instruction *)instr);
            break;
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
        case OMR::Instruction::IsTrg2Src1:
            print(log, (TR::ARMTrg2Src1Instruction *)instr);
            break;
#endif
        case OMR::Instruction::IsMul:
            print(log, (TR::ARMMulInstruction *)instr);
            break;
        case OMR::Instruction::IsControlFlow:
            print(log, (TR::ARMControlFlowInstruction *)instr);
            break;
        case OMR::Instruction::IsMultipleMove:
            print(log, (TR::ARMMultipleMoveInstruction *)instr);
            break;
        default:
            TR_ASSERT(0, "unexpected instruction kind");
            // fall thru
        case OMR::Instruction::IsNotExtended: {
            printPrefix(log, instr);
            log->printf("%-32s", fullOpCodeName(instr));
            log->flush();
        }
    }
}

void TR_Debug::dumpDependencyGroup(OMR::Logger *log, TR::RegisterDependencyGroup *group, int32_t numConditions,
    char *prefix, bool omitNullDependencies)
{
    int32_t i;
    bool foundDep = false;

    log->printf("\n\t%s: ", prefix);
    for (i = 0; i < numConditions; ++i) {
        TR::RegisterDependency *regDep = group->getRegisterDependency(i);
        TR::Register *virtReg = regDep->getRegister();

        if (omitNullDependencies && !virtReg)
            continue;

        TR::RealRegister::RegNum r = group->getRegisterDependency(i)->getRealRegister();

        log->printf(" [%s : ", getName(virtReg));
        if (regDep->isNoReg())
            log->prints("NoReg]");
        else
            log->printf("%s]", getName(_cg->machine()->getRealRegister(r)));

        foundDep = true;
    }

    if (!foundDep)
        log->prints(" None");
}

void TR_Debug::dumpDependencies(OMR::Logger *log, TR::Instruction *instr)
{
    // If we are in instruction selection and dependency
    // information is requested, dump it.
    if (_cg->getStackAtlas())
        return;

    TR::RegisterDependencyConditions *deps = instr->getDependencyConditions();
    if (!deps)
        return;

    if (deps->getNumPreConditions() > 0)
        dumpDependencyGroup(log, deps->getPreConditions(), deps->getNumPreConditions(), " PRE", true);

    if (deps->getNumPostConditions() > 0)
        dumpDependencyGroup(log, deps->getPostConditions(), deps->getNumPostConditions(), "POST", true);

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMLabelInstruction *instr)
{
    printPrefix(log, instr);

    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    if (instr->getOpCodeValue() == TR::InstOpCode::label) {
        print(log, label);
        log->printc(':');
        if (label->isStartInternalControlFlow())
            log->prints(" (Start of internal control flow)");
        else if (label->isEndInternalControlFlow())
            log->prints(" (End of internal control flow)");
    } else if (instr->getOpCodeValue() == TR::InstOpCode::b || instr->getOpCodeValue() == TR::InstOpCode::bl) {
        log->printf("%s\t", fullOpCodeName(instr));
        print(log, label);
        if (snippet) {
            log->prints(" (");
            snippet->printName(log);
            log->printc(')');
        }
    } else {
        log->printf("%s\t", fullOpCodeName(instr));
        print(log, instr->getTarget1Register(), TR_WordReg);
        log->prints(", ");
        print(log, instr->getSource1Register(), TR_WordReg);
        log->prints(", ");
        uint8_t *instrPos = instr->getBinaryEncoding();
        log->prints("#(");
        print(log, label);
        log->prints(" - gr15)");
    }
    printInstructionComment(log, 1, instr);
    dumpDependencies(log, instr);
    log->flush();
}

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::print(OMR::Logger *log, TR::ARMVirtualGuardNOPInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s Site:" POINTER_PRINTF_FORMAT ", ", getOpCodeName(&instr->getOpCode()), instr->getSite());
    print(log, instr->getLabelSymbol());
    printInstructionComment(log, 1, instr);
    dumpDependencies(log, instr);
    log->flush();
}
#endif

void TR_Debug::print(OMR::Logger *log, TR::ARMAdminInstruction *instr)
{
    // Omit admin instructions from post-binary dumps unless they mark basic block boundaries.
    if (instr->getBinaryEncoding() && instr->getNode()->getOpCodeValue() != TR::BBStart
        && instr->getNode()->getOpCodeValue() != TR::BBEnd)
        return;

    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    if (instr->getOpCodeValue() == TR::InstOpCode::fence && instr->getFenceNode()) {
        TR::Node *fenceNode = instr->getFenceNode();
        if (fenceNode->getRelocationType() == TR_AbsoluteAddress)
            log->prints(" Absolute [");
        else if (fenceNode->getRelocationType() == TR_ExternalAbsoluteAddress)
            log->prints(" External Absolute [");
        else
            log->prints(" Relative [");
        for (uint32_t i = 0; i < fenceNode->getNumRelocations(); i++) {
            log->printf(" " POINTER_PRINTF_FORMAT, fenceNode->getRelocationDestination(i));
        }
        log->prints(" ]");
    } else {
        log->prints("\t\t");
    }

    TR::Node *node = instr->getNode();
    if (node && node->getOpCodeValue() == TR::BBStart)
        log->printf(" (BBStart (block_%d))", node->getBlock()->getNumber());
    else if (node && node->getOpCodeValue() == TR::BBEnd)
        log->printf(" (BBEnd (block_%d))", node->getBlock()->getNumber());

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t0x%08x", fullOpCodeName(instr), instr->getSourceImmediate());
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMImmSymInstruction *instr)
{
    uint8_t *bufferPos = instr->getBinaryEncoding();
    int32_t imm = instr->getSourceImmediate();

    TR::SymbolReference *symRef = instr->getSymbolReference();
    TR::Symbol *sym = symRef->getSymbol();
    TR::ResolvedMethodSymbol *calleeSym = sym->getResolvedMethodSymbol();
    TR_ResolvedMethod *callee = calleeSym ? calleeSym->getResolvedMethod() : NULL;

    bool longJump = imm && _cg->directCallRequiresTrampoline((intptr_t)imm, (intptr_t)bufferPos)
        && !_comp->isRecursiveMethodTarget(sym);

    if (bufferPos != NULL && longJump) {
        if (*(int32_t *)bufferPos == 0xe1a0e00f) {
            const char *name = getName(symRef);
            printPrefix(log, NULL, bufferPos, 4);
            log->prints("mov\tgr14, gr15");
            printPrefix(log, NULL, bufferPos + 4, 4);
            log->prints("ldr\tgr15, [gr15, #-4]");
            printPrefix(log, NULL, bufferPos + 8, 4);
            if (name)
                log->printf("DCD\t0x%x\t; %s (" POINTER_PRINTF_FORMAT ")", imm, name);
            else
                log->printf("DCD\t0x%x", imm);
            return;
        } else {
            printARMHelperBranch(log, instr->getSymbolReference(), bufferPos, fullOpCodeName(instr));
        }
    } else {
        const char *name = getName(symRef);
        printPrefix(log, instr);
        if (name) {
            log->printf("%s\t%-24s; ", fullOpCodeName(instr), name);

            TR::Snippet *snippet = sym->getLabelSymbol() ? sym->getLabelSymbol()->getSnippet() : NULL;
            if (snippet) {
                log->printc('(');
                snippet->printName(log);
                log->printc(')');
            } else
                log->printf("(" POINTER_PRINTF_FORMAT ")", imm);
        } else {
            log->printf("%s\t" POINTER_PRINTF_FORMAT, fullOpCodeName(instr), imm);
        }
    }
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMTrg1Src2Instruction *instr)
{
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    TR_RegisterSizes targetSize = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
    TR_RegisterSizes source1Size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
    TR_RegisterSizes source2Size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
#else
    TR_RegisterSizes targetSize = TR_WordReg;
    TR_RegisterSizes source1Size = TR_WordReg;
    TR_RegisterSizes source2Size = TR_WordReg;
#endif

    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    if (instr->getOpCodeValue() == TR::InstOpCode::fmrs) {
        if (instr->getTarget1Register()) {
            print(log, instr->getTarget1Register(), targetSize);
            log->prints(", ");
        }
        if (instr->getSource1Register()) {
            print(log, instr->getSource1Register(), source1Size);
        }
    } else if (instr->getOpCodeValue() == TR::InstOpCode::fmsr) {
        if (instr->getSource1Register()) {
            print(log, instr->getSource1Register(), source1Size);
            log->prints(", ");
        }
        if (instr->getTarget1Register()) {
            print(log, instr->getTarget1Register(), targetSize);
        }
    } else if (instr->getOpCodeValue() == TR::InstOpCode::fmdrr) {
        print(log, instr->getSource2Operand(), TR_DoubleReg);
        log->prints(", ");
        print(log, instr->getTarget1Register(), TR_WordReg);
        log->prints(", ");
        print(log, instr->getSource1Register(), TR_WordReg);
    } else if (instr->getOpCodeValue() == TR::InstOpCode::fcvtds || instr->getOpCodeValue() == TR::InstOpCode::fsitod
        || instr->getOpCodeValue() == TR::InstOpCode::fuitod) {
        print(log, instr->getTarget1Register(), TR_DoubleReg);
        log->prints(", ");
        print(log, instr->getSource2Operand(), TR_WordReg);
    } else if (instr->getOpCodeValue() == TR::InstOpCode::fcvtsd || instr->getOpCodeValue() == TR::InstOpCode::ftosizd
        || instr->getOpCodeValue() == TR::InstOpCode::ftouizd) {
        print(log, instr->getTarget1Register(), TR_WordReg);
        log->prints(", ");
        print(log, instr->getSource2Operand(), TR_DoubleReg);
    } else if (instr->getOpCodeValue() == TR::InstOpCode::fcmpzd || instr->getOpCodeValue() == TR::InstOpCode::fcmpzs) {
        print(log, instr->getTarget1Register(),
            (instr->getOpCodeValue() == TR::InstOpCode::fcmpzd) ? TR_DoubleReg : TR_WordReg);
#if defined(__ARM_ARCH_7A__) && defined(__VFP_FP__) && !defined(__SOFTFP__)
        log->prints(", #0.0");
#endif
    } else
#endif
    {
        if (instr->getTarget1Register()) {
            // TR_RegisterSizes targetSize = TR_WordReg;
            print(log, instr->getTarget1Register(), targetSize);
            log->prints(", ");
        }
        // TR_RegisterSizes source1Size = TR_WordReg;
        if (instr->getSource1Register() && instr->getOpCodeValue() != TR::InstOpCode::swp) {
            print(log, instr->getSource1Register(), source1Size);
            log->prints(", ");
        }
        // TR_RegisterSizes source2Size = TR_WordReg;
        print(log, instr->getSource2Operand(), source2Size);
        if (instr->getSource1Register() && instr->getOpCodeValue() == TR::InstOpCode::swp) {
            log->prints(", [");
            print(log, instr->getSource1Register(), source1Size);
            log->printc(']');
        }
    }
    dumpDependencies(log, instr);
    log->flush();
}

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
void TR_Debug::print(OMR::Logger *log, TR::ARMTrg2Src1Instruction *instr)
{
    // fmrrd
    TR_RegisterSizes size = TR_WordReg;
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    if (instr->getTarget1Register()) {
        print(log, instr->getTarget1Register(), size);
        log->prints(", ");
    }
    if (instr->getTarget2Register()) {
        print(log, instr->getTarget2Register(), size);
        log->prints(", ");
    }
    if (instr->getSource1Register()) {
        print(log, instr->getSource1Register(), TR_DoubleReg);
    }
    dumpDependencies(log, instr);
    log->flush();
}
#endif

void TR_Debug::print(OMR::Logger *log, TR::ARMMulInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));

    print(log, instr->getTargetLoRegister());
    if (instr->getTargetHiRegister()) // if a mull instruction
    {
        log->prints(", ");
        print(log, instr->getTargetHiRegister());
    }
    log->prints(", ");

    print(log, instr->getSource1Register());
    log->prints(", ");

    print(log, instr->getSource2Register());
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMMemSrc1Instruction *instr)
{
    if (instr->getBinaryEncoding() && instr->getMemoryReference()->hasDelayedOffset()
        && !instr->getMemoryReference()->getUnresolvedSnippet()) {
        TR::InstOpCode::Mnemonic op = instr->getOpCodeValue();
        int32_t offset = instr->getMemoryReference()->getOffset();
        if (op == TR::InstOpCode::strh && !constantIsUnsignedImmed8(offset)) {
            printARMDelayedOffsetInstructions(log, instr);
            return;
        } else if (!constantIsImmed12(offset)) {
            printARMDelayedOffsetInstructions(log, instr);
            return;
        }
        // deliberate fall through if none of the above cases are taken.
    }

    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    print(log, instr->getMemoryReference());
    log->prints(", ");
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    print(log, instr->getSourceRegister(), instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg);
#else
    print(log, instr->getSourceRegister());
#endif
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMTrg1Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    print(log, instr->getTargetRegister());
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMMemInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    print(log, instr->getMemoryReference());
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMTrg1MemInstruction *instr)
{
    if (instr->getBinaryEncoding() && instr->getMemoryReference()->hasDelayedOffset()
        && !instr->getMemoryReference()->getUnresolvedSnippet()) {
        TR::InstOpCode::Mnemonic op = instr->getOpCodeValue();
        int32_t offset = instr->getMemoryReference()->getOffset();
        if (op == TR::InstOpCode::add && instr->getMemoryReference()->getIndexRegister()) {
            printARMDelayedOffsetInstructions(log, instr);
            return;
        } else if ((op == TR::InstOpCode::ldrsb || op == TR::InstOpCode::ldrh || op == TR::InstOpCode::ldrsh)
            && !constantIsUnsignedImmed8(offset)) {
            printARMDelayedOffsetInstructions(log, instr);
            return;
        } else if (!constantIsImmed12(offset)) {
            printARMDelayedOffsetInstructions(log, instr);
            return;
        }
        // deliberate fall through if none of the above cases are taken.
    }

    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    print(log, instr->getTargetRegister(), instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg);
#else
    print(log, instr->getTargetRegister());
#endif
    log->prints(", ");
    print(log, instr->getMemoryReference());
    TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
    if (symbol && symbol->isSpillTempAuto()) {
        log->printf("\t; spilled for %s", getName(instr->getNode()->getOpCode()));
    }
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMTrg1MemSrc1Instruction *instr)
{
    // strex
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));
    print(log, instr->getTargetRegister());
    log->prints(", ");
    print(log, instr->getSourceRegister());
    log->prints(", ");
    print(log, instr->getMemoryReference());
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMControlFlowInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));

    int i;
    int numTargets = instr->getNumTargets();
    int numSources = instr->getNumSources();
    for (i = 0; i < numTargets; i++) {
        print(log, instr->getTargetRegister(i));
        if (i != numTargets + numSources - 1) {
            log->prints(", ");
        }
    }
    for (i = 0; i < numSources; i++) {
        print(log, instr->getSourceRegister(i));
        if (i != numSources - 1) {
            log->prints(", ");
        }
    }
    if (instr->getOpCode2Value() != TR::InstOpCode::bad) {
        log->printf(", %s", getOpCodeName(&instr->getOpCode2()));
    }
    if (instr->getLabelSymbol()) {
        log->prints(", ");
        print(log, instr->getLabelSymbol());
    }
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::ARMMultipleMoveInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", fullOpCodeName(instr));

    print(log, instr->getMemoryBaseRegister());
    log->printf("%s, {", instr->isWriteBack() ? "!" : "");

    TR::Machine *machine = _cg->machine();

    uint32_t regList = (uint32_t)instr->getRegisterList();
    if (!instr->getOpCode().isVFPOp()) {
        for (int i = 0; regList && i < 16; i++) {
            if (regList & 1) {
                TR::RealRegister::RegNum r;
                r = (TR::RealRegister::RegNum)((int)TR::RealRegister::FirstGPR + i);
                print(log, machine->getRealRegister(r));
                regList >>= 1;
                if (regList && i != 15)
                    log->printc(',');
            } else
                regList >>= 1;
        }
    } else {
        int startReg = (regList & 0x0f000) >> 12;
        int nreg = (regList & 0x00ff) >> 1;
        TR_RegisterSizes size = instr->doubleFPOp() ? TR_DoubleReg : TR_WordReg;
        for (int i = 0; i < nreg; i++) {
            TR::RealRegister::RegNum r = (TR::RealRegister::RegNum)((int)TR::RealRegister::FirstFPR + startReg + i);
            print(log, machine->getRealRegister(r), size);
            if (i != (nreg - 1))
                log->printc(',');
        }
    }
    log->printc('}');

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printARMHelperBranch(OMR::Logger *log, TR::SymbolReference *symRef, uint8_t *bufferPos,
    const char *opcodeName)
{
    TR::MethodSymbol *methodSym = symRef->getSymbol()->castToMethodSymbol();
    intptr_t target = (intptr_t)methodSym->getMethodAddress();
    char *info = "";

    if (_cg->directCallRequiresTrampoline(target, (intptr_t)bufferPos)) {
        int32_t refNum = symRef->getReferenceNumber();
        if (refNum < TR_ARMnumRuntimeHelpers) {
            target = TR::CodeCacheManager::instance()->findHelperTrampoline(refNum, (void *)bufferPos);
            info = " through trampoline";
        } else if (*((uintptr_t *)bufferPos) == 0xe28fe004) // This is a JNI method
        {
            target = *((intptr_t *)(bufferPos + 8));
            info = " long jump";
        } else {
            target = _comp->fe()->methodTrampolineLookup(_comp, symRef, (void *)bufferPos);
            info = " through trampoline";
        }
    }

    const char *name = getName(symRef);
    printPrefix(log, NULL, bufferPos, 4);
    if (name)
        log->printf("%s\t%s\t;%s (" POINTER_PRINTF_FORMAT ")", opcodeName, name, info, target);
    else
        log->printf("%s\t" POINTER_PRINTF_FORMAT "\t\t;%s", opcodeName, target, info);
}

void TR_Debug::print(OMR::Logger *log, TR::MemoryReference *mr)
{
    log->printc('[');
    if (mr->getBaseRegister() != NULL) {
        print(log, mr->getBaseRegister());
        if (mr->isPostIndexed())
            log->printc(']');
        log->prints(", ");
    }
    if (mr->getIndexRegister() != NULL) {
        print(log, mr->getIndexRegister());
        if (mr->getScale() != 0) {
            TR_ASSERT(mr->getScale() == 4, "strides other than 4 unimplemented");
            log->prints(" LSL #2");
        }
    } else {
        if (mr->getSymbolReference().getSymbol() != NULL) {
            print(log, &mr->getSymbolReference());
        } else {
            log->printf("%+d", mr->getOffset());
        }
    }
    if (!mr->isPostIndexed()) {
        log->printc(']');
    }
    if (mr->isImmediatePreIndexed()) {
        log->printc('!');
    }
}

void TR_Debug::print(OMR::Logger *log, TR_ARMOperand2 *op, TR_RegisterSizes size)
{
    switch (op->_opType) {
        case ARMOp2RegLSLImmed:
            print(log, op->_baseRegister, size);
            log->printf(" LSL #%d", op->_shiftOrImmed);
            break;
        case ARMOp2RegLSRImmed:
            print(log, op->_baseRegister, size);
            log->printf(" LSR #%d", op->_shiftOrImmed);
            break;
        case ARMOp2RegASRImmed:
            print(log, op->_baseRegister, size);
            log->printf(" ASR #%d", op->_shiftOrImmed);
            break;
        case ARMOp2RegRORImmed:
            print(log, op->_baseRegister, size);
            log->printf(" ROR #%d", op->_shiftOrImmed);
            break;
        case ARMOp2Reg:
            print(log, op->_baseRegister, size);
            break;
        case ARMOp2RegRRX:
            print(log, op->_baseRegister, size);
            log->prints(" RRX");
            break;
        case ARMOp2RegLSLReg:
            print(log, op->_baseRegister, size);
            log->prints(" LSL ");
            print(log, op->_shiftRegister, size);
            break;
        case ARMOp2RegLSRReg:
            print(log, op->_baseRegister, size);
            log->prints(" LSR ");
            print(log, op->_shiftRegister, size);
            break;
        case ARMOp2RegASRReg:
            print(log, op->_baseRegister, size);
            log->prints(" ASR ");
            print(log, op->_shiftRegister, size);
            break;
        case ARMOp2RegRORReg:
            print(log, op->_baseRegister, size);
            log->prints(" ROR ");
            print(log, op->_shiftRegister, size);
            break;
        case ARMOp2Immed8r:
            if (op->_immedShift)
                log->printf("#0x%08x (0x%x << %d)", op->_shiftOrImmed << op->_immedShift, op->_shiftOrImmed,
                    op->_immedShift);
            else
                log->printf("#0x%08x", op->_shiftOrImmed);
            break;
    }
}

void TR_Debug::printARMGCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *map)
{
    TR::Machine *machine = _cg->machine();
    log->prints("    registers: {");
    for (int i = 15; i >= 0; i--) {
        if (map->getMap() & (1 << i))
            log->printf("%s ",
                getName(machine->getRealRegister((TR::RealRegister::RegNum)(i + TR::RealRegister::FirstGPR))));
    }

    log->prints("}\n");
}

void TR_Debug::print(OMR::Logger *log, TR::RealRegister *reg, TR_RegisterSizes size)
{
    log->prints(getName(reg, size));
}

const char *TR_Debug::getName(TR::RealRegister *reg, TR_RegisterSizes size)
{
    return getName(reg->getRegisterNumber(), size);
}

const char *TR_Debug::getName(uint32_t realRegisterIndex, TR_RegisterSizes size)
{
    switch (realRegisterIndex) {
        case TR::RealRegister::gr0:
            return "gr0";
        case TR::RealRegister::gr1:
            return "gr1";
        case TR::RealRegister::gr2:
            return "gr2";
        case TR::RealRegister::gr3:
            return "gr3";
        case TR::RealRegister::gr4:
            return "gr4";
        case TR::RealRegister::gr5:
            return "gr5";
        case TR::RealRegister::gr6:
            return "gr6";
        case TR::RealRegister::gr7:
            return "gr7";
        case TR::RealRegister::gr8:
            return "gr8";
        case TR::RealRegister::gr9:
            return "gr9";
        case TR::RealRegister::gr10:
            return "gr10";
        case TR::RealRegister::gr11:
            return "gr11";
        case TR::RealRegister::gr12:
            return "gr12";
        case TR::RealRegister::gr13:
            return "gr13";
        case TR::RealRegister::gr14:
            return "gr14";
        case TR::RealRegister::gr15:
            return "gr15";

#if defined(__VFP_FP__) && !defined(__SOFTFP__)
        case TR::RealRegister::fp0:
            return ((size == TR_DoubleReg) ? "d0" : "s0");
        case TR::RealRegister::fp1:
            return ((size == TR_DoubleReg) ? "d1" : "s2");
        case TR::RealRegister::fp2:
            return ((size == TR_DoubleReg) ? "d2" : "s4");
        case TR::RealRegister::fp3:
            return ((size == TR_DoubleReg) ? "d3" : "s6");
        case TR::RealRegister::fp4:
            return ((size == TR_DoubleReg) ? "d4" : "s8");
        case TR::RealRegister::fp5:
            return ((size == TR_DoubleReg) ? "d5" : "s10");
        case TR::RealRegister::fp6:
            return ((size == TR_DoubleReg) ? "d6" : "s12");
        case TR::RealRegister::fp7:
            return ((size == TR_DoubleReg) ? "d7" : "s14");
        case TR::RealRegister::fp8:
            return ((size == TR_DoubleReg) ? "d8" : "s16");
        case TR::RealRegister::fp9:
            return ((size == TR_DoubleReg) ? "d9" : "s18");
        case TR::RealRegister::fp10:
            return ((size == TR_DoubleReg) ? "d10" : "s20");
        case TR::RealRegister::fp11:
            return ((size == TR_DoubleReg) ? "d11" : "s22");
        case TR::RealRegister::fp12:
            return ((size == TR_DoubleReg) ? "d12" : "s24");
        case TR::RealRegister::fp13:
            return ((size == TR_DoubleReg) ? "d13" : "s26");
        case TR::RealRegister::fp14:
            return ((size == TR_DoubleReg) ? "d14" : "s28");
        case TR::RealRegister::fp15:
            return ((size == TR_DoubleReg) ? "d15" : "s30");

        // For Single-precision registers group (fs0~fs15/FSR)
        case TR::RealRegister::fs0:
            return "S0";
        case TR::RealRegister::fs1:
            return "S1";
        case TR::RealRegister::fs2:
            return "S2";
        case TR::RealRegister::fs3:
            return "S3";
        case TR::RealRegister::fs4:
            return "S4";
        case TR::RealRegister::fs5:
            return "S5";
        case TR::RealRegister::fs6:
            return "S6";
        case TR::RealRegister::fs7:
            return "S7";
        case TR::RealRegister::fs8:
            return "S8";
        case TR::RealRegister::fs9:
            return "S9";
        case TR::RealRegister::fs10:
            return "S10";
        case TR::RealRegister::fs11:
            return "S11";
        case TR::RealRegister::fs12:
            return "S12";
        case TR::RealRegister::fs13:
            return "S13";
        case TR::RealRegister::fs14:
            return "S14";
        case TR::RealRegister::fs15:
            return "S15";
#else
        case TR::RealRegister::fp0:
            return "fp0";
        case TR::RealRegister::fp1:
            return "fp1";
        case TR::RealRegister::fp2:
            return "fp2";
        case TR::RealRegister::fp3:
            return "fp3";
        case TR::RealRegister::fp4:
            return "fp4";
        case TR::RealRegister::fp5:
            return "fp5";
        case TR::RealRegister::fp6:
            return "fp6";
        case TR::RealRegister::fp7:
            return "fp7";
#endif
        default:
            return "???";
    }
}

void TR_Debug::printa(OMR::Logger *log, TR::Snippet *snippet)
{
    switch (snippet->getKind()) {
        case TR::Snippet::IsCall:
            print(log, (TR::ARMCallSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedCall:
            print(log, (TR::ARMUnresolvedCallSnippet *)snippet);
            break;
        case TR::Snippet::IsVirtualUnresolved:
            print(log, (TR::ARMVirtualUnresolvedSnippet *)snippet);
            break;
        case TR::Snippet::IsInterfaceCall:
            print(log, (TR::ARMInterfaceCallSnippet *)snippet);
            break;
        case TR::Snippet::IsStackCheckFailure:
            print(log, (TR::ARMStackCheckFailureSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedData:
            print(log, (TR::UnresolvedDataSnippet *)snippet);
            break;
        case TR::Snippet::IsRecompilation:
            print(log, (TR::ARMRecompilationSnippet *)snippet);
            break;
#ifdef J9_PROJECT_SPECIFIC
        case TR::Snippet::IsHelperCall:
            print(log, (TR::ARMHelperCallSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsMonitorEnter:
            // print(log, (TR::ARMMonitorEnterSnippet *)snippet);
            log->prints("** MonitorEnterSnippet **\n");
            break;
        case TR::Snippet::IsMonitorExit:
            // print(log, (TR::ARMMonitorExitSnippet *)snippet);
            log->prints("** MonitorExitSnippet **\n");
            break;
        default:
            TR_ASSERT(0, "unexpected snippet kind");
    }
}

void TR_Debug::print(OMR::Logger *log, TR::ARMCallSnippet *snippet)
{
#if J9_PROJECT_SPECIFIC
    TR::Node *callNode = snippet->getNode();
    TR::SymbolReference *glueRef = _cg->getSymRef(snippet->getHelper());
    ;
    TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
    TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    printSnippetLabel(log, snippet, bufferPos);
    log->prints(" (");
    log->prints(getName(methodSymRef));
    log->printc(')');

    TR::Machine *machine = _cg->machine();
    TR::Linkage *linkage = _cg->getLinkage(methodSymbol->getLinkageConvention());
    const TR::ARMLinkageProperties &linkageProperties = linkage->getProperties();

    uint32_t numIntArgs = 0;
    uint32_t numFloatArgs = 0;

    int32_t offset;
    if (linkageProperties.getRightToLeft())
        offset = linkage->getOffsetToFirstParm();
    else
        offset = snippet->getSizeOfArguments() + linkage->getOffsetToFirstParm();

    for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++) {
        switch (callNode->getChild(i)->getOpCode().getDataType()) {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
            case TR::Address:
                if (!linkageProperties.getRightToLeft())
                    offset -= 4;
                if (numIntArgs < linkageProperties.getNumIntArgRegs()) {
                    printPrefix(log, NULL, bufferPos, 4);
                    log->printf("str\t[gr7, %+d], ", offset);
                    print(log, machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(numIntArgs)));
                    bufferPos += 4;
                }
                numIntArgs++;
                if (linkageProperties.getRightToLeft())
                    offset += 4;
                break;
            case TR::Int64:
                if (!linkageProperties.getRightToLeft())
                    offset -= 8;
                if (numIntArgs < linkageProperties.getNumIntArgRegs()) {
                    printPrefix(log, NULL, bufferPos, 4);
                    log->printf("str\t[gr7, %+d], ", offset);
                    print(log, machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(numIntArgs)));
                    bufferPos += 4;
                    if (numIntArgs < linkageProperties.getNumIntArgRegs() - 1) {
                        printPrefix(log, NULL, bufferPos, 4);
                        log->printf("str\t[gr7, %+d], ", offset + 4);
                        print(log,
                            machine->getRealRegister(linkageProperties.getIntegerArgumentRegister(numIntArgs + 1)));
                        bufferPos += 4;
                    }
                }
                numIntArgs += 2;
                if (linkageProperties.getRightToLeft())
                    offset += 8;
                break;
// TODO FLOAT
#if 0
         case TR::Float:
            if (!linkageProperties.getRightToLeft())
               offset -= 4;
            if (numFloatArgs < linkageProperties.getNumFloatArgRegs())
               {
               printPrefix(log, NULL, bufferPos, 4);
               log->printf("stfs\t[gr7, %+d], ", offset);
               print(log, machine->getRealRegister(linkageProperties.getFloatArgumentRegister(numFloatArgs)));
               bufferPos += 4;
               }
            numFloatArgs++;
            if (linkageProperties.getRightToLeft())
               offset += 4;
            break;
         case TR::Double:
            if (!linkageProperties.getRightToLeft())
               offset -= 8;
            if (numFloatArgs < linkageProperties.getNumFloatArgRegs())
               {
               printPrefix(log, NULL, bufferPos, 4);
               log->printf("stfd\t[gr7, %+d], ", offset);
               print(log, machine->getRealRegister(linkageProperties.getFloatArgumentRegister(numFloatArgs)));
               bufferPos += 4;
               }
            numFloatArgs++;
            if (linkageProperties.getRightToLeft())
               offset += 8;
            break;
#endif
        }
    }

    printARMHelperBranch(log, glueRef, bufferPos);
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getCallRA());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    if (methodSymRef->isUnresolved())
        log->prints("dd\t0x00000000\t\t; method address (unresolved)");
    else
        log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; method address (interpreted)");
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; lock word for compilation");
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::ARMUnresolvedCallSnippet *snippet)
{
#ifdef J9_PROJECT_SPECIFIC
    TR::SymbolReference *methodSymRef = snippet->getNode()->getSymbolReference();
    TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

    int32_t helperLookupOffset;
    switch (snippet->getNode()->getOpCode().getDataType()) {
        case TR::NoType:
            helperLookupOffset = 0;
            break;
        case TR::Int32:
        case TR::Address:
            helperLookupOffset = 4;
            break;
        case TR::Int64:
            helperLookupOffset = 8;
            break;
        case TR::Float:
            helperLookupOffset = 12;
            break;
        case TR::Double:
            helperLookupOffset = 16;
            break;
    }

    print(log, (TR::ARMCallSnippet *)snippet);
    bufferPos += (snippet->getLength(0) - 12);

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; offset|flag|cpIndex", (helperLookupOffset << 24) | methodSymRef->getCPIndex());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(methodSymRef)->constantPool());
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::ARMVirtualUnresolvedSnippet *snippet)
{
#ifdef J9_PROJECT_SPECIFIC
    TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();

    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    printSnippetLabel(log, snippet, bufferPos);
    log->prints(" (");
    log->prints(getName(callSymRef));
    log->printc(')');

    TR::SymbolReference *glueRef = _cg->getSymRef(TR_ARMvirtualUnresolvedHelper);
    printARMHelperBranch(log, glueRef, bufferPos);
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getReturnLabel()->getCodeLocation());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(callSymRef)->constantPool());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; cpIndex", callSymRef->getCPIndex());
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::ARMInterfaceCallSnippet *snippet)
{
#ifdef J9_PROJECT_SPECIFIC
    TR::SymbolReference *callSymRef = snippet->getNode()->getSymbolReference();
    TR::SymbolReference *glueRef = _cg->getSymRef(TR_ARMinterfaceCallHelper);

    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

    printSnippetLabel(log, snippet, bufferPos);
    log->prints(" (");
    log->prints(getName(callSymRef));
    log->printc(')');

    printARMHelperBranch(log, glueRef, bufferPos);
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; return address", (uintptr_t)snippet->getReturnLabel()->getCodeLocation());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; cpAddress", (uintptr_t)getOwningMethod(callSymRef)->constantPool());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; cpIndex", callSymRef->getCPIndex());
    bufferPos += 4;

    const char *comment[] = { "resolved interface class", "resolved method index" };

    for (int i = 0; i < 2; i++) {
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("dd\t0x%08x\t\t; %s", 0, comment[i]);
        bufferPos += 4;
    }
#if 0
   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; resolved interface class", 0);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; resolved method index", 0);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 1st cached class", -1);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 1st cached target", 0);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 2nd cached class", -1);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 2nd cached target", 0);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 1st lock word", 0);
   bufferPos += 4;

   printPrefix(log, NULL, bufferPos, 4);
   log->printf("dd\t0x%08x\t\t; 2nd lock word", 0);
#endif
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::ARMHelperCallSnippet *snippet)
{
#ifdef J9_PROJECT_SPECIFIC
    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    printSnippetLabel(log, snippet, bufferPos);

    TR::LabelSymbol *restartLabel = snippet->getRestartLabel();
    // int32_t          distance  = target - (uintptr_t)bufferPos;

    printARMHelperBranch(log, snippet->getDestination(), bufferPos);
    bufferPos += 4;

    if (restartLabel) {
        printPrefix(log, NULL, bufferPos, 4);
        // int32_t distance = *((int32_t *) bufferPos) & 0x00ffffff;
        // distance = (distance << 8) >> 8;   // sign extend
        log->printf("b \t" POINTER_PRINTF_FORMAT "\t\t; Return to %s", (intptr_t)(restartLabel->getCodeLocation()),
            getName(restartLabel));
    }
#endif
}

void TR_Debug::print(OMR::Logger *log, TR::ARMStackCheckFailureSnippet *snippet)
{
    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    printSnippetLabel(log, snippet, bufferPos);

    TR::ResolvedMethodSymbol *bodySymbol = _comp->getJittedMethodSymbol();
    TR::SymbolReference *sofRef = _comp->getSymRefTab()->element(TR_stackOverflow);

    bool saveLR
        = (_cg->getSnippetList().size() <= 1 && !bodySymbol->isEHAware() && !_cg->machine()->getLinkRegisterKilled())
        ? true
        : false;
    const TR::ARMLinkageProperties &linkage = _cg->getLinkage()->getProperties();
    uint32_t frameSize = _cg->getFrameSizeInBytes();
    int32_t offset = linkage.getOffsetToFirstLocal();
    uint32_t base, rotate;

    if (constantIsImmed8r(frameSize, &base, &rotate)) {
        // mov R11, frameSize
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("mov\tgr11, #0x%08x", frameSize);
        bufferPos += 4;
    } else if (!constantIsImmed8r(frameSize - offset, &base, &rotate) && constantIsImmed8r(-offset, &base, &rotate)) {
        // sub R11, R11, -offset
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("sub\tgr11, gr11, #0x%08x", -offset);
        bufferPos += 4;
    } else {
        // R11 <- frameSize
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("mov\tgr11, #0x%08x", frameSize & 0xFF);
        bufferPos += 4;
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("add\tgr11, gr11, #0x%08x", (frameSize >> 8 & 0xFF) << 8);
        bufferPos += 4;
        printPrefix(log, NULL, bufferPos, 4);
        log->printf("add\tgr11, gr11, #0x%08x", (frameSize >> 16 & 0xFF) << 16);
        bufferPos += 4;
        // There is no need in another add, since a stack frame can never be that big.
    }

    printPrefix(log, NULL, bufferPos, 4);
    log->prints("add\tgr7, gr7, gr11");
    bufferPos += 4;
    if (saveLR) {
        printPrefix(log, NULL, bufferPos, 4);
        log->prints("str\t[gr7, +0], gr14");
        bufferPos += 4;
    }

    printARMHelperBranch(log, sofRef, bufferPos);
    bufferPos += 4;

    if (saveLR) {
        printPrefix(log, NULL, bufferPos, 4);
        log->prints("ldr\tgr14, [gr7, +0]");
        bufferPos += 4;
    }
    printPrefix(log, NULL, bufferPos, 4);
    log->prints("sub\tgr7, gr7, gr11");
    bufferPos += 4;
    printPrefix(log, NULL, bufferPos, 4);
    log->prints("b\t");
    print(log, snippet->getReStartLabel());
}

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::print(OMR::Logger *log, TR::UnresolvedDataSnippet *snippet)
{
    TR::MemoryReference *mr = snippet->getMemoryReference();
    TR::SymbolReference *symRef = snippet->getDataSymbolReference();

    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    printSnippetLabel(log, snippet, bufferPos);
    log->prints(" (");
    log->prints(getName(symRef));
    log->printc(')');

    printARMHelperBranch(log, _cg->getSymRef(snippet->getHelper()), bufferPos);
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; return address", snippet->getAddressOfDataReference());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; cpIndex", symRef->getCPIndex());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t" POINTER_PRINTF_FORMAT "\t\t; cpAddress", getOwningMethod(symRef)->constantPool());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0x%08x\t\t; memory reference offset",
        symRef->getSymbol()->isConstObjectRef() ? 0 : mr->getOffset());
    bufferPos += 4;

    printPrefix(log, NULL, bufferPos, 4);
    log->printf("dd\t0xe3a0%x000\t\t; instruction template",
        toRealRegister(mr->getModBase())->getRegisterNumber() - TR::RealRegister::FirstGPR);
}
#endif

void TR_Debug::print(OMR::Logger *log, TR::ARMRecompilationSnippet *snippet)
{
#ifdef J9_PROJECT_SPECIFIC
    uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

    printSnippetLabel(log, snippet->getSnippetLabel(), cursor, "Counting Recompilation Snippet");

    TR::SymbolReference *symRef = _cg->getSymRef(TR_ARMcountingRecompileMethod);
    printARMHelperBranch(log, symRef, cursor);
    cursor += 4;

    // methodInfo
    printPrefix(log, NULL, cursor, 4);
    log->printf("dd \t0x%08x\t\t;%s", _comp->getRecompilationInfo()->getMethodInfo(), "methodInfo");
    cursor += 4;

    // startPC
    printPrefix(log, NULL, cursor, 4);
    log->printf("dd \t0x%08x\t\t; startPC ", _cg->getCodeStart());
#endif
}

void TR_Debug::printARMDelayedOffsetInstructions(OMR::Logger *log, TR::ARMMemInstruction *instr)
{
    bool regSpilled;
    uint8_t *bufferPos = instr->getBinaryEncoding();
    int32_t offset = instr->getMemoryReference()->getOffset();
    TR::InstOpCode::Mnemonic op = instr->getOpCodeValue();
    TR::RealRegister *base = toRealRegister(instr->getMemoryReference()->getBaseRegister());

    intParts localVal(offset);
    const size_t regNameSize = 6;
    char *regName = (char *)_comp->trMemory()->allocateHeapMemory(regNameSize);
    snprintf(regName, regNameSize, "gr%d", (*(uint32_t *)bufferPos >> TR::RealRegister::pos_RD) & 0xf);

    if (op == TR::InstOpCode::str || op == TR::InstOpCode::strh || op == TR::InstOpCode::strb
        || toRealRegister(instr->getMemoryDataRegister())->getRegisterNumber() == base->getRegisterNumber()) {
        regSpilled = true;
        printPrefix(log, instr, bufferPos, 4);
        log->printf("str\t[gr7, -4], %s", regName);
        bufferPos += 4;
    } else {
        regSpilled = false;
    }

    printPrefix(log, instr, bufferPos, 4);
    log->printf("mov\t%s, 0x%08x (0x%x << 24)", regName, localVal.getByte3() << 24, localVal.getByte3());
    bufferPos += 4;

    printPrefix(log, instr, bufferPos, 4);
    log->printf("add\t%s, %s, 0x%08x (0x%x << 16)", regName, regName, localVal.getByte2() << 16, localVal.getByte2());
    bufferPos += 4;

    printPrefix(log, instr, bufferPos, 4);
    log->printf("add\t%s, %s, 0x%08x (0x%x << 8)", regName, regName, localVal.getByte1() << 8, localVal.getByte1());
    bufferPos += 4;

    printPrefix(log, instr, bufferPos, 4);
    log->printf("add\t%s, %s, 0x%08x", regName, regName, localVal.getByte0());
    bufferPos += 4;

    printPrefix(log, instr, bufferPos, 4);
    log->printf("%s\t", fullOpCodeName(instr));
    print(log, instr->getTargetRegister());
    log->prints(", ");
    print(log, instr->getMemoryReference());
    bufferPos += 4;

    if (regSpilled) {
        printPrefix(log, instr, bufferPos, 4);
        log->printf("ldr\t%s, [gr7, -4]", regName);
        bufferPos += 4;
    }

    dumpDependencies(log, instr);
    log->flush();

    return;
}

static const char *opCodeToNameMap[] = {
    "bad", "add", "add_r", "adc", "adc_r", "and", "and_r", "b", "bl", "bic", "bic_r", "cmp", "cmn", "eor", "eor_r",
    "ldfs", "ldfd", "ldm", "ldmia", "ldr", "ldrb", "ldrsb", "ldrh", "ldrsh", "mul", "mul_r", "mla", "mla_r", "smull",
    "smull_r", "umull", "umull_r", "smlal", "smlal_r", "umlal", "umlal_r", "mov", "mov_r", "mvn", "mvn_r", "orr",
    "orr_r", "teq", "tst", "sub", "sub_r", "sbc", "sbc_r", "rsb", "rsb_r", "rsc", "rsc_r", "stfs", "stfd", "str",
    "strb", "strh", "stm", "stmdb", "swp", "sxtb", "sxth", "uxtb", "uxth", "fence", "ret", "wrtbar", "proc", "dd",
    "dmb_v6", "dmb", "dmb_st", "ldrex", "strex", "iflong", "setblong", "setbool", "setbflt", "lcmp", "flcmpl", "flcmpg",
    "idiv", "irem", "label", "vgdnop", "nop",
#if defined(__VFP_FP__) && !defined(__SOFTFP__)
#if defined(__ARM_ARCH_7A__)
    "vabs", // fabsd (vabs<c>.f64)
    "vabs", // fabss (vabs<c>.f32)
    "vadd", // faddd (vadd<c>.f64)
    "vadd", // fadds (vadd<c>.f32)
    "vcmp", // fcmpd (vcmp<c>.f64)
    "vcmp", // fcmps (vcmp<c>.f32)
    "vcmp", // fcmpzd (vcmp<c>.f64)
    "vcmp", // fcmpzs (vcmp<c>.f32)
    "vmov", // fcpyd (vmov<c>.f64)
    "vmov", // fcpys (vmov<c>.f32)
    "vcvt", // fcvtds (vcvt<c>.f64.f32)
    "vcvt", // fcvtsd (vcvt<c>.f32.f64)
    "vdiv", // fdivd (vdiv<c>.f64)
    "vdiv", // fdivs (vdiv<c>.f32)
    "vldr", // fldd  (vldr)
    "vldm", // fldmd (vldm)
    "vldm", // fldms (vldm)
    "vldr", // flds (vldr)
    "vmla", // fmacd (vmla.f64)
    "vmla", // fmacs (vmla.f32)
    "vmov", // fmdrr (vmov)
    "vmov", // fmrrd (vmov)
    "vmov", // fmrrs (vmov)
    "vmov", // fmrs (vmov)
    "vmrs", // fmrx (vmrs)
    "vmls", // fmscd (vmls.f64)
    "vmls", // fmscs (vmls.f32)
    "vmov", // fmsr (vmov)
    "vmov", // fmsrr (vmov)
    "vmrs", // fmstat (vmrs APSR_nzcv, FPSCR)
    "vmul", // fmuld (vmul.f64)
    "vmul", // fmuls (vmul.f32)
    "vmsr", // fmxr (vmsr)
    "vneg", // fnegd (vneg.f64)
    "vneg", // fnegs (vneg.f32)
    "vnmla", // fnmacd (vnmla.f64)
    "vnmla", // fnmacs (vnmla.f32)
    "vnmls", // fnmscd (vnmls.f64)
    "vnmls", // fnmscs (vnmls.f32)
    "vnmul", // fnmuld (vnmul.f64)
    "vnmul", // fnmuls (vnmul.f32)
    "vcvt", // fsitod (vcvt.f64.s32)
    "vcvt", // fsitos (vcvt.f32.s32)
    "vsqrt", // fsqrtd (vsqrt.f64)
    "vsqrt", // fsqrts (vsqrt.f32)
    "vstr", // fstd (vstr)
    "vstm", // fstmd (vstm)
    "vstm", // fstms (vstm)
    "vstr", // fsts (vstr)
    "vsub", // fsubd (vsub.f64)
    "vsub", // fsubs (vsub.f32)
    "vcvt", // ftosizd (vcvt.s32.f64)
    "vcvt", // ftosizs (vcvt.s32.f32)
    "vcvt", // ftouizd (vcvt.u32.f64)
    "vcvt", // ftouizs (vcvt.u32.f32)
    "vcvt", // fuitod (vcvt.f64,u32)
    "vcvt" // fuitos (vcvt.f32.u32)
#else
    "fabsd", // fabsd (vabs<c>.f64)
    "fabss", // fabss (vabs<c>.f32)
    "faddd", // faddd (vadd<c>.f64)
    "fadds", // fadds (vadd<c>.f32)
    "fcmpd", // fcmpd (vcmp<c>.f64)
    "fcmps", // fcmps (vcmp<c>.f32)
    "fcmpzd", // fcmpzd (vcmp<c>.f64)
    "fcmpzs", // fcmpzs (vcmp<c>.f32)
    "fcpyd", // fcpyd (vmov<c>.f64)
    "fcpys", // fcpys (vmov<c>.f32)
    "fcvtds", // fcvtds (vcvt<c>.f64.f32)
    "fcvtsd", // fcvtsd (vcvt<c>.f32.f64)
    "fdivd", // fdivd (vdiv<c>.f64)
    "fdivs", // fdivs (vdiv<c>.f32)
    "fldd", // fldd  (vldr)
    "fldmd", // fldmd (vldm)
    "fldms", // fldms (vldm)
    "flds", // flds (vldr)
    "fmacd", // fmacd (vmla.f64)
    "fmacs", // fmacs (vmla.f32)
    "fmdrr", // fmdrr (vmov)
    "fmrrd", // fmrrd (vmov)
    "fmrrs", // fmrrs (vmov)
    "fmrs", // fmrs (vmov)
    "fmrx", // fmrx (vmrs)
    "fmscd", // fmscd (vmls.f64)
    "fmscs", // fmscs (vmls.f32)
    "fmsr", // fmsr (vmov)
    "fmsrr", // fmsrr (vmov)
    "fmstat", // fmstat (vmrs APSR_nzcv, FPSCR)
    "fmuld", // fmuld (vmul.f64)
    "fmuls", // fmuls (vmul.f32)
    "fmxr", // fmxr (vmsr)
    "fnegd", // fnegd (vneg.f64)
    "fnegs", // fnegs (vneg.f32)
    "fnmacd", // fnmacd (vnmla.f64)
    "fnmacs", // fnmacs (vnmla.f32)
    "fnmscd", // fnmscd (vnmls.f64)
    "fnmscs", // fnmscs (vnmls.f32)
    "fnmuld", // fnmuld (vnmul.f64)
    "fnmuls", // fnmuls (vnmul.f32)
    "fsitod", // fsitod (vcvt.f64.s32)
    "fsitos", // fsitos (vcvt.f32.s32)
    "fsqrtd", // fsqrtd (vsqrt.f64)
    "fsqrts", // fsqrts (vsqrt.f32)
    "fstd", // fstd (vstr)
    "fstmd", // fstmd (vstm)
    "fstms", // fstms (vstm)
    "fsts", // fsts (vstr)
    "fsubd", // fsubd (vsub.f64)
    "fsubs", // fsubs (vsub.f32)
    "ftosizd", // ftosizd (vcvt.s32.f64)
    "ftosizs", // ftosizs (vcvt.s32.f32)
    "ftouizd", // ftouizd (vcvt.u32.f64)
    "ftouizs", // ftouizs (vcvt.u32.f32)
    "fuitod", // fuitod (vcvt.f64,u32)
    "fuitos" // fuitos (vcvt.f32.u32)
#endif
#endif
};

const char *TR_Debug::getOpCodeName(TR::InstOpCode *opCode) { return opCodeToNameMap[opCode->getOpCodeValue()]; }

#endif
