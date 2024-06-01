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

#include "../CodeGenTest.hpp"
#include "Util.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/PPCInstruction.hpp"
#include "il/LabelSymbol.hpp"

#include <stdexcept>

#define PPC_INSTRUCTION_ALIGNMENT 64

class PPCBinaryEncoderTest : public TRTest::BinaryEncoderTest<PPC_INSTRUCTION_ALIGNMENT> {

public:

    TRTest::BinaryInstruction getEncodedInstruction(size_t size, uint32_t off) {
        return TRTest::BinaryInstruction(&getAlignedBuf()[off], size);
    }

    TRTest::BinaryInstruction getEncodedInstruction(size_t size) {
        return getEncodedInstruction(size, 0);
    }

    TRTest::BinaryInstruction fixEndianness(TRTest::BinaryInstruction bi) {
        // As the PPC tests are encoded as big endian strings, we need to convert them to little endian
        // if the target cpu is running little endian
        if (_comp.target().cpu.isLittleEndian()) {
            for (int i = 0; i < bi._size / sizeof(uint32_t); i++) {
                std::swap(bi._buf[i * sizeof(uint32_t)], bi._buf[i * sizeof(uint32_t) + 3]);
                std::swap(bi._buf[i * sizeof(uint32_t) + 1], bi._buf[i * sizeof(uint32_t) + 2]);
            }
        }

        return bi;
    }

    TRTest::BinaryInstruction findDifferingBits(TR::InstOpCode op1, TR::InstOpCode op2) {
        return getBlankEncoding(op1) ^ getBlankEncoding(op2);
    }

    TRTest::BinaryInstruction getBlankEncoding(TR::InstOpCode opCode) {
        opCode.copyBinaryToBuffer(&getAlignedBuf()[0]);
        return getEncodedInstruction(opCode.getBinaryLength());
    }
};

class PPCRecordFormSanityTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::InstOpCode::Mnemonic, TRTest::BinaryInstruction>> {};

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
        fixEndianness(std::get<2>(GetParam())),
        findDifferingBits(std::get<0>(GetParam()), std::get<1>(GetParam()))
    );
}

class PPCDirectEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TRTest::BinaryInstruction, bool>> {};

TEST_P(PPCDirectEncodingTest, encode) {
    auto instr = generateInstruction(cg(), std::get<0>(GetParam()), fakeNode);

    ASSERT_EQ(fixEndianness(std::get<1>(GetParam())), encodeInstruction(instr));
}

TEST_P(PPCDirectEncodingTest, encodePrefixCrossingBoundary) {
    auto instr = generateInstruction(cg(), std::get<0>(GetParam()), fakeNode);
    auto encoding = fixEndianness(std::get<1>(GetParam()));

    ASSERT_EQ(
        std::get<2>(GetParam())
        ? encoding.prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode)
        : encoding,
        encodeInstruction(instr, 60)
    );
}

class PPCLabelEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, size_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCLabelEncodingTest, encode) {
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(getAlignedBuf() + std::get<1>(GetParam()));

    auto instr = generateLabelInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label
    );

    ASSERT_EQ(
        fixEndianness(std::get<2>(GetParam())),
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

    auto size = encodeInstruction(instr)._size;
    label->setCodeLocation(getAlignedBuf() + std::get<1>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(fixEndianness(std::get<2>(GetParam())), getEncodedInstruction(size));
}

TEST_P(PPCLabelEncodingTest, labelLocation) {
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(getAlignedBuf() + std::get<1>(GetParam()));

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
        ASSERT_EQ(getAlignedBuf() + std::get<1>(GetParam()), label->getCodeLocation());
}

class PPCConditionalBranchEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, ssize_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCConditionalBranchEncodingTest, encode) {
    auto label = std::get<2>(GetParam()) != -1 ? generateLabelSymbol(cg()) : NULL;
    if (label)
        label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));

    auto instr = generateConditionalBranchInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        label,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    ASSERT_EQ(fixEndianness(std::get<3>(GetParam())), encodeInstruction(instr));
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

    auto size = encodeInstruction(instr)._size;
    label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(fixEndianness(std::get<3>(GetParam())), getEncodedInstruction(size));
}

class PPCImmEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, uint32_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCImmEncodingTest, encode) {
    auto instr = generateImmInstruction(cg(), std::get<0>(GetParam()), fakeNode, std::get<1>(GetParam()));

    ASSERT_EQ(fixEndianness(std::get<2>(GetParam())), encodeInstruction(instr));
}

class PPCImm2EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, uint32_t, uint32_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCImm2EncodingTest, encode) {
    auto instr = generateImm2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(fixEndianness(std::get<3>(GetParam())), encodeInstruction(instr));
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

    ASSERT_EQ(fixEndianness(std::get<3>(GetParam())) ^ differingBits, encodeInstruction(instr));
}

class PPCTrg1EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TRTest::BinaryInstruction>> {};

TEST_P(PPCTrg1EncodingTest, encode) {
    auto instr = generateTrg1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    ASSERT_EQ(
        fixEndianness(std::get<2>(GetParam())),
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
        fixEndianness(std::get<2>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCSrc1EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCSrc1EncodingTest, encode) {
    auto instr = generateSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
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
        fixEndianness(std::get<3>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCSrc2EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>{};

TEST_P(PPCSrc2EncodingTest, encode) {
    auto instr = generateSrc2Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
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
        fixEndianness(std::get<3>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCSrc3EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>{};

TEST_P(PPCSrc3EncodingTest, encode) {
    auto instr = generateSrc3Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(std::get<3>(GetParam()))
    );

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        encodeInstruction(instr)
    );
}

class PPCTrg1ImmEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, TRTest::BinaryInstruction>>{};

TEST_P(PPCTrg1ImmEncodingTest, encode) {
    auto instr = generateTrg1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam())
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
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
        fixEndianness(std::get<3>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>{};

TEST_P(PPCTrg1Src1EncodingTest, encode) {
    auto instr = generateTrg1Src1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
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
        fixEndianness(std::get<3>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1ImmEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, TRTest::BinaryInstruction>>{};

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
        fixEndianness(std::get<4>(GetParam())),
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
        fixEndianness(std::get<4>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src1Imm2EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, uint64_t, TRTest::BinaryInstruction>>{};

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
        fixEndianness(std::get<5>(GetParam())),
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
        fixEndianness(std::get<5>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src2EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>{};

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
        fixEndianness(std::get<4>(GetParam())),
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
        fixEndianness(std::get<4>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src2ImmEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint64_t, TRTest::BinaryInstruction>>{};

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
        fixEndianness(std::get<5>(GetParam())),
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
        fixEndianness(std::get<5>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCTrg1Src3EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>{};

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
        fixEndianness(std::get<5>(GetParam())),
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
        fixEndianness(std::get<5>(GetParam())) ^ differingBits,
        encodeInstruction(instr)
    );
}

class PPCMemEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TRTest::BinaryInstruction>>{};

TEST_P(PPCMemEncodingTest, encode) {
    auto instr = generateMemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()).reify(cg())
    );

    ASSERT_EQ(
        fixEndianness(std::get<2>(GetParam())),
        encodeInstruction(instr)
    );
}

class PPCTrg1MemEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, TRTest::BinaryInstruction, bool>> {};

TEST_P(PPCTrg1MemEncodingTest, encode) {
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam()).reify(cg())
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
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
        fixEndianness(std::get<3>(GetParam())),
        encodeInstruction(instr)
    );
}

TEST_P(PPCTrg1MemEncodingTest, encodePrefixCrossingBoundary) {
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        std::get<2>(GetParam()).reify(cg())
    );

    auto encoding = fixEndianness(std::get<3>(GetParam()));

    ASSERT_EQ(
        std::get<4>(GetParam())
        ? encoding.prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode)
        : encoding,
        encodeInstruction(instr, 60)
    );
}

class PPCTrg1MemPCRelativeEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, int64_t, int64_t, TRTest::BinaryInstruction>> {};

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encode) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0)
    );

    label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        encodeInstruction(instr, 0)
    );
}

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encodePrefixCrossingBoundary) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0)
    );

    label->setCodeLocation(getAlignedBuf() + 64 + std::get<2>(GetParam()));

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        encodeInstruction(instr, 60)
    );
}

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encodeWithRelocation) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0)
    );

    auto size = encodeInstruction(instr)._size;
    label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        getEncodedInstruction(size, 0)
    );
}

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encodePrefixCrossingBoundaryWithRelocation) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateTrg1MemInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0)
    );

    auto size = encodeInstruction(instr, 60)._size;
    label->setCodeLocation(getAlignedBuf() + 64 + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        getEncodedInstruction(size, 60)
    );
}

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encodeFlat) {
    int64_t displacement = std::get<2>(GetParam()) + std::get<3>(GetParam());

    // PPCTrg1ImmInstruction currently only supports 32-bit immediates
    if (displacement < -0x80000000 || displacement > 0x7fffffff)
        return;

    auto instr = generateTrg1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        displacement
    );

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        encodeInstruction(instr, 0)
    );
}

TEST_P(PPCTrg1MemPCRelativeEncodingTest, encodeFlatPrefixCrossingBoundary) {
    int64_t displacement = std::get<2>(GetParam()) + std::get<3>(GetParam());

    // PPCTrg1ImmInstruction currently only supports 32-bit immediates
    if (displacement < -0x80000000 || displacement > 0x7fffffff)
        return;

    auto instr = generateTrg1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        displacement
    );

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        encodeInstruction(instr, 60)
    );
}

class PPCMemSrc1EncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, TRTest::BinaryInstruction, bool>> {};

TEST_P(PPCMemSrc1EncodingTest, encode) {
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()).reify(cg()),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
        encodeInstruction(instr)
    );
}

TEST_P(PPCMemSrc1EncodingTest, encodeFlat) {
    const MemoryReference& mr = std::get<1>(GetParam());

    if (!mr.isIndexForm())
        return;

    auto instr = generateSrc3Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<2>(GetParam())),
        cg()->machine()->getRealRegister(mr.baseReg()),
        cg()->machine()->getRealRegister(mr.indexReg())
    );

    ASSERT_EQ(
        fixEndianness(std::get<3>(GetParam())),
        encodeInstruction(instr)
    );
}

TEST_P(PPCMemSrc1EncodingTest, encodePrefixCrossingBoundary) {
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        std::get<1>(GetParam()).reify(cg()),
        cg()->machine()->getRealRegister(std::get<2>(GetParam()))
    );

    auto encoding = fixEndianness(std::get<3>(GetParam()));

    ASSERT_EQ(
        std::get<4>(GetParam())
        ? encoding.prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode)
        : encoding,
        encodeInstruction(instr, 60)
    );
}

class PPCMemSrc1PCRelativeEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, int64_t, int64_t, TRTest::BinaryInstruction>>{};

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encode) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0),
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        encodeInstruction(instr, 0)
    );
}

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encodePrefixCrossingBoundary) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0),
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    label->setCodeLocation(getAlignedBuf() + 64 + std::get<2>(GetParam()));

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        encodeInstruction(instr, 60)
    );
}

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encodeWithRelocation) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0),
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    auto size = encodeInstruction(instr)._size;
    label->setCodeLocation(getAlignedBuf() + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        getEncodedInstruction(size, 0)
    );
}

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encodePrefixCrossingBoundaryWithRelocation) {
    auto label = generateLabelSymbol(cg());
    auto instr = generateMemSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        TR::MemoryReference::createWithLabel(cg(), label, std::get<3>(GetParam()), 0),
        cg()->machine()->getRealRegister(std::get<1>(GetParam()))
    );

    auto size = encodeInstruction(instr, 60)._size;
    label->setCodeLocation(getAlignedBuf() + 64 + std::get<2>(GetParam()));
    cg()->processRelocations();

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        getEncodedInstruction(size, 60)
    );
}

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encodeFlat) {
    int64_t displacement = std::get<2>(GetParam()) + std::get<3>(GetParam());

    // PPCSrc1Instruction currently only supports 32-bit immediates
    if (displacement < -0x80000000 || displacement > 0x7fffffff)
        return;

    auto instr = generateSrc1Instruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        displacement
    );

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())),
        encodeInstruction(instr, 0)
    );
}

TEST_P(PPCMemSrc1PCRelativeEncodingTest, encodeFlatPrefixCrossingBoundary) {
    int64_t displacement = std::get<2>(GetParam()) + std::get<3>(GetParam());

    // PPCSrc1Instruction currently only supports 32-bit immediates
    if (displacement < -0x80000000 || displacement > 0x7fffffff)
        return;

    auto instr = generateTrg1ImmInstruction(
        cg(),
        std::get<0>(GetParam()),
        fakeNode,
        cg()->machine()->getRealRegister(std::get<1>(GetParam())),
        displacement
    );

    ASSERT_EQ(
        fixEndianness(std::get<4>(GetParam())).prepend(TR::InstOpCode::metadata[TR::InstOpCode::nop].opcode),
        encodeInstruction(instr, 60)
    );
}

class PPCFenceEncodingTest : public PPCBinaryEncoderTest, public ::testing::WithParamInterface<uint32_t> {};

TEST_P(PPCFenceEncodingTest, testEntryRelative32Bit) {
    uint32_t relo = 0xdeadc0deu;

    auto node = TR::Node::createRelative32BitFenceNode(reinterpret_cast<void*>(&relo));
    auto instr = generateAdminInstruction(cg(), TR::InstOpCode::fence, fakeNode, node);

    ASSERT_EQ(TRTest::BinaryInstruction(), encodeInstruction(instr, GetParam()));
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
    std::make_tuple(TR::InstOpCode::assocreg, TRTest::BinaryInstruction(), false),
    std::make_tuple(TR::InstOpCode::bad,      TRTest::BinaryInstruction("00000000"), false),
    std::make_tuple(TR::InstOpCode::isync,    TRTest::BinaryInstruction("4c00012c"), false),
    std::make_tuple(TR::InstOpCode::lwsync,   TRTest::BinaryInstruction("7c2004ac"), false),
    std::make_tuple(TR::InstOpCode::sync,     TRTest::BinaryInstruction("7c0004ac"), false),
    std::make_tuple(TR::InstOpCode::nop,      TRTest::BinaryInstruction("60000000"), false),
    std::make_tuple(TR::InstOpCode::pnop,     TRTest::BinaryInstruction("0700000000000000"), true)
));

INSTANTIATE_TEST_CASE_P(Special, PPCLabelEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::label, -1, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Special, PPCImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dd, 0x00000000u, TRTest::BinaryInstruction("00000000")),
    std::make_tuple(TR::InstOpCode::dd, 0xffffffffu, TRTest::BinaryInstruction("ffffffff")),
    std::make_tuple(TR::InstOpCode::dd, 0x12345678u, TRTest::BinaryInstruction("12345678"))
));

INSTANTIATE_TEST_CASE_P(Special, PPCFenceEncodingTest, ::testing::Values(0, 4, 8));

INSTANTIATE_TEST_CASE_P(Special, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::assocreg, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dd,       TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::isync,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::label,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwsync,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sync,     TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::nop,      TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Branch, PPCDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::bctr,  TRTest::BinaryInstruction("4e800420"), false),
    std::make_tuple(TR::InstOpCode::bctrl, TRTest::BinaryInstruction("4e800421"), false),
    std::make_tuple(TR::InstOpCode::blr,   TRTest::BinaryInstruction("4e800020"), false),
    std::make_tuple(TR::InstOpCode::blrl,  TRTest::BinaryInstruction("4e800021"), false)
));

INSTANTIATE_TEST_CASE_P(Branch, PPCLabelEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::b,           0, TRTest::BinaryInstruction("48000000")),
    std::make_tuple(TR::InstOpCode::b,          -4, TRTest::BinaryInstruction("4bfffffc")),
    std::make_tuple(TR::InstOpCode::b,   0x1fffffc, TRTest::BinaryInstruction("49fffffc")),
    std::make_tuple(TR::InstOpCode::b,  -0x2000000, TRTest::BinaryInstruction("4a000000")),
    std::make_tuple(TR::InstOpCode::bl,          0, TRTest::BinaryInstruction("48000001")),
    std::make_tuple(TR::InstOpCode::bl,         -4, TRTest::BinaryInstruction("4bfffffd")),
    std::make_tuple(TR::InstOpCode::bl,  0x1fffffc, TRTest::BinaryInstruction("49fffffd")),
    std::make_tuple(TR::InstOpCode::bl, -0x2000000, TRTest::BinaryInstruction("4a000001"))
));

