/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "codegen/LoadStoreHandler.hpp"

#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "il/Node_inlines.hpp"

void OMR::Power::LoadStoreHandler::generateComputeAddressSequence(TR::CodeGenerator *cg, TR::Register *addrReg, TR::Node *node, int64_t extraOffset)
    {
    TR_ASSERT_FATAL_WITH_NODE(
        node,
        node->getOpCode().isLoadAddr() || node->getOpCode().isLoadVar() || node->getOpCode().isStore(),
        "Attempt to use generateComputeAddressSequence for non-memory node"
    );

    auto ref = TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, 0, false, extraOffset);

    if (ref.getMemoryReference()->getIndexRegister())
        generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, addrReg, ref.getMemoryReference()->getBaseRegister(), ref.getMemoryReference()->getIndexRegister());
    else
        generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, addrReg, ref.getMemoryReference());

    ref.decReferenceCounts(cg);
    }

TR::MemoryReference *createAddressMemoryReference(TR::CodeGenerator *cg, TR::Register *addrReg, uint32_t length, bool requireIndexForm)
    {
    if (requireIndexForm)
        return TR::MemoryReference::createWithIndexReg(cg, NULL, addrReg, length);
    else
        return TR::MemoryReference::createWithDisplacement(cg, addrReg, 0, length);
    }

void OMR::Power::LoadStoreHandler::generateLoadAddressSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node, TR::Register *addrReg, TR::InstOpCode::Mnemonic loadOp, uint32_t length, bool requireIndexForm)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isLoadVar(), "Attempt to use generateLoadAddressSequence for non-load node");
    TR::LoadStoreHandlerImpl::generateLoadSequence(cg, trgReg, node, createAddressMemoryReference(cg, addrReg, length, requireIndexForm), loadOp);
    }

void OMR::Power::LoadStoreHandler::generatePairedLoadAddressSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node, TR::Register *addrReg)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isLoadVar(), "Attempt to use generatePairedLoadAddressSequence for non-load node");
    TR::LoadStoreHandlerImpl::generatePairedLoadSequence(cg, trgReg, node, createAddressMemoryReference(cg, addrReg, 8, false));
    }

void OMR::Power::LoadStoreHandler::generateStoreAddressSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node, TR::Register *addrReg, TR::InstOpCode::Mnemonic storeOp, uint32_t length, bool requireIndexForm)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isStore(), "Attempt to use generateStoreAddressSequence for non-store node");
    TR::LoadStoreHandlerImpl::generateStoreSequence(cg, srcReg, node, createAddressMemoryReference(cg, addrReg, length, requireIndexForm), storeOp);
    }

void OMR::Power::LoadStoreHandler::generatePairedStoreAddressSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node, TR::Register *addrReg)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isStore(), "Attempt to use generatePairedStoreAddressSequence for non-store node");
    TR::LoadStoreHandlerImpl::generatePairedStoreSequence(cg, srcReg, node, createAddressMemoryReference(cg, addrReg, 8, false));
    }

void OMR::Power::LoadStoreHandler::generateLoadNodeSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node, TR::InstOpCode::Mnemonic loadOp, uint32_t length, bool requireIndexForm, int64_t extraOffset)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isLoadVar(), "Attempt to use generateLoadNodeSequence for non-load node");

    auto ref = TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, length, requireIndexForm, extraOffset);

    TR::LoadStoreHandlerImpl::generateLoadSequence(cg, trgReg, node, ref.getMemoryReference(), loadOp);
    ref.decReferenceCounts(cg);
    }

void OMR::Power::LoadStoreHandler::generatePairedLoadNodeSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isLoadVar(), "Attempt to use generatePairedLoadNodeSequence for non-load node");

    auto ref = TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, 8, false, 0);

    TR::LoadStoreHandlerImpl::generatePairedLoadSequence(cg, trgReg, node, ref.getMemoryReference());
    ref.decReferenceCounts(cg);
    }

void OMR::Power::LoadStoreHandler::generateStoreNodeSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node, TR::InstOpCode::Mnemonic storeOp, uint32_t length, bool requireIndexForm, int64_t extraOffset)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isStore(), "Attempt to use generateStoreNodeSequence for non-store node");
    auto ref = TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, length, requireIndexForm, extraOffset);

    TR::LoadStoreHandlerImpl::generateStoreSequence(cg, srcReg, node, ref.getMemoryReference(), storeOp);
    ref.decReferenceCounts(cg);
    }

