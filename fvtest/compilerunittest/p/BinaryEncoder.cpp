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
#include "il/LabelSymbol.hpp"

#include <iostream>
#include <stdexcept>

class BinaryInstruction {
public:
    std::vector<uint32_t> _words;

    BinaryInstruction() {}

    BinaryInstruction(uint32_t instr) {
        _words.push_back(instr);
    }

    BinaryInstruction(std::vector<uint32_t> words) : _words(words) {}

    const std::vector<uint32_t>& words() const { return _words; }

    BinaryInstruction operator^(const BinaryInstruction& other) const {
        if (_words.size() != other._words.size())
            throw std::invalid_argument("Attempt to XOR instructions of different sizes");

        BinaryInstruction instr(_words);

        for (size_t i = 0; i < _words.size(); i++)
            instr._words[i] ^= other._words[i];

        return instr;
    }

    bool operator==(const BinaryInstruction& other) const {
        return other._words == _words;
    }
};

std::ostream& operator<<(std::ostream& os, const BinaryInstruction& instr) {
    os << "[";

    for (size_t i = 0; i < instr.words().size(); i++) {
        os << " 0x" << std::hex << std::setw(8) << std::setfill('0') << instr.words()[i];
    }

    os << " ]";

    return os;
}

class PowerBinaryEncoderTest : public TRTest::CodeGenTest {
public:
    TR::Node* fakeNode;

    uint32_t buf[64];

    TR::RealRegister* createReg(TR_RegisterKinds kind, TR::RealRegister::RegNum reg, TR::RealRegister::RegMask mask) {
        return new (cg()->trHeapMemory()) TR::RealRegister(kind, 0, TR::RealRegister::Free, reg, mask, cg());
    }

    PowerBinaryEncoderTest() {
        fakeNode = TR::Node::create(TR::treetop);
        cg()->setBinaryBufferStart(reinterpret_cast<uint8_t*>(&buf[0]));
    }

    BinaryInstruction getEncodedInstruction(size_t size, uint32_t off = 0) {
        return std::vector<uint32_t>(&buf[off], &buf[off + (size / 4)]);
    }

    BinaryInstruction encodeInstruction(TR::Instruction *instr, uint32_t off = 0) {
        instr->estimateBinaryLength(0);
        cg()->setBinaryBufferCursor(reinterpret_cast<uint8_t*>(&buf[0]) + off);

        instr->generateBinaryEncoding();
        return getEncodedInstruction(instr->getBinaryLength(), off);
    }

    BinaryInstruction getBlankEncoding(TR::InstOpCode opCode) {
        opCode.copyBinaryToBuffer(reinterpret_cast<uint8_t*>(&buf[0]));

        return getEncodedInstruction(opCode.getBinaryLength());
    }

    BinaryInstruction findDifferingBits(TR::InstOpCode op1, TR::InstOpCode op2) {
        return getBlankEncoding(op1) ^ getBlankEncoding(op2);
    }
};

class PPCRecordFormSanityTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::InstOpCode::Mnemonic, BinaryInstruction>> {};

TEST_P(PPCRecordFormSanityTest, getRecordForm) {
    if (std::get<0>(GetParam()) == TR::InstOpCode::bad)
        return;

    TR::InstOpCode op = std::get<0>(GetParam());
    ASSERT_FALSE(op.isRecordForm());

    if (std::get<1>(GetParam()) != TR::InstOpCode::bad) {
        ASSERT_TRUE(op.hasRecordForm());
        ASSERT_EQ(std::get<1>(GetParam()), op.getRecordFormOpCodeValue());
    } else {
        ASSERT_FALSE(op.hasRecordForm());
    }
}

TEST_P(PPCRecordFormSanityTest, checkRecordForm) {
    if (std::get<1>(GetParam()) == TR::InstOpCode::bad)
        return;

    TR::InstOpCode recordOp = std::get<1>(GetParam());

    ASSERT_TRUE(recordOp.isRecordForm());
    ASSERT_FALSE(recordOp.hasRecordForm());
}

TEST_P(PPCRecordFormSanityTest, checkFlippedBit) {
    if (std::get<0>(GetParam()) == TR::InstOpCode::bad || std::get<1>(GetParam()) == TR::InstOpCode::bad)
        return;

    ASSERT_EQ(
        std::get<2>(GetParam()),
        findDifferingBits(std::get<0>(GetParam()), std::get<1>(GetParam()))
    );
}

class PPCDirectEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, BinaryInstruction>> {};

TEST_P(PPCDirectEncodingTest, encode) {
    auto instr = generateInstruction(cg(), std::get<0>(GetParam()), fakeNode);

    ASSERT_EQ(std::get<1>(GetParam()), encodeInstruction(instr));
}

class PPCLabelEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, ssize_t, BinaryInstruction>> {};

TEST_P(PPCLabelEncodingTest, encode) {
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(reinterpret_cast<uint8_t*>(buf) + std::get<1>(GetParam()));

    auto instr = generateLabelInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label
    );

    ASSERT_EQ(
        std::get<2>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCLabelEncodingTest, encodeWithRelocation) {
    if (std::get<1>(GetParam()) == -1)
        return;

    auto label = generateLabelSymbol(cg());
    auto instr = generateLabelInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label
    );

    auto size = encodeInstruction(instr)._words.size() * 4;
    label->setCodeLocation(reinterpret_cast<uint8_t*>(buf) + std::get<1>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(std::get<2>(GetParam()), getEncodedInstruction(size));
}

TEST_P(PPCLabelEncodingTest, labelLocation) {
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(reinterpret_cast<uint8_t*>(buf) + std::get<1>(GetParam()));

    auto instr = generateLabelInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label
    );

    encodeInstruction(instr);

    if (std::get<1>(GetParam()) == -1)
        ASSERT_EQ(instr->getBinaryEncoding(), label->getCodeLocation());
    else
        ASSERT_EQ(reinterpret_cast<uint8_t*>(buf) + std::get<1>(GetParam()), label->getCodeLocation());
}

class PPCConditionalBranchEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, ssize_t, BinaryInstruction>> {};

TEST_P(PPCConditionalBranchEncodingTest, encode) {
    auto label = std::get<2>(GetParam()) != -1 ? generateLabelSymbol(cg()) : NULL;
    if (label)
        label->setCodeLocation(reinterpret_cast<uint8_t*>(buf) + std::get<2>(GetParam()));

    auto instr = generateConditionalBranchInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

TEST_P(PPCConditionalBranchEncodingTest, encodeWithRelocation) {
    if (std::get<2>(GetParam()) == -1)
        return;

    auto label = generateLabelSymbol(cg());
    auto instr = generateConditionalBranchInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    auto size = encodeInstruction(instr)._words.size() * 4;
    label->setCodeLocation(reinterpret_cast<uint8_t*>(buf) + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(std::get<3>(GetParam()), getEncodedInstruction(size));
}

class PPCImmEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, uint32_t, BinaryInstruction>> {};

TEST_P(PPCImmEncodingTest, encode) {
    auto instr = generateImmInstruction(cg(), std::get<0>(GetParam()), fakeNode, std::get<1>(GetParam()));

    ASSERT_EQ(std::get<2>(GetParam()), encodeInstruction(instr));
}

class PPCImm2EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, uint32_t, uint32_t, BinaryInstruction>> {};

TEST_P(PPCImm2EncodingTest, encode) {
    auto instr = generateImm2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

TEST_P(PPCImm2EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateImm2Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        std::get<1>(GetParam()),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(std::get<3>(GetParam()) ^ differingBits, encodeInstruction(instr));
}

class PPCTrg1EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCTrg1EncodingTest, encode) {
    auto instr = generateTrg1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    ASSERT_EQ(
        std::get<2>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    ASSERT_EQ(
        std::get<2>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCSrc1EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, BinaryInstruction>> {};

TEST_P(PPCSrc1EncodingTest, encode) {
    auto instr = generateSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCSrc1EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateSrc1Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        (std::get<3>(GetParam()) ^ differingBits),
        encodeInstruction(instr)
    );
}

class PPCSrc2EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCSrc2EncodingTest, encode) {
    auto instr = generateSrc2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCSrc2EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateSrc2Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        (std::get<3>(GetParam()) ^ differingBits),
        encodeInstruction(instr)
    );
}

class PPCTrg1ImmEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, BinaryInstruction>> {};

TEST_P(PPCTrg1ImmEncodingTest, encode) {
    auto instr = generateTrg1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1ImmEncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1ImmInstruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        (std::get<3>(GetParam()) ^ differingBits),
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCTrg1Src1EncodingTest, encode) {
    auto instr = generateTrg1Src1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src1EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src1Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        (std::get<3>(GetParam()) ^ differingBits),
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1ImmEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, BinaryInstruction>> {};

TEST_P(PPCTrg1Src1ImmEncodingTest, encode) {
    auto instr = generateTrg1Src1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        (uintptr_t)std::get<3>(GetParam())
    );

    ASSERT_EQ(
        std::get<4>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src1ImmEncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src1ImmInstruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        (uintptr_t)std::get<3>(GetParam())
    );

    ASSERT_EQ(
        std::get<4>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1Imm2EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, uint64_t, BinaryInstruction>> {};

TEST_P(PPCTrg1Src1Imm2EncodingTest, encode) {
    auto instr = generateTrg1Src1Imm2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        std::get<3>(GetParam()),
        std::get<4>(GetParam())
    );

    ASSERT_EQ(
        std::get<5>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src1Imm2EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src1Imm2Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        std::get<3>(GetParam()),
        std::get<4>(GetParam())
    );

    ASSERT_EQ(
        std::get<5>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src2EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCTrg1Src2EncodingTest, encode) {
    auto instr = generateTrg1Src2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam()))
    );

    ASSERT_EQ(
        std::get<4>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src2EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src2Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam()))
    );

    ASSERT_EQ(
        std::get<4>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src2ImmEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint64_t, BinaryInstruction>> {};

TEST_P(PPCTrg1Src2ImmEncodingTest, encode) {
    auto instr = generateTrg1Src2ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam())),
        std::get<4>(GetParam())
    );

    ASSERT_EQ(
        std::get<5>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src2ImmEncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src2ImmInstruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam())),
        std::get<4>(GetParam())
    );

    ASSERT_EQ(
        std::get<5>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src3EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCTrg1Src3EncodingTest, encode) {
    auto instr = generateTrg1Src3Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam())),
        cg()->machine()->getRealRegister(std::get<4>(GetParam()))
    );

    ASSERT_EQ(
        std::get<5>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1Src3EncodingTest, encodeRecordForm) {
    TR::InstOpCode opCode = std::get<0>(GetParam());

    if (!opCode.hasRecordForm())
        return;

    TR::InstOpCode recordOpCode = opCode.getRecordFormOpCodeValue();
    auto differingBits = findDifferingBits(opCode, recordOpCode);
    auto instr = generateTrg1Src3Instruction(
        cg(),
        recordOpCode.getOpCodeValue(),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam())),
        cg()->machine()->getRealRegister(std::get<4>(GetParam()))
    );

    ASSERT_EQ(
        std::get<5>(GetParam()) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCMemEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, BinaryInstruction>> {};

TEST_P(PPCMemEncodingTest, encode) {
    auto instr = generateMemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()).reify(cg())
    );

    ASSERT_EQ(
        std::get<2>(GetParam()),
        encodeInstruction(instr)
    );
}

