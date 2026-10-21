/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#include "codegen/OMRX86Instruction.hpp"

TR::RealRegister *getRealRegister(TR::RealRegister::RegNum regNum, TR::CodeGenerator *cg)
{
    TR::RealRegister *rr = cg->machine()->getRealRegister(regNum);

    if (!cg->comp()->target().cpu.supportsFeature(OMR_FEATURE_X86_AVX512F) && regNum >= TR::RealRegister::k0
        && regNum <= TR::RealRegister::k7) {
        // Without AVX-512 the machine class will not initialize mask registers
        rr = new (cg->trHeapMemory())
            TR::RealRegister(TR_VMR, 0, TR::RealRegister::Free, regNum, TR::RealRegister::vmrMask(regNum), cg);
    }

    return rr;
}

class XDirectEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<
          std::tuple<OP::Mnemonic, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XDirectEncodingTest, encode)
{
    TR::Instruction *instr = Inst(std::get<0>(GetParam()), fakeNode, cg(), std::get<1>(GetParam()));

    ASSERT_EQ(std::get<2>(GetParam()), encodeInstruction(instr));
}

class XLabelEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, size_t, TRTest::BinaryInstruction> > {};

TEST_P(XLabelEncodingTest, encode)
{
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(reinterpret_cast<uint8_t *>(getAlignedBuf()) + std::get<1>(GetParam()));

    auto instr = Inst_Label(std::get<0>(GetParam()), fakeNode, label, cg());

    ASSERT_EQ(std::get<2>(GetParam()), encodeInstruction(instr));
}

class XRegRegEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, TRTest::BinaryInstruction> > {};

TEST_P(XRegRegEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto regB = getRealRegister(std::get<2>(GetParam()), cg());

    auto instr = Inst_RegReg(std::get<0>(GetParam()), fakeNode, regA, regB, cg());

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

class XRegRegImm1EncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegRegImm1EncodingTest, encode)
{
    auto regA = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto regB = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto imm1 = std::get<3>(GetParam());
    auto encoding = std::get<4>(GetParam());

    auto instr = Inst_RegRegImm(std::get<0>(GetParam()), fakeNode, regA, regB, imm1, cg(), encoding);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

class XRegMaskRegRegImmEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding,
          TRTest::BinaryInstruction> > {};

TEST_P(XRegMaskRegRegImmEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto regB = getRealRegister(std::get<2>(GetParam()), cg());
    auto regC = getRealRegister(std::get<3>(GetParam()), cg());
    auto regD = getRealRegister(std::get<4>(GetParam()), cg());
    auto imm1 = std::get<5>(GetParam());
    auto encoding = std::get<6>(GetParam());

    auto instr = Inst_RegMaskRegRegImm(std::get<0>(GetParam()), fakeNode, regA, regB, regC, regD, imm1, cg(), encoding);

    ASSERT_EQ(std::get<7>(GetParam()), encodeInstruction(instr));
}

class XRegImm1EncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, int32_t,
          OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegImm1EncodingTest, encode)
{
    auto regA = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto imm1 = std::get<2>(GetParam());
    auto encoding = std::get<3>(GetParam());

    auto instr = Inst_RegImm(std::get<0>(GetParam()), fakeNode, regA, imm1, cg(), TR_NoRelocation, encoding);

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

class XRegMemEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, int32_t, TRTest::BinaryInstruction> > {};

TEST_P(XRegMemEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto mrBaseReg = getRealRegister(std::get<2>(GetParam()), cg());
    auto mrOffset = std::get<3>(GetParam());

    auto mr = MRef_Bdisp32(mrBaseReg, mrOffset, cg());
    auto instr = Inst_RegMem(std::get<0>(GetParam()), fakeNode, regA, mr, cg());

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(Special, XDirectEncodingTest,
    ::testing::Values(std::make_tuple(OP::UD2, OMR::X86::Default, "0f0b"),
        std::make_tuple(OP::INT3, OMR::X86::Default, "cc"),
        std::make_tuple(OP::RET, OMR::X86::Default, "c3"),
        std::make_tuple(OP::VZEROUPPER, OMR::X86::VEX_L128, "c5f877")));

INSTANTIATE_TEST_CASE_P(Special, XLabelEncodingTest,
    ::testing::Values(std::make_tuple(OP::JMP4, 0x4, "eb02"),
        std::make_tuple(OP::JMP4, -0x4, "ebfa"), std::make_tuple(OP::JGE4, 0x4, "7d02"),
        std::make_tuple(OP::JGE4, -0x4, "7dfa"), std::make_tuple(OP::JGE4, 0x0, "7dfe")));

class XRegRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegRegEncEncodingTest, encode)
{
    TR::RealRegister *regA = getRealRegister(std::get<1>(GetParam()), cg());
    TR::RealRegister *regB = getRealRegister(std::get<2>(GetParam()), cg());
    auto enc = std::get<3>(GetParam());

    auto instr = Inst_RegReg(std::get<0>(GetParam()), fakeNode, regA, regB, cg(), enc);

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(SIMDMinMaxTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        /* REX prefix 48 is unnecessary but not illegal */
        /* TODO: Remove it */
        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f3838c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660feac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f3839c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "0f5dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "66480f5dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f383cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660feec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f383dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "0f5fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "66480f5fc8"),

        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4e27138c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c5f1eac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4e27139c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c5f05dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e1f15dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4e2713cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c5f1eec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4e2713dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c5f05fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e1f15fc8"),

        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c4e27538c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c5f5eac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c4e27539c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c5f45dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c4e1f55dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c4e2753cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c5f5eec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c4e2753dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c5f45fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c4e1f55fc8"),

        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f2750838c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f17508eac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f2750839c8"),
        std::make_tuple(OP::PMINSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f2f50839c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f174085dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f1f5085dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f275083cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f17508eec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f275083dc8"),
        std::make_tuple(OP::PMAXSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f2f5083dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f174085fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f1f5085fc8"),

        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f2752838c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f17528eac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f2752839c8"),
        std::make_tuple(OP::PMINSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f2f52839c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f174285dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f1f5285dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f275283cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f17528eec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f275283dc8"),
        std::make_tuple(OP::PMAXSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f2f5283dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f174285fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f1f5285fc8"),

        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f2754838c8"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f17548eac8"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f2754839c8"),
        std::make_tuple(OP::PMINSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f2f54839c8"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f174485dc8"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f1f5485dc8"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f275483cc8"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f17548eec8"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f275483dc8"),
        std::make_tuple(OP::PMAXSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f2f5483dc8"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f174485fc8"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f1f5485fc8"))));

INSTANTIATE_TEST_CASE_P(SIMDAbsTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f381cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f381dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::Legacy,
            "660f381ec8"),

        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e2791cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e2791dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e2791ec8"),

        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c4e27d1cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c4e27d1dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::VEX_L256,
            "c4e27d1ec8"),

        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f27d081cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f27d081dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f27d081ec8"),
        std::make_tuple(OP::PABSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f2fd081fc8"),

        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f27d281cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f27d281dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f27d281ec8"),
        std::make_tuple(OP::PABSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f2fd281fc8"),

        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f27d481cc8"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f27d481dc8"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f27d481ec8"),
        std::make_tuple(OP::PABSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L512, "62f2fd481fc8"))));

INSTANTIATE_TEST_CASE_P(AVXSimdTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::Legacy,
            "660ffcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm1, OMR::X86::Legacy,
            "660ffcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm1, OMR::X86::Legacy,
            "660ffcc9"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::VEX_L128,
            "c5f1fcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm1, OMR::X86::VEX_L128,
            "c5e9fcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm1, OMR::X86::VEX_L128,
            "c5f1fcc9"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::VEX_L256,
            "c5f5fcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm1, OMR::X86::VEX_L256,
            "c5edfcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm1, OMR::X86::VEX_L256,
            "c5f5fcc9"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm2,
            OMR::X86::EVEX_L128, "62f17508fcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L128, "62f16d08fcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L128, "62f17508fcc9"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm2,
            OMR::X86::EVEX_L256, "62f17528fcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm1,
            OMR::X86::EVEX_L256, "62f16d28fcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm1,
            OMR::X86::EVEX_L256, "62f17528fcc9"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm2,
            OMR::X86::EVEX_L512, "62f17548fcca"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm1,
            OMR::X86::EVEX_L512, "62f16d48fcd1"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm1,
            OMR::X86::EVEX_L512, "62f17548fcc9"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::VEX_L128, "c4e1f951c1"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm4,
            OMR::X86::VEX_L128, "c461f951cc"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1, OMR::X86::Legacy,
            "66480f51c1"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm4, OMR::X86::Legacy,
            "664c0f51cc"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L128, "62f1fd0851c1"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L256, "62f1fd2851c1"),
        std::make_tuple(OP::SQRTPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L512, "62f1fd4851c1"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm4, OMR::X86::Legacy,
            "440f51cc"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::VEX_L128, "c5f851c1"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::VEX_L256, "c5fc51c1"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L128, "62f17c0851c1"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L256, "62f17c2851c1"),
        std::make_tuple(OP::SQRTPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L512, "62f17c4851c1"))));

INSTANTIATE_TEST_CASE_P(LegacySimdTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            OMR::X86::Legacy, "660f60c1"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm2,
            OMR::X86::Legacy, "660f3830e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm4,
            OMR::X86::Legacy, "660f3831d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm0,
            OMR::X86::Legacy, "660f3832c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1, OMR::X86::Legacy,
            "0f55c1"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1, OMR::X86::Legacy,
            "66480f55c1"),
        std::make_tuple(OP::PMOVMSKB4RegReg, TR::RealRegister::eax, TR::RealRegister::xmm0,
            OMR::X86::Legacy, "660fd7c0"),
        std::make_tuple(OP::MOVMSKPSRegReg, TR::RealRegister::ecx, TR::RealRegister::xmm7, OMR::X86::Legacy,
            "0f50cf"),
        std::make_tuple(OP::MOVMSKPDRegReg, TR::RealRegister::edx, TR::RealRegister::xmm15,
            OMR::X86::Legacy, "66410f50d7"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm9,
            OMR::X86::Legacy, "66410f3821c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::Legacy, "66440f3823c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::Legacy, "66440f3833c0"))));

