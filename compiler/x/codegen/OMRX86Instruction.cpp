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

#include "codegen/X86Instruction.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "x/codegen/GuardedDevirtualSnippet.hpp"
#endif
#include "x/codegen/OutlinedInstructions.hpp"
#include "codegen/InstOpCode.hpp"
#include "OMRX86Instruction.hpp"

class TR_VirtualGuardSite;

// Hack markers
//
// TODO: Implement these more systematically
//
#define BACKWARD_BRANCH_BOUNDARY_SPACING (16) // K8 opt guide section 6.1; no indication that this helps on Intel
#define BACKWARD_BRANCH_MAX_PADDING (5)

// Assign an 8-bit register to a virtual register.
//
TR::RealRegister *assign8BitGPRegister(TR::Instruction *instr, TR::Register *virtReg, TR::CodeGenerator *cg)
{
    TR::Machine *machine = cg->machine();
    TR::RealRegister *assignedRegister = virtReg->getAssignedRealRegister();
    TR::RealRegister *candidate = toRealRegister(assignedRegister);

    cg->clearRegisterAssignmentFlags();
    cg->setRegisterAssignmentFlag(TR_NormalAssignment);

    if (candidate->getRegisterNumber() > machine->getLast8BitGPR()) {
        if ((candidate = machine->findBestFreeGPRegister(instr, virtReg, TR_ByteReg)) == NULL) {
            cg->setRegisterAssignmentFlag(TR_RegisterSpilled);
            candidate = machine->freeBestGPRegister(instr, virtReg, TR_ByteReg);
        }

        machine->coerceGPRegisterAssignment(instr, virtReg, candidate->getRegisterNumber());
        assignedRegister = candidate;
    }

    virtReg->setAssignedAsByteRegister(true);
    return assignedRegister;
}

// Assign any GP (including XMM/YMM/ZMM) register to a virtual register.
//
TR::RealRegister *assignGPRegister(TR::Instruction *instr, TR::Register *virtReg, TR_RegisterSizes requestedRegSize,
    TR::CodeGenerator *cg)
{
    TR::Machine *machine = cg->machine();
    TR::RealRegister *assignedRegister;

    cg->clearRegisterAssignmentFlags();
    cg->setRegisterAssignmentFlag(TR_NormalAssignment);

    // Depending on direction of assignment, must get register back from
    // spilled state
    //
    if (virtReg->getTotalUseCount() != virtReg->getFutureUseCount()) {
        cg->setRegisterAssignmentFlag(TR_RegisterReloaded);
        assignedRegister = machine->reverseGPRSpillState(instr, virtReg, NULL, requestedRegSize);
    }

    // If first use, totaluse and futureuse will be the same
    //
    else {
        if ((assignedRegister = machine->findBestFreeGPRegister(instr, virtReg, requestedRegSize, true))) {
            if (cg->enableBetterSpillPlacements())
                cg->removeBetterSpillPlacementCandidate(assignedRegister);
        } else {
            cg->setRegisterAssignmentFlag(TR_RegisterSpilled);
            assignedRegister = machine->freeBestGPRegister(instr, virtReg, requestedRegSize);
        }
    }
    virtReg->setAssignedRegister(assignedRegister);
    virtReg->setAssignedAsByteRegister(requestedRegSize == TR_ByteReg);
    assignedRegister->setAssignedRegister(virtReg);
    assignedRegister->setState(TR::RealRegister::Assigned, virtReg->isPlaceholderReg());
    cg->traceRegAssigned(virtReg, assignedRegister);

    return assignedRegister;
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86LabelInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
void TR::X86LabelInstruction::initialize(TR::LabelSymbol *sym)
{
    _symbol = sym;
    _outlinedInstructionBranch = NULL;
    _reloType = TR_NoRelocation;
    _permitShortening = true;
    if (sym && getOpCodeValue() == OP::label)
        sym->setInstruction(this);
    else if (sym)
        sym->setDirectlyTargeted();
}

TR::X86LabelInstruction::X86LabelInstruction(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym,
    TR::CodeGenerator *cg)
    : TR::Instruction(node, op, cg)
{
    initialize(sym);
}

TR::X86LabelInstruction::X86LabelInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::LabelSymbol *sym, TR::CodeGenerator *cg)
    : TR::Instruction(op, precedingInstruction, cg)
{
    initialize(sym);
}