class PPCTrg1MemEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, BinaryInstruction>> {};

TEST_P(PPCTrg1MemEncodingTest, encode) {
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam()).reify(cg())
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1MemEncodingTest, encodeFlat) {
    TR::Instruction* instr;
    const MemoryReference& mr = std::get<2>(GetParam());

    if (mr.isIndexForm()) {
        instr = generateTrg1Src2Instruction(
            cg(),
            std::get<0>(GetParam()),
            fakeNode,
            cg()->machine()->getRealRegister(std::get<1>(GetParam())),
            cg()->machine()->getRealRegister(mr.baseReg()),
            cg()->machine()->getRealRegister(mr.indexReg())
        );
    } else {
        instr = generateTrg1Src1ImmInstruction(
            cg(),
            std::get<0>(GetParam()),
            fakeNode,
            cg()->machine()->getRealRegister(std::get<1>(GetParam())),
            cg()->machine()->getRealRegister(mr.baseReg()),
            mr.displacement()
        );
    }

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

class PPCMemSrc1EncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, BinaryInstruction>> {};

TEST_P(PPCMemSrc1EncodingTest, encode) {
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()).reify(cg()),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        std::get<3>(GetParam()),
        encodeInstruction(instr)
    );
}

class PPCFenceEncodingTest : public PowerBinaryEncoderTest, public ::testing::WithParamInterface<uint32_t> {};

TEST_P(PPCFenceEncodingTest, testEntryRelative32Bit) {
    uint32_t relo = 0xdeadc0deu;

    auto node = TR::Node::createRelative32BitFenceNode(reinterpret_cast<void*>(&relo));
    auto instr = generateAdminInstruction(cg(), TR::InstOpCode::fence, fakeNode, node);

    ASSERT_EQ(BinaryInstruction(), encodeInstruction(instr, GetParam()));
    ASSERT_EQ(GetParam(), relo);
}

/**
 * Note that the expected binary encoding results for these tests are hardcoded here. In most
 * cases, these values are generated by compiling an assembly file containing the test instructions
 * using GCC, then using objdump -d to disassemble the resulting object file.
 *
 * Unfortunately, due to the fact that the way we represent instructions is different from the way
 * that they're represented in the assembly expected by GCC (especially instructions like rldicl
 * and rlwinm), it is not possible to fully automate this process.
 */

INSTANTIATE_TEST_CASE_P(Special, PPCDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::assocreg, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,      0x00000000u),
    std::make_tuple(TR::InstOpCode::isync,    0x4c00012cu),
    std::make_tuple(TR::InstOpCode::lwsync,   0x7c2004acu),
    std::make_tuple(TR::InstOpCode::sync,     0x7c0004acu),
    std::make_tuple(TR::InstOpCode::nop,      0x60000000u)
));

INSTANTIATE_TEST_CASE_P(Special, PPCLabelEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::label, -1, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Special, PPCImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dd, 0x00000000u, 0x00000000u),
    std::make_tuple(TR::InstOpCode::dd, 0xffffffffu, 0xffffffffu),
    std::make_tuple(TR::InstOpCode::dd, 0x12345678u, 0x12345678u)
));

INSTANTIATE_TEST_CASE_P(Special, PPCFenceEncodingTest, ::testing::Values(0, 4, 8));

INSTANTIATE_TEST_CASE_P(Special, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::assocreg, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dd,       TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::isync,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::label,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwsync,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sync,     TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::nop,      TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Branch, PPCDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::bctr,  0x4e800420u),
    std::make_tuple(TR::InstOpCode::bctrl, 0x4e800421u),
    std::make_tuple(TR::InstOpCode::blr,   0x4e800020u),
    std::make_tuple(TR::InstOpCode::blrl,  0x4e800021u)
));

INSTANTIATE_TEST_CASE_P(Branch, PPCLabelEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::b,           0, 0x48000000u),
    std::make_tuple(TR::InstOpCode::b,          -4, 0x4bfffffcu),
    std::make_tuple(TR::InstOpCode::b,   0x1fffffc, 0x49fffffcu),
    std::make_tuple(TR::InstOpCode::b,  -0x2000000, 0x4a000000u),
    std::make_tuple(TR::InstOpCode::bl,          0, 0x48000001u),
    std::make_tuple(TR::InstOpCode::bl,         -4, 0x4bfffffdu),
    std::make_tuple(TR::InstOpCode::bl,  0x1fffffc, 0x49fffffdu),
    std::make_tuple(TR::InstOpCode::bl, -0x2000000, 0x4a000001u)
));

INSTANTIATE_TEST_CASE_P(Branch, PPCConditionalBranchEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, ssize_t, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr0,       0, 0x41820000u),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr0,      -4, 0x4182fffcu),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr7,  0x7ffc, 0x419e7ffcu),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr7, -0x8000, 0x419e8000u),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr0,       0, 0x41820001u),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr0,      -4, 0x4182fffdu),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr7,  0x7ffc, 0x419e7ffdu),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr7, -0x8000, 0x419e8001u),
    std::make_tuple(TR::InstOpCode::beqlr, TR::RealRegister::cr0,      -1, 0x4d820020u),
    std::make_tuple(TR::InstOpCode::beqlr, TR::RealRegister::cr7,      -1, 0x4d9e0020u),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr0,       0, 0x40800000u),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr0,      -4, 0x4080fffcu),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr7,  0x7ffc, 0x409c7ffcu),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr7, -0x8000, 0x409c8000u),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr0,       0, 0x40800001u),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr0,      -4, 0x4080fffdu),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr7,  0x7ffc, 0x409c7ffdu),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr7, -0x8000, 0x409c8001u),
    std::make_tuple(TR::InstOpCode::bgelr, TR::RealRegister::cr0,      -1, 0x4c800020u),
    std::make_tuple(TR::InstOpCode::bgelr, TR::RealRegister::cr7,      -1, 0x4c9c0020u),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr0,       0, 0x41810000u),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr0,      -4, 0x4181fffcu),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr7,  0x7ffc, 0x419d7ffcu),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr7, -0x8000, 0x419d8000u),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr0,       0, 0x41810001u),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr0,      -4, 0x4181fffdu),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr7,  0x7ffc, 0x419d7ffdu),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr7, -0x8000, 0x419d8001u),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::RealRegister::cr0,      -1, 0x4d810020u),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::RealRegister::cr7,      -1, 0x4d9d0020u),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr0,       0, 0x40810000u),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr0,      -4, 0x4081fffcu),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr7,  0x7ffc, 0x409d7ffcu),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr7, -0x8000, 0x409d8000u),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr0,       0, 0x40810001u),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr0,      -4, 0x4081fffdu),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr7,  0x7ffc, 0x409d7ffdu),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr7, -0x8000, 0x409d8001u),
    std::make_tuple(TR::InstOpCode::blelr, TR::RealRegister::cr0,      -1, 0x4c810020u),
    std::make_tuple(TR::InstOpCode::blelr, TR::RealRegister::cr7,      -1, 0x4c9d0020u),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr0,       0, 0x41800000u),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr0,      -4, 0x4180fffcu),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr7,  0x7ffc, 0x419c7ffcu),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr7, -0x8000, 0x419c8000u),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr0,       0, 0x41800001u),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr0,      -4, 0x4180fffdu),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr7,  0x7ffc, 0x419c7ffdu),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr7, -0x8000, 0x419c8001u),
    std::make_tuple(TR::InstOpCode::bltlr, TR::RealRegister::cr0,      -1, 0x4d800020u),
    std::make_tuple(TR::InstOpCode::bltlr, TR::RealRegister::cr7,      -1, 0x4d9c0020u),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr0,       0, 0x40820000u),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr0,      -4, 0x4082fffcu),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr7,  0x7ffc, 0x409e7ffcu),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr7, -0x8000, 0x409e8000u),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr0,       0, 0x40820001u),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr0,      -4, 0x4082fffdu),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr7,  0x7ffc, 0x409e7ffdu),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr7, -0x8000, 0x409e8001u),
    std::make_tuple(TR::InstOpCode::bnelr, TR::RealRegister::cr0,      -1, 0x4c820020u),
    std::make_tuple(TR::InstOpCode::bnelr, TR::RealRegister::cr7,      -1, 0x4c9e0020u),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr0,       0, 0x40830000u),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr0,      -4, 0x4083fffcu),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr7,  0x7ffc, 0x409f7ffcu),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr7, -0x8000, 0x409f8000u),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr0,       0, 0x41830000u),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr0,      -4, 0x4183fffcu),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr7,  0x7ffc, 0x419f7ffcu),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr7, -0x8000, 0x419f8000u)
)));