INSTANTIATE_TEST_CASE_P(Branch, PPCConditionalBranchEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, ssize_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41820000")),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4182fffc")),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419e7ffc")),
    std::make_tuple(TR::InstOpCode::beq,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419e8000")),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41820001")),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4182fffd")),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419e7ffd")),
    std::make_tuple(TR::InstOpCode::beql,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419e8001")),
    std::make_tuple(TR::InstOpCode::beqlr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4d820020")),
    std::make_tuple(TR::InstOpCode::beqlr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4d9e0020")),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40800000")),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4080fffc")),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409c7ffc")),
    std::make_tuple(TR::InstOpCode::bge,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409c8000")),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40800001")),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4080fffd")),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409c7ffd")),
    std::make_tuple(TR::InstOpCode::bgel,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409c8001")),
    std::make_tuple(TR::InstOpCode::bgelr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4c800020")),
    std::make_tuple(TR::InstOpCode::bgelr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4c9c0020")),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41810000")),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4181fffc")),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419d7ffc")),
    std::make_tuple(TR::InstOpCode::bgt,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419d8000")),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41810001")),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4181fffd")),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419d7ffd")),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419d8001")),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4d810020")),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4d9d0020")),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40810000")),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4081fffc")),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409d7ffc")),
    std::make_tuple(TR::InstOpCode::ble,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409d8000")),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40810001")),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4081fffd")),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409d7ffd")),
    std::make_tuple(TR::InstOpCode::blel,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409d8001")),
    std::make_tuple(TR::InstOpCode::blelr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4c810020")),
    std::make_tuple(TR::InstOpCode::blelr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4c9d0020")),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41800000")),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4180fffc")),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419c7ffc")),
    std::make_tuple(TR::InstOpCode::blt,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419c8000")),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41800001")),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4180fffd")),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419c7ffd")),
    std::make_tuple(TR::InstOpCode::bltl,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419c8001")),
    std::make_tuple(TR::InstOpCode::bltlr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4d800020")),
    std::make_tuple(TR::InstOpCode::bltlr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4d9c0020")),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40820000")),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4082fffc")),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409e7ffc")),
    std::make_tuple(TR::InstOpCode::bne,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409e8000")),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40820001")),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4082fffd")),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409e7ffd")),
    std::make_tuple(TR::InstOpCode::bnel,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409e8001")),
    std::make_tuple(TR::InstOpCode::bnelr, TR::RealRegister::cr0,      -1, TRTest::BinaryInstruction("4c820020")),
    std::make_tuple(TR::InstOpCode::bnelr, TR::RealRegister::cr7,      -1, TRTest::BinaryInstruction("4c9e0020")),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("40830000")),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4083fffc")),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("409f7ffc")),
    std::make_tuple(TR::InstOpCode::bnun,  TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("409f8000")),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr0,       0, TRTest::BinaryInstruction("41830000")),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr0,      -4, TRTest::BinaryInstruction("4183fffc")),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr7,  0x7ffc, TRTest::BinaryInstruction("419f7ffc")),
    std::make_tuple(TR::InstOpCode::bun,   TR::RealRegister::cr7, -0x8000, TRTest::BinaryInstruction("419f8000"))
)));

INSTANTIATE_TEST_CASE_P(Branch, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::b,     TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bctr,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bctrl, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beq,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beql,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::beqlr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bge,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgel,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgelr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgtl,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bgtlr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bl,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ble,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blel,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blelr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blr,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blrl,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::blt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bltl,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bltlr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bne,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnel,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnelr, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bnun,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bun,   TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCImm2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x0, TRTest::BinaryInstruction("fc01010c")),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0xf, 0x0, TRTest::BinaryInstruction("fc01f10c")),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0xf, TRTest::BinaryInstruction("ff80010c")),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x7, TRTest::BinaryInstruction("ff81010c")),
    std::make_tuple(TR::InstOpCode::mtfsfi, 0x0, 0x8, TRTest::BinaryInstruction("fc00010c"))
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCTrg1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfcr,      TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c000026")),
    std::make_tuple(TR::InstOpCode::mfcr,      TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00026")),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c0902a6")),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe902a6")),
    std::make_tuple(TR::InstOpCode::mffs,      TR::RealRegister::gr0,  TRTest::BinaryInstruction("fc00048e")),
    std::make_tuple(TR::InstOpCode::mffs,      TR::RealRegister::gr31, TRTest::BinaryInstruction("ffe0048e")),
    std::make_tuple(TR::InstOpCode::mflr,      TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c0802a6")),
    std::make_tuple(TR::InstOpCode::mflr,      TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe802a6")),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c0222a6")),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe222a6")),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c0322a6")),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe322a6"))
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrfs, TR::RealRegister::cr7, 0, TRTest::BinaryInstruction("ff800080")),
    std::make_tuple(TR::InstOpCode::mcrfs, TR::RealRegister::cr0, 7, TRTest::BinaryInstruction("fc1c0080"))
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCSrc1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtctr,  TR::RealRegister::gr0,     0u, TRTest::BinaryInstruction("7c0903a6")),
    std::make_tuple(TR::InstOpCode::mtctr,  TR::RealRegister::gr31,    0u, TRTest::BinaryInstruction("7fe903a6")),
    std::make_tuple(TR::InstOpCode::mtfsf,  TR::RealRegister::fp0,  0xffu, TRTest::BinaryInstruction("fdfe058e")),
    std::make_tuple(TR::InstOpCode::mtfsf,  TR::RealRegister::fp31, 0x00u, TRTest::BinaryInstruction("fc00fd8e")),
    std::make_tuple(TR::InstOpCode::mtfsfl, TR::RealRegister::fp0,  0xffu, TRTest::BinaryInstruction("fffe058e")),
    std::make_tuple(TR::InstOpCode::mtfsfl, TR::RealRegister::fp31, 0x00u, TRTest::BinaryInstruction("fe00fd8e")),
    std::make_tuple(TR::InstOpCode::mtfsfw, TR::RealRegister::fp0,  0xffu, TRTest::BinaryInstruction("fdff058e")),
    std::make_tuple(TR::InstOpCode::mtfsfw, TR::RealRegister::fp31, 0x00u, TRTest::BinaryInstruction("fc01fd8e")),
    std::make_tuple(TR::InstOpCode::mtlr,   TR::RealRegister::gr0,     0u, TRTest::BinaryInstruction("7c0803a6")),
    std::make_tuple(TR::InstOpCode::mtlr,   TR::RealRegister::gr31,    0u, TRTest::BinaryInstruction("7fe803a6"))
));

INSTANTIATE_TEST_CASE_P(SprMove, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrfs,     TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfcr,      TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfctr,     TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mffs,      TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mflr,      TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mftexasr,  TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mftexasru, TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtctr,     TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtfsf,     TR::InstOpCode::mtfsf_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mtfsfi,    TR::InstOpCode::mtfsfi_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mtfsfl,    TR::InstOpCode::mtfsfl_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mtfsfw,    TR::InstOpCode::mtfsfw_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mtlr,      TR::InstOpCode::bad,      TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Trap, PPCSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("089f0000")),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("08807fff")),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("089f8000")),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0880ffff")),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("099f0000")),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("09807fff")),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("099f8000")),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0980ffff")),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("091f0000")),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("09007fff")),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("091f8000")),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0900ffff")),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0a9f0000")),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0a807fff")),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0a9f8000")),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0a80ffff")),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("08bf0000")),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("08a07fff")),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("08bf8000")),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("08a0ffff")),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("083f0000")),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("08207fff")),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("083f8000")),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0820ffff")),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("08df0000")),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("08c07fff")),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("08df8000")),
    std::make_tuple(TR::InstOpCode::tdllei, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("08c0ffff")),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("085f0000")),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("08407fff")),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("085f8000")),
    std::make_tuple(TR::InstOpCode::tdllti, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0840ffff")),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0a1f0000")),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0a007fff")),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0a1f8000")),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0a00ffff")),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0b1f0000")),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0b007fff")),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0b1f8000")),
    std::make_tuple(TR::InstOpCode::tdneqi, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0b00ffff")),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0c9f0000")),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0c807fff")),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0c9f8000")),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0c80ffff")),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0d9f0000")),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0d807fff")),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0d9f8000")),
    std::make_tuple(TR::InstOpCode::twgei,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0d80ffff")),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0d1f0000")),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0d007fff")),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0d1f8000")),
    std::make_tuple(TR::InstOpCode::twgti,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0d00ffff")),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0e9f0000")),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0e807fff")),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0e9f8000")),
    std::make_tuple(TR::InstOpCode::twlei,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0e80ffff")),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0cbf0000")),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0ca07fff")),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0cbf8000")),
    std::make_tuple(TR::InstOpCode::twlgei, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0ca0ffff")),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0c3f0000")),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0c207fff")),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0c3f8000")),
    std::make_tuple(TR::InstOpCode::twlgti, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0c20ffff")),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0cdf0000")),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0cc07fff")),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0cdf8000")),
    std::make_tuple(TR::InstOpCode::twllei, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0cc0ffff")),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0c5f0000")),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0c407fff")),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0c5f8000")),
    std::make_tuple(TR::InstOpCode::twllti, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0c40ffff")),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0e1f0000")),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0e007fff")),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0e1f8000")),
    std::make_tuple(TR::InstOpCode::twlti,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0e00ffff")),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("0f1f0000")),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("0f007fff")),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("0f1f8000")),
    std::make_tuple(TR::InstOpCode::twneqi, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("0f00ffff"))
)));

INSTANTIATE_TEST_CASE_P(Trap, PPCSrc2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tdeq,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c9f0088")),
    std::make_tuple(TR::InstOpCode::tdeq,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c80f888")),
    std::make_tuple(TR::InstOpCode::tdge,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7d9f0088")),
    std::make_tuple(TR::InstOpCode::tdge,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7d80f888")),
    std::make_tuple(TR::InstOpCode::tdgt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7d1f0088")),
    std::make_tuple(TR::InstOpCode::tdgt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7d00f888")),
    std::make_tuple(TR::InstOpCode::tdle,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7e9f0088")),
    std::make_tuple(TR::InstOpCode::tdle,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7e80f888")),
    std::make_tuple(TR::InstOpCode::tdlge, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7cbf0088")),
    std::make_tuple(TR::InstOpCode::tdlge, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7ca0f888")),
    std::make_tuple(TR::InstOpCode::tdlgt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c3f0088")),
    std::make_tuple(TR::InstOpCode::tdlgt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c20f888")),
    std::make_tuple(TR::InstOpCode::tdlle, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7cdf0088")),
    std::make_tuple(TR::InstOpCode::tdlle, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7cc0f888")),
    std::make_tuple(TR::InstOpCode::tdllt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c5f0088")),
    std::make_tuple(TR::InstOpCode::tdllt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c40f888")),
    std::make_tuple(TR::InstOpCode::tdlt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7e1f0088")),
    std::make_tuple(TR::InstOpCode::tdlt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7e00f888")),
    std::make_tuple(TR::InstOpCode::tdneq, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7f1f0088")),
    std::make_tuple(TR::InstOpCode::tdneq, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7f00f888")),
    std::make_tuple(TR::InstOpCode::tweq,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c9f0008")),
    std::make_tuple(TR::InstOpCode::tweq,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c80f808")),
    std::make_tuple(TR::InstOpCode::twge,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7d9f0008")),
    std::make_tuple(TR::InstOpCode::twge,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7d80f808")),
    std::make_tuple(TR::InstOpCode::twgt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7d1f0008")),
    std::make_tuple(TR::InstOpCode::twgt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7d00f808")),
    std::make_tuple(TR::InstOpCode::twle,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7e9f0008")),
    std::make_tuple(TR::InstOpCode::twle,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7e80f808")),
    std::make_tuple(TR::InstOpCode::twlge, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7cbf0008")),
    std::make_tuple(TR::InstOpCode::twlge, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7ca0f808")),
    std::make_tuple(TR::InstOpCode::twlgt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c3f0008")),
    std::make_tuple(TR::InstOpCode::twlgt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c20f808")),
    std::make_tuple(TR::InstOpCode::twlle, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7cdf0008")),
    std::make_tuple(TR::InstOpCode::twlle, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7cc0f808")),
    std::make_tuple(TR::InstOpCode::twllt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c5f0008")),
    std::make_tuple(TR::InstOpCode::twllt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c40f808")),
    std::make_tuple(TR::InstOpCode::twlt,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7e1f0008")),
    std::make_tuple(TR::InstOpCode::twlt,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7e00f808")),
    std::make_tuple(TR::InstOpCode::twneq, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7f1f0008")),
    std::make_tuple(TR::InstOpCode::twneq, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7f00f808"))
));

INSTANTIATE_TEST_CASE_P(Trap, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tdeq,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdeqi,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdge,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgei,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdgti,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdle,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlei,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlge,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgei, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgt,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlgti, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlle,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllei, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllt,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdllti, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdlti,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tdneq,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneqi, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tweq,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::tweqi,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twge,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgei,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twgti,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twle,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlei,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlge,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgei, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgt,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlgti, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlle,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllei, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllt,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twllti, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twlti,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneq,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::twneqi, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("38220000")),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("3be07fff")),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("381f8000")),
    std::make_tuple(TR::InstOpCode::addi,   TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("39f0ffff")),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("30220000")),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("33e07fff")),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("301f8000")),
    std::make_tuple(TR::InstOpCode::addic,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("31f0ffff")),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("3c220000")),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("3fe07fff")),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("3c1f8000")),
    std::make_tuple(TR::InstOpCode::addis,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("3df0ffff")),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("1c220000")),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("1fe07fff")),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("1c1f8000")),
    std::make_tuple(TR::InstOpCode::mulli,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("1df0ffff")),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("20220000")),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("23e07fff")),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("201f8000")),
    std::make_tuple(TR::InstOpCode::subfic, TR::RealRegister::gr15, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("21f0ffff"))
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("3be00000")),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("38007fff")),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("3be08000")),
    std::make_tuple(TR::InstOpCode::li,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("3800ffff")),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("3fe00000")),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("3c007fff")),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("3fe08000")),
    std::make_tuple(TR::InstOpCode::lis, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("3c00ffff"))
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::addme,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe001d4")),
    std::make_tuple(TR::InstOpCode::addme,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f01d4")),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe005d4")),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f05d4")),
    std::make_tuple(TR::InstOpCode::addze,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00194")),
    std::make_tuple(TR::InstOpCode::addze,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f0194")),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00594")),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f0594")),
    std::make_tuple(TR::InstOpCode::neg,     TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe000d0")),
    std::make_tuple(TR::InstOpCode::neg,     TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f00d0")),
    std::make_tuple(TR::InstOpCode::nego,    TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe004d0")),
    std::make_tuple(TR::InstOpCode::nego,    TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f04d0")),
    std::make_tuple(TR::InstOpCode::subfme,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe001d0")),
    std::make_tuple(TR::InstOpCode::subfme,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f01d0")),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe005d0")),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f05d0")),
    std::make_tuple(TR::InstOpCode::subfze,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00190")),
    std::make_tuple(TR::InstOpCode::subfze,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f0190")),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00590")),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c1f0590"))
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00214")),
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0214")),
    std::make_tuple(TR::InstOpCode::add,    TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa14")),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00014")),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0014")),
    std::make_tuple(TR::InstOpCode::addc,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f814")),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00414")),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0414")),
    std::make_tuple(TR::InstOpCode::addco,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fc14")),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00114")),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0114")),
    std::make_tuple(TR::InstOpCode::adde,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f914")),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00514")),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0514")),
    std::make_tuple(TR::InstOpCode::addeo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fd14")),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00614")),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0614")),
    std::make_tuple(TR::InstOpCode::addo,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fe14")),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe003d2")),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f03d2")),
    std::make_tuple(TR::InstOpCode::divd,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fbd2")),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe007d2")),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f07d2")),
    std::make_tuple(TR::InstOpCode::divdo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00ffd2")),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00392")),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0392")),
    std::make_tuple(TR::InstOpCode::divdu,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fb92")),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00792")),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0792")),
    std::make_tuple(TR::InstOpCode::divduo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00ff92")),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe003d6")),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f03d6")),
    std::make_tuple(TR::InstOpCode::divw,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fbd6")),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe007d6")),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f07d6")),
    std::make_tuple(TR::InstOpCode::divwo,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00ffd6")),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00396")),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0396")),
    std::make_tuple(TR::InstOpCode::divwu,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fb96")),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00796")),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0796")),
    std::make_tuple(TR::InstOpCode::divwuo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00ff96")),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00212")),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0212")),
    std::make_tuple(TR::InstOpCode::modud,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa12")),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00612")),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0612")),
    std::make_tuple(TR::InstOpCode::modsd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fe12")),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00216")),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0216")),
    std::make_tuple(TR::InstOpCode::moduw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa16")),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00616")),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0616")),
    std::make_tuple(TR::InstOpCode::modsw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fe16")),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00092")),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0092")),
    std::make_tuple(TR::InstOpCode::mulhd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f892")),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00012")),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0012")),
    std::make_tuple(TR::InstOpCode::mulhdu, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f812")),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00096")),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0096")),
    std::make_tuple(TR::InstOpCode::mulhw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f896")),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00016")),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0016")),
    std::make_tuple(TR::InstOpCode::mulhwu, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f816")),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe001d2")),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f01d2")),
    std::make_tuple(TR::InstOpCode::mulld,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f9d2")),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe005d2")),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f05d2")),
    std::make_tuple(TR::InstOpCode::mulldo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fdd2")),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe001d6")),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f01d6")),
    std::make_tuple(TR::InstOpCode::mullw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f9d6")),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe005d6")),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f05d6")),
    std::make_tuple(TR::InstOpCode::mullwo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fdd6")),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00050")),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0050")),
    std::make_tuple(TR::InstOpCode::subf,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f850")),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00010")),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0010")),
    std::make_tuple(TR::InstOpCode::subfc,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f810")),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00410")),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0410")),
    std::make_tuple(TR::InstOpCode::subfco, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fc10")),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00110")),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0110")),
    std::make_tuple(TR::InstOpCode::subfe,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f910")),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00510")),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0510")),
    std::make_tuple(TR::InstOpCode::subfeo, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fd10"))
)));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("13e00033")),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("101f0033")),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("1000f833")),
    std::make_tuple(TR::InstOpCode::maddld, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("100007f3"))
));