INSTANTIATE_TEST_CASE_P(AVXSimdRegRegVex128Test, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VBROADCASTSSRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4427918df"),
        std::make_tuple(OP::VPBROADCASTBRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4427978df"),
        std::make_tuple(OP::VPBROADCASTWRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4427979df"),
        std::make_tuple(OP::VPBROADCASTDRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4427958df"),
        std::make_tuple(OP::VPBROADCASTQRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4427959df"),
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4412160df"),
        std::make_tuple(OP::PMOVMSKB4RegReg, TR::RealRegister::eax, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c5f9d7c0"),
        std::make_tuple(OP::MOVMSKPSRegReg, TR::RealRegister::ecx, TR::RealRegister::xmm7,
            OMR::X86::VEX_L128, "c5f850cf"),
        std::make_tuple(OP::MOVMSKPDRegReg, TR::RealRegister::edx, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4c17950d7"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm2,
            OMR::X86::VEX_L128, "c4e27930e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm4,
            OMR::X86::VEX_L128, "c4e27931d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4e27932c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4c17855c7"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4c1f955c7"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4c179fcc7"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm8, OMR::X86::VEX_L128,
            "c4c171fdc8"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm7, OMR::X86::VEX_L128,
            "c5e9fed7"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c4e1e1d4d8"),
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c4c15858e7"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm8, OMR::X86::VEX_L128,
            "c4c1d158e8"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::xmm6, TR::RealRegister::xmm7, OMR::X86::VEX_L128,
            "c5c9f8f7"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::xmm7, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c5c1f9f8"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c44139fac7"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm8, OMR::X86::VEX_L128,
            "c441b1fbc8"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::xmm10, TR::RealRegister::xmm7,
            OMR::X86::VEX_L128, "c5285cd7"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c461a15cd8"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c44119d5e7"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm8,
            OMR::X86::VEX_L128, "c4421140e8"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm7,
            OMR::X86::VEX_L128, "c50859f7"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4618159f8"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c441005eff"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm8,
            OMR::X86::VEX_L128, "c441895ef0"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm7, OMR::X86::VEX_L128,
            "c511dbef"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm0, OMR::X86::VEX_L128,
            "c519ebe0"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L128, "c44121efdf"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm9,
            OMR::X86::VEX_L128, "c4c27921c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4627923c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::VEX_L128, "c4627933c0"))));

INSTANTIATE_TEST_CASE_P(AVXSimdRegRegVex256Test, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VBROADCASTSSRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d18df"),
        std::make_tuple(OP::VBROADCASTSDYmmYmm, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d19df"),
        std::make_tuple(OP::VPBROADCASTBRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d78df"),
        std::make_tuple(OP::VPBROADCASTWRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d79df"),
        std::make_tuple(OP::VPBROADCASTDRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d58df"),
        std::make_tuple(OP::VPBROADCASTQRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::VEX_L256, "c4427d59df"),
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4412560df"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::ymm4, TR::RealRegister::xmm2,
            OMR::X86::VEX_L256, "c4e27d30e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::ymm2, TR::RealRegister::xmm4,
            OMR::X86::VEX_L256, "c4e27d31d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::ymm0, TR::RealRegister::xmm0,
            OMR::X86::VEX_L256, "c4e27d32c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4c17c55c7"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4c1fd55c7"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4c17dfcc7"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm8, OMR::X86::VEX_L256,
            "c4c175fdc8"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm7, OMR::X86::VEX_L256,
            "c5edfed7"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm0, OMR::X86::VEX_L256,
            "c4e1e5d4d8"),
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4c15c58e7"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm8, OMR::X86::VEX_L256,
            "c4c1d558e8"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::ymm6, TR::RealRegister::ymm7, OMR::X86::VEX_L256,
            "c5cdf8f7"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::ymm7, TR::RealRegister::ymm0, OMR::X86::VEX_L256,
            "c5c5f9f8"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4413dfac7"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::ymm9, TR::RealRegister::ymm8, OMR::X86::VEX_L256,
            "c441b5fbc8"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::ymm10, TR::RealRegister::ymm7,
            OMR::X86::VEX_L256, "c52c5cd7"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm0,
            OMR::X86::VEX_L256, "c461a55cd8"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c4411dd5e7"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm8,
            OMR::X86::VEX_L256, "c4421540e8"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm7,
            OMR::X86::VEX_L256, "c50c59f7"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm0,
            OMR::X86::VEX_L256, "c4618559f8"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c441045eff"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm8,
            OMR::X86::VEX_L256, "c4418d5ef0"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm7, OMR::X86::VEX_L256,
            "c515dbef"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm0, OMR::X86::VEX_L256,
            "c51debe0"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm15,
            OMR::X86::VEX_L256, "c44125efdf"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm9,
            OMR::X86::VEX_L256, "c4c27d21c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm0,
            OMR::X86::VEX_L256, "c4627d23c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm0,
            OMR::X86::VEX_L256, "c4627d33c0"))));

INSTANTIATE_TEST_CASE_P(AVXSimdRegRegEVEX128Test, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VBROADCASTSSRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62527d0818df"),
        std::make_tuple(OP::VPBROADCASTBRegMaskGPR, TR::RealRegister::xmm11, TR::RealRegister::eax,
            OMR::X86::EVEX_L128, "62727d087ad8"),
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "6251250860df"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm2,
            OMR::X86::EVEX_L128, "62f27d0830e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm4,
            OMR::X86::EVEX_L128, "62f27d0831d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f27d0832c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62d17c0855c7"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62d1fd0855c7"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62d17d08fcc7"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "62d17508fdc8"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm7,
            OMR::X86::EVEX_L128, "62f16d08fed7"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f1e508d4d8"),
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62d15c0858e7"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "62d1d50858e8"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::xmm6, TR::RealRegister::xmm7,
            OMR::X86::EVEX_L128, "62f14d08f8f7"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::xmm7, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62f14508f9f8"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62513d08fac7"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "6251b508fbc8"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::xmm10, TR::RealRegister::xmm7,
            OMR::X86::EVEX_L128, "62712c085cd7"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "6271a5085cd8"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62511d08d5e7"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "6252150840e8"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm7,
            OMR::X86::EVEX_L128, "62710c0859f7"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "6271850859f8"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "625104085eff"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "62518d085ef0"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm7,
            OMR::X86::EVEX_L128, "62711508dbef"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm0, OMR::X86::EVEX_L128,
            "62711d08ebe0"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62512508efdf"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm9,
            OMR::X86::EVEX_L128, "62d27d0821c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62727d0823c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L128, "62727d0833c0"))));

INSTANTIATE_TEST_CASE_P(AVXSimdRegRegEVEX256Test, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VBROADCASTSSRegReg, TR::RealRegister::ymm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L256, "62527d2818df"),
        std::make_tuple(OP::VPBROADCASTBRegMaskGPR, TR::RealRegister::ymm11, TR::RealRegister::eax,
            OMR::X86::EVEX_L256, "62727d287ad8"),
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "6251252860df"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::ymm4, TR::RealRegister::xmm2,
            OMR::X86::EVEX_L256, "62f27d2830e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::ymm2, TR::RealRegister::xmm4,
            OMR::X86::EVEX_L256, "62f27d2831d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::ymm0, TR::RealRegister::xmm0,
            OMR::X86::EVEX_L256, "62f27d2832c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62d17c2855c7"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62d1fd2855c7"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62d17d28fcc7"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm8,
            OMR::X86::EVEX_L256, "62d17528fdc8"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm7,
            OMR::X86::EVEX_L256, "62f16d28fed7"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "62f1e528d4d8"),
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62d15c2858e7"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm8,
            OMR::X86::EVEX_L256, "62d1d52858e8"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::ymm6, TR::RealRegister::ymm7,
            OMR::X86::EVEX_L256, "62f14d28f8f7"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::ymm7, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "62f14528f9f8"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62513d28fac7"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::ymm9, TR::RealRegister::ymm8,
            OMR::X86::EVEX_L256, "6251b528fbc8"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::ymm10, TR::RealRegister::ymm7,
            OMR::X86::EVEX_L256, "62712c285cd7"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "6271a5285cd8"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62511d28d5e7"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm8,
            OMR::X86::EVEX_L256, "6252152840e8"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm7,
            OMR::X86::EVEX_L256, "62710c2859f7"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "6271852859f8"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "625104285eff"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm8,
            OMR::X86::EVEX_L256, "62518d285ef0"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm7,
            OMR::X86::EVEX_L256, "62711528dbef"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm0, OMR::X86::EVEX_L256,
            "62711d28ebe0"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm15,
            OMR::X86::EVEX_L256, "62512528efdf"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm9,
            OMR::X86::EVEX_L256, "62d27d2821c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "62727d2823c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L256, "62727d2833c0"))));

INSTANTIATE_TEST_CASE_P(AVXSimdRegRegEVEX512Test, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VBROADCASTSSRegReg, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "62527d4818df"),
        std::make_tuple(OP::VBROADCASTSDZmmXmm, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "6252fd4819df"),
        std::make_tuple(OP::VPBROADCASTBRegReg, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "62527d4878df"),
        std::make_tuple(OP::VPBROADCASTBRegMaskGPR, TR::RealRegister::zmm11, TR::RealRegister::eax,
            OMR::X86::EVEX_L512, "62727d487ad8"),
        std::make_tuple(OP::VPBROADCASTWRegReg, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "62527d4879df"),
        std::make_tuple(OP::VPBROADCASTDRegReg, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "62527d4858df"),
        std::make_tuple(OP::VPBROADCASTQRegReg, TR::RealRegister::zmm11, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L512, "62527d4859df"),
        std::make_tuple(OP::PUNPCKLBWRegReg, TR::RealRegister::zmm11, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "6251254860df"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::zmm4, TR::RealRegister::ymm2,
            OMR::X86::EVEX_L512, "62f27d4830e2"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::zmm2, TR::RealRegister::ymm4,
            OMR::X86::EVEX_L512, "62f27d4831d4"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::zmm0, TR::RealRegister::ymm0,
            OMR::X86::EVEX_L512, "62f27d4832c0"),
        std::make_tuple(OP::ANDNPSRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62d17c4855c7"),
        std::make_tuple(OP::ANDNPDRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62d1fd4855c7"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62d17d48fcc7"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm8,
            OMR::X86::EVEX_L512, "62d17548fdc8"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm7,
            OMR::X86::EVEX_L512, "62f16d48fed7"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::zmm3, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "62f1e548d4d8"),
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::zmm4, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62d15c4858e7"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::zmm5, TR::RealRegister::zmm8,
            OMR::X86::EVEX_L512, "62d1d54858e8"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::zmm6, TR::RealRegister::zmm7,
            OMR::X86::EVEX_L512, "62f14d48f8f7"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::zmm7, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "62f14548f9f8"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::zmm8, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62513d48fac7"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::zmm9, TR::RealRegister::zmm8,
            OMR::X86::EVEX_L512, "6251b548fbc8"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::zmm10, TR::RealRegister::zmm7,
            OMR::X86::EVEX_L512, "62712c485cd7"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::zmm11, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "6271a5485cd8"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::zmm12, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62511d48d5e7"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::zmm13, TR::RealRegister::zmm8,
            OMR::X86::EVEX_L512, "6252154840e8"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::zmm14, TR::RealRegister::zmm7,
            OMR::X86::EVEX_L512, "62710c4859f7"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::zmm15, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "6271854859f8"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::zmm15, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "625104485eff"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::zmm14, TR::RealRegister::zmm8,
            OMR::X86::EVEX_L512, "62518d485ef0"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::zmm13, TR::RealRegister::zmm7,
            OMR::X86::EVEX_L512, "62711548dbef"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::zmm12, TR::RealRegister::zmm0, OMR::X86::EVEX_L512,
            "62711d48ebe0"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::zmm11, TR::RealRegister::zmm15,
            OMR::X86::EVEX_L512, "62512548efdf"),
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm9,
            OMR::X86::EVEX_L512, "62d27d4821c9"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::zmm8, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "62727d4823c0"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::zmm8, TR::RealRegister::zmm0,
            OMR::X86::EVEX_L512, "62727d4833c0"))));

