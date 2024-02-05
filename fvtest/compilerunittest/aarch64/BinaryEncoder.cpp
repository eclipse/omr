/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#include <gtest/gtest.h>
#include "../CodeGenTest.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "il/Node_inlines.hpp"

#define ARM64_INSTRUCTION_ALIGNMENT 32

class ARM64BinaryInstruction : public TRTest::BinaryInstruction {
public:

    ARM64BinaryInstruction() : TRTest::BinaryInstruction() {
    }
    ARM64BinaryInstruction(const char *instr) : TRTest::BinaryInstruction(instr) {
        // As the tests are encoded as big endian strings, we need to convert them to little endian
        for (int i = 0; i < _size / sizeof(uint32_t); i++) {
            std::swap(_buf[i * sizeof(uint32_t)], _buf[i * sizeof(uint32_t) + 3]);
            std::swap(_buf[i * sizeof(uint32_t) + 1], _buf[i * sizeof(uint32_t) + 2]);
        }
    }
};

std::ostream &operator<<(std::ostream &os, const ARM64BinaryInstruction &instr) {
    os << static_cast<TRTest::BinaryInstruction>(instr);
    return os;
}

class ARM64Trg1ImmEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1ImmEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto imm = std::get<2>(GetParam());

    auto instr = generateTrg1ImmInstruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, imm);

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

class ARM64Trg1ImmShiftedEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1ImmShiftedEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto imm = std::get<2>(GetParam());
    auto shiftAmount = std::get<3>(GetParam());

    auto instr = generateTrg1ImmShiftedInstruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, imm, shiftAmount);

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

class ARM64Trg1Src1EncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1Src1EncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto src1Reg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));

    auto instr = generateTrg1Src1Instruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, src1Reg);

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

class ARM64Trg1Src2EncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1Src2EncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto src1Reg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto src2Reg = cg()->machine()->getRealRegister(std::get<3>(GetParam()));

    auto instr = generateTrg1Src2Instruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, src1Reg, src2Reg);

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

class ARM64Trg1Src2IndexedElementEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1Src2IndexedElementEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto src1Reg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto src2Reg = cg()->machine()->getRealRegister(std::get<3>(GetParam()));

    auto instr = generateTrg1Src2IndexedElementInstruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, src1Reg, src2Reg, std::get<4>(GetParam()));

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

class ARM64Trg1Src2ImmEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64Trg1Src2ImmEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto src1Reg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto src2Reg = cg()->machine()->getRealRegister(std::get<3>(GetParam()));

    auto instr = generateTrg1Src2ImmInstruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, src1Reg, src2Reg, std::get<4>(GetParam()));

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}


class ARM64VectorShiftImmediateEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64VectorShiftImmediateEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto src1Reg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));

    auto instr = generateVectorShiftImmediateInstruction(cg(), std::get<0>(GetParam()), fakeNode, trgReg, src1Reg, std::get<3>(GetParam()));

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

class ARM64VectorSplatsImmediateEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::DataType, uint64_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64VectorSplatsImmediateEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto elementType = std::get<2>(GetParam());
    auto childNode = TR::Node::create(TR::iconst);
    childNode->setConstValue(std::get<3>(GetParam()));

    auto instr = TR::TreeEvaluator::vsplatsImmediateHelper(fakeNode, cg(), childNode, elementType, trgReg);

    if (op == TR::InstOpCode::bad) {
        ASSERT_EQ(instr, nullptr);
    } else {
        ASSERT_EQ(instr->getOpCodeValue(), op);
        ASSERT_EQ(encodeInstruction(instr), std::get<4>(GetParam()));
    }
}

class ARM64VectorDupElementEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64VectorDupElementEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto srcIndex = std::get<3>(GetParam());

    auto instr = generateVectorDupElementInstruction(cg(), op, fakeNode, trgReg, srcReg, srcIndex);

    ASSERT_EQ(encodeInstruction(instr), std::get<4>(GetParam()));
}

class ARM64MovVectorElementToGPREncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64MovVectorElementToGPREncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto srcIndex = std::get<3>(GetParam());

    auto instr = generateMovVectorElementToGPRInstruction(cg(), op, fakeNode, trgReg, srcReg, srcIndex);

    ASSERT_EQ(encodeInstruction(instr), std::get<4>(GetParam()));
}

class ARM64MovGPRToVectorElementEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64MovGPRToVectorElementEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto trgIndex = std::get<3>(GetParam());

    auto instr = generateMovGPRToVectorElementInstruction(cg(), op, fakeNode, trgReg, srcReg, trgIndex);

    ASSERT_EQ(encodeInstruction(instr), std::get<4>(GetParam()));
}

class ARM64MovVectorElementEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, uint32_t, ARM64BinaryInstruction>> {};

TEST_P(ARM64MovVectorElementEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto trgIndex = std::get<3>(GetParam());
    auto srcIndex = std::get<4>(GetParam());

    auto instr = generateMovVectorElementInstruction(cg(), op, fakeNode, trgReg, srcReg, trgIndex, srcIndex);

    ASSERT_EQ(encodeInstruction(instr), std::get<5>(GetParam()));
}

class ARM64Src1ImmCondEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, uint32_t, uint32_t, TR::ARM64ConditionCode, ARM64BinaryInstruction>> {};

TEST_P(ARM64Src1ImmCondEncodingTest, encode) {
    auto srcReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto imm = std::get<2>(GetParam());
    auto conditionFlags = std::get<3>(GetParam());
    auto cc = std::get<4>(GetParam());
    auto isNegative = (op == TR::InstOpCode::ccmnimmx || op == TR::InstOpCode::ccmnimmw);
    auto is64bit = (op == TR::InstOpCode::ccmpimmx || op == TR::InstOpCode::ccmnimmx);

    auto instr = generateConditionalCompareImmInstruction(cg(), fakeNode, srcReg, imm, conditionFlags, cc, is64bit, isNegative);
    ASSERT_EQ(encodeInstruction(instr), std::get<5>(GetParam()));
}

class ARM64Src2CondEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, uint32_t, TR::ARM64ConditionCode, ARM64BinaryInstruction>> {};

TEST_P(ARM64Src2CondEncodingTest, encode) {
    auto srcReg1 = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg2 = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto op = std::get<0>(GetParam());
    auto conditionFlags = std::get<3>(GetParam());
    auto cc = std::get<4>(GetParam());
    auto isNegative = (op == TR::InstOpCode::ccmnx || op == TR::InstOpCode::ccmnw);
    auto is64bit = (op == TR::InstOpCode::ccmpx || op == TR::InstOpCode::ccmnx);

    auto instr = generateConditionalCompareInstruction(cg(), fakeNode, srcReg1, srcReg2, conditionFlags, cc, is64bit, isNegative);
    ASSERT_EQ(encodeInstruction(instr), std::get<5>(GetParam()));
}

class ARM64CINCEncodingTest : public TRTest::BinaryEncoderTest<ARM64_INSTRUCTION_ALIGNMENT>, public ::testing::WithParamInterface<std::tuple<bool, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::ARM64ConditionCode, ARM64BinaryInstruction>> {};

TEST_P(ARM64CINCEncodingTest, encode) {
    auto trgReg = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto srcReg = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto is64bit = std::get<0>(GetParam());
    auto cc = std::get<3>(GetParam());

    auto instr = generateCIncInstruction(cg(), fakeNode, trgReg, srcReg, cc, is64bit);
    ASSERT_EQ(encodeInstruction(instr), std::get<4>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(MOV, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x3, 0, "d2800003"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x3, 0xffff, "d29fffe3"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x3, 0x1ffff, "d2bfffe3"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x3, 0x2ffff, "d2dfffe3"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x3, 0x3ffff, "d2ffffe3"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x28, 0, "d280001c"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x28, 0xffff, "d29ffffc"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x28, 0x1ffff, "d2bffffc"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x28, 0x2ffff, "d2dffffc"),
    std::make_tuple(TR::InstOpCode::movzx,  TR::RealRegister::x28, 0x3ffff, "d2fffffc"),

    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x3, 0, "f2800003"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x3, 0xffff, "f29fffe3"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x3, 0x1ffff, "f2bfffe3"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x3, 0x2ffff, "f2dfffe3"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x3, 0x3ffff, "f2ffffe3"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x28, 0, "f280001c"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x28, 0xffff, "f29ffffc"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x28, 0x1ffff, "f2bffffc"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x28, 0x2ffff, "f2dffffc"),
    std::make_tuple(TR::InstOpCode::movkx,  TR::RealRegister::x28, 0x3ffff, "f2fffffc"),

    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x3, 0, "92800003"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x3, 0xffff, "929fffe3"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x3, 0x1ffff, "92bfffe3"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x3, 0x2ffff, "92dfffe3"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x3, 0x3ffff, "92ffffe3"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x28, 0, "9280001c"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x28, 0xffff, "929ffffc"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x28, 0x1ffff, "92bffffc"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x28, 0x2ffff, "92dffffc"),
    std::make_tuple(TR::InstOpCode::movnx,  TR::RealRegister::x28, 0x3ffff, "92fffffc"),

    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x3, 0, "52800003"),
    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x3, 0xffff, "529fffe3"),
    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x3, 0x1ffff, "52bfffe3"),
    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x28, 0, "5280001c"),
    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x28, 0xffff, "529ffffc"),
    std::make_tuple(TR::InstOpCode::movzw,  TR::RealRegister::x28, 0x1ffff, "52bffffc"),

    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x3, 0, "72800003"),
    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x3, 0xffff, "729fffe3"),
    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x3, 0x1ffff, "72bfffe3"),
    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x28, 0, "7280001c"),
    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x28, 0xffff, "729ffffc"),
    std::make_tuple(TR::InstOpCode::movkw,  TR::RealRegister::x28, 0x1ffff, "72bffffc"),

    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x3, 0, "12800003"),
    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x3, 0xffff, "129fffe3"),
    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x3, 0x1ffff, "12bfffe3"),
    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x28, 0, "1280001c"),
    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x28, 0xffff, "129ffffc"),
    std::make_tuple(TR::InstOpCode::movnw,  TR::RealRegister::x28, 0x1ffff, "12bffffc")
));

INSTANTIATE_TEST_CASE_P(FMOV, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fmovimmd,  TR::RealRegister::v0, 0, "1e601000"),    // abcdefgh = 0, expanded to 2.0
    std::make_tuple(TR::InstOpCode::fmovimmd,  TR::RealRegister::v0, 0xff, "1e7ff000"), // abcdefgh = 0xff, expanded to -1.9375
    std::make_tuple(TR::InstOpCode::fmovimmd,  TR::RealRegister::v31, 0, "1e60101f"),
    std::make_tuple(TR::InstOpCode::fmovimmd,  TR::RealRegister::v31, 0xff, "1e7ff01f"),

    std::make_tuple(TR::InstOpCode::fmovimms,  TR::RealRegister::v0, 0, "1e201000"),
    std::make_tuple(TR::InstOpCode::fmovimms,  TR::RealRegister::v0, 0xff, "1e3ff000"),
    std::make_tuple(TR::InstOpCode::fmovimms,  TR::RealRegister::v31, 0, "1e20101f"),
    std::make_tuple(TR::InstOpCode::fmovimms,  TR::RealRegister::v31, 0xff, "1e3ff01f"),

    std::make_tuple(TR::InstOpCode::vfmov2d,  TR::RealRegister::v0, 0, "6f00f400"),
    std::make_tuple(TR::InstOpCode::vfmov2d,  TR::RealRegister::v0, 0xff, "6f07f7e0"),
    std::make_tuple(TR::InstOpCode::vfmov2d,  TR::RealRegister::v31, 0, "6f00f41f"),
    std::make_tuple(TR::InstOpCode::vfmov2d,  TR::RealRegister::v31, 0xff, "6f07f7ff"),

    std::make_tuple(TR::InstOpCode::vfmov4s,  TR::RealRegister::v0, 0, "4f00f400"),
    std::make_tuple(TR::InstOpCode::vfmov4s,  TR::RealRegister::v0, 0xff, "4f07f7e0"),
    std::make_tuple(TR::InstOpCode::vfmov4s,  TR::RealRegister::v31, 0, "4f00f41f"),
    std::make_tuple(TR::InstOpCode::vfmov4s,  TR::RealRegister::v31, 0xff, "4f07f7ff")
));

INSTANTIATE_TEST_CASE_P(MOVI, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v0, 0, "4f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v0, 0x55, "4f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v0, 0xff, "4f07e7e0"),
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v31, 0, "4f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v31, 0x55, "4f02e6bf"),
    std::make_tuple(TR::InstOpCode::vmovi16b,  TR::RealRegister::v31, 0xff, "4f07e7ff"),

    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v0, 0, "4f008400"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v0, 0x55, "4f0286a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v0, 0xff, "4f0787e0"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v31, 0, "4f00841f"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v31, 0x55, "4f0286bf"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v31, 0xff, "4f0787ff"),

    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0, "4f000400"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0x55, "4f0206a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0xff, "4f0707e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0, "4f00041f"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0x55, "4f0206bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0xff, "4f0707ff"),

    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v0, 0, "6f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v0, 0xaa, "6f05e540"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v0, 0x55, "6f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v0, 0xff, "6f07e7e0"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v31, 0, "6f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v31, 0xaa, "6f05e55f"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v31, 0x55, "6f02e6bf"),
    std::make_tuple(TR::InstOpCode::vmovi2d,  TR::RealRegister::v31, 0xff, "6f07e7ff"),

    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v0, 0, "2f00e400"),
    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v0, 0x55, "2f02e6a0"),
    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v0, 0xff, "2f07e7e0"),
    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v31, 0, "2f00e41f"),
    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v31, 0x55, "2f02e6bf"),
    std::make_tuple(TR::InstOpCode::movid,  TR::RealRegister::v31, 0xff, "2f07e7ff"),

    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v0, 0, "0f000400"),
    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v0, 0x55, "0f0206a0"),
    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v0, 0xff, "0f0707e0"),
    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v31, 0, "0f00041f"),
    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v31, 0x55, "0f0206bf"),
    std::make_tuple(TR::InstOpCode::vmovi2s,  TR::RealRegister::v31, 0xff, "0f0707ff")
));

INSTANTIATE_TEST_CASE_P(MVNI, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v0, 0, "6f008400"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v0, 0x55, "6f0286a0"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v0, 0xff, "6f0787e0"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v31, 0, "6f00841f"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v31, 0x55, "6f0286bf"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v31, 0xff, "6f0787ff"),

    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0, "6f000400"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0x55, "6f0206a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0xff, "6f0707e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0, "6f00041f"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0x55, "6f0206bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0xff, "6f0707ff")
));

INSTANTIATE_TEST_CASE_P(BIC, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v0, 0x0, "6f009400"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v0, 0x55, "6f0296a0"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v0, 0xff, "6f0797e0"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v31, 0x0, "6f00941f"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v31, 0x55, "6f0296bf"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v31, 0xff, "6f0797ff"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0x0, "6f001400"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0x55, "6f0216a0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0xff, "6f0717e0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0x0, "6f00141f"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0x55, "6f0216bf"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0xff, "6f0717ff")
));

INSTANTIATE_TEST_CASE_P(ORR, ARM64Trg1ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v0, 0x0, "4f009400"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v0, 0x55, "4f0296a0"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v0, 0xff, "4f0797e0"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v31, 0x0, "4f00941f"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v31, 0x55, "4f0296bf"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v31, 0xff, "4f0797ff"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0x0, "4f001400"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0x55, "4f0216a0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0xff, "4f0717e0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0x0, "4f00141f"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0x55, "4f0216bf"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0xff, "4f0717ff")
));

INSTANTIATE_TEST_CASE_P(MOVI, ARM64Trg1ImmShiftedEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v0, 0x55, 8, "4f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v0, 0xff, 8, "4f07a7e0"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v31, 0x55, 8, "4f02a6bf"),
    std::make_tuple(TR::InstOpCode::vmovi8h,  TR::RealRegister::v31, 0xff, 8, "4f07a7ff"),

    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0x55, 8, "4f0226a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0xff, 8, "4f0727e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0x55, 16, "4f0246a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0xff, 16, "4f0747e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0x55, 24, "4f0266a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v0, 0xff, 24, "4f0767e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0x55, 8, "4f0226bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0xff, 8, "4f0727ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0x55, 16, "4f0246bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0xff, 16, "4f0747ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0x55, 24, "4f0266bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s,  TR::RealRegister::v31, 0xff, 24, "4f0767ff"),

    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v0, 0x55, 8, "4f02c6a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v0, 0xff, 8, "4f07c7e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v0, 0x55, 16, "4f02d6a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v0, 0xff, 16, "4f07d7e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v31, 0x55, 8, "4f02c6bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v31, 0xff, 8, "4f07c7ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v31, 0x55, 16, "4f02d6bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one,  TR::RealRegister::v31, 0xff, 16, "4f07d7ff")
));