TR::X86LabelInstruction::X86LabelInstruction(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::Instruction(cond, node, op, cg)
{
    initialize(sym);
}

TR::X86LabelInstruction::X86LabelInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::Instruction(cond, op, precedingInstruction, cg)
{
    initialize(sym);
}

TR::X86LabelInstruction *TR::X86LabelInstruction::getX86LabelInstruction() { return this; }

TR::Snippet *TR::X86LabelInstruction::getSnippetForGC()
{
    return getLabelSymbol() ? getLabelSymbol()->getSnippet() : NULL;
}

void TR::X86LabelInstruction::assignOutlinedInstructions(TR_RegisterKinds kindsToBeAssigned,
    TR::X86LabelInstruction *labelInstruction)
{
    // Switch to the outlined instruction stream and assign registers.
    //
    TR_OutlinedInstructions *oi = cg()->findOutlinedInstructionsFromLabel(labelInstruction->getLabelSymbol());
    TR_ASSERT(oi, "Could not find OutlinedInstructions stream from label.  instr=%p, label=%p\n", labelInstruction,
        labelInstruction->getLabelSymbol());

    if (!oi->hasBeenRegisterAssigned())
        oi->assignRegisters(kindsToBeAssigned, Inst_VFPSave(getPrev(), cg()));
}

// Take the now-assigned register dependencies from this instruction and replicate
// them on the outlined instruction branch instruction.
//
void TR::X86LabelInstruction::addPostDepsToOutlinedInstructionsBranch()
{
    TR::RegisterDependencyConditions *mergeDeps = getDependencyConditions()->clone(cg());

    TR_ASSERT(!_outlinedInstructionBranch->getDependencyConditions(),
        "not expecting existing reg deps on an OOL branch");

    _outlinedInstructionBranch->setDependencyConditions(mergeDeps);

    TR::RegisterDependencyGroup *depGroup = mergeDeps->getPostConditions();
    for (auto i = 0U; i < mergeDeps->getNumPostConditions(); i++) {
        // Bump the use count on all cloned dependencies.
        //
        TR::RegisterDependency *dependency = depGroup->getRegisterDependency(i);
        TR::Register *virtReg = dependency->getRegister();
        virtReg->incTotalUseCount();
        virtReg->incFutureUseCount();

#ifdef DEBUG
        // Ensure all register dependencies have been assigned.
        //
        TR_ASSERT(!dependency->isNoReg(), "unassigned merge dep register");
#endif
    }
}

void TR::X86LabelInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    TR::Compilation *comp = cg()->comp();

    if (kindsToBeAssigned & TR_GPR_Mask) {
        if (getDependencyConditions()) {
            // ----------------------
            // Assign post conditions
            // ----------------------
            //
            getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());

            if (getOpCodeValue() == OP::label && getLabelSymbol()->isInternalControlFlowMerge()) {
                // This is the merge label of a non-linear internal control flow region.
                // Record the register assigner state before continuing on the main path.
                //
                cg()->prepareForNonLinearRegisterAssignmentAtMerge(this);
            } else if (getOpCodeValue() != OP::label && getLabelSymbol()->isStartOfColdInstructionStream()) {
                if (getLabelSymbol()->isNonLinear() && cg()->getUseNonLinearRegisterAssigner()) {
                    // This is a branch to an outlined instruction sequence.  Now perform
                    // register assignment over it.
                    //
                    cg()->performNonLinearRegisterAssignmentAtBranch(this, kindsToBeAssigned);
                } else {
                    // Classic out-of-line instruction register assignment
                    //
                    assignOutlinedInstructions(kindsToBeAssigned, this);
                }
            } else if (_outlinedInstructionBranch) {
                // OBSOLETE???
                addPostDepsToOutlinedInstructionsBranch();
                assignOutlinedInstructions(kindsToBeAssigned, _outlinedInstructionBranch);
            }

            // ---------------------
            // Assign pre conditions
            // ---------------------
            //
            getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        } else if (getOpCodeValue() != OP::label && getLabelSymbol()->isStartOfColdInstructionStream()) {
            if (getLabelSymbol()->isNonLinear() && cg()->getUseNonLinearRegisterAssigner()) {
                // This is a branch to an outlined instruction sequence.  Now perform
                // register assignment over it.
                //
                cg()->performNonLinearRegisterAssignmentAtBranch(this, kindsToBeAssigned);
            } else {
                // Classic out-of-line instruction register assignment
                //
                assignOutlinedInstructions(kindsToBeAssigned, this);
            }
        } else if (getOpCodeValue() == OP::label && getLabelSymbol()->isInternalControlFlowMerge()) {
            // This is the merge label of a non-linear internal control flow region.
            // Record the register assigner state before continuing on the main path.
            //
            cg()->prepareForNonLinearRegisterAssignmentAtMerge(this);
        } else if (getLabelSymbol()) {
#ifdef J9_PROJECT_SPECIFIC
            TR::Snippet *snippet = getLabelSymbol()->getSnippet();
            TR::X86GuardedDevirtualSnippet *devirtSnippet;

            // If this is a GuardedDevirtualSnippet, then must find out what real register
            // contains the class object reference.  This must be used by the snippet to generate
            // the full virtual call sequence.
            //
            if (snippet && (devirtSnippet = snippet->getGuardedDevirtualSnippet())) {
                TR::Register *classObjectRegister = devirtSnippet->getClassObjectRegister();
                if (classObjectRegister != NULL && !classObjectRegister->getRealRegister()) {
                    TR::RealRegister *assignedClassObjectRegister
                        = toRealRegister(classObjectRegister->getAssignedRealRegister());
                    if (assignedClassObjectRegister == NULL
                        && classObjectRegister->getTotalUseCount() == classObjectRegister->getFutureUseCount()) {
                        TR::Machine *machine = cg()->machine();

                        cg()->clearRegisterAssignmentFlags();
                        cg()->setRegisterAssignmentFlag(TR_NormalAssignment);

                        if ((assignedClassObjectRegister
                                = machine->findBestFreeGPRegister(this, classObjectRegister, TR_WordReg))
                            != NULL) {
                            machine->coerceGPRegisterAssignment(this, classObjectRegister,
                                assignedClassObjectRegister->getRegisterNumber());

                            // No use of this register here in the original stream.  So, need to
                            // bump its total use count without bumping its future use count
                            // so that it doesn't look like the first time a register is seen.  That is
                            // make it fail the getTotalUseCount() == getFutureUseCount() test used
                            // to detect the first reference to a register in the assignment direction
                            //
                            classObjectRegister->incTotalUseCount();
                        }
                    }
                    if (assignedClassObjectRegister) {
                        devirtSnippet->setClassObjectRegister(assignedClassObjectRegister);
                    }
                }
            }
#endif
        }

        // Save better spill placement information at conditional branch
        // instructions, and enable or disable better spill placement when
        // entering or leaving a region of internal control flow.
        //
        if (getOpCode().isConditionalBranchOp() && cg()->internalControlFlowNestingDepth() == 0) {
            if (cg()->enableBetterSpillPlacements())
                cg()->saveBetterSpillPlacements(this);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86FenceInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86FenceInstruction::X86FenceInstruction(OP::Mnemonic op, TR::Node *node, TR::Node *fenceNode,
    TR::CodeGenerator *cg)
    : TR::Instruction(node, op, cg)
    , _fenceNode(fenceNode)
{}

TR::X86FenceInstruction::X86FenceInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::Node *node,
    TR::CodeGenerator *cg)
    : TR::Instruction(op, precedingInstruction, cg)
    , _fenceNode(node)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86ImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86ImmInstruction::X86ImmInstruction(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::CodeGenerator *cg,
    int32_t reloKind)
    : TR::Instruction(node, op, cg)
    , _sourceImmediate(imm)
    , _adjustsFramePointerBy(0)
    , _reloKind(reloKind)
{}

TR::X86ImmInstruction::X86ImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
    : TR::Instruction(op, precedingInstruction, cg)
    , _sourceImmediate(imm)
    , _adjustsFramePointerBy(0)
    , _reloKind(reloKind)
{}

TR::X86ImmInstruction::X86ImmInstruction(OP::Mnemonic op, TR::Node *node, int32_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::Instruction(cond, node, op, cg)
    , _sourceImmediate(imm)
    , _adjustsFramePointerBy(0)
    , _reloKind(reloKind)
{}

TR::X86ImmInstruction::X86ImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, int32_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::Instruction(cond, op, precedingInstruction, cg)
    , _sourceImmediate(imm)
    , _adjustsFramePointerBy(0)
    , _reloKind(reloKind)
{
    if (cond && cg->enableRegisterAssociations())
        cond->createRegisterAssociationDirective(this, cg);
}

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
// The following safe virtual downcast method is only used in an assertion
// check within "toIA32ImmInstruction"
//
TR::X86ImmInstruction *TR::X86ImmInstruction::getX86ImmInstruction() { return this; }
#endif

////////////////////////////////////////////////////////////////////////////////
// TR::X86ImmSnippetInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86ImmSnippetInstruction::X86ImmSnippetInstruction(OP::Mnemonic op, TR::Node *node, int32_t imm,
    TR::UnresolvedDataSnippet *us, TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(imm, node, op, cg)
    , _unresolvedSnippet(us)
{}

TR::X86ImmSnippetInstruction::X86ImmSnippetInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    int32_t imm, TR::UnresolvedDataSnippet *us, TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(imm, op, precedingInstruction, cg)
    , _unresolvedSnippet(us)
{}

TR::Snippet *TR::X86ImmSnippetInstruction::getSnippetForGC() { return getUnresolvedSnippet(); }

////////////////////////////////////////////////////////////////////////////////
// TR::X86ImmSymInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86ImmSymInstruction::X86ImmSymInstruction(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::SymbolReference *sr,
    TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(imm, node, op, cg)
    , _symbolReference(sr)
{}

TR::X86ImmSymInstruction::X86ImmSymInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, int32_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(imm, op, precedingInstruction, cg)
    , _symbolReference(sr)
{}

TR::X86ImmSymInstruction::X86ImmSymInstruction(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::SymbolReference *sr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(cond, imm, node, op, cg)
    , _symbolReference(sr)
{}

TR::X86ImmSymInstruction::X86ImmSymInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, int32_t imm,
    TR::SymbolReference *sr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::X86ImmInstruction(cond, imm, op, precedingInstruction, cg)
    , _symbolReference(sr)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegInstruction::X86RegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *reg, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::Instruction(node, op, cg, encoding)
    , _targetRegister(reg)
{
    useRegister(reg);
    getOpCode().trackUpperBitsOnReg(reg, cg);

    // Check the live discardable register list to see if this is the first
    // instruction that kills the rematerialisable range of a register.
    //
    if (cg->enableRematerialisation() && reg->isDiscardable() && getOpCode().modifiesTarget()) {
        TR::ClobberingInstruction *clob = new (cg->trHeapMemory()) TR::ClobberingInstruction(this, cg->trMemory());
        clob->addClobberedRegister(reg);
        cg->addClobberingInstruction(clob);
        cg->removeLiveDiscardableRegister(reg);
        cg->clobberLiveDependentDiscardableRegisters(clob, reg);

        if (debug("dumpRemat")) {
            diagnostic("---> Clobbering %s discardable register %s at instruction %p\n",
                cg->getDebug()->toString(reg->getRematerializationInfo()), cg->getDebug()->getName(reg), this);
        }
    }
}

TR::X86RegInstruction::X86RegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::Register *reg,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::Instruction(op, precedingInstruction, cg, encoding)
    , _targetRegister(reg)
{
    useRegister(reg);
    getOpCode().trackUpperBitsOnReg(reg, cg);
}

TR::X86RegInstruction::X86RegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *reg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::Instruction(cond, node, op, cg, encoding)
    , _targetRegister(reg)
{
    useRegister(reg);
    getOpCode().trackUpperBitsOnReg(reg, cg);

    // Check the live discardable register list to see if this is the first
    // instruction that kills the rematerialisable range of a register.
    //
    if (cg->enableRematerialisation() && reg->isDiscardable() && getOpCode().modifiesTarget()) {
        TR::ClobberingInstruction *clob = new (cg->trHeapMemory()) TR::ClobberingInstruction(this, cg->trMemory());
        clob->addClobberedRegister(reg);
        cg->addClobberingInstruction(clob);
        cg->removeLiveDiscardableRegister(reg);
        cg->clobberLiveDependentDiscardableRegisters(clob, reg);

        if (debug("dumpRemat")) {
            diagnostic("---> Clobbering %s discardable register %s at instruction %p\n",
                cg->getDebug()->toString(reg->getRematerializationInfo()), cg->getDebug()->getName(reg), this);
        }
    }
}

TR::X86RegInstruction::X86RegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::Register *reg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::Instruction(cond, op, precedingInstruction, cg, encoding)
    , _targetRegister(reg)
{
    useRegister(reg);
    getOpCode().trackUpperBitsOnReg(reg, cg);
}

TR::X86RegInstruction *TR::X86RegInstruction::getX86RegInstruction() { return this; }

bool TR::X86RegInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegInstruction::defsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() && getOpCode().modifiesTarget()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86RegInstruction::usesRegister(TR::Register *reg)
{
    if (reg == getTargetRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();

        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());

        getTargetRegister()->unblock();
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR::RealRegister *assignedRegister = getTargetRegister()->getAssignedRealRegister();
        TR_RegisterSizes requestedRegSize = getOpCode().hasByteTarget() ? TR_ByteReg : TR_WordReg;

        if (assignedRegister == NULL) {
            assignedRegister = assignGPRegister(this, getTargetRegister(), requestedRegSize, cg());
        } else if (requestedRegSize == TR_ByteReg) {
            assignedRegister = assign8BitGPRegister(this, getTargetRegister(), cg());
        }

        if (getTargetRegister()->decFutureUseCount() == 0 && assignedRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(getTargetRegister(), assignedRegister);
            getTargetRegister()->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
        }
        setTargetRegister(assignedRegister);
    }

    if (getDependencyConditions()) {
        getTargetRegister()->block();

        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());

        getTargetRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegRegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegRegInstruction::X86RegRegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::Register *sreg,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(treg, node, op, cg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86RegRegInstruction::X86RegRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::Register *sreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(treg, op, precedingInstruction, cg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86RegRegInstruction::X86RegRegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::Register *sreg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(cond, treg, node, op, cg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86RegRegInstruction::X86RegRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::Register *sreg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(cond, treg, op, precedingInstruction, cg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

bool TR::X86RegRegInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegRegInstruction::defsRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().modifiesTarget())
        || (reg == getSourceRegister() && getOpCode().modifiesSource())) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86RegRegInstruction::usesRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().usesTarget()) || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR::Register *firstRegister = getTargetRegister();
        TR::Register *secondRegister = getSourceRegister();

        TR_RegisterSizes firstRequestedRegSize = getOpCode().hasByteTarget() ? TR_ByteReg
            : getOpCode().hasXMMTarget()                                     ? TR_QuadWordReg
            : getOpCode().hasYMMTarget()                                     ? TR_VectorReg256
            : getOpCode().hasZMMTarget()                                     ? TR_VectorReg512
                                                                             : TR_WordReg;

        TR_RegisterSizes secondRequestedRegSize = getOpCode().hasByteSource() ? TR_ByteReg
            : getOpCode().hasXMMSource()                                      ? TR_QuadWordReg
            : getOpCode().hasYMMSource()                                      ? TR_VectorReg256
            : getOpCode().hasZMMSource()                                      ? TR_VectorReg512
                                                                              : TR_WordReg;

        // Ensure both source and target registers have the
        // same width if they are the same virtual register.
        //
        if (firstRegister == secondRegister) {
            firstRequestedRegSize = secondRequestedRegSize;
        }

        bool regRegCopy = isRegRegMove();
        OP::Mnemonic opCode = getOpCodeValue();

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        secondRegister->block();

        TR::RealRegister *assignedFirstRegister = firstRegister->getAssignedRealRegister();

        if (assignedFirstRegister == NULL) {
            assignedFirstRegister = assignGPRegister(this, firstRegister, firstRequestedRegSize, cg());
        } else if (firstRequestedRegSize == TR_ByteReg) {
            assignedFirstRegister = assign8BitGPRegister(this, firstRegister, cg());
        }

        if (firstRegister->decFutureUseCount() == 0 && assignedFirstRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(firstRegister, assignedFirstRegister);
            firstRegister->setAssignedRegister(NULL);
            assignedFirstRegister->setState(TR::RealRegister::Unlatched);
        }

        secondRegister->unblock();
        firstRegister->block();

        TR::RealRegister *assignedSecondRegister = secondRegister->getAssignedRealRegister();

        if (assignedSecondRegister == NULL) {
            TR::Machine *machine = cg()->machine();

            cg()->clearRegisterAssignmentFlags();
            cg()->setRegisterAssignmentFlag(TR_NormalAssignment);

            // If first use, totaluse and futureuse will be the same
            //
            if (secondRegister->getTotalUseCount() == secondRegister->getFutureUseCount()) {
                if (regRegCopy && assignedFirstRegister->getState() == TR::RealRegister::Unlatched) {
                    assignedSecondRegister = assignedFirstRegister;
                } else if ((assignedSecondRegister
                               = machine->findBestFreeGPRegister(this, secondRegister, secondRequestedRegSize, true))) {
                    if (cg()->enableBetterSpillPlacements())
                        cg()->removeBetterSpillPlacementCandidate(toRealRegister(assignedSecondRegister));
                } else {
                    cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
                    assignedSecondRegister = machine->freeBestGPRegister(this, secondRegister, secondRequestedRegSize);
                }
            }

            // Depending on direction of assignment, must get register back from spilled state
            //
            else {
                cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
                assignedSecondRegister
                    = machine->reverseGPRSpillState(this, secondRegister, NULL, secondRequestedRegSize);
            }

            secondRegister->setAssignedRegister(assignedSecondRegister);
            secondRegister->setAssignedAsByteRegister(secondRequestedRegSize == TR_ByteReg);
            assignedSecondRegister->setAssignedRegister(secondRegister);
            assignedSecondRegister->setState(TR::RealRegister::Assigned, secondRegister->isPlaceholderReg());
            cg()->traceRegAssigned(secondRegister, assignedSecondRegister);
        } else if (secondRequestedRegSize == TR_ByteReg) {
            assignedSecondRegister = assign8BitGPRegister(this, secondRegister, cg());
        }

        if (secondRegister->decFutureUseCount() == 0
            && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(secondRegister, assignedSecondRegister);
            secondRegister->setAssignedRegister(NULL);
            assignedSecondRegister->setState(TR::RealRegister::Unlatched);
        }

        firstRegister->unblock();

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        setTargetRegister(assignedFirstRegister);
        setSourceRegister(assignedSecondRegister);

        if (getSourceRegister() == getTargetRegister() && regRegCopy) {
            // Remove this instruction from the stream because it is a noop
            // need to leave this instruction's pointers alone because they
            // will be used in the main assignment loop to traverse to the
            // instruction to be assigned after this one
            //
            if (getPrev()) {
                getPrev()->setNext(getNext());
            }
            if (getNext()) {
                getNext()->setPrev(getPrev());
            }
        }

        // If both operands to an SSE conversion routine are the same XMMR,
        // the type of the register needs to be flipped to that of the source
        // operand during the backward pass, so that preceding code will see
        // the correct type.
        //
        if (firstRegister == secondRegister) {
            if (opCode == OP::CVTSS2SDRegReg)
                secondRegister->setIsSinglePrecision(true);
            else if (opCode == OP::CVTSD2SSRegReg)
                secondRegister->setIsSinglePrecision(false);
        }
    }

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegImmInstruction::X86RegImmInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86RegInstruction(treg, node, op, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

TR::X86RegImmInstruction::X86RegImmInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding, int32_t reloKind)
    : TR::X86RegInstruction(treg, node, op, cg, encoding)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

TR::X86RegImmInstruction::X86RegImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86RegInstruction(treg, op, precedingInstruction, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

TR::X86RegImmInstruction::X86RegImmInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86RegInstruction(cond, treg, node, op, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

TR::X86RegImmInstruction::X86RegImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, int32_t imm, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86RegInstruction(cond, treg, op, precedingInstruction, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

void TR::X86RegImmInstruction::adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg)
{
    TR::RealRegister::RegNum targetReal = toRealRegister(toRealRegister(getTargetRegister()))->getRegisterNumber();
    if (state->_register == targetReal) {
        switch (getOpCodeValue()) {
            case OP::ADD4RegImms:
            case OP::ADD4RegImm4:
            case OP::ADD8RegImms:
            case OP::ADD8RegImm4:
                state->_displacement -= getSourceImmediate();
                break;
            case OP::SUB4RegImms:
            case OP::SUB4RegImm4:
            case OP::SUB8RegImms:
            case OP::SUB8RegImm4:
                state->_displacement += getSourceImmediate();
                break;
            default:
                if (cg->getDebug())
                    TR_ASSERT(0, "Can't compute VFP adjustment due to instruction %s: %s %s, %d",
                        cg->getDebug()->getName(this), cg->getDebug()->getMnemonicName(&getOpCode()),
                        cg->getDebug()->getName(getTargetRegister()), getSourceImmediate());
                else
                    TR_ASSERT(0, "Can't compute VFP adjustment");
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegImmSymInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegImmSymInstruction::X86RegImmSymInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *reg, int32_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::X86RegImmInstruction(imm, reg, node, op, cg)
    , _symbolReference(sr)
{
    autoSetReloKind();
}

TR::X86RegImmSymInstruction::X86RegImmSymInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *reg, int32_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::X86RegImmInstruction(imm, reg, op, precedingInstruction, cg)
    , _symbolReference(sr)
{
    autoSetReloKind();
}

void TR::X86RegImmSymInstruction::autoSetReloKind()
{
    TR::Symbol *symbol = getSymbolReference()->getSymbol();
    if (symbol->isConst() || symbol->isConstantPoolAddress()) {
        setReloKind(TR_ConstantPool);
    } else if (symbol->isClassObject()) {
        setReloKind(TR_ClassAddress);
    } else if (symbol->isMethod()) {
        setReloKind(TR_MethodObject);
    } else if (symbol->isStatic() && !symbol->isNotDataAddress()) {
        setReloKind(TR_DataAddress);
    } else if (symbol->isDebugCounter()) {
        setReloKind(TR_DebugCounter);
    } else if (symbol->isBlockFrequency()) {
        setReloKind(TR_BlockFrequency);
    } else if (symbol->isRecompQueuedFlag()) {
        setReloKind(TR_RecompQueuedFlag);
    } else if (symbol->isEnterEventHookAddress() || symbol->isExitEventHookAddress()) {
        setReloKind(TR_MethodEnterExitHookAddress);
    } else if (cg()->comp()->compileRelocatableCode() && symbol->isCallSiteTableEntry()) {
        setReloKind(TR_CallsiteTableEntryAddress);
    } else if (cg()->comp()->compileRelocatableCode() && symbol->isMethodTypeTableEntry()) {
        setReloKind(TR_MethodTypeTableEntryAddress);
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegRegImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegRegImmInstruction::X86RegRegImmInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::Register *sreg, int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(sreg, treg, node, op, cg, encoding)
    , _sourceImmediate(imm)
{}

TR::X86RegRegImmInstruction::X86RegRegImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::Register *sreg, int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(sreg, treg, op, precedingInstruction, cg, encoding)
    , _sourceImmediate(imm)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegRegRegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegRegRegInstruction::X86RegRegRegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::Register *slreg, TR::Register *srreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(srreg, treg, node, op, cg, encoding)
    , _source2ndRegister(slreg)
{
    useRegister(slreg);
}

TR::X86RegRegRegInstruction::X86RegRegRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::Register *slreg, TR::Register *srreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(srreg, treg, op, precedingInstruction, cg, encoding)
    , _source2ndRegister(slreg)
{
    useRegister(slreg);
}

TR::X86RegRegRegInstruction::X86RegRegRegInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::Register *slreg, TR::Register *srreg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(cond, srreg, treg, node, op, cg, encoding)
    , _source2ndRegister(slreg)
{
    useRegister(slreg);
}

TR::X86RegRegRegInstruction::X86RegRegRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::Register *slreg, TR::Register *srreg, TR::RegisterDependencyConditions *cond,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegRegInstruction(cond, srreg, treg, op, precedingInstruction, cg, encoding)
    , _source2ndRegister(slreg)
{
    useRegister(slreg);
}

bool TR::X86RegRegRegInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() || reg == getSourceRegister() || reg == getSource2ndRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegRegRegInstruction::defsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() && getOpCode().modifiesTarget()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86RegRegRegInstruction::usesRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().usesTarget()) || reg == getSourceRegister()
        || reg == getSource2ndRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegRegRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getSource2ndRegister()->block();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getSource2ndRegister()->unblock();
    }

    TR_RegisterSizes firstRequestedRegSize = getOpCode().hasByteTarget() ? TR_ByteReg
        : getOpCode().hasXMMTarget()                                     ? TR_QuadWordReg
        : getOpCode().hasYMMTarget()                                     ? TR_VectorReg256
        : getOpCode().hasZMMTarget()                                     ? TR_VectorReg512
                                                                         : TR_WordReg;

    TR_RegisterSizes secondRequestedRegSize = getOpCode().hasByteSource() ? TR_ByteReg
        : getOpCode().hasXMMSource()                                      ? TR_QuadWordReg
        : getOpCode().hasYMMSource()                                      ? TR_VectorReg256
        : getOpCode().hasZMMSource()                                      ? TR_VectorReg512
                                                                          : TR_WordReg;
    TR_RegisterSizes thirdRequestedRegSize = secondRequestedRegSize;

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR::Register *firstRegister;
        TR::Register *secondRegister;
        TR::Register *thirdRegister;

        firstRegister = getTargetRegister();
        secondRegister = getSourceRegister();
        thirdRegister = getSource2ndRegister();

        secondRegister->block();
        thirdRegister->block();

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::RealRegister *assignedFirstRegister = firstRegister->getAssignedRealRegister();

        if (assignedFirstRegister == NULL) {
            assignedFirstRegister = assignGPRegister(this, firstRegister, firstRequestedRegSize, cg());
        } else if (firstRequestedRegSize == TR_ByteReg) {
            assignedFirstRegister = assign8BitGPRegister(this, firstRegister, cg());
        }

        if (firstRegister->decFutureUseCount() == 0 && assignedFirstRegister->getState() != TR::RealRegister::Locked
            && firstRegister == getTargetRegister()) {
            cg()->traceRegFreed(firstRegister, assignedFirstRegister);
            firstRegister->setAssignedRegister(NULL);
            assignedFirstRegister->setState(TR::RealRegister::Unlatched);
        }

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        secondRegister->unblock();

        if (getDependencyConditions())
            getDependencyConditions()->blockPreConditionRegisters();

        firstRegister->block();

        TR::RealRegister *assignedSecondRegister = secondRegister->getAssignedRealRegister();

        if (assignedSecondRegister == NULL) {
            assignedSecondRegister = assignGPRegister(this, secondRegister, secondRequestedRegSize, cg());
        } else if (secondRequestedRegSize == TR_ByteReg) {
            assignedSecondRegister = assign8BitGPRegister(this, secondRegister, cg());
        }

        secondRegister->decFutureUseCount();

        if (thirdRegister == getTargetRegister()) {
            if (secondRegister->getFutureUseCount() == 0
                && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(secondRegister, assignedSecondRegister);
                secondRegister->setAssignedRegister(NULL);
                assignedSecondRegister->setState(TR::RealRegister::Unlatched);
            }
            if (firstRegister->getFutureUseCount() == 0
                && assignedFirstRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(firstRegister, assignedFirstRegister);
                firstRegister->setAssignedRegister(NULL);
                assignedFirstRegister->setState(TR::RealRegister::Unlatched);
            }
        }

        secondRegister->block();
        thirdRegister->unblock();

        TR::RealRegister *assignedThirdRegister = thirdRegister->getAssignedRealRegister();

        if (assignedThirdRegister == NULL) {
            assignedThirdRegister = assignGPRegister(this, thirdRegister, thirdRequestedRegSize, cg());
        } else if (thirdRequestedRegSize == TR_ByteReg) {
            assignedThirdRegister = assign8BitGPRegister(this, thirdRegister, cg());
        }

        if (thirdRegister->decFutureUseCount() == 0 && assignedThirdRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(thirdRegister, assignedThirdRegister);
            thirdRegister->setAssignedRegister(NULL);
            assignedThirdRegister->setState(TR::RealRegister::Unlatched);
        }

        if (firstRegister == getTargetRegister()) {
            if (secondRegister->getFutureUseCount() == 0
                && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(secondRegister, assignedSecondRegister);
                secondRegister->setAssignedRegister(NULL);
                assignedSecondRegister->setState(TR::RealRegister::Unlatched);
            }
            if (thirdRegister->getFutureUseCount() == 0
                && assignedThirdRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(thirdRegister, assignedThirdRegister);
                thirdRegister->setAssignedRegister(NULL);
                assignedThirdRegister->setState(TR::RealRegister::Unlatched);
            }
        }

        setTargetRegister(assignedFirstRegister);
        setSourceRegister(assignedSecondRegister);
        setSource2ndRegister(assignedThirdRegister);
        secondRegister->unblock();
        firstRegister->unblock();
        if (getDependencyConditions())
            getDependencyConditions()->unblockPreConditionRegisters();
    }

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getSource2ndRegister()->block();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getSource2ndRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegMaskRegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

bool TR::X86RegMaskRegInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() || reg == getSourceRegister() || reg == getMaskRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegMaskRegInstruction::defsRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().modifiesTarget())
        || (reg == getSourceRegister() && getOpCode().modifiesSource())) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86RegMaskRegInstruction::usesRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().usesTarget()) || reg == getSourceRegister()
        || reg == getMaskRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegMaskRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getMaskRegister()->block();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getMaskRegister()->unblock();
    }

    if (kindsToBeAssigned & getMaskRegister()->getKindAsMask()) {
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::Register *maskRegister = getMaskRegister();
        TR::RealRegister *assignedMaskRegister = maskRegister->getAssignedRealRegister();

        if (assignedMaskRegister == NULL) {
            assignedMaskRegister = assignGPRegister(this, maskRegister, TR_QuadWordReg, cg());
        }

        if (maskRegister->decFutureUseCount() == 0 && assignedMaskRegister->getState() != TR::RealRegister::Locked
            && maskRegister == getMaskRegister()) {
            cg()->traceRegFreed(maskRegister, assignedMaskRegister);
            maskRegister->setAssignedRegister(NULL);
            assignedMaskRegister->setState(TR::RealRegister::Unlatched);
        }

        setMaskRegister(assignedMaskRegister);

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR::Register *firstRegister = getTargetRegister();
        TR::Register *secondRegister = getSourceRegister();

        OMR::X86::Encoding encoding = getEncodingMethod();
        TR_RegisterSizes firstRequestedRegSize = encoding == OMR::X86::EVEX_L512 ? TR_VectorReg512
            : encoding == OMR::X86::EVEX_L256                                    ? TR_VectorReg256
                                                                                 : TR_VectorReg128;

        TR_RegisterSizes secondRequestedRegSize = firstRequestedRegSize;

        bool regRegCopy = isRegRegMove();
        OP::Mnemonic opCode = getOpCodeValue();

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        secondRegister->block();

        TR::RealRegister *assignedFirstRegister = firstRegister->getAssignedRealRegister();

        if (assignedFirstRegister == NULL) {
            assignedFirstRegister = assignGPRegister(this, firstRegister, firstRequestedRegSize, cg());
        }

        if (firstRegister->decFutureUseCount() == 0 && assignedFirstRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(firstRegister, assignedFirstRegister);
            firstRegister->setAssignedRegister(NULL);
            assignedFirstRegister->setState(TR::RealRegister::Unlatched);
        }

        secondRegister->unblock();
        firstRegister->block();

        TR::RealRegister *assignedSecondRegister = secondRegister->getAssignedRealRegister();

        if (assignedSecondRegister == NULL) {
            TR::Machine *machine = cg()->machine();

            cg()->clearRegisterAssignmentFlags();
            cg()->setRegisterAssignmentFlag(TR_NormalAssignment);

            // If first use, totaluse and futureuse will be the same
            //
            if (secondRegister->getTotalUseCount() == secondRegister->getFutureUseCount()) {
                if (regRegCopy && assignedFirstRegister->getState() == TR::RealRegister::Unlatched) {
                    assignedSecondRegister = assignedFirstRegister;
                } else if ((assignedSecondRegister
                               = machine->findBestFreeGPRegister(this, secondRegister, secondRequestedRegSize, true))) {
                    if (cg()->enableBetterSpillPlacements())
                        cg()->removeBetterSpillPlacementCandidate(toRealRegister(assignedSecondRegister));
                } else {
                    cg()->setRegisterAssignmentFlag(TR_RegisterSpilled);
                    assignedSecondRegister = machine->freeBestGPRegister(this, secondRegister, secondRequestedRegSize);
                }
            }

            // Depending on direction of assignment, must get register back from spilled state
            //
            else {
                cg()->setRegisterAssignmentFlag(TR_RegisterReloaded);
                assignedSecondRegister
                    = machine->reverseGPRSpillState(this, secondRegister, NULL, secondRequestedRegSize);
            }

            secondRegister->setAssignedRegister(assignedSecondRegister);
            secondRegister->setAssignedAsByteRegister(secondRequestedRegSize == TR_ByteReg);
            assignedSecondRegister->setAssignedRegister(secondRegister);
            assignedSecondRegister->setState(TR::RealRegister::Assigned, secondRegister->isPlaceholderReg());
            cg()->traceRegAssigned(secondRegister, assignedSecondRegister);
        }

        if (secondRegister->decFutureUseCount() == 0
            && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(secondRegister, assignedSecondRegister);
            secondRegister->setAssignedRegister(NULL);
            assignedSecondRegister->setState(TR::RealRegister::Unlatched);
        }

        firstRegister->unblock();

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        setTargetRegister(assignedFirstRegister);
        setSourceRegister(assignedSecondRegister);
    }

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getMaskRegister()->block();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getMaskRegister()->unblock();
    }
}

bool TR::X86RegMaskRegRegInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() || reg == getSourceRegister() || reg == getSource2ndRegister()
        || reg == getMaskRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegMaskRegRegInstruction::defsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() && getOpCode().modifiesTarget()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86RegMaskRegRegInstruction::usesRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().usesTarget()) || reg == getSourceRegister()
        || reg == getSource2ndRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegMaskRegRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getSource2ndRegister()->block();
        getMaskRegister()->block();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getSource2ndRegister()->unblock();
        getMaskRegister()->unblock();
    }

    OMR::X86::Encoding encoding = getEncodingMethod();
    TR_RegisterSizes firstRequestedRegSize = encoding == OMR::X86::EVEX_L512 ? TR_VectorReg512
        : encoding == OMR::X86::EVEX_L256                                    ? TR_VectorReg256
                                                                             : TR_VectorReg128;

    TR_RegisterSizes secondRequestedRegSize = firstRequestedRegSize;
    TR_RegisterSizes thirdRequestedRegSize = secondRequestedRegSize;

    if (kindsToBeAssigned & getMaskRegister()->getKindAsMask()) {
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::Register *maskRegister = getMaskRegister();
        TR::RealRegister *assignedMaskRegister = maskRegister->getAssignedRealRegister();

        if (assignedMaskRegister == NULL) {
            assignedMaskRegister = assignGPRegister(this, maskRegister, TR_QuadWordReg, cg());
        }

        if (maskRegister->decFutureUseCount() == 0 && assignedMaskRegister->getState() != TR::RealRegister::Locked
            && maskRegister == getMaskRegister()) {
            cg()->traceRegFreed(maskRegister, assignedMaskRegister);
            maskRegister->setAssignedRegister(NULL);
            assignedMaskRegister->setState(TR::RealRegister::Unlatched);
        }

        setMaskRegister(assignedMaskRegister);

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR::Register *firstRegister;
        TR::Register *secondRegister;
        TR::Register *thirdRegister;

        firstRegister = getTargetRegister();
        secondRegister = getSourceRegister();
        thirdRegister = getSource2ndRegister();

        secondRegister->block();
        thirdRegister->block();

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::RealRegister *assignedFirstRegister = firstRegister->getAssignedRealRegister();

        if (assignedFirstRegister == NULL) {
            assignedFirstRegister = assignGPRegister(this, firstRegister, firstRequestedRegSize, cg());
        } else if (firstRequestedRegSize == TR_ByteReg) {
            assignedFirstRegister = assign8BitGPRegister(this, firstRegister, cg());
        }

        if (firstRegister->decFutureUseCount() == 0 && assignedFirstRegister->getState() != TR::RealRegister::Locked
            && firstRegister == getTargetRegister()) {
            cg()->traceRegFreed(firstRegister, assignedFirstRegister);
            firstRegister->setAssignedRegister(NULL);
            assignedFirstRegister->setState(TR::RealRegister::Unlatched);
        }

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        secondRegister->unblock();

        if (getDependencyConditions())
            getDependencyConditions()->blockPreConditionRegisters();

        firstRegister->block();

        TR::RealRegister *assignedSecondRegister = secondRegister->getAssignedRealRegister();

        if (assignedSecondRegister == NULL) {
            assignedSecondRegister = assignGPRegister(this, secondRegister, secondRequestedRegSize, cg());
        } else if (secondRequestedRegSize == TR_ByteReg) {
            assignedSecondRegister = assign8BitGPRegister(this, secondRegister, cg());
        }

        secondRegister->decFutureUseCount();

        if (thirdRegister == getTargetRegister()) {
            if (secondRegister->getFutureUseCount() == 0
                && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(secondRegister, assignedSecondRegister);
                secondRegister->setAssignedRegister(NULL);
                assignedSecondRegister->setState(TR::RealRegister::Unlatched);
            }
            if (firstRegister->getFutureUseCount() == 0
                && assignedFirstRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(firstRegister, assignedFirstRegister);
                firstRegister->setAssignedRegister(NULL);
                assignedFirstRegister->setState(TR::RealRegister::Unlatched);
            }
        }

        secondRegister->block();
        thirdRegister->unblock();

        TR::RealRegister *assignedThirdRegister = thirdRegister->getAssignedRealRegister();

        if (assignedThirdRegister == NULL) {
            assignedThirdRegister = assignGPRegister(this, thirdRegister, thirdRequestedRegSize, cg());
        } else if (thirdRequestedRegSize == TR_ByteReg) {
            assignedThirdRegister = assign8BitGPRegister(this, thirdRegister, cg());
        }

        if (thirdRegister->decFutureUseCount() == 0 && assignedThirdRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(thirdRegister, assignedThirdRegister);
            thirdRegister->setAssignedRegister(NULL);
            assignedThirdRegister->setState(TR::RealRegister::Unlatched);
        }

        if (firstRegister == getTargetRegister()) {
            if (secondRegister->getFutureUseCount() == 0
                && assignedSecondRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(secondRegister, assignedSecondRegister);
                secondRegister->setAssignedRegister(NULL);
                assignedSecondRegister->setState(TR::RealRegister::Unlatched);
            }
            if (thirdRegister->getFutureUseCount() == 0
                && assignedThirdRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(thirdRegister, assignedThirdRegister);
                thirdRegister->setAssignedRegister(NULL);
                assignedThirdRegister->setState(TR::RealRegister::Unlatched);
            }
        }

        setTargetRegister(assignedFirstRegister);
        setSourceRegister(assignedSecondRegister);
        setSource2ndRegister(assignedThirdRegister);
        secondRegister->unblock();
        firstRegister->unblock();
        if (getDependencyConditions())
            getDependencyConditions()->unblockPreConditionRegisters();
    }

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getSourceRegister()->block();
        getSource2ndRegister()->block();
        getMaskRegister()->block();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getSourceRegister()->unblock();
        getSource2ndRegister()->unblock();
        getMaskRegister()->unblock();
    }
}