INSTANTIATE_TEST_CASE_P(LegacyRegRegImm1SimdTest, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::Legacy, "f20f70c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xf,
            OMR::X86::Legacy, "f20f70c10f"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::Legacy, "0fc2c100"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xc,
            OMR::X86::Legacy, "0fc2c10c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::Legacy, "66480fc2c100"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xc,
            OMR::X86::Legacy, "66480fc2c10c"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegImm1Vex128Test, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::VEX_L128, "c5fb70c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xf,
            OMR::X86::VEX_L128, "c5fb70c10f"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::VEX_L128, "c5f8c2c100"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xc,
            OMR::X86::VEX_L128, "c5f8c2c10c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::VEX_L128, "c4e1f9c2c100"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xc,
            OMR::X86::VEX_L128, "c4e1f9c2c10c"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegImm1Vex256Test, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VEXTRACTF128RegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::VEX_L256, "c4e37d19c800"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0x0,
            OMR::X86::VEX_L256, "c5ff70c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0xf,
            OMR::X86::VEX_L256, "c5ff70c10f"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0x0,
            OMR::X86::VEX_L256, "c5fcc2c100"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0xc,
            OMR::X86::VEX_L256, "c5fcc2c10c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0x0,
            OMR::X86::VEX_L256, "c4e1fdc2c100"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0xc,
            OMR::X86::VEX_L256, "c4e1fdc2c10c"))));

INSTANTIATE_TEST_CASE_P(AVXRegImm1Vex256Test, XRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, int32_t,
                            OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PSLLQRegImm1, TR::RealRegister::xmm0, 0x1f, OMR::X86::VEX_L128, "c4e1f973f01f"),
        std::make_tuple(OP::PSLLQRegImm1, TR::RealRegister::xmm0, 0x1f, OMR::X86::VEX_L256, "c4e1fd73f01f"),
        std::make_tuple(OP::PSLLQRegImm1, TR::RealRegister::xmm0, 0x1f, OMR::X86::EVEX_L128,
            "62f1fd0873f01f"),
        std::make_tuple(OP::PSLLQRegImm1, TR::RealRegister::ymm0, 0x1f, OMR::X86::EVEX_L256,
            "62f1fd2873f01f"),
        std::make_tuple(OP::PSLLQRegImm1, TR::RealRegister::zmm0, 0x1f, OMR::X86::EVEX_L512,
            "62f1fd4873f01f"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegImm1Evex128Test, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPTERNLOGDRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L128, "62f37d0825c10f"),
        std::make_tuple(OP::VPTERNLOGQRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L128, "62f3fd0825c10f"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0x0,
            OMR::X86::EVEX_L128, "62f17f0870c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1, 0xf,
            OMR::X86::EVEX_L128, "62f17f0870c10f")
        /* cmpps, cmppd excluded until write mask registers are supported. */
        )));

INSTANTIATE_TEST_CASE_P(AVXRegRegImm1Evex256Test, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPTERNLOGDRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L256, "62f37d2825c10f"),
        std::make_tuple(OP::VPTERNLOGQRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L256, "62f3fd2825c10f"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0x0,
            OMR::X86::EVEX_L256, "62f17f2870c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::ymm0, TR::RealRegister::ymm1, 0xf,
            OMR::X86::EVEX_L256, "62f17f2870c10f")
        /* cmpps, cmppd excluded until write mask registers are supported. */
        )));

INSTANTIATE_TEST_CASE_P(AVXRegRegImm1Evex512Test, XRegRegImm1EncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPTERNLOGDRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L512, "62f37d4825c10f"),
        std::make_tuple(OP::VPTERNLOGQRegMaskRegRegImm1, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
            0xf, OMR::X86::EVEX_L512, "62f3fd4825c10f"),
        std::make_tuple(OP::VEXTRACTF64X4YmmZmmImm1, TR::RealRegister::zmm0, TR::RealRegister::zmm1, 0x0,
            OMR::X86::EVEX_L512, "62f3fd481bc800"),
        std::make_tuple(OP::VEXTRACTF64X4YmmZmmImm1, TR::RealRegister::zmm0, TR::RealRegister::zmm1, 0xc,
            OMR::X86::EVEX_L512, "62f3fd481bc80c"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::zmm0, TR::RealRegister::zmm1, 0x0,
            OMR::X86::EVEX_L512, "62f17f4870c100"),
        std::make_tuple(OP::PSHUFLWRegRegImm1, TR::RealRegister::zmm0, TR::RealRegister::zmm1, 0xF,
            OMR::X86::EVEX_L512, "62f17f4870c10f"))));

INSTANTIATE_TEST_CASE_P(AVXRegMaskRegRegImmEvex512Test, XRegMaskRegRegImmEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t,
                            OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::k0, TR::RealRegister::k0,
            TR::RealRegister::xmm0, TR::RealRegister::xmm0, 0xc, OMR::X86::EVEX_L512, "62f17c48c2c00c"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::k0, TR::RealRegister::k1,
            TR::RealRegister::xmm0, TR::RealRegister::xmm0, 0xc, OMR::X86::EVEX_L512, "62f17c49c2c00c"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm1, TR::RealRegister::xmm2, 0xc, OMR::X86::EVEX_L512, "62f1744fc2e20c"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm8, TR::RealRegister::xmm2, 0xc, OMR::X86::EVEX_L512, "62f13c4fc2e20c"),
        std::make_tuple(OP::CMPPSRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm5, TR::RealRegister::xmm9, 0xc, OMR::X86::EVEX_L512, "62d1544fc2e10c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::k0, TR::RealRegister::k0,
            TR::RealRegister::xmm0, TR::RealRegister::xmm0, 0xc, OMR::X86::EVEX_L512, "62f1fd48c2c00c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::k0, TR::RealRegister::k1,
            TR::RealRegister::xmm0, TR::RealRegister::xmm0, 0xc, OMR::X86::EVEX_L512, "62f1fd49c2c00c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm1, TR::RealRegister::xmm2, 0xc, OMR::X86::EVEX_L512, "62f1f54fc2e20c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm8, TR::RealRegister::xmm2, 0xc, OMR::X86::EVEX_L512, "62f1bd4fc2e20c"),
        std::make_tuple(OP::CMPPDRegRegImm1, TR::RealRegister::k4, TR::RealRegister::k7,
            TR::RealRegister::xmm5, TR::RealRegister::xmm9, 0xc, OMR::X86::EVEX_L512, "62d1d54fc2e10c"))));

class XRegRegRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegRegRegEncEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto regB = getRealRegister(std::get<2>(GetParam()), cg());
    auto regC = getRealRegister(std::get<3>(GetParam()), cg());
    auto enc = std::get<4>(GetParam());

    auto instr = Inst_RegRegReg(std::get<0>(GetParam()), fakeNode, regA, regB, regC, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(RegRegRegParallelBitOpsEncTest, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::PDEP4RegRegReg, TR::RealRegister::eax, TR::RealRegister::ecx,
                TR::RealRegister::ebx, OMR::X86::VEX_LZ, "c4e273f5c3"),
            std::make_tuple(OP::PDEP4RegRegReg, TR::RealRegister::r15, TR::RealRegister::ecx,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c44273f5fc"),
            std::make_tuple(OP::PDEP4RegRegReg, TR::RealRegister::ecx, TR::RealRegister::r15,
                TR::RealRegister::ecx, OMR::X86::VEX_LZ, "c4e203f5c9"),
            std::make_tuple(OP::PDEP4RegRegReg, TR::RealRegister::r12, TR::RealRegister::eax,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c4427bf5e4"),
            std::make_tuple(OP::PEXT4RegRegReg, TR::RealRegister::eax, TR::RealRegister::ecx,
                TR::RealRegister::ebx, OMR::X86::VEX_LZ, "c4e272f5c3"),
            std::make_tuple(OP::PEXT4RegRegReg, TR::RealRegister::r15, TR::RealRegister::ecx,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c44272f5fc"),
            std::make_tuple(OP::PEXT4RegRegReg, TR::RealRegister::ecx, TR::RealRegister::r15,
                TR::RealRegister::ecx, OMR::X86::VEX_LZ, "c4e202f5c9"),
            std::make_tuple(OP::PEXT4RegRegReg, TR::RealRegister::r12, TR::RealRegister::eax,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c4427af5e4"),
            std::make_tuple(OP::PDEP8RegRegReg, TR::RealRegister::eax, TR::RealRegister::ecx,
                TR::RealRegister::ebx, OMR::X86::VEX_LZ, "c4e2f3f5c3"),
            std::make_tuple(OP::PDEP8RegRegReg, TR::RealRegister::r15, TR::RealRegister::ecx,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c442f3f5fc"),
            std::make_tuple(OP::PDEP8RegRegReg, TR::RealRegister::ecx, TR::RealRegister::r15,
                TR::RealRegister::ecx, OMR::X86::VEX_LZ, "c4e283f5c9"),
            std::make_tuple(OP::PDEP8RegRegReg, TR::RealRegister::r12, TR::RealRegister::eax,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c442fbf5e4"),
            std::make_tuple(OP::PEXT8RegRegReg, TR::RealRegister::eax, TR::RealRegister::ecx,
                TR::RealRegister::ebx, OMR::X86::VEX_LZ, "c4e2f2f5c3"),
            std::make_tuple(OP::PEXT8RegRegReg, TR::RealRegister::r15, TR::RealRegister::ecx,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c442f2f5fc"),
            std::make_tuple(OP::PEXT8RegRegReg, TR::RealRegister::ecx, TR::RealRegister::r15,
                TR::RealRegister::ecx, OMR::X86::VEX_LZ, "c4e282f5c9"),
            std::make_tuple(OP::PEXT8RegRegReg, TR::RealRegister::r12, TR::RealRegister::eax,
                TR::RealRegister::r12, OMR::X86::VEX_LZ, "c442faf5e4"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegRegSimdVEX128Test, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c4e2f1a8c0"),
            std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c4e279a8c9"),
            std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm2,
                TR::RealRegister::xmm2, OMR::X86::VEX_L128, "c4e26947da"),
            std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm3,
                TR::RealRegister::xmm3, OMR::X86::VEX_L128, "c4e2e147e3"),
            std::make_tuple(OP::PCMPEQBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f974c9"),
            std::make_tuple(OP::PCMPEQWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f975c9"),
            std::make_tuple(OP::PCMPEQDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f976c9"),
            std::make_tuple(OP::PCMPEQQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c4e2f929c9"),
            std::make_tuple(OP::PCMPGTBRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f964c9"),
            std::make_tuple(OP::PCMPGTWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f965c9"),
            std::make_tuple(OP::PCMPGTDRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c5f966c9"),
            std::make_tuple(OP::PCMPGTQRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::VEX_L128, "c4e2f937c9"),
            std::make_tuple(OP::ORPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c18156c7"),
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c101fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm14,
                TR::RealRegister::xmm8, OMR::X86::VEX_L128, "c4c109fdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm13,
                TR::RealRegister::xmm7, OMR::X86::VEX_L128, "c591fed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm12,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c4e199d4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm11,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c12058e7"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm10,
                TR::RealRegister::xmm8, OMR::X86::VEX_L128, "c4c1a958e8"),
            std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::xmm6, TR::RealRegister::xmm9,
                TR::RealRegister::xmm7, OMR::X86::VEX_L128, "c5b1f8f7"),
            std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::xmm7, TR::RealRegister::xmm8,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c5b9f9f8"),
            std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm7,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44141fac7"),
            std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm15,
                TR::RealRegister::xmm8, OMR::X86::VEX_L128, "c44181fbc8"),
            std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::xmm10, TR::RealRegister::xmm14,
                TR::RealRegister::xmm7, OMR::X86::VEX_L128, "c5085cd7"),
            std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm13,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c461915cd8"),
            std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm12,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44119d5e7"),
            std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm11,
                TR::RealRegister::xmm8, OMR::X86::VEX_L128, "c4422140e8"),
            std::make_tuple(OP::MULPSRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm10,
                TR::RealRegister::xmm7, OMR::X86::VEX_L128, "c52859f7"),
            std::make_tuple(OP::MULPDRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm9,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c461b159f8"),
            std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm7,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c441405eff"),
            std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm6,
                TR::RealRegister::xmm8, OMR::X86::VEX_L128, "c441c95ef0"),
            std::make_tuple(OP::PANDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm5,
                TR::RealRegister::xmm7, OMR::X86::VEX_L128, "c551dbef"),
            std::make_tuple(OP::PORRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm4,
                TR::RealRegister::xmm0, OMR::X86::VEX_L128, "c559ebe0"),
            std::make_tuple(OP::PXORRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm3,
                TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44161efdf"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegRegSimdVEX256Test, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm1,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c4e2f5a8c0"),
            std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm0,
                TR::RealRegister::ymm1, OMR::X86::VEX_L256, "c4e27da8c9"),
            std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::VEX_L256, "c4e26d47da"),
            std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm3,
                TR::RealRegister::ymm3, OMR::X86::VEX_L256, "c4e2e547e3"),
            std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::VEX_L256, "c4e26d46da"),
            std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::VEX_L256, "c4e26d45da"),
            std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm3,
                TR::RealRegister::ymm3, OMR::X86::VEX_L256, "c4e2e545e3"),
            std::make_tuple(OP::ORPDRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c18556c7"),
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c105fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm14,
                TR::RealRegister::ymm8, OMR::X86::VEX_L256, "c4c10dfdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm13,
                TR::RealRegister::ymm7, OMR::X86::VEX_L256, "c595fed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm12,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c4e19dd4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm11,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c12458e7"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm10,
                TR::RealRegister::ymm8, OMR::X86::VEX_L256, "c4c1ad58e8"),
            std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::ymm6, TR::RealRegister::ymm9,
                TR::RealRegister::ymm7, OMR::X86::VEX_L256, "c5b5f8f7"),
            std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::ymm7, TR::RealRegister::ymm8,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c5bdf9f8"),
            std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm7,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c44145fac7"),
            std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::ymm9, TR::RealRegister::ymm15,
                TR::RealRegister::ymm8, OMR::X86::VEX_L256, "c44185fbc8"),
            std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::ymm10, TR::RealRegister::ymm14,
                TR::RealRegister::ymm7, OMR::X86::VEX_L256, "c50c5cd7"),
            std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm13,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c461955cd8"),
            std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm12,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4411dd5e7"),
            std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm11,
                TR::RealRegister::ymm8, OMR::X86::VEX_L256, "c4422540e8"),
            std::make_tuple(OP::MULPSRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm10,
                TR::RealRegister::ymm7, OMR::X86::VEX_L256, "c52c59f7"),
            std::make_tuple(OP::MULPDRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm9,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c461b559f8"),
            std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm7,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c441445eff"),
            std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm6,
                TR::RealRegister::ymm8, OMR::X86::VEX_L256, "c441cd5ef0"),
            std::make_tuple(OP::PANDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm5,
                TR::RealRegister::ymm7, OMR::X86::VEX_L256, "c555dbef"),
            std::make_tuple(OP::PORRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm4,
                TR::RealRegister::ymm0, OMR::X86::VEX_L256, "c55debe0"),
            std::make_tuple(OP::PXORRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm3,
                TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c44165efdf"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegRegSimdEVEX128Test, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "62f2f508a8c0"),
            std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm0,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f27d08a8c9"),
            std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm1,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f2f50812d1"),
            std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm2,
                TR::RealRegister::xmm2, OMR::X86::EVEX_L128, "62f26d0847da"),
            std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm3,
                TR::RealRegister::xmm3, OMR::X86::EVEX_L128, "62f2e50847e3"),
            std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm1,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f2f50811c1"),
            std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm8,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "62d23d0846c8"),
            std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d2850846d7"),
            std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm1,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f2f50810d9"),
            std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm2,
                TR::RealRegister::xmm2, OMR::X86::EVEX_L128, "62f26d0845e2"),
            std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm3,
                TR::RealRegister::xmm3, OMR::X86::EVEX_L128, "62f2e50845eb"),
            std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d2050815d7"),
            std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm1,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f2f50815d9"),
            std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm2,
                TR::RealRegister::xmm2, OMR::X86::EVEX_L128, "62f26d0814e2"),
            std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm3,
                TR::RealRegister::xmm3, OMR::X86::EVEX_L128, "62f2e50814eb"),
            std::make_tuple(OP::ORPDRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d1850856c7"),
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::xmm15,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d10508fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::xmm1, TR::RealRegister::xmm14,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "62d10d08fdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::xmm2, TR::RealRegister::xmm13,
                TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62f11508fed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::xmm12,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "62f19d08d4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::xmm11,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d1240858e7"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::xmm10,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "62d1ad0858e8"),
            std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::xmm6, TR::RealRegister::xmm9,
                TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62f13508f8f7"),
            std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::xmm7, TR::RealRegister::xmm8,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "62f13d08f9f8"),
            std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::xmm8, TR::RealRegister::xmm7,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62514508fac7"),
            std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::xmm9, TR::RealRegister::xmm15,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "62518508fbc8"),
            std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::xmm10, TR::RealRegister::xmm14,
                TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62710c085cd7"),
            std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm13,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "627195085cd8"),
            std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm12,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62511d08d5e7"),
            std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm11,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "6252250840e8"),
            std::make_tuple(OP::MULPSRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm10,
                TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62712c0859f7"),
            std::make_tuple(OP::MULPDRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm9,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "6271b50859f8"),
            std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::xmm15, TR::RealRegister::xmm7,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "625144085eff"),
            std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::xmm14, TR::RealRegister::xmm6,
                TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "6251cd085ef0"),
            std::make_tuple(OP::PANDRegReg, TR::RealRegister::xmm13, TR::RealRegister::xmm5,
                TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62715508dbef"),
            std::make_tuple(OP::PORRegReg, TR::RealRegister::xmm12, TR::RealRegister::xmm4,
                TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "62715d08ebe0"),
            std::make_tuple(OP::PXORRegReg, TR::RealRegister::xmm11, TR::RealRegister::xmm3,
                TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62516508efdf"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegRegSimdEVEX256Test, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm1,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "62f2f528a8c0"),
            std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm0,
                TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f27d28a8c9"),
            std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm1,
                TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f2f52812d1"),
            std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::EVEX_L256, "62f26d2847da"),
            std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm3,
                TR::RealRegister::ymm3, OMR::X86::EVEX_L256, "62f2e52847e3"),
            std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm1,
                TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f2f52811c1"),
            std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm8,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "62d23d2846c8"),
            std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d2852846d7"),
            std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm1,
                TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f2f52810d9"),
            std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::EVEX_L256, "62f26d2845e2"),
            std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm3,
                TR::RealRegister::ymm3, OMR::X86::EVEX_L256, "62f2e52845eb"),
            std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d2052815d7"),
            std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm1,
                TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f2f52815d9"),
            std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm2,
                TR::RealRegister::ymm2, OMR::X86::EVEX_L256, "62f26d2814e2"),
            std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm3,
                TR::RealRegister::ymm3, OMR::X86::EVEX_L256, "62f2e52814eb"),
            std::make_tuple(OP::ORPDRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d1852856c7"),
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::ymm0, TR::RealRegister::ymm15,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d10528fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::ymm14,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "62d10d28fdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::ymm2, TR::RealRegister::ymm13,
                TR::RealRegister::ymm7, OMR::X86::EVEX_L256, "62f11528fed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::ymm3, TR::RealRegister::ymm12,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "62f19d28d4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::ymm4, TR::RealRegister::ymm11,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d1242858e7"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::ymm5, TR::RealRegister::ymm10,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "62d1ad2858e8"),
            std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::ymm6, TR::RealRegister::ymm9,
                TR::RealRegister::ymm7, OMR::X86::EVEX_L256, "62f13528f8f7"),
            std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::ymm7, TR::RealRegister::ymm8,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "62f13d28f9f8"),
            std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::ymm8, TR::RealRegister::ymm7,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62514528fac7"),
            std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::ymm9, TR::RealRegister::ymm15,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "62518528fbc8"),
            std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::ymm10, TR::RealRegister::ymm14,
                TR::RealRegister::ymm7, OMR::X86::EVEX_L256, "62710c285cd7"),
            std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm13,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "627195285cd8"),
            std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm12,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62511d28d5e7"),
            std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm11,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "6252252840e8"),
            std::make_tuple(OP::MULPSRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm10,
                TR::RealRegister::ymm7, OMR::X86::EVEX_L256, "62712c2859f7"),
            std::make_tuple(OP::MULPDRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm9,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "6271b52859f8"),
            std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::ymm15, TR::RealRegister::ymm7,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "625144285eff"),
            std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::ymm14, TR::RealRegister::ymm6,
                TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "6251cd285ef0"),
            std::make_tuple(OP::PANDRegReg, TR::RealRegister::ymm13, TR::RealRegister::ymm5,
                TR::RealRegister::ymm7, OMR::X86::EVEX_L256, "62715528dbef"),
            std::make_tuple(OP::PORRegReg, TR::RealRegister::ymm12, TR::RealRegister::ymm4,
                TR::RealRegister::ymm0, OMR::X86::EVEX_L256, "62715d28ebe0"),
            std::make_tuple(OP::PXORRegReg, TR::RealRegister::ymm11, TR::RealRegister::ymm3,
                TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62516528efdf"))));

INSTANTIATE_TEST_CASE_P(AVXRegRegRegSimdEVEX512Test, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm1,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "62f2f548a8c0"),
            std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm0,
                TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f27d48a8c9"),
            std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm1,
                TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f2f54812d1"),
            std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::zmm3, TR::RealRegister::zmm2,
                TR::RealRegister::zmm2, OMR::X86::EVEX_L512, "62f26d4847da"),
            std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::zmm4, TR::RealRegister::zmm3,
                TR::RealRegister::zmm3, OMR::X86::EVEX_L512, "62f2e54847e3"),
            std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm1,
                TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f2f54811c1"),
            std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm8,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "62d23d4846c8"),
            std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm15,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d2854846d7"),
            std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::zmm3, TR::RealRegister::zmm1,
                TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f2f54810d9"),
            std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::zmm4, TR::RealRegister::zmm2,
                TR::RealRegister::zmm2, OMR::X86::EVEX_L512, "62f26d4845e2"),
            std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::zmm5, TR::RealRegister::zmm3,
                TR::RealRegister::zmm3, OMR::X86::EVEX_L512, "62f2e54845eb"),
            std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm15,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d2054815d7"),
            std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::zmm3, TR::RealRegister::zmm1,
                TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f2f54815d9"),
            std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::zmm4, TR::RealRegister::zmm2,
                TR::RealRegister::zmm2, OMR::X86::EVEX_L512, "62f26d4814e2"),
            std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::zmm5, TR::RealRegister::zmm3,
                TR::RealRegister::zmm3, OMR::X86::EVEX_L512, "62f2e54814eb"),
            std::make_tuple(OP::ORPDRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm15,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d1854856c7"),
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::zmm0, TR::RealRegister::zmm15,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d10548fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::zmm1, TR::RealRegister::zmm14,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "62d10d48fdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::zmm2, TR::RealRegister::zmm13,
                TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62f11548fed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::zmm3, TR::RealRegister::zmm12,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "62f19d48d4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::zmm4, TR::RealRegister::zmm11,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d1244858e7"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::zmm5, TR::RealRegister::zmm10,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "62d1ad4858e8"),
            std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::zmm6, TR::RealRegister::zmm9,
                TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62f13548f8f7"),
            std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::zmm7, TR::RealRegister::zmm8,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "62f13d48f9f8"),
            std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::zmm8, TR::RealRegister::zmm7,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62514548fac7"),
            std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::zmm9, TR::RealRegister::zmm15,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "62518548fbc8"),
            std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::zmm10, TR::RealRegister::zmm14,
                TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62710c485cd7"),
            std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::zmm11, TR::RealRegister::zmm13,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "627195485cd8"),
            std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::zmm12, TR::RealRegister::zmm12,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62511d48d5e7"),
            std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::zmm13, TR::RealRegister::zmm11,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "6252254840e8"),
            std::make_tuple(OP::MULPSRegReg, TR::RealRegister::zmm14, TR::RealRegister::zmm10,
                TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62712c4859f7"),
            std::make_tuple(OP::MULPDRegReg, TR::RealRegister::zmm15, TR::RealRegister::zmm9,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "6271b54859f8"),
            std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::zmm15, TR::RealRegister::zmm7,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "625144485eff"),
            std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::zmm14, TR::RealRegister::zmm6,
                TR::RealRegister::zmm8, OMR::X86::EVEX_L512, "6251cd485ef0"),
            std::make_tuple(OP::PANDRegReg, TR::RealRegister::zmm13, TR::RealRegister::zmm5,
                TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62715548dbef"),
            std::make_tuple(OP::PORRegReg, TR::RealRegister::zmm12, TR::RealRegister::zmm4,
                TR::RealRegister::zmm0, OMR::X86::EVEX_L512, "62715d48ebe0"),
            std::make_tuple(OP::PXORRegReg, TR::RealRegister::zmm11, TR::RealRegister::zmm3,
                TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62516548efdf"))));

INSTANTIATE_TEST_CASE_P(AVX512MaskRegSimdTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPMOVB2MRegReg, TR::RealRegister::k1, TR::RealRegister::xmm1,
            OMR::X86::EVEX_L128, "62f27e0829c9"),
        std::make_tuple(OP::VPMOVW2MRegReg, TR::RealRegister::k2, TR::RealRegister::xmm8,
            OMR::X86::EVEX_L128, "62d2fe0829d0"),
        std::make_tuple(OP::VPMOVD2MRegReg, TR::RealRegister::k3, TR::RealRegister::xmm2,
            OMR::X86::EVEX_L128, "62f27e0839da"),
        std::make_tuple(OP::VPMOVQ2MRegReg, TR::RealRegister::k4, TR::RealRegister::xmm15,
            OMR::X86::EVEX_L128, "62d2fe0839e7"))));