INSTANTIATE_TEST_CASE_P(Branch, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::b,     TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bctr,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bctrl, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beq,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beql,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beqlr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bge,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgel,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgelr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bl,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ble,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blel,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blelr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blr,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blrl,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bltl,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bltlr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bne,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnel,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnelr, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnun,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bun,   TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCImm2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x0, 0xfc01010cu),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0xf, 0x0, 0xfc01f10cu),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0xf, 0xff80010cu),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x7, 0xff81010cu),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x8, 0xfc00010cu)
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCTrg1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfcr,      TR::RealRegister::gr0,  0x7c000026u),
    std::make_tuple(TR::InstOpCode::mfcr,      TR::RealRegister::gr31, 0x7fe00026u),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::RealRegister::gr0,  0x7c0902a6u),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::RealRegister::gr31, 0x7fe902a6u),
    std::make_tuple(TR::InstOpCode::mffs,      TR::RealRegister::gr0,  0xfc00048eu),
    std::make_tuple(TR::InstOpCode::mffs,      TR::RealRegister::gr31, 0xffe0048eu),
    std::make_tuple(TR::InstOpCode::mflr,      TR::RealRegister::gr0,  0x7c0802a6u),
    std::make_tuple(TR::InstOpCode::mflr,      TR::RealRegister::gr31, 0x7fe802a6u),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::RealRegister::gr0,  0x7c0222a6u),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::RealRegister::gr31, 0x7fe222a6u),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::RealRegister::gr0,  0x7c0322a6u),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::RealRegister::gr31, 0x7fe322a6u)
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrfs, TR::RealRegister::cr7, 0, 0xff800080u),
    std::make_tuple(TR::InstOpCode::mcrfs, TR::RealRegister::cr0, 7, 0xfc1c0080u)
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCSrc1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtctr,  TR::RealRegister::gr0,     0u, 0x7c0903a6u),
    std::make_tuple(TR::InstOpCode::mtctr,  TR::RealRegister::gr31,    0u, 0x7fe903a6u),
    std::make_tuple(TR::InstOpCode::mtfsf,  TR::RealRegister::fp0,  0xffu, 0xfdfe058eu),
    std::make_tuple(TR::InstOpCode::mtfsf,  TR::RealRegister::fp31, 0x00u, 0xfc00fd8eu),
    std::make_tuple(TR::InstOpCode::mtfsfl, TR::RealRegister::fp0,  0xffu, 0xfffe058eu),
    std::make_tuple(TR::InstOpCode::mtfsfl, TR::RealRegister::fp31, 0x00u, 0xfe00fd8eu),
    std::make_tuple(TR::InstOpCode::mtfsfw, TR::RealRegister::fp0,  0xffu, 0xfdff058eu),
    std::make_tuple(TR::InstOpCode::mtfsfw, TR::RealRegister::fp31, 0x00u, 0xfc01fd8eu),
    std::make_tuple(TR::InstOpCode::mtlr,   TR::RealRegister::gr0,     0u, 0x7c0803a6u),
    std::make_tuple(TR::InstOpCode::mtlr,   TR::RealRegister::gr31,    0u, 0x7fe803a6u)
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrfs,     TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfcr,      TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mffs,      TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mflr,      TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtctr,     TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtfsf,     TR::InstOpCode::mtfsf_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::mtfsfi,    TR::InstOpCode::mtfsfi_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::mtfsfl,    TR::InstOpCode::mtfsfl_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::mtfsfw,    TR::InstOpCode::mtfsfw_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::mtlr,      TR::InstOpCode::bad,      BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Trap, PPCSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr31, 0x00000000u, 0x089f0000u),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr0,  0x00007fffu, 0x08807fffu),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr31, 0xffff8000u, 0x089f8000u),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr0,  0xffffffffu, 0x0880ffffu),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr31, 0x00000000u, 0x099f0000u),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr0,  0x00007fffu, 0x09807fffu),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr31, 0xffff8000u, 0x099f8000u),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr0,  0xffffffffu, 0x0980ffffu),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr31, 0x00000000u, 0x091f0000u),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr0,  0x00007fffu, 0x09007fffu),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr31, 0xffff8000u, 0x091f8000u),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr0,  0xffffffffu, 0x0900ffffu),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr31, 0x00000000u, 0x0a9f0000u),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr0,  0x00007fffu, 0x0a807fffu),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr31, 0xffff8000u, 0x0a9f8000u),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr0,  0xffffffffu, 0x0a80ffffu),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr31, 0x00000000u, 0x08bf0000u),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr0,  0x00007fffu, 0x08a07fffu),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr31, 0xffff8000u, 0x08bf8000u),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr0,  0xffffffffu, 0x08a0ffffu),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr31, 0x00000000u, 0x083f0000u),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr0,  0x00007fffu, 0x08207fffu),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr31, 0xffff8000u, 0x083f8000u),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr0,  0xffffffffu, 0x0820ffffu),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr31, 0x00000000u, 0x08df0000u),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr0,  0x00007fffu, 0x08c07fffu),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr31, 0xffff8000u, 0x08df8000u),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr0,  0xffffffffu, 0x08c0ffffu),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr31, 0x00000000u, 0x085f0000u),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr0,  0x00007fffu, 0x08407fffu),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr31, 0xffff8000u, 0x085f8000u),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr0,  0xffffffffu, 0x0840ffffu),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr31, 0x00000000u, 0x0a1f0000u),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr0,  0x00007fffu, 0x0a007fffu),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr31, 0xffff8000u, 0x0a1f8000u),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr0,  0xffffffffu, 0x0a00ffffu),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr31, 0x00000000u, 0x0b1f0000u),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr0,  0x00007fffu, 0x0b007fffu),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr31, 0xffff8000u, 0x0b1f8000u),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr0,  0xffffffffu, 0x0b00ffffu),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr31, 0x00000000u, 0x0c9f0000u),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr0,  0x00007fffu, 0x0c807fffu),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr31, 0xffff8000u, 0x0c9f8000u),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr0,  0xffffffffu, 0x0c80ffffu),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr31, 0x00000000u, 0x0d9f0000u),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr0,  0x00007fffu, 0x0d807fffu),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr31, 0xffff8000u, 0x0d9f8000u),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr0,  0xffffffffu, 0x0d80ffffu),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr31, 0x00000000u, 0x0d1f0000u),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr0,  0x00007fffu, 0x0d007fffu),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr31, 0xffff8000u, 0x0d1f8000u),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr0,  0xffffffffu, 0x0d00ffffu),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr31, 0x00000000u, 0x0e9f0000u),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr0,  0x00007fffu, 0x0e807fffu),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr31, 0xffff8000u, 0x0e9f8000u),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr0,  0xffffffffu, 0x0e80ffffu),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr31, 0x00000000u, 0x0cbf0000u),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr0,  0x00007fffu, 0x0ca07fffu),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr31, 0xffff8000u, 0x0cbf8000u),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr0,  0xffffffffu, 0x0ca0ffffu),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr31, 0x00000000u, 0x0c3f0000u),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr0,  0x00007fffu, 0x0c207fffu),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr31, 0xffff8000u, 0x0c3f8000u),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr0,  0xffffffffu, 0x0c20ffffu),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr31, 0x00000000u, 0x0cdf0000u),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr0,  0x00007fffu, 0x0cc07fffu),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr31, 0xffff8000u, 0x0cdf8000u),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr0,  0xffffffffu, 0x0cc0ffffu),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr31, 0x00000000u, 0x0c5f0000u),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr0,  0x00007fffu, 0x0c407fffu),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr31, 0xffff8000u, 0x0c5f8000u),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr0,  0xffffffffu, 0x0c40ffffu),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr31, 0x00000000u, 0x0e1f0000u),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr0,  0x00007fffu, 0x0e007fffu),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr31, 0xffff8000u, 0x0e1f8000u),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr0,  0xffffffffu, 0x0e00ffffu),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr31, 0x00000000u, 0x0f1f0000u),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr0,  0x00007fffu, 0x0f007fffu),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr31, 0xffff8000u, 0x0f1f8000u),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr0,  0xffffffffu, 0x0f00ffffu)
)));

INSTANTIATE_TEST_CASE_P(Trap, PPCSrc2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tdeq,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c9f0088u),
    std::make_tuple(TR::InstOpCode::tdeq,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c80f888u),
    std::make_tuple(TR::InstOpCode::tdge,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7d9f0088u),
    std::make_tuple(TR::InstOpCode::tdge,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7d80f888u),
    std::make_tuple(TR::InstOpCode::tdgt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7d1f0088u),
    std::make_tuple(TR::InstOpCode::tdgt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7d00f888u),
    std::make_tuple(TR::InstOpCode::tdle,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7e9f0088u),
    std::make_tuple(TR::InstOpCode::tdle,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7e80f888u),
    std::make_tuple(TR::InstOpCode::tdlge, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7cbf0088u),
    std::make_tuple(TR::InstOpCode::tdlge, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7ca0f888u),
    std::make_tuple(TR::InstOpCode::tdlgt, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c3f0088u),
    std::make_tuple(TR::InstOpCode::tdlgt, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c20f888u),
    std::make_tuple(TR::InstOpCode::tdlle, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7cdf0088u),
    std::make_tuple(TR::InstOpCode::tdlle, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7cc0f888u),
    std::make_tuple(TR::InstOpCode::tdllt, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c5f0088u),
    std::make_tuple(TR::InstOpCode::tdllt, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c40f888u),
    std::make_tuple(TR::InstOpCode::tdlt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7e1f0088u),
    std::make_tuple(TR::InstOpCode::tdlt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7e00f888u),
    std::make_tuple(TR::InstOpCode::tdneq, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7f1f0088u),
    std::make_tuple(TR::InstOpCode::tdneq, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7f00f888u),
    std::make_tuple(TR::InstOpCode::tweq,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c9f0008u),
    std::make_tuple(TR::InstOpCode::tweq,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c80f808u),
    std::make_tuple(TR::InstOpCode::twge,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7d9f0008u),
    std::make_tuple(TR::InstOpCode::twge,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7d80f808u),
    std::make_tuple(TR::InstOpCode::twgt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7d1f0008u),
    std::make_tuple(TR::InstOpCode::twgt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7d00f808u),
    std::make_tuple(TR::InstOpCode::twle,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7e9f0008u),
    std::make_tuple(TR::InstOpCode::twle,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7e80f808u),
    std::make_tuple(TR::InstOpCode::twlge, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7cbf0008u),
    std::make_tuple(TR::InstOpCode::twlge, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7ca0f808u),
    std::make_tuple(TR::InstOpCode::twlgt, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c3f0008u),
    std::make_tuple(TR::InstOpCode::twlgt, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c20f808u),
    std::make_tuple(TR::InstOpCode::twlle, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7cdf0008u),
    std::make_tuple(TR::InstOpCode::twlle, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7cc0f808u),
    std::make_tuple(TR::InstOpCode::twllt, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c5f0008u),
    std::make_tuple(TR::InstOpCode::twllt, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c40f808u),
    std::make_tuple(TR::InstOpCode::twlt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7e1f0008u),
    std::make_tuple(TR::InstOpCode::twlt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7e00f808u),
    std::make_tuple(TR::InstOpCode::twneq, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7f1f0008u),
    std::make_tuple(TR::InstOpCode::twneq, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7f00f808u)
));