INSTANTIATE_TEST_CASE_P(Arithmetic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::add,     TR::InstOpCode::add_r,     TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addc,    TR::InstOpCode::addc_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addco,   TR::InstOpCode::addco_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::adde,    TR::InstOpCode::adde_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addeo,   TR::InstOpCode::addeo_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addo,    TR::InstOpCode::addo_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addi,    TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::addic,   TR::InstOpCode::addic_r,   TRTest::BinaryInstruction("04000000")),
    std::make_tuple(TR::InstOpCode::addis,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::addme,   TR::InstOpCode::addme_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addmeo,  TR::InstOpCode::addmeo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addze,   TR::InstOpCode::addze_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::addzeo,  TR::InstOpCode::addzeo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divd,    TR::InstOpCode::divd_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divdo,   TR::InstOpCode::divdo_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divdu,   TR::InstOpCode::divdu_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divwuo,  TR::InstOpCode::divwuo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divw,    TR::InstOpCode::divw_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divwo,   TR::InstOpCode::divwo_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divwu,   TR::InstOpCode::divwu_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::divwuo,  TR::InstOpCode::divwuo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::li,      TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lis,     TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::maddld,  TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modsd,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modsw,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::modud,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::moduw,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mulhd,   TR::InstOpCode::mulhd_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulhdu,  TR::InstOpCode::mulhdu_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulhw,   TR::InstOpCode::mulhw_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulhwu,  TR::InstOpCode::mulhwu_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulld,   TR::InstOpCode::mulld_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulldo,  TR::InstOpCode::mulldo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mulli,   TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mullw,   TR::InstOpCode::mullw_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mullwo,  TR::InstOpCode::mullwo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::neg,     TR::InstOpCode::neg_r,     TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::nego,    TR::InstOpCode::nego_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subf,    TR::InstOpCode::subf_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfc,   TR::InstOpCode::subfc_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfco,  TR::InstOpCode::subfco_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfe,   TR::InstOpCode::subfe_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfeo,  TR::InstOpCode::subfeo_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfic,  TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::subfme,  TR::InstOpCode::subfme_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfmeo, TR::InstOpCode::subfmeo_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfze,  TR::InstOpCode::subfze_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::subfzeo, TR::InstOpCode::subfzeo_r, TRTest::BinaryInstruction("00000001"))
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("2c820000")),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("2f807fff")),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr0, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("2c1f8000")),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::RealRegister::cr3, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("2d90ffff")),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("2ca20000")),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("2fa07fff")),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr0, TR::RealRegister::gr31, 0xffff8000u, TRTest::BinaryInstruction("2c3f8000")),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::RealRegister::cr3, TR::RealRegister::gr16, 0xffffffffu, TRTest::BinaryInstruction("2db0ffff")),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("28820000")),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("2b807fff")),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr0, TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("281f8000")),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::RealRegister::cr3, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("2990ffff")),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr1, TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("28a20000")),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr7, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("2ba07fff")),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr0, TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("283f8000")),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::RealRegister::cr3, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("29b0ffff"))
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7f800000")),
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0000")),
    std::make_tuple(TR::InstOpCode::cmp4,   TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f800")),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fa00000")),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c3f0000")),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c20f800")),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7f800040")),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0040")),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f840")),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fa00040")),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c3f0040")),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c20f840")),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7f8001c0")),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f01c0")),
    std::make_tuple(TR::InstOpCode::cmpeqb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f9c0")),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr7, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ff800040")),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr0, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0040")),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::RealRegister::cr0, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f840")),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr7, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ff800000")),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr0, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0000")),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::RealRegister::cr0, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f800"))
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0, TRTest::BinaryInstruction("7f800180")),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  0, TRTest::BinaryInstruction("7c1f0180")),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0, TRTest::BinaryInstruction("7c00f980")),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr7, TR::RealRegister::gr0,  TR::RealRegister::gr0,  1, TRTest::BinaryInstruction("7fa00180")),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr31, TR::RealRegister::gr0,  1, TRTest::BinaryInstruction("7c3f0180")),
    std::make_tuple(TR::InstOpCode::cmprb, TR::RealRegister::cr0, TR::RealRegister::gr0,  TR::RealRegister::gr31, 1, TRTest::BinaryInstruction("7c20f980"))
));

INSTANTIATE_TEST_CASE_P(Comparison, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cmp4,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmp8,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpi4,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpi8,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpl4,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpl8,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpli4, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmpli8, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cmprb,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcmpo,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcmpu,  TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mcrf, TR::RealRegister::cr7,  TR::RealRegister::cr0, TRTest::BinaryInstruction("4f800000")),
    std::make_tuple(TR::InstOpCode::mcrf, TR::RealRegister::cr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("4c1c0000")),
    std::make_tuple(TR::InstOpCode::setb, TR::RealRegister::gr31, TR::RealRegister::cr0, TRTest::BinaryInstruction("7fe00100")),
    std::make_tuple(TR::InstOpCode::setb, TR::RealRegister::gr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("7c1c0100"))
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCSrc1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mtcr,   TR::RealRegister::gr0,      0, TRTest::BinaryInstruction("7c0ff120")),
    std::make_tuple(TR::InstOpCode::mtcr,   TR::RealRegister::gr31,     0, TRTest::BinaryInstruction("7feff120")),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr0,  0x01u, TRTest::BinaryInstruction("7c101120")),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr31, 0x01u, TRTest::BinaryInstruction("7ff01120")),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr0,  0x80u, TRTest::BinaryInstruction("7c180120")),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::RealRegister::gr31, 0x80u, TRTest::BinaryInstruction("7ff80120"))
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr0,  0x01u, TRTest::BinaryInstruction("7c101026")),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr31, 0x01u, TRTest::BinaryInstruction("7ff01026")),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr0,  0x80u, TRTest::BinaryInstruction("7c180026")),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::RealRegister::gr31, 0x80u, TRTest::BinaryInstruction("7ff80026"))
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::setbc,   TR::RealRegister::gr31, TR::RealRegister::cr0, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7fe00300")),
    std::make_tuple(TR::InstOpCode::setbc,   TR::RealRegister::gr0,  TR::RealRegister::cr7, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7c1c0300")),
    std::make_tuple(TR::InstOpCode::setbc,   TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_GT, TRTest::BinaryInstruction("7c010300")),
    std::make_tuple(TR::InstOpCode::setbc,   TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_EQ, TRTest::BinaryInstruction("7c020300")),
    std::make_tuple(TR::InstOpCode::setbc,   TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_FU, TRTest::BinaryInstruction("7c030300")),
    std::make_tuple(TR::InstOpCode::setbcr,  TR::RealRegister::gr31, TR::RealRegister::cr0, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7fe00340")),
    std::make_tuple(TR::InstOpCode::setbcr,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7c1c0340")),
    std::make_tuple(TR::InstOpCode::setbcr,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_GT, TRTest::BinaryInstruction("7c010340")),
    std::make_tuple(TR::InstOpCode::setbcr,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_EQ, TRTest::BinaryInstruction("7c020340")),
    std::make_tuple(TR::InstOpCode::setbcr,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_FU, TRTest::BinaryInstruction("7c030340")),
    std::make_tuple(TR::InstOpCode::setnbc,  TR::RealRegister::gr31, TR::RealRegister::cr0, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7fe00380")),
    std::make_tuple(TR::InstOpCode::setnbc,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7c1c0380")),
    std::make_tuple(TR::InstOpCode::setnbc,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_GT, TRTest::BinaryInstruction("7c010380")),
    std::make_tuple(TR::InstOpCode::setnbc,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_EQ, TRTest::BinaryInstruction("7c020380")),
    std::make_tuple(TR::InstOpCode::setnbc,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_FU, TRTest::BinaryInstruction("7c030380")),
    std::make_tuple(TR::InstOpCode::setnbcr, TR::RealRegister::gr31, TR::RealRegister::cr0, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7fe003c0")),
    std::make_tuple(TR::InstOpCode::setnbcr, TR::RealRegister::gr0,  TR::RealRegister::cr7, TR::RealRegister::CRCC_LT, TRTest::BinaryInstruction("7c1c03c0")),
    std::make_tuple(TR::InstOpCode::setnbcr, TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_GT, TRTest::BinaryInstruction("7c0103c0")),
    std::make_tuple(TR::InstOpCode::setnbcr, TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_EQ, TRTest::BinaryInstruction("7c0203c0")),
    std::make_tuple(TR::InstOpCode::setnbcr, TR::RealRegister::gr0,  TR::RealRegister::cr0, TR::RealRegister::CRCC_FU, TRTest::BinaryInstruction("7c0303c0"))
));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::iseleq, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7fe0009e")),
    std::make_tuple(TR::InstOpCode::iseleq, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7c1f009e")),
    std::make_tuple(TR::InstOpCode::iseleq, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::cr0, TRTest::BinaryInstruction("7c00f89e")),
    std::make_tuple(TR::InstOpCode::iseleq, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("7c00079e")),
    std::make_tuple(TR::InstOpCode::iselgt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7fe0005e")),
    std::make_tuple(TR::InstOpCode::iselgt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7c1f005e")),
    std::make_tuple(TR::InstOpCode::iselgt, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::cr0, TRTest::BinaryInstruction("7c00f85e")),
    std::make_tuple(TR::InstOpCode::iselgt, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("7c00075e")),
    std::make_tuple(TR::InstOpCode::isellt, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7fe0001e")),
    std::make_tuple(TR::InstOpCode::isellt, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7c1f001e")),
    std::make_tuple(TR::InstOpCode::isellt, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::cr0, TRTest::BinaryInstruction("7c00f81e")),
    std::make_tuple(TR::InstOpCode::isellt, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("7c00071e")),
    std::make_tuple(TR::InstOpCode::iselun, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7fe000de")),
    std::make_tuple(TR::InstOpCode::iselun, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::cr0, TRTest::BinaryInstruction("7c1f00de")),
    std::make_tuple(TR::InstOpCode::iselun, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::cr0, TRTest::BinaryInstruction("7c00f8de")),
    std::make_tuple(TR::InstOpCode::iselun, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::cr7, TRTest::BinaryInstruction("7c0007de"))
));

#define CR_RT(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RT)
#define CR_RA(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RA)
#define CR_RB(cond) (TR::RealRegister::CRCC_ ## cond << TR::RealRegister::pos_RB)
#define CR_TEST_ALL (CR_RT(LT) | CR_RA(GT) | CR_RB(EQ))

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCTrg1Src2ImmEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint64_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800202")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0202")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e202")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600202")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030202")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001a02")),
    std::make_tuple(TR::InstOpCode::crand,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897202")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800102")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0102")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e102")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600102")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030102")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001902")),
    std::make_tuple(TR::InstOpCode::crandc, TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897102")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800242")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0242")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e242")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600242")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030242")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001a42")),
    std::make_tuple(TR::InstOpCode::creqv,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897242")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f8001c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c01c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e1c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c6001c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c0301c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c0019c2")),
    std::make_tuple(TR::InstOpCode::crnand, TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c8971c2")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800042")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0042")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e042")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600042")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030042")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001842")),
    std::make_tuple(TR::InstOpCode::crnor,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897042")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800382")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0382")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e382")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600382")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030382")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001b82")),
    std::make_tuple(TR::InstOpCode::cror,   TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897382")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800342")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0342")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e342")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600342")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030342")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001b42")),
    std::make_tuple(TR::InstOpCode::crorc,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897342")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr7, TR::RealRegister::cr0, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4f800182")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr7, TR::RealRegister::cr0, 0,           TRTest::BinaryInstruction("4c1c0182")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr7, 0,           TRTest::BinaryInstruction("4c00e182")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RT(SO),   TRTest::BinaryInstruction("4c600182")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RA(SO),   TRTest::BinaryInstruction("4c030182")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr0, TR::RealRegister::cr0, TR::RealRegister::cr0, CR_RB(SO),   TRTest::BinaryInstruction("4c001982")),
    std::make_tuple(TR::InstOpCode::crxor,  TR::RealRegister::cr1, TR::RealRegister::cr2, TR::RealRegister::cr3, CR_TEST_ALL, TRTest::BinaryInstruction("4c897182"))
)));