INSTANTIATE_TEST_CASE_P(AVX512RegMaskSimdTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPMOVM2BRegReg, TR::RealRegister::xmm1, TR::RealRegister::k5,
            OMR::X86::EVEX_L128, "62f27e0828cd"),
        std::make_tuple(OP::VPMOVM2WRegReg, TR::RealRegister::xmm8, TR::RealRegister::k6,
            OMR::X86::EVEX_L128, "6272fe0828c6"),
        std::make_tuple(OP::VPMOVM2DRegReg, TR::RealRegister::xmm2, TR::RealRegister::k7,
            OMR::X86::EVEX_L128, "62f27e0838d7"),
        std::make_tuple(OP::VPMOVM2QRegReg, TR::RealRegister::xmm15, TR::RealRegister::k1,
            OMR::X86::EVEX_L128, "6272fe0838f9"))));

INSTANTIATE_TEST_CASE_P(AVX512MaskMaskSimdTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::KNOTBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2, OMR::X86::VEX_L128,
            "c5f944ca"),
        std::make_tuple(OP::KNOTWRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4, OMR::X86::VEX_L128,
            "c5f844dc"),
        std::make_tuple(OP::KNOTDRegRegReg, TR::RealRegister::k5, TR::RealRegister::k6, OMR::X86::VEX_L128,
            "c4e1f944ee"),
        std::make_tuple(OP::KNOTQRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1, OMR::X86::VEX_L128,
            "c4e1f844f9"))));

INSTANTIATE_TEST_CASE_P(AVX512MaskMaskMaskTest, XRegRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::KORBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2,
                TR::RealRegister::k3, OMR::X86::VEX_L256, "c5ed45cb"),
            std::make_tuple(OP::KORWRegRegReg, TR::RealRegister::k4, TR::RealRegister::k5,
                TR::RealRegister::k6, OMR::X86::VEX_L256, "c5d445e6"),
            std::make_tuple(OP::KORDRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1,
                TR::RealRegister::k2, OMR::X86::VEX_L256, "c4e1f545fa"),
            std::make_tuple(OP::KORQRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4,
                TR::RealRegister::k5, OMR::X86::VEX_L256, "c4e1dc45dd"),

            std::make_tuple(OP::KANDBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2,
                TR::RealRegister::k3, OMR::X86::VEX_L256, "c5ed41cb"),
            std::make_tuple(OP::KANDWRegRegReg, TR::RealRegister::k4, TR::RealRegister::k5,
                TR::RealRegister::k6, OMR::X86::VEX_L256, "c5d441e6"),
            std::make_tuple(OP::KANDDRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1,
                TR::RealRegister::k2, OMR::X86::VEX_L256, "c4e1f541fa"),
            std::make_tuple(OP::KANDQRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4,
                TR::RealRegister::k5, OMR::X86::VEX_L256, "c4e1dc41dd"),

            std::make_tuple(OP::KXORBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2,
                TR::RealRegister::k3, OMR::X86::VEX_L256, "c5ed47cb"),
            std::make_tuple(OP::KXORWRegRegReg, TR::RealRegister::k4, TR::RealRegister::k5,
                TR::RealRegister::k6, OMR::X86::VEX_L256, "c5d447e6"),
            std::make_tuple(OP::KXORDRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1,
                TR::RealRegister::k2, OMR::X86::VEX_L256, "c4e1f547fa"),
            std::make_tuple(OP::KXORQRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4,
                TR::RealRegister::k5, OMR::X86::VEX_L256, "c4e1dc47dd"),

            std::make_tuple(OP::KANDNBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2,
                TR::RealRegister::k3, OMR::X86::VEX_L256, "c5ed42cb"),
            std::make_tuple(OP::KANDNWRegRegReg, TR::RealRegister::k4, TR::RealRegister::k5,
                TR::RealRegister::k6, OMR::X86::VEX_L256, "c5d442e6"),
            std::make_tuple(OP::KANDNDRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1,
                TR::RealRegister::k2, OMR::X86::VEX_L256, "c4e1f542fa"),
            std::make_tuple(OP::KANDNQRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4,
                TR::RealRegister::k5, OMR::X86::VEX_L256, "c4e1dc42dd"),

            std::make_tuple(OP::KXNORBRegRegReg, TR::RealRegister::k1, TR::RealRegister::k2,
                TR::RealRegister::k3, OMR::X86::VEX_L256, "c5ed46cb"),
            std::make_tuple(OP::KXNORWRegRegReg, TR::RealRegister::k4, TR::RealRegister::k5,
                TR::RealRegister::k6, OMR::X86::VEX_L256, "c5d446e6"),
            std::make_tuple(OP::KXNORDRegRegReg, TR::RealRegister::k7, TR::RealRegister::k1,
                TR::RealRegister::k2, OMR::X86::VEX_L256, "c4e1f546fa"),
            std::make_tuple(OP::KXNORQRegRegReg, TR::RealRegister::k3, TR::RealRegister::k4,
                TR::RealRegister::k5, OMR::X86::VEX_L256, "c4e1dc46dd"))));