INSTANTIATE_TEST_CASE_P(Trap, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tdeq,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdge,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdle,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlge,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgt,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlle,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllei, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllt,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllti, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdneq,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneqi, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tweq,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twge,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgei,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgti,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twle,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlei,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlge,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgei, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgt,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgti, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlle,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllei, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllt,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllti, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlti,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneq,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneqi, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x38220000u),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x3be07fffu),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, 0x381f8000u),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, 0x39f0ffffu),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x30220000u),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x33e07fffu),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, 0x301f8000u),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, 0x31f0ffffu),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x3c220000u),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x3fe07fffu),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, 0x3c1f8000u),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, 0x3df0ffffu),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x1c220000u),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x1fe07fffu),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, 0x1c1f8000u),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, 0x1df0ffffu),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x20220000u),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x23e07fffu),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, 0x201f8000u),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, 0x21f0ffffu)
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr31, 0x00000000u, 0x3be00000u),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr0,  0x00007fffu, 0x38007fffu),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr31, 0xffff8000u, 0x3be08000u),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr0,  0xffffffffu, 0x3800ffffu),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr31, 0x00000000u, 0x3fe00000u),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr0,  0x00007fffu, 0x3c007fffu),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr31, 0xffff8000u, 0x3fe08000u),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr0,  0xffffffffu, 0x3c00ffffu)
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::addme,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe001d4u),
    std::make_tuple(TR::InstOpCode::addme,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f01d4u),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe005d4u),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f05d4u),
    std::make_tuple(TR::InstOpCode::addze,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00194u),
    std::make_tuple(TR::InstOpCode::addze,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f0194u),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00594u),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f0594u),
    std::make_tuple(TR::InstOpCode::neg,     TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe000d0u),
    std::make_tuple(TR::InstOpCode::neg,     TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f00d0u),
    std::make_tuple(TR::InstOpCode::nego,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe004d0u),
    std::make_tuple(TR::InstOpCode::nego,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f04d0u),
    std::make_tuple(TR::InstOpCode::subfme,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe001d0u),
    std::make_tuple(TR::InstOpCode::subfme,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f01d0u),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe005d0u),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f05d0u),
    std::make_tuple(TR::InstOpCode::subfze,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00190u),
    std::make_tuple(TR::InstOpCode::subfze,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f0190u),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00590u),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c1f0590u)
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00214u),
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0214u),
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fa14u),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00014u),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0014u),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f814u),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00414u),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0414u),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fc14u),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00114u),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0114u),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f914u),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00514u),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0514u),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fd14u),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00614u),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0614u),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fe14u),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe003d2u),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f03d2u),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fbd2u),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe007d2u),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f07d2u),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00ffd2u),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00392u),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0392u),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fb92u),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00792u),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0792u),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00ff92u),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe003d6u),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f03d6u),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fbd6u),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe007d6u),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f07d6u),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00ffd6u),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00396u),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0396u),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fb96u),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00796u),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0796u),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00ff96u),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00212u),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0212u),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fa12u),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00612u),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0612u),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fe12u),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00216u),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0216u),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fa16u),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00616u),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0616u),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fe16u),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00092u),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0092u),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f892u),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00012u),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0012u),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f812u),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00096u),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0096u),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f896u),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00016u),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0016u),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f816u),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe001d2u),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f01d2u),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f9d2u),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe005d2u),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f05d2u),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fdd2u),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe001d6u),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f01d6u),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f9d6u),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe005d6u),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f05d6u),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fdd6u),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00050u),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0050u),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f850u),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00010u),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0010u),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f810u),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00410u),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0410u),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fc10u),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00110u),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0110u),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f910u),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fe00510u),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0510u),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fd10u)
)));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x13e00033u),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x101f0033u),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x1000f833u),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x100007f3u)
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::add,     TR::InstOpCode::add_r,     0x00000001u),
    std::make_tuple(TR::InstOpCode::addc,    TR::InstOpCode::addc_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::addco,   TR::InstOpCode::addco_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::adde,    TR::InstOpCode::adde_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::addeo,   TR::InstOpCode::addeo_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::addo,    TR::InstOpCode::addo_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::addi,    TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::addic,   TR::InstOpCode::addic_r,   0x04000000u),
    std::make_tuple(TR::InstOpCode::addis,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::addme,   TR::InstOpCode::addme_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::InstOpCode::addmeo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::addze,   TR::InstOpCode::addze_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::InstOpCode::addzeo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::divd,    TR::InstOpCode::divd_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::divdo,   TR::InstOpCode::divdo_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::divdu,   TR::InstOpCode::divdu_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::divwuo,  TR::InstOpCode::divwuo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::divw,    TR::InstOpCode::divw_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::divwo,   TR::InstOpCode::divwo_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::divwu,   TR::InstOpCode::divwu_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::divwuo,  TR::InstOpCode::divwuo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::li,      TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lis,     TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::maddld,  TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modsd,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modsw,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modud,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::moduw,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mulhd,   TR::InstOpCode::mulhd_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::mulhdu,  TR::InstOpCode::mulhdu_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::mulhw,   TR::InstOpCode::mulhw_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::mulhwu,  TR::InstOpCode::mulhwu_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::mulld,   TR::InstOpCode::mulld_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::mulldo,  TR::InstOpCode::mulldo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::mulli,   TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mullw,   TR::InstOpCode::mullw_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::mullwo,  TR::InstOpCode::mullwo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::neg,     TR::InstOpCode::neg_r,     0x00000001u),
    std::make_tuple(TR::InstOpCode::nego,    TR::InstOpCode::nego_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::subf,    TR::InstOpCode::subf_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::subfc,   TR::InstOpCode::subfc_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::subfco,  TR::InstOpCode::subfco_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::subfe,   TR::InstOpCode::subfe_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::subfeo,  TR::InstOpCode::subfeo_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::subfic,  TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::subfme,  TR::InstOpCode::subfme_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::InstOpCode::subfmeo_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::subfze,  TR::InstOpCode::subfze_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::InstOpCode::subfzeo_r, 0x00000001u)
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, 0x2c820000u),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, 0x2f807fffu),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr0, TR::RealRegister::gr31, 0xffff8000u, 0x2c1f8000u),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr3, TR::RealRegister::gr16, 0xffffffffu, 0x2d90ffffu),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, 0x2ca20000u),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, 0x2fa07fffu),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr0, TR::RealRegister::gr31, 0xffff8000u, 0x2c3f8000u),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr3, TR::RealRegister::gr16, 0xffffffffu, 0x2db0ffffu),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, 0x28820000u),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, 0x2b807fffu),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr0, TR::RealRegister::gr31, 0x00008000u, 0x281f8000u),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr3, TR::RealRegister::gr16, 0x0000ffffu, 0x2990ffffu),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, 0x28a20000u),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, 0x2ba07fffu),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr0, TR::RealRegister::gr31, 0x00008000u, 0x283f8000u),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr3, TR::RealRegister::gr16, 0x0000ffffu, 0x29b0ffffu)
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7f800000u),
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0000u),
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f800u),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fa00000u),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c3f0000u),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c20f800u),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7f800040u),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0040u),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f840u),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7fa00040u),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c3f0040u),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c20f840u),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7f8001c0u),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f01c0u),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f9c0u),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr7, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xff800040u),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr0, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f0040u),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr0, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f840u),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr7, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xff800000u),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr0, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f0000u),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr0, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f800u)
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0, 0x7f800180u),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0, 0x7c1f0180u),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0, 0x7c00f980u),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  1, 0x7fa00180u),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  1, 0x7c3f0180u),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 1, 0x7c20f980u)
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmp4,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmprb,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrf, TR::RealRegister::cr7,  TR::RealRegister::cr0, 0x4f800000u),
    std::make_tuple(TR::InstOpCode::mcrf, TR::RealRegister::cr0,  TR::RealRegister::cr7, 0x4c1c0000u),
    std::make_tuple(TR::InstOpCode::setb, TR::RealRegister::gr31, TR::RealRegister::cr0, 0x7fe00100u),
    std::make_tuple(TR::InstOpCode::setb, TR::RealRegister::gr0,  TR::RealRegister::cr7, 0x7c1c0100u)
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCSrc1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtcr,   TR::RealRegister::gr0,      0, 0x7c0ff120u),
    std::make_tuple(TR::InstOpCode::mtcr,   TR::RealRegister::gr31,     0, 0x7feff120u),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr0,  0x01u, 0x7c101120u),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr31, 0x01u, 0x7ff01120u),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr0,  0x80u, 0x7c180120u),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr31, 0x80u, 0x7ff80120u)
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr0,  0x01u, 0x7c101026u),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr31, 0x01u, 0x7ff01026u),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr0,  0x80u, 0x7c180026u),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr31, 0x80u, 0x7ff80026u)
));

