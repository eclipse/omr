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

#ifndef OMR_X86_I386_REAL_REGISTER_INCL
#define OMR_X86_I386_REAL_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REAL_REGISTER_CONNECTOR
#define OMR_REAL_REGISTER_CONNECTOR

namespace OMR {
namespace X86 { namespace I386 {
class RealRegister;
}} // namespace X86::I386

typedef OMR::X86::I386::RealRegister RealRegisterConnector;
} // namespace OMR
#else
#error OMR::X86::I386::RealRegister expected to be a primary connector, but a OMR connector is already defined
#endif

#include "x/codegen/OMRRealRegister.hpp"

#include <stdint.h>
#include "omrformatconsts.h"
#include "codegen/RegisterConstants.hpp"

namespace TR {
class CodeGenerator;
}

namespace OMR { namespace X86 { namespace I386 {

class OMR_EXTENSIBLE RealRegister : public OMR::X86::RealRegister {
protected:
    RealRegister(TR::CodeGenerator *cg)
        : OMR::X86::RealRegister(cg)
    {}

    RealRegister(TR_RegisterKinds rk, uint16_t w, RegState s, RegNum ri, RegMask m, TR::CodeGenerator *cg)
        : OMR::X86::RealRegister(rk, w, s, ri, m, cg)
    {}

public:
    static RegNum xmmIndex(uint8_t r)
    {
        TR_ASSERT_FATAL(r >= 0 && r <= OMR::RealRegister::NumXMMRs, "xmm index %" OMR_PRIu8 " out of range", r);
        return static_cast<RegNum>(OMR::RealRegister::xmm0 + r);
    }

    typedef uint32_t RegMaskUInt;

    static RegMask gprMask(RegNum idx)
    {
        if (idx == OMR::RealRegister::NoReg) {
            return OMR::RealRegister::noRegMask;
        }

        static_assert((static_cast<RegMaskUInt>(OMR::RealRegister::LastGPR)
                          - static_cast<RegMaskUInt>(OMR::RealRegister::FirstGPR) + 1)
                == OMR::RealRegister::NumGPRs,
            "Expected contiguous range for GPR enum values");

        return static_cast<RegMask>(
            1 << (static_cast<RegMaskUInt>(idx) - static_cast<RegMaskUInt>(OMR::RealRegister::FirstGPR)));
    }

    static RegMask xmmrMask(RegNum idx)
    {
        if (idx == OMR::RealRegister::NoReg) {
            return OMR::RealRegister::noRegMask;
        }

        static_assert((static_cast<RegMaskUInt>(OMR::RealRegister::LastXMMR)
                          - static_cast<RegMaskUInt>(OMR::RealRegister::FirstXMMR) + 1)
                == OMR::RealRegister::NumXMMRs,
            "Expected contiguous range for XMMR enum values");

        return static_cast<RegMask>(
            1 << (static_cast<RegMaskUInt>(idx) - static_cast<RegMaskUInt>(OMR::RealRegister::FirstXMMR)));
    }

    static RegMask vmrMask(RegNum idx)
    {
        if (idx == OMR::RealRegister::NoReg) {
            return OMR::RealRegister::noRegMask;
        }

        static_assert((static_cast<RegMaskUInt>(OMR::RealRegister::LastVMR)
                          - static_cast<RegMaskUInt>(OMR::RealRegister::FirstVMR) + 1)
                == OMR::RealRegister::NumVMRs,
            "Expected contiguous range for VMR enum values");

        return static_cast<RegMask>(
            1 << (static_cast<RegMaskUInt>(idx) - static_cast<RegMaskUInt>(OMR::RealRegister::FirstVMR)));
    }

    void setRegisterNumber() { TR_ASSERT(0, "X86 RealRegister doesn't have setRegisterNumber() implementation"); }

    void setRegisterFieldInOpcode(uint8_t *opcodeByte)
    {
        *opcodeByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // reg field is in bits 0-2 of opcode
    }