INSTANTIATE_TEST_CASE_P(ConditionLogic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::crand,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crandc, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::creqv,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crnand, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crnor,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::cror,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crorc,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::crxor,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mcrf,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfocrf, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtcr,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtocrf, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::setb,   TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("70410000")),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("701f7fff")),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("73e08000")),
    std::make_tuple(TR::InstOpCode::andi_r,  TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("720fffff")),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("74410000")),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("741f7fff")),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("77e08000")),
    std::make_tuple(TR::InstOpCode::andis_r, TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("760fffff")),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("60410000")),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("601f7fff")),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("63e08000")),
    std::make_tuple(TR::InstOpCode::ori,     TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("620fffff")),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("64410000")),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("641f7fff")),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("67e08000")),
    std::make_tuple(TR::InstOpCode::oris,    TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("660fffff")),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("68410000")),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("681f7fff")),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("6be08000")),
    std::make_tuple(TR::InstOpCode::xori,    TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("6a0fffff")),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr1,  TR::RealRegister::gr2,  0x00000000u, TRTest::BinaryInstruction("6c410000")),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00007fffu, TRTest::BinaryInstruction("6c1f7fff")),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00008000u, TRTest::BinaryInstruction("6fe08000")),
    std::make_tuple(TR::InstOpCode::xoris,   TR::RealRegister::gr15, TR::RealRegister::gr16, 0x0000ffffu, TRTest::BinaryInstruction("6e0fffff"))
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::brd,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0176")),
    std::make_tuple(TR::InstOpCode::brd,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00176")),
    std::make_tuple(TR::InstOpCode::brh,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f01b6")),
    std::make_tuple(TR::InstOpCode::brh,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe001b6")),
    std::make_tuple(TR::InstOpCode::brw,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0136")),
    std::make_tuple(TR::InstOpCode::brw,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00136")),
    std::make_tuple(TR::InstOpCode::extsb, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0774")),
    std::make_tuple(TR::InstOpCode::extsb, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00774")),
    std::make_tuple(TR::InstOpCode::extsh, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0734")),
    std::make_tuple(TR::InstOpCode::extsh, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00734")),
    std::make_tuple(TR::InstOpCode::extsw, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f07b4")),
    std::make_tuple(TR::InstOpCode::extsw, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe007b4")),
    std::make_tuple(TR::InstOpCode::mr,    TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("601f0000")),
    std::make_tuple(TR::InstOpCode::mr,    TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("63e00000"))
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0038")),
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00038")),
    std::make_tuple(TR::InstOpCode::AND,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f838")),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0078")),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00078")),
    std::make_tuple(TR::InstOpCode::andc, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f878")),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0238")),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00238")),
    std::make_tuple(TR::InstOpCode::eqv,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa38")),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f03b8")),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe003b8")),
    std::make_tuple(TR::InstOpCode::nand, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fbb8")),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f00f8")),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe000f8")),
    std::make_tuple(TR::InstOpCode::nor,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f8f8")),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0378")),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00378")),
    std::make_tuple(TR::InstOpCode::OR,   TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fb78")),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0338")),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00338")),
    std::make_tuple(TR::InstOpCode::orc,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fb38")),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0278")),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00278")),
    std::make_tuple(TR::InstOpCode::XOR,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa78"))
));

INSTANTIATE_TEST_CASE_P(Bitwise, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::AND,   TR::InstOpCode::and_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::andc,  TR::InstOpCode::andc_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::bad,   TR::InstOpCode::andi_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,   TR::InstOpCode::andis_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::brd,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::brh,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::brw,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::extsb, TR::InstOpCode::extsb_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::extsw, TR::InstOpCode::extsw_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::eqv,   TR::InstOpCode::eqv_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::mr,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::nand,  TR::InstOpCode::nand_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::OR,    TR::InstOpCode::or_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::orc,   TR::InstOpCode::orc_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::ori,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::oris,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::XOR,   TR::InstOpCode::xor_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::xori,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xoris, TR::InstOpCode::bad,     TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(BitCounting, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0074")),
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00074")),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0034")),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe00034")),
    std::make_tuple(TR::InstOpCode::popcntd, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f03f4")),
    std::make_tuple(TR::InstOpCode::popcntd, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe003f4")),
    std::make_tuple(TR::InstOpCode::popcntw, TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f02f4")),
    std::make_tuple(TR::InstOpCode::popcntw, TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7fe002f4"))
));

INSTANTIATE_TEST_CASE_P(BitCounting, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::cntlzd,  TR::InstOpCode::cntlzd_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::cntlzw,  TR::InstOpCode::cntlzw_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::popcntd, TR::InstOpCode::bad,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::popcntw, TR::InstOpCode::bad,      TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, TRTest::BinaryInstruction("7c4106f4")),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, TRTest::BinaryInstruction("7c1fa6f4")),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr0,  TR::RealRegister::gr31, 63u, TRTest::BinaryInstruction("7fe0fef6")),
    std::make_tuple(TR::InstOpCode::extswsli, TR::RealRegister::gr15, TR::RealRegister::gr16, 32u, TRTest::BinaryInstruction("7e0f06f6")),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, TRTest::BinaryInstruction("7c410674")),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, TRTest::BinaryInstruction("7c1fa674")),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 63u, TRTest::BinaryInstruction("7fe0fe76")),
    std::make_tuple(TR::InstOpCode::sradi,    TR::RealRegister::gr15, TR::RealRegister::gr16, 32u, TRTest::BinaryInstruction("7e0f0676")),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr1,  TR::RealRegister::gr2,   0u, TRTest::BinaryInstruction("7c410670")),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr31, TR::RealRegister::gr0,  20u, TRTest::BinaryInstruction("7c1fa670")),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr0,  TR::RealRegister::gr31, 31u, TRTest::BinaryInstruction("7fe0fe70")),
    std::make_tuple(TR::InstOpCode::srawi,    TR::RealRegister::gr15, TR::RealRegister::gr16, 16u, TRTest::BinaryInstruction("7e0f8670"))
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src1Imm2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("781f0008")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7be00008")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, TRTest::BinaryInstruction("7800f80a")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, TRTest::BinaryInstruction("780007e8")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, TRTest::BinaryInstruction("7800f808")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, TRTest::BinaryInstruction("7800f828")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffff00000000ull, TRTest::BinaryInstruction("7800000a")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffff00000000ull, TRTest::BinaryInstruction("7800040a")),
    std::make_tuple(TR::InstOpCode::rldic,  TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, TRTest::BinaryInstruction("780007c8")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("781f0000")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7be00000")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7800f802")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, TRTest::BinaryInstruction("780007e0")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7800f800")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x00000000ffffffffull, TRTest::BinaryInstruction("7800f820")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffffffffffffull, TRTest::BinaryInstruction("78000002")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffffffffffffull, TRTest::BinaryInstruction("78000402")),
    std::make_tuple(TR::InstOpCode::rldicl, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, TRTest::BinaryInstruction("780007c0")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x8000000000000000ull, TRTest::BinaryInstruction("781f0004")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x8000000000000000ull, TRTest::BinaryInstruction("7be00004")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, TRTest::BinaryInstruction("7800f806")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("780007e4")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7800ffe4")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, TRTest::BinaryInstruction("7800f824")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffffffffffffull, TRTest::BinaryInstruction("780007e6")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffff000000000000ull, TRTest::BinaryInstruction("780003c6")),
    std::make_tuple(TR::InstOpCode::rldicr, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0xffffffff00000000ull, TRTest::BinaryInstruction("780007c4")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("781f000c")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0xffffffffffffffffull, TRTest::BinaryInstruction("7be0000c")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  63u, 0x8000000000000000ull, TRTest::BinaryInstruction("7800f80e")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000000001ull, TRTest::BinaryInstruction("780007ec")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0xffffffff80000000ull, TRTest::BinaryInstruction("7800f80c")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, TRTest::BinaryInstruction("7800f82c")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0xffffffff00000000ull, TRTest::BinaryInstruction("7800000e")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  32u, 0x0000ffff00000000ull, TRTest::BinaryInstruction("7800040e")),
    std::make_tuple(TR::InstOpCode::rldimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000001ffffffffull, TRTest::BinaryInstruction("780007cc")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x0000000080000000ull, TRTest::BinaryInstruction("501f0000")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x0000000080000000ull, TRTest::BinaryInstruction("53e00000")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, TRTest::BinaryInstruction("5000f800")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000080000001ull, TRTest::BinaryInstruction("500007c0")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ffffffffull, TRTest::BinaryInstruction("5000003e")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000ffff00ull, TRTest::BinaryInstruction("5000022e")),
    std::make_tuple(TR::InstOpCode::rlwimi, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ff0000ffull, TRTest::BinaryInstruction("5000060e")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr31, TR::RealRegister::gr0,   0u, 0x0000000080000000ull, TRTest::BinaryInstruction("541f0000")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr31,  0u, 0x0000000080000000ull, TRTest::BinaryInstruction("57e00000")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  31u, 0x0000000080000000ull, TRTest::BinaryInstruction("5400f800")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000080000001ull, TRTest::BinaryInstruction("540007c0")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ffffffffull, TRTest::BinaryInstruction("5400003e")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x0000000000ffff00ull, TRTest::BinaryInstruction("5400022e")),
    std::make_tuple(TR::InstOpCode::rlwinm, TR::RealRegister::gr0,  TR::RealRegister::gr0,   0u, 0x00000000ff0000ffull, TRTest::BinaryInstruction("5400060e"))
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0036")),
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00036")),
    std::make_tuple(TR::InstOpCode::sld,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f836")),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0030")),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00030")),
    std::make_tuple(TR::InstOpCode::slw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00f830")),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0634")),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00634")),
    std::make_tuple(TR::InstOpCode::srad, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fe34")),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0630")),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00630")),
    std::make_tuple(TR::InstOpCode::sraw, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fe30")),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0436")),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00436")),
    std::make_tuple(TR::InstOpCode::srd,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fc36")),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f0430")),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe00430")),
    std::make_tuple(TR::InstOpCode::srw,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fc30"))
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0xffffffffffffffffull, TRTest::BinaryInstruction("781f0010")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0xffffffffffffffffull, TRTest::BinaryInstruction("7be00010")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0xffffffffffffffffull, TRTest::BinaryInstruction("7800f810")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000000001ull, TRTest::BinaryInstruction("780007f0")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ffffffffull, TRTest::BinaryInstruction("78000030")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000001ffffffffull, TRTest::BinaryInstruction("780007d0")),
    std::make_tuple(TR::InstOpCode::rldcl, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000ffffffffffffull, TRTest::BinaryInstruction("78000410")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ffffffffull, TRTest::BinaryInstruction("5c1f003e")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  0x00000000ffffffffull, TRTest::BinaryInstruction("5fe0003e")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, 0x00000000ffffffffull, TRTest::BinaryInstruction("5c00f83e")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000080000000ull, TRTest::BinaryInstruction("5c000000")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000000001ull, TRTest::BinaryInstruction("5c0007fe")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000080000001ull, TRTest::BinaryInstruction("5c0007c0")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x0000000000ffff00ull, TRTest::BinaryInstruction("5c00022e")),
    std::make_tuple(TR::InstOpCode::rlwnm, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  0x00000000ff0000ffull, TRTest::BinaryInstruction("5c00060e"))
));

INSTANTIATE_TEST_CASE_P(ShiftAndRotate, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::extswsli, TR::InstOpCode::extswsli_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rldcl,    TR::InstOpCode::rldcl_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rldic,    TR::InstOpCode::rldic_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rldicl,   TR::InstOpCode::rldicl_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rldicr,   TR::InstOpCode::rldicr_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rldimi,   TR::InstOpCode::rldimi_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rlwimi,   TR::InstOpCode::rlwimi_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rlwinm,   TR::InstOpCode::rlwinm_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::rlwnm,    TR::InstOpCode::rlwnm_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::sld,      TR::InstOpCode::sld_r,      TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::slw,      TR::InstOpCode::slw_r,      TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::srad,     TR::InstOpCode::srad_r,     TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::sradi,    TR::InstOpCode::sradi_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::sraw,     TR::InstOpCode::sraw_r,     TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::srawi,    TR::InstOpCode::srawi_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::srd,      TR::InstOpCode::srd_r,      TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::srw,      TR::InstOpCode::srw_r,      TRTest::BinaryInstruction("00000001"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr31, 0x00000000u, TRTest::BinaryInstruction("13e0030c")),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr0,  0x0000000fu, TRTest::BinaryInstruction("100f030c")),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr15, 0xfffffff0u, TRTest::BinaryInstruction("11f0030c")),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::RealRegister::vr16, 0xffffffffu, TRTest::BinaryInstruction("121f030c")),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr31, 0x00000000u, TRTest::BinaryInstruction("13e0034c")),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr0,  0x0000000fu, TRTest::BinaryInstruction("100f034c")),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr15, 0xfffffff0u, TRTest::BinaryInstruction("11f0034c")),
    std::make_tuple(TR::InstOpCode::vspltish, TR::RealRegister::vr16, 0xffffffffu, TRTest::BinaryInstruction("121f034c")),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr31, 0x00000000u, TRTest::BinaryInstruction("13e0038c")),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr0,  0x0000000fu, TRTest::BinaryInstruction("100f038c")),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr15, 0xfffffff0u, TRTest::BinaryInstruction("11f0038c")),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::RealRegister::vr16, 0xffffffffu, TRTest::BinaryInstruction("121f038c"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, TRTest::BinaryInstruction("1020120c")),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr31, TR::RealRegister::vr0,   5u, TRTest::BinaryInstruction("13e5020c")),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr0,  TR::RealRegister::vr31, 15u, TRTest::BinaryInstruction("100ffa0c")),
    std::make_tuple(TR::InstOpCode::vspltb, TR::RealRegister::vr15, TR::RealRegister::vr16,  7u, TRTest::BinaryInstruction("11e7820c")),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, TRTest::BinaryInstruction("1020124c")),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr31, TR::RealRegister::vr0,   5u, TRTest::BinaryInstruction("13e5024c")),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr0,  TR::RealRegister::vr31,  7u, TRTest::BinaryInstruction("1007fa4c")),
    std::make_tuple(TR::InstOpCode::vsplth, TR::RealRegister::vr15, TR::RealRegister::vr16,  3u, TRTest::BinaryInstruction("11e3824c")),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr1,  TR::RealRegister::vr2,   0u, TRTest::BinaryInstruction("1020128c")),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr31, TR::RealRegister::vr0,   2u, TRTest::BinaryInstruction("13e2028c")),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr0,  TR::RealRegister::vr31,  3u, TRTest::BinaryInstruction("1003fa8c")),
    std::make_tuple(TR::InstOpCode::vspltw, TR::RealRegister::vr15, TR::RealRegister::vr16,  1u, TRTest::BinaryInstruction("11e1828c"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vupkhsb,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0020e")),
    std::make_tuple(TR::InstOpCode::vupkhsb,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa0e")),
    std::make_tuple(TR::InstOpCode::vupkhsh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0024e")),
    std::make_tuple(TR::InstOpCode::vupkhsh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa4e")),
    std::make_tuple(TR::InstOpCode::vupklsb,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0028e")),
    std::make_tuple(TR::InstOpCode::vupklsb,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa8e")),
    std::make_tuple(TR::InstOpCode::vupklsh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e002ce")),
    std::make_tuple(TR::InstOpCode::vupklsh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000face")),
    std::make_tuple(TR::InstOpCode::vclzlsbb, TR::RealRegister::gr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("10000602")),
    std::make_tuple(TR::InstOpCode::vclzlsbb, TR::RealRegister::gr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00602")),
    std::make_tuple(TR::InstOpCode::vclzlsbb, TR::RealRegister::gr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fe02")),
    std::make_tuple(TR::InstOpCode::vclzlsbb, TR::RealRegister::gr31, TR::RealRegister::vr31, TRTest::BinaryInstruction("13e0fe02")),
    std::make_tuple(TR::InstOpCode::vctzlsbb, TR::RealRegister::gr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("10010602")),
    std::make_tuple(TR::InstOpCode::vctzlsbb, TR::RealRegister::gr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e10602")),
    std::make_tuple(TR::InstOpCode::vctzlsbb, TR::RealRegister::gr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1001fe02")),
    std::make_tuple(TR::InstOpCode::vctzlsbb, TR::RealRegister::gr31, TR::RealRegister::vr31, TRTest::BinaryInstruction("13e1fe02")),
    std::make_tuple(TR::InstOpCode::vnegw,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e60602")),
    std::make_tuple(TR::InstOpCode::vnegw,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1006fe02")),
    std::make_tuple(TR::InstOpCode::vnegd,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e70602")),
    std::make_tuple(TR::InstOpCode::vnegd,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1007fe02"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00300")),
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0300")),
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb00")),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00340")),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0340")),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb40")),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00380")),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0380")),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb80")),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00000")),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0000")),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f800")),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00200")),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0200")),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa00")),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e000c0")),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f00c0")),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f8c0")),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00040")),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0040")),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f840")),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00240")),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0240")),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa40")),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00080")),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0080")),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f880")),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00280")),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0280")),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa80")),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00404")),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0404")),
    std::make_tuple(TR::InstOpCode::vand,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc04")),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00444")),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0444")),
    std::make_tuple(TR::InstOpCode::vandc,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc44")),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00006")),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0006")),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f806")),
    std::make_tuple(TR::InstOpCode::vcmpneb,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00007")),
    std::make_tuple(TR::InstOpCode::vcmpneb,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0007")),
    std::make_tuple(TR::InstOpCode::vcmpneb,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f807")),
    std::make_tuple(TR::InstOpCode::vcmpneh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00047")),
    std::make_tuple(TR::InstOpCode::vcmpneh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0047")),
    std::make_tuple(TR::InstOpCode::vcmpneh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f847")),
    std::make_tuple(TR::InstOpCode::vcmpnew,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00087")),
    std::make_tuple(TR::InstOpCode::vcmpnew,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0087")),
    std::make_tuple(TR::InstOpCode::vcmpnew,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f887")),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00046")),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0046")),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f846")),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00086")),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0086")),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f886")),
    std::make_tuple(TR::InstOpCode::vcmpequd, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e000c7")),
    std::make_tuple(TR::InstOpCode::vcmpequd, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f00c7")),
    std::make_tuple(TR::InstOpCode::vcmpequd, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f8c7")),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00306")),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0306")),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb06")),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00346")),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0346")),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb46")),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00386")),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0386")),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb86")),
    std::make_tuple(TR::InstOpCode::vcmpgtsd, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e003c7")),
    std::make_tuple(TR::InstOpCode::vcmpgtsd, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f03c7")),
    std::make_tuple(TR::InstOpCode::vcmpgtsd, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fbc7")),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00206")),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0206")),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa06")),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00246")),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0246")),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa46")),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00286")),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0286")),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa86")),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00102")),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0102")),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f902")),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00142")),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0142")),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f942")),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00182")),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0182")),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f982")),
    std::make_tuple(TR::InstOpCode::vmaxsd,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e001c2")),
    std::make_tuple(TR::InstOpCode::vmaxsd,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f01c2")),
    std::make_tuple(TR::InstOpCode::vmaxsd,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f9c2")),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00002")),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0002")),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f802")),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00042")),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0042")),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f842")),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00082")),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0082")),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f882")),
    std::make_tuple(TR::InstOpCode::vmaxfp,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0040a")),
    std::make_tuple(TR::InstOpCode::vmaxfp,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f040a")),
    std::make_tuple(TR::InstOpCode::vmaxfp,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc0a")),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00302")),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0302")),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb02")),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00342")),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0342")),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb42")),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00382")),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0382")),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb82")),
    std::make_tuple(TR::InstOpCode::vminsd,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e003c2")),
    std::make_tuple(TR::InstOpCode::vminsd,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f03c2")),
    std::make_tuple(TR::InstOpCode::vminsd,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fbc2")),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00202")),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0202")),
    std::make_tuple(TR::InstOpCode::vminub,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa02")),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00242")),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0242")),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa42")),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00282")),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0282")),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa82")),
    std::make_tuple(TR::InstOpCode::vminfp,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0044a")),
    std::make_tuple(TR::InstOpCode::vminfp,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f044a")),
    std::make_tuple(TR::InstOpCode::vminfp,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc4a")),
    std::make_tuple(TR::InstOpCode::vmrghb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0000c")),
    std::make_tuple(TR::InstOpCode::vmrghb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f000c")),
    std::make_tuple(TR::InstOpCode::vmrghb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f80c")),
    std::make_tuple(TR::InstOpCode::vmrglb,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0010c")),
    std::make_tuple(TR::InstOpCode::vmrglb,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f010c")),
    std::make_tuple(TR::InstOpCode::vmrglb,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f90c")),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00348")),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0348")),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb48")),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00148")),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0148")),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f948")),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00048")),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0048")),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f848")),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00089")),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0089")),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f889")),
    std::make_tuple(TR::InstOpCode::vmuleub,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00208")),
    std::make_tuple(TR::InstOpCode::vmuleub,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0208")),
    std::make_tuple(TR::InstOpCode::vmuleub,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa08")),
    std::make_tuple(TR::InstOpCode::vmuloub,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00008")),
    std::make_tuple(TR::InstOpCode::vmuloub,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0008")),
    std::make_tuple(TR::InstOpCode::vmuloub,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f808")),
    std::make_tuple(TR::InstOpCode::vmuleuw,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00288")),
    std::make_tuple(TR::InstOpCode::vmuleuw,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0288")),
    std::make_tuple(TR::InstOpCode::vmuleuw,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa88")),
    std::make_tuple(TR::InstOpCode::vmulouw,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00088")),
    std::make_tuple(TR::InstOpCode::vmulouw,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0088")),
    std::make_tuple(TR::InstOpCode::vmulouw,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f888")),
    std::make_tuple(TR::InstOpCode::vmulld,   TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e001c9")),
    std::make_tuple(TR::InstOpCode::vmulld,   TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f01c9")),
    std::make_tuple(TR::InstOpCode::vmulld,   TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f9c9")),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00504")),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0504")),
    std::make_tuple(TR::InstOpCode::vnor,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fd04")),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00484")),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0484")),
    std::make_tuple(TR::InstOpCode::vor,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc84")),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00004")),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0004")),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f804")),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00044")),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0044")),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f844")),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00084")),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0084")),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f884")),
    std::make_tuple(TR::InstOpCode::vrld,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e000C4")),
    std::make_tuple(TR::InstOpCode::vrld,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f00C4")),
    std::make_tuple(TR::InstOpCode::vrld,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f8C4")),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e001c4")),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f01c4")),
    std::make_tuple(TR::InstOpCode::vsl,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f9c4")),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00104")),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0104")),
    std::make_tuple(TR::InstOpCode::vslb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f904")),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00144")),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0144")),
    std::make_tuple(TR::InstOpCode::vslh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f944")),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0040c")),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f040c")),
    std::make_tuple(TR::InstOpCode::vslo,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc0c")),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00184")),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0184")),
    std::make_tuple(TR::InstOpCode::vslw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000f984")),
    std::make_tuple(TR::InstOpCode::vsld,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e005c4")),
    std::make_tuple(TR::InstOpCode::vsld,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f05c4")),
    std::make_tuple(TR::InstOpCode::vsld,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fdc4")),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e002c4")),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f02c4")),
    std::make_tuple(TR::InstOpCode::vsr,      TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fac4")),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00304")),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0304")),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb04")),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00344")),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0344")),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb44")),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00384")),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0384")),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fb84")),
    std::make_tuple(TR::InstOpCode::vsrad,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e003c4")),
    std::make_tuple(TR::InstOpCode::vsrad,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f03c4")),
    std::make_tuple(TR::InstOpCode::vsrad,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fbc4")),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00204")),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0204")),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa04")),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00244")),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0244")),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa44")),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0044c")),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f044c")),
    std::make_tuple(TR::InstOpCode::vsro,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc4c")),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00284")),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0284")),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fa84")),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00700")),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0700")),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000ff00")),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00740")),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0740")),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000ff40")),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00780")),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0780")),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000ff80")),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00400")),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0400")),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc00")),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00600")),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0600")),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fe00")),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e004c0")),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f04c0")),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fcc0")),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00440")),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0440")),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc40")),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00640")),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0640")),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fe40")),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00480")),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0480")),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fc80")),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00680")),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0680")),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fe80")),
    std::make_tuple(TR::InstOpCode::vsum4sbs, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00708")),
    std::make_tuple(TR::InstOpCode::vsum4sbs, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0708")),
    std::make_tuple(TR::InstOpCode::vsum4sbs, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000ff08")),
    std::make_tuple(TR::InstOpCode::vsum4shs, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00648")),
    std::make_tuple(TR::InstOpCode::vsum4shs, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0648")),
    std::make_tuple(TR::InstOpCode::vsum4shs, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fe48")),
    std::make_tuple(TR::InstOpCode::vsumsws,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00788")),
    std::make_tuple(TR::InstOpCode::vsumsws,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0788")),
    std::make_tuple(TR::InstOpCode::vsumsws,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000ff88")),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e004c4")),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f04c4")),
    std::make_tuple(TR::InstOpCode::vxor,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("1000fcc4"))
)));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,   0, TRTest::BinaryInstruction("13e0002c")),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,   0, TRTest::BinaryInstruction("101f002c")),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31,  0, TRTest::BinaryInstruction("1000f82c")),
    std::make_tuple(TR::InstOpCode::vsldoi, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  15, TRTest::BinaryInstruction("100003ec"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00026")),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0026")),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("1000f826")),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("100007e6")),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0002b")),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f002b")),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("1000f82b")),
    std::make_tuple(TR::InstOpCode::vperm,    TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("100007eb")),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e0002a")),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f002a")),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("1000f82a")),
    std::make_tuple(TR::InstOpCode::vsel,     TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("100007ea")),
    std::make_tuple(TR::InstOpCode::vmladduhm,TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("13e00022")),
    std::make_tuple(TR::InstOpCode::vmladduhm,TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TR::RealRegister::vr0,  TRTest::BinaryInstruction("101f0022")),
    std::make_tuple(TR::InstOpCode::vmladduhm,TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TR::RealRegister::vr0,  TRTest::BinaryInstruction("1000f822")),
    std::make_tuple(TR::InstOpCode::vmladduhm,TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr0,  TR::RealRegister::vr31, TRTest::BinaryInstruction("100007e2"))
));