void padUnresolvedReferenceInstruction(TR::Instruction *instr, TR::MemoryReference *mr, TR::CodeGenerator *cg)
{
    mr->getUnresolvedDataSnippet()->setDataReferenceInstruction(instr);
    Inst_BoundaryAvoidance(TR::X86BoundaryAvoidanceInstruction::unresolvedAtomicRegions, 8, 8, instr, cg);
}

void insertUnresolvedReferenceInstructionMemoryBarrier(TR::CodeGenerator *cg, int32_t barrier, TR::Instruction *inst,
    TR::MemoryReference *mr, TR::Register *srcReg, TR::MemoryReference *anotherMr)
{
    TR::InstOpCode fenceOp;
    bool is5ByteFence = false;

    TR_ASSERT_FATAL(cg->comp()->compileRelocatableCode() || cg->comp()->isOutOfProcessCompilation()
            || cg->comp()->compilePortableCode()
            || cg->comp()->target().cpu.requiresLFence() == cg->getX86ProcessorInfo().requiresLFENCE(),
        "requiresLFence() failed\n");

    if (barrier & LockOR) {
        fenceOp.setOpCodeValue(OP::LOR4MemImms);
        is5ByteFence = true;
    } else if ((barrier & kMemoryFence) == kMemoryFence)
        fenceOp.setOpCodeValue(OP::MFENCE);
    else if ((barrier & kLoadFence) && cg->comp()->target().cpu.requiresLFence())
        fenceOp.setOpCodeValue(OP::LFENCE);
    else if (barrier & kStoreFence)
        fenceOp.setOpCodeValue(OP::SFENCE);
    else
        TR_ASSERT(false, "No valid memory barrier has been found. \n");

    TR::Instruction *padInst = NULL;
    TR::Instruction *fenceInst = NULL;
    TR::Instruction *lockPrefix = NULL;

    if (is5ByteFence) {
        // generate LOCK OR dword ptr [esp], 0
        padInst = Inst_Alignment(inst, 8, cg);
        TR::RealRegister *espReal = cg->machine()->getRealRegister(TR::RealRegister::esp);
        TR::MemoryReference *espMemRef = MRef_Bdisp32(espReal, 0, cg);
        fenceInst
            = new (cg->trHeapMemory()) TR::X86MemImmInstruction(padInst, fenceOp.getOpCodeValue(), espMemRef, 0, cg);
    } else {
        // generate memory fence
        padInst = Inst_Alignment(inst, 4, cg);
        fenceInst = new (cg->trHeapMemory()) TR::Instruction(fenceOp.getOpCodeValue(), padInst, cg);
    }

    TR::LabelSymbol *doneLabel = TR::LabelSymbol::create(cg->trHeapMemory(), cg);
    TR::Register *baseReg = mr->getBaseRegister();
    TR::Register *indexReg = mr->getIndexRegister();
    TR::Register *addressReg = NULL;

    if (cg->comp()->target().is64Bit())
        addressReg = mr->getAddressRegister();

    TR::RegisterDependencyConditions *deps = RegDeps((uint8_t)0, 7, cg);

    if (baseReg)
        deps->addPostCondition(baseReg, TR::RealRegister::NoReg, cg);

    if (indexReg)
        deps->addPostCondition(indexReg, TR::RealRegister::NoReg, cg);

    if (srcReg)
        deps->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);

    if (addressReg)
        deps->addPostCondition(addressReg, TR::RealRegister::NoReg, cg);

    if (anotherMr) {
        addressReg = NULL;
        baseReg = anotherMr->getBaseRegister();
        indexReg = anotherMr->getIndexRegister();
        if (cg->comp()->target().is64Bit())
            addressReg = anotherMr->getAddressRegister();

        if (baseReg)
            deps->addPostCondition(baseReg, TR::RealRegister::NoReg, cg);
        if (indexReg)
            deps->addPostCondition(indexReg, TR::RealRegister::NoReg, cg);
        if (addressReg)
            deps->addPostCondition(addressReg, TR::RealRegister::NoReg, cg);
    }

    deps->stopAddingConditions();

    if (deps)
        Inst_Label(fenceInst, OP::label, doneLabel, deps, cg);
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemInstruction::X86MemInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::CodeGenerator *cg, TR::Register *sreg, OMR::X86::Encoding encoding)
    : TR::Instruction(node, op, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }

    if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport)) {
        int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

        if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr, sreg);
    }

    // Find out if this instruction clobbers the memory reference associated with
    // a live discardable register.
    //
    if (cg->enableRematerialisation() && getOpCode().modifiesTarget() && !cg->getLiveDiscardableRegisters().empty()) {
        cg->clobberLiveDiscardableRegisters(this, mr);
    }
}