#define CR_RT(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RT)
#define CR_RA(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RA)
#define CR_RB(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RB)
#define CR_TEST_ALL (CR_RT(LT) | CR_RA(GT) | CR_RB(EQ))

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src2ImmEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint64_t, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800202u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0202u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e202u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600202u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030202u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001a02u),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897202u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800102u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0102u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e102u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600102u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030102u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001902u),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897102u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800242u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0242u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e242u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600242u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030242u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001a42u),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897242u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f8001c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c01c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e1c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c6001c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c0301c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c0019c2u),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c8971c2u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800042u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0042u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e042u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600042u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030042u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001842u),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897042u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800382u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0382u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e382u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600382u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030382u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001b82u),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897382u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800342u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0342u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e342u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600342u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030342u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001b42u),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897342u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           0x4f800182u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           0x4c1c0182u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           0x4c00e182u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   0x4c600182u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   0x4c030182u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   0x4c001982u),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, 0x4c897182u)
)));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::crand,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crandc, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::creqv,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crnand, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crnor,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cror,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crorc,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crxor,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mcrf,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtcr,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::setb,   TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x70410000u),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x701f7fffu),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x73e08000u),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x720fffffu),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x74410000u),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x741f7fffu),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x77e08000u),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x760fffffu),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x60410000u),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x601f7fffu),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x63e08000u),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x620fffffu),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x64410000u),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x641f7fffu),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x67e08000u),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x660fffffu),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x68410000u),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x681f7fffu),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x6be08000u),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x6a0fffffu),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, 0x6c410000u),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, 0x6c1f7fffu),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, 0x6fe08000u),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, 0x6e0fffffu)
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::extsb, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0774u),
    std::make_tuple(TR::InstOpCode::extsb, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe00774u),
    std::make_tuple(TR::InstOpCode::extsh, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0734u),
    std::make_tuple(TR::InstOpCode::extsh, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe00734u),
    std::make_tuple(TR::InstOpCode::extsw, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f07b4u),
    std::make_tuple(TR::InstOpCode::extsw, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe007b4u),
    std::make_tuple(TR::InstOpCode::mr,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x601f0000u),
    std::make_tuple(TR::InstOpCode::mr,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x63e00000u)
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0038u),
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00038u),
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f838u),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0078u),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00078u),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f878u),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0238u),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00238u),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fa38u),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f03b8u),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe003b8u),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fbb8u),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f00f8u),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe000f8u),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f8f8u),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0378u),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00378u),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fb78u),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0338u),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00338u),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fb38u),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0278u),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00278u),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fa78u)
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::AND,   TR::InstOpCode::and_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::andc,  TR::InstOpCode::andc_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::bad,   TR::InstOpCode::andi_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,   TR::InstOpCode::andis_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::extsb, TR::InstOpCode::extsb_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::extsw, TR::InstOpCode::extsw_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::eqv,   TR::InstOpCode::eqv_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::mr,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::nand,  TR::InstOpCode::nand_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::OR,    TR::InstOpCode::or_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::orc,   TR::InstOpCode::orc_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::ori,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::oris,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::XOR,   TR::InstOpCode::xor_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::xori,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xoris, TR::InstOpCode::bad,     BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(BitCounting, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0074u),
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe00074u),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f0034u),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe00034u),
    std::make_tuple(TR::InstOpCode::popcntd, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f03f4u),
    std::make_tuple(TR::InstOpCode::popcntd, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe003f4u),
    std::make_tuple(TR::InstOpCode::popcntw, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7c1f02f4u),
    std::make_tuple(TR::InstOpCode::popcntw, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7fe002f4u)
));

INSTANTIATE_TEST_CASE_P(BitCounting, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::InstOpCode::cntlzd_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::InstOpCode::cntlzw_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::popcntd, TR::InstOpCode::bad,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::popcntw, TR::InstOpCode::bad,      BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, 0x7c4106f4u),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, 0x7c1fa6f4u),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr0,  TR::RealRegister::gr31, 63u, 0x7fe0fef6u),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr15, TR::RealRegister::gr16, 32u, 0x7e0f06f6u),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, 0x7c410674u),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, 0x7c1fa674u),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 63u, 0x7fe0fe76u),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr15, TR::RealRegister::gr16, 32u, 0x7e0f0676u),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, 0x7c410670u),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, 0x7c1fa670u),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 31u, 0x7fe0fe70u),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr15, TR::RealRegister::gr16, 16u, 0x7e0f8670u)
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src1Imm2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, 0x781f0008u),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, 0x7be00008u),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, 0x7800f80au),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, 0x780007e8u),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, 0x7800f808u),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, 0x7800f828u),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffff00000000ull, 0x7800000au),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffff00000000ull, 0x7800040au),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, 0x780007c8u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, 0x781f0000u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, 0x7be00000u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0xffffffffffffffffull, 0x7800f802u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, 0x780007e0u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffffffffffffull, 0x7800f800u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x00000000ffffffffull, 0x7800f820u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffffffffffffull, 0x78000002u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffffffffffffull, 0x78000402u),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, 0x780007c0u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x8000000000000000ull, 0x781f0004u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x8000000000000000ull, 0x7be00004u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, 0x7800f806u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, 0x780007e4u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffffffffffffull, 0x7800ffe4u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, 0x7800f824u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffffffffffffull, 0x780007e6u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffff000000000000ull, 0x780003c6u),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0xffffffff00000000ull, 0x780007c4u),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, 0x781f000cu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, 0x7be0000cu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, 0x7800f80eu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, 0x780007ecu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, 0x7800f80cu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, 0x7800f82cu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffff00000000ull, 0x7800000eu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffff00000000ull, 0x7800040eu),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, 0x780007ccu),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x0000000080000000ull, 0x501f0000u),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x0000000080000000ull, 0x53e00000u),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, 0x5000f800u),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000080000001ull, 0x500007c0u),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ffffffffull, 0x5000003eu),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000ffff00ull, 0x5000022eu),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ff0000ffull, 0x5000060eu),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x0000000080000000ull, 0x541f0000u),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x0000000080000000ull, 0x57e00000u),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, 0x5400f800u),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000080000001ull, 0x540007c0u),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ffffffffull, 0x5400003eu),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000ffff00ull, 0x5400022eu),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ff0000ffull, 0x5400060eu)
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0036u),
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00036u),
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f836u),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0030u),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00030u),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00f830u),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0634u),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00634u),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fe34u),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0630u),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00630u),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fe30u),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0436u),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00436u),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fc36u),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x7c1f0430u),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x7fe00430u),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x7c00fc30u)
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0xffffffffffffffffull, 0x781f0010u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0xffffffffffffffffull, 0x7be00010u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffffffffffffffffull, 0x7800f810u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000000001ull, 0x780007f0u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ffffffffull, 0x78000030u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000001ffffffffull, 0x780007d0u),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000ffffffffffffull, 0x78000410u),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ffffffffull, 0x5c1f003eu),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00000000ffffffffull, 0x5fe0003eu),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00000000ffffffffull, 0x5c00f83eu),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000080000000ull, 0x5c000000u),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000000001ull, 0x5c0007feu),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000080000001ull, 0x5c0007c0u),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000ffff00ull, 0x5c00022eu),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ff0000ffull, 0x5c00060eu)
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::extswsli, TR::InstOpCode::extswsli_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::rldcl,    TR::InstOpCode::rldcl_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::rldic,    TR::InstOpCode::rldic_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::rldicl,   TR::InstOpCode::rldicl_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::rldicr,   TR::InstOpCode::rldicr_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::rldimi,   TR::InstOpCode::rldimi_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::rlwimi,   TR::InstOpCode::rlwimi_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::rlwinm,   TR::InstOpCode::rlwinm_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::rlwnm,    TR::InstOpCode::rlwnm_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::sld,      TR::InstOpCode::sld_r,      0x00000001u),
    std::make_tuple(TR::InstOpCode::slw,      TR::InstOpCode::slw_r,      0x00000001u),
    std::make_tuple(TR::InstOpCode::srad,     TR::InstOpCode::srad_r,     0x00000001u),
    std::make_tuple(TR::InstOpCode::sradi,    TR::InstOpCode::sradi_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::sraw,     TR::InstOpCode::sraw_r,     0x00000001u),
    std::make_tuple(TR::InstOpCode::srawi,    TR::InstOpCode::srawi_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::srd,      TR::InstOpCode::srd_r,      0x00000001u),
    std::make_tuple(TR::InstOpCode::srw,      TR::InstOpCode::srw_r,      0x00000001u)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr31, 0x00000000u, 0x13e0030cu),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr0,  0x0000000fu, 0x100f030cu),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr15, 0xfffffff0u, 0x11f0030cu),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr16, 0xffffffffu, 0x121f030cu),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr31, 0x00000000u, 0x13e0034cu),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr0,  0x0000000fu, 0x100f034cu),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr15, 0xfffffff0u, 0x11f0034cu),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr16, 0xffffffffu, 0x121f034cu),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr31, 0x00000000u, 0x13e0038cu),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr0,  0x0000000fu, 0x100f038cu),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr15, 0xfffffff0u, 0x11f0038cu),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr16, 0xffffffffu, 0x121f038cu)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, 0x1020120cu),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr31, TR::RealRegister::vr0,   5u, 0x13e5020cu),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr0,  TR::RealRegister::vr31, 15u, 0x100ffa0cu),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr15, TR::RealRegister::vr16,  7u, 0x11e7820cu),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, 0x1020124cu),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr31, TR::RealRegister::vr0,   5u, 0x13e5024cu),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr0,  TR::RealRegister::vr31,  7u, 0x1007fa4cu),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr15, TR::RealRegister::vr16,  3u, 0x11e3824cu),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, 0x1020128cu),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr31, TR::RealRegister::vr0,   2u, 0x13e2028cu),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr0,  TR::RealRegister::vr31,  3u, 0x1003fa8cu),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr15, TR::RealRegister::vr16,  1u, 0x11e1828cu)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vupkhsb, TR::RealRegister::vr31, TR::RealRegister::vr0,  0x13e0020eu),
    std::make_tuple(TR::InstOpCode::vupkhsb, TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa0eu),
    std::make_tuple(TR::InstOpCode::vupkhsh, TR::RealRegister::vr31, TR::RealRegister::vr0,  0x13e0024eu),
    std::make_tuple(TR::InstOpCode::vupkhsh, TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa4eu),
    std::make_tuple(TR::InstOpCode::vupklsb, TR::RealRegister::vr31, TR::RealRegister::vr0,  0x13e0028eu),
    std::make_tuple(TR::InstOpCode::vupklsb, TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa8eu),
    std::make_tuple(TR::InstOpCode::vupklsh, TR::RealRegister::vr31, TR::RealRegister::vr0,  0x13e002ceu),
    std::make_tuple(TR::InstOpCode::vupklsh, TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000faceu)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00300u),
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0300u),
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb00u),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00340u),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0340u),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb40u),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00380u),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0380u),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb80u),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00000u),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0000u),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f800u),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00200u),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0200u),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa00u),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e000c0u),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f00c0u),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f8c0u),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00040u),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0040u),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f840u),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00240u),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0240u),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa40u),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00080u),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0080u),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f880u),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00280u),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0280u),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa80u),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00404u),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0404u),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc04u),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00444u),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0444u),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc44u),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00006u),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0006u),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f806u),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00046u),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0046u),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f846u),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00086u),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0086u),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f886u),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00306u),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0306u),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb06u),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00346u),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0346u),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb46u),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00386u),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0386u),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb86u),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00206u),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0206u),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa06u),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00246u),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0246u),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa46u),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00286u),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0286u),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa86u),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00102u),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0102u),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f902u),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00142u),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0142u),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f942u),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00182u),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0182u),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f982u),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00002u),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0002u),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f802u),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00042u),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0042u),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f842u),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00082u),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0082u),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f882u),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00302u),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0302u),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb02u),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00342u),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0342u),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb42u),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00382u),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0382u),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb82u),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00202u),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0202u),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa02u),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00242u),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0242u),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa42u),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00282u),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0282u),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa82u),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00348u),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0348u),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb48u),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00148u),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0148u),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f948u),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00048u),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0048u),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f848u),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00089u),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0089u),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f889u),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00504u),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0504u),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fd04u),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00484u),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0484u),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc84u),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00004u),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0004u),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f804u),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00044u),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0044u),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f844u),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00084u),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0084u),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f884u),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e001c4u),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f01c4u),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f9c4u),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00104u),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0104u),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f904u),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00144u),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0144u),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f944u),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e0040cu),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f040cu),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc0cu),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00184u),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0184u),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000f984u),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e002c4u),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f02c4u),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fac4u),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00304u),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0304u),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb04u),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00344u),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0344u),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb44u),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00384u),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0384u),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fb84u),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00204u),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0204u),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa04u),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00244u),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0244u),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa44u),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e0044cu),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f044cu),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc4cu),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00284u),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0284u),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fa84u),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00700u),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0700u),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000ff00u),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00740u),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0740u),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000ff40u),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00780u),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0780u),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000ff80u),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00400u),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0400u),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc00u),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00600u),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0600u),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fe00u),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e004c0u),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f04c0u),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fcc0u),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00440u),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0440u),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc40u),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00640u),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0640u),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fe40u),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00480u),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0480u),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fc80u),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00680u),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f0680u),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fe80u),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e004c4u),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x101f04c4u),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x1000fcc4u)
)));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,   0, 0x13e0002cu),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,   0, 0x101f002cu),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31,  0, 0x1000f82cu),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  15, 0x100003ecu)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e00026u),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x101f0026u),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x1000f826u),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x100007e6u),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e0002bu),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x101f002bu),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x1000f82bu),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x100007ebu),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x13e0002au),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  0x101f002au),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  0x1000f82au),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, 0x100007eau)
));