INSTANTIATE_TEST_CASE_P(VMX, PPCRecordFormSanityTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::InstOpCode::Mnemonic, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::vaddsbs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddshs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddsws,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddubm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddubs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vaddudm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduhm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduhs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduwm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vadduws,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vand,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vandc,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vcmpequb, TR::InstOpCode::vcmpequb_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpneb,  TR::InstOpCode::vcmpneb_r,  TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpneh,  TR::InstOpCode::vcmpneh_r,  TRTest::BinaryInstruction("0000000f")),
    std::make_tuple(TR::InstOpCode::vcmpnew,  TR::InstOpCode::vcmpnew_r,  TRTest::BinaryInstruction("0000000f")),
    std::make_tuple(TR::InstOpCode::vcmpequh, TR::InstOpCode::vcmpequh_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpequw, TR::InstOpCode::vcmpequw_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpequd, TR::InstOpCode::vcmpequd_r, TRTest::BinaryInstruction("0000000f")),
    std::make_tuple(TR::InstOpCode::vcmpgtsb, TR::InstOpCode::vcmpgtsb_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpgtsh, TR::InstOpCode::vcmpgtsh_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpgtsw, TR::InstOpCode::vcmpgtsw_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpgtsd, TR::InstOpCode::vcmpgtsd_r, TRTest::BinaryInstruction("0000000f")),
    std::make_tuple(TR::InstOpCode::vcmpgtub, TR::InstOpCode::vcmpgtub_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpgtuh, TR::InstOpCode::vcmpgtuh_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vcmpgtuw, TR::InstOpCode::vcmpgtuw_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::vmaxsb,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxsh,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxsw,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxsd,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxub,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxuh,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxuw,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmaxfp,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsb,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsh,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsw,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminsd,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminub,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminuh,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminuw,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vminfp,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmsumuhm, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulesh,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulosh,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulouh,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmuluwm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmladduhm,TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmuleub,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmuloub,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmuleuw,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulouw,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vmulld,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vnor,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vor,      TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vperm,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlb,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlh,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrlw,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vrld,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsel,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsl,      TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslb,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsldoi,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslh,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslo,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vslw,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsld,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltb,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsplth,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltisb, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltish, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltisw, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vspltw,   TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsr,      TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrab,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrah,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsraw,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrad,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrb,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrh,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsro,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsrw,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubsbs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubshs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubsws,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsububm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsububs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubudm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuhm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuhs,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuwm,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsubuws,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsum4sbs, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsum4shs, TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vsumsws,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupkhsb,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupkhsh,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupklsb,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vupklsh,  TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vxor,     TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vnegw,    TR::InstOpCode::bad,        TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::vnegd,    TR::InstOpCode::bad,        TRTest::BinaryInstruction())
)));

INSTANTIATE_TEST_CASE_P(VSXScalarFixed, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr31,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0066")),
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr0,   TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00067")),
    std::make_tuple(TR::InstOpCode::mfvsrd,   TR::RealRegister::gr0,   TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00066")),
    std::make_tuple(TR::InstOpCode::mfvsrld,  TR::RealRegister::gr31,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0266")),
    std::make_tuple(TR::InstOpCode::mfvsrld,  TR::RealRegister::gr0,   TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00267")),
    std::make_tuple(TR::InstOpCode::mfvsrld,  TR::RealRegister::gr0,   TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00266")),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr31,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f00e6")),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr0,   TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe000e7")),
    std::make_tuple(TR::InstOpCode::mfvsrwz,  TR::RealRegister::gr0,   TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe000e6")),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr63, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe00167")),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr31, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe00166")),
    std::make_tuple(TR::InstOpCode::mtvsrd,   TR::RealRegister::vsr0,  TR::RealRegister::gr31,  TRTest::BinaryInstruction("7c1f0166")),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr63, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe001a7")),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr31, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe001a6")),
    std::make_tuple(TR::InstOpCode::mtvsrwa,  TR::RealRegister::vsr0,  TR::RealRegister::gr31,  TRTest::BinaryInstruction("7c1f01a6")),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr63, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe001e7")),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr31, TR::RealRegister::gr0,   TRTest::BinaryInstruction("7fe001e6")),
    std::make_tuple(TR::InstOpCode::mtvsrwz,  TR::RealRegister::vsr0,  TR::RealRegister::gr31,  TRTest::BinaryInstruction("7c1f01e6"))
));

INSTANTIATE_TEST_CASE_P(VSXScalarFixed, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::mfvsrd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfvsrld, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mfvsrwz, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrwz, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::mtvsrwa, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXScalarConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00425")),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00424")),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fc26")),
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fc24")),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0042d")),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0042c")),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fc2e")),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fc2c")),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00525")),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00524")),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fd26")),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fd24")),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0052d")),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0052c")),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fd2e")),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fd2c"))
));

INSTANTIATE_TEST_CASE_P(VSXScalarConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xscvdpsp,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvdpspn, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvspdp,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xscvspdpn, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007e1")),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007e0")),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ffe2")),
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ffe0")),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006e1")),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006e0")),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fee2")),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fee0"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvcvsxddp, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvcvsxdsp, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr1,  TR::RealRegister::vsr2,  0u, TRTest::BinaryInstruction("f0201290")),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  2u, TRTest::BinaryInstruction("f3e20291")),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 3u, TRTest::BinaryInstruction("f003fa92")),
    std::make_tuple(TR::InstOpCode::xxspltw, TR::RealRegister::vsr33, TR::RealRegister::vsr34, 1u, TRTest::BinaryInstruction("f0211293"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00411")),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00410")),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0414")),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0410")),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fc12")),
    std::make_tuple(TR::InstOpCode::xxland,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fc10")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00451")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00450")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0454")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0450")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fc52")),
    std::make_tuple(TR::InstOpCode::xxlandc, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fc50")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e005d1")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e005d0")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f05d4")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f05d0")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fdd2")),
    std::make_tuple(TR::InstOpCode::xxleqv,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fdd0")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00591")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00590")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0594")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0590")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fd92")),
    std::make_tuple(TR::InstOpCode::xxlnand, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fd90")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00511")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00510")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0514")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0510")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fd12")),
    std::make_tuple(TR::InstOpCode::xxlnor,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fd10")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00491")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00490")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0494")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0490")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fc92")),
    std::make_tuple(TR::InstOpCode::xxlor,   TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fc90")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00551")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00550")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0554")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0550")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fd52")),
    std::make_tuple(TR::InstOpCode::xxlorc,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fd50")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00091")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00090")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0094")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0090")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000f892")),
    std::make_tuple(TR::InstOpCode::xxmrghw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000f890")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00191")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00190")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0194")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0190")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000f992")),
    std::make_tuple(TR::InstOpCode::xxmrglw, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000f990"))
)));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x0u, TRTest::BinaryInstruction("f3e00051")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x0u, TRTest::BinaryInstruction("f3e00050")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  0x0u, TRTest::BinaryInstruction("f01f0054")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  0x0u, TRTest::BinaryInstruction("f01f0050")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, 0x0u, TRTest::BinaryInstruction("f000f852")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, 0x0u, TRTest::BinaryInstruction("f000f850")),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  0x3u, TRTest::BinaryInstruction("f0000350")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     0, TRTest::BinaryInstruction("f3e00011")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     0, TRTest::BinaryInstruction("f3e00010")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,     0, TRTest::BinaryInstruction("f01f0014")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,     0, TRTest::BinaryInstruction("f01f0010")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63,    0, TRTest::BinaryInstruction("f000f812")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31,    0, TRTest::BinaryInstruction("f000f810")),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,     3, TRTest::BinaryInstruction("f0000310"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00030")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00031")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0030")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0034")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f000f830")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f000f832")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f00007f0")),
    std::make_tuple(TR::InstOpCode::xxsel, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f00007f8"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCTrg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxspltib, TR::RealRegister::vsr31, 0x00000000u, TRTest::BinaryInstruction("f3e002d0")),
    std::make_tuple(TR::InstOpCode::xxspltib, TR::RealRegister::vsr0,  0x0000000fu, TRTest::BinaryInstruction("f0007ad0")),
    std::make_tuple(TR::InstOpCode::xxspltib, TR::RealRegister::vsr15, 0xfffffff0u, TRTest::BinaryInstruction("f1e782d0")),
    std::make_tuple(TR::InstOpCode::xxspltib, TR::RealRegister::vsr16, 0xffffffffu, TRTest::BinaryInstruction("f207fad0"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorFixed, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xxland,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlandc,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxleqv,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlnand,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlnor,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlor,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxlorc,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxmrghw,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxmrglw,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxpermdi, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxsel,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxsldwi,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxspltw,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xxspltib, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007e5")),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007e4")),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ffe6")),
    std::make_tuple(TR::InstOpCode::xvnegdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ffe4")),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006e5")),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006e4")),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fee6")),
    std::make_tuple(TR::InstOpCode::xvnegsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fee4")),
    std::make_tuple(TR::InstOpCode::xvsqrtsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0022d")),
    std::make_tuple(TR::InstOpCode::xvsqrtsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0022c")),
    std::make_tuple(TR::InstOpCode::xvsqrtsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa2e")),
    std::make_tuple(TR::InstOpCode::xvsqrtsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa2c")),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0032d")),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e0032c")),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb2e")),
    std::make_tuple(TR::InstOpCode::xvsqrtdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb2c")),
    std::make_tuple(TR::InstOpCode::xvabssp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00665")),
    std::make_tuple(TR::InstOpCode::xvabssp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00664")),
    std::make_tuple(TR::InstOpCode::xvabssp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe66")),
    std::make_tuple(TR::InstOpCode::xvabssp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe64")),
    std::make_tuple(TR::InstOpCode::xvabsdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00765")),
    std::make_tuple(TR::InstOpCode::xvabsdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00764")),
    std::make_tuple(TR::InstOpCode::xvabsdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff66")),
    std::make_tuple(TR::InstOpCode::xvabsdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff64"))
));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCTrg1Src2EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00301")),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00300")),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0304")),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0300")),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb02")),
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb00")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00201")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00200")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0204")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0200")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa02")),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa00")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00319")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00318")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f031c")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0318")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb1a")),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb18")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00219")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00218")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f021c")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0218")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa1a")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa18")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00399")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00398")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f039c")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0398")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb9a")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb98")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00299")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00298")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f029c")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0298")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa9a")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa98")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00359")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00358")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f035c")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0358")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb5a")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb58")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00259")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00258")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f025c")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0258")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa5a")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa58")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e003c1")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e003c0")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f03c4")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f03c0")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fbc2")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fbc0")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e002c1")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e002c0")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f02c4")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f02c0")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fac2")),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fac0")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00309")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00308")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f030c")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0308")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb0a")),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb08")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00209")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00208")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f020c")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0208")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa0a")),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa08")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00349")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00348")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f034c")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0348")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb4a")),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb48")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00249")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00248")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f024c")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0248")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa4a")),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa48")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00701")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00700")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0704")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0700")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff02")),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff00")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00601")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00600")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0604")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0600")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe02")),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe00")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00741")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00740")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0744")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0740")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff42")),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff40")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00641")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00640")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0644")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0640")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe42")),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe40")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00389")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00388")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f038c")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0388")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb8a")),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb88")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00289")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00288")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f028c")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0288")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa8a")),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa88")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e003c9")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e003c8")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f03cc")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f03c8")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fbca")),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fbc8")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e002c9")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e002c8")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f02cc")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f02c8")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000faca")),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fac8")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00381")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00380")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0384")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0380")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fb82")),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fb80")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00281")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00280")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0284")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0280")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa82")),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa80")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00709")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00708")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f070c")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0708")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff0a")),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff08")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00609")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00608")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f060c")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0608")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe0a")),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe08")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00749")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00748")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f074c")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0748")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff4a")),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff48")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00649")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00648")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f064c")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0648")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe4a")),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe48")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00789")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00788")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f078c")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0788")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ff8a")),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ff88")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00689")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00688")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f068c")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0688")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fe8a")),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fe88")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007c9")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e007c8")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f07cc")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f07c8")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000ffca")),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000ffc8")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006c9")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e006c8")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f06cc")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f06c8")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000feca")),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fec8")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00241")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00240")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0244")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0240")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa42")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa40")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00241")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f3e00240")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0244")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f01f0240")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr63, TRTest::BinaryInstruction("f000fa42")),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::RealRegister::vsr0,  TR::RealRegister::vsr0,  TR::RealRegister::vsr31, TRTest::BinaryInstruction("f000fa40"))
)));

