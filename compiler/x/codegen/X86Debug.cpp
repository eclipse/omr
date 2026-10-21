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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/GCRegisterMap.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "ras/Logger.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "codegen/InstOpCode.hpp"
#include "env/CompilerEnv.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "x/codegen/CallSnippet.hpp"
#endif

namespace TR {
class Register;
}

void TR_Debug::printx(OMR::Logger *log, TR::Instruction *instr)
{
    switch (instr->getKind()) {
        case TR::Instruction::IsNoOperand:
            print(log, (TR::X86NoOperandInstruction *)instr);
            break;
        case TR::Instruction::IsLabel:
            print(log, (TR::X86LabelInstruction *)instr);
            break;
        case TR::Instruction::IsPadding:
            print(log, (TR::X86PaddingInstruction *)instr);
            break;
        case TR::Instruction::IsAlignment:
            print(log, (TR::X86AlignmentInstruction *)instr);
            break;
        case TR::Instruction::IsBoundaryAvoidance:
            print(log, (TR::X86BoundaryAvoidanceInstruction *)instr);
            break;
        case TR::Instruction::IsPatchableCodeAlignment:
            print(log, (TR::X86PatchableCodeAlignmentInstruction *)instr);
            break;
#ifdef J9_PROJECT_SPECIFIC
        case TR::Instruction::IsVirtualGuardNOP:
            print(log, (TR::X86VirtualGuardNOPInstruction *)instr);
            break;
#endif
        case TR::Instruction::IsFence:
            print(log, (TR::X86FenceInstruction *)instr);
            break;
        case TR::Instruction::IsImm:
            print(log, (TR::X86ImmInstruction *)instr);
            break;
        case TR::Instruction::IsImm64:
            print(log, (TR::AMD64Imm64Instruction *)instr);
            break;
        case TR::Instruction::IsImm64Sym:
            print(log, (TR::AMD64Imm64SymInstruction *)instr);
        case TR::Instruction::IsImmSnippet:
            print(log, (TR::X86ImmSnippetInstruction *)instr);
            break;
        case TR::Instruction::IsImmSym:
            print(log, (TR::X86ImmSymInstruction *)instr);
            break;
        case TR::Instruction::IsReg:
            print(log, (TR::X86RegInstruction *)instr);
            break;
        case TR::Instruction::IsRegReg:
            print(log, (TR::X86RegRegInstruction *)instr);
            break;
        case TR::Instruction::IsRegMaskReg:
            print(log, (TR::X86RegMaskRegInstruction *)instr);
            break;
        case TR::Instruction::IsRegMaskMem:
            print(log, (TR::X86RegMaskMemInstruction *)instr);
            break;
        case TR::Instruction::IsRegRegReg:
            print(log, (TR::X86RegRegRegInstruction *)instr);
            break;
        case TR::Instruction::IsRegMaskRegReg:
            print(log, (TR::X86RegMaskRegRegInstruction *)instr);
            break;
        case TR::Instruction::IsRegMaskRegRegImm:
            print(log, (TR::X86RegMaskRegRegImmInstruction *)instr);
            break;
        case TR::Instruction::IsRegRegImm:
            print(log, (TR::X86RegRegImmInstruction *)instr);
            break;
#ifdef TR_TARGET_64BIT
        case TR::Instruction::IsRegImm64:
        case TR::Instruction::IsRegImm64Sym:
            print(log, (TR::AMD64RegImm64Instruction *)instr);
            break;
#endif
        case TR::Instruction::IsRegImm:
        case TR::Instruction::IsRegImmSym:
            print(log, (TR::X86RegImmInstruction *)instr);
            break;
        case TR::Instruction::IsRegMem:
            print(log, (TR::X86RegMemInstruction *)instr);
            break;
        case TR::Instruction::IsRegMemImm:
            print(log, (TR::X86RegMemImmInstruction *)instr);
            break;
        case TR::Instruction::IsRegRegMem:
            print(log, (TR::X86RegRegMemInstruction *)instr);
            break;
        case TR::Instruction::IsMem:
        case TR::Instruction::IsMemTable:
        case TR::Instruction::IsCallMem:
            print(log, (TR::X86MemInstruction *)instr);
            break;
        case TR::Instruction::IsMemImm:
        case TR::Instruction::IsMemImmSym:
        case TR::Instruction::IsMemImmSnippet:
            print(log, (TR::X86MemImmInstruction *)instr);
            break;
        case TR::Instruction::IsMemReg:
            print(log, (TR::X86MemRegInstruction *)instr);
            break;
        case TR::Instruction::IsMemMaskReg:
            print(log, (TR::X86MemMaskRegInstruction *)instr);
            break;
        case TR::Instruction::IsMemRegImm:
            print(log, (TR::X86MemRegImmInstruction *)instr);
            break;
        case TR::Instruction::IsVFPSave:
            print(log, (TR::X86VFPSaveInstruction *)instr);
            break;
        case TR::Instruction::IsVFPRestore:
            print(log, (TR::X86VFPRestoreInstruction *)instr);
            break;
        case TR::Instruction::IsVFPDedicate:
            print(log, (TR::X86VFPDedicateInstruction *)instr);
            break;
        case TR::Instruction::IsVFPRelease:
            print(log, (TR::X86VFPReleaseInstruction *)instr);
            break;
        case TR::Instruction::IsVFPCallCleanup:
            print(log, (TR::X86VFPCallCleanupInstruction *)instr);
            break;
        default:
            TR_ASSERT(0, "Unknown instruction kind");
            // fall thru
        case TR::Instruction::IsNotExtended: {
            printPrefix(log, instr);
            log->printf("%-32s", getMnemonicName(&instr->getOpCode()));
            printInstructionComment(log, 0, instr);
            dumpDependencies(log, instr);
            log->flush();
        } break;
    }
}

void TR_Debug::printPrefix(OMR::Logger *log, TR::Instruction *instr)
{
    printPrefix(log, instr, instr->getBinaryEncoding(), instr->getBinaryLength());
}

void TR_Debug::printDependencyConditions(OMR::Logger *log, TR::RegisterDependencyGroup *conditions,
    uint8_t numConditions, char *prefix)
{
    const size_t bufSize = 32;
    char buf[bufSize];
    char *cursor;
    int len, i;

    for (i = 0; i < numConditions; i++) {
        cursor = buf;
        size_t remainingSize = bufSize;
        memset(buf, ' ', 23);
        len = snprintf(cursor, remainingSize, "    %s[%d]", prefix, i);
        *(cursor + len) = ' ';
        cursor += 12;
        remainingSize -= 12;

        *(cursor++) = '(';
        remainingSize--;
        TR::RegisterDependency *regDep = conditions->getRegisterDependency(i);
        if (regDep->isNoReg()) {
            len = snprintf(cursor, remainingSize, "NoReg");
        } else if (regDep->isByteReg()) {
            len = snprintf(cursor, remainingSize, "ByteReg");
        } else if (regDep->isSpilledReg()) {
            len = snprintf(cursor, remainingSize, "SpilledReg");
        } else {
            TR::RealRegister::RegNum r = regDep->getRealRegister();
            len = snprintf(cursor, remainingSize, "%s", getName(_cg->machine()->getRealRegister(r)));
        }

        *(cursor + len) = ')';
        *(cursor + len + 1) = 0x00;

        log->prints(buf);

        if (conditions->getRegisterDependency(i)->getRegister()) {
            printFullRegInfo(log, conditions->getRegisterDependency(i)->getRegister());
        } else {
            log->prints("[ None        ]\n");
        }
    }
}

void TR_Debug::printFullRegisterDependencyInfo(OMR::Logger *log, TR::RegisterDependencyConditions *conditions)
{
    if (conditions->getNumPreConditions() > 0) {
        printDependencyConditions(log, conditions->getPreConditions(), conditions->getNumPreConditions(), "Pre");
    }

    if (conditions->getNumPostConditions() > 0) {
        printDependencyConditions(log, conditions->getPostConditions(), conditions->getNumPostConditions(), "Post");
    }
}

void TR_Debug::dumpDependencyGroup(OMR::Logger *log, TR::RegisterDependencyGroup *group, int32_t numConditions,
    char *prefix, bool omitNullDependencies)
{
    TR::RealRegister::RegNum r;
    TR::Register *virtReg;
    int32_t i;
    bool foundDep = false;

    log->printf("\n\t%s:", prefix);
    for (i = 0; i < numConditions; ++i) {
        TR::RegisterDependency *regDep = group->getRegisterDependency(i);
        virtReg = regDep->getRegister();

        if (omitNullDependencies && !virtReg) {
            continue;
        }

        r = regDep->getRealRegister();
        log->printf(" [%s : ", getName(virtReg));
        if (regDep->isNoReg())
            log->prints("NoReg]");
        else if (regDep->isByteReg())
            log->prints("ByteReg]");
        else if (regDep->isSpilledReg())
            log->prints("SpilledReg]");
        else
            log->printf("%s]", getName(_cg->machine()->getRealRegister(r)));

        foundDep = true;
    }

    if (!foundDep)
        log->prints(" None");
}