TR::X86MemInstruction::X86MemInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, TR::CodeGenerator *cg, TR::Register *sreg, OMR::X86::Encoding encoding)
    : TR::Instruction(op, precedingInstruction, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }

    if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport)) {
        int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

        if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr, sreg);
    }
}

TR::X86MemInstruction::X86MemInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, TR::Register *sreg, OMR::X86::Encoding encoding)
    : TR::Instruction(cond, node, op, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }

    if (!cg->comp()->getOption(TR_DisableNewX86VolatileSupport)) {
        int32_t barrier = memoryBarrierRequired(this->getOpCode(), mr, cg, true);

        if (barrier)
            insertUnresolvedReferenceInstructionMemoryBarrier(cg, barrier, this, mr, sreg);
    }
}

bool TR::X86MemInstruction::refsRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg)) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86MemInstruction::defsRegister(TR::Register *reg)
{
    if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    } else {
        return false;
    }
}

bool TR::X86MemInstruction::usesRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg)) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

TR::Snippet *TR::X86MemInstruction::getSnippetForGC() { return getMemoryReference()->getUnresolvedDataSnippet(); }

void TR::X86MemInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
    }

    if (kindsToBeAssigned & TR_GPR_Mask) {
        TR::RegisterDependencyConditions *deps = getDependencyConditions();
        if (deps) {
            deps->blockPreConditionRegisters();
        }

        getMemoryReference()->assignRegisters(this, cg());

        if (deps) {
            deps->unblockPreConditionRegisters();
        }
    }

#ifdef J9_PROJECT_SPECIFIC
    if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask)) {
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet) {
            if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask))
                snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
        }
    }
#endif

    if (getDependencyConditions()) {
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
    }
}

void TR::X86MemTableInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    // Call inherited logic
    //
    TR::X86MemInstruction::assignRegisters(kindsToBeAssigned);
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86CallMemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86CallMemInstruction::X86CallMemInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::X86MemInstruction(cond, mr, node, op, cg)
    , _adjustsFramePointerBy(0)
{}

TR::X86CallMemInstruction::X86CallMemInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
    : TR::X86MemInstruction(cond, mr, op, precedingInstruction, cg)
    , _adjustsFramePointerBy(0)
{}

TR::X86CallMemInstruction::X86CallMemInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::CodeGenerator *cg)
    : TR::X86MemInstruction(mr, node, op, cg)
    , _adjustsFramePointerBy(0)
{}

void TR::X86CallMemInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getDependencyConditions()->blockPostConditionRealDependencyRegisters(cg());
    }
    getMemoryReference()->assignRegisters(this, cg());
    if (getDependencyConditions()) {
        getDependencyConditions()->unblockPostConditionRealDependencyRegisters(cg());
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemImmInstruction::X86MemImmInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86MemInstruction(mr, node, op, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

TR::X86MemImmInstruction::X86MemImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind)
    : TR::X86MemInstruction(mr, op, precedingInstruction, cg)
    , _sourceImmediate(imm)
    , _reloKind(reloKind)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemImmSymInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemImmSymInstruction::X86MemImmSymInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    int32_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::X86MemImmInstruction(imm, mr, node, op, cg)
    , _symbolReference(sr)
{}

TR::X86MemImmSymInstruction::X86MemImmSymInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, int32_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::X86MemImmInstruction(imm, mr, op, precedingInstruction, cg)
    , _symbolReference(sr)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemRegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemRegInstruction::X86MemRegInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::Register *sreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86MemInstruction(mr, node, op, cg, sreg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86MemRegInstruction::X86MemRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, TR::Register *sreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86MemInstruction(mr, op, precedingInstruction, cg, sreg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86MemRegInstruction::X86MemRegInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::Register *sreg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86MemInstruction(cond, mr, node, op, cg, sreg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

TR::X86MemRegInstruction::X86MemRegInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, TR::Register *sreg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::X86MemInstruction(cond, mr, op, precedingInstruction, cg, sreg, encoding)
    , _sourceRegister(sreg)
{
    useRegister(sreg);
}

bool TR::X86MemRegInstruction::refsRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86MemRegInstruction::defsRegister(TR::Register *reg)
{
    if (reg == getSourceRegister() && getOpCode().modifiesSource()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86MemRegInstruction::usesRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86MemRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getSourceRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getSourceRegister()->unblock();
    }

    TR::RealRegister *assignedRegister = NULL;
    TR_RegisterSizes requestedRegSize = TR_WordReg;

    if (kindsToBeAssigned & getSourceRegister()->getKindAsMask()) {
        assignedRegister = getSourceRegister()->getAssignedRealRegister();
        TR::RealRegister::RegState oldState = TR::RealRegister::Free;
        bool blockedEbp = false;
        if ((getMemoryReference()->getBaseRegister() == cg()->getVMThreadRegister())
            || (getMemoryReference()->getIndexRegister() == cg()->getVMThreadRegister())) {
            blockedEbp = true;
            oldState = cg()->machine()->getRealRegister(TR::RealRegister::ebp)->getState();
            cg()->machine()
                ->getRealRegister(TR::RealRegister::ebp)
                ->setState(TR::RealRegister::Locked); //(TR::RealRegister::Locked);
        }
        getMemoryReference()->blockRegisters();
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        if (getOpCode().hasByteSource()) {
            requestedRegSize = TR_ByteReg;
        } else if (getOpCode().hasXMMSource()) {
            requestedRegSize = TR_QuadWordReg;
        } else if (getOpCode().hasYMMSource()) {
            requestedRegSize = TR_VectorReg256;
        } else if (getOpCode().hasZMMSource()) {
            requestedRegSize = TR_VectorReg512;
        }

        if (assignedRegister == NULL) {
            assignedRegister = assignGPRegister(this, getSourceRegister(), requestedRegSize, cg());
        } else if (requestedRegSize == TR_ByteReg) {
            assignedRegister = assign8BitGPRegister(this, getSourceRegister(), cg());
        }

        // If the source register became discardable because of this instruction, reset
        // its rematerializability before we allocate registers for this instruction.
        //
        if (cg()->enableRematerialisation() && getSourceRegister()->isDiscardable()
            && getSourceRegister()->getRematerializationInfo()->getDefinition() == this) {
            if (debug("dumpRemat")) {
                diagnostic("---> Deactivating %s discardable register %s at instruction %p\n",
                    cg()->getDebug()->toString(getSourceRegister()->getRematerializationInfo()),
                    cg()->getDebug()->getName(getSourceRegister()), this);
            }

            getSourceRegister()->resetIsDiscardable();
            getSourceRegister()->getRematerializationInfo()->resetRematerialized();
        }

#ifdef J9_PROJECT_SPECIFIC
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet) {
            if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask))
                snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
        }
#endif

        if (blockedEbp) {
            switch (oldState) {
                case TR::RealRegister::Free:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Free);
                    break;
                case TR::RealRegister::Unlatched:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Unlatched);
                    break;
                case TR::RealRegister::Assigned:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Assigned);
                    break;
                case TR::RealRegister::Blocked:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Blocked);
                    break;
                case TR::RealRegister::Locked:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Locked);
                    break;
            }
        }

        getMemoryReference()->unblockRegisters();

        if (getSourceRegister()->decFutureUseCount() == 0 && assignedRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(getSourceRegister(), assignedRegister);
            getSourceRegister()->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
        }

        setSourceRegister(assignedRegister);

        if (assignedRegister != NULL) {
            assignedRegister->block();
            getMemoryReference()->assignRegisters(this, cg());
            assignedRegister->unblock();
        } else {
            getMemoryReference()->assignRegisters(this, cg());
        }

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    if (getDependencyConditions()) {
        getSourceRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getSourceRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemMaskRegInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

bool TR::X86MemMaskRegInstruction::refsRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86MemMaskRegInstruction::defsRegister(TR::Register *reg)
{
    if (reg == getSourceRegister() && getOpCode().modifiesSource()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->defsRegister(reg);
    }

    return false;
}

bool TR::X86MemMaskRegInstruction::usesRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86MemMaskRegInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getSourceRegister()->block();
        getMaskRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getSourceRegister()->unblock();
        getMaskRegister()->unblock();
    }

    if (kindsToBeAssigned & getMaskRegister()->getKindAsMask()) {
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::Register *maskRegister = getMaskRegister();
        TR::RealRegister *assignedMaskRegister = maskRegister->getAssignedRealRegister();

        if (assignedMaskRegister == NULL) {
            assignedMaskRegister = assignGPRegister(this, maskRegister, TR_QuadWordReg, cg());
        }

        if (maskRegister->decFutureUseCount() == 0 && assignedMaskRegister->getState() != TR::RealRegister::Locked
            && maskRegister == getMaskRegister()) {
            cg()->traceRegFreed(maskRegister, assignedMaskRegister);
            maskRegister->setAssignedRegister(NULL);
            assignedMaskRegister->setState(TR::RealRegister::Unlatched);
        }

        setMaskRegister(assignedMaskRegister);

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    TR::RealRegister *assignedRegister = NULL;
    OMR::X86::Encoding encoding = getEncodingMethod();
    TR_RegisterSizes requestedRegSize = encoding == OMR::X86::EVEX_L512 ? TR_VectorReg512
        : encoding == OMR::X86::EVEX_L256                               ? TR_VectorReg256
                                                                        : TR_VectorReg128;

    if (kindsToBeAssigned & getSourceRegister()->getKindAsMask()) {
        assignedRegister = getSourceRegister()->getAssignedRealRegister();
        TR::RealRegister::RegState oldState = TR::RealRegister::Free;
        bool blockedEbp = false;
        if ((getMemoryReference()->getBaseRegister() == cg()->getVMThreadRegister())
            || (getMemoryReference()->getIndexRegister() == cg()->getVMThreadRegister())) {
            blockedEbp = true;
            oldState = cg()->machine()->getRealRegister(TR::RealRegister::ebp)->getState();
            cg()->machine()
                ->getRealRegister(TR::RealRegister::ebp)
                ->setState(TR::RealRegister::Locked); //(TR::RealRegister::Locked);
        }
        getMemoryReference()->blockRegisters();
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        if (assignedRegister == NULL) {
            assignedRegister = assignGPRegister(this, getSourceRegister(), requestedRegSize, cg());
        }

        // If the source register became discardable because of this instruction, reset
        // its rematerializability before we allocate registers for this instruction.
        //
        if (cg()->enableRematerialisation() && getSourceRegister()->isDiscardable()
            && getSourceRegister()->getRematerializationInfo()->getDefinition() == this) {
            if (debug("dumpRemat")) {
                diagnostic("---> Deactivating %s discardable register %s at instruction %p\n",
                    cg()->getDebug()->toString(getSourceRegister()->getRematerializationInfo()),
                    cg()->getDebug()->getName(getSourceRegister()), this);
            }

            getSourceRegister()->resetIsDiscardable();
            getSourceRegister()->getRematerializationInfo()->resetRematerialized();
        }

#ifdef J9_PROJECT_SPECIFIC
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet) {
            if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask))
                snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
        }