INSTANTIATE_TEST_CASE_P(VMX, PPCRecordFormSanityTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::InstOpCode::Mnemonic, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vand,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vandc,    TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::InstOpCode::vcmpequb_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::InstOpCode::vcmpequh_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::InstOpCode::vcmpequw_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::InstOpCode::vcmpgtsb_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::InstOpCode::vcmpgtsh_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::InstOpCode::vcmpgtsw_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::InstOpCode::vcmpgtub_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::InstOpCode::vcmpgtuh_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::InstOpCode::vcmpgtuw_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminub,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vnor,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vor,      TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vperm,    TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsel,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsl,      TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslb,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsldoi,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslh,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslo,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslw,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltb,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsplth,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltish, TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltw,   TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsr,      TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsro,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupkhsb,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupkhsh,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupklsb,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupklsh,  TR::InstOpCode::bad,        BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vxor,     TR::InstOpCode::bad,        BinaryInstruction())
)));

INSTANTIATE_TEST_CASE_P(VSXScalarFixed, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr31,  TR::RealRegister::vsr0,  0x7c1f0066u),
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr0,   TR::RealRegister::vsr63, 0x7fe00067u),
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr0,   TR::RealRegister::vsr31, 0x7fe00066u),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr31,  TR::RealRegister::vsr0,  0x7c1f00e6u),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr0,   TR::RealRegister::vsr63, 0x7fe000e7u),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr0,   TR::RealRegister::vsr31, 0x7fe000e6u),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr63, TR::RealRegister::gr0,   0x7fe00167u),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr31, TR::RealRegister::gr0,   0x7fe00166u),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr0,  TR::RealRegister::gr31,  0x7c1f0166u),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr63, TR::RealRegister::gr0,   0x7fe001a7u),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr31, TR::RealRegister::gr0,   0x7fe001a6u),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr0,  TR::RealRegister::gr31,  0x7c1f01a6u),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr63, TR::RealRegister::gr0,   0x7fe001e7u),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr31, TR::RealRegister::gr0,   0x7fe001e6u),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr0,  TR::RealRegister::gr31,  0x7c1f01e6u)
));

INSTANTIATE_TEST_CASE_P(VSXScalarFixed, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfvsrd,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfvsrwz, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrd,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrwz, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrwa, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXScalarConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e00425u),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e00424u),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fc26u),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fc24u),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e0042du),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e0042cu),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fc2eu),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fc2cu),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e00525u),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e00524u),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fd26u),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fd24u),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e0052du),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e0052cu),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fd2eu),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fd2cu)
));

INSTANTIATE_TEST_CASE_P(VSXScalarConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e007e1u),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e007e0u),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ffe2u),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ffe0u),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e006e1u),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e006e0u),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fee2u),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fee0u)
));

INSTANTIATE_TEST_CASE_P(VSXVectorConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr1,  TR::RealRegister::vsr2,  0u, 0xf0201290u),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  2u, 0xf3e20291u),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 3u, 0xf003fa92u),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr33, TR::RealRegister::vsr34, 1u, 0xf0211293u)
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00411u),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00410u),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0414u),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0410u),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fc12u),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fc10u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00451u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00450u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0454u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0450u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fc52u),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fc50u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e005d1u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e005d0u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f05d4u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f05d0u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fdd2u),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fdd0u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00591u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00590u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0594u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0590u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fd92u),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fd90u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00511u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00510u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0514u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0510u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fd12u),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fd10u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00491u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00490u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0494u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0490u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fc92u),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fc90u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00551u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00550u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0554u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0550u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fd52u),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fd50u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00091u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00090u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0094u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0090u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000f892u),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000f890u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00191u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00190u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0194u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0190u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000f992u),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000f990u)
)));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x0u, 0xf3e00051u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x0u, 0xf3e00050u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0x0u, 0xf01f0054u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0x0u, 0xf01f0050u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0x0u, 0xf000f852u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0x0u, 0xf000f850u),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x3u, 0xf0000350u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     0, 0xf3e00011u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     0, 0xf3e00010u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,     0, 0xf01f0014u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,     0, 0xf01f0010u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63,    0, 0xf000f812u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31,    0, 0xf000f810u),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     3, 0xf0000310u)
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00030u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00031u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf01f0030u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf01f0034u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf000f830u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf000f832u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf00007f0u),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf00007f8u)
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxland,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlandc,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxleqv,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlnand,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlnor,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlor,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlorc,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxmrghw,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxmrglw,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxsel,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxspltw,  TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e007e5u),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e007e4u),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ffe6u),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ffe4u),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e006e5u),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e006e4u),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fee6u),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fee4u),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf3e0032du),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf3e0032cu),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb2eu),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb2cu)
));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00301u),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00300u),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0304u),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0300u),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb02u),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb00u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00201u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00200u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0204u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0200u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa02u),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa00u),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00319u),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00318u),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f031cu),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0318u),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb1au),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb18u),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00219u),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00218u),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f021cu),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0218u),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa1au),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa18u),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00399u),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00398u),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f039cu),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0398u),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb9au),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb98u),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00299u),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00298u),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f029cu),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0298u),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa9au),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa98u),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00359u),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00358u),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f035cu),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0358u),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb5au),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb58u),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00259u),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00258u),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f025cu),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0258u),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa5au),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa58u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e003c1u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e003c0u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f03c4u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f03c0u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fbc2u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fbc0u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e002c1u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e002c0u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f02c4u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f02c0u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fac2u),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fac0u),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00309u),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00308u),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f030cu),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0308u),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb0au),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb08u),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00209u),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00208u),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f020cu),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0208u),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa0au),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa08u),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00349u),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00348u),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f034cu),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0348u),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb4au),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb48u),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00249u),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00248u),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f024cu),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0248u),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa4au),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa48u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00701u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00700u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0704u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0700u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ff02u),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ff00u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00601u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00600u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0604u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0600u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fe02u),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fe00u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00741u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00740u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0744u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0740u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ff42u),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ff40u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00641u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00640u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0644u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0640u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fe42u),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fe40u),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00389u),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00388u),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f038cu),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0388u),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb8au),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb88u),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00289u),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00288u),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f028cu),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0288u),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa8au),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa88u),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e003c9u),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e003c8u),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f03ccu),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f03c8u),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fbcau),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fbc8u),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e002c9u),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e002c8u),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f02ccu),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f02c8u),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000facau),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fac8u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00381u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00380u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0384u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0380u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fb82u),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fb80u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00281u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00280u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0284u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0280u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa82u),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa80u),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00709u),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00708u),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f070cu),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0708u),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ff0au),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ff08u),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00609u),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00608u),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f060cu),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0608u),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fe0au),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fe08u),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00749u),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00748u),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f074cu),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0748u),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ff4au),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ff48u),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00649u),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00648u),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f064cu),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0648u),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fe4au),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fe48u),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00789u),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00788u),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f078cu),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0788u),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ff8au),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ff88u),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00689u),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00688u),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f068cu),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0688u),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fe8au),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fe88u),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e007c9u),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e007c8u),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f07ccu),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f07c8u),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000ffcau),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000ffc8u),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e006c9u),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e006c8u),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f06ccu),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f06c8u),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fecau),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fec8u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00241u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00240u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0244u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0240u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa42u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa40u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00241u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0xf3e00240u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0xf01f0244u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0xf01f0240u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0xf000fa42u),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0xf000fa40u)
)));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::InstOpCode::xvcmpeqdp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::InstOpCode::xvcmpeqsp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::InstOpCode::xvcmpgedp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::InstOpCode::xvcmpgesp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::InstOpCode::xvcmpgtdp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::InstOpCode::xvcmpgtsp_r, 0x00000400u),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnegdp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnegsp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsqrtdp,   TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsubdp,    TR::InstOpCode::bad,         BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::InstOpCode::bad,         BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fabs,   TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00210u),
    std::make_tuple(TR::InstOpCode::fabs,   TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fa10u),
    std::make_tuple(TR::InstOpCode::fmr,    TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00090u),
    std::make_tuple(TR::InstOpCode::fmr,    TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f890u),
    std::make_tuple(TR::InstOpCode::fnabs,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00110u),
    std::make_tuple(TR::InstOpCode::fnabs,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f910u),
    std::make_tuple(TR::InstOpCode::fneg,   TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00050u),
    std::make_tuple(TR::InstOpCode::fneg,   TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f850u),
    std::make_tuple(TR::InstOpCode::fsqrt,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0002cu),
    std::make_tuple(TR::InstOpCode::fsqrt,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f82cu),
    std::make_tuple(TR::InstOpCode::fsqrts, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe0002cu),
    std::make_tuple(TR::InstOpCode::fsqrts, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f82cu)
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe0002au),
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f002au),
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f82au),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe0002au),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec1f002au),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f82au),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe00024u),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f0024u),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f824u),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe00024u),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec1f0024u),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f824u),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe00032u),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f0032u),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc0007f2u),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe00032u),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec1f0032u),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec0007f2u),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe00028u),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc1f0028u),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f828u),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe00028u),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec1f0028u),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f828u)
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe0003au),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xfc1f003au),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc0007fau),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f83au),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe0003au),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xec1f003au),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec0007fau),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f83au),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe00038u),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xfc1f0038u),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc0007f8u),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f838u),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe00038u),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xec1f0038u),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec0007f8u),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f838u),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe0003eu),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xfc1f003eu),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc0007feu),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f83eu),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe0003eu),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xec1f003eu),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec0007feu),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f83eu),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe0003cu),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xfc1f003cu),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc0007fcu),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f83cu),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xefe0003cu),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xec1f003cu),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xec0007fcu),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00f83cu),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xffe0002eu),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0xfc1f002eu),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xfc0007eeu),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f82eu)
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fabs,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fadd,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fadds,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fdiv,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fdivs,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmr,     TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmul,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmuls,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnabs,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fneg,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsel,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsqrt,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsqrts,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsub,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsubs,   TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fcfid,   TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0069cu),
    std::make_tuple(TR::InstOpCode::fcfid,   TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fe9cu),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe0069cu),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fe9cu),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0079cu),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00ff9cu),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe0079cu),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00ff9cu),
    std::make_tuple(TR::InstOpCode::fctid,   TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0065cu),
    std::make_tuple(TR::InstOpCode::fctid,   TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fe5cu),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0065eu),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fe5eu),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0001cu),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f81cu),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe0001eu),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f81eu)
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fcfid,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctid,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatRound, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::frim, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe003d0u),
    std::make_tuple(TR::InstOpCode::frim, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fbd0u),
    std::make_tuple(TR::InstOpCode::frin, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00310u),
    std::make_tuple(TR::InstOpCode::frin, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fb10u),
    std::make_tuple(TR::InstOpCode::frip, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00390u),
    std::make_tuple(TR::InstOpCode::frip, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00fb90u),
    std::make_tuple(TR::InstOpCode::frsp, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xffe00018u),
    std::make_tuple(TR::InstOpCode::frsp, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xfc00f818u)
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatRound, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::frim, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frin, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frip, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frsp, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x3fu, 0xec1ffd84u),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x3fu, 0xef80fd84u),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x00u, 0xec1f0184u),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x00u, 0xef800184u),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x3fu, 0xec1ffdc4u),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x3fu, 0xef80fdc4u),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x00u, 0xec1f01c4u),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x00u, 0xef8001c4u)
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcffix,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe00644u),
    std::make_tuple(TR::InstOpCode::dcffix,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fe44u),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::RealRegister::fp30, TR::RealRegister::fp0,  0xffc00644u),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::RealRegister::fp0,  TR::RealRegister::fp30, 0xfc00f644u),
    std::make_tuple(TR::InstOpCode::dctfix,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe00244u),
    std::make_tuple(TR::InstOpCode::dctfix,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fa44u),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe00284u),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fa84u),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe00684u),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fe84u),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::RealRegister::fp30, TR::RealRegister::fp0,  0xffc00604u),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::RealRegister::fp0,  TR::RealRegister::fp30, 0xfc00f604u),
    std::make_tuple(TR::InstOpCode::dxex,    TR::RealRegister::fp31, TR::RealRegister::fp0,  0xefe002c4u),
    std::make_tuple(TR::InstOpCode::dxex,    TR::RealRegister::fp0,  TR::RealRegister::fp31, 0xec00fac4u)
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x0u, 0xefe00006u),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0x0u, 0xec1f0006u),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0x0u, 0xec00f806u),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x3u, 0xec000606u),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x2u, 0xec000406u),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x1u, 0xec000206u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x0u, 0xefe00046u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0x0u, 0xec1f0046u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0x0u, 0xec00f846u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x3u, 0xec000646u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x2u, 0xec000446u),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x1u, 0xec000246u)
));