void OMR::Power::LoadStoreHandler::generatePairedStoreNodeSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, node->getOpCode().isStore(), "Attempt to use generatePairedStoreNodeSequence for non-store node");
    auto ref = TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, 8, false, 0);

    TR::LoadStoreHandlerImpl::generatePairedStoreSequence(cg, srcReg, node, ref.getMemoryReference());
    ref.decReferenceCounts(cg);
    }

bool OMR::Power::LoadStoreHandler::isSimpleLoad(TR::CodeGenerator *cg, TR::Node *node)
    {
    if (!node->getOpCode().isLoad())
        return false;

#ifdef J9_PROJECT_SPECIFIC
    if (node->getSymbolReference()->isUnresolved())
        return false;

    if (node->getSymbol()->isAtLeastOrStrongerThanAcquireRelease() && cg->comp()->target().isSMP())
        return false;
#endif

    return cg->comp()->target().is64Bit() || node->getOpCode().getDataType() != TR::Int64;
    }

OMR::Power::NodeMemoryReference OMR::Power::LoadStoreHandler::generateSimpleLoadMemoryReference(TR::CodeGenerator *cg, TR::Node *node, uint32_t length, bool requireIndexForm, int64_t extraOffset)
    {
    TR_ASSERT_FATAL_WITH_NODE(node, TR::LoadStoreHandler::isSimpleLoad(cg, node), "Attempt to use generateSimpleLoadMemoryReference for a node which is not a simple load");
    return TR::LoadStoreHandlerImpl::generateMemoryReference(cg, node, length, requireIndexForm, extraOffset);
    }

OMR::Power::NodeMemoryReference OMR::Power::LoadStoreHandlerImpl::generateMemoryReference(TR::CodeGenerator *cg, TR::Node *node, uint32_t length, bool requireIndexForm, int64_t extraOffset)
    {
    TR::MemoryReference *memRef = TR::MemoryReference::createWithRootLoadOrStore(cg, node, length);

    if (extraOffset != 0)
        memRef->addToOffset(node, extraOffset, cg);

    if (requireIndexForm)
        memRef->forceIndexedForm(node, cg);

    return OMR::Power::NodeMemoryReference(memRef);
    }

void OMR::Power::LoadStoreHandlerImpl::generateLoadSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node, TR::MemoryReference *memRef, TR::InstOpCode::Mnemonic loadOp)
    {
    generateTrg1MemInstruction(cg, loadOp, node, trgReg, memRef);
    cg->insertPrefetchIfNecessary(node, trgReg);

#ifdef J9_PROJECT_SPECIFIC
    if (node->getSymbol()->isAtLeastOrStrongerThanAcquireRelease() && cg->comp()->target().isSMP())
        TR::TreeEvaluator::postSyncConditions(node, cg, trgReg, memRef, cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P7) ? TR::InstOpCode::lwsync : TR::InstOpCode::isync);
#endif
    }