void TR_Debug::dumpDependencies(OMR::Logger *log, TR::Instruction *instr)
{
    // If we are in instruction selection or register assignment and
    // dependency information is requested, dump it.
    //
    if (_cg->getStackAtlas() // not in instruction selection
        && !(_registerAssignmentTraceFlags & TRACERA_IN_PROGRESS && // not in register assignment...
            _comp->getOption(TR_TraceRA)) // or dependencies are not traced
    )
        return;

    TR::RegisterDependencyConditions *deps = instr->getDependencyConditions();
    if (!deps)
        return; // Nothing to dump

    if (deps->getNumPreConditions() > 0)
        dumpDependencyGroup(log, deps->getPreConditions(), deps->getNumPreConditions(), " PRE", true);

    if (deps->getNumPostConditions() > 0)
        dumpDependencyGroup(log, deps->getPostConditions(), deps->getNumPostConditions(), "POST", true);

    log->flush();
}

void TR_Debug::printRegisterInfoHeader(OMR::Logger *log, TR::Instruction *instr)
{
    log->prints("\n  Referenced Regs:        Register         State        Assigned      Total Future Flags\n");
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::Instruction *instr)
{
    if (instr->getDependencyConditions()) {
        printRegisterInfoHeader(log, instr);
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }
}

void TR_Debug::print(OMR::Logger *log, TR::X86PaddingInstruction *instr)
{
    printPrefix(log, instr);
    if (instr->getBinaryEncoding())
        log->printf("nop (%d byte%s)\t\t%s Padding (%d byte%s)", instr->getBinaryLength(),
            (instr->getBinaryLength() == 1) ? "" : "s", commentString(), instr->getLength(),
            (instr->getLength() == 1) ? "" : "s");
    else
        log->printf("nop\t\t\t%s Padding (%d byte%s)", commentString(), instr->getLength(),
            (instr->getLength() == 1) ? "" : "s");

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86AlignmentInstruction *instr)
{
    uint8_t length = instr->getBinaryLength();
    uint8_t margin = instr->getMargin();

    printPrefix(log, instr);
    if (instr->getBinaryEncoding())
        log->printf("nop (%d byte%s)\t\t%s ", instr->getBinaryLength(), (instr->getBinaryLength() == 1) ? "" : "s",
            commentString());
    else
        log->printf("nop\t\t\t%s ", commentString());

    if (margin)
        log->printf("Alignment (boundary=%d, margin=%d)", instr->getBoundary(), instr->getMargin());
    else
        log->printf("Alignment (boundary=%d)", instr->getBoundary());

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printBoundaryAvoidanceInfo(OMR::Logger *log, TR::X86BoundaryAvoidanceInstruction *instr)
{
    log->printf(" @%d", instr->getBoundarySpacing());
    if (instr->getMaxPadding() < instr->getBoundarySpacing() - 1)
        log->printf(" max %d", instr->getMaxPadding());

    log->prints(" [");

    const char *sep = "";
    for (const TR_AtomicRegion *region = instr->getAtomicRegions(); region->getLength(); region++) {
        log->printf("%s0x%x:%d", sep, region->getStart(), region->getLength());
        sep = ",";
    }

    log->printc(']');
}

void TR_Debug::print(OMR::Logger *log, TR::X86BoundaryAvoidanceInstruction *instr)
{
    printPrefix(log, instr);
    if (instr->getBinaryEncoding())
        log->printf("nop (%d byte%s)\t\t%s ", instr->getBinaryLength(), (instr->getBinaryLength() == 1) ? "" : "s",
            commentString());
    else
        log->printf("nop\t\t\t%s ", commentString());

    log->prints("Avoid boundary");
    printBoundaryAvoidanceInfo(log, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86PatchableCodeAlignmentInstruction *instr)
{
    printPrefix(log, instr);
    if (instr->getBinaryEncoding())
        log->printf("nop (%d byte%s)\t\t%s ", instr->getBinaryLength(), (instr->getBinaryLength() == 1) ? "" : "s",
            commentString());
    else
        log->printf("nop\t\t\t%s ", commentString());

    log->prints("Align patchable code");
    printBoundaryAvoidanceInfo(log, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86NoOperandInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%-32s", getMnemonicName(&instr->getOpCode()));
    printInstructionComment(log, 0, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86LabelInstruction *instr)
{
    printPrefix(log, instr);
    TR::LabelSymbol *label = instr->getLabelSymbol();
    TR::Snippet *snippet = label ? label->getSnippet() : NULL;
    if (instr->getOpCodeValue() == OP::label) {
        print(log, label);

        log->printc(':');
        printInstructionComment(log, snippet ? 2 : 3, instr);

        if (label->isStartInternalControlFlow())
            log->printf("\t%s (Start of internal control flow)", commentString());
        else if (label->isEndInternalControlFlow())
            log->printf("\t%s (End of internal control flow)", commentString());
    } else {
        log->printf("%s\t", getMnemonicName(&instr->getOpCode()));

        if (label) {
            print(log, label);
            printInstructionComment(log, snippet ? 2 : 3, instr);
        } else {
            log->prints("Label L<null>");
            printInstructionComment(log, 2, instr);
        }

        if (snippet)
            log->printf("\t%s (%s)", commentString(), getName(snippet));
    }

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86FenceInstruction *instr)
{
    // Omit fences from post-binary dumps unless they mark basic block boundaries.
    if (instr->getBinaryEncoding() && instr->getNode()->getOpCodeValue() != TR::BBStart
        && instr->getNode()->getOpCodeValue() != TR::BBEnd)
        return;

    TR::Node *node = instr->getNode();
    if (node && node->getOpCodeValue() == TR::BBStart) {
        TR::Block *block = node->getBlock();
        if (block->isExtensionOfPreviousBlock()) {
            log->prints("\n........................................");
        } else {
            log->prints("\n========================================");
        }
    }

    printPrefix(log, instr);
    log->prints(getMnemonicName(&instr->getOpCode()));
    if (instr->getFenceNode()->getNumRelocations() > 0) {
        if (instr->getFenceNode()->getRelocationType() == TR_AbsoluteAddress)
            log->prints(" Absolute [");
        else if (instr->getFenceNode()->getRelocationType() == TR_ExternalAbsoluteAddress)
            log->prints(" External Absolute [");
        else
            log->prints(" Relative [");

        for (auto i = 0U; i < instr->getFenceNode()->getNumRelocations(); ++i)
            log->printf(" " POINTER_PRINTF_FORMAT, instr->getFenceNode()->getRelocationDestination(i));

        log->prints(" ]");
    }

    printInstructionComment(log, (instr->getFenceNode()->getNumRelocations() > 0) ? 1 : 3, instr);

    printBlockInfo(log, node);
    dumpDependencies(log, instr);
    log->flush();
}

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::print(OMR::Logger *log, TR::X86VirtualGuardNOPInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s Site:" POINTER_PRINTF_FORMAT ", ", getMnemonicName(&instr->getOpCode()), instr->getSite());
    print(log, instr->getLabelSymbol());
    printInstructionComment(log, 1, instr);
    dumpDependencies(log, instr);
    log->flush();
}
#endif

void TR_Debug::print(OMR::Logger *log, TR::X86ImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));

    if (instr->getOpCode().isCallImmOp() && instr->getNode()->getSymbolReference()) {
        TR::SymbolReference *symRef = instr->getNode()->getSymbolReference();
        const char *symName = getName(symRef);

        log->printf("%-24s", symName);
        printInstructionComment(log, 0, instr);
        if (symRef->isUnresolved())
            log->prints(" (unresolved method)");
        else
            log->printf(" (" POINTER_PRINTF_FORMAT ")",
                instr->getSourceImmediate()); // TODO:AMD64: Target address gets truncated
    } else {
        printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
        printInstructionComment(log, 2, instr);
    }

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::AMD64Imm64Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));

    if (instr->getOpCode().isCallImmOp() && instr->getNode()->getSymbolReference()) {
        TR::SymbolReference *symRef = instr->getNode()->getSymbolReference();
        const char *symName = getName(symRef);

        log->printf("%-24s", symName);
        printInstructionComment(log, 0, instr);
        if (symRef->isUnresolved())
            log->prints(" (unresolved method)");
        else
            log->printf(" (" POINTER_PRINTF_FORMAT ")", instr->getSourceImmediate());
    } else {
        printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
        printInstructionComment(log, 2, instr);
    }

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::AMD64Imm64SymInstruction *instr)
{
    printPrefix(log, instr);
    TR::Symbol *sym = instr->getSymbolReference()->getSymbol();
    const char *name = getName(instr->getSymbolReference());

    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (sym->getMethodSymbol() && name) {
        log->printf("%-24s%s %s (" POINTER_PRINTF_FORMAT ")", name, commentString(), getOpCodeName(&instr->getOpCode()),
            instr->getSourceImmediate());
    } else if (sym->getLabelSymbol() && name) {
        if (sym->getLabelSymbol()->getSnippet())
            log->printf("%-24s%s %s (%s)", name, commentString(), getOpCodeName(&instr->getOpCode()),
                getName(sym->getLabelSymbol()->getSnippet()));
        else
            log->printf("%-24s%s %s (" POINTER_PRINTF_FORMAT ")", name, commentString(),
                getOpCodeName(&instr->getOpCode()), instr->getSourceImmediate());
    } else {
        printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
        printInstructionComment(log, 2, instr);
    }

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::AMD64RegImm64Instruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), TR_DoubleWordReg);
        log->prints(", ");
    }
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 1, instr);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86VFPSaveInstruction *instr)
{
    printPrefix(log, instr);
    log->prints("vfpSave");

    printInstructionComment(log, 3, instr);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86VFPRestoreInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("vfpRestore [%s]", getName(instr->getSaveInstruction()));

    printInstructionComment(log, 3, instr);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86VFPDedicateInstruction *instr)
{
    print(log, (TR::X86RegMemInstruction *)instr);

    log->printf("%s vfpDedicate %s", commentString(), getName(instr->getTargetRegister()));
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86VFPReleaseInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("vfpRelease [%s]", getName(instr->getDedicateInstruction()));

    printInstructionComment(log, 3, instr);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86VFPCallCleanupInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("vfpCallCleanup (%d bytes)", instr->getStackPointerAdjustment());

    printInstructionComment(log, 3, instr);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86ImmSnippetInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86ImmSymInstruction *instr)
{
    TR::Symbol *sym = instr->getSymbolReference()->getSymbol();
    const char *name = getName(instr->getSymbolReference());

    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    intptr_t targetAddress = 0;

    //  64 bit always gets the targetAddress from the symRef
    if (_comp->target().is64Bit()) {
        // new code patching might have a call to a snippet label, which is not a method
        if (!sym->isLabel()) {
            targetAddress = (intptr_t)instr->getSymbolReference()->getMethodAddress();
        }
    } else {
        targetAddress = instr->getSourceImmediate();
    }

    if (name) {
        log->printf("%-24s", name);
    } else {
        log->printf(POINTER_PRINTF_FORMAT, targetAddress);
    }

    if (sym->getMethodSymbol() && name) {
        log->printf("%s %s (" POINTER_PRINTF_FORMAT ")", commentString(), getOpCodeName(&instr->getOpCode()),
            targetAddress);
    } else if (sym->getLabelSymbol() && name) {
        if (sym->getLabelSymbol()->getSnippet())
            log->printf("%s %s (%s)", commentString(), getOpCodeName(&instr->getOpCode()),
                getName(sym->getLabelSymbol()->getSnippet()));
        else
            log->printf("%s %s (" POINTER_PRINTF_FORMAT ")", commentString(), getOpCodeName(&instr->getOpCode()),
                targetAddress);
    } else {
        log->printf(" \t\t%s %s", commentString(), getOpCodeName(&instr->getOpCode()));
    }

    printInstructionComment(log, 0, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0))
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
    printInstructionComment(log, 3, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    log->prints("    Target            ");
    printFullRegInfo(log, instr->getTargetRegister());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegRegInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0))
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0) && !(instr->getOpCode().sourceRegIsImplicit() != 0))
        log->prints(", ");
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0))
        print(log, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMaskRegInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (instr->getOpCode().targetRegIsImplicit() == 0 || instr->getMaskRegister()) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        if (instr->getMaskRegister()) {
            log->printc('{');
            print(log, instr->getMaskRegister());
            log->printc('}');
        }
        log->prints(", ");
    }

    if (instr->getOpCode().sourceRegIsImplicit() == 0)
        print(log, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));

    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    log->prints("    Target            ");
    printFullRegInfo(log, instr->getTargetRegister());
    log->prints("    Source            ");
    printFullRegInfo(log, instr->getSourceRegister());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 1, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegRegImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0)) {
        print(log, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
        log->prints(", ");
    }
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    log->printf(" \t%s %s", commentString(), getOpCodeName(&instr->getOpCode()));
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMaskRegRegInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));

    if (instr->getOpCode().targetRegIsImplicit() == 0 || instr->getMaskRegister()) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        if (instr->getMaskRegister()) {
            log->printc('{');
            print(log, instr->getMaskRegister());
            log->printc('}');
        }
        log->prints(", ");
    }

    TR_RegisterSizes sourceSize = getSourceSizeFromInstruction(instr);

    if (instr->getOpCode().sourceRegIsImplicit() == 0) {
        print(log, instr->getSource2ndRegister(), sourceSize);
        log->prints(", ");
        print(log, instr->getSourceRegister(), sourceSize);
    }

    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMaskRegRegImmInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));

    if (instr->getOpCode().targetRegIsImplicit() == 0 || instr->getMaskRegister()) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        if (instr->getMaskRegister()) {
            log->printc('{');
            print(log, instr->getMaskRegister());
            log->printc('}');
        }
        log->prints(", ");
    }

    TR_RegisterSizes sourceSize = getSourceSizeFromInstruction(instr);

    if (instr->getOpCode().sourceRegIsImplicit() == 0) {
        print(log, instr->getSource2ndRegister(), sourceSize);
        log->prints(", ");
        print(log, instr->getSourceRegister(), sourceSize);
    }

    log->prints(", ");
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);

    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegRegRegInstruction *instr)
{
    printPrefix(log, instr);
    log->printf("%s\t", getMnemonicName(&instr->getOpCode()));
    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }
    TR_RegisterSizes sourceSize = getSourceSizeFromInstruction(instr);
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0)) {
        print(log, instr->getSource2ndRegister(), sourceSize);
        log->prints(", ");
        print(log, instr->getSourceRegister(), sourceSize);
    }

    printInstructionComment(log, 2, instr);
    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegRegInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    log->prints("    Source            ");
    printFullRegInfo(log, instr->getSourceRegister());
    log->prints("    2ndSource         ");
    printFullRegInfo(log, instr->getSource2ndRegister());
    log->prints("    Target            ");
    printFullRegInfo(log, instr->getTargetRegister());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