INSTANTIATE_TEST_CASE_P(VSXVectorFloat, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::xvadddp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvaddsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvcmpeqdp,  TR::InstOpCode::xvcmpeqdp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvcmpeqsp,  TR::InstOpCode::xvcmpeqsp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvcmpgedp,  TR::InstOpCode::xvcmpgedp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvcmpgesp,  TR::InstOpCode::xvcmpgesp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvcmpgtdp,  TR::InstOpCode::xvcmpgtdp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvcmpgtsp,  TR::InstOpCode::xvcmpgtsp_r, TRTest::BinaryInstruction("00000400")),
    std::make_tuple(TR::InstOpCode::xvdivdp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvdivsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddadp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddasp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddmdp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaddmsp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaxdp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmaxsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmindp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvminsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubadp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubasp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubmdp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmsubmsp,  TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmuldp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvmulsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnegdp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnegsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddadp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddasp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddmdp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmaddmsp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubadp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubasp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubmdp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvnmsubmsp, TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsqrtsp,   TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsqrtdp,   TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvabssp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvabsdp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsubdp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::xvsubsp,    TR::InstOpCode::bad,         TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fabs,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00210")),
    std::make_tuple(TR::InstOpCode::fabs,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fa10")),
    std::make_tuple(TR::InstOpCode::fmr,    TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00090")),
    std::make_tuple(TR::InstOpCode::fmr,    TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f890")),
    std::make_tuple(TR::InstOpCode::fnabs,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00110")),
    std::make_tuple(TR::InstOpCode::fnabs,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f910")),
    std::make_tuple(TR::InstOpCode::fneg,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00050")),
    std::make_tuple(TR::InstOpCode::fneg,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f850")),
    std::make_tuple(TR::InstOpCode::fsqrt,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0002c")),
    std::make_tuple(TR::InstOpCode::fsqrt,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f82c")),
    std::make_tuple(TR::InstOpCode::fsqrts, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0002c")),
    std::make_tuple(TR::InstOpCode::fsqrts, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f82c"))
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0002a")),
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f002a")),
    std::make_tuple(TR::InstOpCode::fadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f82a")),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0002a")),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f002a")),
    std::make_tuple(TR::InstOpCode::fadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f82a")),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00024")),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0024")),
    std::make_tuple(TR::InstOpCode::fdiv,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f824")),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00024")),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f0024")),
    std::make_tuple(TR::InstOpCode::fdivs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f824")),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00032")),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0032")),
    std::make_tuple(TR::InstOpCode::fmul,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc0007f2")),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00032")),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f0032")),
    std::make_tuple(TR::InstOpCode::fmuls, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec0007f2")),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00028")),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0028")),
    std::make_tuple(TR::InstOpCode::fsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f828")),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00028")),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f0028")),
    std::make_tuple(TR::InstOpCode::fsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f828"))
));

INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCTrg1Src3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0003a")),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f003a")),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc0007fa")),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f83a")),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0003a")),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f003a")),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec0007fa")),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f83a")),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00038")),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f0038")),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc0007f8")),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f838")),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00038")),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f0038")),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec0007f8")),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f838")),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0003e")),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f003e")),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc0007fe")),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f83e")),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0003e")),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f003e")),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec0007fe")),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f83e")),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0003c")),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f003c")),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc0007fc")),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f83c")),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0003c")),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec1f003c")),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ec0007fc")),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00f83c")),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0002e")),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc1f002e")),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("fc0007ee")),
    std::make_tuple(TR::InstOpCode::fsel,    TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f82e"))
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatArithmetic, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fabs,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fadd,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fadds,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fdiv,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fdivs,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmadd,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmadds,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmr,     TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmsub,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmsubs,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmul,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fmuls,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnabs,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fneg,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmadd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmadds, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmsub,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fnmsubs, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsel,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsqrt,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsqrts,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsub,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fsubs,   TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatConvert, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fcfid,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0069c")),
    std::make_tuple(TR::InstOpCode::fcfid,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fe9c")),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0069c")),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fe9c")),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0079c")),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00ff9c")),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe0079c")),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00ff9c")),
    std::make_tuple(TR::InstOpCode::fctid,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0065c")),
    std::make_tuple(TR::InstOpCode::fctid,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fe5c")),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0065e")),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fe5e")),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0001c")),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f81c")),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe0001e")),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f81e"))
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatConvert, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fcfid,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfids,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfidu,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fcfidus, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctid,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctidz,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctiw,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::fctiwz,  TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(FloatRound, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::frim, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe003d0")),
    std::make_tuple(TR::InstOpCode::frim, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fbd0")),
    std::make_tuple(TR::InstOpCode::frin, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00310")),
    std::make_tuple(TR::InstOpCode::frin, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fb10")),
    std::make_tuple(TR::InstOpCode::frip, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00390")),
    std::make_tuple(TR::InstOpCode::frip, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00fb90")),
    std::make_tuple(TR::InstOpCode::frsp, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffe00018")),
    std::make_tuple(TR::InstOpCode::frsp, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("fc00f818"))
));

// NOTE: Many of these opcodes *do* have record form variants in the Power ISA, but we have not yet
// made use of them. These tests should be updated when/if we do add these instructions.
INSTANTIATE_TEST_CASE_P(FloatRound, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::frim, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frin, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frip, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::frsp, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x3fu, TRTest::BinaryInstruction("ec1ffd84")),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x3fu, TRTest::BinaryInstruction("ef80fd84")),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x00u, TRTest::BinaryInstruction("ec1f0184")),
    std::make_tuple(TR::InstOpCode::dtstdc, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x00u, TRTest::BinaryInstruction("ef800184")),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x3fu, TRTest::BinaryInstruction("ec1ffdc4")),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x3fu, TRTest::BinaryInstruction("ef80fdc4")),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr0, TR::RealRegister::fp31, 0x00u, TRTest::BinaryInstruction("ec1f01c4")),
    std::make_tuple(TR::InstOpCode::dtstdg, TR::RealRegister::cr7, TR::RealRegister::fp0,  0x00u, TRTest::BinaryInstruction("ef8001c4"))
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcffix,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00644")),
    std::make_tuple(TR::InstOpCode::dcffix,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fe44")),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::RealRegister::fp30, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffc00644")),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::RealRegister::fp0,  TR::RealRegister::fp30, TRTest::BinaryInstruction("fc00f644")),
    std::make_tuple(TR::InstOpCode::dctfix,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00244")),
    std::make_tuple(TR::InstOpCode::dctfix,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fa44")),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00284")),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fa84")),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe00684")),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fe84")),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::RealRegister::fp30, TR::RealRegister::fp0,  TRTest::BinaryInstruction("ffc00604")),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::RealRegister::fp0,  TR::RealRegister::fp30, TRTest::BinaryInstruction("fc00f604")),
    std::make_tuple(TR::InstOpCode::dxex,    TR::RealRegister::fp31, TR::RealRegister::fp0,  TRTest::BinaryInstruction("efe002c4")),
    std::make_tuple(TR::InstOpCode::dxex,    TR::RealRegister::fp0,  TR::RealRegister::fp31, TRTest::BinaryInstruction("ec00fac4"))
));

INSTANTIATE_TEST_CASE_P(DFP, PPCTrg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x0u, TRTest::BinaryInstruction("efe00006")),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0x0u, TRTest::BinaryInstruction("ec1f0006")),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0x0u, TRTest::BinaryInstruction("ec00f806")),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x3u, TRTest::BinaryInstruction("ec000606")),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x2u, TRTest::BinaryInstruction("ec000406")),
    std::make_tuple(TR::InstOpCode::dqua,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x1u, TRTest::BinaryInstruction("ec000206")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp31, TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x0u, TRTest::BinaryInstruction("efe00046")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp31, TR::RealRegister::fp0,  0x0u, TRTest::BinaryInstruction("ec1f0046")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp31, 0x0u, TRTest::BinaryInstruction("ec00f846")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x3u, TRTest::BinaryInstruction("ec000646")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x2u, TRTest::BinaryInstruction("ec000446")),
    std::make_tuple(TR::InstOpCode::drrnd, TR::RealRegister::fp0,  TR::RealRegister::fp0,  TR::RealRegister::fp0,  0x1u, TRTest::BinaryInstruction("ec000246"))
));

INSTANTIATE_TEST_CASE_P(DFP, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcffix,  TR::InstOpCode::dcffix_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::dcffixq, TR::InstOpCode::dcffixq_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::ddedpd,  TR::InstOpCode::ddedpd_r,  TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::denbcdu, TR::InstOpCode::denbcdu_r, TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::dqua,    TR::InstOpCode::dqua_r,    TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::drdpq,   TR::InstOpCode::drdpq_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::drrnd,   TR::InstOpCode::drrnd_r,   TRTest::BinaryInstruction("00000001")),
    std::make_tuple(TR::InstOpCode::dtstdc,  TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dtstdg,  TR::InstOpCode::bad,       TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dxex,    TR::InstOpCode::dxex_r,    TRTest::BinaryInstruction("00000001"))
));

INSTANTIATE_TEST_CASE_P(TM, PPCDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::tabort_r,   TRTest::BinaryInstruction("7c00071d"), false),
    std::make_tuple(TR::InstOpCode::tbegin_r,   TRTest::BinaryInstruction("7c00051d"), false),
    std::make_tuple(TR::InstOpCode::tbeginro_r, TRTest::BinaryInstruction("7c20051d"), false),
    std::make_tuple(TR::InstOpCode::tend_r,     TRTest::BinaryInstruction("7c00055d"), false)
));

INSTANTIATE_TEST_CASE_P(TM, PPCSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c9f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c807edd")),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c9f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdeqi_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c80fedd")),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7d9f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7d807edd")),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7d9f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdgei_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7d80fedd")),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7d1f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7d007edd")),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7d1f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdgti_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7d00fedd")),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7e9f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7e807edd")),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7e9f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdlei_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7e80fedd")),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7cbf06dd")),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7ca07edd")),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7cbf86dd")),
    std::make_tuple(TR::InstOpCode::tabortdlgei_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7ca0fedd")),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c3f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c207edd")),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c3f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdlgti_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c20fedd")),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7cdf06dd")),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7cc07edd")),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7cdf86dd")),
    std::make_tuple(TR::InstOpCode::tabortdllei_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7cc0fedd")),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c5f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c407edd")),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c5f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdllti_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c40fedd")),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7e1f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7e007edd")),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7e1f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdlti_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7e00fedd")),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7f1f06dd")),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7f007edd")),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7f1f86dd")),
    std::make_tuple(TR::InstOpCode::tabortdneqi_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7f00fedd")),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c9f069d")),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c807e9d")),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c9f869d")),
    std::make_tuple(TR::InstOpCode::tabortweqi_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c80fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7d9f069d")),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7d807e9d")),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7d9f869d")),
    std::make_tuple(TR::InstOpCode::tabortwgei_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7d80fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7d1f069d")),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7d007e9d")),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7d1f869d")),
    std::make_tuple(TR::InstOpCode::tabortwgti_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7d00fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7e9f069d")),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7e807e9d")),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7e9f869d")),
    std::make_tuple(TR::InstOpCode::tabortwlei_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7e80fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7cbf069d")),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7ca07e9d")),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7cbf869d")),
    std::make_tuple(TR::InstOpCode::tabortwlgei_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7ca0fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c3f069d")),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c207e9d")),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c3f869d")),
    std::make_tuple(TR::InstOpCode::tabortwlgti_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c20fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7cdf069d")),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7cc07e9d")),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7cdf869d")),
    std::make_tuple(TR::InstOpCode::tabortwllei_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7cc0fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7c5f069d")),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7c407e9d")),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7c5f869d")),
    std::make_tuple(TR::InstOpCode::tabortwllti_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7c40fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7e1f069d")),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7e007e9d")),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7e1f869d")),
    std::make_tuple(TR::InstOpCode::tabortwlti_r,  TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7e00fe9d")),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr31, 0x00000000u, TRTest::BinaryInstruction("7f1f069d")),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr0,  0x0000000fu, TRTest::BinaryInstruction("7f007e9d")),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr31, 0xfffffff0u, TRTest::BinaryInstruction("7f1f869d")),
    std::make_tuple(TR::InstOpCode::tabortwneqi_r, TR::RealRegister::gr0,  0xffffffffu, TRTest::BinaryInstruction("7f00fe9d"))
)));

INSTANTIATE_TEST_CASE_P(TM, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabort_r,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdeqi_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdgei_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdgti_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlei_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlgei_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlgti_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdllei_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdllti_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdlti_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortdneqi_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortweqi_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwgei_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwgti_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlei_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlgei_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlgti_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwllei_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwllti_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwlti_r,  TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tabortwneqi_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tbegin_r,      TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tbeginro_r,    TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad, TR::InstOpCode::tend_r,        TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(Prefetch, PPCMemEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00022c")),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fa2c")),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f022c")),
    std::make_tuple(TR::InstOpCode::dcbt,    MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), TRTest::BinaryInstruction("7c0f7a2c")),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0001ec")),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f9ec")),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f01ec")),
    std::make_tuple(TR::InstOpCode::dcbtst,  MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), TRTest::BinaryInstruction("7c0f79ec")),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7e0001ec")),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7e00f9ec")),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7e1f01ec")),
    std::make_tuple(TR::InstOpCode::dcbtstt, MemoryReference(TR::RealRegister::gr15, TR::RealRegister::gr15), TRTest::BinaryInstruction("7e0f79ec"))
));

INSTANTIATE_TEST_CASE_P(Prefetch, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::dcbt,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dcbtst,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::dcbtstt, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadPrefix, PPCTrg1MemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("0600000088000000"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("060000008be00000"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000881f0000"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("0602000088000000"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffff8800ffff"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffff8800ffff"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffff88000000"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("060000008800ffff"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("0600000088007fff"), true),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffff88008000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000e4000000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000e7e00000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000e41f0000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000e4000000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffe400ffff"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffe400ffff"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffe4000000"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000e400ffff"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000e4007fff"), true),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffe4008000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000c8000000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000cbe00000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000c81f0000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("06020000c8000000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffffc800ffff"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffffc800ffff"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffffc8000000"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("06000000c800ffff"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("06000000c8007fff"), true),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffffc8008000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000c0000000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000c3e00000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000c01f0000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("06020000c0000000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffffc000ffff"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffffc000ffff"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffffc0000000"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("06000000c000ffff"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("06000000c0007fff"), true),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffffc0008000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000a8000000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000abe00000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000a81f0000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("06020000a8000000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffffa800ffff"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffffa800ffff"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffffa8000000"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("06000000a800ffff"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("06000000a8007fff"), true),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffffa8008000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000a0000000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("06000000a3e00000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000a01f0000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("06020000a0000000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffffa000ffff"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffffa000ffff"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffffa0000000"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("06000000a000ffff"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("06000000a0007fff"), true),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffffa0008000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000e0000000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr30,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000e3c00000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000e01f0000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000e0000000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffe000ffff"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffe000ffff"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffe0000000"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000e000ffff"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000e0007fff"), true),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffe0008000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000a4000000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000a7e00000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000a41f0000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000a4000000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffa400ffff"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffa400ffff"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffa4000000"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000a400ffff"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000a4007fff"), true),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffa4008000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("0600000080000000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("0600000083e00000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("06000000801f0000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("0602000080000000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0601ffff8000ffff"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0603ffff8000ffff"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0601ffff80000000"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("060000008000ffff"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("0600000080007fff"), true),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0603ffff80008000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000a8000000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000abe00000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000a81f0000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000a8000000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffa800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffa800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffa8000000"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000a800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000a8007fff"), true),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffa8008000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000ac000000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000afe00000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000ac1f0000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000ac000000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffac00ffff"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffac00ffff"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffac000000"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000ac00ffff"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000ac007fff"), true),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffac008000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000c8000000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000cbe00000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  0x000000000), TRTest::BinaryInstruction("04000000cfe00000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, 0x000000000), TRTest::BinaryInstruction("04000000c81f0000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x200000000), TRTest::BinaryInstruction("04020000c8000000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TRTest::BinaryInstruction("0401ffffc800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x000000001), TRTest::BinaryInstruction("0403ffffc800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TRTest::BinaryInstruction("0401ffffc8000000"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TRTest::BinaryInstruction("04000000c800ffff"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x000007fff), TRTest::BinaryInstruction("04000000c8007fff"), true),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x000008000), TRTest::BinaryInstruction("0403ffffc8008000"), true)
)));