INSTANTIATE_TEST_CASE_P(DFP, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcffix,  TR::InstOpCode::dcffix_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::InstOpCode::dcffixq_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::InstOpCode::ddedpd_r,  0x00000001u),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::InstOpCode::denbcdu_r, 0x00000001u),
    std::make_tuple(TR::InstOpCode::dqua,    TR::InstOpCode::dqua_r,    0x00000001u),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::InstOpCode::drdpq_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::drrnd,   TR::InstOpCode::drrnd_r,   0x00000001u),
    std::make_tuple(TR::InstOpCode::dtstdc,  TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dtstdg,  TR::InstOpCode::bad,       BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dxex,    TR::InstOpCode::dxex_r,    0x00000001u)
));

INSTANTIATE_TEST_CASE_P(TM, PPCDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tabort_r,   0x7c00071du),
    std::make_tuple(TR::InstOpCode::tbegin_r,   0x7c00051du),
    std::make_tuple(TR::InstOpCode::tbeginro_r, 0x7c20051du),
    std::make_tuple(TR::InstOpCode::tend_r,     0x7c00055du)
));

INSTANTIATE_TEST_CASE_P(TM, PPCSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr31, 0x00000000u, 0x7c9f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7c807eddu),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7c9f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7c80feddu),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr31, 0x00000000u, 0x7d9f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7d807eddu),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7d9f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7d80feddu),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr31, 0x00000000u, 0x7d1f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7d007eddu),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7d1f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7d00feddu),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr31, 0x00000000u, 0x7e9f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7e807eddu),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7e9f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7e80feddu),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr31, 0x00000000u, 0x7cbf06ddu),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr0,  0x0000000fu, 0x7ca07eddu),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7cbf86ddu),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr0,  0xffffffffu, 0x7ca0feddu),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr31, 0x00000000u, 0x7c3f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr0,  0x0000000fu, 0x7c207eddu),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7c3f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr0,  0xffffffffu, 0x7c20feddu),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr31, 0x00000000u, 0x7cdf06ddu),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr0,  0x0000000fu, 0x7cc07eddu),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7cdf86ddu),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr0,  0xffffffffu, 0x7cc0feddu),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr31, 0x00000000u, 0x7c5f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr0,  0x0000000fu, 0x7c407eddu),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7c5f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr0,  0xffffffffu, 0x7c40feddu),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr31, 0x00000000u, 0x7e1f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7e007eddu),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7e1f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7e00feddu),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr31, 0x00000000u, 0x7f1f06ddu),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr0,  0x0000000fu, 0x7f007eddu),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7f1f86ddu),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr0,  0xffffffffu, 0x7f00feddu),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr31, 0x00000000u, 0x7c9f069du),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7c807e9du),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7c9f869du),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7c80fe9du),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr31, 0x00000000u, 0x7d9f069du),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7d807e9du),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7d9f869du),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7d80fe9du),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr31, 0x00000000u, 0x7d1f069du),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7d007e9du),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7d1f869du),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7d00fe9du),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr31, 0x00000000u, 0x7e9f069du),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7e807e9du),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7e9f869du),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7e80fe9du),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr31, 0x00000000u, 0x7cbf069du),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr0,  0x0000000fu, 0x7ca07e9du),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7cbf869du),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr0,  0xffffffffu, 0x7ca0fe9du),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr31, 0x00000000u, 0x7c3f069du),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr0,  0x0000000fu, 0x7c207e9du),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7c3f869du),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr0,  0xffffffffu, 0x7c20fe9du),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr31, 0x00000000u, 0x7cdf069du),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr0,  0x0000000fu, 0x7cc07e9du),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7cdf869du),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr0,  0xffffffffu, 0x7cc0fe9du),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr31, 0x00000000u, 0x7c5f069du),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr0,  0x0000000fu, 0x7c407e9du),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7c5f869du),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr0,  0xffffffffu, 0x7c40fe9du),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr31, 0x00000000u, 0x7e1f069du),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr0,  0x0000000fu, 0x7e007e9du),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr31, 0xfffffff0u, 0x7e1f869du),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr0,  0xffffffffu, 0x7e00fe9du),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr31, 0x00000000u, 0x7f1f069du),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr0,  0x0000000fu, 0x7f007e9du),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr31, 0xfffffff0u, 0x7f1f869du),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr0,  0xffffffffu, 0x7f00fe9du)
)));

INSTANTIATE_TEST_CASE_P(TM, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabort_r,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdeqi_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdgei_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdgti_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlei_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlgei_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlgti_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdllei_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdllti_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlti_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdneqi_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortweqi_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwgei_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwgti_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlei_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlgei_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlgti_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwllei_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwllti_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlti_r,  BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwneqi_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tbegin_r,      BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tbeginro_r,    BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tend_r,        BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Prefetch, PPCMemEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00022cu),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fa2cu),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f022cu),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), 0x7c0f7a2cu),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0001ecu),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f9ecu),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f01ecu),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), 0x7c0f79ecu),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7e0001ecu),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7e00f9ecu),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7e1f01ecu),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), 0x7e0f79ecu)
));