int32_t TR_Debug::printPrefixAndMnemonicWithoutBarrier(OMR::Logger *log, TR::Instruction *instr, int32_t barrier)
{
    int32_t barrierLength = ::estimateMemoryBarrierBinaryLength(barrier, _comp->cg());
    int32_t nonBarrierLength = instr->getBinaryLength() - barrierLength;

    printPrefix(log, instr, instr->getBinaryEncoding(), nonBarrierLength);
    log->printf("%s%s\t", (barrier & LockPrefix) ? "lock " : "", getMnemonicName(&instr->getOpCode()));

    return nonBarrierLength;
}

void TR_Debug::printPrefixAndMemoryBarrier(OMR::Logger *log, TR::Instruction *instr, int32_t barrier,
    int32_t barrierOffset)
{
    int32_t barrierLength = ::estimateMemoryBarrierBinaryLength(barrier, _comp->cg());
    uint8_t *barrierStart = instr->getBinaryEncoding() ? (instr->getBinaryEncoding() + barrierOffset) : NULL;

    printPrefix(log, instr, barrierStart, barrierLength);
}

void TR_Debug::print(OMR::Logger *log, TR::X86MemInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    print(log, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86MemInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    printReferencedRegisterInfo(log, instr->getMemoryReference());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86MemImmInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    print(log, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
    log->prints(", ");
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 1, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86MemRegInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    print(log, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0)) {
        log->prints(", ");
        print(log, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
    }
    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86MemMaskRegInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    print(log, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));

    if (instr->getOpCode().targetRegIsImplicit() == 0 || instr->getMaskRegister()) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        if (instr->getMaskRegister()) {
            log->printc('{');
            print(log, instr->getMaskRegister());
            log->printc('}');
        }
        log->prints(", ");
    }

    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86MemRegInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    log->prints("    Source            ");
    printFullRegInfo(log, instr->getSourceRegister());
    printReferencedRegisterInfo(log, instr->getMemoryReference());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86MemRegImmInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    print(log, instr->getMemoryReference(), getTargetSizeFromInstruction(instr));
    log->prints(", ");
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0)) {
        print(log, instr->getSourceRegister(), getSourceSizeFromInstruction(instr));
        log->prints(", ");
    }
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 1, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMemInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }
    print(log, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());
    TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
    if (symbol && symbol->isSpillTempAuto()) {
        log->printf("%s, spilled for %s", commentString(), getName(instr->getNode()->getOpCode()));
    }

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMaskMemInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    if (instr->getOpCode().targetRegIsImplicit() == 0 || instr->getMaskRegister()) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        if (instr->getMaskRegister()) {
            log->printc('{');
            print(log, instr->getMaskRegister());
            log->printc('}');
        }
        log->prints(", ");
    }

    print(log, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());
    TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
    if (symbol && symbol->isSpillTempAuto()) {
        log->printf("%s, spilled for %s", commentString(), getName(instr->getNode()->getOpCode()));
    }

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegMemInstruction *instr)
{
    printRegisterInfoHeader(log, instr);
    log->prints("    Target            ");
    printFullRegInfo(log, instr->getTargetRegister());

    printReferencedRegisterInfo(log, instr->getMemoryReference());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegMemImmInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }

    print(log, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
    log->prints(", ");
    printIntConstant(log, instr->getSourceImmediate(), 16, getImmediateSizeFromInstruction(instr), true);
    printInstructionComment(log, 1, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::X86RegRegMemInstruction *instr)
{
    int32_t barrier = memoryBarrierRequired(instr->getOpCode(), instr->getMemoryReference(), _cg, false);
    int32_t barrierOffset = printPrefixAndMnemonicWithoutBarrier(log, instr, barrier);

    if (!(instr->getOpCode().targetRegIsImplicit() != 0)) {
        print(log, instr->getTargetRegister(), getTargetSizeFromInstruction(instr));
        log->prints(", ");
    }
    if (!(instr->getOpCode().sourceRegIsImplicit() != 0)) {
        print(log, instr->getSource2ndRegister(), getSourceSizeFromInstruction(instr));
        log->prints(", ");
    }
    print(log, instr->getMemoryReference(), getSourceSizeFromInstruction(instr));
    printInstructionComment(log, 2, instr);
    printMemoryReferenceComment(log, instr->getMemoryReference());
    TR::Symbol *symbol = instr->getMemoryReference()->getSymbolReference().getSymbol();
    if (symbol && symbol->isSpillTempAuto()) {
        log->printf("%s, spilled for %s", commentString(), getName(instr->getNode()->getOpCode()));
    }

    if (barrier & NeedsExplicitBarrier)
        printPrefixAndMemoryBarrier(log, instr, barrier, barrierOffset);

    dumpDependencies(log, instr);
    log->flush();
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::X86RegRegMemInstruction *instr)
{
    printReferencedRegisterInfo(log, instr->getMemoryReference());

    printFullRegInfo(log, instr->getSourceRegister());
    log->prints("    2ndSource         ");
    printFullRegInfo(log, instr->getSource2ndRegister());
    log->prints("    Target            ");
    printFullRegInfo(log, instr->getTargetRegister());

    if (instr->getDependencyConditions()) {
        printFullRegisterDependencyInfo(log, instr->getDependencyConditions());
    }

    log->flush();
}

void TR_Debug::print(OMR::Logger *log, TR::MemoryReference *mr, TR_RegisterSizes operandSize)
{
    const char *typeSpecifier[10] = { "byte", // TR_ByteReg
        "word", // TR_HalfWordReg
        "dword", // TR_WordReg
        "qword", // TR_DoubleWordReg
        "oword", // TR_QuadWordReg
        "dword", // TR_FloatReg
        "qword", // TR_DoubleReg
        "xmmword", // TR_VectorReg128
        "ymmword", // TR_VectorReg256
        "zmmword" }; // TR_VectorReg512

    TR_RegisterSizes addressSize = (_comp->target().cpu.isAMD64() ? TR_DoubleWordReg : TR_WordReg);
    bool hasTerm = false;
    bool hasPrecedingTerm = false;
    log->printf("%s ptr [", typeSpecifier[operandSize]);
    if (mr->getBaseRegister()) {
        print(log, mr->getBaseRegister(), addressSize);
        hasPrecedingTerm = true;
        hasTerm = true;
    }

    if (mr->getIndexRegister()) {
        if (hasPrecedingTerm)
            log->printc('+');
        else
            hasPrecedingTerm = true;
        log->printf("%d*", mr->getStrideMultiplier());
        print(log, mr->getIndexRegister(), addressSize);
        hasTerm = true;
    }

    TR::Symbol *sym = mr->getSymbolReference().getSymbol();

    if (sym != NULL || mr->getSymbolReference().getOffset() != 0) {
        intptr_t disp32 = mr->getDisplacement();

        if (!hasPrecedingTerm) {
            // Annotate the constant emitted to indicate it is an absolute address,
            // and further annotate to indicate RIP-relative addressability.
            //
#ifdef TR_TARGET_64BIT
            if (mr->getForceRIPRelative()) {
                log->prints("rip $");
            } else
#endif
            {
                log->printc('$');
            }

            // Treat this as an absolute reference and display in base16.
            //
            printIntConstant(log, disp32, 16, addressSize, true);
        } else {
            // Treat this as a relative offset and display in base10, or in base16 if the displacement was
            // explicitly forced wider.
            //
            if ((disp32 != 0) || mr->isForceWideDisplacement()) {
                if (disp32 > 0) {
                    log->printc('+');
                } else {
                    log->printc('-');
                    disp32 = -disp32;
                }
            }

            if (mr->isForceWideDisplacement())
                printIntConstant(log, disp32, 16, TR_WordReg);
            else if (disp32 != 0)
                printIntConstant(log, disp32, 16);
        }

        hasTerm = true;
    }

    if (!hasTerm) {
        // This must be an absolute memory reference to a label
        //
        TR::LabelSymbol *label = mr->getLabel();

        TR_ASSERT_FATAL(label, "expecting a label");

        TR::X86ConstantDataSnippet *cds = NULL;
        if (label->getSnippet() && label->getSnippet()->getKind() == TR::Snippet::IsConstantData) {
            cds = static_cast<TR::X86ConstantDataSnippet *>(label->getSnippet());
        }

        int64_t disp = (int64_t)(label->getCodeLocation());

        if (label && !cds) {
            print(log, label);
            if (disp) {
                log->prints(" : ");
                printHexConstant(log, disp, _comp->target().is64Bit() ? 16 : 8, false);
            }
        } else if (disp) {
            printHexConstant(log, _comp->target().is64Bit() ? disp : (uint32_t)disp, _comp->target().is64Bit() ? 16 : 8,
                true);
        } else if (cds) {
            log->prints("Data ");
            print(log, label);
            log->prints(": ");
            auto data = cds->getRawData();
            for (auto i = 0; i < cds->getDataSize(); i++) {
                log->printf("%02x ", 0xff & (unsigned int)(data[i]));
            }
            log->prints("| ");
            cds->printValue(log, this);
        } else {
            log->prints("UNKNOWN DATA");
        }
    }

    log->printc(']');
}

int32_t TR_Debug::printIntConstant(OMR::Logger *log, int64_t value, int8_t radix, TR_RegisterSizes size,
    bool padWithZeros)
{
    const int8_t registerSizeToWidth[7] = { 2, // TR_ByteReg
        4, // TR_HalfWordReg
        8, // TR_WordReg
        16, // TR_DoubleWordReg
        32, // TR_QuadWordReg
        8, // TR_FloatReg
        16 }; // TR_DoubleReg

    int8_t width = registerSizeToWidth[size];

    switch (radix) {
        case 10:
            return printDecimalConstant(log, value, width, padWithZeros);

        case 16:
            return printHexConstant(log, value, width, padWithZeros);

        default:
            TR_ASSERT(0, "Can't print in unimplemented radix %d\n", radix);
            break;
    }

    return 0;
}

int32_t TR_Debug::printDecimalConstant(OMR::Logger *log, int64_t value, int8_t width, bool padWithZeros)
{
    log->printf("%lld", value);
    return 0;
}

int32_t TR_Debug::printHexConstant(OMR::Logger *log, int64_t value, int8_t width, bool padWithZeros)
{
    // we probably need to revisit generateMasmListingSyntax
    const char *prefix = _comp->target().isLinux() ? "0x" : (_cg->generateMasmListingSyntax() ? "0" : "0x");
    const char *suffix = _comp->target().isLinux() ? "" : (_cg->generateMasmListingSyntax() ? "h" : "");

    if (padWithZeros)
        log->printf("%s%0*llx%s", prefix, width, value, suffix);
    else
        log->printf("%s%llx%s", prefix, value, suffix);

    return 0;
}

void TR_Debug::printFPRegisterComment(OMR::Logger *log, TR::Register *source, TR::Register *target)
{
    log->prints(" using ");
    if (target)
        print(log, target);
    if (source && target)
        log->prints(" & ");
    if (source)
        print(log, source);
}

void TR_Debug::printInstructionComment(OMR::Logger *log, int32_t tabStops, TR::Instruction *instr)
{
    while (tabStops-- > 0)
        log->printc('\t');

    log->printf("%s %s", commentString(), getOpCodeName(&instr->getOpCode()));
    dumpInstructionComments(log, instr);
}

void TR_Debug::printMemoryReferenceComment(OMR::Logger *log, TR::MemoryReference *mr)
{
    TR::Symbol *symbol = mr->getSymbolReference().getSymbol();

    if (symbol == NULL && mr->getSymbolReference().getOffset() == 0)
        return;

    if (symbol && symbol->isSpillTempAuto()) {
        const char *prefix = (symbol->getDataType() == TR::Float || symbol->getDataType() == TR::Double) ? "#FP" : "#";
        log->printf(", %sSPILL%d", prefix, symbol->getSize());
    }

    log->prints(", SymRef");

    print(log, &mr->getSymbolReference());
}

TR_RegisterSizes TR_Debug::getTargetSizeFromInstruction(TR::Instruction *instr)
{
    if (instr->getOpCode().hasIntTarget() != 0)
        return TR_WordReg;
    else if (instr->getOpCode().hasShortTarget() != 0)
        return TR_HalfWordReg;
    else if (instr->getOpCode().hasByteTarget() != 0)
        return TR_ByteReg;
    else if ((instr->getOpCode().hasLongTarget() != 0) || (instr->getOpCode().doubleFPOp() != 0))
        return TR_DoubleWordReg;

    OMR::X86::Encoding encoding = instr->getEncodingMethod();

    if (encoding == OMR::X86::Default) {
        encoding = static_cast<OMR::X86::Encoding>(instr->getOpCode().info().vex_l);
    }

    if (encoding == OMR::X86::VEX_L128 || encoding == OMR::X86::EVEX_L128)
        return TR_VectorReg128;
    else if (encoding == OMR::X86::VEX_L256 || encoding == OMR::X86::EVEX_L256)
        return TR_VectorReg256;
    else if (encoding == OMR::X86::EVEX_L512)
        return TR_VectorReg512;
    else if (instr->getOpCode().hasXMMTarget())
        return TR_QuadWordReg;
    else if (instr->getOpCode().hasYMMTarget())
        return TR_VectorReg256;
    else if (instr->getOpCode().hasZMMTarget())
        return TR_VectorReg512;
    else
        return TR_WordReg;
}

TR_RegisterSizes TR_Debug::getSourceSizeFromInstruction(TR::Instruction *instr)
{
    if (instr->getOpCode().hasIntSource() != 0)
        return TR_WordReg;
    else if (instr->getOpCode().hasShortSource() != 0)
        return TR_HalfWordReg;
    else if ((&instr->getOpCode())->hasByteSource())
        return TR_ByteReg;
    else if ((instr->getOpCode().hasLongSource() != 0) || (instr->getOpCode().doubleFPOp() != 0))
        return TR_DoubleWordReg;

    OMR::X86::Encoding encoding = instr->getEncodingMethod();

    if (encoding == OMR::X86::Default) {
        encoding = static_cast<OMR::X86::Encoding>(instr->getOpCode().info().vex_l);
    }

    if (encoding == OMR::X86::VEX_L128 || encoding == OMR::X86::EVEX_L128)
        return TR_VectorReg128;
    else if (encoding == OMR::X86::VEX_L256 || encoding == OMR::X86::EVEX_L256)
        return TR_VectorReg256;
    else if (encoding == OMR::X86::EVEX_L512)
        return TR_VectorReg512;
    else if (instr->getOpCode().hasXMMSource())
        return TR_QuadWordReg;
    else if (instr->getOpCode().hasYMMSource())
        return TR_VectorReg256;
    else if (instr->getOpCode().hasZMMSource())
        return TR_VectorReg512;
    else
        return TR_WordReg;
}

TR_RegisterSizes TR_Debug::getImmediateSizeFromInstruction(TR::Instruction *instr)
{
    TR_RegisterSizes immedSize;

    if (instr->getOpCode().hasShortImmediate() != 0)
        immedSize = TR_HalfWordReg;
    else if (instr->getOpCode().hasByteImmediate() != 0)
        immedSize = TR_ByteReg;
    else if (instr->getOpCode().hasLongImmediate() != 0)
        immedSize = TR_DoubleWordReg;
    else
        // IntImmediate or SignExtendImmediate
        //
        immedSize = TR_WordReg;

    return immedSize;
}

void TR_Debug::printReferencedRegisterInfo(OMR::Logger *log, TR::MemoryReference *mr)
{
    if (mr->getBaseRegister()) {
        log->prints("    Base Reg          ");
        printFullRegInfo(log, mr->getBaseRegister());
    }

    if (mr->getIndexRegister()) {
        log->prints("    Index Reg         ");
        printFullRegInfo(log, mr->getIndexRegister());
    }

    log->flush();
}

void TR_Debug::printX86GCRegisterMap(OMR::Logger *log, TR::GCRegisterMap *map)
{
    TR::Machine *machine = _cg->machine();

    log->printf("    slot pushes: %d", ((map->getMap() & _cg->getRegisterMapInfoBitsMask()) >> 16));

    log->prints("    registers: {");
    for (int32_t i = machine->getFirstGPR(); i <= machine->getLastAssignableGPR(); ++i) {
        if (map->getMap() & (1 << (i - 1))) // TODO:AMD64: Use the proper mask value
            log->printf("%s ", getName(machine->getRealRegister((TR::RealRegister::RegNum)i)));
    }

    log->prints("}\n");
}

void TR_Debug::print(OMR::Logger *log, TR::RealRegister *reg, TR_RegisterSizes size)
{
    switch (size) {
        case TR_WordReg:
        case TR_FloatReg:
        case TR_DoubleReg:
            log->prints(getName(reg));
            break;
        case TR_VectorReg128:
        case TR_VectorReg256:
        case TR_VectorReg512:
        case TR_QuadWordReg:
        case TR_DoubleWordReg:
        case TR_HalfWordReg:
        case TR_ByteReg:
            log->prints(getName(reg, size));
            break;
        default:
            break;
    }
}

void TR_Debug::printFullRegInfo(OMR::Logger *log, TR::RealRegister *reg)
{
    log->prints("[ ");

    log->printf("%-12s ][ ", getName(reg));

    static const char *stateNames[5] = { "Free", "Unlatched", "Assigned", "Blocked", "Locked" };

    log->printf("%-10s ][ ", stateNames[reg->getState()]);

    log->printf("%-12s ]\n", reg->getAssignedRegister() ? getName(reg->getAssignedRegister()) : " ");

    log->flush();
}

static const char *unknownRegisterName(const char kind = 0)
{
    // This function allows us to put a breakpoint to trap unknown registers
    switch (kind) {
        case 'f':
            return "fp?";
        case 's':
            return "st(?)";
        case 'm':
            return "mm?";
        case 'r':
            return "r?";
        case 'x':
            return "xmm?";
        case 'v':
            return "vfp?";
        default:
            return "???";
    }
}

const char *TR_Debug::getName(uint32_t realRegisterIndex, TR_RegisterSizes size)
{
    switch (realRegisterIndex) {
        case TR::RealRegister::NoReg:
            return "noReg";
        case TR::RealRegister::ByteReg:
            return "byteReg";
        case TR::RealRegister::SpilledReg:
            return "spilledReg";
        case TR::RealRegister::eax:
            switch (size) {
                case 0:
                    return "al";
                case 1:
                    return "ax";
                case 2:
                case -1:
                    return "eax";
                case 3:
                    return "rax";
                default:
                    return "?a?";
            }
        case TR::RealRegister::ebx:
            switch (size) {
                case 0:
                    return "bl";
                case 1:
                    return "bx";
                case 2:
                case -1:
                    return "ebx";
                case 3:
                    return "rbx";
                default:
                    return "?b?";
            }
        case TR::RealRegister::ecx:
            switch (size) {
                case 0:
                    return "cl";
                case 1:
                    return "cx";
                case 2:
                case -1:
                    return "ecx";
                case 3:
                    return "rcx";
                default:
                    return "?c?";
            }
        case TR::RealRegister::edx:
            switch (size) {
                case 0:
                    return "dl";
                case 1:
                    return "dx";
                case 2:
                case -1:
                    return "edx";
                case 3:
                    return "rdx";
                default:
                    return "?d?";
            }
        case TR::RealRegister::edi:
            switch (size) {
                case 0:
                    return "dil";
                case 1:
                    return "di";
                case 2:
                case -1:
                    return "edi";
                case 3:
                    return "rdi";
                default:
                    return "?di?";
            }
        case TR::RealRegister::esi:
            switch (size) {
                case 0:
                    return "sil";
                case 1:
                    return "si";
                case 2:
                case -1:
                    return "esi";
                case 3:
                    return "rsi";
                default:
                    return "?si?";
            }
        case TR::RealRegister::ebp:
            switch (size) {
                case 0:
                    return "bpl";
                case 1:
                    return "bp";
                case 2:
                case -1:
                    return "ebp";
                case 3:
                    return "rbp";
                default:
                    return "?bp?";
            }
        case TR::RealRegister::esp:
            switch (size) {
                case 0:
                    return "spl";
                case 1:
                    return "sp";
                case 2:
                case -1:
                    return "esp";
                case 3:
                    return "rsp";
                default:
                    return "?sp?";
            }
#ifdef TR_TARGET_64BIT
        case TR::RealRegister::r8:
            switch (size) {
                case 0:
                    return "r8b";
                case 1:
                    return "r8w";
                case 2:
                    return "r8d";
                case 3:
                case -1:
                    return "r8";
                default:
                    return "r8?";
            }
        case TR::RealRegister::r9:
            switch (size) {
                case 0:
                    return "r9b";
                case 1:
                    return "r9w";
                case 2:
                    return "r9d";
                case 3:
                case -1:
                    return "r9";
                default:
                    return "r9?";
            }
        case TR::RealRegister::r10:
            switch (size) {
                case 0:
                    return "r10b";
                case 1:
                    return "r10w";
                case 2:
                    return "r10d";
                case 3:
                case -1:
                    return "r10";
                default:
                    return "r10?";
            }
        case TR::RealRegister::r11:
            switch (size) {
                case 0:
                    return "r11b";
                case 1:
                    return "r11w";
                case 2:
                    return "r11d";
                case 3:
                case -1:
                    return "r11";
                default:
                    return "r11?";
            }
        case TR::RealRegister::r12:
            switch (size) {
                case 0:
                    return "r12b";
                case 1:
                    return "r12w";
                case 2:
                    return "r12d";
                case 3:
                case -1:
                    return "r12";
                default:
                    return "r12?";
            }
        case TR::RealRegister::r13:
            switch (size) {
                case 0:
                    return "r13b";
                case 1:
                    return "r13w";
                case 2:
                    return "r13d";
                case 3:
                case -1:
                    return "r13";
                default:
                    return "r13?";
            }
        case TR::RealRegister::r14:
            switch (size) {
                case 0:
                    return "r14b";
                case 1:
                    return "r14w";
                case 2:
                    return "r14d";
                case 3:
                case -1:
                    return "r14";
                default:
                    return "r14?";
            }
        case TR::RealRegister::r15:
            switch (size) {
                case 0:
                    return "r15b";
                case 1:
                    return "r15w";
                case 2:
                    return "r15d";
                case 3:
                case -1:
                    return "r15";
                default:
                    return "r15?";
            }
        case TR::RealRegister::r16:
            switch (size) {
                case 0:
                    return "r16b";
                case 1:
                    return "r16w";
                case 2:
                    return "r16d";
                case 3:
                case -1:
                    return "r16";
                default:
                    return "r16?";
            }
        case TR::RealRegister::r17:
            switch (size) {
                case 0:
                    return "r17b";
                case 1:
                    return "r17w";
                case 2:
                    return "r17d";
                case 3:
                case -1:
                    return "r17";
                default:
                    return "r17?";
            }
        case TR::RealRegister::r18:
            switch (size) {
                case 0:
                    return "r18b";
                case 1:
                    return "r18w";
                case 2:
                    return "r18d";
                case 3:
                case -1:
                    return "r18";
                default:
                    return "r18?";
            }
        case TR::RealRegister::r19:
            switch (size) {
                case 0:
                    return "r19b";
                case 1:
                    return "r19w";
                case 2:
                    return "r19d";
                case 3:
                case -1:
                    return "r19";
                default:
                    return "r19?";
            }
        case TR::RealRegister::r20:
            switch (size) {
                case 0:
                    return "r20b";
                case 1:
                    return "r20w";
                case 2:
                    return "r20d";
                case 3:
                case -1:
                    return "r20";
                default:
                    return "r20?";
            }
        case TR::RealRegister::r21:
            switch (size) {
                case 0:
                    return "r21b";
                case 1:
                    return "r21w";
                case 2:
                    return "r21d";
                case 3:
                case -1:
                    return "r21";
                default:
                    return "r21?";
            }
        case TR::RealRegister::r22:
            switch (size) {
                case 0:
                    return "r22b";
                case 1:
                    return "r22w";
                case 2:
                    return "r22d";
                case 3:
                case -1:
                    return "r22";
                default:
                    return "r22?";
            }
        case TR::RealRegister::r23:
            switch (size) {
                case 0:
                    return "r23b";
                case 1:
                    return "r23w";
                case 2:
                    return "r23d";
                case 3:
                case -1:
                    return "r23";
                default:
                    return "r23?";
            }
        case TR::RealRegister::r24:
            switch (size) {
                case 0:
                    return "r24b";
                case 1:
                    return "r24w";
                case 2:
                    return "r24d";
                case 3:
                case -1:
                    return "r24";
                default:
                    return "r24?";
            }
        case TR::RealRegister::r25:
            switch (size) {
                case 0:
                    return "r25b";
                case 1:
                    return "r25w";
                case 2:
                    return "r25d";
                case 3:
                case -1:
                    return "r25";
                default:
                    return "r25?";
            }
        case TR::RealRegister::r26:
            switch (size) {
                case 0:
                    return "r26b";
                case 1:
                    return "r26w";
                case 2:
                    return "r26d";
                case 3:
                case -1:
                    return "r26";
                default:
                    return "r26?";
            }
        case TR::RealRegister::r27:
            switch (size) {
                case 0:
                    return "r27b";
                case 1:
                    return "r27w";
                case 2:
                    return "r27d";
                case 3:
                case -1:
                    return "r27";
                default:
                    return "r27?";
            }
        case TR::RealRegister::r28:
            switch (size) {
                case 0:
                    return "r28b";
                case 1:
                    return "r28w";
                case 2:
                    return "r28d";
                case 3:
                case -1:
                    return "r28";
                default:
                    return "r28?";
            }
        case TR::RealRegister::r29:
            switch (size) {
                case 0:
                    return "r29b";
                case 1:
                    return "r29w";
                case 2:
                    return "r29d";
                case 3:
                case -1:
                    return "r29";
                default:
                    return "r29?";
            }
        case TR::RealRegister::r30:
            switch (size) {
                case 0:
                    return "r30b";
                case 1:
                    return "r30w";
                case 2:
                    return "r30d";
                case 3:
                case -1:
                    return "r30";
                default:
                    return "r30?";
            }
        case TR::RealRegister::r31:
            switch (size) {
                case 0:
                    return "r31b";
                case 1:
                    return "r31w";
                case 2:
                    return "r31d";
                case 3:
                case -1:
                    return "r31";
                default:
                    return "r31?";
            }
#endif
        case TR::RealRegister::vfp:
            switch (size) {
                case 2:
                case 3:
                case -1:
                    return "vfp";
                default:
                    return unknownRegisterName('v');
            } // 3 is for AMD64
        case TR::RealRegister::st0Return:
            return "st0";
        case TR::RealRegister::xmm0:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm0";
                case TR_VectorReg256:
                    return "ymm0";
                case TR_VectorReg512:
                    return "zmm0";
                default:
                    return "?mm0";
            }
        case TR::RealRegister::xmm1:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm1";
                case TR_VectorReg256:
                    return "ymm1";
                case TR_VectorReg512:
                    return "zmm1";
                default:
                    return "?mm1";
            }
        case TR::RealRegister::xmm2:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm2";
                case TR_VectorReg256:
                    return "ymm2";
                case TR_VectorReg512:
                    return "zmm2";
                default:
                    return "?mm2";
            }
        case TR::RealRegister::xmm3:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm3";
                case TR_VectorReg256:
                    return "ymm3";
                case TR_VectorReg512:
                    return "zmm3";
                default:
                    return "?mm3";
            }
        case TR::RealRegister::xmm4:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm4";
                case TR_VectorReg256:
                    return "ymm4";
                case TR_VectorReg512:
                    return "zmm4";
                default:
                    return "?mm4";
            }
        case TR::RealRegister::xmm5:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm5";
                case TR_VectorReg256:
                    return "ymm5";
                case TR_VectorReg512:
                    return "zmm5";
                default:
                    return "?mm5";
            }
        case TR::RealRegister::xmm6:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm6";
                case TR_VectorReg256:
                    return "ymm6";
                case TR_VectorReg512:
                    return "zmm6";
                default:
                    return "?mm6";
            }
        case TR::RealRegister::xmm7:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm7";
                case TR_VectorReg256:
                    return "ymm7";
                case TR_VectorReg512:
                    return "zmm7";
                default:
                    return "?mm7";
            }