INSTANTIATE_TEST_CASE_P(LoadPrefix, PPCTrg1MemPCRelativeEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, int64_t, int64_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000088000000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("061000008be00000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("0612000088000000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffff8800ffff")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffff8800ffff")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffff88000000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("061000008800ffff")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("0610000088007fff")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffff88008000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("0610000088008000")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("0610000088007ffe")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("061000008800ffff")),
    std::make_tuple(TR::InstOpCode::plbz,   TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffff8800ffff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000e4000000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000e7e00000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000e4000000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffe400ffff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffe400ffff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffe4000000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000e400ffff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000e4007fff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffe4008000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000e4008000")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000e4007ffe")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000e400ffff")),
    std::make_tuple(TR::InstOpCode::pld,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffe400ffff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000c8000000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000cbe00000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000c8000000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffc800ffff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffc800ffff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffc8000000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000c800ffff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000c8007fff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffc8008000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000c8008000")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000c8007ffe")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000c800ffff")),
    std::make_tuple(TR::InstOpCode::plfd,   TR::RealRegister::fp0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffc800ffff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000c0000000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000c3e00000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000c0000000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffc000ffff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffc000ffff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffc0000000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000c000ffff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000c0007fff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffc0008000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000c0008000")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000c0007ffe")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000c000ffff")),
    std::make_tuple(TR::InstOpCode::plfs,   TR::RealRegister::fp0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffc000ffff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000a8000000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000abe00000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000a8000000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffa8000000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000a800ffff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000a8007fff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffa8008000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000a8008000")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000a8007ffe")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000a800ffff")),
    std::make_tuple(TR::InstOpCode::plha,   TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000a0000000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000a3e00000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000a0000000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffa000ffff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffa000ffff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffa0000000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000a000ffff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000a0007fff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffa0008000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000a0008000")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000a0007ffe")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000a000ffff")),
    std::make_tuple(TR::InstOpCode::plhz,   TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffa000ffff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000e0000000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr30,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000e3c00000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000e0000000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffe000ffff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffe000ffff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffe0000000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000e000ffff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000e0007fff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffe0008000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000e0008000")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000e0007ffe")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000e000ffff")),
    std::make_tuple(TR::InstOpCode::plq,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffe000ffff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000a4000000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000a7e00000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000a4000000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffa400ffff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffa400ffff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffa4000000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000a400ffff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000a4007fff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffa4008000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000a4008000")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000a4007ffe")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000a400ffff")),
    std::make_tuple(TR::InstOpCode::plwa,   TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffa400ffff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000080000000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000083e00000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("0612000080000000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffff8000ffff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffff8000ffff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffff80000000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("061000008000ffff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("0610000080007fff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffff80008000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("0610000080008000")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("0610000080007ffe")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("061000008000ffff")),
    std::make_tuple(TR::InstOpCode::plwz,   TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffff8000ffff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000a8000000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000abe00000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000a8000000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffa8000000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000a800ffff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000a8007fff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffa8008000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000a8008000")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000a8007ffe")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000a800ffff")),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::RealRegister::vr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffa800ffff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000ac000000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000afe00000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000ac000000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffac00ffff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffac00ffff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffac000000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000ac00ffff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000ac007fff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffac008000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000ac008000")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000ac007ffe")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000ac00ffff")),
    std::make_tuple(TR::InstOpCode::plxssp, TR::RealRegister::vr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffac00ffff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000c8000000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr31,  0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000cbe00000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr63,  0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000cfe00000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000c8000000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffc800ffff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffc800ffff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffc8000000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000c800ffff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000c8007fff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,  -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffc8008000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000c8008000")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000c8007ffe")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000c800ffff")),
    std::make_tuple(TR::InstOpCode::plxv,   TR::RealRegister::vsr0,   0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffc800ffff"))
)));

INSTANTIATE_TEST_CASE_P(LoadPrefix, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::plbz,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pld,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plfd,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plfs,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plha,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plhz,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plq,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plwa,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plwz,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plxsd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plxssp, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::plxv,   TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadDisp, PPCTrg1MemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("88000000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("88000000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("8be00000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("8be00000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("881f0000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("881f0000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("88007fff"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("88007fff"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("88008000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("88008000"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("8800ffff"), false),
    std::make_tuple(TR::InstOpCode::lbz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("8800ffff"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("8c010000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("8c010000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("8fe10000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("8fe10000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("8c1f0000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("8c1f0000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("8c017fff"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("8c017fff"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("8c018000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("8c018000"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("8c01ffff"), false),
    std::make_tuple(TR::InstOpCode::lbzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("8c01ffff"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("e8000000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("e8000000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("ebe00000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("ebe00000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7ffc), TRTest::BinaryInstruction("e8007ffc"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7ffc), TRTest::BinaryInstruction("e8007ffc"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("e8008000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("e8008000"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0004), TRTest::BinaryInstruction("e800fffc"), false),
    std::make_tuple(TR::InstOpCode::ld,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0004), TRTest::BinaryInstruction("e800fffc"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("e8010001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("e8010001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("ebe10001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("ebe10001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7ffc), TRTest::BinaryInstruction("e8017ffd"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7ffc), TRTest::BinaryInstruction("e8017ffd"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("e8018001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("e8018001"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0004), TRTest::BinaryInstruction("e801fffd"), false),
    std::make_tuple(TR::InstOpCode::ldu,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0004), TRTest::BinaryInstruction("e801fffd"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c8000000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c8000000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("cbe00000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("cbe00000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c81f0000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c81f0000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("c8007fff"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("c8007fff"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("c8008000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("c8008000"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("c800ffff"), false),
    std::make_tuple(TR::InstOpCode::lfd,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("c800ffff"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("cc010000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("cc010000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("cfe10000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("cfe10000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("cc1f0000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("cc1f0000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("cc017fff"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("cc017fff"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("cc018000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("cc018000"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("cc01ffff"), false),
    std::make_tuple(TR::InstOpCode::lfdu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("cc01ffff"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c0000000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c0000000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c3e00000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("c3e00000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c01f0000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c01f0000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("c0007fff"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("c0007fff"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("c0008000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("c0008000"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("c000ffff"), false),
    std::make_tuple(TR::InstOpCode::lfs,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("c000ffff"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("c4010000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("c4010000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("c7e10000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("c7e10000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c41f0000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("c41f0000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("c4017fff"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("c4017fff"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("c4018000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("c4018000"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("c401ffff"), false),
    std::make_tuple(TR::InstOpCode::lfsu, TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("c401ffff"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a8000000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a8000000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("abe00000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("abe00000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a81f0000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a81f0000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("a8007fff"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("a8007fff"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("a8008000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("a8008000"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("a800ffff"), false),
    std::make_tuple(TR::InstOpCode::lha,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("a800ffff"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("ac010000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("ac010000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("afe10000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("afe10000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("ac1f0000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("ac1f0000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("ac017fff"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("ac017fff"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("ac018000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("ac018000"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("ac01ffff"), false),
    std::make_tuple(TR::InstOpCode::lhau, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("ac01ffff"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a0000000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a0000000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a3e00000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("a3e00000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a01f0000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a01f0000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("a0007fff"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("a0007fff"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("a0008000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("a0008000"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("a000ffff"), false),
    std::make_tuple(TR::InstOpCode::lhz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("a000ffff"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("a4010000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("a4010000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("a7e10000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("a7e10000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a41f0000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("a41f0000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("a4017fff"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("a4017fff"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("a4018000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("a4018000"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("a401ffff"), false),
    std::make_tuple(TR::InstOpCode::lhzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("a401ffff"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("b8200000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("b8200000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("bbe00000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("bbe00000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr30, 0x0000), TRTest::BinaryInstruction("bbfe0000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr30, 0x0000), TRTest::BinaryInstruction("bbfe0000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr29, 0x0000), TRTest::BinaryInstruction("bbfd0000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr29, 0x0000), TRTest::BinaryInstruction("bbfd0000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("b8207fff"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("b8207fff"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("b8208000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("b8208000"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("b820ffff"), false),
    std::make_tuple(TR::InstOpCode::lmw,  TR::RealRegister::gr1,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("b820ffff"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("e8000002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("e8000002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("ebe00002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("ebe00002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("e81f0002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7ffc), TRTest::BinaryInstruction("e8007ffe"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7ffc), TRTest::BinaryInstruction("e8007ffe"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("e8008002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("e8008002"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0004), TRTest::BinaryInstruction("e800fffe"), false),
    std::make_tuple(TR::InstOpCode::lwa,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0004), TRTest::BinaryInstruction("e800fffe"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("80000000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("80000000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("83e00000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("83e00000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("801f0000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("801f0000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("80007fff"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  0x7fff), TRTest::BinaryInstruction("80007fff"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("80008000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("80008000"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("8000ffff"), false),
    std::make_tuple(TR::InstOpCode::lwz,  TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0, -0x0001), TRTest::BinaryInstruction("8000ffff"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("84010000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("84010000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("87e10000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  0x0000), TRTest::BinaryInstruction("87e10000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("841f0000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("841f0000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("84017fff"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  0x7fff), TRTest::BinaryInstruction("84017fff"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("84018000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x8000), TRTest::BinaryInstruction("84018000"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("8401ffff"), false),
    std::make_tuple(TR::InstOpCode::lwzu, TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1, -0x0001), TRTest::BinaryInstruction("8401ffff"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f4000001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f4000001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f7e00001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f7e00001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f7e00009"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  0x0000), TRTest::BinaryInstruction("f7e00009"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("f41f0001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, 0x0000), TRTest::BinaryInstruction("f41f0001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x7ff0), TRTest::BinaryInstruction("f4007ff1"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  0x7ff0), TRTest::BinaryInstruction("f4007ff1"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("f4008001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x8000), TRTest::BinaryInstruction("f4008001"), false),
    std::make_tuple(TR::InstOpCode::lxv,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0, -0x0010), TRTest::BinaryInstruction("f400fff1"), false)
)));

INSTANTIATE_TEST_CASE_P(LoadDisp, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lbz,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lbzu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ld,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldu,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfs,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lha,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhau, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhz,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lmw,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwa,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwz,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzu, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadIndex, PPCTrg1MemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, MemoryReference, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0100ee"), false),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe100ee"), false),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f00ee"), false),
    std::make_tuple(TR::InstOpCode::lbzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01f8ee"), false),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0000ae"), false),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe000ae"), false),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f00ae"), false),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f8ae"), false),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0000a8"), false),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe000a8"), false),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f00a8"), false),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f8a8"), false),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000428"), false),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00428"), false),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0428"), false),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fc28"), false),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c01006a"), false),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe1006a"), false),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f006a"), false),
    std::make_tuple(TR::InstOpCode::ldux,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01f86a"), false),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00002a"), false),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0002a"), false),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f002a"), false),
    std::make_tuple(TR::InstOpCode::ldx,     TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f82a"), false),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0104ee"), false),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe104ee"), false),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f04ee"), false),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01fcee"), false),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0004ae"), false),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe004ae"), false),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f04ae"), false),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fcae"), false),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0006ae"), false),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe006ae"), false),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f06ae"), false),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00feae"), false),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0006ee"), false),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe006ee"), false),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f06ee"), false),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00feee"), false),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c01046e"), false),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe1046e"), false),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f046e"), false),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01fc6e"), false),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00042e"), false),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0042e"), false),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f042e"), false),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::RealRegister::fp0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fc2e"), false),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0102ee"), false),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe102ee"), false),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f02ee"), false),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01faee"), false),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0002ae"), false),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe002ae"), false),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f02ae"), false),
    std::make_tuple(TR::InstOpCode::lhax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00faae"), false),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00062c"), false),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0062c"), false),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f062c"), false),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fe2c"), false),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c01026e"), false),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe1026e"), false),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f026e"), false),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01fa6e"), false),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00022e"), false),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0022e"), false),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f022e"), false),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fa2e"), false),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0000ce"), false),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe000ce"), false),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f00ce"), false),
    std::make_tuple(TR::InstOpCode::lvx,     TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f8ce"), false),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00000e"), false),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0000e"), false),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f000e"), false),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f80e"), false),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00004e"), false),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0004e"), false),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f004e"), false),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f84e"), false),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00008e"), false),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0008e"), false),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f008e"), false),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::RealRegister::vr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f88e"), false),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000028"), false),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00028"), false),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0028"), false),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f828"), false),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0102ea"), false),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe102ea"), false),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f02ea"), false),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01faea"), false),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0002aa"), false),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe002aa"), false),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f02aa"), false),
    std::make_tuple(TR::InstOpCode::lwax,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00faaa"), false),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00042c"), false),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0042c"), false),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f042c"), false),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fc2c"), false),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c01006e"), false),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe1006e"), false),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f006e"), false),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c01f86e"), false),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c00002e"), false),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr31,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe0002e"), false),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f002e"), false),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::RealRegister::gr0,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f82e"), false),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000498"), false),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00498"), false),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00499"), false),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0498"), false),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fc98"), false),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000098"), false),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00098"), false),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00099"), false),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0098"), false),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f898"), false),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000018"), false),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00018"), false),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00019"), false),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0018"), false),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00f818"), false),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000418"), false),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00418"), false),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00419"), false),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0418"), false),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fc18"), false),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000698"), false),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00698"), false),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00699"), false),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0698"), false),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fe98"), false),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000298"), false),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00298"), false),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00299"), false),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0298"), false),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fa98"), false),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c000618"), false),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00618"), false),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe00619"), false),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f0618"), false),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fe18"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c0006d8"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr31, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe006d8"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7fe006d9"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr63, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr31), TRTest::BinaryInstruction("7ffffed9"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TRTest::BinaryInstruction("7c1f06d8"), false),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::RealRegister::vsr0,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TRTest::BinaryInstruction("7c00fed8"), false)
)));

