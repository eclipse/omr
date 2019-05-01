/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"

TR::RealRegister *
OMR::Power::RealRegister::regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk, TR::CodeGenerator *cg)
   {
   RegNum rr;

   int32_t bitPos = TR::RealRegister::getBitPosInMask(mask);

   if (rk == TR_GPR)
      rr = FirstGPR;
   else if (rk == TR_FPR)
      rr = FirstFPR;
   else if (rk == TR_CCR)
      rr = FirstCCR;
   else if (rk == TR_VRF)
      rr = FirstVRF;
   return cg->machine()->getRealRegister(RegNum(rr+bitPos));
   }

TR_RegisterMask
OMR::Power::RealRegister::getAvailableRegistersMask(TR_RegisterKinds rk)
   {
   if (rk == TR_GPR)
      return AvailableGPRMask;
   else if (rk == TR_FPR)
      return AvailableFPRMask;
   else if (rk == TR_CCR)
      return AvailableCCRMask;
   else if (rk == TR_VRF)
      return AvailableVRFMask;
   else
      return 0;
   }

const uint8_t OMR::Power::RealRegister::fullRegBinaryEncodings[OMR::Power::RealRegister::NumRegisters] =
    {0x00, 	// NoReg
     0x00, 	// r0
     0x01, 	// r1
     0x02, 	// r2
     0x03, 	// r3
     0x04, 	// r4
     0x05, 	// r5
     0x06, 	// r6
     0x07, 	// r7
     0x08, 	// r8
     0x09, 	// r9
     0x0a, 	// r10
     0x0b, 	// r11
     0x0c, 	// r12
     0x0d, 	// r13
     0x0e, 	// r14
     0x0f, 	// r15
     0x10, 	// r16
     0x11, 	// r17
     0x12, 	// r18
     0x13, 	// r19
     0x14, 	// r20
     0x15, 	// r21
     0x16, 	// r22
     0x17, 	// r23
     0x18, 	// r24
     0x19, 	// r25
     0x1a, 	// r26
     0x1b, 	// r27
     0x1c, 	// r28
     0x1d, 	// r29
     0x1e, 	// r30
     0x1f, 	// r31
     0x00,  // fp0 || vsr0
     0x01,  // fp1 || vsr1
     0x02,  // fp2 || vsr2
     0x03,  // fp3 || vsr3
     0x04,  // fp4 || vsr4
     0x05,  // fp5 || vsr5
     0x06,  // fp6 || vsr6
     0x07,  // fp7 || vsr7
     0x08,  // fp8 || vsr8
     0x09,  // fp9 || vsr9
     0x0a,  // fp10 || vsr10
     0x0b,  // fp11 || vsr11
     0x0c,  // fp12 || vsr12
     0x0d,  // fp13 || vsr13
     0x0e,  // fp14 || vsr14
     0x0f,  // fp15 || vsr15
     0x10,  // fp16 || vsr16
     0x11,  // fp17 || vsr17
     0x12,  // fp18 || vsr18
     0x13,  // fp19 || vsr19
     0x14,  // fp20 || vsr20
     0x15,  // fp21 || vsr21
     0x16,  // fp22 || vsr22
     0x17,  // fp23 || vsr23
     0x18,  // fp24 || vsr24
     0x19,  // fp25 || vsr25
     0x1a,  // fp26 || vsr26
     0x1b,  // fp27 || vsr27
     0x1c,  // fp28 || vsr28
     0x1d,  // fp29 || vsr29
     0x1e,  // fp30 || vsr30
     0x1f,  // fp31 || vsr31
     0x00,  // vr0  || vsr32
     0x01,  // vr1  || vsr33
     0x02,  // vr2  || vsr34
     0x03,  // vr3  || vsr35
     0x04,  // vr4  || vsr36
     0x05,  // vr5  || vsr37
     0x06,  // vr6  || vsr38
     0x07,  // vr7  || vsr39
     0x08,  // vr8  || vsr40
     0x09,  // vr9  || vsr41
     0x0a,  // vr10 || vsr42
     0x0b,  // vr11 || vsr43
     0x0c,  // vr12 || vsr44
     0x0d,  // vr13 || vsr45
     0x0e,  // vr14 || vsr46
     0x0f,  // vr15 || vsr47
     0x10,  // vr16 || vsr48
     0x11,  // vr17 || vsr49
     0x12,  // vr18 || vsr50
     0x13,  // vr19 || vsr51
     0x14,  // vr20 || vsr52
     0x15,  // vr21 || vsr53
     0x16,  // vr22 || vsr54
     0x17,  // vr23 || vsr55
     0x18,  // vr24 || vsr56
     0x19,  // vr25 || vsr57
     0x1a,  // vr26 || vsr58
     0x1b,  // vr27 || vsr59
     0x1c,  // vr28 || vsr60
     0x1d,  // vr29 || vsr61
     0x1e,  // vr30 || vsr62
     0x1f,  // vr31 || vsr63
     0x00, 	// cr0
     0x01, 	// cr1
     0x02, 	// cr2
     0x03, 	// cr3
     0x04, 	// cr4
     0x05, 	// cr5
     0x06, 	// cr6
     0x07, 	// cr7
     0x00, 	// mq
     0x08, 	// lr
     0x09   // ctr
};

void
OMR::Power::RealRegister::setRegisterFieldRT(uint32_t *instruction)
   {
   if (self()->isConditionRegister())
     *instruction |= fullRegBinaryEncodings[_registerNumber] << (pos_RT + 2);
   else
     *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RT;
   }

void
OMR::Power::RealRegister::setRegisterFieldRA(uint32_t *instruction)
   {
   if (self()->isConditionRegister())
      *instruction |= fullRegBinaryEncodings[_registerNumber] << (pos_RA + 2);
   else
      *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RA;
   }
void
OMR::Power::RealRegister::setRegisterFieldRB(uint32_t *instruction)
  {
      if (self()->isConditionRegister())
        *instruction |= fullRegBinaryEncodings[_registerNumber] << (pos_RB + 2);
      else
         *instruction |= fullRegBinaryEncodings[_registerNumber] << pos_RB;
  }
void
OMR::Power::RealRegister::setRegisterFieldXS(uint32_t *instruction)
   {
   self()->setRegisterFieldRS(instruction);
   if (self()->getKind() == TR_VRF) // upper 32 registers
      *instruction |= 1 << pos_SX;
   }

void
OMR::Power::RealRegister::setRegisterFieldXT(uint32_t *instruction)
   {
   self()->setRegisterFieldRT(instruction);
   if (self()->getKind() == TR_VRF) // upper 32 registers
      *instruction |= 1 << pos_TX;
   }

void
OMR::Power::RealRegister::setRegisterFieldXA(uint32_t *instruction)
   {
   self()->setRegisterFieldRA(instruction);
   if (self()->getKind() == TR_VRF) // upper 32 registers
      *instruction |= 1 << pos_AX;
   }

void
OMR::Power::RealRegister::setRegisterFieldXB(uint32_t *instruction)
   {
   self()->setRegisterFieldRB(instruction);
   if (self()->getKind() == TR_VRF) // upper 32 registers
      *instruction |= 1 << pos_BX;
   }

void
OMR::Power::RealRegister::setRegisterFieldXC(uint32_t *instruction)
   {
   self()->setRegisterFieldRC(instruction);
   if (self()->getKind() == TR_VRF) // upper 32 registers
      *instruction |= 1 << pos_CX;
   }