#ifdef TR_TARGET_64BIT
        case TR::RealRegister::xmm8:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm8";
                case TR_VectorReg256:
                    return "ymm8";
                case TR_VectorReg512:
                    return "zmm8";
                default:
                    return "?mm8";
            }
        case TR::RealRegister::xmm9:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm9";
                case TR_VectorReg256:
                    return "ymm9";
                case TR_VectorReg512:
                    return "zmm9";
                default:
                    return "?mm9";
            }
        case TR::RealRegister::xmm10:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm10";
                case TR_VectorReg256:
                    return "ymm10";
                case TR_VectorReg512:
                    return "zmm10";
                default:
                    return "?mm10";
            }
        case TR::RealRegister::xmm11:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm11";
                case TR_VectorReg256:
                    return "ymm11";
                case TR_VectorReg512:
                    return "zmm11";
                default:
                    return "?mm11";
            }
        case TR::RealRegister::xmm12:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm12";
                case TR_VectorReg256:
                    return "ymm12";
                case TR_VectorReg512:
                    return "zmm12";
                default:
                    return "?mm12";
            }
        case TR::RealRegister::xmm13:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm13";
                case TR_VectorReg256:
                    return "ymm13";
                case TR_VectorReg512:
                    return "zmm13";
                default:
                    return "?mm13";
            }
        case TR::RealRegister::xmm14:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm14";
                case TR_VectorReg256:
                    return "ymm14";
                case TR_VectorReg512:
                    return "zmm14";
                default:
                    return "?mm14";
            }
        case TR::RealRegister::xmm15:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm15";
                case TR_VectorReg256:
                    return "ymm15";
                case TR_VectorReg512:
                    return "zmm15";
                default:
                    return "?mm15";
            }
        case TR::RealRegister::xmm16:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm16";
                case TR_VectorReg256:
                    return "ymm16";
                case TR_VectorReg512:
                    return "zmm16";
                default:
                    return "?mm16";
            }
        case TR::RealRegister::xmm17:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm17";
                case TR_VectorReg256:
                    return "ymm17";
                case TR_VectorReg512:
                    return "zmm17";
                default:
                    return "?mm17";
            }
        case TR::RealRegister::xmm18:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm18";
                case TR_VectorReg256:
                    return "ymm18";
                case TR_VectorReg512:
                    return "zmm18";
                default:
                    return "?mm18";
            }
        case TR::RealRegister::xmm19:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm19";
                case TR_VectorReg256:
                    return "ymm19";
                case TR_VectorReg512:
                    return "zmm19";
                default:
                    return "?mm19";
            }
        case TR::RealRegister::xmm20:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm20";
                case TR_VectorReg256:
                    return "ymm20";
                case TR_VectorReg512:
                    return "zmm20";
                default:
                    return "?mm20";
            }
        case TR::RealRegister::xmm21:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm21";
                case TR_VectorReg256:
                    return "ymm21";
                case TR_VectorReg512:
                    return "zmm21";
                default:
                    return "?mm21";
            }
        case TR::RealRegister::xmm22:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm22";
                case TR_VectorReg256:
                    return "ymm22";
                case TR_VectorReg512:
                    return "zmm22";
                default:
                    return "?mm22";
            }
        case TR::RealRegister::xmm23:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm23";
                case TR_VectorReg256:
                    return "ymm23";
                case TR_VectorReg512:
                    return "zmm23";
                default:
                    return "?mm23";
            }
        case TR::RealRegister::xmm24:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm24";
                case TR_VectorReg256:
                    return "ymm24";
                case TR_VectorReg512:
                    return "zmm24";
                default:
                    return "?mm24";
            }
        case TR::RealRegister::xmm25:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm25";
                case TR_VectorReg256:
                    return "ymm25";
                case TR_VectorReg512:
                    return "zmm25";
                default:
                    return "?mm25";
            }
        case TR::RealRegister::xmm26:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm26";
                case TR_VectorReg256:
                    return "ymm26";
                case TR_VectorReg512:
                    return "zmm26";
                default:
                    return "?mm26";
            }
        case TR::RealRegister::xmm27:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm27";
                case TR_VectorReg256:
                    return "ymm27";
                case TR_VectorReg512:
                    return "zmm27";
                default:
                    return "?mm27";
            }
        case TR::RealRegister::xmm28:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm28";
                case TR_VectorReg256:
                    return "ymm28";
                case TR_VectorReg512:
                    return "zmm28";
                default:
                    return "?mm28";
            }
        case TR::RealRegister::xmm29:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm29";
                case TR_VectorReg256:
                    return "ymm29";
                case TR_VectorReg512:
                    return "zmm29";
                default:
                    return "?mm29";
            }
        case TR::RealRegister::xmm30:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm30";
                case TR_VectorReg256:
                    return "ymm30";
                case TR_VectorReg512:
                    return "zmm30";
                default:
                    return "?mm30";
            }
        case TR::RealRegister::xmm31:
            switch (size) {
                case TR_QuadWordReg:
                case TR_FloatReg:
                case TR_DoubleReg:
                case TR_VectorReg128:
                case -1:
                    return "xmm31";
                case TR_VectorReg256:
                    return "ymm31";
                case TR_VectorReg512:
                    return "zmm31";
                default:
                    return "?mm31";
            }