void OMR::Power::LoadStoreHandlerImpl::generatePairedLoadSequence(TR::CodeGenerator *cg, TR::Register *trgReg, TR::Node *node, TR::MemoryReference *memRef)
    {
    #ifdef J9_PROJECT_SPECIFIC
    if (node->getSymbolReference()->isUnresolved())
        {
        TR::SymbolReference *vrlSymRef = cg->comp()->getSymRefTab()->findOrCreateVolatileReadLongSymbolRef(cg->comp()->getMethodSymbol());

        memRef->getUnresolvedSnippet()->setIs32BitLong();

        TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(4, 4, cg->trMemory());
        TR::addDependency(deps, trgReg->getHighOrder(), TR::RealRegister::gr3, TR_GPR, cg);
        TR::addDependency(deps, trgReg->getLowOrder(), TR::RealRegister::gr4, TR_GPR, cg);
        TR::addDependency(deps, NULL, TR::RealRegister::gr11, TR_GPR, cg);

        TR::Register *baseReg = memRef->getBaseRegister();
        if (baseReg)
            {
            TR::addDependency(deps, baseReg, TR::RealRegister::NoReg, TR_GPR, cg);
            deps->getPreConditions()->getRegisterDependency(3)->setExcludeGPR0();
            deps->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
            }

        generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, trgReg->getHighOrder(), memRef);
        generateDepImmSymInstruction(
            cg,
            TR::InstOpCode::bl,
            node,
            reinterpret_cast<uintptr_t>(vrlSymRef->getSymbol()->castToMethodSymbol()->getMethodAddress()),
            deps,
            vrlSymRef
        );

        cg->machine()->setLinkRegisterKilled(true);
        }
    // Since non-volatiles are implemented as two separate loads, we must use a special sequence to perform the load in
    // a single instruction even when SMP is disabled.
    else if (node->getSymbol()->isAtLeastOrStrongerThanAcquireRelease())
        {
        if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8) && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))
            {
            TR::Register *tempHiReg = cg->allocateRegister(TR_FPR);
            TR::Register *tempLoReg = cg->allocateRegister(TR_FPR);

            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, tempLoReg, memRef);
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, tempHiReg, tempLoReg, 0);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, trgReg->getHighOrder(), tempHiReg);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, trgReg->getLowOrder(), tempLoReg);

            if (cg->comp()->target().isSMP())
                generateInstruction(cg, TR::InstOpCode::lwsync, node);

            cg->stopUsingRegister(tempLoReg);
            cg->stopUsingRegister(tempHiReg);
            }
        else
            {
            TR::Register *tempReg = cg->allocateRegister(TR_FPR);
            TR_BackingStore *spillLocation = cg->allocateSpill(8, false, NULL);

            TR::MemoryReference *spillRef = TR::MemoryReference::createWithSymRef(cg, node, spillLocation->getSymbolReference(), 8);
            TR::MemoryReference *spillRefHi = TR::MemoryReference::createWithMemRef(cg, node, *spillRef, 0, 4);
            TR::MemoryReference *spillRefLo = TR::MemoryReference::createWithMemRef(cg, node, *spillRef, 4, 4);

            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, tempReg, memRef);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, spillRef, tempReg);

            if (cg->comp()->target().isSMP())
                generateInstruction(cg, cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P7) ? TR::InstOpCode::lwsync : TR::InstOpCode::isync, node);

            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, trgReg->getHighOrder(), spillRefHi);
            generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, trgReg->getLowOrder(), spillRefLo);

            cg->freeSpill(spillLocation, 8, 0);
            cg->stopUsingRegister(tempReg);
            }
        }
    else
#endif
        {
        TR::MemoryReference *memRefHi = TR::MemoryReference::createWithMemRef(cg, node, *memRef, 0, 4);
        TR::MemoryReference *memRefLo = TR::MemoryReference::createWithMemRef(cg, node, *memRef, 4, 4);

        generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, trgReg->getHighOrder(), memRefHi);
        generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, trgReg->getLowOrder(), memRefLo);
        }
    }

#ifdef J9_PROJECT_SPECIFIC
enum class StoreSyncRequirements
    {
    None,
    AtomicWrite,
    LazySet,
    Full
    };

StoreSyncRequirements getStoreSyncRequirements(TR::CodeGenerator *cg, TR::Node *node)
    {
    if (node->getSymbol()->isAtLeastOrStrongerThanAcquireRelease())
        {
        // Even when SMP is disabled, we need to ensure that the write is performed in a single instruction to prevent
        // the current thread from being pre-empted during the store. This really only affects writes to longs on
        // ppc32, which are normally done as 2 separate stores. For other platforms and types of stores, AtomicWrite is
        // equivalent to None.
        if (!cg->comp()->target().isSMP())
            return StoreSyncRequirements::AtomicWrite;

        if (node->getSymbol()->isVolatile())
            return StoreSyncRequirements::Full;

        return StoreSyncRequirements::LazySet;
        }

    return StoreSyncRequirements::None;
    }
#endif

void OMR::Power::LoadStoreHandlerImpl::generateStoreSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node, TR::MemoryReference *memRef, TR::InstOpCode::Mnemonic storeOp)
    {
#ifdef J9_PROJECT_SPECIFIC
    StoreSyncRequirements sync = getStoreSyncRequirements(cg, node);

    if (sync == StoreSyncRequirements::LazySet || sync == StoreSyncRequirements::Full)
        generateInstruction(cg, TR::InstOpCode::lwsync, node);
#endif

    generateMemSrc1Instruction(cg, storeOp, node, memRef, srcReg);

#ifdef J9_PROJECT_SPECIFIC
    if (sync == StoreSyncRequirements::Full)
        TR::TreeEvaluator::postSyncConditions(node, cg, srcReg, memRef, TR::InstOpCode::sync);
#endif
    }