INSTANTIATE_TEST_CASE_P(GeneralPurposeRegRegTest, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::LZCNT2RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "66f30fbdc1"),
        std::make_tuple(OP::LZCNT4RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "f30fbdc1"),
        std::make_tuple(OP::LZCNT8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "f3480fbdc1"),

        std::make_tuple(OP::TZCNT2RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "66f30fbcc1"),
        std::make_tuple(OP::TZCNT4RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "f30fbcc1"),
        std::make_tuple(OP::TZCNT8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, OMR::X86::Default,
            "f3480fbcc1"))));

INSTANTIATE_TEST_CASE_P(Branch, XRegRegEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::XOR4RegReg, TR::RealRegister::eax, TR::RealRegister::eax, "33c0"),
        std::make_tuple(OP::XOR4RegReg, TR::RealRegister::ecx, TR::RealRegister::ebp, "33cd"),
        std::make_tuple(OP::ADD4RegReg, TR::RealRegister::eax, TR::RealRegister::eax, "03c0"),
        std::make_tuple(OP::SUB4RegReg, TR::RealRegister::eax, TR::RealRegister::eax, "2bc0"),
        std::make_tuple(OP::IMUL4RegReg, TR::RealRegister::eax, TR::RealRegister::eax, "0fafc0"),

        std::make_tuple(OP::IMUL8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480fafc1"),
        std::make_tuple(OP::IMUL8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480fafc8"),

        std::make_tuple(OP::CMOVG8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f4fc1"),
        std::make_tuple(OP::CMOVG8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f4fc8"),
        std::make_tuple(OP::CMOVE8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f44c1"),
        std::make_tuple(OP::CMOVE8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f44c8"),

        std::make_tuple(OP::XCHG8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4887c8"),
        std::make_tuple(OP::XCHG8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4887c1"),

        std::make_tuple(OP::MOV8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "488bc1"),
        std::make_tuple(OP::MOV8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "488bc8"),

        std::make_tuple(OP::CMOVB8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f42c1"),
        std::make_tuple(OP::CMOVB8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f42c8"),

        std::make_tuple(OP::POPCNT8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "f3480fb8c1"),
        std::make_tuple(OP::POPCNT8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "f3480fb8c8"),

        std::make_tuple(OP::CMOVL8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f4cc1"),
        std::make_tuple(OP::CMOVL8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f4cc8"),

        std::make_tuple(OP::ADD8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4803c1"),
        std::make_tuple(OP::ADD8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4803c8"),
        std::make_tuple(OP::ADC8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4813c1"),
        std::make_tuple(OP::ADC8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4813c8"),

        std::make_tuple(OP::TEST8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4885c1"),
        std::make_tuple(OP::TEST8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4885c8"),

        std::make_tuple(OP::CMOVLE8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f4ec1"),
        std::make_tuple(OP::CMOVLE8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f4ec8"),

        std::make_tuple(OP::BSF8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480fbcc1"),
        std::make_tuple(OP::BSF8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480fbcc8"),

        std::make_tuple(OP::SHLD4RegRegCL, TR::RealRegister::eax, TR::RealRegister::ecx, "0fa5c8"),
        std::make_tuple(OP::SHLD4RegRegCL, TR::RealRegister::ecx, TR::RealRegister::eax, "0fa5c1"),

        std::make_tuple(OP::BSR8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480fbdc1"),
        std::make_tuple(OP::BSR8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480fbdc8"),

        std::make_tuple(OP::SUB8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "482bc1"),
        std::make_tuple(OP::SUB8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "482bc8"),

        std::make_tuple(OP::CMP8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "483bc1"),
        std::make_tuple(OP::CMP8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "483bc8"),

        std::make_tuple(OP::CMOVGE8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f4dc1"),
        std::make_tuple(OP::CMOVGE8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f4dc8"),

        std::make_tuple(OP::BT8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480fa3c8"),
        std::make_tuple(OP::BT8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480fa3c1"),

        std::make_tuple(OP::AND8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4823c1"),
        std::make_tuple(OP::AND8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4823c8"),

        std::make_tuple(OP::XOR8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "4833c1"),
        std::make_tuple(OP::XOR8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "4833c8"),

        std::make_tuple(OP::BTS4RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "0fabc8"),
        std::make_tuple(OP::BTS4RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "0fabc1"),

        std::make_tuple(OP::OR8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480bc1"),
        std::make_tuple(OP::OR8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480bc8"),

        std::make_tuple(OP::CMOVS8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f48c1"),
        std::make_tuple(OP::CMOVS8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f48c8"),

        std::make_tuple(OP::CMOVNE8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "480f45c1"),
        std::make_tuple(OP::CMOVNE8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "480f45c8"),

        std::make_tuple(OP::SBB8RegReg, TR::RealRegister::eax, TR::RealRegister::ecx, "481bc1"),
        std::make_tuple(OP::SBB8RegReg, TR::RealRegister::ecx, TR::RealRegister::eax, "481bc8"),

        std::make_tuple(OP::SHRD4RegRegCL, TR::RealRegister::eax, TR::RealRegister::ecx, "0fadc8"),
        std::make_tuple(OP::SHRD4RegRegCL, TR::RealRegister::ecx, TR::RealRegister::eax, "0fadc1"))));

class XRegMemEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegMemEncEncodingTest, encode)
{
    auto target = getRealRegister(std::get<1>(GetParam()), cg());
    auto base = getRealRegister(std::get<2>(GetParam()), cg());
    auto disp = std::get<3>(GetParam());
    auto enc = std::get<4>(GetParam());

    auto mr = MRef_Bdisp32(base, disp, cg());
    auto instr = Inst_RegMem(std::get<0>(GetParam()), fakeNode, target, mr, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(X86RegMemEnc, XRegMemEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::Legacy, "f30f6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::VEX_L128, "c5fa6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::VEX_L256, "c5fe6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62f17e086f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L256, "62717e286f10"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L512, "62717e486f10"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::Legacy, "66480f514908"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::VEX_L128, "c4e1f9514908"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::VEX_L256, "c461fd5110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L128, "6271fd085110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x10,
            OMR::X86::EVEX_L128, "6271fd08515001"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x1,
            OMR::X86::EVEX_L128, "6271fd08519001000000"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L256, "6271fd285110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x20,
            OMR::X86::EVEX_L256, "6271fd28515001"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x2,
            OMR::X86::EVEX_L256, "6271fd28519002000000"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L512, "6271fd485110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x4,
            OMR::X86::EVEX_L512, "6271fd48519004000000"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x40,
            OMR::X86::EVEX_L512, "6271fd48515001"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::Legacy, "f30f6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::VEX_L128, "c5fa6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::VEX_L256, "c5fe6f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62f17e086f09"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L256, "62717e286f10"),
        std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L512, "62717e486f10"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::Legacy, "66480f514908"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::VEX_L128, "c4e1f9514908"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::VEX_L256, "c461fd5110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L128, "6271fd085110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L256, "6271fd285110"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L512, "6271fd485110"),
        std::make_tuple(OP::PMOVSXBDRegMem, TR::RealRegister::zmm1, TR::RealRegister::eax, 0x0,
            OMR::X86::EVEX_L512, "62f27d482108"),
        std::make_tuple(OP::PMOVSXWDRegMem, TR::RealRegister::zmm2, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L512, "62f27d48239108000000"))));

INSTANTIATE_TEST_CASE_P(X86MaskMemEnc, XRegMemEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::KMOVBMaskMem, TR::RealRegister::k1, TR::RealRegister::esp, 0x4,
            OMR::X86::VEX_L128, "c5f9904c2404"),
        std::make_tuple(OP::KMOVWMaskMem, TR::RealRegister::k2, TR::RealRegister::esp, 0x4,
            OMR::X86::VEX_L128, "c5f890542404"),
        std::make_tuple(OP::KMOVDMaskMem, TR::RealRegister::k3, TR::RealRegister::esp, 0x4,
            OMR::X86::VEX_L128, "c4e1f9905c2404"),
        std::make_tuple(OP::KMOVQMaskMem, TR::RealRegister::k4, TR::RealRegister::esp, 0x4,
            OMR::X86::VEX_L128, "c4e1f890642404"))));

class XMemRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XMemRegEncEncodingTest, encode)
{
    auto target = getRealRegister(std::get<1>(GetParam()), cg());
    auto base = getRealRegister(std::get<2>(GetParam()), cg());
    auto disp = std::get<3>(GetParam());
    auto enc = std::get<4>(GetParam());

    auto mr = MRef_Bdisp32(base, disp, cg());
    auto instr = Inst_MemReg(std::get<0>(GetParam()), fakeNode, mr, target, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(X86MemMaskEnc, XMemRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::KMOVBMemMask, TR::RealRegister::k7, TR::RealRegister::esp, 0x0,
            OMR::X86::VEX_L128, "c5f9913c24"),
        std::make_tuple(OP::KMOVWMemMask, TR::RealRegister::k6, TR::RealRegister::esp, 0x4,
            OMR::X86::VEX_L128, "c5f891742404"),
        std::make_tuple(OP::KMOVDMemMask, TR::RealRegister::k5, TR::RealRegister::esp, 0x8,
            OMR::X86::VEX_L128, "c4e1f9916c2408"),
        std::make_tuple(OP::KMOVQMemMask, TR::RealRegister::k4, TR::RealRegister::esp, 0x0,
            OMR::X86::VEX_L128, "c4e1f8912424"))));

INSTANTIATE_TEST_CASE_P(MaskMaskEnc, XRegRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::KMOVBMaskMask, TR::RealRegister::k1, TR::RealRegister::k7, OMR::X86::VEX_L128,
            "c5f990cf"),
        std::make_tuple(OP::KMOVWMaskMask, TR::RealRegister::k2, TR::RealRegister::k6, OMR::X86::VEX_L128,
            "c5f890d6"),
        std::make_tuple(OP::KMOVDMaskMask, TR::RealRegister::k3, TR::RealRegister::k5, OMR::X86::VEX_L128,
            "c4e1f990dd"),
        std::make_tuple(OP::KMOVQMaskMask, TR::RealRegister::k4, TR::RealRegister::k4, OMR::X86::VEX_L128,
            "c4e1f890e4"),
        std::make_tuple(OP::KMOVWRegMask, TR::RealRegister::eax, TR::RealRegister::k4, OMR::X86::VEX_L128,
            "c5f893c4"),
        std::make_tuple(OP::KMOVWMaskReg, TR::RealRegister::k3, TR::RealRegister::ebx, OMR::X86::VEX_L128,
            "c5f892db"),
        std::make_tuple(OP::KMOVQRegMask, TR::RealRegister::ecx, TR::RealRegister::k2, OMR::X86::VEX_L128,
            "c4e1fb93ca"),
        std::make_tuple(OP::KMOVQMaskReg, TR::RealRegister::k1, TR::RealRegister::edx, OMR::X86::VEX_L128,
            "c4e1fb92ca"),
        std::make_tuple(OP::KTESTBRegReg, TR::RealRegister::k2, TR::RealRegister::k4, OMR::X86::VEX_L128,
            "c5f999d4"),
        std::make_tuple(OP::KTESTWRegReg, TR::RealRegister::k3, TR::RealRegister::k3, OMR::X86::VEX_L128,
            "c5f899db"),
        std::make_tuple(OP::KTESTDRegReg, TR::RealRegister::k4, TR::RealRegister::k2, OMR::X86::VEX_L128,
            "c4e1f999e2"),
        std::make_tuple(OP::KTESTQRegReg, TR::RealRegister::k5, TR::RealRegister::k1, OMR::X86::VEX_L128,
            "c4e1f899e9"))));

class XRegMaskRegRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<
          std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
              TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegMaskRegRegEncEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto maskReg = getRealRegister(std::get<2>(GetParam()), cg());
    auto regB = getRealRegister(std::get<3>(GetParam()), cg());
    auto regC = getRealRegister(std::get<4>(GetParam()), cg());
    auto enc = std::get<5>(GetParam());

    auto instr = Inst_RegMaskRegReg(std::get<0>(GetParam()), fakeNode, regA, maskReg, regB, regC, cg(), enc);

    ASSERT_EQ(std::get<6>(GetParam()), encodeInstruction(instr));
}

class XRegMaskRegRegImmEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding,
          TRTest::BinaryInstruction> > {};

TEST_P(XRegMaskRegRegImmEncEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto maskReg = getRealRegister(std::get<2>(GetParam()), cg());
    auto regB = getRealRegister(std::get<3>(GetParam()), cg());
    auto regC = getRealRegister(std::get<4>(GetParam()), cg());
    auto imm = std::get<5>(GetParam());
    auto enc = std::get<6>(GetParam());

    auto instr = Inst_RegMaskRegRegImm(std::get<0>(GetParam()), fakeNode, regA, maskReg, regB, regC, imm, cg(), enc);

    ASSERT_EQ(std::get<7>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(XRegMaskRegRegEncTest, XRegMaskRegRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::k1,
                TR::RealRegister::xmm4, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d15d09fcc7"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::k2,
                TR::RealRegister::ymm3, TR::RealRegister::ymm8, OMR::X86::EVEX_L256, "62d1652afdc8"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::zmm2, TR::RealRegister::k3,
                TR::RealRegister::zmm2, TR::RealRegister::zmm7, OMR::X86::EVEX_L512, "62f16d4bfed7"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::k4,
                TR::RealRegister::xmm1, TR::RealRegister::xmm0, OMR::X86::EVEX_L128, "62f1f50cd4d8"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::k5,
                TR::RealRegister::xmm2, TR::RealRegister::xmm8, OMR::X86::EVEX_L128, "62d16c0d58e0"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::k6,
                TR::RealRegister::xmm3, TR::RealRegister::xmm7, OMR::X86::EVEX_L128, "62f1e50e58ef"))));

INSTANTIATE_TEST_CASE_P(XRegMaskRegRegImmEncTest, XRegMaskRegRegImmEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t,
                            OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPCMPBMaskMaskRegRegImm, TR::RealRegister::k1, TR::RealRegister::k4,
            TR::RealRegister::xmm1, TR::RealRegister::xmm4, 1, OMR::X86::EVEX_L128, "62f3750c3fcc01"),
        std::make_tuple(OP::VPCMPWMaskMaskRegRegImm, TR::RealRegister::k2, TR::RealRegister::k3,
            TR::RealRegister::ymm2, TR::RealRegister::ymm3, 2, OMR::X86::EVEX_L256, "62f3ed2b3fd302"))));

class XRegMaskRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
          TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegMaskRegEncEncodingTest, encode)
{
    auto regA = getRealRegister(std::get<1>(GetParam()), cg());
    auto maskReg = getRealRegister(std::get<2>(GetParam()), cg());
    auto regB = getRealRegister(std::get<3>(GetParam()), cg());
    auto enc = std::get<4>(GetParam());

    auto instr = Inst_RegMaskReg(std::get<0>(GetParam()), fakeNode, regA, maskReg, regB, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(XRegMaskRegEncTest, XRegMaskRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm0, TR::RealRegister::k1,
                TR::RealRegister::xmm4, OMR::X86::EVEX_L128, "62f17d09fcc4"),
            std::make_tuple(OP::PADDWRegReg, TR::RealRegister::ymm1, TR::RealRegister::k2,
                TR::RealRegister::ymm3, OMR::X86::EVEX_L256, "62f1752afdcb"),
            std::make_tuple(OP::PADDDRegReg, TR::RealRegister::zmm2, TR::RealRegister::k3,
                TR::RealRegister::zmm2, OMR::X86::EVEX_L512, "62f16d4bfed2"),
            std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm3, TR::RealRegister::k4,
                TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f1e50cd4d9"),
            std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm4, TR::RealRegister::k5,
                TR::RealRegister::xmm2, OMR::X86::EVEX_L128, "62f15c0d58e2"),
            std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm5, TR::RealRegister::k6,
                TR::RealRegister::xmm3, OMR::X86::EVEX_L128, "62f1d50e58eb"),
            std::make_tuple(OP::MOVDQURegReg, TR::RealRegister::xmm6, TR::RealRegister::k6,
                TR::RealRegister::xmm4, OMR::X86::EVEX_L128, "62f17e0e6ff4"),
            std::make_tuple(OP::MOVDQURegReg, TR::RealRegister::xmm7, TR::RealRegister::k6,
                TR::RealRegister::xmm5, OMR::X86::EVEX_L256, "62f17e2e6ffd"),
            std::make_tuple(OP::MOVDQURegReg, TR::RealRegister::xmm8, TR::RealRegister::k6,
                TR::RealRegister::xmm6, OMR::X86::EVEX_L512, "62717e4e6fc6"))));

class XMemEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<
          std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, int32_t, TRTest::BinaryInstruction> > {};

TEST_P(XMemEncEncodingTest, encode)
{
    auto base = getRealRegister(std::get<1>(GetParam()), cg());
    auto disp = std::get<2>(GetParam());

    auto mr = MRef_Bdisp32(base, disp, cg());
    auto instr = Inst_Mem(std::get<0>(GetParam()), fakeNode, mr, cg());
    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(X86MemEnc, XMemEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<
            std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, int32_t, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::CLWBMem, TR::RealRegister::eax, 0, "660fae30"))));

class XRegMaskMemEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<
          std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
              TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XRegMaskMemEncEncodingTest, encode)
{
    auto target = getRealRegister(std::get<1>(GetParam()), cg());
    auto mask = getRealRegister(std::get<2>(GetParam()), cg());
    auto base = getRealRegister(std::get<3>(GetParam()), cg());
    auto disp = std::get<4>(GetParam());
    auto enc = std::get<5>(GetParam());

    auto mr = MRef_Bdisp32(base, disp, cg());
    auto instr = Inst_RegMaskMem(std::get<0>(GetParam()), fakeNode, target, mask, mr, cg(), enc);

    ASSERT_EQ(std::get<6>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(X86RegMaskMemEnc, XRegMaskMemEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::k7,
                TR::RealRegister::ecx, 0x0, OMR::X86::EVEX_L128, "62f17e0f6f09"),
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::k1,
                TR::RealRegister::ecx, 0x0, OMR::X86::EVEX_L128, "62f17e096f09"),
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::k2,
                TR::RealRegister::ecx, 0x0, OMR::X86::EVEX_L128, "62f17e0a6f09"),
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm1, TR::RealRegister::k3,
                TR::RealRegister::ecx, 0x0, OMR::X86::EVEX_L128, "62f17e0b6f09"),
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::k4,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L256, "62717e2c6f10"),
            std::make_tuple(OP::MOVDQURegMem, TR::RealRegister::xmm10, TR::RealRegister::k5,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L512, "62717e4d6f10"),
            std::make_tuple(OP::VMOVDQU8RegMem, TR::RealRegister::xmm10, TR::RealRegister::k5,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L512, "62717f4d6f10"),
            std::make_tuple(OP::VMOVDQU16RegMem, TR::RealRegister::xmm10, TR::RealRegister::k5,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L512, "6271ff4d6f10"),
            std::make_tuple(OP::VMOVDQU64RegMem, TR::RealRegister::xmm10, TR::RealRegister::k5,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L512, "6271fe4d6f10"),
            std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::k6,
                TR::RealRegister::ecx, 0x8, OMR::X86::EVEX_L128, "62f1fd0e518908000000"),
            std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::k7,
                TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L128, "6271fd0f5110"))));

class XMemMaskRegEncEncodingTest
    : public TRTest::BinaryEncoderTest<>
    , public ::testing::WithParamInterface<
          std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
              TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> > {};

TEST_P(XMemMaskRegEncEncodingTest, encode)
{
    auto target = getRealRegister(std::get<1>(GetParam()), cg());
    auto mask = getRealRegister(std::get<2>(GetParam()), cg());
    auto base = getRealRegister(std::get<3>(GetParam()), cg());
    auto disp = std::get<4>(GetParam());
    auto enc = std::get<5>(GetParam());

    auto mr = MRef_Bdisp32(base, disp, cg());
    auto instr = Inst_MemMaskReg(std::get<0>(GetParam()), fakeNode, mr, mask, target, cg(), enc);

    ASSERT_EQ(std::get<6>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_CASE_P(X86MemMaskRegEnc, XMemMaskRegEncEncodingTest,
    ::testing::ValuesIn(
        *TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum,
            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
            std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm0, TR::RealRegister::k7,
                TR::RealRegister::esp, 0x0, OMR::X86::EVEX_L128, "62f17e0f7f0424"),
            std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::k6,
                TR::RealRegister::esp, 0x4, OMR::X86::EVEX_L128, "62f17e0e7f8c2404000000"),
            std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm2, TR::RealRegister::k5,
                TR::RealRegister::esp, 0x8, OMR::X86::EVEX_L128, "62f17e0d7f942408000000"),
            std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm3, TR::RealRegister::k4,
                TR::RealRegister::esp, 0x0, OMR::X86::EVEX_L128, "62f17e0c7f1c24"))));