#endif
        case TR::RealRegister::k0:
            return "k0";
        case TR::RealRegister::k1:
            return "k1";
        case TR::RealRegister::k2:
            return "k2";
        case TR::RealRegister::k3:
            return "k3";
        case TR::RealRegister::k4:
            return "k4";
        case TR::RealRegister::k5:
            return "k5";
        case TR::RealRegister::k6:
            return "k6";
        case TR::RealRegister::k7:
            return "k7";
        default:
            TR_ASSERT(0, "unexpected register number");
            return unknownRegisterName();
    }
}

const char *TR_Debug::getName(TR::RealRegister *reg, TR_RegisterSizes size)
{
    if ((reg->getKind() == TR_FPR && size != TR_VectorReg256 && size != TR_VectorReg512) || reg->getKind() == TR_VRF)
        size = TR_QuadWordReg;

    return getName(reg->getRegisterNumber(), size);
}

void TR_Debug::dumpInstructionWithVFPState(OMR::Logger *log, TR::Instruction *instr, const TR_VFPState *prevState)
{
    const TR_VFPState &vfpState = _cg->vfpState();
    print(log, instr);

    // Print the VFP state if something changed
    //
    // Note: IA32.cpp version 1.158 has code to compare the VFP state with
    // that calculated using the old VFP scheme to make sure it's correct.
    // TODO: Remove this note when we trust the new VFP scheme.
    //
    if (prevState && (vfpState != *prevState)) {
        log->printf("\n\t%s VFP=%s+%d", commentString(), getName(vfpState._register), vfpState._displacement);
    }
    log->flush();
}

