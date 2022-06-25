/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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

#include <gtest/gtest.h>
#include "../CodeGenTest.hpp"
#include "codegen/OMRX86Instruction.hpp"


class XDirectEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TRTest::BinaryInstruction>> {};

TEST_P(XDirectEncodingTest, encode) {
    auto instr = generateInstruction(std::get<0>(GetParam()), fakeNode, cg());

    ASSERT_EQ(std::get<1>(GetParam()), encodeInstruction(instr));
}

class XLabelEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, size_t, TRTest::BinaryInstruction>> {};

TEST_P(XLabelEncodingTest, encode) {
    auto label = generateLabelSymbol(cg());
    label->setCodeLocation(reinterpret_cast<uint8_t*>(getAlignedBuf()) + std::get<1>(GetParam()));

    auto instr = generateLabelInstruction(
        std::get<0>(GetParam()),
        fakeNode,
        label,
        cg()
    );

    ASSERT_EQ(
        std::get<2>(GetParam()),
        encodeInstruction(instr)
   );
}

class XRegRegEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>> {};

TEST_P(XRegRegEncodingTest, encode) {
    auto regA = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto regB = cg()->machine()->getRealRegister(std::get<2>(GetParam()));

    auto instr = generateRegRegInstruction(std::get<0>(GetParam()), fakeNode, regA, regB, cg());

    ASSERT_EQ(std::get<3>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_SUITE_P(Special, XDirectEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::UD2,  "0f0b"),
    std::make_tuple(TR::InstOpCode::INT3, "cc"),
    std::make_tuple(TR::InstOpCode::RET,  "c3")
));

INSTANTIATE_TEST_SUITE_P(Special, XLabelEncodingTest, ::testing::Values(
    std::make_tuple(TR::InstOpCode::JMP4,  0x4, "eb02"),
    std::make_tuple(TR::InstOpCode::JMP4, -0x4, "ebfa"),
    std::make_tuple(TR::InstOpCode::JGE4,  0x4, "7d02"),
    std::make_tuple(TR::InstOpCode::JGE4, -0x4, "7dfa"),
    std::make_tuple(TR::InstOpCode::JGE4,  0x0, "7dfe")
));

class XRegRegEncEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>> {};

TEST_P(XRegRegEncEncodingTest, encode) {
    auto regA = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto regB = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto enc = std::get<3>(GetParam());

    auto instr = generateRegRegInstruction(std::get<0>(GetParam()), fakeNode, regA, regB, cg(), enc);

    ASSERT_EQ(std::get<4>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_SUITE_P(AVXSimdTest, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::Legacy,    "660ffcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm2, TR::RealRegister::xmm1, OMR::X86::Legacy,    "660ffcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm1, OMR::X86::Legacy,    "660ffcc9"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::VEX_L128,  "c5f1fcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm2, TR::RealRegister::xmm1, OMR::X86::VEX_L128,  "c5e9fcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm1, OMR::X86::VEX_L128,  "c5f1fcc9"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::VEX_L256,  "c5f5fcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm2, TR::RealRegister::ymm1, OMR::X86::VEX_L256,  "c5edfcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm1, TR::RealRegister::ymm1, OMR::X86::VEX_L256,  "c5f5fcc9"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm2, OMR::X86::EVEX_L128, "62f17508fcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm2, TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f16d08fcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm1, TR::RealRegister::xmm1, OMR::X86::EVEX_L128, "62f17508fcc9"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm1, TR::RealRegister::ymm2, OMR::X86::EVEX_L256, "62f17528fcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm2, TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f16d28fcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm1, TR::RealRegister::ymm1, OMR::X86::EVEX_L256, "62f17528fcc9"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::zmm1, TR::RealRegister::zmm2, OMR::X86::EVEX_L512, "62f17548fcca"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::zmm2, TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f16d48fcd1"),
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::zmm1, TR::RealRegister::zmm1, OMR::X86::EVEX_L512, "62f17548fcc9"),
    std::make_tuple(TR::InstOpCode::VSQRTPDRegReg,  TR::RealRegister::xmm0, TR::RealRegister::xmm1, OMR::X86::VEX_L128,  "c5f951c1"),
    std::make_tuple(TR::InstOpCode::VSQRTPDRegReg,  TR::RealRegister::xmm9, TR::RealRegister::xmm4, OMR::X86::VEX_L128,  "c57951cc")
)));