INSTANTIATE_TEST_CASE_P(MVNI, ARM64Trg1ImmShiftedEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v0, 0x55, 8, "6f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v0, 0xff, 8, "6f07a7e0"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v31, 0x55, 8, "6f02a6bf"),
    std::make_tuple(TR::InstOpCode::vmvni8h,  TR::RealRegister::v31, 0xff, 8, "6f07a7ff"),

    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0x55, 8, "6f0226a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0xff, 8, "6f0727e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0x55, 16, "6f0246a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0xff, 16, "6f0747e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0x55, 24, "6f0266a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v0, 0xff, 24, "6f0767e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0x55, 8, "6f0226bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0xff, 8, "6f0727ff"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0x55, 16, "6f0246bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0xff, 16, "6f0747ff"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0x55, 24, "6f0266bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s,  TR::RealRegister::v31, 0xff, 24, "6f0767ff"),

    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v0, 0x55, 8, "6f02c6a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v0, 0xff, 8, "6f07c7e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v0, 0x55, 16, "6f02d6a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v0, 0xff, 16, "6f07d7e0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v31, 0x55, 8, "6f02c6bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v31, 0xff, 8, "6f07c7ff"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v31, 0x55, 16, "6f02d6bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one,  TR::RealRegister::v31, 0xff, 16, "6f07d7ff")
));

INSTANTIATE_TEST_CASE_P(BIC, ARM64Trg1ImmShiftedEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v0, 0x55, 8, "6f02b6a0"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v0, 0xff, 8, "6f07b7e0"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v31, 0x55, 8, "6f02b6bf"),
    std::make_tuple(TR::InstOpCode::vbicimm8h, TR::RealRegister::v31, 0xff, 8, "6f07b7ff"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0x55, 8, "6f0236a0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0xff, 8, "6f0737e0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0x55, 16, "6f0256a0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0xff, 16, "6f0757e0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0x55, 24, "6f0276a0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v0, 0xff, 24, "6f0777e0"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0x55, 8, "6f0236bf"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0xff, 8, "6f0737ff"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0x55, 16, "6f0256bf"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0xff, 16, "6f0757ff"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0x55, 24, "6f0276bf"),
    std::make_tuple(TR::InstOpCode::vbicimm4s, TR::RealRegister::v31, 0xff, 24, "6f0777ff")
));

INSTANTIATE_TEST_CASE_P(ORR, ARM64Trg1ImmShiftedEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v0, 0x55, 8, "4f02b6a0"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v0, 0xff, 8, "4f07b7e0"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v31, 0x55, 8, "4f02b6bf"),
    std::make_tuple(TR::InstOpCode::vorrimm8h, TR::RealRegister::v31, 0xff, 8, "4f07b7ff"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0x55, 8, "4f0236a0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0xff, 8, "4f0737e0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0x55, 16, "4f0256a0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0xff, 16, "4f0757e0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0x55, 24, "4f0276a0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v0, 0xff, 24, "4f0777e0"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0x55, 8, "4f0236bf"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0xff, 8, "4f0737ff"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0x55, 16, "4f0256bf"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0xff, 16, "4f0757ff"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0x55, 24, "4f0276bf"),
    std::make_tuple(TR::InstOpCode::vorrimm4s, TR::RealRegister::v31, 0xff, 24, "4f0777ff")
));

INSTANTIATE_TEST_CASE_P(VectorSQRT, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfsqrt4s,  TR::RealRegister::v15, TR::RealRegister::v0, "6ea1f80f"),
    std::make_tuple(TR::InstOpCode::vfsqrt4s,  TR::RealRegister::v31, TR::RealRegister::v0, "6ea1f81f"),
    std::make_tuple(TR::InstOpCode::vfsqrt4s,  TR::RealRegister::v0, TR::RealRegister::v15, "6ea1f9e0"),
    std::make_tuple(TR::InstOpCode::vfsqrt4s,  TR::RealRegister::v0, TR::RealRegister::v31, "6ea1fbe0"),

    std::make_tuple(TR::InstOpCode::vfsqrt2d,  TR::RealRegister::v15, TR::RealRegister::v0, "6ee1f80f"),
    std::make_tuple(TR::InstOpCode::vfsqrt2d,  TR::RealRegister::v31, TR::RealRegister::v0, "6ee1f81f"),
    std::make_tuple(TR::InstOpCode::vfsqrt2d,  TR::RealRegister::v0, TR::RealRegister::v15, "6ee1f9e0"),
    std::make_tuple(TR::InstOpCode::vfsqrt2d,  TR::RealRegister::v0, TR::RealRegister::v31, "6ee1fbe0")
));