//
// IA32 Instructions
//

void TR_Debug::printRegRegInstruction(OMR::Logger *log, const char *opCode, TR::RealRegister *reg1,
    TR::RealRegister *reg2)
{
    log->printf("%s\t", opCode);
    print(log, reg1);
    if (reg2) {
        log->prints(", ");
        print(log, reg2);
    }
}

void TR_Debug::printRegMemInstruction(OMR::Logger *log, const char *opCode, TR::RealRegister *reg,
    TR::RealRegister *base, int32_t offset)
{
    log->printf("%s\t", opCode);
    print(log, reg);
    if (base) {
        log->prints(", [");
        print(log, base);
        log->printf(" +%d]", offset);
    }
}

void TR_Debug::printRegImmInstruction(OMR::Logger *log, const char *opCode, TR::RealRegister *reg, int32_t imm)
{
    log->printf("%s\t", opCode);
    print(log, reg);
    if (imm <= 1024)
        log->printf(", %d", imm);
    else
        log->printf(", " POINTER_PRINTF_FORMAT, imm);
}

void TR_Debug::printMemRegInstruction(OMR::Logger *log, const char *opCode, TR::RealRegister *base, int32_t offset,
    TR::RealRegister *reg)
{
    log->printf("%s\t", opCode);
    log->printc('[');
    print(log, base);
    log->printf(" +%d]", offset);
    if (reg) {
        log->prints(", ");
        print(log, reg);
    }
}