INSTANTIATE_TEST_SUITE_P(AVXSimdRegRegVex128Test, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm0,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c179fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::xmm1,  TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4c171fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::xmm2,  TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c5e9fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::xmm3,  TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c4e1e1d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::xmm4,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c15858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::xmm5,  TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4c1d158e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::xmm6,  TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c5c9f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::xmm7,  TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c5c1f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::xmm8,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44139fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::xmm9,  TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c441b1fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::xmm10, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c5285cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::xmm11, TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c461a15cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::xmm12, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44119d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::xmm13, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4421140e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c50859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c4618159f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c441005eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c441895ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::xmm13, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c511dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::xmm12, TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c519ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::xmm11, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44121efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXSimdRegRegVex256Test, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm0,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c17dfcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::ymm1,  TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4c175fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::ymm2,  TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c5edfed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::ymm3,  TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c4e1e5d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::ymm4,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c15c58e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::ymm5,  TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4c1d558e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::ymm6,  TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c5cdf8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::ymm7,  TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c5c5f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::ymm8,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4413dfac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::ymm9,  TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c441b5fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::ymm10, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c52c5cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::ymm11, TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c461a55cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::ymm12, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4411dd5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::ymm13, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4421540e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c50c59f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c4618559f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c441045eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4418d5ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::ymm13, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c515dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::ymm12, TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c51debe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::ymm11, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c44125efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXSimdRegRegEVEX128Test, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm0,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d17d08fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::xmm1,  TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62d17508fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::xmm2,  TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62f16d08fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::xmm3,  TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62f1e508d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::xmm4,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d15c0858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::xmm5,  TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62d1d50858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::xmm6,  TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62f14d08f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::xmm7,  TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62f14508f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::xmm8,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62513d08fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::xmm9,  TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "6251b508fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::xmm10, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62712c085cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::xmm11, TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "6271a5085cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::xmm12, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62511d08d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::xmm13, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "6252150840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62710c0859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "6271850859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "625104085eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62518d085ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::xmm13, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62711508dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::xmm12, TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62711d08ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::xmm11, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62512508efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXSimdRegRegEVEX256Test, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm0,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d17d28fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::ymm1,  TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62d17528fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::ymm2,  TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62f16d28fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::ymm3,  TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62f1e528d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::ymm4,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d15c2858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::ymm5,  TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62d1d52858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::ymm6,  TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62f14d28f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::ymm7,  TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62f14528f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::ymm8,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62513d28fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::ymm9,  TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "6251b528fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::ymm10, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62712c285cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::ymm11, TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "6271a5285cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::ymm12, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62511d28d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::ymm13, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "6252152840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62710c2859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "6271852859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "625104285eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62518d285ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::ymm13, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62711528dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::ymm12, TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62711d28ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::ymm11, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62512528efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXSimdRegRegEVEX512Test, XRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::zmm0,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d17d48fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::zmm1,  TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62d17548fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::zmm2,  TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62f16d48fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::zmm3,  TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62f1e548d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::zmm4,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d15c4858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::zmm5,  TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62d1d54858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::zmm6,  TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62f14d48f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::zmm7,  TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62f14548f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::zmm8,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62513d48fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::zmm9,  TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "6251b548fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::zmm10, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62712c485cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::zmm11, TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "6271a5485cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::zmm12, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62511d48d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::zmm13, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "6252154840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::zmm14, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62710c4859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::zmm15, TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "6271854859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::zmm15, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "625104485eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::zmm14, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62518d485ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::zmm13, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62711548dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::zmm12, TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62711d48ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::zmm11, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62512548efdf")
)));

class XRegRegRegEncEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>> {};