INSTANTIATE_TEST_CASE_P(X86MemRegDisplacement, XMemRegEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62F17E087F09"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L128, "62F17E087F8901000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L128, "62F17E087F8902000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L128, "62F17E087F8904000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L128, "62F17E087F8908000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L128, "62F17E087F4901"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L128, "62F17E087F4902"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L128, "62F17E087F4904"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F17E087F4908"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L256, "62F17E287F09"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L256, "62F17E287F8901000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L256, "62F17E287F8902000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L256, "62F17E287F8904000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L256, "62F17E287F8908000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L256, "62F17E287F8910000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L256, "62F17E287F4901"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L256, "62F17E287F4902"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L256, "62F17E287F4904"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L512, "62F17E487F09"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F17E487F8901000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L512, "62F17E487F8902000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L512, "62F17E487F8904000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L512, "62F17E487F8908000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L512, "62F17E487F8910000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L512, "62F17E487F8920000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L512, "62F17E487F4901"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F17E487F4902"))));

INSTANTIATE_TEST_CASE_P(X86RegMemDisplacement, XRegMemEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::ADDPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62F174085809"),
        std::make_tuple(OP::ADDPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L128, "62F1F508588901000000"),
        std::make_tuple(OP::DIVPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L128, "62F174085E8902000000"),
        std::make_tuple(OP::DIVPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L128, "62F1F5085E8904000000"),
        std::make_tuple(OP::MULPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L128, "62F17408598908000000"),
        std::make_tuple(OP::MULPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L128, "62F1F508594901"),
        std::make_tuple(OP::VSQRTPDRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L128, "62F1FD08514902"),
        std::make_tuple(OP::VSQRTPSRegMem, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L128, "62F17C08514904"),
        std::make_tuple(OP::MOVDQURegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F17E086F4908"),
        std::make_tuple(OP::PADDWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62F17508FD09"),
        std::make_tuple(OP::PADDDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L128, "62F17508FE8901000000"),
        std::make_tuple(OP::PADDQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L128, "62F1F508D48902000000"),
        std::make_tuple(OP::PSUBBRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L128, "62F17508F88904000000"),
        std::make_tuple(OP::PSUBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L128, "62F17508F98908000000"),
        std::make_tuple(OP::PSUBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L128, "62F17508FA4901"),
        std::make_tuple(OP::PSUBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L128, "62F1F508FB4902"),
        std::make_tuple(OP::PANDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L128, "62F17508DB4904"),
        std::make_tuple(OP::PORRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F17508EB4908"),
        std::make_tuple(OP::VMOVDQU8RegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L256, "62F17F286F09"),
        std::make_tuple(OP::VMOVDQU16RegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L256, "62F1FF286F8901000000"),
        std::make_tuple(OP::MOVDQUMemReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L256, "62F17E287F8902000000"),
        std::make_tuple(OP::VMOVDQU64RegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L256, "62F1FE286F8904000000"),
        std::make_tuple(OP::ORPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L256, "62F1F528568908000000"),
        std::make_tuple(OP::PMULLWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L256, "62F17528D58910000000"),
        std::make_tuple(OP::PMULLDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L256, "62F27528404901"),
        std::make_tuple(OP::PABSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L256, "62F27D281C4902"),
        std::make_tuple(OP::PABSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L256, "62F27D281D4904"),
        std::make_tuple(OP::PABSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L256, "62F27D281E09"),
        std::make_tuple(OP::PABSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L256, "62F2FD281F8901000000"),
        std::make_tuple(OP::PADDBRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L256, "62F17528FC8902000000"),
        std::make_tuple(OP::PXORRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L256, "62F17528EF8904000000"),
        std::make_tuple(OP::PMINSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L256, "62F27528388908000000"),
        std::make_tuple(OP::PMINSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L256, "62F17528EA8910000000"),
        std::make_tuple(OP::PMINSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L256, "62F27528394901"),
        std::make_tuple(OP::PMINSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L256, "62F2F528394902"),
        std::make_tuple(OP::MINPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L256, "62F174285D4904"),
        std::make_tuple(OP::MINPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L512, "62F1F5485D09"),
        std::make_tuple(OP::PMAXSBRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F275483C8901000000"),
        std::make_tuple(OP::PMAXSWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x2,
            OMR::X86::EVEX_L512, "62F17548EE8902000000"),
        std::make_tuple(OP::PMAXSDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x4,
            OMR::X86::EVEX_L512, "62F275483D8904000000"),
        std::make_tuple(OP::PMAXSQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x8,
            OMR::X86::EVEX_L512, "62F2F5483D8908000000"),
        std::make_tuple(OP::MAXPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L512, "62F174485F8910000000"),
        std::make_tuple(OP::MAXPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L512, "62F1F5485F8920000000"),
        std::make_tuple(OP::SUBPSRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L512, "62F174485C4901"),
        std::make_tuple(OP::SUBPDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F1F5485C4902"))));

INSTANTIATE_TEST_CASE_P(X86RegMemDisplacement2, XRegMemEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548128901000000"),
        std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548478901000000"),
        std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548478901000000"),
        std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548118901000000"),
        std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548468901000000"),
        std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548468901000000"),
        std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548108901000000"),
        std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548458901000000"),
        std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548458901000000"),
        std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548158901000000"),
        std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548158901000000"),
        std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548148901000000"),
        std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548148901000000"),
        std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F2F548A88901000000"),
        std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27548A88901000000"),
        std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508124908"),
        std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508474908"),
        std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508474908"),
        std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508114908"),
        std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508464908"),
        std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508464908"),
        std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508104908"),
        std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508454908"),
        std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508454908"),
        std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508154908"),
        std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508154908"),
        std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508144908"),
        std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508144908"),
        std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F2F508A84908"),
        std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27508A84908"),
        std::make_tuple(OP::VPSLLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548124902"),
        std::make_tuple(OP::VPSLLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548474902"),
        std::make_tuple(OP::VPSLLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548474902"),
        std::make_tuple(OP::VPSRAVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548114902"),
        std::make_tuple(OP::VPSRAVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548464902"),
        std::make_tuple(OP::VPSRAVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548464902"),
        std::make_tuple(OP::VPSRLVWRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548104902"),
        std::make_tuple(OP::VPSRLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548454902"),
        std::make_tuple(OP::VPSRLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548454902"),
        std::make_tuple(OP::VPROLVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548154902"),
        std::make_tuple(OP::VPROLVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548154902"),
        std::make_tuple(OP::VPRORVDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548144902"),
        std::make_tuple(OP::VPRORVQRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548144902"),
        std::make_tuple(OP::VFMADD213PDRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F2F548A84902"),
        std::make_tuple(OP::VFMADD213PSRegRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27548A84902"))));

INSTANTIATE_TEST_CASE_P(X86RegMemDisplacement3, XRegMemEncEncodingTest,
    ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<OP::Mnemonic, TR::RealRegister::RegNum,
                            TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction> >(
        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L128, "62F27D08214904"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L128, "62F27D08234904"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L128, "62F27D08304908"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L128, "62F27D08314920"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L128, "62F27D083209"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L128, "62F27D08338901000000"),

        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L256, "62F27D28214902"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L256, "62F27D28234902"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L256, "62F27D28304904"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L256, "62F27D28314910"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L256, "62F27D283209"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L256, "62F27D28338901000000"),

        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28214920"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28234910"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28304910"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28314920"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28324940"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x100,
            OMR::X86::EVEX_L256, "62F27D28334910"),

        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28218901010000"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28238901010000"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28308901010000"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28318901010000"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28328901010000"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x101,
            OMR::X86::EVEX_L256, "62F27D28338901010000"),

        std::make_tuple(OP::PMOVSXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x10,
            OMR::X86::EVEX_L512, "62F27D48214901"),
        std::make_tuple(OP::PMOVSXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x20,
            OMR::X86::EVEX_L512, "62F27D48234901"),
        std::make_tuple(OP::PMOVZXBWRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x40,
            OMR::X86::EVEX_L512, "62F27D48304902"),
        std::make_tuple(OP::PMOVZXBDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x80,
            OMR::X86::EVEX_L512, "62F27D48314908"),
        std::make_tuple(OP::PMOVZXBQRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x0,
            OMR::X86::EVEX_L512, "62F27D483209"),
        std::make_tuple(OP::PMOVZXWDRegReg, TR::RealRegister::xmm1, TR::RealRegister::ecx, 0x1,
            OMR::X86::EVEX_L512, "62F27D48338901000000"))));