void TR_Debug::printMemImmInstruction(OMR::Logger *log, const char *opCode, TR::RealRegister *base, int32_t offset,
    int32_t imm)
{
    log->printf("%s\t", opCode);
    log->printc('[');
    print(log, base);
    log->printf(" +%d]", offset);
    if (imm <= 1024)
        log->printf(", %d", imm);
    else
        log->printf(", " POINTER_PRINTF_FORMAT, imm);
}

void TR_Debug::printLabelInstruction(OMR::Logger *log, const char *opCode, TR::LabelSymbol *label)
{
    log->printf("%s\t", opCode);
    print(log, label);
}

//
// IA32 Snippets
//

#ifdef J9_PROJECT_SPECIFIC
void TR_Debug::print(OMR::Logger *log, TR::X86CallSnippet *snippet)
{
    uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
    TR::Node *callNode = snippet->getNode();
    TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
    TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

    printSnippetLabel(log, snippet->getSnippetLabel(), bufferPos, getName(snippet));

    bool isJitInduceOSRCall = false;
    bool isJitDispatchJ9Method = false;
    if (methodSymbol->isHelper() && methodSymRef->isOSRInductionHelper())
        isJitInduceOSRCall = true;
    else if (callNode->isJitDispatchJ9MethodCall(_comp))
        isJitDispatchJ9Method = true;

    bool hasMovRegJ9Method = !isJitInduceOSRCall && !isJitDispatchJ9Method;

    const char *helperName = NULL;
    if (isJitInduceOSRCall)
        helperName = getRuntimeHelperName(methodSymRef->getReferenceNumber());
    else if (isJitDispatchJ9Method)
        helperName = "j2iTransition";
    else
        helperName = "interpreterStaticAndSpecialGlue";

    if (_comp->target().is64Bit()) {
        static const int slotSize = sizeof(uintptr_t);
        static const int numArgGPRs = 4;
        static const int numArgFPRs = 8;
        static const char *gprStem[numArgGPRs] = { "ax", "si", "dx", "cx" };

        int32_t numChildren = callNode->getNumChildren();
        int32_t firstArgIndex = 0;
        if (isJitDispatchJ9Method)
            firstArgIndex++; // skip the J9Method

        int32_t offset = slotSize; // return address slot
        for (int32_t i = firstArgIndex; i < numChildren; i++) {
            TR::DataType type = callNode->getChild(i)->getType();
            offset += ((type == TR::Int64 || type == TR::Double) ? 2 : 1) * slotSize;
        }

        int32_t gprIndex = 0;
        int32_t fprIndex = 0;
        for (int32_t i = firstArgIndex; i < numChildren; i++) {
            if (gprIndex >= numArgGPRs && fprIndex >= numArgFPRs)
                break;

            TR::Node *child = callNode->getChild(i);
            TR::DataType type = child->getType();
            offset -= ((type == TR::Int64 || type == TR::Double) ? 2 : 1) * slotSize;

            bool is32Bit = TR::DataType::getSize(type) <= 4;
            bool isFpr = type.isFloatingPoint();
            if (isFpr && fprIndex < numArgFPRs) {
                // No need to check for VEX vs. legacy encoding. These instructions
                // are the same length either way.
                int32_t instrSize = 6 + (offset >= 0x80 ? 3 : 0);
                printPrefix(log, NULL, bufferPos, instrSize);
                log->printf("vmovs%c\t%cword ptr [rsp+0x%x], xmm%d"
                            "\t\t# save registers for interpreter call snippet",
                    is32Bit ? 's' : 'd', is32Bit ? 'd' : 'q', offset, fprIndex);

                bufferPos += instrSize;
                fprIndex++;
            } else if (!isFpr && gprIndex < numArgGPRs) {
                int32_t instrSize = (is32Bit ? 4 : 5) + (offset >= 0x80 ? 3 : 0);
                printPrefix(log, NULL, bufferPos, instrSize);
                log->printf("mov\t%cword ptr [rsp+0x%x], %c%s"
                            "\t\t# save registers for interpreter call snippet",
                    is32Bit ? 'd' : 'q', offset, is32Bit ? 'e' : 'r', gprStem[gprIndex]);

                bufferPos += instrSize;
                gprIndex++;
            }
        }

        if (hasMovRegJ9Method) {
            intptr_t ramMethod = (intptr_t)methodSymbol->getMethodAddress();
            printPrefix(log, NULL, bufferPos, 10);
            log->printf("mov\trdi, 0x%zx\t\t# MOV8RegImm64", ramMethod);
            bufferPos += 10;
        }

        printPrefix(log, NULL, bufferPos, 5);
        log->printf("jmp\t%s\t\t# jump out of snippet code", helperName);
        bufferPos += 5;
    } else {
        if (hasMovRegJ9Method) {
            intptr_t ramMethod = (intptr_t)methodSymbol->getMethodAddress();
            printPrefix(log, NULL, bufferPos, 5);
            log->printf("mov\tedi, 0x%x\t\t# MOV8RegImm32", ramMethod);
            bufferPos += 5;
        }

        printPrefix(log, NULL, bufferPos, 5);
        log->printf("jmp\t%s\t\t# jump out of snippet code", helperName);
        bufferPos += 5;
    }
}

