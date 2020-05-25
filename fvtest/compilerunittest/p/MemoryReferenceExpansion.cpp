/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "../CodeGenTest.hpp"
#include "Util.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/PPCInstruction.hpp"
#include "codegen/PPCTableOfConstants.hpp"

class PPCMemInstructionExpansionTest : public TRTest::CodeGenTest {};

TEST_F(PPCMemInstructionExpansionTest, zeroDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, smallPositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x7fff, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0x7fff, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, smallNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8000, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(-0x8000, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, largePositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x8000, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    TR::Instruction* lastInstr = instr->expandInstruction();

    ASSERT_NE(instr, lastInstr);
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(lastInstr, cg()->getAppendInstruction());
    ASSERT_FALSE(lastInstr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsMemSrc1, startInstr->getNext()->getKind());
    auto stTempInstr = static_cast<TR::PPCMemSrc1Instruction*>(startInstr->getNext());

    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? TR::InstOpCode::std : TR::InstOpCode::stw,
        stTempInstr->getOpCodeValue()
    );
    ASSERT_EQ(cg()->getStackPointerRegister(), stTempInstr->getMemoryReference()->getBaseRegister());
    ASSERT_TRUE(stTempInstr->getSourceRegister());
    ASSERT_TRUE(stTempInstr->getSourceRegister()->getRealRegister());
    ASSERT_FALSE(stTempInstr->getMemoryReference()->getIndexRegister());
    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? -8 : -4,
        stTempInstr->getMemoryReference()->getOffset()
    );

    ASSERT_TRUE(stTempInstr->getNext());
    ASSERT_EQ(stTempInstr->getNext()->getKind(), TR::Instruction::IsTrg1Src1Imm);
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(stTempInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(stTempInstr->getSourceRegister(), addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(0x0001, addisInstr->getSourceImmediate());

    ASSERT_EQ(instr, addisInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(stTempInstr->getSourceRegister(), mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(-0x8000, mr->getOffset());

    ASSERT_TRUE(instr->getNext());
    ASSERT_EQ(instr->getNext()->getKind(), TR::Instruction::IsTrg1Mem);
    auto ldTempInstr = static_cast<TR::PPCTrg1MemInstruction*>(instr->getNext());

    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? TR::InstOpCode::ld : TR::InstOpCode::lwz,
        ldTempInstr->getOpCodeValue()
    );
    ASSERT_EQ(stTempInstr->getSourceRegister(), ldTempInstr->getTargetRegister());
    ASSERT_FALSE(ldTempInstr->getMemoryReference()->getIndexRegister());
    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? -8 : -4,
        ldTempInstr->getMemoryReference()->getOffset()
    );
    ASSERT_EQ(lastInstr, ldTempInstr);
}

TEST_F(PPCMemInstructionExpansionTest, largeNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8001, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    TR::Instruction* lastInstr = instr->expandInstruction();

    ASSERT_NE(instr, lastInstr);
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(lastInstr, cg()->getAppendInstruction());
    ASSERT_FALSE(lastInstr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsMemSrc1, startInstr->getNext()->getKind());
    auto stTempInstr = static_cast<TR::PPCMemSrc1Instruction*>(startInstr->getNext());

    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? TR::InstOpCode::std : TR::InstOpCode::stw,
        stTempInstr->getOpCodeValue()
    );
    ASSERT_EQ(cg()->getStackPointerRegister(), stTempInstr->getMemoryReference()->getBaseRegister());
    ASSERT_TRUE(stTempInstr->getSourceRegister());
    ASSERT_TRUE(stTempInstr->getSourceRegister()->getRealRegister());
    ASSERT_FALSE(stTempInstr->getMemoryReference()->getIndexRegister());
    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? -8 : -4,
        stTempInstr->getMemoryReference()->getOffset()
    );

    ASSERT_TRUE(stTempInstr->getNext());
    ASSERT_EQ(stTempInstr->getNext()->getKind(), TR::Instruction::IsTrg1Src1Imm);
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(stTempInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(stTempInstr->getSourceRegister(), addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(-0x0001, addisInstr->getSourceImmediate());

    ASSERT_EQ(instr, addisInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(stTempInstr->getSourceRegister(), mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0x7fff, mr->getOffset());

    ASSERT_TRUE(instr->getNext());
    ASSERT_EQ(instr->getNext()->getKind(), TR::Instruction::IsTrg1Mem);
    auto ldTempInstr = static_cast<TR::PPCTrg1MemInstruction*>(instr->getNext());

    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? TR::InstOpCode::ld : TR::InstOpCode::lwz,
        ldTempInstr->getOpCodeValue()
    );
    ASSERT_EQ(stTempInstr->getSourceRegister(), ldTempInstr->getTargetRegister());
    ASSERT_FALSE(ldTempInstr->getMemoryReference()->getIndexRegister());
    ASSERT_EQ(
        cg()->comp()->target().is64Bit() ? -8 : -4,
        ldTempInstr->getMemoryReference()->getOffset()
    );
    ASSERT_EQ(lastInstr, ldTempInstr);
}

TEST_F(PPCMemInstructionExpansionTest, modBaseLargePositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x8000, 4, cg());

    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(0x0001, addisInstr->getSourceImmediate());

    ASSERT_EQ(instr, addisInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(-0x8000, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, modBaseLargeNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8001, 4, cg());

    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(-0x0001, addisInstr->getSourceImmediate());

    ASSERT_EQ(instr, addisInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwz, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0x7fff, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, simpleIndex) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, 4, cg());

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_EQ(indexReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexZeroDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setIndexRegister(indexReg);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexSmallPositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x7fff, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setIndexRegister(indexReg);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Imm, startInstr->getNext()->getKind());
    auto liInstr = static_cast<TR::PPCTrg1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::li, liInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, liInstr->getTargetRegister());
    ASSERT_EQ(0x7fff, liInstr->getSourceImmediate());

    ASSERT_EQ(instr, liInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_EQ(indexReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexSmallNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8000, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setIndexRegister(indexReg);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Imm, startInstr->getNext()->getKind());
    auto liInstr = static_cast<TR::PPCTrg1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::li, liInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, liInstr->getTargetRegister());
    ASSERT_EQ(-0x8000, liInstr->getSourceImmediate());

    ASSERT_EQ(instr, liInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_EQ(indexReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexLargePositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x8000, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setIndexRegister(indexReg);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Imm, startInstr->getNext()->getKind());
    auto lisInstr = static_cast<TR::PPCTrg1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::lis, lisInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, lisInstr->getTargetRegister());
    ASSERT_EQ(0x0000, lisInstr->getSourceImmediate());

    ASSERT_TRUE(lisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, lisInstr->getNext()->getKind());
    auto oriInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(lisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::ori, oriInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, oriInstr->getTargetRegister());
    ASSERT_EQ(indexReg, oriInstr->getSource1Register());
    ASSERT_EQ(0x8000, oriInstr->getSourceImmediate());

    ASSERT_EQ(instr, oriInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_EQ(indexReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexLargeNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::Register* indexReg = cg()->machine()->getRealRegister(TR::RealRegister::gr2);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8001, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setIndexRegister(indexReg);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Imm, startInstr->getNext()->getKind());
    auto lisInstr = static_cast<TR::PPCTrg1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::lis, lisInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, lisInstr->getTargetRegister());
    ASSERT_EQ(-0x0001, lisInstr->getSourceImmediate());

    ASSERT_TRUE(lisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, lisInstr->getNext()->getKind());
    auto oriInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(lisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::ori, oriInstr->getOpCodeValue());
    ASSERT_EQ(indexReg, oriInstr->getTargetRegister());
    ASSERT_EQ(indexReg, oriInstr->getSource1Register());
    ASSERT_EQ(0x7fff, oriInstr->getSourceImmediate());

    ASSERT_EQ(instr, oriInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(baseReg, mr->getBaseRegister());
    ASSERT_EQ(indexReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexModBaseZeroDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexModBaseSmallPositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x7fff, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addiInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addi, addiInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addiInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addiInstr->getSource1Register());
    ASSERT_EQ(0x7fff, addiInstr->getSourceImmediate());

    ASSERT_EQ(instr, addiInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexModBaseSmallNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8000, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addiInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addi, addiInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addiInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addiInstr->getSource1Register());
    ASSERT_EQ(-0x8000, addiInstr->getSourceImmediate());

    ASSERT_EQ(instr, addiInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexModBaseLargePositiveDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, 0x8000, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(0x0001, addisInstr->getSourceImmediate());

    ASSERT_TRUE(addisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, addisInstr->getNext()->getKind());
    auto addiInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(addisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addi, addiInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addiInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addiInstr->getSource1Register());
    ASSERT_EQ(-0x8000, addiInstr->getSourceImmediate());

    ASSERT_EQ(instr, addiInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, delayedIndexModBaseLargeNegativeDisp) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* baseReg = cg()->machine()->getRealRegister(TR::RealRegister::gr1);
    TR::MemoryReference* mr = new (cg()->trHeapMemory()) TR::MemoryReference(baseReg, -0x8001, 4, cg());

    mr->setUsingDelayedIndexedForm();
    mr->setBaseModifiable();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::lwz,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addisInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addisInstr->getSource1Register());
    ASSERT_EQ(-0x0001, addisInstr->getSourceImmediate());

    ASSERT_TRUE(addisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, addisInstr->getNext()->getKind());
    auto addiInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(addisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addi, addiInstr->getOpCodeValue());
    ASSERT_EQ(baseReg, addiInstr->getTargetRegister());
    ASSERT_EQ(baseReg, addiInstr->getSource1Register());
    ASSERT_EQ(0x7fff, addiInstr->getSourceImmediate());

    ASSERT_EQ(instr, addiInstr->getNext());
    ASSERT_EQ(TR::InstOpCode::lwzx, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(cg()->machine()->getRealRegister(TR::RealRegister::gr0), mr->getBaseRegister());
    ASSERT_EQ(baseReg, mr->getIndexRegister());
    ASSERT_EQ(0, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, tocSmallPositiveDisp) {
    if (!cg()->comp()->target().is64Bit())
        return;

    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* tocReg = cg()->getTOCBaseRegister();

    TR::StaticSymbol* sym = TR::StaticSymbol::create(cg()->trHeapMemory(), TR::Int32);
    sym->setStaticAddress(NULL);
    sym->setTOCIndex(0xfff);

    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    symRef->setUnresolved();

    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);
    mr->setUsingStaticTOC();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    // This has to be set *after* creating the instruction to avoid some unwanted behaviour in bookKeepingRegisterUses
    // which will attempt to modify some register conditions that don't exist and cause a crash.
    mr->setModBase(tocReg);

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::ld, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(tocReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0x7ff8, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, tocSmallNegativeDisp) {
    if (!cg()->comp()->target().is64Bit())
        return;

    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* tocReg = cg()->getTOCBaseRegister();

    TR::StaticSymbol* sym = TR::StaticSymbol::create(cg()->trHeapMemory(), TR::Int32);
    sym->setStaticAddress(NULL);
    sym->setTOCIndex(-0x1000);

    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    symRef->setUnresolved();

    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);
    mr->setUsingStaticTOC();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    // This has to be set *after* creating the instruction to avoid some unwanted behaviour in bookKeepingRegisterUses
    // which will attempt to modify some register conditions that don't exist and cause a crash.
    mr->setModBase(tocReg);

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_EQ(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_EQ(TR::InstOpCode::ld, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(tocReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(-0x8000, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, tocLargePositiveDisp) {
    if (!cg()->comp()->target().is64Bit())
        return;

    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* tocReg = cg()->getTOCBaseRegister();

    TR::StaticSymbol* sym = TR::StaticSymbol::create(cg()->trHeapMemory(), TR::Int32);
    sym->setStaticAddress(NULL);
    sym->setTOCIndex(0x1000);

    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    symRef->setUnresolved();

    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);
    mr->setUsingStaticTOC();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    // This has to be set *after* creating the instruction to avoid some unwanted behaviour in bookKeepingRegisterUses
    // which will attempt to modify some register conditions that don't exist and cause a crash.
    mr->setModBase(tocReg);

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstruction = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstruction->getOpCodeValue());
    ASSERT_EQ(dataReg, addisInstruction->getTargetRegister());
    ASSERT_EQ(tocReg, addisInstruction->getSource1Register());
    ASSERT_EQ(0x0001, addisInstruction->getSourceImmediate());

    ASSERT_EQ(instr, addisInstruction->getNext());
    ASSERT_EQ(TR::InstOpCode::ld, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(dataReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(-0x8000, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, tocLargeNegativeDisp) {
    if (!cg()->comp()->target().is64Bit())
        return;

    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* tocReg = cg()->getTOCBaseRegister();

    TR::StaticSymbol* sym = TR::StaticSymbol::create(cg()->trHeapMemory(), TR::Int32);
    sym->setStaticAddress(NULL);
    sym->setTOCIndex(-0x1001);

    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    symRef->setUnresolved();

    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);
    mr->setUsingStaticTOC();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    // This has to be set *after* creating the instruction to avoid some unwanted behaviour in bookKeepingRegisterUses
    // which will attempt to modify some register conditions that don't exist and cause a crash.
    mr->setModBase(tocReg);

    ASSERT_EQ(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_EQ(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, startInstr->getNext()->getKind());
    auto addisInstruction = static_cast<TR::PPCTrg1Src1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::addis, addisInstruction->getOpCodeValue());
    ASSERT_EQ(dataReg, addisInstruction->getTargetRegister());
    ASSERT_EQ(tocReg, addisInstruction->getSource1Register());
    ASSERT_EQ(-0x0001, addisInstruction->getSourceImmediate());

    ASSERT_EQ(instr, addisInstruction->getNext());
    ASSERT_EQ(TR::InstOpCode::ld, instr->getOpCodeValue());
    ASSERT_EQ(dataReg, instr->getTargetRegister(0));
    ASSERT_EQ(mr, instr->getMemoryReference());
    ASSERT_EQ(dataReg, mr->getBaseRegister());
    ASSERT_FALSE(mr->getIndexRegister());
    ASSERT_EQ(0x7ff8, mr->getOffset());
}

TEST_F(PPCMemInstructionExpansionTest, tocFull) {
    if (!cg()->comp()->target().is64Bit())
        return;

    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);
    TR::Register* tocReg = cg()->getTOCBaseRegister();

    TR::StaticSymbol* sym = TR::StaticSymbol::create(cg()->trHeapMemory(), TR::Int32);
    sym->setStaticAddress(reinterpret_cast<void*>(static_cast<uintptr_t>(0xdeadc0decafebabeULL)));
    sym->setTOCIndex(PTOC_FULL_INDEX);

    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    symRef->setUnresolved();

    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);
    mr->setUsingStaticTOC();

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    // This has to be set *after* creating the instruction to avoid some unwanted behaviour in bookKeepingRegisterUses
    // which will attempt to modify some register conditions that don't exist and cause a crash.
    mr->setModBase(tocReg);

    ASSERT_NE(instr, instr->expandInstruction());
    ASSERT_EQ(startInstr, cg()->getFirstInstruction());
    ASSERT_NE(instr, startInstr->getNext());
    ASSERT_NE(instr, cg()->getAppendInstruction());
    ASSERT_FALSE(instr->getNext());

    ASSERT_TRUE(startInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Imm, startInstr->getNext()->getKind());
    auto lisInstr = static_cast<TR::PPCTrg1ImmInstruction*>(startInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::lis, lisInstr->getOpCodeValue());
    ASSERT_EQ(dataReg, lisInstr->getTargetRegister());
    ASSERT_EQ((int16_t)0xdead, lisInstr->getSourceImmediate());

    ASSERT_TRUE(lisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, lisInstr->getNext()->getKind());
    auto oriInstr1 = static_cast<TR::PPCTrg1Src1ImmInstruction*>(lisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::ori, oriInstr1->getOpCodeValue());
    ASSERT_EQ(dataReg, oriInstr1->getTargetRegister());
    ASSERT_EQ(dataReg, oriInstr1->getSource1Register());
    ASSERT_EQ(0xc0de, oriInstr1->getSourceImmediate());

    ASSERT_TRUE(oriInstr1->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm2, oriInstr1->getNext()->getKind());
    auto rldicrInstr = static_cast<TR::PPCTrg1Src1Imm2Instruction*>(oriInstr1->getNext());

    ASSERT_EQ(TR::InstOpCode::rldicr, rldicrInstr->getOpCodeValue());
    ASSERT_EQ(dataReg, rldicrInstr->getTargetRegister());
    ASSERT_EQ(dataReg, rldicrInstr->getSource1Register());
    ASSERT_EQ(32, rldicrInstr->getSourceImmediate());
    ASSERT_EQ(0xffffffff00000000ULL, rldicrInstr->getLongMask());

    ASSERT_TRUE(rldicrInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, rldicrInstr->getNext()->getKind());
    auto orisInstr = static_cast<TR::PPCTrg1Src1ImmInstruction*>(rldicrInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::oris, orisInstr->getOpCodeValue());
    ASSERT_EQ(dataReg, orisInstr->getTargetRegister());
    ASSERT_EQ(dataReg, orisInstr->getSource1Register());
    ASSERT_EQ(0xcafe, orisInstr->getSourceImmediate());

    ASSERT_TRUE(orisInstr->getNext());
    ASSERT_EQ(TR::Instruction::IsTrg1Src1Imm, orisInstr->getNext()->getKind());
    auto oriInstr2 = static_cast<TR::PPCTrg1Src1ImmInstruction*>(orisInstr->getNext());

    ASSERT_EQ(TR::InstOpCode::ori, oriInstr2->getOpCodeValue());
    ASSERT_EQ(dataReg, oriInstr2->getTargetRegister());
    ASSERT_EQ(dataReg, oriInstr2->getSource1Register());
    ASSERT_EQ(0xbabe, oriInstr2->getSourceImmediate());

    ASSERT_FALSE(oriInstr2->getNext());
}

TEST_F(PPCMemInstructionExpansionTest, delayedOffset) {
    TR::Register* dataReg = cg()->machine()->getRealRegister(TR::RealRegister::gr0);

    TR::AutomaticSymbol* sym = TR::AutomaticSymbol::create(cg()->trHeapMemory());
    TR::SymbolReference* symRef = new (cg()->trHeapMemory()) TR::SymbolReference(cg()->comp()->getSymRefTab(), sym);
    TR::MemoryReference *mr = new (cg()->trHeapMemory()) TR::MemoryReference(cg());

    mr->setSymbolReference(symRef);

    TR::Node* fakeNode = TR::Node::create(TR::treetop);
    TR::Instruction* startInstr = generateLabelInstruction(cg(), TR::InstOpCode::label, fakeNode, generateLabelSymbol(cg()));
    TR::Instruction* instr = generateTrg1MemInstruction(
        cg(),
        TR::InstOpCode::ld,
        fakeNode,
        dataReg,
        mr
    );

    ASSERT_EQ(0, mr->getOffset());
    ASSERT_EQ(0, mr->getOffset(*cg()->comp()));
    ASSERT_FALSE(mr->isDelayedOffsetDone());

    sym->setOffset(0x7fff);

    ASSERT_EQ(0, mr->getOffset());
    ASSERT_EQ(0x7fff, mr->getOffset(*cg()->comp()));
    ASSERT_FALSE(mr->isDelayedOffsetDone());

    instr->expandInstruction();

    ASSERT_EQ(0x7fff, mr->getOffset());
    ASSERT_EQ(0x7fff, mr->getOffset(*cg()->comp()));
    ASSERT_TRUE(mr->isDelayedOffsetDone());
}