INSTANTIATE_TEST_CASE_P(VectorMLA, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20cc0f"),
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20cc1f"),
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20cde0"),
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20cfe0"),
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2fcc00"),
    std::make_tuple(TR::InstOpCode::vfmla4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3fcc00"),

    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60cc0f"),
    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60cc1f"),
    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60cde0"),
    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60cfe0"),
    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6fcc00"),
    std::make_tuple(TR::InstOpCode::vfmla2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7fcc00"),

    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20940f"),
    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20941f"),
    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e2095e0"),
    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e2097e0"),
    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f9400"),
    std::make_tuple(TR::InstOpCode::vmla16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f9400"),

    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60940f"),
    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60941f"),
    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e6095e0"),
    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e6097e0"),
    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f9400"),
    std::make_tuple(TR::InstOpCode::vmla8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f9400"),

    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0940f"),
    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0941f"),
    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea095e0"),
    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea097e0"),
    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf9400"),
    std::make_tuple(TR::InstOpCode::vmla4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf9400")
));

INSTANTIATE_TEST_CASE_P(VectorMIN, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e206c0f"),
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e206c1f"),
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e206de0"),
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e206fe0"),
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f6c00"),
    std::make_tuple(TR::InstOpCode::vsmin16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f6c00"),

    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e606c0f"),
    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e606c1f"),
    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e606de0"),
    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e606fe0"),
    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f6c00"),
    std::make_tuple(TR::InstOpCode::vsmin8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f6c00"),

    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea06c0f"),
    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea06c1f"),
    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea06de0"),
    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea06fe0"),
    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf6c00"),
    std::make_tuple(TR::InstOpCode::vsmin4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf6c00"),

    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0f40f"),
    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0f41f"),
    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0f5e0"),
    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0f7e0"),
    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaff400"),
    std::make_tuple(TR::InstOpCode::vfmin4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebff400"),

    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0f40f"),
    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0f41f"),
    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0f5e0"),
    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0f7e0"),
    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eeff400"),
    std::make_tuple(TR::InstOpCode::vfmin2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4efff400")
));

INSTANTIATE_TEST_CASE_P(VectorMAX, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20640f"),
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20641f"),
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e2065e0"),
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e2067e0"),
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f6400"),
    std::make_tuple(TR::InstOpCode::vsmax16b,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f6400"),

    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60640f"),
    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60641f"),
    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e6065e0"),
    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e6067e0"),
    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f6400"),
    std::make_tuple(TR::InstOpCode::vsmax8h,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f6400"),

    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0640f"),
    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0641f"),
    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea065e0"),
    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea067e0"),
    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf6400"),
    std::make_tuple(TR::InstOpCode::vsmax4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf6400"),

    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20f40f"),
    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20f41f"),
    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20f5e0"),
    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20f7e0"),
    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2ff400"),
    std::make_tuple(TR::InstOpCode::vfmax4s,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3ff400"),

    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60f40f"),
    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60f41f"),
    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60f5e0"),
    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60f7e0"),
    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6ff400"),
    std::make_tuple(TR::InstOpCode::vfmax2d,  TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7ff400")
));

INSTANTIATE_TEST_CASE_P(VectorLogic, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e201c0f"),
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e201c1f"),
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e201de0"),
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e201fe0"),
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f1c00"),
    std::make_tuple(TR::InstOpCode::vand16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f1c00"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e601c0f"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e601c1f"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e601de0"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e601fe0"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f1c00"),
    std::make_tuple(TR::InstOpCode::vbic16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f1c00"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea01c0f"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea01c1f"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea01de0"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea01fe0"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf1c00"),
    std::make_tuple(TR::InstOpCode::vorr16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf1c00"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e201c0f"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e201c1f"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e201de0"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e201fe0"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f1c00"),
    std::make_tuple(TR::InstOpCode::veor16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f1c00")
));

INSTANTIATE_TEST_CASE_P(VectorABS, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vabs16b,  TR::RealRegister::v15, TR::RealRegister::v0, "4e20b80f"),
    std::make_tuple(TR::InstOpCode::vabs16b,  TR::RealRegister::v31, TR::RealRegister::v0, "4e20b81f"),
    std::make_tuple(TR::InstOpCode::vabs16b,  TR::RealRegister::v0, TR::RealRegister::v15, "4e20b9e0"),
    std::make_tuple(TR::InstOpCode::vabs16b,  TR::RealRegister::v0, TR::RealRegister::v31, "4e20bbe0"),

    std::make_tuple(TR::InstOpCode::vabs8h,  TR::RealRegister::v15, TR::RealRegister::v0, "4e60b80f"),
    std::make_tuple(TR::InstOpCode::vabs8h,  TR::RealRegister::v31, TR::RealRegister::v0, "4e60b81f"),
    std::make_tuple(TR::InstOpCode::vabs8h,  TR::RealRegister::v0, TR::RealRegister::v15, "4e60b9e0"),
    std::make_tuple(TR::InstOpCode::vabs8h,  TR::RealRegister::v0, TR::RealRegister::v31, "4e60bbe0"),

    std::make_tuple(TR::InstOpCode::vabs4s,  TR::RealRegister::v15, TR::RealRegister::v0, "4ea0b80f"),
    std::make_tuple(TR::InstOpCode::vabs4s,  TR::RealRegister::v31, TR::RealRegister::v0, "4ea0b81f"),
    std::make_tuple(TR::InstOpCode::vabs4s,  TR::RealRegister::v0, TR::RealRegister::v15, "4ea0b9e0"),
    std::make_tuple(TR::InstOpCode::vabs4s,  TR::RealRegister::v0, TR::RealRegister::v31, "4ea0bbe0"),

    std::make_tuple(TR::InstOpCode::vabs2d,  TR::RealRegister::v15, TR::RealRegister::v0, "4ee0b80f"),
    std::make_tuple(TR::InstOpCode::vabs2d,  TR::RealRegister::v31, TR::RealRegister::v0, "4ee0b81f"),
    std::make_tuple(TR::InstOpCode::vabs2d,  TR::RealRegister::v0, TR::RealRegister::v15, "4ee0b9e0"),
    std::make_tuple(TR::InstOpCode::vabs2d,  TR::RealRegister::v0, TR::RealRegister::v31, "4ee0bbe0"),

    std::make_tuple(TR::InstOpCode::vfabs4s,  TR::RealRegister::v15, TR::RealRegister::v0, "4ea0f80f"),
    std::make_tuple(TR::InstOpCode::vfabs4s,  TR::RealRegister::v31, TR::RealRegister::v0, "4ea0f81f"),
    std::make_tuple(TR::InstOpCode::vfabs4s,  TR::RealRegister::v0, TR::RealRegister::v15, "4ea0f9e0"),
    std::make_tuple(TR::InstOpCode::vfabs4s,  TR::RealRegister::v0, TR::RealRegister::v31, "4ea0fbe0"),

    std::make_tuple(TR::InstOpCode::vfabs2d,  TR::RealRegister::v15, TR::RealRegister::v0, "4ee0f80f"),
    std::make_tuple(TR::InstOpCode::vfabs2d,  TR::RealRegister::v31, TR::RealRegister::v0, "4ee0f81f"),
    std::make_tuple(TR::InstOpCode::vfabs2d,  TR::RealRegister::v0, TR::RealRegister::v15, "4ee0f9e0"),
    std::make_tuple(TR::InstOpCode::vfabs2d,  TR::RealRegister::v0, TR::RealRegister::v31, "4ee0fbe0")
));

INSTANTIATE_TEST_CASE_P(VectorCMP1, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e208c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e208c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e208de0"),
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e208fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e608c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e608c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e608de0"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e608fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea08c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea08c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea08de0"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea08fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee08c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee08c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee08de0"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee08fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eef8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6eff8c00"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e203c0f"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e203c1f"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e203de0"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e203fe0"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e603c0f"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e603c1f"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e603de0"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e603fe0"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea03c0f"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea03c1f"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea03de0"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea03fe0"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee03c0f"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee03c1f"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee03de0"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee03fe0"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eef3c00"),
    std::make_tuple(TR::InstOpCode::vcmhs2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6eff3c00")
));
INSTANTIATE_TEST_CASE_P(VectorCMP2, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e203c0f"),
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e203c1f"),
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e203de0"),
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e203fe0"),
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f3c00"),
    std::make_tuple(TR::InstOpCode::vcmge16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f3c00"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e603c0f"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e603c1f"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e603de0"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e603fe0"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f3c00"),
    std::make_tuple(TR::InstOpCode::vcmge8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f3c00"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea03c0f"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea03c1f"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea03de0"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea03fe0"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf3c00"),
    std::make_tuple(TR::InstOpCode::vcmge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf3c00"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee03c0f"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee03c1f"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee03de0"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee03fe0"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eef3c00"),
    std::make_tuple(TR::InstOpCode::vcmge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4eff3c00"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20340f"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20341f"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e2035e0"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e2037e0"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f3400"),
    std::make_tuple(TR::InstOpCode::vcmhi16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f3400"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60340f"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60341f"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e6035e0"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e6037e0"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f3400"),
    std::make_tuple(TR::InstOpCode::vcmhi8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f3400"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0340f"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0341f"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea035e0"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea037e0"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf3400"),
    std::make_tuple(TR::InstOpCode::vcmhi4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf3400"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0340f"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0341f"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee035e0"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee037e0"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eef3400"),
    std::make_tuple(TR::InstOpCode::vcmhi2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6eff3400")
));
INSTANTIATE_TEST_CASE_P(VectorCMP3, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20340f"),
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20341f"),
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e2035e0"),
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e2037e0"),
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f3400"),
    std::make_tuple(TR::InstOpCode::vcmgt16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f3400"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60340f"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60341f"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e6035e0"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e6037e0"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f3400"),
    std::make_tuple(TR::InstOpCode::vcmgt8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f3400"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0340f"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0341f"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea035e0"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea037e0"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf3400"),
    std::make_tuple(TR::InstOpCode::vcmgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf3400"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0340f"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0341f"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee035e0"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee037e0"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eef3400"),
    std::make_tuple(TR::InstOpCode::vcmgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4eff3400"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e208c0f"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e208c1f"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e208de0"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e208fe0"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e608c0f"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e608c1f"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e608de0"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e608fe0"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea08c0f"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea08c1f"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea08de0"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea08fe0"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee08c0f"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee08c1f"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee08de0"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee08fe0"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eef8c00"),
    std::make_tuple(TR::InstOpCode::vcmtst2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4eff8c00")
));

INSTANTIATE_TEST_CASE_P(VectorCMP4, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e208c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e208c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e208de0"),
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e208fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e2f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq8b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e3f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e608c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e608c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e608de0"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e608fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e6f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq4h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e7f8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2ea08c0f"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2ea08c1f"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2ea08de0"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2ea08fe0"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2eaf8c00"),
    std::make_tuple(TR::InstOpCode::vcmeq2s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2ebf8c00")
));

INSTANTIATE_TEST_CASE_P(VectorCMPZERO1, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmeq16b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e20980f"),
    std::make_tuple(TR::InstOpCode::vcmeq16b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e20981f"),
    std::make_tuple(TR::InstOpCode::vcmeq16b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e2099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq16b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e209be0"),
    std::make_tuple(TR::InstOpCode::vcmeq8h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e60980f"),
    std::make_tuple(TR::InstOpCode::vcmeq8h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e60981f"),
    std::make_tuple(TR::InstOpCode::vcmeq8h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e6099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq8h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e609be0"),
    std::make_tuple(TR::InstOpCode::vcmeq4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0980f"),
    std::make_tuple(TR::InstOpCode::vcmeq4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0981f"),
    std::make_tuple(TR::InstOpCode::vcmeq4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea09be0"),
    std::make_tuple(TR::InstOpCode::vcmeq2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0980f"),
    std::make_tuple(TR::InstOpCode::vcmeq2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0981f"),
    std::make_tuple(TR::InstOpCode::vcmeq2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee09be0"),
    std::make_tuple(TR::InstOpCode::vcmge16b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6e20880f"),
    std::make_tuple(TR::InstOpCode::vcmge16b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6e20881f"),
    std::make_tuple(TR::InstOpCode::vcmge16b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6e2089e0"),
    std::make_tuple(TR::InstOpCode::vcmge16b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6e208be0"),
    std::make_tuple(TR::InstOpCode::vcmge8h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6e60880f"),
    std::make_tuple(TR::InstOpCode::vcmge8h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6e60881f"),
    std::make_tuple(TR::InstOpCode::vcmge8h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6e6089e0"),
    std::make_tuple(TR::InstOpCode::vcmge8h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6e608be0"),
    std::make_tuple(TR::InstOpCode::vcmge4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0880f"),
    std::make_tuple(TR::InstOpCode::vcmge4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0881f"),
    std::make_tuple(TR::InstOpCode::vcmge4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ea089e0"),
    std::make_tuple(TR::InstOpCode::vcmge4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ea08be0"),
    std::make_tuple(TR::InstOpCode::vcmge2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0880f"),
    std::make_tuple(TR::InstOpCode::vcmge2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0881f"),
    std::make_tuple(TR::InstOpCode::vcmge2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ee089e0"),
    std::make_tuple(TR::InstOpCode::vcmge2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ee08be0"),
    std::make_tuple(TR::InstOpCode::vcmgt16b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e20880f"),
    std::make_tuple(TR::InstOpCode::vcmgt16b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e20881f"),
    std::make_tuple(TR::InstOpCode::vcmgt16b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e2089e0"),
    std::make_tuple(TR::InstOpCode::vcmgt16b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e208be0"),
    std::make_tuple(TR::InstOpCode::vcmgt8h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e60880f"),
    std::make_tuple(TR::InstOpCode::vcmgt8h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e60881f"),
    std::make_tuple(TR::InstOpCode::vcmgt8h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e6089e0"),
    std::make_tuple(TR::InstOpCode::vcmgt8h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e608be0"),
    std::make_tuple(TR::InstOpCode::vcmgt4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0880f"),
    std::make_tuple(TR::InstOpCode::vcmgt4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0881f"),
    std::make_tuple(TR::InstOpCode::vcmgt4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea089e0"),
    std::make_tuple(TR::InstOpCode::vcmgt4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea08be0"),
    std::make_tuple(TR::InstOpCode::vcmgt2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0880f"),
    std::make_tuple(TR::InstOpCode::vcmgt2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0881f"),
    std::make_tuple(TR::InstOpCode::vcmgt2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee089e0"),
    std::make_tuple(TR::InstOpCode::vcmgt2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee08be0")
));

INSTANTIATE_TEST_CASE_P(VectorCMPZERO2, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmle16b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6e20980f"),
    std::make_tuple(TR::InstOpCode::vcmle16b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6e20981f"),
    std::make_tuple(TR::InstOpCode::vcmle16b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6e2099e0"),
    std::make_tuple(TR::InstOpCode::vcmle16b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6e209be0"),
    std::make_tuple(TR::InstOpCode::vcmle8h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6e60980f"),
    std::make_tuple(TR::InstOpCode::vcmle8h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6e60981f"),
    std::make_tuple(TR::InstOpCode::vcmle8h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6e6099e0"),
    std::make_tuple(TR::InstOpCode::vcmle8h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6e609be0"),
    std::make_tuple(TR::InstOpCode::vcmle4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0980f"),
    std::make_tuple(TR::InstOpCode::vcmle4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0981f"),
    std::make_tuple(TR::InstOpCode::vcmle4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ea099e0"),
    std::make_tuple(TR::InstOpCode::vcmle4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ea09be0"),
    std::make_tuple(TR::InstOpCode::vcmle2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0980f"),
    std::make_tuple(TR::InstOpCode::vcmle2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0981f"),
    std::make_tuple(TR::InstOpCode::vcmle2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ee099e0"),
    std::make_tuple(TR::InstOpCode::vcmle2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ee09be0"),
    std::make_tuple(TR::InstOpCode::vcmlt16b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e20a80f"),
    std::make_tuple(TR::InstOpCode::vcmlt16b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e20a81f"),
    std::make_tuple(TR::InstOpCode::vcmlt16b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e20a9e0"),
    std::make_tuple(TR::InstOpCode::vcmlt16b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e20abe0"),
    std::make_tuple(TR::InstOpCode::vcmlt8h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4e60a80f"),
    std::make_tuple(TR::InstOpCode::vcmlt8h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4e60a81f"),
    std::make_tuple(TR::InstOpCode::vcmlt8h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4e60a9e0"),
    std::make_tuple(TR::InstOpCode::vcmlt8h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4e60abe0"),
    std::make_tuple(TR::InstOpCode::vcmlt4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0a80f"),
    std::make_tuple(TR::InstOpCode::vcmlt4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0a81f"),
    std::make_tuple(TR::InstOpCode::vcmlt4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea0a9e0"),
    std::make_tuple(TR::InstOpCode::vcmlt4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea0abe0"),
    std::make_tuple(TR::InstOpCode::vcmlt2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0a80f"),
    std::make_tuple(TR::InstOpCode::vcmlt2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0a81f"),
    std::make_tuple(TR::InstOpCode::vcmlt2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee0a9e0"),
    std::make_tuple(TR::InstOpCode::vcmlt2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee0abe0")
));

INSTANTIATE_TEST_CASE_P(VectorCMPZERO3, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcmeq8b_zero, TR::RealRegister::v15, TR::RealRegister::v0, "0e20980f"),
    std::make_tuple(TR::InstOpCode::vcmeq8b_zero, TR::RealRegister::v31, TR::RealRegister::v0, "0e20981f"),
    std::make_tuple(TR::InstOpCode::vcmeq8b_zero, TR::RealRegister::v0, TR::RealRegister::v15, "0e2099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq8b_zero, TR::RealRegister::v0, TR::RealRegister::v31, "0e209be0"),
    std::make_tuple(TR::InstOpCode::vcmeq4h_zero, TR::RealRegister::v15, TR::RealRegister::v0, "0e60980f"),
    std::make_tuple(TR::InstOpCode::vcmeq4h_zero, TR::RealRegister::v31, TR::RealRegister::v0, "0e60981f"),
    std::make_tuple(TR::InstOpCode::vcmeq4h_zero, TR::RealRegister::v0, TR::RealRegister::v15, "0e6099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq4h_zero, TR::RealRegister::v0, TR::RealRegister::v31, "0e609be0"),
    std::make_tuple(TR::InstOpCode::vcmeq2s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "0ea0980f"),
    std::make_tuple(TR::InstOpCode::vcmeq2s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "0ea0981f"),
    std::make_tuple(TR::InstOpCode::vcmeq2s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "0ea099e0"),
    std::make_tuple(TR::InstOpCode::vcmeq2s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "0ea09be0")
));

INSTANTIATE_TEST_CASE_P(VectorFCMP0, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20e40f"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20e41f"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2fe400"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3fe400"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60e40f"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60e41f"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6fe400"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7fe400"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20e40f"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20e41f"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fe400"),
    std::make_tuple(TR::InstOpCode::vfcmge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fe400"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60e40f"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60e41f"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fe400"),
    std::make_tuple(TR::InstOpCode::vfcmge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fe400"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0e40f"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0e41f"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafe400"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfe400"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0e40f"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0e41f"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0e5e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0e7e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eefe400"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6effe400")
));

INSTANTIATE_TEST_CASE_P(VectorFCMP1, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20ec0f"),
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20ec1f"),
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20ede0"),
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20efe0"),
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fec00"),
    std::make_tuple(TR::InstOpCode::vfacge4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fec00"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60ec0f"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60ec1f"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60ede0"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60efe0"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fec00"),
    std::make_tuple(TR::InstOpCode::vfacge2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fec00"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0ec0f"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0ec1f"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0ede0"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0efe0"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafec00"),
    std::make_tuple(TR::InstOpCode::vfacgt4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfec00"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0ec0f"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0ec1f"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0ede0"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0efe0"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eefec00"),
    std::make_tuple(TR::InstOpCode::vfacgt2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6effec00")
));

INSTANTIATE_TEST_CASE_P(VectorFCMPZERO0, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfcmeq4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0d80f"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0d81f"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea0d9e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea0dbe0"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0d80f"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0d81f"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee0d9e0"),
    std::make_tuple(TR::InstOpCode::vfcmeq2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee0dbe0"),
    std::make_tuple(TR::InstOpCode::vfcmge4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0c80f"),
    std::make_tuple(TR::InstOpCode::vfcmge4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0c81f"),
    std::make_tuple(TR::InstOpCode::vfcmge4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ea0c9e0"),
    std::make_tuple(TR::InstOpCode::vfcmge4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ea0cbe0"),
    std::make_tuple(TR::InstOpCode::vfcmge2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0c80f"),
    std::make_tuple(TR::InstOpCode::vfcmge2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0c81f"),
    std::make_tuple(TR::InstOpCode::vfcmge2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ee0c9e0"),
    std::make_tuple(TR::InstOpCode::vfcmge2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ee0cbe0"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0c80f"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0c81f"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea0c9e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea0cbe0"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0c80f"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0c81f"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee0c9e0"),
    std::make_tuple(TR::InstOpCode::vfcmgt2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee0cbe0"),
    std::make_tuple(TR::InstOpCode::vfcmle4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0d80f"),
    std::make_tuple(TR::InstOpCode::vfcmle4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0d81f"),
    std::make_tuple(TR::InstOpCode::vfcmle4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ea0d9e0"),
    std::make_tuple(TR::InstOpCode::vfcmle4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ea0dbe0"),
    std::make_tuple(TR::InstOpCode::vfcmle2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0d80f"),
    std::make_tuple(TR::InstOpCode::vfcmle2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0d81f"),
    std::make_tuple(TR::InstOpCode::vfcmle2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "6ee0d9e0"),
    std::make_tuple(TR::InstOpCode::vfcmle2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "6ee0dbe0"),
    std::make_tuple(TR::InstOpCode::vfcmlt4s_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0e80f"),
    std::make_tuple(TR::InstOpCode::vfcmlt4s_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0e81f"),
    std::make_tuple(TR::InstOpCode::vfcmlt4s_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ea0e9e0"),
    std::make_tuple(TR::InstOpCode::vfcmlt4s_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ea0ebe0"),
    std::make_tuple(TR::InstOpCode::vfcmlt2d_zero, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0e80f"),
    std::make_tuple(TR::InstOpCode::vfcmlt2d_zero, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0e81f"),
    std::make_tuple(TR::InstOpCode::vfcmlt2d_zero, TR::RealRegister::v0, TR::RealRegister::v15, "4ee0e9e0"),
    std::make_tuple(TR::InstOpCode::vfcmlt2d_zero, TR::RealRegister::v0, TR::RealRegister::v31, "4ee0ebe0")
));

INSTANTIATE_TEST_CASE_P(VectorShiftLeftImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f09540f"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v15, TR::RealRegister::v0, 7, "4f0f540f"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f09541f"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v31, TR::RealRegister::v0, 7, "4f0f541f"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f0955e0"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v0, TR::RealRegister::v15, 7, "4f0f55e0"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f0957e0"),
    std::make_tuple(TR::InstOpCode::vshl16b, TR::RealRegister::v0, TR::RealRegister::v31, 7, "4f0f57e0"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f11540f"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v15, TR::RealRegister::v0, 15, "4f1f540f"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f11541f"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v31, TR::RealRegister::v0, 15, "4f1f541f"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f1155e0"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v0, TR::RealRegister::v15, 15, "4f1f55e0"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f1157e0"),
    std::make_tuple(TR::InstOpCode::vshl8h, TR::RealRegister::v0, TR::RealRegister::v31, 15, "4f1f57e0"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f21540f"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v15, TR::RealRegister::v0, 31, "4f3f540f"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f21541f"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v31, TR::RealRegister::v0, 31, "4f3f541f"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f2155e0"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v0, TR::RealRegister::v15, 31, "4f3f55e0"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f2157e0"),
    std::make_tuple(TR::InstOpCode::vshl4s, TR::RealRegister::v0, TR::RealRegister::v31, 31, "4f3f57e0"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f41540f"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v15, TR::RealRegister::v0, 63, "4f7f540f"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f41541f"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v31, TR::RealRegister::v0, 63, "4f7f541f"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f4155e0"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v0, TR::RealRegister::v15, 63, "4f7f55e0"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f4157e0"),
    std::make_tuple(TR::InstOpCode::vshl2d, TR::RealRegister::v0, TR::RealRegister::v31, 63, "4f7f57e0")
));

INSTANTIATE_TEST_CASE_P(VectorSignedShiftRightImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f0f040f"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v15, TR::RealRegister::v0, 7, "4f09040f"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f0f041f"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v31, TR::RealRegister::v0, 7, "4f09041f"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f0f05e0"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v0, TR::RealRegister::v15, 7, "4f0905e0"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f0f07e0"),
    std::make_tuple(TR::InstOpCode::vsshr16b, TR::RealRegister::v0, TR::RealRegister::v31, 7, "4f0907e0"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f1f040f"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v15, TR::RealRegister::v0, 15, "4f11040f"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f1f041f"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v31, TR::RealRegister::v0, 15, "4f11041f"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f1f05e0"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v0, TR::RealRegister::v15, 15, "4f1105e0"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f1f07e0"),
    std::make_tuple(TR::InstOpCode::vsshr8h, TR::RealRegister::v0, TR::RealRegister::v31, 15, "4f1107e0"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f3f040f"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v15, TR::RealRegister::v0, 31, "4f21040f"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f3f041f"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v31, TR::RealRegister::v0, 31, "4f21041f"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f3f05e0"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v0, TR::RealRegister::v15, 31, "4f2105e0"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f3f07e0"),
    std::make_tuple(TR::InstOpCode::vsshr4s, TR::RealRegister::v0, TR::RealRegister::v31, 31, "4f2107e0"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f7f040f"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v15, TR::RealRegister::v0, 63, "4f41040f"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f7f041f"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v31, TR::RealRegister::v0, 63, "4f41041f"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f7f05e0"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v0, TR::RealRegister::v15, 63, "4f4105e0"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f7f07e0"),
    std::make_tuple(TR::InstOpCode::vsshr2d, TR::RealRegister::v0, TR::RealRegister::v31, 63, "4f4107e0")
));

INSTANTIATE_TEST_CASE_P(VectorUnsignedShiftRightImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f0f040f"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v15, TR::RealRegister::v0, 7, "6f09040f"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f0f041f"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v31, TR::RealRegister::v0, 7, "6f09041f"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f0f05e0"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v0, TR::RealRegister::v15, 7, "6f0905e0"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f0f07e0"),
    std::make_tuple(TR::InstOpCode::vushr16b, TR::RealRegister::v0, TR::RealRegister::v31, 7, "6f0907e0"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f1f040f"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v15, TR::RealRegister::v0, 15, "6f11040f"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f1f041f"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v31, TR::RealRegister::v0, 15, "6f11041f"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f1f05e0"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v0, TR::RealRegister::v15, 15, "6f1105e0"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f1f07e0"),
    std::make_tuple(TR::InstOpCode::vushr8h, TR::RealRegister::v0, TR::RealRegister::v31, 15, "6f1107e0"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f3f040f"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v15, TR::RealRegister::v0, 31, "6f21040f"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f3f041f"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v31, TR::RealRegister::v0, 31, "6f21041f"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f3f05e0"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v0, TR::RealRegister::v15, 31, "6f2105e0"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f3f07e0"),
    std::make_tuple(TR::InstOpCode::vushr4s, TR::RealRegister::v0, TR::RealRegister::v31, 31, "6f2107e0"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f7f040f"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v15, TR::RealRegister::v0, 63, "6f41040f"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f7f041f"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v31, TR::RealRegister::v0, 63, "6f41041f"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f7f05e0"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v0, TR::RealRegister::v15, 63, "6f4105e0"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f7f07e0"),
    std::make_tuple(TR::InstOpCode::vushr2d, TR::RealRegister::v0, TR::RealRegister::v31, 63, "6f4107e0")
));

INSTANTIATE_TEST_CASE_P(VectorSignedShiftLeft, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20440f"),
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20441f"),
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e2045e0"),
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e2047e0"),
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2f4400"),
    std::make_tuple(TR::InstOpCode::vsshl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3f4400"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60440f"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60441f"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e6045e0"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e6047e0"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6f4400"),
    std::make_tuple(TR::InstOpCode::vsshl8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7f4400"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0440f"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0441f"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea045e0"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea047e0"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eaf4400"),
    std::make_tuple(TR::InstOpCode::vsshl4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebf4400"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0440f"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0441f"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee045e0"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee047e0"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eef4400"),
    std::make_tuple(TR::InstOpCode::vsshl2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4eff4400")
));

INSTANTIATE_TEST_CASE_P(VectorUnsignedShiftLeft, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20440f"),
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20441f"),
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e2045e0"),
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e2047e0"),
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f4400"),
    std::make_tuple(TR::InstOpCode::vushl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f4400"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60440f"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60441f"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e6045e0"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e6047e0"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f4400"),
    std::make_tuple(TR::InstOpCode::vushl8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f4400"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0440f"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0441f"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea045e0"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea047e0"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf4400"),
    std::make_tuple(TR::InstOpCode::vushl4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf4400"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0440f"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0441f"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee045e0"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee047e0"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eef4400"),
    std::make_tuple(TR::InstOpCode::vushl2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6eff4400")
));

INSTANTIATE_TEST_CASE_P(VectorSignedShiftLeftLongImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f09a40f"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v15, TR::RealRegister::v0, 7, "0f0fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f09a41f"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v31, TR::RealRegister::v0, 7, "0f0fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f09a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v0, TR::RealRegister::v15, 7, "0f0fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f09a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll_8h, TR::RealRegister::v0, TR::RealRegister::v31, 7, "0f0fa7e0"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f11a40f"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v15, TR::RealRegister::v0, 15, "0f1fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f11a41f"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v31, TR::RealRegister::v0, 15, "0f1fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f11a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v0, TR::RealRegister::v15, 15, "0f1fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f11a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll_4s, TR::RealRegister::v0, TR::RealRegister::v31, 15, "0f1fa7e0"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f21a40f"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v15, TR::RealRegister::v0, 31, "0f3fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f21a41f"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v31, TR::RealRegister::v0, 31, "0f3fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f21a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v0, TR::RealRegister::v15, 31, "0f3fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f21a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll_2d, TR::RealRegister::v0, TR::RealRegister::v31, 31, "0f3fa7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f09a40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 7, "4f0fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f09a41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 7, "4f0fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f09a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 7, "4f0fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f09a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 7, "4f0fa7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f11a40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 15, "4f1fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f11a41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 15, "4f1fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f11a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 15, "4f1fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f11a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 15, "4f1fa7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f21a40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v15, TR::RealRegister::v0, 31, "4f3fa40f"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f21a41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v31, TR::RealRegister::v0, 31, "4f3fa41f"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f21a5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v0, TR::RealRegister::v15, 31, "4f3fa5e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f21a7e0"),
    std::make_tuple(TR::InstOpCode::vsshll2_2d, TR::RealRegister::v0, TR::RealRegister::v31, 31, "4f3fa7e0")
));

INSTANTIATE_TEST_CASE_P(VectorUnsignedShiftLeftLongImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "2f09a40f"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v15, TR::RealRegister::v0, 7, "2f0fa40f"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "2f09a41f"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v31, TR::RealRegister::v0, 7, "2f0fa41f"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "2f09a5e0"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v0, TR::RealRegister::v15, 7, "2f0fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "2f09a7e0"),
    std::make_tuple(TR::InstOpCode::vushll_8h, TR::RealRegister::v0, TR::RealRegister::v31, 7, "2f0fa7e0"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "2f11a40f"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v15, TR::RealRegister::v0, 15, "2f1fa40f"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "2f11a41f"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v31, TR::RealRegister::v0, 15, "2f1fa41f"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "2f11a5e0"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v0, TR::RealRegister::v15, 15, "2f1fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "2f11a7e0"),
    std::make_tuple(TR::InstOpCode::vushll_4s, TR::RealRegister::v0, TR::RealRegister::v31, 15, "2f1fa7e0"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "2f21a40f"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v15, TR::RealRegister::v0, 31, "2f3fa40f"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "2f21a41f"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v31, TR::RealRegister::v0, 31, "2f3fa41f"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "2f21a5e0"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v0, TR::RealRegister::v15, 31, "2f3fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "2f21a7e0"),
    std::make_tuple(TR::InstOpCode::vushll_2d, TR::RealRegister::v0, TR::RealRegister::v31, 31, "2f3fa7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f09a40f"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 7, "6f0fa40f"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f09a41f"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 7, "6f0fa41f"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f09a5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 7, "6f0fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f09a7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 7, "6f0fa7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f11a40f"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 15, "6f1fa40f"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f11a41f"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 15, "6f1fa41f"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f11a5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 15, "6f1fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f11a7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 15, "6f1fa7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f21a40f"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v15, TR::RealRegister::v0, 31, "6f3fa40f"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f21a41f"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v31, TR::RealRegister::v0, 31, "6f3fa41f"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f21a5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v0, TR::RealRegister::v15, 31, "6f3fa5e0"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f21a7e0"),
    std::make_tuple(TR::InstOpCode::vushll2_2d, TR::RealRegister::v0, TR::RealRegister::v31, 31, "6f3fa7e0")
));

INSTANTIATE_TEST_CASE_P(VectorShiftLeftInsertImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f09540f"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v15, TR::RealRegister::v0, 7, "6f0f540f"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f09541f"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v31, TR::RealRegister::v0, 7, "6f0f541f"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f0955e0"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v0, TR::RealRegister::v15, 7, "6f0f55e0"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f0957e0"),
    std::make_tuple(TR::InstOpCode::vsli16b, TR::RealRegister::v0, TR::RealRegister::v31, 7, "6f0f57e0"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f11540f"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v15, TR::RealRegister::v0, 15, "6f1f540f"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f11541f"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v31, TR::RealRegister::v0, 15, "6f1f541f"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f1155e0"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v0, TR::RealRegister::v15, 15, "6f1f55e0"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f1157e0"),
    std::make_tuple(TR::InstOpCode::vsli8h, TR::RealRegister::v0, TR::RealRegister::v31, 15, "6f1f57e0"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f21540f"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v15, TR::RealRegister::v0, 31, "6f3f540f"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f21541f"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v31, TR::RealRegister::v0, 31, "6f3f541f"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f2155e0"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v0, TR::RealRegister::v15, 31, "6f3f55e0"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f2157e0"),
    std::make_tuple(TR::InstOpCode::vsli4s, TR::RealRegister::v0, TR::RealRegister::v31, 31, "6f3f57e0"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f41540f"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v15, TR::RealRegister::v0, 63, "6f7f540f"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f41541f"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v31, TR::RealRegister::v0, 63, "6f7f541f"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f4155e0"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v0, TR::RealRegister::v15, 63, "6f7f55e0"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f4157e0"),
    std::make_tuple(TR::InstOpCode::vsli2d, TR::RealRegister::v0, TR::RealRegister::v31, 63, "6f7f57e0")
));

INSTANTIATE_TEST_CASE_P(VectorShiftRightInsertImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f0f440f"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v15, TR::RealRegister::v0, 7, "6f09440f"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f0f441f"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v31, TR::RealRegister::v0, 7, "6f09441f"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f0f45e0"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v0, TR::RealRegister::v15, 7, "6f0945e0"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f0f47e0"),
    std::make_tuple(TR::InstOpCode::vsri16b, TR::RealRegister::v0, TR::RealRegister::v31, 7, "6f0947e0"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f1f440f"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v15, TR::RealRegister::v0, 15, "6f11440f"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f1f441f"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v31, TR::RealRegister::v0, 15, "6f11441f"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f1f45e0"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v0, TR::RealRegister::v15, 15, "6f1145e0"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f1f47e0"),
    std::make_tuple(TR::InstOpCode::vsri8h, TR::RealRegister::v0, TR::RealRegister::v31, 15, "6f1147e0"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f3f440f"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v15, TR::RealRegister::v0, 31, "6f21440f"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f3f441f"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v31, TR::RealRegister::v0, 31, "6f21441f"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f3f45e0"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v0, TR::RealRegister::v15, 31, "6f2145e0"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f3f47e0"),
    std::make_tuple(TR::InstOpCode::vsri4s, TR::RealRegister::v0, TR::RealRegister::v31, 31, "6f2147e0"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "6f7f440f"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v15, TR::RealRegister::v0, 63, "6f41440f"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "6f7f441f"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v31, TR::RealRegister::v0, 63, "6f41441f"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "6f7f45e0"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v0, TR::RealRegister::v15, 63, "6f4145e0"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "6f7f47e0"),
    std::make_tuple(TR::InstOpCode::vsri2d, TR::RealRegister::v0, TR::RealRegister::v31, 63, "6f4147e0")
));

INSTANTIATE_TEST_CASE_P(VectorShiftRightNarrowImmediate, ARM64VectorShiftImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f0f840f"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v15, TR::RealRegister::v0, 8, "0f08840f"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f0f841f"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v31, TR::RealRegister::v0, 8, "0f08841f"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f0f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v0, TR::RealRegister::v15, 8, "0f0885e0"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f0f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn_8b, TR::RealRegister::v0, TR::RealRegister::v31, 8, "0f0887e0"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f1f840f"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v15, TR::RealRegister::v0, 16, "0f10840f"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f1f841f"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v31, TR::RealRegister::v0, 16, "0f10841f"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f1f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v0, TR::RealRegister::v15, 16, "0f1085e0"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f1f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn_4h, TR::RealRegister::v0, TR::RealRegister::v31, 16, "0f1087e0"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "0f3f840f"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v15, TR::RealRegister::v0, 32, "0f20840f"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "0f3f841f"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v31, TR::RealRegister::v0, 32, "0f20841f"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "0f3f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v0, TR::RealRegister::v15, 32, "0f2085e0"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "0f3f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn_2s, TR::RealRegister::v0, TR::RealRegister::v31, 32, "0f2087e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f0f840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v15, TR::RealRegister::v0, 8, "4f08840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f0f841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v31, TR::RealRegister::v0, 8, "4f08841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f0f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v0, TR::RealRegister::v15, 8, "4f0885e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f0f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_16b, TR::RealRegister::v0, TR::RealRegister::v31, 8, "4f0887e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f1f840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v15, TR::RealRegister::v0, 16, "4f10840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f1f841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v31, TR::RealRegister::v0, 16, "4f10841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f1f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v0, TR::RealRegister::v15, 16, "4f1085e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f1f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_8h, TR::RealRegister::v0, TR::RealRegister::v31, 16, "4f1087e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4f3f840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v15, TR::RealRegister::v0, 32, "4f20840f"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4f3f841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v31, TR::RealRegister::v0, 32, "4f20841f"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4f3f85e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v0, TR::RealRegister::v15, 32, "4f2085e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4f3f87e0"),
    std::make_tuple(TR::InstOpCode::vshrn2_4s, TR::RealRegister::v0, TR::RealRegister::v31, 32, "4f2087e0")
));

INSTANTIATE_TEST_CASE_P(VectorShiftLeftLongByElementSize, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vshll_8h, TR::RealRegister::v15, TR::RealRegister::v0, "2e21380f"),
    std::make_tuple(TR::InstOpCode::vshll_8h, TR::RealRegister::v31, TR::RealRegister::v0, "2e21381f"),
    std::make_tuple(TR::InstOpCode::vshll_8h, TR::RealRegister::v0, TR::RealRegister::v15, "2e2139e0"),
    std::make_tuple(TR::InstOpCode::vshll_8h, TR::RealRegister::v0, TR::RealRegister::v31, "2e213be0"),
    std::make_tuple(TR::InstOpCode::vshll_4s, TR::RealRegister::v15, TR::RealRegister::v0, "2e61380f"),
    std::make_tuple(TR::InstOpCode::vshll_4s, TR::RealRegister::v31, TR::RealRegister::v0, "2e61381f"),
    std::make_tuple(TR::InstOpCode::vshll_4s, TR::RealRegister::v0, TR::RealRegister::v15, "2e6139e0"),
    std::make_tuple(TR::InstOpCode::vshll_4s, TR::RealRegister::v0, TR::RealRegister::v31, "2e613be0"),
    std::make_tuple(TR::InstOpCode::vshll_2d, TR::RealRegister::v15, TR::RealRegister::v0, "2ea1380f"),
    std::make_tuple(TR::InstOpCode::vshll_2d, TR::RealRegister::v31, TR::RealRegister::v0, "2ea1381f"),
    std::make_tuple(TR::InstOpCode::vshll_2d, TR::RealRegister::v0, TR::RealRegister::v15, "2ea139e0"),
    std::make_tuple(TR::InstOpCode::vshll_2d, TR::RealRegister::v0, TR::RealRegister::v31, "2ea13be0"),
    std::make_tuple(TR::InstOpCode::vshll2_8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e21380f"),
    std::make_tuple(TR::InstOpCode::vshll2_8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e21381f"),
    std::make_tuple(TR::InstOpCode::vshll2_8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e2139e0"),
    std::make_tuple(TR::InstOpCode::vshll2_8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e213be0"),
    std::make_tuple(TR::InstOpCode::vshll2_4s, TR::RealRegister::v15, TR::RealRegister::v0, "6e61380f"),
    std::make_tuple(TR::InstOpCode::vshll2_4s, TR::RealRegister::v31, TR::RealRegister::v0, "6e61381f"),
    std::make_tuple(TR::InstOpCode::vshll2_4s, TR::RealRegister::v0, TR::RealRegister::v15, "6e6139e0"),
    std::make_tuple(TR::InstOpCode::vshll2_4s, TR::RealRegister::v0, TR::RealRegister::v31, "6e613be0"),
    std::make_tuple(TR::InstOpCode::vshll2_2d, TR::RealRegister::v15, TR::RealRegister::v0, "6ea1380f"),
    std::make_tuple(TR::InstOpCode::vshll2_2d, TR::RealRegister::v31, TR::RealRegister::v0, "6ea1381f"),
    std::make_tuple(TR::InstOpCode::vshll2_2d, TR::RealRegister::v0, TR::RealRegister::v15, "6ea139e0"),
    std::make_tuple(TR::InstOpCode::vshll2_2d, TR::RealRegister::v0, TR::RealRegister::v31, "6ea13be0")
));

INSTANTIATE_TEST_CASE_P(VectorBitwise, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee01c0f"),
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee01c1f"),
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee01de0"),
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee01fe0"),
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eef1c00"),
    std::make_tuple(TR::InstOpCode::vbif16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6eff1c00"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea01c0f"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea01c1f"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea01de0"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea01fe0"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf1c00"),
    std::make_tuple(TR::InstOpCode::vbit16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf1c00"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e601c0f"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e601c1f"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e601de0"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e601fe0"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f1c00"),
    std::make_tuple(TR::InstOpCode::vbsl16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f1c00")
));

INSTANTIATE_TEST_CASE_P(VectorRev, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vrev16_16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e20180f"),
    std::make_tuple(TR::InstOpCode::vrev16_16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e20181f"),
    std::make_tuple(TR::InstOpCode::vrev16_16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e2019e0"),
    std::make_tuple(TR::InstOpCode::vrev16_16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e201be0"),
    std::make_tuple(TR::InstOpCode::vrev32_16b, TR::RealRegister::v15, TR::RealRegister::v0, "6e20080f"),
    std::make_tuple(TR::InstOpCode::vrev32_16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e20081f"),
    std::make_tuple(TR::InstOpCode::vrev32_16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e2009e0"),
    std::make_tuple(TR::InstOpCode::vrev32_16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e200be0"),
    std::make_tuple(TR::InstOpCode::vrev32_8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e60080f"),
    std::make_tuple(TR::InstOpCode::vrev32_8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e60081f"),
    std::make_tuple(TR::InstOpCode::vrev32_8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e6009e0"),
    std::make_tuple(TR::InstOpCode::vrev32_8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e600be0"),
    std::make_tuple(TR::InstOpCode::vrev64_16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e20080f"),
    std::make_tuple(TR::InstOpCode::vrev64_16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e20081f"),
    std::make_tuple(TR::InstOpCode::vrev64_16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e2009e0"),
    std::make_tuple(TR::InstOpCode::vrev64_16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e200be0"),
    std::make_tuple(TR::InstOpCode::vrev64_8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e60080f"),
    std::make_tuple(TR::InstOpCode::vrev64_8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e60081f"),
    std::make_tuple(TR::InstOpCode::vrev64_8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e6009e0"),
    std::make_tuple(TR::InstOpCode::vrev64_8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e600be0"),
    std::make_tuple(TR::InstOpCode::vrev64_4s, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0080f"),
    std::make_tuple(TR::InstOpCode::vrev64_4s, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0081f"),
    std::make_tuple(TR::InstOpCode::vrev64_4s, TR::RealRegister::v0, TR::RealRegister::v15, "4ea009e0"),
    std::make_tuple(TR::InstOpCode::vrev64_4s, TR::RealRegister::v0, TR::RealRegister::v31, "4ea00be0")
));

INSTANTIATE_TEST_CASE_P(VectorZip1, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e00380f"),
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e00381f"),
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e0039e0"),
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e003be0"),
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e0f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e1f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e40380f"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e40381f"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e4039e0"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e403be0"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e4f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e5f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e80380f"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e80381f"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e8039e0"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e803be0"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e8f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e9f3800"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0380f"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0381f"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ec039e0"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ec03be0"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4ecf3800"),
    std::make_tuple(TR::InstOpCode::vzip1_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4edf3800")
));

INSTANTIATE_TEST_CASE_P(VectorZip2, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e00780f"),
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e00781f"),
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e0079e0"),
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e007be0"),
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e0f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e1f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e40780f"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e40781f"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e4079e0"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e407be0"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e4f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e5f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e80780f"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e80781f"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e8079e0"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e807be0"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e8f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e9f7800"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0780f"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0781f"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ec079e0"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ec07be0"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4ecf7800"),
    std::make_tuple(TR::InstOpCode::vzip2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4edf7800")
));

INSTANTIATE_TEST_CASE_P(VectorUnzip1, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e00180f"),
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e00181f"),
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e0019e0"),
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e001be0"),
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e0f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e1f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e40180f"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e40181f"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e4019e0"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e401be0"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e4f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e5f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e80180f"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e80181f"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e8019e0"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e801be0"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e8f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e9f1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0180f"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0181f"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ec019e0"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ec01be0"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4ecf1800"),
    std::make_tuple(TR::InstOpCode::vuzp1_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4edf1800")
));

INSTANTIATE_TEST_CASE_P(VectorUnzip2, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e00580f"),
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e00581f"),
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e0059e0"),
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e005be0"),
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e0f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e1f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e40580f"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e40581f"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e4059e0"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e405be0"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e4f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e5f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e80580f"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e80581f"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e8059e0"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e805be0"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e8f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e9f5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0580f"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ec0581f"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ec059e0"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ec05be0"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4ecf5800"),
    std::make_tuple(TR::InstOpCode::vuzp2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4edf5800")
));

INSTANTIATE_TEST_CASE_P(VectorUMLAL, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e20800f"),
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e20801f"),
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e2081e0"),
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e2083e0"),
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e2f8000"),
    std::make_tuple(TR::InstOpCode::vumlal_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e3f8000"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e60800f"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e60801f"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e6081e0"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e6083e0"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e6f8000"),
    std::make_tuple(TR::InstOpCode::vumlal_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e7f8000"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0800f"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0801f"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2ea081e0"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2ea083e0"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2eaf8000"),
    std::make_tuple(TR::InstOpCode::vumlal_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2ebf8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20800f"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20801f"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e2081e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e2083e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2f8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3f8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60800f"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60801f"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e6081e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e6083e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6f8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7f8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0800f"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0801f"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea081e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea083e0"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaf8000"),
    std::make_tuple(TR::InstOpCode::vumlal2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebf8000")
));

INSTANTIATE_TEST_CASE_P(VectorUMLSL, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e20a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e20a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e20a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e20a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e2fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e3fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e60a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e60a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e60a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e60a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e6fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e7fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2ea0a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2ea0a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2eafa000"),
    std::make_tuple(TR::InstOpCode::vumlsl_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2ebfa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0a00f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0a01f"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0a1e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0a3e0"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafa000"),
    std::make_tuple(TR::InstOpCode::vumlsl2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfa000")
));

INSTANTIATE_TEST_CASE_P(VectorUMULL, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e20c00f"),
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e20c01f"),
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e20c1e0"),
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e20c3e0"),
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e2fc000"),
    std::make_tuple(TR::InstOpCode::vumull_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e3fc000"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2e60c00f"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2e60c01f"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2e60c1e0"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2e60c3e0"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2e6fc000"),
    std::make_tuple(TR::InstOpCode::vumull_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2e7fc000"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0c00f"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "2ea0c01f"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "2ea0c1e0"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "2ea0c3e0"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "2eafc000"),
    std::make_tuple(TR::InstOpCode::vumull_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "2ebfc000"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20c00f"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20c01f"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20c1e0"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20c3e0"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fc000"),
    std::make_tuple(TR::InstOpCode::vumull2_8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fc000"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60c00f"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60c01f"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60c1e0"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60c3e0"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fc000"),
    std::make_tuple(TR::InstOpCode::vumull2_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fc000"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0c00f"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0c01f"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0c1e0"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0c3e0"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafc000"),
    std::make_tuple(TR::InstOpCode::vumull2_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfc000")
));

INSTANTIATE_TEST_CASE_P(VectorSplatsImm1, ARM64VectorSplatsImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int8,               0, "4f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int8,            0x55, "4f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int8,            0xff, "4f07e7e0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int8,              0, "4f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int8,           0x55, "4f02e6bf"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int8,           0xff, "4f07e7ff"),

    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int16,            0x55, "4f0286a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int16,            0xff, "4f0787e0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int16,           0x55, "4f0286bf"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int16,           0xff, "4f0787ff"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int16,          0x5500, "4f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int16,          0xff00, "4f07a7e0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int16,         0x5500, "4f02a6bf"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int16,         0xff00, "4f07a7ff"),

    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int16,              0, "4f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int16,         0x5555, "4f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int16,         0xffff, "4f07e7e0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int16,             0, "4f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int16,        0x5555, "4f02e6bf"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int16,        0xffff, "4f07e7ff"),

    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v0, TR::Int16,          0xaaff, "6f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v0, TR::Int16,          0x55ff, "6f05a540"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v31, TR::Int16,         0xaaff, "6f02a6bf"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v31, TR::Int16,         0x55ff, "6f05a55f"),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int16,              0x0ff0, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int16,              0xf00f, "")
));

INSTANTIATE_TEST_CASE_P(VectorSplatsImm2, ARM64VectorSplatsImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,            0x55, "4f0206a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,            0xff, "4f0707e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,           0x55, "4f0206bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,           0xff, "4f0707ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,          0x5500, "4f0226a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,          0xff00, "4f0727e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,        0x550000, "4f0246a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,        0xff0000, "4f0747e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,      0x55000000, "4f0266a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int32,      0xff000000, "4f0767e0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,         0x5500, "4f0226bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,         0xff00, "4f0727ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,       0x550000, "4f0246bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,       0xff0000, "4f0747ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,     0x55000000, "4f0266bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v31, TR::Int32,     0xff000000, "4f0767ff"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v0, TR::Int32,      0x55ff, "4f02c6a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v0, TR::Int32,      0xaaff, "4f05c540"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v0, TR::Int32,    0x55ffff, "4f02d6a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v0, TR::Int32,    0xaaffff, "4f05d540"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v31, TR::Int32,     0x55ff, "4f02c6bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v31, TR::Int32,     0xaaff, "4f05c55f"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v31, TR::Int32,   0x55ffff, "4f02d6bf"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v31, TR::Int32,   0xaaffff, "4f05d55f"),

    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int32,             0, "4f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int32,              0, "4f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int32,     0x55555555, "4f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int32,      0x00550055, "4f0286a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int32,      0x00ff00ff, "4f0787e0"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v0, TR::Int32,      0xaaffaaff, "6f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int32,     0x55005500, "4f02a6bf"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v31, TR::Int32,     0xff00ff00, "4f07a7ff"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v31, TR::Int32,     0xffaaffaa, "6f0286bf"),

    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v0, TR::Int32,      0x00ffff00, "6f03e4c0"),
    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v31, TR::Int32,     0xff0000ff, "6f04e73f")
));

INSTANTIATE_TEST_CASE_P(VectorSplatsImm3, ARM64VectorSplatsImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0xffffaaff, "6f0226a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0xffff55ff, "6f052540"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0xffaaffff, "6f0246a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0xff55ffff, "6f054540"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0xaaffffff, "6f0266a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v0, TR::Int32,      0x55ffffff, "6f056540"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0xffffaaff, "6f0226bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0xffff55ff, "6f05255f"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0xffaaffff, "6f0246bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0xff55ffff, "6f05455f"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0xaaffffff, "6f0266bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int32,     0x55ffffff, "6f05655f"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v0, TR::Int32,  0xffffaa00, "6f02c6a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v0, TR::Int32,  0xffff5500, "6f05c540"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v0, TR::Int32,  0xffaa0000, "6f02d6a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v0, TR::Int32,  0xff550000, "6f05d540"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v31, TR::Int32, 0xffffaa00, "6f02c6bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v31, TR::Int32, 0xffff5500, "6f05c55f"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v31, TR::Int32, 0xffaa0000, "6f02d6bf"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v31, TR::Int32, 0xff550000, "6f05d55f"),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,              0x0ff0, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,            0x0ff000, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,            0xf0000f, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,          0x0f00f000, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,          0x0ff00000, ""),
    std::make_tuple(TR::InstOpCode::bad, TR::RealRegister::v0, TR::Int32,          0xf00ff00f, "")
));

INSTANTIATE_TEST_CASE_P(VectorSplatsImm4, ARM64VectorSplatsImmediateEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v0, TR::Int64,      0xff00ff00ffff0000LL, "6f05e580"),
    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v0, TR::Int64,      0x00ffff0000ff00ffLL, "6f03e4a0"),
    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v31, TR::Int64,     0xff00ff00ff0000ffLL, "6f05e53f"),
    std::make_tuple(TR::InstOpCode::vmovi2d, TR::RealRegister::v31, TR::Int64,     0xff0000ff00ff00ffLL, "6f04e6bf"),

    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int64,                        0, "4f00e400"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int64,                       0, "4f00e41f"),
    std::make_tuple(TR::InstOpCode::vmovi16b,TR::RealRegister::v0, TR::Int64,      0x5555555555555555LL, "4f02e6a0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v0, TR::Int64,     0xffffffffffffffffLL, "4f07e7e0"),
    std::make_tuple(TR::InstOpCode::vmovi16b, TR::RealRegister::v31, TR::Int64,    0xffffffffffffffffLL, "4f07e7ff"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int64,      0x0055005500550055LL, "4f0286a0"),
    std::make_tuple(TR::InstOpCode::vmovi8h, TR::RealRegister::v0, TR::Int64,      0x00ff00ff00ff00ffLL, "4f0787e0"),
    std::make_tuple(TR::InstOpCode::vmvni8h, TR::RealRegister::v0, TR::Int64,      0xaaffaaffaaffaaffLL, "6f02a6a0"),
    std::make_tuple(TR::InstOpCode::vmovi4s, TR::RealRegister::v0, TR::Int64,      0x0055000000550000LL, "4f0246a0"),
    std::make_tuple(TR::InstOpCode::vmvni4s, TR::RealRegister::v31, TR::Int64,     0xffff55ffffff55ffLL, "6f05255f"),
    std::make_tuple(TR::InstOpCode::vmovi4s_one, TR::RealRegister::v0, TR::Int64,  0x00aaffff00aaffffLL, "4f05d540"),
    std::make_tuple(TR::InstOpCode::vmvni4s_one, TR::RealRegister::v31, TR::Int64, 0xffaa0000ffaa0000LL, "6f02d6bf"),

    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                         0x01, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                       0x0055, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                       0x01ff, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                       0x5500, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                      0x1ff00, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                     0x550000, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                    0x1ff0000, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                   0x55000000, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,                0x1ff000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,               0x5500000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,              0x1ff00000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,             0x550000000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,            0x1ff0000000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,           0x55000000000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,          0x1ff000000000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x5500000000000000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x00000ff000000ff0LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x000ff000000ff000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x00f0000f00f0000fLL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x0f00f0000f00f000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0x0ff000000ff00000LL, ""),
    std::make_tuple(TR::InstOpCode::bad,  TR::RealRegister::v0, TR::Int64,         0xf00ff00ff00ff00fLL, "")
));

INSTANTIATE_TEST_CASE_P(DUP, ARM64VectorDupElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4e0105e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4e0305e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v15, 15, "4e1f05e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v31, 0, "4e0107e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4e0307e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v0, TR::RealRegister::v31, 15, "4e1f07e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4e0205e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4e0605e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v15, 7, "4e1e05e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v31, 0, "4e0207e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4e0607e0"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v0, TR::RealRegister::v31, 7, "4e1e07e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4e0405e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4e0c05e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v15, 3, "4e1c05e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v31, 0, "4e0407e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4e0c07e0"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v0, TR::RealRegister::v31, 3, "4e1c07e0"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4e0805e0"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4e1805e0"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v0, TR::RealRegister::v31, 0, "4e0807e0"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4e1807e0"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4e01040f"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4e03040f"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v15, TR::RealRegister::v0, 15, "4e1f040f"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v31, TR::RealRegister::v0, 0, "4e01041f"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4e03041f"),
    std::make_tuple(TR::InstOpCode::vdupe16b, TR::RealRegister::v31, TR::RealRegister::v0, 15, "4e1f041f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4e02040f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4e06040f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v15, TR::RealRegister::v0, 7, "4e1e040f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v31, TR::RealRegister::v0, 0, "4e02041f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4e06041f"),
    std::make_tuple(TR::InstOpCode::vdupe8h, TR::RealRegister::v31, TR::RealRegister::v0, 7, "4e1e041f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4e04040f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4e0c040f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v15, TR::RealRegister::v0, 3, "4e1c040f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v31, TR::RealRegister::v0, 0, "4e04041f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4e0c041f"),
    std::make_tuple(TR::InstOpCode::vdupe4s, TR::RealRegister::v31, TR::RealRegister::v0, 3, "4e1c041f"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4e08040f"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4e18040f"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v31, TR::RealRegister::v0, 0, "4e08041f"),
    std::make_tuple(TR::InstOpCode::vdupe2d, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4e18041f")
));

INSTANTIATE_TEST_CASE_P(SMOV1, ARM64MovVectorElementToGPREncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v15, 0, "0e012de0"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v15, 1, "0e032de0"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v15, 15, "0e1f2de0"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v31, 0, "0e012fe0"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v31, 1, "0e032fe0"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x0, TR::RealRegister::v31, 15, "0e1f2fe0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v15, 0, "0e022de0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v15, 1, "0e062de0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v15, 7, "0e1e2de0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v31, 0, "0e022fe0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v31, 1, "0e062fe0"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x0, TR::RealRegister::v31, 7, "0e1e2fe0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v15, 0, "4e012de0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v15, 1, "4e032de0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v15, 15, "4e1f2de0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v31, 0, "4e012fe0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v31, 1, "4e032fe0"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x0, TR::RealRegister::v31, 15, "4e1f2fe0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v15, 0, "4e022de0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v15, 1, "4e062de0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v15, 7, "4e1e2de0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v31, 0, "4e022fe0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v31, 1, "4e062fe0"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x0, TR::RealRegister::v31, 7, "4e1e2fe0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v15, 0, "4e042de0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v15, 1, "4e0c2de0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v15, 3, "4e1c2de0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v31, 0, "4e042fe0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v31, 1, "4e0c2fe0"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x0, TR::RealRegister::v31, 3, "4e1c2fe0")
));

INSTANTIATE_TEST_CASE_P(SMOV2, ARM64MovVectorElementToGPREncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x3, TR::RealRegister::v0, 0, "0e012c03"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x3, TR::RealRegister::v0, 1, "0e032c03"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x3, TR::RealRegister::v0, 15, "0e1f2c03"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x28, TR::RealRegister::v0, 0, "0e012c1c"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x28, TR::RealRegister::v0, 1, "0e032c1c"),
    std::make_tuple(TR::InstOpCode::smovwb, TR::RealRegister::x28, TR::RealRegister::v0, 15, "0e1f2c1c"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x3, TR::RealRegister::v0, 0, "0e022c03"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x3, TR::RealRegister::v0, 1, "0e062c03"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x3, TR::RealRegister::v0, 7, "0e1e2c03"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x28, TR::RealRegister::v0, 0, "0e022c1c"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x28, TR::RealRegister::v0, 1, "0e062c1c"),
    std::make_tuple(TR::InstOpCode::smovwh, TR::RealRegister::x28, TR::RealRegister::v0, 7, "0e1e2c1c"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x3, TR::RealRegister::v0, 0, "4e012c03"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x3, TR::RealRegister::v0, 1, "4e032c03"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x3, TR::RealRegister::v0, 15, "4e1f2c03"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x15, TR::RealRegister::v0, 0, "4e012c0f"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x15, TR::RealRegister::v0, 1, "4e032c0f"),
    std::make_tuple(TR::InstOpCode::smovxb, TR::RealRegister::x15, TR::RealRegister::v0, 15, "4e1f2c0f"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x3, TR::RealRegister::v0, 0, "4e022c03"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x3, TR::RealRegister::v0, 1, "4e062c03"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x3, TR::RealRegister::v0, 7, "4e1e2c03"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x15, TR::RealRegister::v0, 0, "4e022c0f"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x15, TR::RealRegister::v0, 1, "4e062c0f"),
    std::make_tuple(TR::InstOpCode::smovxh, TR::RealRegister::x15, TR::RealRegister::v0, 7, "4e1e2c0f"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x3, TR::RealRegister::v0, 0, "4e042c03"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x3, TR::RealRegister::v0, 1, "4e0c2c03"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x3, TR::RealRegister::v0, 3, "4e1c2c03"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x15, TR::RealRegister::v0, 0, "4e042c0f"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x15, TR::RealRegister::v0, 1, "4e0c2c0f"),
    std::make_tuple(TR::InstOpCode::smovxs, TR::RealRegister::x15, TR::RealRegister::v0, 3, "4e1c2c0f")
));

INSTANTIATE_TEST_CASE_P(UMOV, ARM64MovVectorElementToGPREncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v15, 0, "0e013de0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v15, 1, "0e033de0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v15, 15, "0e1f3de0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v31, 0, "0e013fe0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v31, 1, "0e033fe0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x0, TR::RealRegister::v31, 15, "0e1f3fe0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v15, 0, "0e023de0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v15, 1, "0e063de0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v15, 7, "0e1e3de0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v31, 0, "0e023fe0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v31, 1, "0e063fe0"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x0, TR::RealRegister::v31, 7, "0e1e3fe0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v15, 0, "0e043de0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v15, 1, "0e0c3de0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v15, 3, "0e1c3de0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v31, 0, "0e043fe0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v31, 1, "0e0c3fe0"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x0, TR::RealRegister::v31, 3, "0e1c3fe0"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x0, TR::RealRegister::v15, 0, "4e083de0"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x0, TR::RealRegister::v15, 1, "4e183de0"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x0, TR::RealRegister::v31, 0, "4e083fe0"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x0, TR::RealRegister::v31, 1, "4e183fe0"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x3, TR::RealRegister::v0, 0, "0e013c03"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x3, TR::RealRegister::v0, 1, "0e033c03"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x3, TR::RealRegister::v0, 15, "0e1f3c03"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x28, TR::RealRegister::v0, 0, "0e013c1c"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x28, TR::RealRegister::v0, 1, "0e033c1c"),
    std::make_tuple(TR::InstOpCode::umovwb, TR::RealRegister::x28, TR::RealRegister::v0, 15, "0e1f3c1c"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x3, TR::RealRegister::v0, 0, "0e023c03"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x3, TR::RealRegister::v0, 1, "0e063c03"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x3, TR::RealRegister::v0, 7, "0e1e3c03"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x28, TR::RealRegister::v0, 0, "0e023c1c"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x28, TR::RealRegister::v0, 1, "0e063c1c"),
    std::make_tuple(TR::InstOpCode::umovwh, TR::RealRegister::x28, TR::RealRegister::v0, 7, "0e1e3c1c"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x3, TR::RealRegister::v0, 0, "0e043c03"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x3, TR::RealRegister::v0, 1, "0e0c3c03"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x3, TR::RealRegister::v0, 3, "0e1c3c03"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x15, TR::RealRegister::v0, 0, "0e043c0f"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x15, TR::RealRegister::v0, 1, "0e0c3c0f"),
    std::make_tuple(TR::InstOpCode::umovws, TR::RealRegister::x15, TR::RealRegister::v0, 3, "0e1c3c0f"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x3, TR::RealRegister::v0, 0, "4e083c03"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x3, TR::RealRegister::v0, 1, "4e183c03"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x15, TR::RealRegister::v0, 0, "4e083c0f"),
    std::make_tuple(TR::InstOpCode::umovxd, TR::RealRegister::x15, TR::RealRegister::v0, 1, "4e183c0f")
));

INSTANTIATE_TEST_CASE_P(INS, ARM64MovGPRToVectorElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x3, 0, "4e011c60"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x3, 1, "4e031c60"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x3, 15, "4e1f1c60"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x28, 0, "4e011f80"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x28, 1, "4e031f80"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v0, TR::RealRegister::x28, 15, "4e1f1f80"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x3, 0, "4e021c60"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x3, 1, "4e061c60"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x3, 7, "4e1e1c60"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x28, 0, "4e021f80"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x28, 1, "4e061f80"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v0, TR::RealRegister::x28, 7, "4e1e1f80"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x3, 0, "4e041c60"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x3, 1, "4e0c1c60"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x3, 3, "4e1c1c60"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x15, 0, "4e041de0"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x15, 1, "4e0c1de0"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v0, TR::RealRegister::x15, 3, "4e1c1de0"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v0, TR::RealRegister::x3, 0, "4e081c60"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v0, TR::RealRegister::x3, 1, "4e181c60"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v0, TR::RealRegister::x15, 0, "4e081de0"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v0, TR::RealRegister::x15, 1, "4e181de0"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v15, TR::RealRegister::x0, 0, "4e011c0f"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v15, TR::RealRegister::x0, 1, "4e031c0f"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v15, TR::RealRegister::x0, 15, "4e1f1c0f"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v31, TR::RealRegister::x0, 0, "4e011c1f"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v31, TR::RealRegister::x0, 1, "4e031c1f"),
    std::make_tuple(TR::InstOpCode::vinswb, TR::RealRegister::v31, TR::RealRegister::x0, 15, "4e1f1c1f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v15, TR::RealRegister::x0, 0, "4e021c0f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v15, TR::RealRegister::x0, 1, "4e061c0f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v15, TR::RealRegister::x0, 7, "4e1e1c0f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v31, TR::RealRegister::x0, 0, "4e021c1f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v31, TR::RealRegister::x0, 1, "4e061c1f"),
    std::make_tuple(TR::InstOpCode::vinswh, TR::RealRegister::v31, TR::RealRegister::x0, 7, "4e1e1c1f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v15, TR::RealRegister::x0, 0, "4e041c0f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v15, TR::RealRegister::x0, 1, "4e0c1c0f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v15, TR::RealRegister::x0, 3, "4e1c1c0f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v31, TR::RealRegister::x0, 0, "4e041c1f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v31, TR::RealRegister::x0, 1, "4e0c1c1f"),
    std::make_tuple(TR::InstOpCode::vinsws, TR::RealRegister::v31, TR::RealRegister::x0, 3, "4e1c1c1f"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v15, TR::RealRegister::x0, 0, "4e081c0f"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v15, TR::RealRegister::x0, 1, "4e181c0f"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v31, TR::RealRegister::x0, 0, "4e081c1f"),
    std::make_tuple(TR::InstOpCode::vinsxd, TR::RealRegister::v31, TR::RealRegister::x0, 1, "4e181c1f")
));

INSTANTIATE_TEST_CASE_P(INS1, ARM64MovVectorElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v15, 0, 1, "6e010de0"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v15, 1, 15, "6e037de0"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v15, 15, 0, "6e1f05e0"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v31, 0, 1, "6e010fe0"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v31, 1, 15, "6e037fe0"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v0, TR::RealRegister::v31, 15, 0, "6e1f07e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v15, 0, 1, "6e0215e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v15, 1, 7, "6e0675e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v15, 7, 0, "6e1e05e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v31, 0, 1, "6e0217e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v31, 1, 7, "6e0677e0"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v0, TR::RealRegister::v31, 7, 0, "6e1e07e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v15, 0, 1, "6e0425e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v15, 1, 3, "6e0c65e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v15, 3, 0, "6e1c05e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v31, 0, 1, "6e0427e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v31, 1, 3, "6e0c67e0"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v0, TR::RealRegister::v31, 3, 0, "6e1c07e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v15, 0, 0, "6e0805e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v15, 1, 0, "6e1805e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v15, 0, 1, "6e0845e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v15, 1, 1, "6e1845e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v31, 0, 0, "6e0807e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v31, 1, 0, "6e1807e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v31, 0, 1, "6e0847e0"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v0, TR::RealRegister::v31, 1, 1, "6e1847e0")
));

INSTANTIATE_TEST_CASE_P(INS2, ARM64MovVectorElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v15, TR::RealRegister::v0, 0, 1, "6e010c0f"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v15, TR::RealRegister::v0, 1, 15, "6e037c0f"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v15, TR::RealRegister::v0, 15, 0, "6e1f040f"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v31, TR::RealRegister::v0, 0, 1, "6e010c1f"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v31, TR::RealRegister::v0, 1, 15, "6e037c1f"),
    std::make_tuple(TR::InstOpCode::vinseb, TR::RealRegister::v31, TR::RealRegister::v0, 15, 0, "6e1f041f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v15, TR::RealRegister::v0, 0, 1, "6e02140f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v15, TR::RealRegister::v0, 1, 7, "6e06740f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v15, TR::RealRegister::v0, 7, 0, "6e1e040f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v31, TR::RealRegister::v0, 0, 1, "6e02141f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v31, TR::RealRegister::v0, 1, 7, "6e06741f"),
    std::make_tuple(TR::InstOpCode::vinseh, TR::RealRegister::v31, TR::RealRegister::v0, 7, 0, "6e1e041f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v15, TR::RealRegister::v0, 0, 1, "6e04240f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v15, TR::RealRegister::v0, 1, 3, "6e0c640f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v15, TR::RealRegister::v0, 3, 0, "6e1c040f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v31, TR::RealRegister::v0, 0, 1, "6e04241f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v31, TR::RealRegister::v0, 1, 3, "6e0c641f"),
    std::make_tuple(TR::InstOpCode::vinses, TR::RealRegister::v31, TR::RealRegister::v0, 3, 0, "6e1c041f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v15, TR::RealRegister::v0, 0, 0, "6e08040f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v15, TR::RealRegister::v0, 1, 0, "6e18040f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v15, TR::RealRegister::v0, 0, 1, "6e08440f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v15, TR::RealRegister::v0, 1, 1, "6e18440f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v31, TR::RealRegister::v0, 0, 0, "6e08041f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v31, TR::RealRegister::v0, 1, 0, "6e18041f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v31, TR::RealRegister::v0, 0, 1, "6e08441f"),
    std::make_tuple(TR::InstOpCode::vinsed, TR::RealRegister::v31, TR::RealRegister::v0, 1, 1, "6e18441f")
));

INSTANTIATE_TEST_CASE_P(VectorADDV, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vaddv16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e31b9e0"),
    std::make_tuple(TR::InstOpCode::vaddv8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e71b9e0"),
    std::make_tuple(TR::InstOpCode::vaddv4s, TR::RealRegister::v0, TR::RealRegister::v15, "4eb1b9e0"),
    std::make_tuple(TR::InstOpCode::vaddv16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e31bbe0"),
    std::make_tuple(TR::InstOpCode::vaddv8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e71bbe0"),
    std::make_tuple(TR::InstOpCode::vaddv4s, TR::RealRegister::v0, TR::RealRegister::v31, "4eb1bbe0"),
    std::make_tuple(TR::InstOpCode::vaddv16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e31b80f"),
    std::make_tuple(TR::InstOpCode::vaddv8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e71b80f"),
    std::make_tuple(TR::InstOpCode::vaddv4s, TR::RealRegister::v15, TR::RealRegister::v0, "4eb1b80f"),
    std::make_tuple(TR::InstOpCode::vaddv16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e31b81f"),
    std::make_tuple(TR::InstOpCode::vaddv8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e71b81f"),
    std::make_tuple(TR::InstOpCode::vaddv4s, TR::RealRegister::v31, TR::RealRegister::v0, "4eb1b81f")
));

INSTANTIATE_TEST_CASE_P(VectorSADDLV, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsaddlv8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e3039e0"),
    std::make_tuple(TR::InstOpCode::vsaddlv4s, TR::RealRegister::v0, TR::RealRegister::v15, "4e7039e0"),
    std::make_tuple(TR::InstOpCode::vsaddlv2d, TR::RealRegister::v0, TR::RealRegister::v15, "4eb039e0"),
    std::make_tuple(TR::InstOpCode::vsaddlv8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e303be0"),
    std::make_tuple(TR::InstOpCode::vsaddlv4s, TR::RealRegister::v0, TR::RealRegister::v31, "4e703be0"),
    std::make_tuple(TR::InstOpCode::vsaddlv2d, TR::RealRegister::v0, TR::RealRegister::v31, "4eb03be0"),
    std::make_tuple(TR::InstOpCode::vsaddlv8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e30380f"),
    std::make_tuple(TR::InstOpCode::vsaddlv4s, TR::RealRegister::v15, TR::RealRegister::v0, "4e70380f"),
    std::make_tuple(TR::InstOpCode::vsaddlv2d, TR::RealRegister::v15, TR::RealRegister::v0, "4eb0380f"),
    std::make_tuple(TR::InstOpCode::vsaddlv8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e30381f"),
    std::make_tuple(TR::InstOpCode::vsaddlv4s, TR::RealRegister::v31, TR::RealRegister::v0, "4e70381f"),
    std::make_tuple(TR::InstOpCode::vsaddlv2d, TR::RealRegister::v31, TR::RealRegister::v0, "4eb0381f")
));

INSTANTIATE_TEST_CASE_P(VectorUADDLV, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vuaddlv8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e3039e0"),
    std::make_tuple(TR::InstOpCode::vuaddlv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6e7039e0"),
    std::make_tuple(TR::InstOpCode::vuaddlv2d, TR::RealRegister::v0, TR::RealRegister::v15, "6eb039e0"),
    std::make_tuple(TR::InstOpCode::vuaddlv8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e303be0"),
    std::make_tuple(TR::InstOpCode::vuaddlv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6e703be0"),
    std::make_tuple(TR::InstOpCode::vuaddlv2d, TR::RealRegister::v0, TR::RealRegister::v31, "6eb03be0"),
    std::make_tuple(TR::InstOpCode::vuaddlv8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e30380f"),
    std::make_tuple(TR::InstOpCode::vuaddlv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6e70380f"),
    std::make_tuple(TR::InstOpCode::vuaddlv2d, TR::RealRegister::v15, TR::RealRegister::v0, "6eb0380f"),
    std::make_tuple(TR::InstOpCode::vuaddlv8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e30381f"),
    std::make_tuple(TR::InstOpCode::vuaddlv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6e70381f"),
    std::make_tuple(TR::InstOpCode::vuaddlv2d, TR::RealRegister::v31, TR::RealRegister::v0, "6eb0381f")
));

INSTANTIATE_TEST_CASE_P(VectorFloatReduceMINMAX, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfmaxnmv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6e30c9e0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6e30cbe0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6e30c80f"),
    std::make_tuple(TR::InstOpCode::vfmaxnmv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6e30c81f"),
    std::make_tuple(TR::InstOpCode::vfmaxv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6e30f9e0"),
    std::make_tuple(TR::InstOpCode::vfmaxv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6e30fbe0"),
    std::make_tuple(TR::InstOpCode::vfmaxv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6e30f80f"),
    std::make_tuple(TR::InstOpCode::vfmaxv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6e30f81f"),
    std::make_tuple(TR::InstOpCode::vfminnmv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6eb0c9e0"),
    std::make_tuple(TR::InstOpCode::vfminnmv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6eb0cbe0"),
    std::make_tuple(TR::InstOpCode::vfminnmv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6eb0c80f"),
    std::make_tuple(TR::InstOpCode::vfminnmv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6eb0c81f"),
    std::make_tuple(TR::InstOpCode::vfminv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6eb0f9e0"),
    std::make_tuple(TR::InstOpCode::vfminv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6eb0fbe0"),
    std::make_tuple(TR::InstOpCode::vfminv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6eb0f80f"),
    std::make_tuple(TR::InstOpCode::vfminv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6eb0f81f")
));

INSTANTIATE_TEST_CASE_P(VectorSignedReduceMINMAX, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsmaxv16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e30a9e0"),
    std::make_tuple(TR::InstOpCode::vsmaxv8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e70a9e0"),
    std::make_tuple(TR::InstOpCode::vsmaxv4s, TR::RealRegister::v0, TR::RealRegister::v15, "4eb0a9e0"),
    std::make_tuple(TR::InstOpCode::vsmaxv16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e30abe0"),
    std::make_tuple(TR::InstOpCode::vsmaxv8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e70abe0"),
    std::make_tuple(TR::InstOpCode::vsmaxv4s, TR::RealRegister::v0, TR::RealRegister::v31, "4eb0abe0"),
    std::make_tuple(TR::InstOpCode::vsmaxv16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e30a80f"),
    std::make_tuple(TR::InstOpCode::vsmaxv8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e70a80f"),
    std::make_tuple(TR::InstOpCode::vsmaxv4s, TR::RealRegister::v15, TR::RealRegister::v0, "4eb0a80f"),
    std::make_tuple(TR::InstOpCode::vsmaxv16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e30a81f"),
    std::make_tuple(TR::InstOpCode::vsmaxv8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e70a81f"),
    std::make_tuple(TR::InstOpCode::vsmaxv4s, TR::RealRegister::v31, TR::RealRegister::v0, "4eb0a81f"),
    std::make_tuple(TR::InstOpCode::vsminv16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e31a9e0"),
    std::make_tuple(TR::InstOpCode::vsminv8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e71a9e0"),
    std::make_tuple(TR::InstOpCode::vsminv4s, TR::RealRegister::v0, TR::RealRegister::v15, "4eb1a9e0"),
    std::make_tuple(TR::InstOpCode::vsminv16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e31abe0"),
    std::make_tuple(TR::InstOpCode::vsminv8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e71abe0"),
    std::make_tuple(TR::InstOpCode::vsminv4s, TR::RealRegister::v0, TR::RealRegister::v31, "4eb1abe0"),
    std::make_tuple(TR::InstOpCode::vsminv16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e31a80f"),
    std::make_tuple(TR::InstOpCode::vsminv8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e71a80f"),
    std::make_tuple(TR::InstOpCode::vsminv4s, TR::RealRegister::v15, TR::RealRegister::v0, "4eb1a80f"),
    std::make_tuple(TR::InstOpCode::vsminv16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e31a81f"),
    std::make_tuple(TR::InstOpCode::vsminv8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e71a81f"),
    std::make_tuple(TR::InstOpCode::vsminv4s, TR::RealRegister::v31, TR::RealRegister::v0, "4eb1a81f")
));

INSTANTIATE_TEST_CASE_P(VectorUnsignedReduceMINMAX, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vumaxv16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e30a9e0"),
    std::make_tuple(TR::InstOpCode::vumaxv8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e70a9e0"),
    std::make_tuple(TR::InstOpCode::vumaxv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6eb0a9e0"),
    std::make_tuple(TR::InstOpCode::vumaxv16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e30abe0"),
    std::make_tuple(TR::InstOpCode::vumaxv8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e70abe0"),
    std::make_tuple(TR::InstOpCode::vumaxv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6eb0abe0"),
    std::make_tuple(TR::InstOpCode::vumaxv16b, TR::RealRegister::v15, TR::RealRegister::v0, "6e30a80f"),
    std::make_tuple(TR::InstOpCode::vumaxv8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e70a80f"),
    std::make_tuple(TR::InstOpCode::vumaxv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6eb0a80f"),
    std::make_tuple(TR::InstOpCode::vumaxv16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e30a81f"),
    std::make_tuple(TR::InstOpCode::vumaxv8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e70a81f"),
    std::make_tuple(TR::InstOpCode::vumaxv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6eb0a81f"),
    std::make_tuple(TR::InstOpCode::vuminv16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e31a9e0"),
    std::make_tuple(TR::InstOpCode::vuminv8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e71a9e0"),
    std::make_tuple(TR::InstOpCode::vuminv4s, TR::RealRegister::v0, TR::RealRegister::v15, "6eb1a9e0"),
    std::make_tuple(TR::InstOpCode::vuminv16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e31abe0"),
    std::make_tuple(TR::InstOpCode::vuminv8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e71abe0"),
    std::make_tuple(TR::InstOpCode::vuminv4s, TR::RealRegister::v0, TR::RealRegister::v31, "6eb1abe0"),
    std::make_tuple(TR::InstOpCode::vuminv16b, TR::RealRegister::v15, TR::RealRegister::v0, "6e31a80f"),
    std::make_tuple(TR::InstOpCode::vuminv8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e71a80f"),
    std::make_tuple(TR::InstOpCode::vuminv4s, TR::RealRegister::v15, TR::RealRegister::v0, "6eb1a80f"),
    std::make_tuple(TR::InstOpCode::vuminv16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e31a81f"),
    std::make_tuple(TR::InstOpCode::vuminv8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e71a81f"),
    std::make_tuple(TR::InstOpCode::vuminv4s, TR::RealRegister::v31, TR::RealRegister::v0, "6eb1a81f")
));

INSTANTIATE_TEST_CASE_P(VectorAddp, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20bc0f"),
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20bc1f"),
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20bde0"),
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20bfe0"),
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2fbc00"),
    std::make_tuple(TR::InstOpCode::vaddp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3fbc00"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e60bc0f"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60bc1f"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60bde0"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60bfe0"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6fbc00"),
    std::make_tuple(TR::InstOpCode::vaddp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7fbc00"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0bc0f"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0bc1f"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0bde0"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0bfe0"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eafbc00"),
    std::make_tuple(TR::InstOpCode::vaddp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebfbc00"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0bc0f"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ee0bc1f"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ee0bde0"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ee0bfe0"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eefbc00"),
    std::make_tuple(TR::InstOpCode::vaddp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4effbc00")
));

INSTANTIATE_TEST_CASE_P(ScalarAddp, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::addp2d, TR::RealRegister::v0, TR::RealRegister::v15, "5ef1b9e0"),
    std::make_tuple(TR::InstOpCode::addp2d, TR::RealRegister::v0, TR::RealRegister::v31, "5ef1bbe0"),
    std::make_tuple(TR::InstOpCode::addp2d, TR::RealRegister::v15, TR::RealRegister::v0, "5ef1b80f"),
    std::make_tuple(TR::InstOpCode::addp2d, TR::RealRegister::v31, TR::RealRegister::v0, "5ef1b81f")
));

INSTANTIATE_TEST_CASE_P(VectorFAddp, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20d40f"),
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20d41f"),
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20d5e0"),
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20d7e0"),
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fd400"),
    std::make_tuple(TR::InstOpCode::vfaddp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fd400"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60d40f"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60d41f"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60d5e0"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60d7e0"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fd400"),
    std::make_tuple(TR::InstOpCode::vfaddp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fd400")
));

INSTANTIATE_TEST_CASE_P(ScalarFAddp, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::faddp2s, TR::RealRegister::v0, TR::RealRegister::v15, "7e30d9e0"),
    std::make_tuple(TR::InstOpCode::faddp2s, TR::RealRegister::v0, TR::RealRegister::v31, "7e30dbe0"),
    std::make_tuple(TR::InstOpCode::faddp2s, TR::RealRegister::v15, TR::RealRegister::v0, "7e30d80f"),
    std::make_tuple(TR::InstOpCode::faddp2s, TR::RealRegister::v31, TR::RealRegister::v0, "7e30d81f"),
    std::make_tuple(TR::InstOpCode::faddp2d, TR::RealRegister::v0, TR::RealRegister::v15, "7e70d9e0"),
    std::make_tuple(TR::InstOpCode::faddp2d, TR::RealRegister::v0, TR::RealRegister::v31, "7e70dbe0"),
    std::make_tuple(TR::InstOpCode::faddp2d, TR::RealRegister::v15, TR::RealRegister::v0, "7e70d80f"),
    std::make_tuple(TR::InstOpCode::faddp2d, TR::RealRegister::v31, TR::RealRegister::v0, "7e70d81f")
));

INSTANTIATE_TEST_CASE_P(ScalarFmulElem, ARM64Trg1Src2IndexedElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 0, "5f80900f"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 1, "5fa0900f"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 2, "5f80981f"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 3, "5fa0981f"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 0, "5f8091e0"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 1, "5fa091e0"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 2, "5f809be0"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 3, "5fa09be0"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 0, "5f8f9000"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 1, "5faf9000"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 2, "5f9f9800"),
    std::make_tuple(TR::InstOpCode::fmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 3, "5fbf9800"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 0, "5fc0900f"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 1, "5fc0981f"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 0, "5fc091e0"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 1, "5fc09be0"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 0, "5fcf9000"),
    std::make_tuple(TR::InstOpCode::fmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 1, "5fdf9800")
));

INSTANTIATE_TEST_CASE_P(VectorFmulElem, ARM64Trg1Src2IndexedElementEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 0, "4f80900f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 1, "4fa0900f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 2, "4f80981f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 3, "4fa0981f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4f8091e0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 1, "4fa091e0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 2, "4f809be0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 3, "4fa09be0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4f8f9000"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 1, "4faf9000"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 2, "4f9f9800"),
    std::make_tuple(TR::InstOpCode::vfmulelem_4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 3, "4fbf9800"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 0, "4fc0900f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 1, "4fc0981f"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 0, "4fc091e0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 1, "4fc09be0"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 0, "4fcf9000"),
    std::make_tuple(TR::InstOpCode::vfmulelem_2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 1, "4fdf9800")
));

INSTANTIATE_TEST_CASE_P(VectorExt, ARM64Trg1Src2ImmEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, 1, "6e00080f"),
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, 9, "6e00481f"),
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, 5, "6e0029e0"),
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, 13, "6e006be0"),
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, 7, "6e0f3800"),
    std::make_tuple(TR::InstOpCode::vext16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, 15, "6e1f7800")
));

INSTANTIATE_TEST_CASE_P(VectorFloatMinMaxPairwise, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20c40f"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20c41f"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20c5e0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20c7e0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fc400"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fc400"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60c40f"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60c41f"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60c5e0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60c7e0"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fc400"),
    std::make_tuple(TR::InstOpCode::vfmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fc400"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20f40f"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20f41f"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20f5e0"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20f7e0"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2ff400"),
    std::make_tuple(TR::InstOpCode::vfmaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3ff400"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e60f40f"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60f41f"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60f5e0"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60f7e0"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6ff400"),
    std::make_tuple(TR::InstOpCode::vfmaxp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7ff400"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0c40f"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0c41f"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0c5e0"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0c7e0"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafc400"),
    std::make_tuple(TR::InstOpCode::vfminnmp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfc400"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0c40f"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0c41f"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0c5e0"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0c7e0"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eefc400"),
    std::make_tuple(TR::InstOpCode::vfminnmp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6effc400"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0f40f"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0f41f"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0f5e0"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0f7e0"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eaff400"),
    std::make_tuple(TR::InstOpCode::vfminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebff400"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0f40f"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ee0f41f"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ee0f5e0"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ee0f7e0"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eeff400"),
    std::make_tuple(TR::InstOpCode::vfminp2d, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6efff400")
));

INSTANTIATE_TEST_CASE_P(ScalarFloatMinMaxPairwise, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::fmaxnmp2s, TR::RealRegister::v15, TR::RealRegister::v0, "7e30c80f"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2s, TR::RealRegister::v31, TR::RealRegister::v0, "7e30c81f"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2s, TR::RealRegister::v0, TR::RealRegister::v15, "7e30c9e0"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2s, TR::RealRegister::v0, TR::RealRegister::v31, "7e30cbe0"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2d, TR::RealRegister::v15, TR::RealRegister::v0, "7e70c80f"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2d, TR::RealRegister::v31, TR::RealRegister::v0, "7e70c81f"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v15, "7e70c9e0"),
    std::make_tuple(TR::InstOpCode::fmaxnmp2d, TR::RealRegister::v0, TR::RealRegister::v31, "7e70cbe0"),
    std::make_tuple(TR::InstOpCode::fmaxp2s, TR::RealRegister::v15, TR::RealRegister::v0, "7e30f80f"),
    std::make_tuple(TR::InstOpCode::fmaxp2s, TR::RealRegister::v31, TR::RealRegister::v0, "7e30f81f"),
    std::make_tuple(TR::InstOpCode::fmaxp2s, TR::RealRegister::v0, TR::RealRegister::v15, "7e30f9e0"),
    std::make_tuple(TR::InstOpCode::fmaxp2s, TR::RealRegister::v0, TR::RealRegister::v31, "7e30fbe0"),
    std::make_tuple(TR::InstOpCode::fmaxp2d, TR::RealRegister::v15, TR::RealRegister::v0, "7e70f80f"),
    std::make_tuple(TR::InstOpCode::fmaxp2d, TR::RealRegister::v31, TR::RealRegister::v0, "7e70f81f"),
    std::make_tuple(TR::InstOpCode::fmaxp2d, TR::RealRegister::v0, TR::RealRegister::v15, "7e70f9e0"),
    std::make_tuple(TR::InstOpCode::fmaxp2d, TR::RealRegister::v0, TR::RealRegister::v31, "7e70fbe0"),
    std::make_tuple(TR::InstOpCode::fminnmp2s, TR::RealRegister::v15, TR::RealRegister::v0, "7eb0c80f"),
    std::make_tuple(TR::InstOpCode::fminnmp2s, TR::RealRegister::v31, TR::RealRegister::v0, "7eb0c81f"),
    std::make_tuple(TR::InstOpCode::fminnmp2s, TR::RealRegister::v0, TR::RealRegister::v15, "7eb0c9e0"),
    std::make_tuple(TR::InstOpCode::fminnmp2s, TR::RealRegister::v0, TR::RealRegister::v31, "7eb0cbe0"),
    std::make_tuple(TR::InstOpCode::fminnmp2d, TR::RealRegister::v15, TR::RealRegister::v0, "7ef0c80f"),
    std::make_tuple(TR::InstOpCode::fminnmp2d, TR::RealRegister::v31, TR::RealRegister::v0, "7ef0c81f"),
    std::make_tuple(TR::InstOpCode::fminnmp2d, TR::RealRegister::v0, TR::RealRegister::v15, "7ef0c9e0"),
    std::make_tuple(TR::InstOpCode::fminnmp2d, TR::RealRegister::v0, TR::RealRegister::v31, "7ef0cbe0"),
    std::make_tuple(TR::InstOpCode::fminp2s, TR::RealRegister::v15, TR::RealRegister::v0, "7eb0f80f"),
    std::make_tuple(TR::InstOpCode::fminp2s, TR::RealRegister::v31, TR::RealRegister::v0, "7eb0f81f"),
    std::make_tuple(TR::InstOpCode::fminp2s, TR::RealRegister::v0, TR::RealRegister::v15, "7eb0f9e0"),
    std::make_tuple(TR::InstOpCode::fminp2s, TR::RealRegister::v0, TR::RealRegister::v31, "7eb0fbe0"),
    std::make_tuple(TR::InstOpCode::fminp2d, TR::RealRegister::v15, TR::RealRegister::v0, "7ef0f80f"),
    std::make_tuple(TR::InstOpCode::fminp2d, TR::RealRegister::v31, TR::RealRegister::v0, "7ef0f81f"),
    std::make_tuple(TR::InstOpCode::fminp2d, TR::RealRegister::v0, TR::RealRegister::v15, "7ef0f9e0"),
    std::make_tuple(TR::InstOpCode::fminp2d, TR::RealRegister::v0, TR::RealRegister::v31, "7ef0fbe0")
));

INSTANTIATE_TEST_CASE_P(VectorXTN, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vxtn_8b, TR::RealRegister::v15, TR::RealRegister::v0, "0e21280f"),
    std::make_tuple(TR::InstOpCode::vxtn_8b, TR::RealRegister::v31, TR::RealRegister::v0, "0e21281f"),
    std::make_tuple(TR::InstOpCode::vxtn_8b, TR::RealRegister::v0, TR::RealRegister::v15, "0e2129e0"),
    std::make_tuple(TR::InstOpCode::vxtn_8b, TR::RealRegister::v0, TR::RealRegister::v31, "0e212be0"),
    std::make_tuple(TR::InstOpCode::vxtn_4h, TR::RealRegister::v15, TR::RealRegister::v0, "0e61280f"),
    std::make_tuple(TR::InstOpCode::vxtn_4h, TR::RealRegister::v31, TR::RealRegister::v0, "0e61281f"),
    std::make_tuple(TR::InstOpCode::vxtn_4h, TR::RealRegister::v0, TR::RealRegister::v15, "0e6129e0"),
    std::make_tuple(TR::InstOpCode::vxtn_4h, TR::RealRegister::v0, TR::RealRegister::v31, "0e612be0"),
    std::make_tuple(TR::InstOpCode::vxtn_2s, TR::RealRegister::v15, TR::RealRegister::v0, "0ea1280f"),
    std::make_tuple(TR::InstOpCode::vxtn_2s, TR::RealRegister::v31, TR::RealRegister::v0, "0ea1281f"),
    std::make_tuple(TR::InstOpCode::vxtn_2s, TR::RealRegister::v0, TR::RealRegister::v15, "0ea129e0"),
    std::make_tuple(TR::InstOpCode::vxtn_2s, TR::RealRegister::v0, TR::RealRegister::v31, "0ea12be0"),
    std::make_tuple(TR::InstOpCode::vxtn2_16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e21280f"),
    std::make_tuple(TR::InstOpCode::vxtn2_16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e21281f"),
    std::make_tuple(TR::InstOpCode::vxtn2_16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e2129e0"),
    std::make_tuple(TR::InstOpCode::vxtn2_16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e212be0"),
    std::make_tuple(TR::InstOpCode::vxtn2_8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e61280f"),
    std::make_tuple(TR::InstOpCode::vxtn2_8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e61281f"),
    std::make_tuple(TR::InstOpCode::vxtn2_8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e6129e0"),
    std::make_tuple(TR::InstOpCode::vxtn2_8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e612be0"),
    std::make_tuple(TR::InstOpCode::vxtn2_4s, TR::RealRegister::v15, TR::RealRegister::v0, "4ea1280f"),
    std::make_tuple(TR::InstOpCode::vxtn2_4s, TR::RealRegister::v31, TR::RealRegister::v0, "4ea1281f"),
    std::make_tuple(TR::InstOpCode::vxtn2_4s, TR::RealRegister::v0, TR::RealRegister::v15, "4ea129e0"),
    std::make_tuple(TR::InstOpCode::vxtn2_4s, TR::RealRegister::v0, TR::RealRegister::v31, "4ea12be0")
));

INSTANTIATE_TEST_CASE_P(CCMPIMM, ARM64Src1ImmCondEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x0, 31, 3, TR::CC_EQ, "fa5f0803"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x0, 31, 9, TR::CC_CC, "fa5f3809"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x0, 15, 3, TR::CC_NE, "fa4f1803"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x0, 15, 9, TR::CC_HI, "fa4f8809"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x15, 0, 12, TR::CC_CS, "fa4029ec"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x15, 0, 8, TR::CC_LT, "fa40b9e8"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x29, 0, 12, TR::CC_CC, "fa403bac"),
    std::make_tuple(TR::InstOpCode::ccmpimmx, TR::RealRegister::x29, 0, 8, TR::CC_LE, "fa40dba8"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x0, 31, 3, TR::CC_EQ, "7a5f0803"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x0, 31, 9, TR::CC_CC, "7a5f3809"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x0, 15, 3, TR::CC_NE, "7a4f1803"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x0, 15, 9, TR::CC_HI, "7a4f8809"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x15, 0, 12, TR::CC_CS, "7a4029ec"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x15, 0, 8, TR::CC_LT, "7a40b9e8"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x29, 0, 12, TR::CC_CC, "7a403bac"),
    std::make_tuple(TR::InstOpCode::ccmpimmw, TR::RealRegister::x29, 0, 8, TR::CC_LE, "7a40dba8")
));

INSTANTIATE_TEST_CASE_P(CCMNIMM, ARM64Src1ImmCondEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x0, 31, 3, TR::CC_EQ, "ba5f0803"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x0, 31, 9, TR::CC_CC, "ba5f3809"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x0, 15, 3, TR::CC_NE, "ba4f1803"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x0, 15, 9, TR::CC_HI, "ba4f8809"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x15, 0, 12, TR::CC_CS, "ba4029ec"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x15, 0, 8, TR::CC_LT, "ba40b9e8"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x29, 0, 12, TR::CC_CC, "ba403bac"),
    std::make_tuple(TR::InstOpCode::ccmnimmx, TR::RealRegister::x29, 0, 8, TR::CC_LE, "ba40dba8"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x0, 31, 3, TR::CC_EQ, "3a5f0803"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x0, 31, 9, TR::CC_CC, "3a5f3809"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x0, 15, 3, TR::CC_NE, "3a4f1803"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x0, 15, 9, TR::CC_HI, "3a4f8809"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x15, 0, 12, TR::CC_CS, "3a4029ec"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x15, 0, 8, TR::CC_LT, "3a40b9e8"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x29, 0, 12, TR::CC_CC, "3a403bac"),
    std::make_tuple(TR::InstOpCode::ccmnimmw, TR::RealRegister::x29, 0, 8, TR::CC_LE, "3a40dba8")
));

INSTANTIATE_TEST_CASE_P(CCMP, ARM64Src2CondEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x0, TR::RealRegister::x29, 3, TR::CC_EQ, "fa5d0003"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x0, TR::RealRegister::x29, 9, TR::CC_CC, "fa5d3009"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x0, TR::RealRegister::x15, 3, TR::CC_NE, "fa4f1003"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x0, TR::RealRegister::x15, 9, TR::CC_HI, "fa4f8009"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x15, TR::RealRegister::x0, 12, TR::CC_CS, "fa4021ec"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x15, TR::RealRegister::x0, 8, TR::CC_LT, "fa40b1e8"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x29, TR::RealRegister::x0, 12, TR::CC_CC, "fa4033ac"),
    std::make_tuple(TR::InstOpCode::ccmpx, TR::RealRegister::x29, TR::RealRegister::x0, 8, TR::CC_LE, "fa40d3a8"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x0, TR::RealRegister::x29, 3, TR::CC_EQ, "7a5d0003"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x0, TR::RealRegister::x29, 9, TR::CC_CC, "7a5d3009"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x0, TR::RealRegister::x15, 3, TR::CC_NE, "7a4f1003"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x0, TR::RealRegister::x15, 9, TR::CC_HI, "7a4f8009"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x15, TR::RealRegister::x0, 12, TR::CC_CS, "7a4021ec"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x15, TR::RealRegister::x0, 8, TR::CC_LT, "7a40b1e8"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x29, TR::RealRegister::x0, 12, TR::CC_CC, "7a4033ac"),
    std::make_tuple(TR::InstOpCode::ccmpw, TR::RealRegister::x29, TR::RealRegister::x0, 8, TR::CC_LE, "7a40d3a8")
));

INSTANTIATE_TEST_CASE_P(CCMN, ARM64Src2CondEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x0, TR::RealRegister::x29, 3, TR::CC_EQ, "ba5d0003"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x0, TR::RealRegister::x29, 9, TR::CC_CC, "ba5d3009"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x0, TR::RealRegister::x15, 3, TR::CC_NE, "ba4f1003"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x0, TR::RealRegister::x15, 9, TR::CC_HI, "ba4f8009"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x15, TR::RealRegister::x0, 12, TR::CC_CS, "ba4021ec"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x15, TR::RealRegister::x0, 8, TR::CC_LT, "ba40b1e8"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x29, TR::RealRegister::x0, 12, TR::CC_CC, "ba4033ac"),
    std::make_tuple(TR::InstOpCode::ccmnx, TR::RealRegister::x29, TR::RealRegister::x0, 8, TR::CC_LE, "ba40d3a8"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x0, TR::RealRegister::x29, 3, TR::CC_EQ, "3a5d0003"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x0, TR::RealRegister::x29, 9, TR::CC_CC, "3a5d3009"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x0, TR::RealRegister::x15, 3, TR::CC_NE, "3a4f1003"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x0, TR::RealRegister::x15, 9, TR::CC_HI, "3a4f8009"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x15, TR::RealRegister::x0, 12, TR::CC_CS, "3a4021ec"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x15, TR::RealRegister::x0, 8, TR::CC_LT, "3a40b1e8"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x29, TR::RealRegister::x0, 12, TR::CC_CC, "3a4033ac"),
    std::make_tuple(TR::InstOpCode::ccmnw, TR::RealRegister::x29, TR::RealRegister::x0, 8, TR::CC_LE, "3a40d3a8")
));

INSTANTIATE_TEST_CASE_P(CINC, ARM64CINCEncodingTest, ::testing::Values(
    std::make_tuple(true, TR::RealRegister::x0, TR::RealRegister::x29, TR::CC_EQ, "9a9d17a0"),
    std::make_tuple(true, TR::RealRegister::x0, TR::RealRegister::x15, TR::CC_CC, "9a8f25e0"),
    std::make_tuple(true, TR::RealRegister::x15, TR::RealRegister::x0, TR::CC_NE, "9a80040f"),
    std::make_tuple(true, TR::RealRegister::x29, TR::RealRegister::x0, TR::CC_HI, "9a80941d"),
    std::make_tuple(false, TR::RealRegister::x0, TR::RealRegister::x29, TR::CC_CS, "1a9d37a0"),
    std::make_tuple(false, TR::RealRegister::x0, TR::RealRegister::x15, TR::CC_LT, "1a8fa5e0"),
    std::make_tuple(false, TR::RealRegister::x15, TR::RealRegister::x0, TR::CC_CC, "1a80240f"),
    std::make_tuple(false, TR::RealRegister::x29, TR::RealRegister::x0, TR::CC_LE, "1a80c41d")
));

INSTANTIATE_TEST_CASE_P(VectorSMAXP, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20a40f"),
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20a41f"),
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20a5e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20a7e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2fa400"),
    std::make_tuple(TR::InstOpCode::vsmaxp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3fa400"),
    std::make_tuple(TR::InstOpCode::vsmaxp8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60a41f"),
    std::make_tuple(TR::InstOpCode::vsmaxp8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60a5e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60a7e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6fa400"),
    std::make_tuple(TR::InstOpCode::vsmaxp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7fa400"),
    std::make_tuple(TR::InstOpCode::vsmaxp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0a41f"),
    std::make_tuple(TR::InstOpCode::vsmaxp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0a5e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0a7e0"),
    std::make_tuple(TR::InstOpCode::vsmaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eafa400"),
    std::make_tuple(TR::InstOpCode::vsmaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebfa400")
));

INSTANTIATE_TEST_CASE_P(VectorSMINP, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "4e20ac0f"),
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e20ac1f"),
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e20ade0"),
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e20afe0"),
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e2fac00"),
    std::make_tuple(TR::InstOpCode::vsminp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e3fac00"),
    std::make_tuple(TR::InstOpCode::vsminp8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4e60ac1f"),
    std::make_tuple(TR::InstOpCode::vsminp8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4e60ade0"),
    std::make_tuple(TR::InstOpCode::vsminp8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4e60afe0"),
    std::make_tuple(TR::InstOpCode::vsminp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4e6fac00"),
    std::make_tuple(TR::InstOpCode::vsminp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4e7fac00"),
    std::make_tuple(TR::InstOpCode::vsminp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "4ea0ac1f"),
    std::make_tuple(TR::InstOpCode::vsminp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0ade0"),
    std::make_tuple(TR::InstOpCode::vsminp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0afe0"),
    std::make_tuple(TR::InstOpCode::vsminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "4eafac00"),
    std::make_tuple(TR::InstOpCode::vsminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "4ebfac00")
));

INSTANTIATE_TEST_CASE_P(VectorUMAXP, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20a40f"),
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20a41f"),
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20a5e0"),
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20a7e0"),
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fa400"),
    std::make_tuple(TR::InstOpCode::vumaxp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fa400"),
    std::make_tuple(TR::InstOpCode::vumaxp8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60a41f"),
    std::make_tuple(TR::InstOpCode::vumaxp8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60a5e0"),
    std::make_tuple(TR::InstOpCode::vumaxp8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60a7e0"),
    std::make_tuple(TR::InstOpCode::vumaxp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fa400"),
    std::make_tuple(TR::InstOpCode::vumaxp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fa400"),
    std::make_tuple(TR::InstOpCode::vumaxp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0a41f"),
    std::make_tuple(TR::InstOpCode::vumaxp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0a5e0"),
    std::make_tuple(TR::InstOpCode::vumaxp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0a7e0"),
    std::make_tuple(TR::InstOpCode::vumaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafa400"),
    std::make_tuple(TR::InstOpCode::vumaxp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfa400")
));

INSTANTIATE_TEST_CASE_P(VectorUMINP, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v15, TR::RealRegister::v0, TR::RealRegister::v0, "6e20ac0f"),
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e20ac1f"),
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e20ade0"),
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e20afe0"),
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e2fac00"),
    std::make_tuple(TR::InstOpCode::vuminp16b, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e3fac00"),
    std::make_tuple(TR::InstOpCode::vuminp8h, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6e60ac1f"),
    std::make_tuple(TR::InstOpCode::vuminp8h, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6e60ade0"),
    std::make_tuple(TR::InstOpCode::vuminp8h, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6e60afe0"),
    std::make_tuple(TR::InstOpCode::vuminp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6e6fac00"),
    std::make_tuple(TR::InstOpCode::vuminp8h, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6e7fac00"),
    std::make_tuple(TR::InstOpCode::vuminp4s, TR::RealRegister::v31, TR::RealRegister::v0, TR::RealRegister::v0, "6ea0ac1f"),
    std::make_tuple(TR::InstOpCode::vuminp4s, TR::RealRegister::v0, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0ade0"),
    std::make_tuple(TR::InstOpCode::vuminp4s, TR::RealRegister::v0, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0afe0"),
    std::make_tuple(TR::InstOpCode::vuminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v15, "6eafac00"),
    std::make_tuple(TR::InstOpCode::vuminp4s, TR::RealRegister::v0, TR::RealRegister::v0, TR::RealRegister::v31, "6ebfac00")
));

INSTANTIATE_TEST_CASE_P(VectorSADDLP, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vsaddlp16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e20281f"),
    std::make_tuple(TR::InstOpCode::vsaddlp8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e60281f"),
    std::make_tuple(TR::InstOpCode::vsaddlp4s, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0281f"),
    std::make_tuple(TR::InstOpCode::vsaddlp16b, TR::RealRegister::v15, TR::RealRegister::v0, "4e20280f"),
    std::make_tuple(TR::InstOpCode::vsaddlp8h, TR::RealRegister::v15, TR::RealRegister::v0, "4e60280f"),
    std::make_tuple(TR::InstOpCode::vsaddlp4s, TR::RealRegister::v15, TR::RealRegister::v0, "4ea0280f"),
    std::make_tuple(TR::InstOpCode::vsaddlp16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e2029e0"),
    std::make_tuple(TR::InstOpCode::vsaddlp8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e6029e0"),
    std::make_tuple(TR::InstOpCode::vsaddlp4s, TR::RealRegister::v0, TR::RealRegister::v15, "4ea029e0"),
    std::make_tuple(TR::InstOpCode::vsaddlp16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e202be0"),
    std::make_tuple(TR::InstOpCode::vsaddlp8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e602be0"),
    std::make_tuple(TR::InstOpCode::vsaddlp4s, TR::RealRegister::v0, TR::RealRegister::v31, "4ea02be0")
));

INSTANTIATE_TEST_CASE_P(VectorUADDLP, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vuaddlp16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e20281f"),
    std::make_tuple(TR::InstOpCode::vuaddlp8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e60281f"),
    std::make_tuple(TR::InstOpCode::vuaddlp4s, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0281f"),
    std::make_tuple(TR::InstOpCode::vuaddlp16b, TR::RealRegister::v15, TR::RealRegister::v0, "6e20280f"),
    std::make_tuple(TR::InstOpCode::vuaddlp8h, TR::RealRegister::v15, TR::RealRegister::v0, "6e60280f"),
    std::make_tuple(TR::InstOpCode::vuaddlp4s, TR::RealRegister::v15, TR::RealRegister::v0, "6ea0280f"),
    std::make_tuple(TR::InstOpCode::vuaddlp16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e2029e0"),
    std::make_tuple(TR::InstOpCode::vuaddlp8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e6029e0"),
    std::make_tuple(TR::InstOpCode::vuaddlp4s, TR::RealRegister::v0, TR::RealRegister::v15, "6ea029e0"),
    std::make_tuple(TR::InstOpCode::vuaddlp16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e202be0"),
    std::make_tuple(TR::InstOpCode::vuaddlp8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e602be0"),
    std::make_tuple(TR::InstOpCode::vuaddlp4s, TR::RealRegister::v0, TR::RealRegister::v31, "6ea02be0")
));

INSTANTIATE_TEST_CASE_P(VectorCLS, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vcls16b, TR::RealRegister::v0, TR::RealRegister::v31, "4e204be0"),
    std::make_tuple(TR::InstOpCode::vcls16b, TR::RealRegister::v0, TR::RealRegister::v15, "4e2049e0"),
    std::make_tuple(TR::InstOpCode::vcls16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e20481f"),
    std::make_tuple(TR::InstOpCode::vcls16b, TR::RealRegister::v31, TR::RealRegister::v0, "4e20481f"),
    std::make_tuple(TR::InstOpCode::vcls8h, TR::RealRegister::v0, TR::RealRegister::v31, "4e604be0"),
    std::make_tuple(TR::InstOpCode::vcls8h, TR::RealRegister::v0, TR::RealRegister::v15, "4e6049e0"),
    std::make_tuple(TR::InstOpCode::vcls8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e60481f"),
    std::make_tuple(TR::InstOpCode::vcls8h, TR::RealRegister::v31, TR::RealRegister::v0, "4e60481f"),
    std::make_tuple(TR::InstOpCode::vcls4s, TR::RealRegister::v0, TR::RealRegister::v31, "4ea04be0"),
    std::make_tuple(TR::InstOpCode::vcls4s, TR::RealRegister::v0, TR::RealRegister::v15, "4ea049e0"),
    std::make_tuple(TR::InstOpCode::vcls4s, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0481f"),
    std::make_tuple(TR::InstOpCode::vcls4s, TR::RealRegister::v31, TR::RealRegister::v0, "4ea0481f")
));

INSTANTIATE_TEST_CASE_P(VectorCLZ, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vclz16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e204be0"),
    std::make_tuple(TR::InstOpCode::vclz16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e2049e0"),
    std::make_tuple(TR::InstOpCode::vclz16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e20481f"),
    std::make_tuple(TR::InstOpCode::vclz16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e20481f"),
    std::make_tuple(TR::InstOpCode::vclz8h, TR::RealRegister::v0, TR::RealRegister::v31, "6e604be0"),
    std::make_tuple(TR::InstOpCode::vclz8h, TR::RealRegister::v0, TR::RealRegister::v15, "6e6049e0"),
    std::make_tuple(TR::InstOpCode::vclz8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e60481f"),
    std::make_tuple(TR::InstOpCode::vclz8h, TR::RealRegister::v31, TR::RealRegister::v0, "6e60481f"),
    std::make_tuple(TR::InstOpCode::vclz4s, TR::RealRegister::v0, TR::RealRegister::v31, "6ea04be0"),
    std::make_tuple(TR::InstOpCode::vclz4s, TR::RealRegister::v0, TR::RealRegister::v15, "6ea049e0"),
    std::make_tuple(TR::InstOpCode::vclz4s, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0481f"),
    std::make_tuple(TR::InstOpCode::vclz4s, TR::RealRegister::v31, TR::RealRegister::v0, "6ea0481f")
));

INSTANTIATE_TEST_CASE_P(VectorRBIT, ARM64Trg1Src1EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::vrbit16b, TR::RealRegister::v0, TR::RealRegister::v31, "6e605be0"),
    std::make_tuple(TR::InstOpCode::vrbit16b, TR::RealRegister::v0, TR::RealRegister::v15, "6e6059e0"),
    std::make_tuple(TR::InstOpCode::vrbit16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e60581f"),
    std::make_tuple(TR::InstOpCode::vrbit16b, TR::RealRegister::v31, TR::RealRegister::v0, "6e60581f")
));

INSTANTIATE_TEST_CASE_P(SMULH, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x0, TR::RealRegister::x15, TR::RealRegister::x29, "9b5d7de0"),
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x0, TR::RealRegister::x29, TR::RealRegister::x15, "9b4f7fa0"),
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x15, TR::RealRegister::x0, TR::RealRegister::x29, "9b5d7c0f"),
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x15, TR::RealRegister::x29, TR::RealRegister::x0, "9b407faf"),
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x29, TR::RealRegister::x0, TR::RealRegister::x15, "9b4f7c1d"),
    std::make_tuple(TR::InstOpCode::smulh, TR::RealRegister::x29, TR::RealRegister::x15, TR::RealRegister::x0, "9b407dfd")
));

INSTANTIATE_TEST_CASE_P(UMULH, ARM64Trg1Src2EncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x0, TR::RealRegister::x15, TR::RealRegister::x29, "9bdd7de0"),
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x0, TR::RealRegister::x29, TR::RealRegister::x15, "9bcf7fa0"),
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x15, TR::RealRegister::x0, TR::RealRegister::x29, "9bdd7c0f"),
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x15, TR::RealRegister::x29, TR::RealRegister::x0, "9bc07faf"),
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x29, TR::RealRegister::x0, TR::RealRegister::x15, "9bcf7c1d"),
    std::make_tuple(TR::InstOpCode::umulh, TR::RealRegister::x29, TR::RealRegister::x15, TR::RealRegister::x0, "9bc07dfd")
));