#endif

        if (blockedEbp) {
            switch (oldState) {
                case TR::RealRegister::Free:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Free);
                    break;
                case TR::RealRegister::Unlatched:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Unlatched);
                    break;
                case TR::RealRegister::Assigned:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Assigned);
                    break;
                case TR::RealRegister::Blocked:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Blocked);
                    break;
                case TR::RealRegister::Locked:
                    cg()->machine()->getRealRegister(TR::RealRegister::ebp)->resetState(TR::RealRegister::Locked);
                    break;
            }
        }

        getMemoryReference()->unblockRegisters();

        if (getSourceRegister()->decFutureUseCount() == 0 && assignedRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(getSourceRegister(), assignedRegister);
            getSourceRegister()->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
        }

        setSourceRegister(assignedRegister);

        if (assignedRegister != NULL) {
            assignedRegister->block();
            getMemoryReference()->assignRegisters(this, cg());
            assignedRegister->unblock();
        } else {
            getMemoryReference()->assignRegisters(this, cg());
        }

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    if (getDependencyConditions()) {
        getSourceRegister()->block();
        getMaskRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getSourceRegister()->unblock();
        getMaskRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86MemRegImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86MemRegImmInstruction::X86MemRegImmInstruction(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::Register *sreg, int32_t imm, TR::CodeGenerator *cg)
    : TR::X86MemRegInstruction(sreg, mr, node, op, cg)
    , _sourceImmediate(imm)
{}

TR::X86MemRegImmInstruction::X86MemRegImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::MemoryReference *mr, TR::Register *sreg, int32_t imm, TR::CodeGenerator *cg)
    : TR::X86MemRegInstruction(sreg, mr, op, precedingInstruction, cg)
    , _sourceImmediate(imm)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegMemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegMemInstruction::X86RegMemInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(treg, node, op, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }

    // Find out if this instruction clobbers the memory reference associated with
    // a live discardable register.
    //
    if (cg->enableRematerialisation()
        && (getOpCodeValue() == OP::LEA2RegMem || getOpCodeValue() == OP::LEA4RegMem
            || getOpCodeValue() == OP::LEA8RegMem)
        && !cg->getLiveDiscardableRegisters().empty()) {
        cg->clobberLiveDiscardableRegisters(this, mr);
    }
}

TR::X86RegMemInstruction::X86RegMemInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(treg, op, precedingInstruction, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }
}

TR::X86RegMemInstruction::X86RegMemInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::MemoryReference *mr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(cond, treg, node, op, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }

    // Find out if this instruction clobbers the memory reference associated with
    // a live discardable register.
    //
    if (cg->enableRematerialisation()
        && (getOpCodeValue() == OP::LEA2RegMem || getOpCodeValue() == OP::LEA4RegMem
            || getOpCodeValue() == OP::LEA8RegMem)
        && !cg->getLiveDiscardableRegisters().empty()) {
        cg->clobberLiveDiscardableRegisters(this, mr);
    }
}

TR::X86RegMemInstruction::X86RegMemInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::MemoryReference *mr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::X86RegInstruction(cond, treg, op, precedingInstruction, cg, encoding)
    , _memoryReference(mr)
{
    mr->useRegisters(this, cg);
    if (mr->getUnresolvedDataSnippet() != NULL) {
        padUnresolvedReferenceInstruction(this, mr, cg);
    }
}

bool TR::X86RegMemInstruction::refsRegister(TR::Register *reg)
{
    if (reg == getTargetRegister() || getMemoryReference()->refsRegister(reg)) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegMemInstruction::usesRegister(TR::Register *reg)
{
    if ((reg == getTargetRegister() && getOpCode().usesTarget()) || getMemoryReference()->refsRegister(reg)) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

TR::Snippet *TR::X86RegMemInstruction::getSnippetForGC() { return getMemoryReference()->getUnresolvedDataSnippet(); }

void TR::X86RegMemInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getMemoryReference()->unblockRegisters();
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        TR_RegisterSizes requestedRegSize = TR_WordReg;

        if (getOpCode().hasByteTarget()) {
            requestedRegSize = TR_ByteReg;
        } else if (getOpCode().hasXMMTarget()) {
            requestedRegSize = TR_QuadWordReg;
        } else if (getOpCode().hasYMMTarget()) {
            requestedRegSize = TR_VectorReg256;
        } else if (getOpCode().hasZMMTarget()) {
            requestedRegSize = TR_VectorReg512;
        }

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }
        getMemoryReference()->blockRegisters();

        TR::RealRegister *assignedRegister = getTargetRegister()->getAssignedRealRegister();
        if (assignedRegister == NULL) {
            assignedRegister = assignGPRegister(this, getTargetRegister(), requestedRegSize, cg());
        } else if (requestedRegSize == TR_ByteReg) {
            assignedRegister = assign8BitGPRegister(this, getTargetRegister(), cg());
        }

        getMemoryReference()->unblockRegisters();
        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        if (getTargetRegister()->decFutureUseCount() == 0 && assignedRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(getTargetRegister(), assignedRegister);
            getTargetRegister()->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
        }

        if (getDependencyConditions())
            getDependencyConditions()->blockPreConditionRegisters();

        setTargetRegister(assignedRegister);

        getTargetRegister()->block();
        getMemoryReference()->assignRegisters(this, cg());
        getTargetRegister()->unblock();

        if (getDependencyConditions())
            getDependencyConditions()->unblockPreConditionRegisters();
    }

#ifdef J9_PROJECT_SPECIFIC
    if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask)) {
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet) {
            if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask))
                snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
        }
    }
#endif

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getMemoryReference()->unblockRegisters();
    }

    if (getOpCodeValue() == OP::LEARegMem()) {
        TR::Register *solitaryMemrefRegister = NULL;
        if (getMemoryReference()->getDisplacement() == 0) {
            if (getMemoryReference()->getBaseRegister() == NULL)
                solitaryMemrefRegister = getMemoryReference()->getIndexRegister();
            else if (getMemoryReference()->getIndexRegister() == NULL)
                solitaryMemrefRegister = getMemoryReference()->getBaseRegister();
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegMemImmInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegMemImmInstruction::X86RegMemImmInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::MemoryReference *mr, int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegMemInstruction(mr, treg, node, op, cg, encoding)
    , _sourceImmediate(imm)
{}

TR::X86RegMemImmInstruction::X86RegMemImmInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *treg, TR::MemoryReference *mr, int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegMemInstruction(mr, treg, op, precedingInstruction, cg, encoding)
    , _sourceImmediate(imm)
{}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegRegMemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::X86RegRegMemInstruction::X86RegRegMemInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *slreg,
    TR::Register *srreg, TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
    : TR::X86RegMemInstruction(op, node, slreg, mr, cg, encoding)
    , _source2ndRegister(srreg)
{
    useRegister(srreg);
}

TR::X86RegRegMemInstruction::X86RegRegMemInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *slreg,
    TR::Register *srreg, TR::MemoryReference *mr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
    : TR::X86RegMemInstruction(op, node, slreg, mr, cond, cg, encoding)
    , _source2ndRegister(srreg)
{
    useRegister(srreg);
}

bool TR::X86RegRegMemInstruction::refsRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getTargetRegister() || reg == getSource2ndRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

// check refsRegister
bool TR::X86RegRegMemInstruction::usesRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getTargetRegister() || reg == getSource2ndRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegRegMemInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getMemoryReference()->blockRegisters();
        getTargetRegister()->block();
        getSource2ndRegister()->block();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getTargetRegister()->unblock();
        getSource2ndRegister()->unblock();
    }

    TR::RealRegister *assignedTargetRegister = NULL;
    TR::RealRegister *assignedSource2ndRegister = NULL;
    TR_RegisterSizes requestedRegSize = getOpCode().hasByteTarget() ? TR_ByteReg
        : getOpCode().hasXMMTarget()                                ? TR_QuadWordReg
        : getOpCode().hasYMMTarget()                                ? TR_VectorReg256
        : getOpCode().hasZMMTarget()                                ? TR_VectorReg512
                                                                    : TR_WordReg;

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
#ifdef J9_PROJECT_SPECIFIC
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet && (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask)))
            snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
#endif

        assignedTargetRegister = getTargetRegister()->getAssignedRealRegister();
        getSource2ndRegister()->block();
        getMemoryReference()->blockRegisters();

        if (assignedTargetRegister == NULL) {
            assignedTargetRegister = assignGPRegister(this, getTargetRegister(), requestedRegSize, cg());
        } else if (requestedRegSize == TR_ByteReg) {
            assignedTargetRegister = assign8BitGPRegister(this, getTargetRegister(), cg());
        }

        getSource2ndRegister()->unblock();
        getTargetRegister()->block();

        assignedSource2ndRegister = getSource2ndRegister()->getAssignedRealRegister();
        if (assignedSource2ndRegister == NULL) {
            assignedSource2ndRegister = assignGPRegister(this, getSource2ndRegister(), requestedRegSize, cg());
        }

        getTargetRegister()->unblock();
        getMemoryReference()->unblockRegisters();

        if (assignedTargetRegister != NULL) {
            assignedTargetRegister->block();
            assignedSource2ndRegister->block();
            getMemoryReference()->assignRegisters(this, cg());
            assignedTargetRegister->unblock();
            assignedSource2ndRegister->unblock();

            if (getTargetRegister()->decFutureUseCount() == 0
                && assignedTargetRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(getTargetRegister(), assignedTargetRegister);
                getTargetRegister()->setAssignedRegister(NULL);
                assignedTargetRegister->setState(TR::RealRegister::Unlatched);
            }

            if (getSource2ndRegister()->decFutureUseCount() == 0
                && assignedSource2ndRegister->getState() != TR::RealRegister::Locked) {
                cg()->traceRegFreed(getSource2ndRegister(), assignedSource2ndRegister);
                getSource2ndRegister()->setAssignedRegister(NULL);
                assignedSource2ndRegister->setState(TR::RealRegister::Unlatched);
            }
            setTargetRegister(assignedTargetRegister);
            setSource2ndRegister(assignedSource2ndRegister);
        } else {
            getMemoryReference()->assignRegisters(this, cg());
        }
    }

    if (getDependencyConditions()) {
        getMemoryReference()->blockRegisters();
        getTargetRegister()->block();
        getSource2ndRegister()->block();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getMemoryReference()->unblockRegisters();
        getTargetRegister()->unblock();
        getSource2ndRegister()->unblock();
    }
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86RegMaskMemInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

