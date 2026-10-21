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

#include "z/codegen/CallSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/S390Instruction.hpp"

#define TR_S390_ARG_SLOT_SIZE 4
#define OPCODE_SPACING 8

uint8_t *TR::S390CallSnippet::storeArgumentItem(TR::InstOpCode::Mnemonic op, uint8_t *buffer, TR::RealRegister *reg,
    int32_t offset, TR::CodeGenerator *cg)
{
    TR::RealRegister *stackPtr = cg->getStackPointerRealRegister();
    TR::InstOpCode opCode(op);
    opCode.copyBinaryToBuffer(buffer);
    reg->setRegisterField(toS390Cursor(buffer));
    stackPtr->setBaseRegisterField(toS390Cursor(buffer));
    TR_ASSERT((offset & 0xfffff000) == 0, "CallSnippet: Unexpected displacement %d for storeArgumentItem", offset);
    *toS390Cursor(buffer) |= offset & 0x00000fff;
    return buffer + opCode.getInstructionLength();
}

uint8_t *TR::S390CallSnippet::S390FlushArgumentsToStackHelper(uint8_t *buffer, TR::Node *callNode, int32_t argSize,
    TR::CodeGenerator *cg, int argStart, bool rightToLeft, TR::Linkage *linkage)
{
    int32_t intArgNum = 0, floatArgNum = 0, offset;
    TR::Machine *machine = cg->machine();

    if (rightToLeft) {
        offset = linkage->getOffsetToFirstParm();
    } else {
        offset = argSize + linkage->getOffsetToFirstParm();
    }

    for (int32_t i = argStart; i < callNode->getNumChildren(); i++) {
        TR::Node *child = callNode->getChild(i);
        switch (child->getDataType()) {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
                if (!rightToLeft) {
                    offset -= cg->comp()->target().is64Bit() ? 8 : 4;
                }
                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    buffer = storeArgumentItem(TR::InstOpCode::ST, buffer,
                        machine->getRealRegister(linkage->getIntegerArgumentRegister(intArgNum)), offset, cg);
                }
                intArgNum++;

                if (rightToLeft) {
                    offset += cg->comp()->target().is64Bit() ? 8 : 4;
                }
                break;
            case TR::Address:
                if (!rightToLeft) {
                    offset -= cg->comp()->target().is64Bit() ? 8 : 4;
                }

                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    buffer = storeArgumentItem(TR::InstOpCode::getStoreOpCode(), buffer,
                        machine->getRealRegister(linkage->getIntegerArgumentRegister(intArgNum)), offset, cg);
                }
                intArgNum++;

                if (rightToLeft) {
                    offset += cg->comp()->target().is64Bit() ? 8 : 4;
                }
                break;

            case TR::Int64:
                if (!rightToLeft) {
                    offset -= (cg->comp()->target().is64Bit() ? 16 : 8);
                }
                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    if (cg->comp()->target().is64Bit()) {
                        buffer = storeArgumentItem(TR::InstOpCode::STG, buffer,
                            machine->getRealRegister(linkage->getIntegerArgumentRegister(intArgNum)), offset, cg);
                    } else {
                        buffer = storeArgumentItem(TR::InstOpCode::ST, buffer,
                            machine->getRealRegister(linkage->getIntegerArgumentRegister(intArgNum)), offset, cg);
                        if (intArgNum < linkage->getNumIntegerArgumentRegisters() - 1) {
                            buffer = storeArgumentItem(TR::InstOpCode::ST, buffer,
                                machine->getRealRegister(linkage->getIntegerArgumentRegister(intArgNum + 1)),
                                offset + 4, cg);
                        }
                    }
                }
                intArgNum += cg->comp()->target().is64Bit() ? 1 : 2;
                if (rightToLeft) {
                    offset += cg->comp()->target().is64Bit() ? 16 : 8;
                }
                break;

            case TR::Float:
                if (!rightToLeft) {
                    offset -= cg->comp()->target().is64Bit() ? 8 : 4;
                }
                if (floatArgNum < linkage->getNumFloatArgumentRegisters()) {
                    buffer = storeArgumentItem(TR::InstOpCode::STE, buffer,
                        machine->getRealRegister(linkage->getFloatArgumentRegister(floatArgNum)), offset, cg);
                }
                floatArgNum++;
                if (rightToLeft) {
                    offset += cg->comp()->target().is64Bit() ? 8 : 4;
                }
                break;

            case TR::Double:
                if (!rightToLeft) {
                    offset -= cg->comp()->target().is64Bit() ? 16 : 8;
                }
                if (floatArgNum < linkage->getNumFloatArgumentRegisters()) {
                    buffer = storeArgumentItem(TR::InstOpCode::STD, buffer,
                        machine->getRealRegister(linkage->getFloatArgumentRegister(floatArgNum)), offset, cg);
                }
                floatArgNum++;
                if (rightToLeft) {
                    offset += cg->comp()->target().is64Bit() ? 16 : 8;
                }
                break;
        }
    }

    return buffer;
}