TEST_P(XRegRegRegEncEncodingTest, encode) {
    auto regA = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto regB = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto regC = cg()->machine()->getRealRegister(std::get<3>(GetParam()));
    auto enc = std::get<4>(GetParam());

    auto instr = generateRegRegRegInstruction(std::get<0>(GetParam()), fakeNode, regA, regB, regC, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_SUITE_P(AVXRegRegRegSimdVEX128Test, XRegRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm0,  TR::RealRegister::xmm15, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c101fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::xmm1,  TR::RealRegister::xmm14, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4c109fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::xmm2,  TR::RealRegister::xmm13, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c591fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::xmm3,  TR::RealRegister::xmm12, TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c4e199d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::xmm4,  TR::RealRegister::xmm11, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c4c12058e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::xmm5,  TR::RealRegister::xmm10, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4c1a958e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::xmm6,  TR::RealRegister::xmm9,  TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c5b1f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::xmm7,  TR::RealRegister::xmm8,  TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c5b9f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::xmm8,  TR::RealRegister::xmm7,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44141fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::xmm9,  TR::RealRegister::xmm15, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c44181fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::xmm10, TR::RealRegister::xmm14, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c5085cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::xmm11, TR::RealRegister::xmm13, TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c461915cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::xmm12, TR::RealRegister::xmm12, TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44119d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::xmm13, TR::RealRegister::xmm11, TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c4422140e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm10, TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c52859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm9,  TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c461b159f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm7,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c441405eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm6,  TR::RealRegister::xmm8,  OMR::X86::VEX_L128, "c441c95ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::xmm13, TR::RealRegister::xmm5,  TR::RealRegister::xmm7,  OMR::X86::VEX_L128, "c551dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::xmm12, TR::RealRegister::xmm4,  TR::RealRegister::xmm0,  OMR::X86::VEX_L128, "c559ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::xmm11, TR::RealRegister::xmm3,  TR::RealRegister::xmm15, OMR::X86::VEX_L128, "c44161efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXRegRegRegSimdVEX256Test, XRegRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm0,  TR::RealRegister::ymm15, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c105fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::ymm1,  TR::RealRegister::ymm14, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4c10dfdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::ymm2,  TR::RealRegister::ymm13, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c595fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::ymm3,  TR::RealRegister::ymm12, TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c4e19dd4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::ymm4,  TR::RealRegister::ymm11, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4c12458e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::ymm5,  TR::RealRegister::ymm10, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4c1ad58e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::ymm6,  TR::RealRegister::ymm9,  TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c5b5f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::ymm7,  TR::RealRegister::ymm8,  TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c5bdf9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::ymm8,  TR::RealRegister::ymm7,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c44145fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::ymm9,  TR::RealRegister::ymm15, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c44185fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::ymm10, TR::RealRegister::ymm14, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c50c5cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::ymm11, TR::RealRegister::ymm13, TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c461955cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::ymm12, TR::RealRegister::ymm12, TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c4411dd5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::ymm13, TR::RealRegister::ymm11, TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c4422540e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm10, TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c52c59f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm9,  TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c461b559f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm7,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c441445eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm6,  TR::RealRegister::ymm8,  OMR::X86::VEX_L256, "c441cd5ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::ymm13, TR::RealRegister::ymm5,  TR::RealRegister::ymm7,  OMR::X86::VEX_L256, "c555dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::ymm12, TR::RealRegister::ymm4,  TR::RealRegister::ymm0,  OMR::X86::VEX_L256, "c55debe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::ymm11, TR::RealRegister::ymm3,  TR::RealRegister::ymm15, OMR::X86::VEX_L256, "c44165efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXRegRegRegSimdEVEX128Test, XRegRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::xmm0,  TR::RealRegister::xmm15, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d10508fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::xmm1,  TR::RealRegister::xmm14, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62d10d08fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::xmm2,  TR::RealRegister::xmm13, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62f11508fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::xmm3,  TR::RealRegister::xmm12, TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62f19d08d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::xmm4,  TR::RealRegister::xmm11, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62d1240858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::xmm5,  TR::RealRegister::xmm10, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62d1ad0858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::xmm6,  TR::RealRegister::xmm9,  TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62f13508f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::xmm7,  TR::RealRegister::xmm8,  TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62f13d08f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::xmm8,  TR::RealRegister::xmm7,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62514508fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::xmm9,  TR::RealRegister::xmm15, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "62518508fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::xmm10, TR::RealRegister::xmm14, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62710c085cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::xmm11, TR::RealRegister::xmm13, TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "627195085cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::xmm12, TR::RealRegister::xmm12, TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62511d08d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::xmm13, TR::RealRegister::xmm11, TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "6252250840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm10, TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62712c0859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm9,  TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "6271b50859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::xmm15, TR::RealRegister::xmm7,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "625144085eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::xmm14, TR::RealRegister::xmm6,  TR::RealRegister::xmm8,  OMR::X86::EVEX_L128, "6251cd085ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::xmm13, TR::RealRegister::xmm5,  TR::RealRegister::xmm7,  OMR::X86::EVEX_L128, "62715508dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::xmm12, TR::RealRegister::xmm4,  TR::RealRegister::xmm0,  OMR::X86::EVEX_L128, "62715d08ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::xmm11, TR::RealRegister::xmm3,  TR::RealRegister::xmm15, OMR::X86::EVEX_L128, "62516508efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXRegRegRegSimdEVEX256Test, XRegRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::ymm0,  TR::RealRegister::ymm15, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d10528fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::ymm1,  TR::RealRegister::ymm14, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62d10d28fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::ymm2,  TR::RealRegister::ymm13, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62f11528fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::ymm3,  TR::RealRegister::ymm12, TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62f19d28d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::ymm4,  TR::RealRegister::ymm11, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62d1242858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::ymm5,  TR::RealRegister::ymm10, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62d1ad2858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::ymm6,  TR::RealRegister::ymm9,  TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62f13528f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::ymm7,  TR::RealRegister::ymm8,  TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62f13d28f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::ymm8,  TR::RealRegister::ymm7,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62514528fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::ymm9,  TR::RealRegister::ymm15, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "62518528fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::ymm10, TR::RealRegister::ymm14, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62710c285cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::ymm11, TR::RealRegister::ymm13, TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "627195285cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::ymm12, TR::RealRegister::ymm12, TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62511d28d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::ymm13, TR::RealRegister::ymm11, TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "6252252840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm10, TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62712c2859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm9,  TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "6271b52859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::ymm15, TR::RealRegister::ymm7,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "625144285eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::ymm14, TR::RealRegister::ymm6,  TR::RealRegister::ymm8,  OMR::X86::EVEX_L256, "6251cd285ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::ymm13, TR::RealRegister::ymm5,  TR::RealRegister::ymm7,  OMR::X86::EVEX_L256, "62715528dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::ymm12, TR::RealRegister::ymm4,  TR::RealRegister::ymm0,  OMR::X86::EVEX_L256, "62715d28ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::ymm11, TR::RealRegister::ymm3,  TR::RealRegister::ymm15, OMR::X86::EVEX_L256, "62516528efdf")
)));