INSTANTIATE_TEST_CASE_P(Prefetch, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcbt,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dcbtst,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dcbtstt, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadDisp, PPCTrg1MemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0x88000000u),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0x8be00000u),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0x881f0000u),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0x88007fffu),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0x88008000u),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0x8800ffffu),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0x8c010000u),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0x8fe10000u),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0x8c1f0000u),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0x8c017fffu),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0x8c018000u),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0x8c01ffffu),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xe8000000u),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xebe00000u),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xe81f0000u),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7ffc), 0xe8007ffcu),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xe8008000u),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0004), 0xe800fffcu),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0xe8010001u),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0xebe10001u),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xe81f0001u),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x7ffc), 0xe8017ffdu),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0xe8018001u),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x0004), 0xe801fffdu),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xc8000000u),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xcbe00000u),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xc81f0000u),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0xc8007fffu),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xc8008000u),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0xc800ffffu),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0xcc010000u),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0xcfe10000u),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xcc1f0000u),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0xcc017fffu),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0xcc018000u),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0xcc01ffffu),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xc0000000u),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xc3e00000u),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xc01f0000u),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0xc0007fffu),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xc0008000u),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0xc000ffffu),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0xc4010000u),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0xc7e10000u),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xc41f0000u),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0xc4017fffu),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0xc4018000u),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0xc401ffffu),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xa8000000u),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xabe00000u),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xa81f0000u),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0xa8007fffu),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xa8008000u),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0xa800ffffu),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0xac010000u),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0xafe10000u),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xac1f0000u),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0xac017fffu),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0xac018000u),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0xac01ffffu),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xa0000000u),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xa3e00000u),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xa01f0000u),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0xa0007fffu),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xa0008000u),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0xa000ffffu),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0xa4010000u),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0xa7e10000u),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xa41f0000u),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0xa4017fffu),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0xa4018000u),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0xa401ffffu),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xb8200000u),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xbbe00000u),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr30, 0x0000), 0xbbfe0000u),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr29, 0x0000), 0xbbfd0000u),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0xb8207fffu),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xb8208000u),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0xb820ffffu),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0xe8000002u),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0xebe00002u),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0xe81f0002u),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7ffc), 0xe8007ffeu),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0xe8008002u),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0004), 0xe800fffeu),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), 0x80000000u),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr0,  0x0000), 0x83e00000u),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0x801f0000u),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0,  0x7fff), 0x80007fffu),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), 0x80008000u),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr0, -0x0001), 0x8000ffffu),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x0000), 0x84010000u),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr31, MemoryReference(TR::RealRegister::gr1,  0x0000), 0x87e10000u),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), 0x841f0000u),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1,  0x7fff), 0x84017fffu),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x8000), 0x84018000u),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,  MemoryReference(TR::RealRegister::gr1, -0x0001), 0x8401ffffu)
)));

INSTANTIATE_TEST_CASE_P(LoadDisp, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lbz,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lbzu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ld,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldu,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfd,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfs,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lha,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhau, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhz,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lmw,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwa,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwz,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzu, TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadIndex, PPCTrg1MemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c0100eeu),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe100eeu),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f00eeu),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01f8eeu),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0000aeu),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe000aeu),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f00aeu),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f8aeu),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0000a8u),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe000a8u),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f00a8u),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f8a8u),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000428u),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00428u),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0428u),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fc28u),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c01006au),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe1006au),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f006au),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01f86au),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00002au),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0002au),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f002au),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f82au),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c0104eeu),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe104eeu),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f04eeu),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01fceeu),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0004aeu),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe004aeu),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f04aeu),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fcaeu),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0006aeu),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe006aeu),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f06aeu),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00feaeu),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0006eeu),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe006eeu),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f06eeu),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00feeeu),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c01046eu),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe1046eu),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f046eu),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01fc6eu),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00042eu),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0042eu),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f042eu),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fc2eu),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c0102eeu),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe102eeu),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f02eeu),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01faeeu),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0002aeu),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe002aeu),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f02aeu),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00faaeu),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00062cu),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0062cu),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f062cu),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fe2cu),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c01026eu),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe1026eu),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f026eu),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01fa6eu),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00022eu),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0022eu),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f022eu),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fa2eu),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0000ceu),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe000ceu),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f00ceu),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f8ceu),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00000eu),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0000eu),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f000eu),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f80eu),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00004eu),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0004eu),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f004eu),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f84eu),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00008eu),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0008eu),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f008eu),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f88eu),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000028u),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00028u),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0028u),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f828u),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c0102eau),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe102eau),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f02eau),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01faeau),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c0002aau),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe002aau),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f02aau),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00faaau),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00042cu),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0042cu),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f042cu),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fc2cu),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7c01006eu),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), 0x7fe1006eu),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f006eu),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), 0x7c01f86eu),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c00002eu),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe0002eu),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f002eu),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f82eu),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000498u),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00498u),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00499u),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0498u),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fc98u),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000098u),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00098u),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00099u),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0098u),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f898u),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000018u),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00018u),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00019u),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0018u),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00f818u),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000418u),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00418u),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00419u),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0418u),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fc18u),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000698u),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00698u),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00699u),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0698u),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fe98u),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000298u),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00298u),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00299u),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0298u),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fa98u),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7c000618u),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00618u),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), 0x7fe00619u),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), 0x7c1f0618u),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), 0x7c00fe18u)
)));

INSTANTIATE_TEST_CASE_P(LoadIndex, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lbzux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldux,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldx,     TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhax,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvx,     TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwax,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StoreDisp, PPCMemSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,  0x98000000u),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0x981f0000u),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31, 0x9be00000u),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,  0x98007fffu),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,  0x98008000u),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,  0x9800ffffu),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,  0x9c010000u),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0x9c1f0000u),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31, 0x9fe10000u),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,  0x9c017fffu),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,  0x9c018000u),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,  0x9c01ffffu),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,  0xf8000000u),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0xf81f0000u),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31, 0xfbe00000u),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x7ffc), TR::RealRegister::gr0,  0xf8007ffcu),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,  0xf8008000u),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,  -0x0004), TR::RealRegister::gr0,  0xf800fffcu),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,  0xf8010001u),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0xf81f0001u),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31, 0xfbe10001u),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x7ffc), TR::RealRegister::gr0,  0xf8017ffdu),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,  0xf8018001u),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,  -0x0004), TR::RealRegister::gr0,  0xf801fffdu),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp0,  0xd8000000u),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,  0xd81f0000u),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp31, 0xdbe00000u),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::fp0,  0xd8007fffu),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::fp0,  0xd8008000u),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::fp0,  0xd800ffffu),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp0,  0xdc010000u),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,  0xdc1f0000u),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp31, 0xdfe10000u),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::fp0,  0xdc017fffu),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::fp0,  0xdc018000u),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::fp0,  0xdc01ffffu),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp0,  0xd0000000u),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,  0xd01f0000u),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp31, 0xd3e00000u),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::fp0,  0xd0007fffu),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::fp0,  0xd0008000u),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::fp0,  0xd000ffffu),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp0,  0xd4010000u),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,  0xd41f0000u),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp31, 0xd7e10000u),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::fp0,  0xd4017fffu),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::fp0,  0xd4018000u),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::fp0,  0xd401ffffu),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,  0xb0000000u),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0xb01f0000u),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31, 0xb3e00000u),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,  0xb0007fffu),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,  0xb0008000u),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,  0xb000ffffu),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,  0xb4010000u),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0xb41f0000u),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31, 0xb7e10000u),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,  0xb4017fffu),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,  0xb4018000u),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,  0xb401ffffu),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,  0xbc000000u),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0xbc1f0000u),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31, 0xbfe00000u),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,  0xbc007fffu),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,  0xbc008000u),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,  0xbc00ffffu),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,  0x90000000u),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0x901f0000u),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31, 0x93e00000u),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,  0x90007fffu),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,  0x90008000u),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,  0x9000ffffu),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,  0x94010000u),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,  0x941f0000u),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31, 0x97e10000u),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,  0x94017fffu),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,  0x94018000u),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,  0x9401ffffu)
)));

INSTANTIATE_TEST_CASE_P(StoreDisp, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stb,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stbu,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::std,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdu,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfd,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfs,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsu, TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sth,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthu,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stmw,  TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stw,   TR::InstOpCode::bad, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwu,  TR::InstOpCode::bad, BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StoreIndex, PPCMemSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c0101eeu),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f01eeu),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c01f9eeu),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe101eeu),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c0001aeu),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f01aeu),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00f9aeu),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe001aeu),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c0001adu),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f01adu),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00f9adu),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe001adu),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c01016au),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f016au),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c01f96au),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe1016au),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00012au),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f012au),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00f92au),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0012au),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c0105eeu),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c1f05eeu),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::fp0,   0x7c01fdeeu),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  0x7fe105eeu),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c0005aeu),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c1f05aeu),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   0x7c00fdaeu),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  0x7fe005aeu),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c0007aeu),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c1f07aeu),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   0x7c00ffaeu),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  0x7fe007aeu),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c01056eu),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c1f056eu),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::fp0,   0x7c01fd6eu),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  0x7fe1056eu),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c00052eu),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   0x7c1f052eu),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   0x7c00fd2eu),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  0x7fe0052eu),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00072cu),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f072cu),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00ff2cu),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0072cu),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c01036eu),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f036eu),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c01fb6eu),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe1036eu),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00032eu),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f032eu),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00fb2eu),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0032eu),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c0001ceu),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c1f01ceu),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   0x7c00f9ceu),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  0x7fe001ceu),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c00010eu),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c1f010eu),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   0x7c00f90eu),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  0x7fe0010eu),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c00014eu),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c1f014eu),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   0x7c00f94eu),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  0x7fe0014eu),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c00018eu),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   0x7c1f018eu),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   0x7c00f98eu),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  0x7fe0018eu),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00052cu),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f052cu),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00fd2cu),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0052cu),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00012du),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f012du),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00f92du),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0012du),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c01016eu),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f016eu),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c01f96eu),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe1016eu),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c00012eu),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   0x7c1f012eu),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   0x7c00f92eu),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  0x7fe0012eu),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c000598u),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c1f0598u),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  0x7c00fd98u),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, 0x7fe00598u),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, 0x7fe00599u),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c000118u),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c1f0118u),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  0x7c00f918u),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, 0x7fe00118u),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, 0x7fe00119u),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c000518u),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c1f0518u),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  0x7c00fd18u),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, 0x7fe00518u),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, 0x7fe00519u),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c000798u),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c1f0798u),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  0x7c00ff98u),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, 0x7fe00798u),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, 0x7fe00799u),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c000718u),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  0x7c1f0718u),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  0x7c00ff18u),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, 0x7fe00718u),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, 0x7fe00719u)
)));

INSTANTIATE_TEST_CASE_P(StoreIndex, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stbux,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stbx,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,     TR::InstOpCode::stdcx_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdux,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdx,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdux,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdx,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfiwx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsux,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsx,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthbrx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthux,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthx,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvx,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvebx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvehx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvewx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwbrx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,     TR::InstOpCode::stwcx_r, BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwux,   TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwx,    TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsdx,  TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsiwx, TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsspx, TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvd2x, TR::InstOpCode::bad,     BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvw4x, TR::InstOpCode::bad,     BinaryInstruction())
));