uint8_t *TR::S390CallSnippet::S390FlushArgumentsToStack(uint8_t *buffer, TR::Node *callNode, int32_t argSize,
    TR::CodeGenerator *cg)
{
    TR::Linkage *linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention());

    // we want the arguments for induceOSR to be passed from left to right as in any other non-helper call
    bool rightToLeft = linkage->getRightToLeft() && !callNode->getSymbolReference()->isOSRInductionHelper();
    return S390FlushArgumentsToStackHelper(buffer, callNode, argSize, cg, callNode->getFirstArgumentIndex(),
        rightToLeft, linkage);
}

int32_t TR::S390CallSnippet::adjustCallOffsetWithTrampoline(uintptr_t targetAddr, uint8_t *currentInst,
    TR::SymbolReference *callSymRef, TR::Snippet *snippet)
{
    uintptr_t currentInstPtr = reinterpret_cast<uintptr_t>(currentInst);
    int32_t offsetHalfWords = static_cast<int32_t>((targetAddr - currentInstPtr) / 2);
    TR::CodeGenerator *cg = snippet->cg();
    if (cg->directCallRequiresTrampoline(targetAddr, currentInstPtr)) {
        if (callSymRef->getReferenceNumber() < TR_S390numRuntimeHelpers)
            targetAddr = TR::CodeCacheManager::instance()->findHelperTrampoline(callSymRef->getReferenceNumber(),
                reinterpret_cast<void *>(currentInstPtr));
        else
            targetAddr
                = cg->fe()->methodTrampolineLookup(cg->comp(), callSymRef, reinterpret_cast<void *>(currentInstPtr));

        TR_ASSERT_FATAL(cg->comp()->target().cpu.isTargetWithinBranchRelativeRILRange(targetAddr,
                            reinterpret_cast<intptr_t>(currentInst)),
            "Local trampoline must be directly reachable, address %p is out of addressible range using RIL "
            "instructions",
            targetAddr);

        offsetHalfWords = static_cast<int32_t>((targetAddr - currentInstPtr) / 2);

        snippet->setUsedTrampoline(true);
    }

    snippet->setSnippetDestAddr(targetAddr);

    return offsetHalfWords;
}

/**
 * @return the total instruction length in bytes for setting up arguments
 */
int32_t TR::S390CallSnippet::instructionCountForArguments(TR::Node *callNode, TR::CodeGenerator *cg)
{
    return instructionCountForArgumentsInner(callNode, cg, callNode->getFirstArgumentIndex());
}

int32_t TR::S390CallSnippet::instructionCountForArgumentsInner(TR::Node *callNode, TR::CodeGenerator *cg,
    int32_t argStart)
{
    int32_t intArgNum = 0, floatArgNum = 0, count = 0;
    TR::Linkage *linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention());

    for (int32_t i = argStart; i < callNode->getNumChildren(); i++) {
        TR::Node *child = callNode->getChild(i);
        switch (child->getDataType()) {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::ST);
                }
                intArgNum++;
                break;
            case TR::Address:
                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::getLoadOpCode());
                }
                intArgNum++;
                break;
            case TR::Int64:
                if (intArgNum < linkage->getNumIntegerArgumentRegisters()) {
                    count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::getLoadOpCode());
                    if ((cg->comp()->target().is32Bit()) && intArgNum < linkage->getNumIntegerArgumentRegisters() - 1) {
                        count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::getLoadOpCode());
                    }
                }
                intArgNum += cg->comp()->target().is64Bit() ? 1 : 2;
                break;
            case TR::Float:
                if (floatArgNum < linkage->getNumFloatArgumentRegisters()) {
                    count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::LE);
                }
                floatArgNum++;
                break;
            case TR::Double:
                if (floatArgNum < linkage->getNumFloatArgumentRegisters()) {
                    count += TR::InstOpCode::getInstructionLength(TR::InstOpCode::LD);
                }
                floatArgNum++;
                break;
        }
    }
    return count;
}