bool TR::X86RegMaskMemInstruction::refsRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getTargetRegister() || reg == getSource2ndRegister()
        || reg == getMaskRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->refsRegister(reg);
    }

    return false;
}

bool TR::X86RegMaskMemInstruction::usesRegister(TR::Register *reg)
{
    if (getMemoryReference()->refsRegister(reg) || reg == getTargetRegister() || reg == getSource2ndRegister()
        || reg == getMaskRegister()) {
        return true;
    } else if (getDependencyConditions()) {
        return getDependencyConditions()->usesRegister(reg);
    }

    return false;
}

void TR::X86RegMaskMemInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getMaskRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPostConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getMaskRegister()->unblock();
        getMemoryReference()->unblockRegisters();
    }

    if (kindsToBeAssigned & getMaskRegister()->getKindAsMask()) {
        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }

        TR::Register *maskRegister = getMaskRegister();
        TR::RealRegister *assignedMaskRegister = maskRegister->getAssignedRealRegister();

        if (assignedMaskRegister == NULL) {
            assignedMaskRegister = assignGPRegister(this, maskRegister, TR_QuadWordReg, cg());
        }

        if (maskRegister->decFutureUseCount() == 0 && assignedMaskRegister->getState() != TR::RealRegister::Locked
            && maskRegister == getMaskRegister()) {
            cg()->traceRegFreed(maskRegister, assignedMaskRegister);
            maskRegister->setAssignedRegister(NULL);
            assignedMaskRegister->setState(TR::RealRegister::Unlatched);
        }

        setMaskRegister(assignedMaskRegister);

        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }
    }

    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        OMR::X86::Encoding encoding = getEncodingMethod();
        TR_RegisterSizes requestedRegSize = encoding == OMR::X86::EVEX_L512 ? TR_VectorReg512
            : encoding == OMR::X86::EVEX_L256                               ? TR_VectorReg256
                                                                            : TR_VectorReg128;

        if (getDependencyConditions()) {
            getDependencyConditions()->blockPreConditionRegisters();
            getDependencyConditions()->blockPostConditionRegisters();
        }
        getMemoryReference()->blockRegisters();

        TR::RealRegister *assignedRegister = getTargetRegister()->getAssignedRealRegister();

        if (assignedRegister == NULL) {
            assignedRegister = assignGPRegister(this, getTargetRegister(), requestedRegSize, cg());
        }

        getMemoryReference()->unblockRegisters();
        if (getDependencyConditions()) {
            getDependencyConditions()->unblockPreConditionRegisters();
            getDependencyConditions()->unblockPostConditionRegisters();
        }

        if (getTargetRegister()->decFutureUseCount() == 0 && assignedRegister->getState() != TR::RealRegister::Locked) {
            cg()->traceRegFreed(getTargetRegister(), assignedRegister);
            getTargetRegister()->setAssignedRegister(NULL);
            assignedRegister->setState(TR::RealRegister::Unlatched);
        }

        if (getDependencyConditions())
            getDependencyConditions()->blockPreConditionRegisters();

        setTargetRegister(assignedRegister);

        getTargetRegister()->block();
        getMemoryReference()->assignRegisters(this, cg());
        getTargetRegister()->unblock();

        if (getDependencyConditions())
            getDependencyConditions()->unblockPreConditionRegisters();
    }

#ifdef J9_PROJECT_SPECIFIC
    if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask)) {
        TR::UnresolvedDataSnippet *snippet = getMemoryReference()->getUnresolvedDataSnippet();
        if (snippet) {
            if (kindsToBeAssigned & (TR_FPR_Mask | TR_VRF_Mask))
                snippet->setHasLiveXMMRegisters((cg()->machine()->fpGetNumberOfLiveXMMRs() > 0) ? true : false);
        }
    }
#endif

    if (getDependencyConditions()) {
        getTargetRegister()->block();
        getMemoryReference()->blockRegisters();
        getDependencyConditions()->assignPreConditionRegisters(this, kindsToBeAssigned, cg());
        getTargetRegister()->unblock();
        getMemoryReference()->unblockRegisters();
    }
}

OP::Mnemonic getBranchOrSetOpCodeForFPComparison(TR::ILOpCodes cmpOp)
{
    OP::Mnemonic op;

    switch (cmpOp) {
        case TR::iffcmpeq:
        case TR::ifdcmpeq:
            op = OP::JE4;
            break;

        case TR::fcmpeq:
        case TR::dcmpeq:
            op = OP::SETE1Reg;
            break;

        case TR::iffcmpneu:
        case TR::ifdcmpneu:
        case TR::iffcmpne:
        case TR::ifdcmpne:
            op = OP::JNE4;
            break;

        case TR::fcmpneu:
        case TR::dcmpneu:
        case TR::fcmpne:
        case TR::dcmpne:
            op = OP::SETNE1Reg;
            break;

        case TR::iffcmpleu:
        case TR::ifdcmpleu:
            op = OP::JBE4;
            break;

        case TR::fcmpleu:
        case TR::dcmpleu:
            op = OP::SETBE1Reg;
            break;

        case TR::iffcmpgt:
        case TR::ifdcmpgt:
            op = OP::JA4;
            break;

        case TR::fcmpgt:
        case TR::dcmpgt:
            op = OP::SETA1Reg;
            break;

        case TR::iffcmplt:
        case TR::ifdcmplt:
        case TR::iffcmpltu:
        case TR::ifdcmpltu:
            op = OP::JB4;
            break;

        case TR::fcmplt:
        case TR::dcmplt:
        case TR::fcmpltu:
        case TR::dcmpltu:
            op = OP::SETB1Reg;
            break;

        case TR::iffcmpge:
        case TR::ifdcmpge:
        case TR::iffcmpgeu:
        case TR::ifdcmpgeu:
            op = OP::JAE4;
            break;

        case TR::fcmpge:
        case TR::dcmpge:
        case TR::fcmpgeu:
        case TR::dcmpgeu:
            op = OP::SETAE1Reg;
            break;

#ifdef DEBUG
        case TR::fcmpl:
        case TR::dcmpl:
        case TR::fcmpg:
        case TR::dcmpg:
            TR_ASSERT(0, "getBranchOrSetOpCodeForFPComparison() ==> comparison op is neither branch nor set\n");
            break;

        case TR::iffcmple:
        case TR::ifdcmple:
        case TR::fcmple:
        case TR::dcmple:
        case TR::iffcmpgtu:
        case TR::ifdcmpgtu:
        case TR::fcmpgtu:
        case TR::dcmpgtu:
            TR_ASSERT(0, "getBranchOrSetOpCodeForFPComparison() ==> comparison op should have been replaced\n");
            break;

#endif /* DEBUG */
        default:
            TR_ASSERT(0, "getBranchOrSetOpCodeForFPComparison() ==> invalid comparison op: %d\n", cmpOp);
    }

    return op;
}

////////////////////////////////////////////////////////////////////////////////
// TR_IA32VFPxxx member functions
////////////////////////////////////////////////////////////////////////////////

void TR::X86VFPDedicateInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (kindsToBeAssigned & getTargetRegister()->getKindAsMask()) {
        // Going backward, the DedicateInstruction marks the point where the
        // register can safely be used again.
        //
        toRealRegister(getTargetRegister())->resetState(TR::RealRegister::Free);
    }

    // No actual register assignment is necessary here because _targetRegister
    // is already a real register, and _memoryReference is [vfp+0]
}

void TR::X86VFPReleaseInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (kindsToBeAssigned & _dedicateInstruction->getTargetRegister()->getKindAsMask()) {
        // Going backward, the ReleaseInstruction marks the point where the
        // register must be reserved for the frame pointer.
        //
        // Note that it's ok to assume _dedicateInstruction->getTargetRegister()
        // is a real register, even though register assignment hasn't reached
        // that instruction yet, because it was a real register in the first place.
        //
        toRealRegister(_dedicateInstruction->getTargetRegister())->setState(TR::RealRegister::Locked);
    }
}

TR_AtomicRegion TR::X86PatchableCodeAlignmentInstruction::spinLoopAtomicRegions[] = {
    { 0x00, 2 }, // Spin loops just need two atomically-patchable bytes
    {    0, 0 }
};

TR_AtomicRegion TR::X86PatchableCodeAlignmentInstruction::CALLImm4AtomicRegions[] = {
    // We could use 4 here to align just the displacement, but instead we align
    // the whole instruction for performance reasons.
    //
    { 0x0, 5 },
    {   0, 0 }
};

TR_AtomicRegion TR::X86BoundaryAvoidanceInstruction::unresolvedAtomicRegions[] = {
    { 0x00, 8 }, // The first 8 bytes of an unresolved reference must be atomic
    {    0, 0 }
};

void TR::X86BoundaryAvoidanceInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    if (_targetCode != NULL && _targetCode != getNext()) {
        // Someone has inserted code between the padding and the instruction
        // needing alignment, so move them back together make sure they are
        // adjacent.
        //
        move(_targetCode->getPrev());
    }
}

int32_t TR::X86BoundaryAvoidanceInstruction::betterPadLength(int32_t oldPadLength,
    const TR_AtomicRegion *unaccommodatedRegion, int32_t unaccommodatedRegionStart)
{
    return oldPadLength + (int32_t)cg()->alignment(unaccommodatedRegionStart, _boundarySpacing); // cast explicitly
}

////////////////////////////////////////////////////////////////////////////////
// TR::AMD64RegImm64SymInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////