#endif

void TR_Debug::printX86OOLSequences(OMR::Logger *log)
{
    auto oiIterator = _cg->getOutlinedInstructionsList().begin();
    while (oiIterator != _cg->getOutlinedInstructionsList().end()) {
        log->prints("\n------------ start out-of-line instructions\n");

        TR::Instruction *instr = (*oiIterator)->getFirstInstruction();

        do {
            print(log, instr);
            instr = instr->getNext();
        } while (instr != (*oiIterator)->getAppendInstruction());

        if ((*oiIterator)->getAppendInstruction()) {
            print(log, (*oiIterator)->getAppendInstruction());
        }
        log->prints("\n------------ end out-of-line instructions\n");

        ++oiIterator;
    }
}

//
// AMD64 Snippets
//

#ifdef TR_TARGET_64BIT
uint8_t *TR_Debug::printArgumentFlush(OMR::Logger *log, TR::Node *callNode,
    bool isFlushToStack, // flush to stack or flush to regs
    uint8_t *bufferPos)
{
    TR::MethodSymbol *methodSymbol = callNode->getSymbol()->castToMethodSymbol();
    TR::Linkage *linkage = _cg->getLinkage();

    const TR::X86LinkageProperties &linkageProperties = linkage->getProperties();

    int32_t numGPArgs = 0;
    int32_t numFPArgs = 0;

    for (int i = callNode->getFirstArgumentIndex(); i < callNode->getNumChildren(); i++) {
        TR::Node *child = callNode->getChild(i);
        int8_t modrmOffset = 0;
        const char *opCodeName = NULL, *regName = NULL;

        // Compute differences based on datatype
        //
        switch (child->getDataType()) {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
                if (numGPArgs < linkageProperties.getNumIntegerArgumentRegisters()) {
                    opCodeName = "mov";
                    modrmOffset = 1;
                    TR::RealRegister::RegNum reg = linkageProperties.getIntegerArgumentRegister(numGPArgs);
                    regName = getName(_cg->machine()->getRealRegister(reg));
                }
                numGPArgs++;
                break;
            case TR::Address:
            case TR::Int64:
                if (numGPArgs < linkageProperties.getNumIntegerArgumentRegisters()) {
                    opCodeName = "mov";
                    modrmOffset = 2;
                    TR::RealRegister::RegNum reg = linkageProperties.getIntegerArgumentRegister(numGPArgs);
                    regName = getName(_cg->machine()->getRealRegister(reg), TR_DoubleWordReg);
                }
                numGPArgs++;
                break;
            case TR::Float:
                if (numFPArgs < linkageProperties.getNumFloatArgumentRegisters()) {
                    opCodeName = "movss";
                    modrmOffset = 3;
                    TR::RealRegister::RegNum reg = linkageProperties.getFloatArgumentRegister(numFPArgs);
                    regName = getName(_cg->machine()->getRealRegister(reg), TR_QuadWordReg);
                }
                numFPArgs++;
                break;
            case TR::Double:
                if (numFPArgs < linkageProperties.getNumFloatArgumentRegisters()) {
                    opCodeName = "movsd";
                    modrmOffset = 3;
                    TR::RealRegister::RegNum reg = linkageProperties.getFloatArgumentRegister(numFPArgs);
                    regName = getName(_cg->machine()->getRealRegister(reg), TR_QuadWordReg);
                }
                numFPArgs++;
                break;
            default:
                break;
        }

        if (opCodeName) {
            // Compute differences based on addressing mode
            //
            uint32_t instrLength, displacement;
            switch (bufferPos[modrmOffset] & 0xc0) {
                case 0x40:
                    displacement = *(uint8_t *)(bufferPos + modrmOffset + 2);
                    instrLength = modrmOffset + 3;
                    break;
                default:
                    TR_ASSERT(0, "Unexpected Mod/RM value 0x%02x", bufferPos[modrmOffset]);
                    // fall through
                case 0x80:
                    displacement = *(uint32_t *)(bufferPos + modrmOffset + 2);
                    instrLength = modrmOffset + 6;
                    break;
            }

            // Print the instruction
            //
            printPrefix(log, NULL, bufferPos, instrLength);
            if (isFlushToStack)
                log->printf("%s\t[rsp +%d], %s", opCodeName, displacement, regName);
            else // Flush from stack to argument registers.
                log->printf("%s\t%s, [rsp +%d]", opCodeName, regName, displacement);
            bufferPos += instrLength;
        } else {
            // This argument is not in a register; skip it
        }
    }
    return bufferPos;
}

#endif

static const char *opCodeToNameMap[] = {
#define INSTRUCTION(name, mnemonic, binary, ...) #name
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
};

static const char *opCodeToMnemonicMap[] = {
#define INSTRUCTION(name, mnemonic, binary, ...) #mnemonic
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
};

const char *TR_Debug::getOpCodeName(TR::InstOpCode *opCode) { return opCodeToNameMap[opCode->getOpCodeValue()]; }

const char *TR_Debug::getMnemonicName(TR::InstOpCode *opCode)
{
    if (_comp->target().isLinux()) {
        int32_t o = opCode->getOpCodeValue();
        if (o == (int32_t)OP::DQImm64)
            return dqString();
        if (o == (int32_t)OP::DDImm4)
            return ddString();
        if (o == (int32_t)OP::DWImm2)
            return dwString();
        if (o == (int32_t)OP::DBImm1)
            return dbString();
    }
    return opCodeToMnemonicMap[opCode->getOpCodeValue()];
}