INSTANTIATE_TEST_CASE_P(LoadIndex, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lbzux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lbzx,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldarx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldbrx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldux,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::ldx,     TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfdx,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfiwax,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfiwzx,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lfsx,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhaux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhax,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhbrx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lhzx,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvx,     TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvebx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvehx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lvewx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwarx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwaux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwax,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwbrx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzux,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lwzx,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsdx,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsiwax, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsiwzx, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxsspx,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvd2x,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvdsx,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvw4x,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvb16x, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(LoadVSXLength, PPCTrg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lxvl,  TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c00021a")),
    std::make_tuple(TR::InstOpCode::lxvl,  TR::RealRegister::vsr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0021a")),
    std::make_tuple(TR::InstOpCode::lxvl,  TR::RealRegister::vsr63, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0021b")),
    std::make_tuple(TR::InstOpCode::lxvl,  TR::RealRegister::vsr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f021a")),
    std::make_tuple(TR::InstOpCode::lxvl,  TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa1a")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c00025a")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0025a")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr63, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0025b")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr63, TR::RealRegister::gr31, TR::RealRegister::gr31, TRTest::BinaryInstruction("7ffffa5b")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f025a")),
    std::make_tuple(TR::InstOpCode::lxvll, TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fa5a"))
));

INSTANTIATE_TEST_CASE_P(LoadVSXLength, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::lxvl,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::lxvll, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StorePrefix, PPCMemSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0600000098000000"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("060000009be00000"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000981f0000"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0602000098000000"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffff9800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffff9800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffff98000000"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("060000009800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0600000098007fff"), true),
    std::make_tuple(TR::InstOpCode::pstb,    MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffff98008000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f4000000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("04000000f7e00000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f41f0000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04020000f4000000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0401fffff400ffff"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0403fffff400ffff"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0401fffff4000000"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f400ffff"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f4007fff"), true),
    std::make_tuple(TR::InstOpCode::pstd,    MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0403fffff4008000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d8000000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("06000000dbe00000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d81f0000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06020000d8000000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0601ffffd800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0603ffffd800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0601ffffd8000000"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d8007fff"), true),
    std::make_tuple(TR::InstOpCode::pstfd,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0603ffffd8008000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d0000000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("06000000d3e00000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d01f0000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06020000d0000000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0601ffffd000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0603ffffd000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0601ffffd0000000"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("06000000d0007fff"), true),
    std::make_tuple(TR::InstOpCode::pstfs,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("0603ffffd0008000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000b0000000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("06000000b3e00000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000b01f0000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06020000b0000000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffffb000ffff"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffffb000ffff"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffffb0000000"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000b000ffff"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000b0007fff"), true),
    std::make_tuple(TR::InstOpCode::psth,    MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffffb0008000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f0000000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr30,  TRTest::BinaryInstruction("04000000f3c00000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f01f0000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04020000f0000000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0401fffff000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0403fffff000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0401fffff0000000"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("04000000f0007fff"), true),
    std::make_tuple(TR::InstOpCode::pstq,    MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0403fffff0008000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0600000090000000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("0600000093e00000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("06000000901f0000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0602000090000000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffff9000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffff9000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0601ffff90000000"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("060000009000ffff"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0600000090007fff"), true),
    std::make_tuple(TR::InstOpCode::pstw,    MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("0603ffff90008000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000b8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vr31,  TRTest::BinaryInstruction("04000000bbe00000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000b81f0000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04020000b8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0401ffffb800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0403ffffb800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0401ffffb8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000b800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000b8007fff"), true),
    std::make_tuple(TR::InstOpCode::pstxsd,  MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0403ffffb8008000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000bc000000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vr31,  TRTest::BinaryInstruction("04000000bfe00000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000bc1f0000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04020000bc000000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0401ffffbc00ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0403ffffbc00ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0401ffffbc000000"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000bc00ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::vr0,   TRTest::BinaryInstruction("04000000bc007fff"), true),
    std::make_tuple(TR::InstOpCode::pstxssp, MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::vr0,   TRTest::BinaryInstruction("0403ffffbc008000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("04000000d8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vsr31, TRTest::BinaryInstruction("04000000dbe00000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x000000000), TR::RealRegister::vsr63, TRTest::BinaryInstruction("04000000dfe00000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr31, 0x000000000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("04000000d81f0000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0, -0x200000000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("04020000d8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x1ffffffff), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("0401ffffd800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0, -0x000000001), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("0403ffffd800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x1ffff0000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("0401ffffd8000000"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x00000ffff), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("04000000d800ffff"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0,  0x000007fff), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("04000000d8007fff"), true),
    std::make_tuple(TR::InstOpCode::pstxv,   MemoryReference(TR::RealRegister::gr0, -0x000008000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("0403ffffd8008000"), true)
)));

INSTANTIATE_TEST_CASE_P(StorePrefix, PPCMemSrc1PCRelativeEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, int64_t, int64_t, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000098000000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("061000009be00000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("0612000098000000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffff9800ffff")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffff9800ffff")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffff98000000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("061000009800ffff")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("0610000098007fff")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffff98008000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("0610000098008000")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("0610000098007ffe")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("061000009800ffff")),
    std::make_tuple(TR::InstOpCode::pstb,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffff9800ffff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000f4000000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000f7e00000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000f4000000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411fffff400ffff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413fffff400ffff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411fffff4000000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000f400ffff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000f4007fff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413fffff4008000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000f4008000")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000f4007ffe")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000f400ffff")),
    std::make_tuple(TR::InstOpCode::pstd,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411fffff400ffff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000d8000000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000dbe00000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000d8000000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffd800ffff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffd800ffff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffd8000000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000d800ffff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000d8007fff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffd8008000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000d8008000")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000d8007ffe")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000d800ffff")),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::RealRegister::fp0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffd800ffff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000d0000000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000d3e00000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000d0000000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffd000ffff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffd000ffff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffd0000000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000d000ffff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000d0007fff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffd0008000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000d0008000")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000d0007ffe")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000d000ffff")),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::RealRegister::fp0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffd000ffff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000b0000000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("06100000b3e00000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("06120000b0000000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffffb000ffff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffffb000ffff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffffb0000000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("06100000b000ffff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("06100000b0007fff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffffb0008000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("06100000b0008000")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("06100000b0007ffe")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("06100000b000ffff")),
    std::make_tuple(TR::InstOpCode::psth,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffffb000ffff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000f0000000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr30,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000f3c00000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000f0000000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411fffff000ffff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413fffff000ffff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411fffff0000000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000f000ffff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000f0007fff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413fffff0008000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000f0008000")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000f0007ffe")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000f000ffff")),
    std::make_tuple(TR::InstOpCode::pstq,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411fffff000ffff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000090000000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("0610000093e00000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("0612000090000000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0611ffff9000ffff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0613ffff9000ffff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0611ffff90000000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("061000009000ffff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("0610000090007fff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0613ffff90008000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("0610000090008000")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("0610000090007ffe")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("061000009000ffff")),
    std::make_tuple(TR::InstOpCode::pstw,    TR::RealRegister::gr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0611ffff9000ffff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000b8000000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000bbe00000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000b8000000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffb800ffff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffb800ffff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffb8000000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000b800ffff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000b8007fff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffb8008000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000b8008000")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000b8007ffe")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000b800ffff")),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::RealRegister::vr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffb800ffff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000bc000000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr31,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000bfe00000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,   -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000bc000000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffbc00ffff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,   -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffbc00ffff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffbc000000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000bc00ffff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000bc007fff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,   -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffbc008000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000bc008000")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000bc007ffe")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000bc00ffff")),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::RealRegister::vr0,    0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffbc00ffff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000d8000000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr31,  0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000dbe00000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr63,  0x000000000,  0x000000000, TRTest::BinaryInstruction("04100000dfe00000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,  -0x200000000,  0x000000000, TRTest::BinaryInstruction("04120000d8000000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x1ffffffff,  0x000000000, TRTest::BinaryInstruction("0411ffffd800ffff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,  -0x000000001,  0x000000000, TRTest::BinaryInstruction("0413ffffd800ffff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x1ffff0000,  0x000000000, TRTest::BinaryInstruction("0411ffffd8000000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x00000ffff,  0x000000000, TRTest::BinaryInstruction("04100000d800ffff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000000000, TRTest::BinaryInstruction("04100000d8007fff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,  -0x000008000,  0x000000000, TRTest::BinaryInstruction("0413ffffd8008000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000000001, TRTest::BinaryInstruction("04100000d8008000")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x000007fff, -0x000000001, TRTest::BinaryInstruction("04100000d8007ffe")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x000007fff,  0x000008000, TRTest::BinaryInstruction("04100000d800ffff")),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::RealRegister::vsr0,   0x0ffffffff,  0x100000000, TRTest::BinaryInstruction("0411ffffd800ffff"))
)));

INSTANTIATE_TEST_CASE_P(StorePrefix, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::pstb,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstd,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstfd,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstfs,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::psth,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstq,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstw,    TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstxsd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstxssp, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::pstxv,   TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StoreDisp, PPCMemSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("98000000"), false),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("981f0000"), false),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("9be00000"), false),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("98007fff"), false),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("98008000"), false),
    std::make_tuple(TR::InstOpCode::stb,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9800ffff"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9c010000"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9c1f0000"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("9fe10000"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9c017fff"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9c018000"), false),
    std::make_tuple(TR::InstOpCode::stbu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9c01ffff"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8000000"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f81f0000"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("fbe00000"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,   0x7ffc), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8007ffc"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8008000"), false),
    std::make_tuple(TR::InstOpCode::std,   MemoryReference(TR::RealRegister::gr0,  -0x0004), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f800fffc"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8010001"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f81f0001"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("fbe10001"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,   0x7ffc), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8017ffd"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f8018001"), false),
    std::make_tuple(TR::InstOpCode::stdu,  MemoryReference(TR::RealRegister::gr1,  -0x0004), TR::RealRegister::gr0,   TRTest::BinaryInstruction("f801fffd"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d8000000"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d81f0000"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("dbe00000"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d8007fff"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d8008000"), false),
    std::make_tuple(TR::InstOpCode::stfd,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d800ffff"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("dc010000"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("dc1f0000"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("dfe10000"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("dc017fff"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("dc018000"), false),
    std::make_tuple(TR::InstOpCode::stfdu, MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("dc01ffff"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d0000000"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d01f0000"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("d3e00000"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d0007fff"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d0008000"), false),
    std::make_tuple(TR::InstOpCode::stfs,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d000ffff"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d4010000"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d41f0000"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::fp31,  TRTest::BinaryInstruction("d7e10000"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d4017fff"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d4018000"), false),
    std::make_tuple(TR::InstOpCode::stfsu, MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::fp0,   TRTest::BinaryInstruction("d401ffff"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b0000000"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b01f0000"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("b3e00000"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b0007fff"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b0008000"), false),
    std::make_tuple(TR::InstOpCode::sth,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b000ffff"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b4010000"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b41f0000"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("b7e10000"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b4017fff"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b4018000"), false),
    std::make_tuple(TR::InstOpCode::sthu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("b401ffff"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("bc000000"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("bc1f0000"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("bfe00000"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("bc007fff"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("bc008000"), false),
    std::make_tuple(TR::InstOpCode::stmw,  MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("bc00ffff"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("90000000"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("901f0000"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("93e00000"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("90007fff"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("90008000"), false),
    std::make_tuple(TR::InstOpCode::stw,   MemoryReference(TR::RealRegister::gr0,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9000ffff"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("94010000"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("941f0000"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x0000), TR::RealRegister::gr31,  TRTest::BinaryInstruction("97e10000"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,   0x7fff), TR::RealRegister::gr0,   TRTest::BinaryInstruction("94017fff"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,  -0x8000), TR::RealRegister::gr0,   TRTest::BinaryInstruction("94018000"), false),
    std::make_tuple(TR::InstOpCode::stwu,  MemoryReference(TR::RealRegister::gr1,  -0x0001), TR::RealRegister::gr0,   TRTest::BinaryInstruction("9401ffff"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f4000005"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr31,  0x0000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f41f0005"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::vsr31, TRTest::BinaryInstruction("f7e00005"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,   0x0000), TR::RealRegister::vsr63, TRTest::BinaryInstruction("f7e0000d"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,   0x7ff0), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f4007ff5"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,  -0x8000), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f4008005"), false),
    std::make_tuple(TR::InstOpCode::stxv,  MemoryReference(TR::RealRegister::gr0,  -0x0010), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("f400fff5"), false)
)));

INSTANTIATE_TEST_CASE_P(StoreDisp, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stb,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stbu,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::std,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdu,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfd,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfs,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsu, TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sth,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthu,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stmw,  TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stw,   TR::InstOpCode::bad, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwu,  TR::InstOpCode::bad, TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StoreIndex, PPCMemSrc1EncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, MemoryReference, TR::RealRegister::RegNum, TRTest::BinaryInstruction, bool>>(
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c0101ee"), false),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f01ee"), false),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01f9ee"), false),
    std::make_tuple(TR::InstOpCode::stbux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe101ee"), false),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c0001ae"), false),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f01ae"), false),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00f9ae"), false),
    std::make_tuple(TR::InstOpCode::stbx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe001ae"), false),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c0001ad"), false),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f01ad"), false),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00f9ad"), false),
    std::make_tuple(TR::InstOpCode::stdcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe001ad"), false),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01016a"), false),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f016a"), false),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01f96a"), false),
    std::make_tuple(TR::InstOpCode::stdux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe1016a"), false),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00012a"), false),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f012a"), false),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00f92a"), false),
    std::make_tuple(TR::InstOpCode::stdx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0012a"), false),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c0105ee"), false),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c1f05ee"), false),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c01fdee"), false),
    std::make_tuple(TR::InstOpCode::stfdux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  TRTest::BinaryInstruction("7fe105ee"), false),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c0005ae"), false),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c1f05ae"), false),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c00fdae"), false),
    std::make_tuple(TR::InstOpCode::stfdx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  TRTest::BinaryInstruction("7fe005ae"), false),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c0007ae"), false),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c1f07ae"), false),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c00ffae"), false),
    std::make_tuple(TR::InstOpCode::stfiwx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  TRTest::BinaryInstruction("7fe007ae"), false),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c01056e"), false),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c1f056e"), false),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c01fd6e"), false),
    std::make_tuple(TR::InstOpCode::stfsux,  MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  TRTest::BinaryInstruction("7fe1056e"), false),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c00052e"), false),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c1f052e"), false),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::fp0,   TRTest::BinaryInstruction("7c00fd2e"), false),
    std::make_tuple(TR::InstOpCode::stfsx,   MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::fp31,  TRTest::BinaryInstruction("7fe0052e"), false),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00072c"), false),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f072c"), false),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00ff2c"), false),
    std::make_tuple(TR::InstOpCode::sthbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0072c"), false),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01036e"), false),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f036e"), false),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01fb6e"), false),
    std::make_tuple(TR::InstOpCode::sthux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe1036e"), false),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00032e"), false),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f032e"), false),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00fb2e"), false),
    std::make_tuple(TR::InstOpCode::sthx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0032e"), false),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c0001ce"), false),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c1f01ce"), false),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00f9ce"), false),
    std::make_tuple(TR::InstOpCode::stvx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  TRTest::BinaryInstruction("7fe001ce"), false),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00010e"), false),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c1f010e"), false),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00f90e"), false),
    std::make_tuple(TR::InstOpCode::stvebx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  TRTest::BinaryInstruction("7fe0010e"), false),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00014e"), false),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c1f014e"), false),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00f94e"), false),
    std::make_tuple(TR::InstOpCode::stvehx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  TRTest::BinaryInstruction("7fe0014e"), false),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00018e"), false),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c1f018e"), false),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vr0,   TRTest::BinaryInstruction("7c00f98e"), false),
    std::make_tuple(TR::InstOpCode::stvewx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vr31,  TRTest::BinaryInstruction("7fe0018e"), false),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00052c"), false),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f052c"), false),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00fd2c"), false),
    std::make_tuple(TR::InstOpCode::stwbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0052c"), false),
    std::make_tuple(TR::InstOpCode::stdbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c000528"), false),
    std::make_tuple(TR::InstOpCode::stdbrx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f0528"), false),
    std::make_tuple(TR::InstOpCode::stdbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00fd28"), false),
    std::make_tuple(TR::InstOpCode::stdbrx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe00528"), false),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00012d"), false),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f012d"), false),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00f92d"), false),
    std::make_tuple(TR::InstOpCode::stwcx_r, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0012d"), false),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01016e"), false),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f016e"), false),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c01f96e"), false),
    std::make_tuple(TR::InstOpCode::stwux,   MemoryReference(TR::RealRegister::gr1,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe1016e"), false),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00012e"), false),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c1f012e"), false),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::gr0,   TRTest::BinaryInstruction("7c00f92e"), false),
    std::make_tuple(TR::InstOpCode::stwx,    MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::gr31,  TRTest::BinaryInstruction("7fe0012e"), false),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000598"), false),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0598"), false),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00fd98"), false),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00598"), false),
    std::make_tuple(TR::InstOpCode::stxsdx,  MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00599"), false),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000118"), false),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0118"), false),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00f918"), false),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00118"), false),
    std::make_tuple(TR::InstOpCode::stxsiwx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00119"), false),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000518"), false),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0518"), false),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00fd18"), false),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00518"), false),
    std::make_tuple(TR::InstOpCode::stxsspx, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00519"), false),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000798"), false),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0798"), false),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00ff98"), false),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00798"), false),
    std::make_tuple(TR::InstOpCode::stxvd2x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00799"), false),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000718"), false),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0718"), false),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00ff18"), false),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00718"), false),
    std::make_tuple(TR::InstOpCode::stxvw4x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00719"), false),
    std::make_tuple(TR::InstOpCode::stxvh8x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c000758"), false),
    std::make_tuple(TR::InstOpCode::stxvh8x, MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f0758"), false),
    std::make_tuple(TR::InstOpCode::stxvh8x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00ff58"), false),
    std::make_tuple(TR::InstOpCode::stxvh8x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe00758"), false),
    std::make_tuple(TR::InstOpCode::stxvh8x, MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe00759"), false),
    std::make_tuple(TR::InstOpCode::stxvb16x,MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c0007d8"), false),
    std::make_tuple(TR::InstOpCode::stxvb16x,MemoryReference(TR::RealRegister::gr31, TR::RealRegister::gr0 ), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c1f07d8"), false),
    std::make_tuple(TR::InstOpCode::stxvb16x,MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr31), TR::RealRegister::vsr0,  TRTest::BinaryInstruction("7c00ffd8"), false),
    std::make_tuple(TR::InstOpCode::stxvb16x,MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr31, TRTest::BinaryInstruction("7fe007d8"), false),
    std::make_tuple(TR::InstOpCode::stxvb16x,MemoryReference(TR::RealRegister::gr0,  TR::RealRegister::gr0 ), TR::RealRegister::vsr63, TRTest::BinaryInstruction("7fe007d9"), false)
)));

INSTANTIATE_TEST_CASE_P(StoreIndex, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stbux,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stbx,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,     TR::InstOpCode::stdcx_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdux,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdx,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdux,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfdx,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfiwx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsux,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stfsx,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthbrx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthux,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::sthx,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvx,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvebx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvehx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stvewx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwbrx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stdbrx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::bad,     TR::InstOpCode::stwcx_r, TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwux,   TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stwx,    TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsdx,  TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsiwx, TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxsspx, TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvd2x, TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvw4x, TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvh8x, TR::InstOpCode::bad,     TRTest::BinaryInstruction()),
    std::make_tuple(TR::InstOpCode::stxvb16x,TR::InstOpCode::bad,     TRTest::BinaryInstruction())
));

INSTANTIATE_TEST_CASE_P(StoreVSXLength, PPCSrc3EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stxvl, TR::RealRegister::vsr31, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0031a")),
    std::make_tuple(TR::InstOpCode::stxvl, TR::RealRegister::vsr63, TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7fe0031b")),
    std::make_tuple(TR::InstOpCode::stxvl, TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c00031a")),
    std::make_tuple(TR::InstOpCode::stxvl, TR::RealRegister::vsr0,  TR::RealRegister::gr31, TR::RealRegister::gr0,  TRTest::BinaryInstruction("7c1f031a")),
    std::make_tuple(TR::InstOpCode::stxvl, TR::RealRegister::vsr0,  TR::RealRegister::gr0,  TR::RealRegister::gr31, TRTest::BinaryInstruction("7c00fb1a"))
));

INSTANTIATE_TEST_CASE_P(StoreVSXLength, PPCRecordFormSanityTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::stxvl, TR::InstOpCode::bad, TRTest::BinaryInstruction())
));