TR::AMD64RegImm64SymInstruction::AMD64RegImm64SymInstruction(OP::Mnemonic op, TR::Node *node, TR::Register *reg,
    uint64_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::AMD64RegImm64Instruction(imm, reg, node, op, cg)
    , _symbolReference(sr)
{
    autoSetReloKind();
}

TR::AMD64RegImm64SymInstruction::AMD64RegImm64SymInstruction(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *reg, uint64_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
    : TR::AMD64RegImm64Instruction(imm, reg, op, precedingInstruction, cg)
    , _symbolReference(sr)
{
    autoSetReloKind();
}

void TR::AMD64RegImm64SymInstruction::autoSetReloKind()
{
    TR::Symbol *symbol = getSymbolReference()->getSymbol();
    if (symbol->isDebugCounter())
        setReloKind(TR_DebugCounter);
    else if (symbol->isConst() || symbol->isConstantPoolAddress())
        setReloKind(TR_ConstantPool);
    else if (symbol->isStatic() && !getSymbolReference()->isUnresolved() && !symbol->isClassObject()
        && !symbol->isNotDataAddress())
        setReloKind(TR_DataAddress);
    else if (symbol->isBlockFrequency())
        setReloKind(TR_BlockFrequency);
    else if (symbol->isRecompQueuedFlag())
        setReloKind(TR_RecompQueuedFlag);
    else if (symbol->isEnterEventHookAddress() || symbol->isExitEventHookAddress())
        setReloKind(TR_MethodEnterExitHookAddress);
    else if (cg()->comp()->compileRelocatableCode() && symbol->isCallSiteTableEntry()
        && !getSymbolReference()->isUnresolved())
        setReloKind(TR_CallsiteTableEntryAddress);
    else if (cg()->comp()->compileRelocatableCode() && symbol->isMethodTypeTableEntry()
        && !getSymbolReference()->isUnresolved())
        setReloKind(TR_MethodTypeTableEntryAddress);
    else
        setReloKind(-1);
}

////////////////////////////////////////////////////////////////////////////////
// TR::X86VFPCallCleanupInstruction
////////////////////////////////////////////////////////////////////////////////

void TR::X86VFPCallCleanupInstruction::assignRegisters(TR_RegisterKinds kindsToBeAssigned)
{
    OMR::X86::Instruction::assignRegisters(kindsToBeAssigned);
}

#ifdef J9_PROJECT_SPECIFIC
////////////////////////////////////////////////////////////////////////////////
// TR::X86VirtualGuardNOPInstruction:: member functions
////////////////////////////////////////////////////////////////////////////////
///
bool TR::X86VirtualGuardNOPInstruction::usesRegister(TR::Register *reg)
{
    if (_nopSize > 0 && cg()->machine()->getRealRegister(_register) == reg)
        return true;
    if (getDependencyConditions())
        return getDependencyConditions()->usesRegister(reg);
    return false;
}

bool TR::X86VirtualGuardNOPInstruction::defsRegister(TR::Register *reg) { return usesRegister(reg); }

bool TR::X86VirtualGuardNOPInstruction::refsRegister(TR::Register *reg) { return usesRegister(reg); }
#endif

////////////////////////////////////////////////////////////////////////////////
// Factory functions
////////////////////////////////////////////////////////////////////////////////

namespace OMR { namespace X86 {

// X86Instruction
//
TR::Instruction *Inst(TR::Instruction *prev, OP::Mnemonic op, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::Instruction(op, prev, cg, encoding);
}

TR::Instruction *Inst(OP::Mnemonic op, TR::Node *node, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::Instruction(node, op, cg, encoding);
}

TR::Instruction *Inst(OP::Mnemonic op, TR::Node *node, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::Instruction(cond, node, op, cg, encoding);
}

// X86NoOperandInstruction
//
TR::X86NoOperandInstruction *Inst0(TR::Instruction *prev, OP::Mnemonic op, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86NoOperandInstruction(prev, op, cg);
}

TR::X86NoOperandInstruction *Inst0(OP::Mnemonic op, TR::Node *node, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86NoOperandInstruction(op, node, cg);
}

TR::X86NoOperandInstruction *Inst0(OP::Mnemonic op, TR::Node *node, TR::RegisterDependencyConditions *cond,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86NoOperandInstruction(op, node, cond, cg);
}

TR::X86NoOperandInstruction *Inst0(TR::Instruction *prev, OP::Mnemonic op, TR::RegisterDependencyConditions *cond,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86NoOperandInstruction(prev, op, cond, cg);
}

// X86AlignmentInstruction
//
TR::X86AlignmentInstruction *Inst_Alignment(TR::Node *node, uint8_t boundary, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86AlignmentInstruction(node, boundary, cg);
}

TR::X86AlignmentInstruction *Inst_Alignment(TR::Node *node, uint8_t boundary, uint8_t margin, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86AlignmentInstruction(node, boundary, margin, cg);
}

TR::X86AlignmentInstruction *Inst_Alignment(TR::Instruction *precedingInstruction, uint8_t boundary,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86AlignmentInstruction(precedingInstruction, boundary, cg);
}

TR::X86AlignmentInstruction *Inst_Alignment(TR::Instruction *precedingInstruction, uint8_t boundary, uint8_t margin,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86AlignmentInstruction(precedingInstruction, boundary, margin, cg);
}

// X86BoundaryAvoidanceInstruction
//
TR::X86BoundaryAvoidanceInstruction *Inst_BoundaryAvoidance(const TR_AtomicRegion *atomicRegions,
    uint8_t boundarySpacing, uint8_t maxPadding, TR::Instruction *targetCode, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory())
        TR::X86BoundaryAvoidanceInstruction(atomicRegions, boundarySpacing, maxPadding, targetCode, cg);
}

// X86CallMemInstruction
//
TR::X86CallMemInstruction *Inst_CallMem(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86CallMemInstruction(op, node, mr, cond, cg);
}

TR::X86CallMemInstruction *Inst_CallMem(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86CallMemInstruction(op, node, mr, cg);
}

TR::X86CallMemInstruction *Inst_CallMem(TR::Instruction *prevInstr, OP::Mnemonic op, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86CallMemInstruction(prevInstr, op, mr, cond, cg);
}

// X86ConditionalJump
//
TR::X86LabelInstruction *Inst_ConditionalJump(OP::Mnemonic opCode, TR::Node *ifNode, TR::CodeGenerator *cg)
{
    TR::X86LabelInstruction *inst;
    TR::LabelSymbol *destinationLabel = ifNode->getBranchDestination()->getNode()->getLabel();

    if (ifNode->getNumChildren() == 3) {
        TR::Node *glRegDep = ifNode->getChild(2);
        inst = Inst_Label(opCode, ifNode, destinationLabel, glRegDep, true, cg);
    } else {
        inst = Inst_Label(opCode, ifNode, destinationLabel, cg);
    }

    return inst;
}

// X86FenceInstruction
//
TR::X86FenceInstruction *Inst_Fence(OP::Mnemonic op, TR::Node *node, TR::Node *fenceNode, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86FenceInstruction(op, node, fenceNode, cg);
}

// X86ImmInstruction
//
TR::X86ImmInstruction *Inst_Imm(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::RegisterDependencyConditions *cond,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmInstruction(op, node, imm, cond, cg);
}

TR::X86ImmInstruction *Inst_Imm(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::X86ImmInstruction(op, node, imm, cg, reloKind);
}

TR::X86ImmInstruction *Inst_Imm(TR::Instruction *prev, OP::Mnemonic op, int32_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmInstruction(prev, op, imm, cond, cg);
}

TR::X86ImmInstruction *Inst_Imm(TR::Instruction *prev, OP::Mnemonic op, int32_t imm, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmInstruction(prev, op, imm, cg);
}

// X86HelperCallInstruction
//
TR::X86ImmSymInstruction *Inst_HelperCall(TR::Instruction *cursor, TR_RuntimeHelper index, TR::CodeGenerator *cg)
{
    TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(index);
    cg->resetIsLeafMethod();
    return new (cg->trHeapMemory()) TR::X86ImmSymInstruction(cursor, OP::CALLImm4,
        static_cast<int32_t>(reinterpret_cast<intptr_t>(helperSymRef->getMethodAddress())), helperSymRef, cg);
}

TR::X86ImmSymInstruction *Inst_HelperCall(TR::Node *node, TR_RuntimeHelper index,
    TR::RegisterDependencyConditions *dependencies, TR::CodeGenerator *cg)
{
    TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(index);
    cg->resetIsLeafMethod();
    return Inst_ImmSym(OP::CALLImm4, node,
        static_cast<int32_t>(reinterpret_cast<intptr_t>(helperSymRef->getMethodAddress())), helperSymRef, dependencies,
        cg);
}

// AMD64Imm64Instruction
//
TR::AMD64Imm64Instruction *Inst_Imm64(OP::Mnemonic op, TR::Node *node, uint64_t imm, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64Instruction(op, node, imm, cg);
}

TR::AMD64Imm64Instruction *Inst_Imm64(TR::Instruction *precedingInstruction, OP::Mnemonic op, uint64_t imm,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64Instruction(precedingInstruction, op, imm, cg);
}

TR::AMD64Imm64Instruction *Inst_Imm64(OP::Mnemonic op, TR::Node *node, uint64_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64Instruction(op, node, imm, cond, cg);
}

TR::AMD64Imm64Instruction *Inst_Imm64(TR::Instruction *precedingInstruction, OP::Mnemonic op, uint64_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64Instruction(precedingInstruction, op, imm, cond, cg);
}

// AMD64Imm64SymInstruction
//
TR::AMD64Imm64SymInstruction *Inst_Imm64Sym(OP::Mnemonic op, TR::Node *node, uint64_t imm, TR::SymbolReference *sr,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64SymInstruction(op, node, imm, sr, cg);
}

TR::AMD64Imm64SymInstruction *Inst_Imm64Sym(TR::Instruction *precedingInstruction, OP::Mnemonic op, uint64_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64SymInstruction(precedingInstruction, op, imm, sr, cg);
}

TR::AMD64Imm64SymInstruction *Inst_Imm64Sym(OP::Mnemonic op, TR::Node *node, uint64_t imm, TR::SymbolReference *sr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64SymInstruction(op, node, imm, sr, cond, cg);
}

TR::AMD64Imm64SymInstruction *Inst_Imm64Sym(TR::Instruction *precedingInstruction, OP::Mnemonic op, uint64_t imm,
    TR::SymbolReference *sr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64Imm64SymInstruction(precedingInstruction, op, imm, sr, cond, cg);
}

// X86ImmSnippetInstruction
//
TR::X86ImmSnippetInstruction *Inst_ImmSnippet(OP::Mnemonic op, TR::Node *node, int32_t imm,
    TR::UnresolvedDataSnippet *snippet, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmSnippetInstruction(op, node, imm, snippet, cg);
}

// X86ImmSymInstruction
//
TR::X86ImmSymInstruction *Inst_ImmSym(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::SymbolReference *sr,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmSymInstruction(op, node, imm, sr, cg);
}

TR::X86ImmSymInstruction *Inst_ImmSym(OP::Mnemonic op, TR::Node *node, int32_t imm, TR::SymbolReference *sr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmSymInstruction(op, node, imm, sr, cond, cg);
}

TR::X86ImmSymInstruction *Inst_ImmSym(TR::Instruction *prev, OP::Mnemonic op, int32_t imm, TR::SymbolReference *sr,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmSymInstruction(prev, op, imm, sr, cg);
}

TR::X86ImmSymInstruction *Inst_ImmSym(TR::Instruction *prev, OP::Mnemonic op, int32_t imm, TR::SymbolReference *sr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86ImmSymInstruction(prev, op, imm, sr, cond, cg);
}

// X86JumpInstruction
//
TR::X86LabelInstruction *Inst_Jump(OP::Mnemonic opCode, TR::Node *jumpNode, TR::CodeGenerator *cg,
    bool evaluateGlRegDeps)
{
    TR::LabelSymbol *destinationLabel = jumpNode->getBranchDestination()->getNode()->getLabel();
    TR::X86LabelInstruction *inst;

    (jumpNode->getNumChildren() > 0)
        ? inst = Inst_Label(opCode, jumpNode, destinationLabel, jumpNode->getFirstChild(), evaluateGlRegDeps, cg)
        : inst = Inst_Label(opCode, jumpNode, destinationLabel, cg);

    return inst;
}

// X86LabelInstruction
//
TR::X86LabelInstruction *Inst_Label(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86LabelInstruction(op, node, sym, cg);
}

TR::X86LabelInstruction *Inst_Label(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86LabelInstruction(op, node, sym, cond, cg);
}

TR::X86LabelInstruction *Inst_Label(TR::Instruction *prev, OP::Mnemonic op, TR::LabelSymbol *sym, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86LabelInstruction(prev, op, sym, cg);
}

TR::X86LabelInstruction *Inst_Label(TR::Instruction *prev, OP::Mnemonic op, TR::LabelSymbol *sym,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86LabelInstruction(prev, op, sym, cond, cg);
}

TR::X86LabelInstruction *Inst_Label(OP::Mnemonic opCode, TR::Node *node, TR::LabelSymbol *label, TR::Node *glRegDep,
    bool evaluateGlRegDeps, TR::CodeGenerator *cg)
{
    if (evaluateGlRegDeps) {
        cg->evaluate(glRegDep);
    }

    TR::X86LabelInstruction *instr = Inst_Label(opCode, node, label, RegDeps(glRegDep, cg, 0), cg);

    return instr;
}

// X86LongLabelInstruction
//
TR::X86LabelInstruction *Inst_LongLabel(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym, TR::CodeGenerator *cg)
{
    TR::X86LabelInstruction *toReturn = new (cg->trHeapMemory()) TR::X86LabelInstruction(op, node, sym, cg);
    toReturn->prohibitShortening();
    return toReturn;
}

TR::X86LabelInstruction *Inst_LongLabel(OP::Mnemonic op, TR::Node *node, TR::LabelSymbol *sym,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    TR::X86LabelInstruction *toReturn = new (cg->trHeapMemory()) TR::X86LabelInstruction(op, node, sym, cond, cg);
    toReturn->prohibitShortening();
    return toReturn;
}

// X86MemInstruction
//
TR::X86MemInstruction *Inst_Mem(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemInstruction(op, node, mr, cg);
}

TR::X86MemInstruction *Inst_Mem(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::MemoryReference *mr,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemInstruction(precedingInstruction, op, mr, cg);
}

TR::X86MemInstruction *Inst_Mem(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemInstruction(op, node, mr, cond, cg);
}

// X86MemImmInstruction
//
TR::X86MemImmInstruction *Inst_MemImm(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::X86MemImmInstruction(op, node, mr, imm, cg, reloKind);
}

TR::X86MemImmInstruction *Inst_MemImm(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::MemoryReference *mr,
    int32_t imm, TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::X86MemImmInstruction(precedingInstruction, op, mr, imm, cg, reloKind);
}

// X86MemImmSymInstruction
//
TR::X86MemImmSymInstruction *Inst_MemImmSym(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, int32_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemImmSymInstruction(op, node, mr, imm, sr, cg);
}

// X86MemMaskInstruction
//
TR::X86MemMaskRegInstruction *Inst_MemMaskReg(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::Register *mreg, TR::Register *sreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding, bool zeroMask)
{
    return new (cg->trHeapMemory()) TR::X86MemMaskRegInstruction(op, node, mr, mreg, sreg, cg, encoding, zeroMask);
}

TR::X86MemMaskRegInstruction *Inst_MemMaskReg(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr,
    TR::Register *mreg, TR::Register *sreg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding, bool zeroMask)
{
    return new (cg->trHeapMemory())
        TR::X86MemMaskRegInstruction(op, node, mr, mreg, sreg, cond, cg, encoding, zeroMask);
}

// X86MemRegInstruction
//
TR::X86MemRegInstruction *Inst_MemReg(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, TR::Register *sreg,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86MemRegInstruction(op, node, mr, sreg, cg, encoding);
}

TR::X86MemRegInstruction *Inst_MemReg(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, TR::Register *sreg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86MemRegInstruction(op, node, mr, sreg, cond, cg, encoding);
}

TR::X86MemRegInstruction *Inst_MemReg(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::MemoryReference *mr,
    TR::Register *sreg, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86MemRegInstruction(precedingInstruction, op, mr, sreg, cg, encoding);
}

// X86MemTableInstruction
//
TR::X86MemTableInstruction *Inst_MemTable(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemTableInstruction(op, node, mr, numEntries, cg);
}

TR::X86MemTableInstruction *Inst_MemTable(OP::Mnemonic op, TR::Node *node, TR::MemoryReference *mr, ncount_t numEntries,
    TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86MemTableInstruction(op, node, mr, numEntries, deps, cg);
}

// X86PaddingInstruction
//
TR::X86PaddingInstruction *Inst_Padding(uint8_t length, TR::Node *node, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PaddingInstruction(length, node, cg);
}

TR::X86PaddingInstruction *Inst_Padding(TR::Instruction *precedingInstruction, uint8_t length, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PaddingInstruction(precedingInstruction, length, cg);
}

TR::X86PaddingInstruction *Inst_Padding(TR::Instruction *precedingInstruction, uint8_t length,
    TR_PaddingProperties properties, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PaddingInstruction(precedingInstruction, length, properties, cg);
}

// X86PaddingSnippetInstruction
//
TR::X86PaddingSnippetInstruction *Inst_PaddingSnippet(uint8_t length, TR::Node *node, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PaddingSnippetInstruction(length, node, cg);
}

// X86PatchableCodeAlignmentInstruction
//
TR::X86PatchableCodeAlignmentInstruction *Inst_PatchableCodeAlignment(const TR_AtomicRegion *atomicRegions,
    TR::Instruction *patchableCode, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PatchableCodeAlignmentInstruction(atomicRegions, patchableCode, cg);
}

TR::X86PatchableCodeAlignmentInstruction *Inst_PatchableCodeAlignment(TR::Instruction *prev,
    const TR_AtomicRegion *atomicRegions, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86PatchableCodeAlignmentInstruction(prev, atomicRegions, cg);
}

// X86PatchableCodeAlignmentWithProtectiveNop
//
TR::X86PatchableCodeAlignmentInstruction *Inst_PatchableCodeAlignmentWithProtectiveNop(
    const TR_AtomicRegion *atomicRegions, TR::Instruction *patchableCode, int32_t sizeOfProtectiveNop,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory())
        TR::X86PatchableCodeAlignmentInstruction(atomicRegions, patchableCode, sizeOfProtectiveNop, cg);
}

// X86RegInstruction
//
TR::X86RegInstruction *Inst_Reg(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegInstruction(op, node, treg, cg);
}

TR::X86RegInstruction *Inst_Reg(OP::Mnemonic op, TR::Node *node, TR::Register *treg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegInstruction(op, node, treg, cond, cg);
}

TR::X86RegInstruction *Inst_Reg(TR::Instruction *prev, OP::Mnemonic op, TR::Register *reg1, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegInstruction(reg1, op, prev, cg);
}

TR::X86RegInstruction *Inst_Reg(TR::Instruction *prev, OP::Mnemonic op, TR::Register *reg1,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegInstruction(cond, reg1, op, prev, cg);
}

// X86RegImmInstruction
//
TR::X86RegImmInstruction *Inst_RegImm(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegImmInstruction(op, node, treg, imm, cond, cg);
}

TR::X86RegImmInstruction *Inst_RegImm(TR::Instruction *instr, OP::Mnemonic op, TR::Register *treg, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::X86RegImmInstruction(instr, op, treg, imm, cg, reloKind);
}

TR::X86RegImmInstruction *Inst_RegImm(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::X86RegImmInstruction(op, node, treg, imm, cg, reloKind);
}

TR::X86RegImmInstruction *Inst_RegImm(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::CodeGenerator *cg, int32_t reloKind, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegImmInstruction(op, node, treg, imm, cg, encoding, reloKind);
}

// X86RegImmSymInstruction
//
TR::X86RegImmSymInstruction *Inst_RegImmSym(OP::Mnemonic op, TR::Node *node, TR::Register *treg, int32_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86RegImmSymInstruction(op, node, treg, imm, sr, cg);
}

// AMD64RegImm64Instruction
//
TR::AMD64RegImm64Instruction *Inst_RegImm64(OP::Mnemonic op, TR::Node *node, TR::Register *treg, uint64_t imm,
    TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::AMD64RegImm64Instruction(op, node, treg, imm, cg, reloKind);
}

TR::AMD64RegImm64Instruction *Inst_RegImm64(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::Register *treg,
    uint64_t imm, TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::AMD64RegImm64Instruction(precedingInstruction, op, treg, imm, cg, reloKind);
}

TR::AMD64RegImm64Instruction *Inst_RegImm64(OP::Mnemonic op, TR::Node *node, TR::Register *treg, uint64_t imm,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory()) TR::AMD64RegImm64Instruction(op, node, treg, imm, cond, cg, reloKind);
}

TR::AMD64RegImm64Instruction *Inst_RegImm64(TR::Instruction *precedingInstruction, OP::Mnemonic op, TR::Register *treg,
    uint64_t imm, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, int32_t reloKind)
{
    return new (cg->trHeapMemory())
        TR::AMD64RegImm64Instruction(precedingInstruction, op, treg, imm, cond, cg, reloKind);
}

// AMD64RegImm64SymInstruction
//
TR::AMD64RegImm64SymInstruction *Inst_RegImm64Sym(OP::Mnemonic op, TR::Node *node, TR::Register *reg, uint64_t imm,
    TR::SymbolReference *sr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64RegImm64SymInstruction(op, node, reg, imm, sr, cg);
}

TR::AMD64RegImm64SymInstruction *Inst_RegImm64Sym(TR::Instruction *precedingInstruction, OP::Mnemonic op,
    TR::Register *reg, uint64_t imm, TR::SymbolReference *sr, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::AMD64RegImm64SymInstruction(precedingInstruction, op, reg, imm, sr, cg);
}

// X86RegMaskMemInstruction
//
TR::X86RegMaskMemInstruction *Inst_RegMaskMem(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *mreg,
    TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding, bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory()) TR::X86RegMaskMemInstruction(op, node, reg1, mreg, mr, cg, encoding, zeroMask);
}