uint8_t *TR::S390CallSnippet::printS390ArgumentsFlush(OMR::Logger *log, TR::Node *node, uint8_t *bufferPos,
    int32_t argSize, TR_Debug *debug, int32_t argStart, TR::Machine *machine, TR::Linkage *privateLinkage)
{
    int32_t offset = 0, intArgNum = 0, floatArgNum = 0;

    TR::RealRegister *stackPtr = privateLinkage->getStackPointerRealRegister();

    if (privateLinkage->getRightToLeft()) {
        offset = privateLinkage->getOffsetToFirstParm();
    } else {
        offset = argSize + privateLinkage->getOffsetToFirstParm();
    }

    for (int i = argStart; i < node->getNumChildren(); i++) {
        TR::Node *child = node->getChild(i);
        switch (child->getDataType()) {
            case TR::Int8:
            case TR::Int16:
            case TR::Int32:
            case TR::Address:
                if (!privateLinkage->getRightToLeft()) {
                    offset -= comp()->target().is64Bit() ? 8 : 4;
                }
                if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters()) {
                    TR::InstOpCode::Mnemonic opcode
                        = (comp()->target().is64Bit() && child->getDataType() == TR::Address) ? TR::InstOpCode::STG
                                                                                              : TR::InstOpCode::ST;

                    uint8_t opcodeSize = (comp()->target().is64Bit() && child->getDataType() == TR::Address) ? 6 : 4;
                    bufferPos = printRXInstruction(log, debug, opcode,
                        machine->getRealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)), stackPtr,
                        offset, bufferPos, opcodeSize);
                }
                intArgNum++;
                if (privateLinkage->getRightToLeft()) {
                    offset -= comp()->target().is64Bit() ? 8 : 4;
                }
                break;

            case TR::Int64:
                if (!privateLinkage->getRightToLeft()) {
                    offset -= (comp()->target().is64Bit() ? 16 : 8);
                }
                if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters()) {
                    if (comp()->target().is64Bit()) {
                        bufferPos = printRXInstruction(log, debug, TR::InstOpCode::STG,
                            machine->getRealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)), stackPtr,
                            offset, bufferPos, 6);
                    } else {
                        bufferPos = printRXInstruction(log, debug, TR::InstOpCode::ST,
                            machine->getRealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum)), stackPtr,
                            offset, bufferPos, 4);

                        if (intArgNum < privateLinkage->getNumIntegerArgumentRegisters() - 1) {
                            bufferPos = printRXInstruction(log, debug, TR::InstOpCode::ST,
                                machine->getRealRegister(privateLinkage->getIntegerArgumentRegister(intArgNum + 1)),
                                stackPtr, offset, bufferPos, 4);
                        }
                    }
                }
                intArgNum += comp()->target().is64Bit() ? 1 : 2;
                if (privateLinkage->getRightToLeft()) {
                    offset += comp()->target().is64Bit() ? 16 : 8;
                }
                break;

            case TR::Float:
                if (!privateLinkage->getRightToLeft()) {
                    offset -= 4;
                }
                if (floatArgNum < privateLinkage->getNumFloatArgumentRegisters()) {
                    bufferPos = printRXInstruction(log, debug, TR::InstOpCode::STD,
                        machine->getRealRegister(privateLinkage->getFloatArgumentRegister(floatArgNum)), stackPtr,
                        offset, bufferPos, 4);
                }
                floatArgNum++;
                if (privateLinkage->getRightToLeft()) {
                    offset += 4;
                }
                break;

            case TR::Double:
                if (!privateLinkage->getRightToLeft()) {
                    offset -= 8;
                }
                if (floatArgNum < privateLinkage->getNumFloatArgumentRegisters()) {
                    bufferPos = printRXInstruction(log, debug, TR::InstOpCode::STE,
                        machine->getRealRegister(privateLinkage->getFloatArgumentRegister(floatArgNum)), stackPtr,
                        offset, bufferPos, 4);
                }
                floatArgNum++;
                if (privateLinkage->getRightToLeft()) {
                    offset += 8;
                }
                break;
        }
    }

    return bufferPos;
}

uint8_t *TR::S390CallSnippet::getCallRA()
{
    // Return Address is the address of the instr follows the branch instr
    TR_ASSERT(getBranchInstruction() != NULL, "CallSnippet: branchInstruction is NULL");
    return (uint8_t *)(getBranchInstruction()->getNext()->getBinaryEncoding());
}

uint8_t *TR::S390CallSnippet::emitSnippetBody()
{
    TR_ASSERT(0, "TR::S390CallSnippet::emitSnippetBody not implemented");
    return NULL;
}

uint32_t TR::S390CallSnippet::getLength(int32_t estimatedSnippetStart) { return 0; }

uint8_t *TR::S390CallSnippet::printRXInstruction(OMR::Logger *log, TR_Debug *debug, TR::InstOpCode::Mnemonic opcode,
    TR::RealRegister *targetReg, TR::RealRegister *baseReg, int32_t offset, uint8_t *cursor, uint8_t instrSize)
{
    printOpcode(log, debug, opcode, cursor, instrSize);
    debug->print(log, targetReg);
    log->printf(",%d(,", offset);
    debug->print(log, baseReg);
    log->printc(')');
    cursor += instrSize;
    return cursor;
}

uint8_t *TR::S390CallSnippet::printRRInstruction(OMR::Logger *log, TR_Debug *debug, TR::InstOpCode::Mnemonic opcode,
    TR::RealRegister *targetReg, TR::RealRegister *srcReg, uint8_t *cursor, uint8_t instrSize)
{
    printOpcode(log, debug, opcode, cursor, instrSize);
    debug->print(log, targetReg);
    log->printc(',');
    debug->print(log, srcReg);
    cursor += instrSize;
    return cursor;
}

void TR::S390CallSnippet::printOpcode(OMR::Logger *log, TR_Debug *debug, TR::InstOpCode::Mnemonic opcode,
    uint8_t *cursor, uint8_t instrSize)
{
    debug->printPrefix(log, NULL, cursor, instrSize);
    const char *mnemonicName = TR::InstOpCode::metadata[opcode].name;
    log->printf("%-*s", OPCODE_SPACING, mnemonicName);
}