void OMR::Power::LoadStoreHandlerImpl::generatePairedStoreSequence(TR::CodeGenerator *cg, TR::Register *srcReg, TR::Node *node, TR::MemoryReference *memRef)
    {
#ifdef J9_PROJECT_SPECIFIC
    StoreSyncRequirements sync = getStoreSyncRequirements(cg, node);

    if (node->getSymbolReference()->isUnresolved())
        {
        TR::Register *addrReg = cg->allocateRegister();
        TR::SymbolReference *vwlSymRef = cg->comp()->getSymRefTab()->findOrCreateVolatileWriteLongSymbolRef(cg->comp()->getMethodSymbol());

        memRef->getUnresolvedSnippet()->setIs32BitLong();

        TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
        TR::addDependency(deps, addrReg, TR::RealRegister::gr3, TR_GPR, cg);
        TR::addDependency(deps, srcReg->getHighOrder(), TR::RealRegister::gr4, TR_GPR, cg);
        TR::addDependency(deps, srcReg->getLowOrder(), TR::RealRegister::gr5, TR_GPR, cg);
        TR::addDependency(deps, NULL, TR::RealRegister::gr11, TR_GPR, cg);

        TR::Register *baseReg = memRef->getBaseRegister();
        if (baseReg)
            {
            TR::addDependency(deps, baseReg, TR::RealRegister::NoReg, TR_GPR, cg);
            deps->getPreConditions()->getRegisterDependency(4)->setExcludeGPR0();
            deps->getPostConditions()->getRegisterDependency(4)->setExcludeGPR0();
            }

        generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, addrReg, memRef);
        generateDepImmSymInstruction(
            cg,
            TR::InstOpCode::bl,
            node,
            reinterpret_cast<uintptr_t>(vwlSymRef->getSymbol()->castToMethodSymbol()->getMethodAddress()),
            deps,
            vwlSymRef
        );

        cg->machine()->setLinkRegisterKilled(true);
        cg->stopUsingRegister(addrReg);
        }
    else if (sync != StoreSyncRequirements::None)
        {
        if (cg->comp()->target().cpu.isAtLeast(OMR_PROCESSOR_PPC_P8) && cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_PPC_HAS_VSX))
            {
            TR::Register *tempHiReg = cg->allocateRegister(TR_FPR);
            TR::Register *tempLoReg = cg->allocateRegister(TR_FPR);

            generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, tempHiReg, srcReg->getHighOrder());
            generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, tempLoReg, srcReg->getLowOrder());
            generateTrg1Src2Instruction(cg, TR::InstOpCode::xxmrghw, node, tempHiReg, tempHiReg, tempLoReg);
            generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, tempHiReg, tempHiReg, tempHiReg, 2);

            if (sync == StoreSyncRequirements::LazySet || sync == StoreSyncRequirements::Full)
                generateInstruction(cg, TR::InstOpCode::lwsync, node);

            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, memRef, tempHiReg);

            if (sync == StoreSyncRequirements::Full)
                generateInstruction(cg, TR::InstOpCode::sync, node);

            cg->stopUsingRegister(tempLoReg);
            cg->stopUsingRegister(tempHiReg);
            }
        else
            {
            TR::Register *tempReg = cg->allocateRegister(TR_FPR);
            TR_BackingStore *spillLocation = cg->allocateSpill(8, false, NULL);

            TR::MemoryReference *spillRef = TR::MemoryReference::createWithSymRef(cg, node, spillLocation->getSymbolReference(), 8);
            TR::MemoryReference *spillRefHi = TR::MemoryReference::createWithMemRef(cg, node, *spillRef, 0, 4);
            TR::MemoryReference *spillRefLo = TR::MemoryReference::createWithMemRef(cg, node, *spillRef, 4, 4);

            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, spillRefHi, srcReg->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, spillRefLo, srcReg->getLowOrder());

            if (sync == StoreSyncRequirements::LazySet || sync == StoreSyncRequirements::Full)
                generateInstruction(cg, TR::InstOpCode::lwsync, node);

            generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, tempReg, spillRef);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, memRef, tempReg);

            if (sync == StoreSyncRequirements::Full)
                generateInstruction(cg, TR::InstOpCode::sync, node);

            cg->freeSpill(spillLocation, 8, 0);
            cg->stopUsingRegister(tempReg);
            }
        }
    else
#endif
        {
        TR::MemoryReference *memRefHi = TR::MemoryReference::createWithMemRef(cg, node, *memRef, 0, 4);
        TR::MemoryReference *memRefLo = TR::MemoryReference::createWithMemRef(cg, node, *memRef, 4, 4);

        generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, memRefHi, srcReg->getHighOrder());
        generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, memRefLo, srcReg->getLowOrder());
        }
    }