TR::X86RegMaskMemInstruction *Inst_RegMaskMem(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *mreg,
    TR::MemoryReference *mr, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg, OMR::X86::Encoding encoding,
    bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskMemInstruction(op, node, reg1, mreg, mr, deps, cg, encoding, zeroMask);
}

// X86RegMaskRegInstruction
//
TR::X86RegMaskRegInstruction *Inst_RegMaskReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *mreg,
    TR::Register *reg2, TR::CodeGenerator *cg, OMR::X86::Encoding encoding, bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory()) TR::X86RegMaskRegInstruction(reg1, mreg, reg2, node, op, cg, encoding, zeroMask);
}

TR::X86RegMaskRegInstruction *Inst_RegMaskReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *mreg,
    TR::Register *reg2, TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg, OMR::X86::Encoding encoding,
    bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskRegInstruction(reg1, mreg, reg2, node, op, deps, cg, encoding, zeroMask);
}

// X86RegMaskRegRegInstruction
//
TR::X86RegMaskRegRegInstruction *Inst_RegMaskRegReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1,
    TR::Register *mreg, TR::Register *reg2, TR::Register *reg3, TR::CodeGenerator *cg, OMR::X86::Encoding encoding,
    bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskRegRegInstruction(reg1, mreg, reg2, reg3, node, op, cg, encoding, zeroMask);
}

TR::X86RegMaskRegRegInstruction *Inst_RegMaskRegReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1,
    TR::Register *mreg, TR::Register *reg2, TR::Register *reg3, TR::RegisterDependencyConditions *deps,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding, bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskRegRegInstruction(reg1, mreg, reg2, reg3, node, op, deps, cg, encoding, zeroMask);
}

// X86RegMaskRegRegImmInstruction
//
TR::X86RegMaskRegRegImmInstruction *Inst_RegMaskRegRegImm(OP::Mnemonic op, TR::Node *node, TR::Register *reg1,
    TR::Register *mreg, TR::Register *reg2, TR::Register *reg3, int32_t imm, TR::CodeGenerator *cg,
    OMR::X86::Encoding encoding, bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskRegRegImmInstruction(reg1, mreg, reg2, reg3, imm, node, op, cg, encoding, zeroMask);
}

TR::X86RegMaskRegRegImmInstruction *Inst_RegMaskRegRegImm(OP::Mnemonic op, TR::Node *node, TR::Register *reg1,
    TR::Register *mreg, TR::Register *reg2, TR::Register *reg3, int32_t imm, TR::RegisterDependencyConditions *deps,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding, bool zeroMask)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Bad && encoding >= OMR::X86::Encoding::EVEX_L128,
        "Must use EVEX encoding for AVX-512 instructions");
    TR_ASSERT_FATAL(mreg->getKind() == TR_VMR, "Mask register must be a VMR");

    return new (cg->trHeapMemory())
        TR::X86RegMaskRegRegImmInstruction(reg1, mreg, reg2, reg3, imm, node, op, deps, cg, encoding, zeroMask);
}

// X86RegMemInstruction
//
TR::X86RegMemInstruction *Inst_RegMem(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::MemoryReference *mr,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegMemInstruction(op, node, treg, mr, cg, encoding);
}

TR::X86RegMemInstruction *Inst_RegMem(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::MemoryReference *mr,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegMemInstruction(op, node, treg, mr, cond, cg, encoding);
}

TR::X86RegMemInstruction *Inst_RegMem(TR::Instruction *instr, OP::Mnemonic op, TR::Register *treg,
    TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegMemInstruction(instr, op, treg, mr, cg, encoding);
}

// X86RegMemImmInstruction
//
TR::X86RegMemImmInstruction *Inst_RegMemImm(OP::Mnemonic op, TR::Node *node, TR::Register *reg, TR::MemoryReference *mr,
    int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegMemImmInstruction(op, node, reg, mr, imm, cg, encoding);
}

// X86RegRegInstruction
//
TR::X86RegRegInstruction *Inst_RegReg(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::Register *sreg,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegRegInstruction(op, node, treg, sreg, cg, encoding);
}

TR::X86RegRegInstruction *Inst_RegReg(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::Register *sreg,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegRegInstruction(op, node, treg, sreg, cond, cg, encoding);
}

TR::X86RegRegInstruction *Inst_RegReg(TR::Instruction *instr, OP::Mnemonic op, TR::Register *treg, TR::Register *sreg,
    TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegRegInstruction(instr, op, treg, sreg, cg, encoding);
}

// X86RegRegImmInstruction
//
TR::X86RegRegImmInstruction *Inst_RegRegImm(OP::Mnemonic op, TR::Node *node, TR::Register *treg, TR::Register *sreg,
    int32_t imm, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    return new (cg->trHeapMemory()) TR::X86RegRegImmInstruction(op, node, treg, sreg, imm, cg, encoding);
}

// X86RegRegMemInstruction
//
TR::X86RegRegMemInstruction *Inst_RegRegMem(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *reg2,
    TR::MemoryReference *mr, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Legacy, "Cannot use legacy SSE encoding for 3-operand instruction");

    return new (cg->trHeapMemory()) TR::X86RegRegMemInstruction(op, node, reg1, reg2, mr, cg, encoding);
}

TR::X86RegRegMemInstruction *Inst_RegRegMem(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *reg2,
    TR::MemoryReference *mr, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Legacy, "Cannot use legacy SSE encoding for 3-operand instruction");

    return new (cg->trHeapMemory()) TR::X86RegRegMemInstruction(op, node, reg1, reg2, mr, cond, cg, encoding);
}

// X86RegRegRegInstruction
//
TR::X86RegRegRegInstruction *Inst_RegRegReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *reg2,
    TR::Register *reg3, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Legacy, "Cannot use legacy SSE encoding for 3-operand instruction");

    return new (cg->trHeapMemory()) TR::X86RegRegRegInstruction(op, node, reg1, reg2, reg3, cg, encoding);
}

TR::X86RegRegRegInstruction *Inst_RegRegReg(OP::Mnemonic op, TR::Node *node, TR::Register *reg1, TR::Register *reg2,
    TR::Register *reg3, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg, OMR::X86::Encoding encoding)
{
    TR_ASSERT_FATAL(encoding != OMR::X86::Legacy, "Cannot use legacy SSE encoding for 3-operand instruction");

    return new (cg->trHeapMemory()) TR::X86RegRegRegInstruction(op, node, reg1, reg2, reg3, cond, cg, encoding);
}

// X86VFPCallCleanupInstruction
//
TR::X86VFPCallCleanupInstruction *Inst_VFPCallCleanup(TR::Instruction *precedingInstruction, int32_t adjustment,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPCallCleanupInstruction(precedingInstruction, adjustment, cg);
}

TR::X86VFPCallCleanupInstruction *Inst_VFPCallCleanup(int32_t adjustment, TR::Node *node, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPCallCleanupInstruction(adjustment, node, cg);
}

// X86VFPDedicateInstruction
//
TR::X86VFPDedicateInstruction *Inst_VFPDedicate(TR::Instruction *precedingInstruction,
    TR::RealRegister *framePointerReg, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPDedicateInstruction(precedingInstruction, framePointerReg, cg);
}

TR::X86VFPDedicateInstruction *Inst_VFPDedicate(TR::RealRegister *framePointerReg, TR::Node *node,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPDedicateInstruction(framePointerReg, node, cg);
}

TR::X86VFPDedicateInstruction *Inst_VFPDedicate(TR::Instruction *precedingInstruction,
    TR::RealRegister *framePointerReg, TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPDedicateInstruction(precedingInstruction, framePointerReg, cond, cg);
}

TR::X86VFPDedicateInstruction *Inst_VFPDedicate(TR::RealRegister *framePointerReg, TR::Node *node,
    TR::RegisterDependencyConditions *cond, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPDedicateInstruction(framePointerReg, node, cond, cg);
}

// X86VFPReleaseInstruction
//
TR::X86VFPReleaseInstruction *Inst_VFPRelease(TR::Instruction *precedingInstruction,
    TR::X86VFPDedicateInstruction *dedicateInstruction, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPReleaseInstruction(precedingInstruction, dedicateInstruction, cg);
}

TR::X86VFPReleaseInstruction *Inst_VFPRelease(TR::X86VFPDedicateInstruction *dedicateInstruction, TR::Node *node,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPReleaseInstruction(dedicateInstruction, node, cg);
}

// X86VFPRestoreInstruction
//
TR::X86VFPRestoreInstruction *Inst_VFPRestore(TR::Instruction *precedingInstruction,
    TR::X86VFPSaveInstruction *saveInstruction, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPRestoreInstruction(precedingInstruction, saveInstruction, cg);
}

TR::X86VFPRestoreInstruction *Inst_VFPRestore(TR::X86VFPSaveInstruction *saveInstruction, TR::Node *node,
    TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPRestoreInstruction(saveInstruction, node, cg);
}

// X86VFPSaveInstruction
//
TR::X86VFPSaveInstruction *Inst_VFPSave(TR::Instruction *precedingInstruction, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPSaveInstruction(precedingInstruction, cg);
}

TR::X86VFPSaveInstruction *Inst_VFPSave(TR::Node *node, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VFPSaveInstruction(node, cg);
}

#ifdef J9_PROJECT_SPECIFIC
TR::X86VirtualGuardNOPInstruction *Inst_VirtualGuardNOP(TR::Node *node, TR_VirtualGuardSite *site,
    TR::RegisterDependencyConditions *deps, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VirtualGuardNOPInstruction(OP::vgnop, node, site, deps, cg);
}

TR::X86VirtualGuardNOPInstruction *Inst_VirtualGuardNOP(TR::Node *node, TR_VirtualGuardSite *site,
    TR::RegisterDependencyConditions *deps, TR::LabelSymbol *label, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VirtualGuardNOPInstruction(OP::vgnop, node, site, deps, cg, label);
}

TR::X86VirtualGuardNOPInstruction *Inst_VirtualGuardNOP(TR::Instruction *i, TR::Node *node, TR_VirtualGuardSite *site,
    TR::RegisterDependencyConditions *deps, TR::LabelSymbol *label, TR::CodeGenerator *cg)
{
    return new (cg->trHeapMemory()) TR::X86VirtualGuardNOPInstruction(i, OP::vgnop, node, site, deps, cg, label);
}
#endif

}} // namespace OMR::X86