INSTANTIATE_TEST_SUITE_P(AVXRegRegRegSimdEVEX512Test, XRegRegRegEncEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TR::RealRegister::RegNum, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::PADDBRegReg,    TR::RealRegister::zmm0,  TR::RealRegister::zmm15, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d10548fcc7"),
    std::make_tuple(TR::InstOpCode::PADDWRegReg,    TR::RealRegister::zmm1,  TR::RealRegister::zmm14, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62d10d48fdc8"),
    std::make_tuple(TR::InstOpCode::PADDDRegReg,    TR::RealRegister::zmm2,  TR::RealRegister::zmm13, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62f11548fed7"),
    std::make_tuple(TR::InstOpCode::PADDQRegReg,    TR::RealRegister::zmm3,  TR::RealRegister::zmm12, TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62f19d48d4d8"),
    std::make_tuple(TR::InstOpCode::ADDPSRegReg,    TR::RealRegister::zmm4,  TR::RealRegister::zmm11, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62d1244858e7"),
    std::make_tuple(TR::InstOpCode::ADDPDRegReg,    TR::RealRegister::zmm5,  TR::RealRegister::zmm10, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62d1ad4858e8"),
    std::make_tuple(TR::InstOpCode::PSUBBRegReg,    TR::RealRegister::zmm6,  TR::RealRegister::zmm9,  TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62f13548f8f7"),
    std::make_tuple(TR::InstOpCode::PSUBWRegReg,    TR::RealRegister::zmm7,  TR::RealRegister::zmm8,  TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62f13d48f9f8"),
    std::make_tuple(TR::InstOpCode::PSUBDRegReg,    TR::RealRegister::zmm8,  TR::RealRegister::zmm7,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62514548fac7"),
    std::make_tuple(TR::InstOpCode::PSUBQRegReg,    TR::RealRegister::zmm9,  TR::RealRegister::zmm15, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "62518548fbc8"),
    std::make_tuple(TR::InstOpCode::SUBPSRegReg,    TR::RealRegister::zmm10, TR::RealRegister::zmm14, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62710c485cd7"),
    std::make_tuple(TR::InstOpCode::SUBPDRegReg,    TR::RealRegister::zmm11, TR::RealRegister::zmm13, TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "627195485cd8"),
    std::make_tuple(TR::InstOpCode::PMULLWRegReg,   TR::RealRegister::zmm12, TR::RealRegister::zmm12, TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62511d48d5e7"),
    std::make_tuple(TR::InstOpCode::PMULLDRegReg,   TR::RealRegister::zmm13, TR::RealRegister::zmm11, TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "6252254840e8"),
    std::make_tuple(TR::InstOpCode::MULPSRegReg,    TR::RealRegister::zmm14, TR::RealRegister::zmm10, TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62712c4859f7"),
    std::make_tuple(TR::InstOpCode::MULPDRegReg,    TR::RealRegister::zmm15, TR::RealRegister::zmm9,  TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "6271b54859f8"),
    std::make_tuple(TR::InstOpCode::DIVPSRegReg,    TR::RealRegister::zmm15, TR::RealRegister::zmm7,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "625144485eff"),
    std::make_tuple(TR::InstOpCode::DIVPDRegReg,    TR::RealRegister::zmm14, TR::RealRegister::zmm6,  TR::RealRegister::zmm8,  OMR::X86::EVEX_L512, "6251cd485ef0"),
    std::make_tuple(TR::InstOpCode::PANDRegReg,     TR::RealRegister::zmm13, TR::RealRegister::zmm5,  TR::RealRegister::zmm7,  OMR::X86::EVEX_L512, "62715548dbef"),
    std::make_tuple(TR::InstOpCode::PORRegReg,      TR::RealRegister::zmm12, TR::RealRegister::zmm4,  TR::RealRegister::zmm0,  OMR::X86::EVEX_L512, "62715d48ebe0"),
    std::make_tuple(TR::InstOpCode::PXORRegReg,     TR::RealRegister::zmm11, TR::RealRegister::zmm3,  TR::RealRegister::zmm15, OMR::X86::EVEX_L512, "62516548efdf")
)));

INSTANTIATE_TEST_SUITE_P(Branch, XRegRegEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::XOR4RegReg,    TR::RealRegister::eax, TR::RealRegister::eax,    "33c0"),
    std::make_tuple(TR::InstOpCode::XOR4RegReg,    TR::RealRegister::ecx, TR::RealRegister::ebp,    "33cd"),
    std::make_tuple(TR::InstOpCode::ADD4RegReg,    TR::RealRegister::eax, TR::RealRegister::eax,    "03c0"),
    std::make_tuple(TR::InstOpCode::SUB4RegReg,    TR::RealRegister::eax, TR::RealRegister::eax,    "2bc0"),
    std::make_tuple(TR::InstOpCode::IMUL4RegReg,   TR::RealRegister::eax, TR::RealRegister::eax,    "0fafc0"),

    std::make_tuple(TR::InstOpCode::IMUL8RegReg,     TR::RealRegister::eax,  TR::RealRegister::ecx, "480fafc1"),
    std::make_tuple(TR::InstOpCode::IMUL8RegReg,     TR::RealRegister::ecx,  TR::RealRegister::eax, "480fafc8"),

    std::make_tuple(TR::InstOpCode::CMOVG8RegReg,    TR::RealRegister::eax,  TR::RealRegister::ecx, "480f4fc1"),
    std::make_tuple(TR::InstOpCode::CMOVG8RegReg,    TR::RealRegister::ecx,  TR::RealRegister::eax, "480f4fc8"),
    std::make_tuple(TR::InstOpCode::CMOVE8RegReg,    TR::RealRegister::eax,  TR::RealRegister::ecx, "480f44c1"),
    std::make_tuple(TR::InstOpCode::CMOVE8RegReg,    TR::RealRegister::ecx,  TR::RealRegister::eax, "480f44c8"),

    std::make_tuple(TR::InstOpCode::XCHG8RegReg,     TR::RealRegister::eax,  TR::RealRegister::ecx, "4887c8"),
    std::make_tuple(TR::InstOpCode::XCHG8RegReg,     TR::RealRegister::ecx,  TR::RealRegister::eax, "4887c1"),

    std::make_tuple(TR::InstOpCode::MOV8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "488bc1"),
    std::make_tuple(TR::InstOpCode::MOV8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "488bc8"),

    std::make_tuple(TR::InstOpCode::CMOVB8RegReg,    TR::RealRegister::eax,  TR::RealRegister::ecx, "480f42c1"),
    std::make_tuple(TR::InstOpCode::CMOVB8RegReg,    TR::RealRegister::ecx,  TR::RealRegister::eax, "480f42c8"),

    std::make_tuple(TR::InstOpCode::POPCNT8RegReg,   TR::RealRegister::eax,  TR::RealRegister::ecx, "f3480fb8c1"),
    std::make_tuple(TR::InstOpCode::POPCNT8RegReg,   TR::RealRegister::ecx,  TR::RealRegister::eax, "f3480fb8c8"),

    std::make_tuple(TR::InstOpCode::CMOVL8RegReg,    TR::RealRegister::eax,  TR::RealRegister::ecx, "480f4cc1"),
    std::make_tuple(TR::InstOpCode::CMOVL8RegReg,    TR::RealRegister::ecx,  TR::RealRegister::eax, "480f4cc8"),

    std::make_tuple(TR::InstOpCode::ADD8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "4803c1"),
    std::make_tuple(TR::InstOpCode::ADD8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "4803c8"),
    std::make_tuple(TR::InstOpCode::ADC8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "4813c1"),
    std::make_tuple(TR::InstOpCode::ADC8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "4813c8"),

    std::make_tuple(TR::InstOpCode::TEST8RegReg,     TR::RealRegister::eax,  TR::RealRegister::ecx, "4885c1"),
    std::make_tuple(TR::InstOpCode::TEST8RegReg,     TR::RealRegister::ecx,  TR::RealRegister::eax, "4885c8"),

    std::make_tuple(TR::InstOpCode::CMOVLE8RegReg,   TR::RealRegister::eax,  TR::RealRegister::ecx, "480f4ec1"),
    std::make_tuple(TR::InstOpCode::CMOVLE8RegReg,   TR::RealRegister::ecx,  TR::RealRegister::eax, "480f4ec8"),

    std::make_tuple(TR::InstOpCode::BSF8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "480fbcc1"),
    std::make_tuple(TR::InstOpCode::BSF8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "480fbcc8"),

    std::make_tuple(TR::InstOpCode::SHLD4RegRegCL,   TR::RealRegister::eax,  TR::RealRegister::ecx, "0fa5c8"),
    std::make_tuple(TR::InstOpCode::SHLD4RegRegCL,   TR::RealRegister::ecx,  TR::RealRegister::eax, "0fa5c1"),

    std::make_tuple(TR::InstOpCode::BSR8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "480fbdc1"),
    std::make_tuple(TR::InstOpCode::BSR8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "480fbdc8"),

    std::make_tuple(TR::InstOpCode::SUB8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "482bc1"),
    std::make_tuple(TR::InstOpCode::SUB8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "482bc8"),

    std::make_tuple(TR::InstOpCode::CMP8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "483bc1"),
    std::make_tuple(TR::InstOpCode::CMP8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "483bc8"),

    std::make_tuple(TR::InstOpCode::CMOVGE8RegReg,   TR::RealRegister::eax,  TR::RealRegister::ecx, "480f4dc1"),
    std::make_tuple(TR::InstOpCode::CMOVGE8RegReg,   TR::RealRegister::ecx,  TR::RealRegister::eax, "480f4dc8"),

    std::make_tuple(TR::InstOpCode::BT8RegReg,       TR::RealRegister::eax,  TR::RealRegister::ecx, "480fa3c8"),
    std::make_tuple(TR::InstOpCode::BT8RegReg,       TR::RealRegister::ecx,  TR::RealRegister::eax, "480fa3c1"),

    std::make_tuple(TR::InstOpCode::AND8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "4823c1"),
    std::make_tuple(TR::InstOpCode::AND8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "4823c8"),

    std::make_tuple(TR::InstOpCode::XOR8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "4833c1"),
    std::make_tuple(TR::InstOpCode::XOR8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "4833c8"),

    std::make_tuple(TR::InstOpCode::BTS4RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "0fabc8"),
    std::make_tuple(TR::InstOpCode::BTS4RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "0fabc1"),

    std::make_tuple(TR::InstOpCode::OR8RegReg,       TR::RealRegister::eax,  TR::RealRegister::ecx, "480bc1"),
    std::make_tuple(TR::InstOpCode::OR8RegReg,       TR::RealRegister::ecx,  TR::RealRegister::eax, "480bc8"),

    std::make_tuple(TR::InstOpCode::CMOVS8RegReg,    TR::RealRegister::eax,  TR::RealRegister::ecx, "480f48c1"),
    std::make_tuple(TR::InstOpCode::CMOVS8RegReg,    TR::RealRegister::ecx,  TR::RealRegister::eax, "480f48c8"),

    std::make_tuple(TR::InstOpCode::CMOVNE8RegReg,   TR::RealRegister::eax,  TR::RealRegister::ecx, "480f45c1"),
    std::make_tuple(TR::InstOpCode::CMOVNE8RegReg,   TR::RealRegister::ecx,  TR::RealRegister::eax, "480f45c8"),

    std::make_tuple(TR::InstOpCode::SBB8RegReg,      TR::RealRegister::eax,  TR::RealRegister::ecx, "481bc1"),
    std::make_tuple(TR::InstOpCode::SBB8RegReg,      TR::RealRegister::ecx,  TR::RealRegister::eax, "481bc8"),

    std::make_tuple(TR::InstOpCode::SHRD4RegRegCL,   TR::RealRegister::eax,  TR::RealRegister::ecx, "0fadc8"),
    std::make_tuple(TR::InstOpCode::SHRD4RegRegCL,   TR::RealRegister::ecx,  TR::RealRegister::eax, "0fadc1"),

    std::make_tuple(TR::InstOpCode::SQRTPDRegReg,   TR::RealRegister::xmm0,  TR::RealRegister::xmm1, "660f51c1"),
    std::make_tuple(TR::InstOpCode::SQRTPDRegReg,   TR::RealRegister::xmm9,  TR::RealRegister::xmm4, "66440f51cc")
)));

class XRegMemEncodingTest : public TRTest::BinaryEncoderTest<>, public ::testing::WithParamInterface<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction>> {};

TEST_P(XRegMemEncodingTest, encode) {
    auto target = cg()->machine()->getRealRegister(std::get<1>(GetParam()));
    auto base = cg()->machine()->getRealRegister(std::get<2>(GetParam()));
    auto disp = std::get<3>(GetParam());
    auto enc = std::get<4>(GetParam());

    auto mr = generateX86MemoryReference(base, disp, cg());
    auto instr = generateRegMemInstruction(std::get<0>(GetParam()), fakeNode, target, mr, cg(), enc);

    ASSERT_EQ(std::get<5>(GetParam()), encodeInstruction(instr));
}

INSTANTIATE_TEST_SUITE_P(X86RegMemEnc, XRegMemEncodingTest, ::testing::ValuesIn(*TRTest::MakeVector<std::tuple<TR::InstOpCode::Mnemonic, TR::RealRegister::RegNum, TR::RealRegister::RegNum, int32_t, OMR::X86::Encoding, TRTest::BinaryInstruction>>(
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm1,  TR::RealRegister::ecx, 0x0, OMR::X86::Legacy,    "f30f6f09"),
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm1,  TR::RealRegister::ecx, 0x0, OMR::X86::VEX_L128,  "c5fa6f09"),
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm1,  TR::RealRegister::ecx, 0x0, OMR::X86::VEX_L256,  "c5fe6f09"),
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm1,  TR::RealRegister::ecx, 0x0, OMR::X86::EVEX_L128, "62f17e086f09"),
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L256, "62717e286f10"),
    std::make_tuple(TR::InstOpCode::MOVDQURegMem,  TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0, OMR::X86::EVEX_L512, "62717e486f10"),
    std::make_tuple(TR::InstOpCode::VSQRTPDRegMem, TR::RealRegister::xmm1,  TR::RealRegister::ecx, 0x8, OMR::X86::VEX_L128,  "c5f9514908"),
    std::make_tuple(TR::InstOpCode::VSQRTPDRegMem, TR::RealRegister::xmm10, TR::RealRegister::eax, 0x0, OMR::X86::VEX_L128,  "c5795110")
)));