    void setMaskRegisterInEvex(uint8_t *evex, bool zero = false)
    {
        uint8_t regNum = getRegisterNumber() - OMR::RealRegister::k0;
        *evex = (*evex & 0xf8) | (0x7 & regNum) | (zero ? 0x80 : 0);
    }

    void setSourceRegisterFieldInEVEX(uint8_t *evexP0)
    {
        uint8_t regNum = ((_fullRegisterBinaryEncodings[_registerNumber].needsRexPlusRXB << 3)
            | _fullRegisterBinaryEncodings[_registerNumber].id);
        uint8_t bits = 0;
        *evexP0 &= 0x9F;

        if (regNum & 0x10) {
            bits |= 0x4;
        }

        if (regNum & 0x8) {
            bits |= 0x2;
        }

        *evexP0 |= (~bits & 0x6) << 4;
    }

    void setSource2ndRegisterFieldInEVEX(uint8_t *evexP1)
    {
        uint8_t regNum = ((_fullRegisterBinaryEncodings[_registerNumber].needsRexPlusRXB << 3)
            | _fullRegisterBinaryEncodings[_registerNumber].id);

        *evexP1 &= 0x87; // zero out vvvv bits
        *evexP1 |= (~(regNum << 3)) & 0x78;
        uint8_t *evexP2 = evexP1 + 1;
        *evexP2 &= 0xf7;

        if (!(regNum & 0x10)) {
            *evexP2 |= 0x8;
        }
    }

    void setTargetRegisterFieldInEVEX(uint8_t *evexP0)
    {
        uint8_t regNum = ((_fullRegisterBinaryEncodings[_registerNumber].needsRexPlusRXB << 3)
            | _fullRegisterBinaryEncodings[_registerNumber].id);
        uint8_t bits = 0;
        *evexP0 &= 0x6F;

        if (regNum & 0x10) {
            bits |= 0x1;
        }

        if (regNum & 0x8) {
            bits |= 0x8;
        }

        *evexP0 |= (~bits & 0x9) << 4;
    }

    /** \brief
     *    Fill vvvv field in a VEX prefix
     *
     *  \param opcodeByte
     *    The address of VEX prefix byte containing vvvv field
     */
    void setRegisterFieldInVEX(uint8_t *opcodeByte)
    {
        *opcodeByte ^= ((_fullRegisterBinaryEncodings[_registerNumber].needsRexPlusRXB << 3)
                           | _fullRegisterBinaryEncodings[_registerNumber].id)
            << 3; // vvvv is in bits 3-6 of last byte of VEX
    }

    void setRegisterFieldInModRM(uint8_t *modRMByte)
    {
        *modRMByte |= _fullRegisterBinaryEncodings[_registerNumber].id << 3; // reg field is in bits 3-5 of ModRM byte
    }

    void setRMRegisterFieldInModRM(uint8_t *modRMByte)
    {
        *modRMByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // RM field is in bits 0-2 of ModRM byte
    }

    void setBaseRegisterFieldInSIB(uint8_t *SIBByte)
    {
        *SIBByte |= _fullRegisterBinaryEncodings[_registerNumber].id; // base register field is in bits 0-2 of SIB byte
    }

    void setIndexRegisterFieldInSIB(uint8_t *SIBByte)
    {
        *SIBByte |= _fullRegisterBinaryEncodings[_registerNumber].id
            << 3; // index register field is in bits 3-5 SIB byte
    }

    uint8_t rexBits(uint8_t rxbBits, bool isByteOperand) { return 0; }

    // (AMD64: see x86-64 Architecture Programmer's Manual, Volume 3, section 1.7.2.
    bool needsDisp() { return _fullRegisterBinaryEncodings[_registerNumber].needsDisp; }

    bool needsSIB() { return _fullRegisterBinaryEncodings[_registerNumber].needsSIB; }

private:
    static const struct RegisterBinaryEncoding _fullRegisterBinaryEncodings[NumRegisters];
};

}}} // namespace OMR::X86::I386

#endif
