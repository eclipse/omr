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

#ifndef OMR_Z_INSTOPCODE_INCL
#define OMR_Z_INSTOPCODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTOPCODE_CONNECTOR
#define OMR_INSTOPCODE_CONNECTOR
namespace OMR { namespace Z { class InstOpCode; } }
namespace OMR { typedef OMR::Z::InstOpCode InstOpCodeConnector; }
#else
#error OMR::Z::InstOpCode expected to be a primary connector, but a OMR connector is already defined
#endif

#include <stdint.h>
#include "compiler/codegen/OMRInstOpCode.hpp"
#include "compiler/z/env/OMRCPU.hpp"
#include "env/Processors.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }

namespace OMR
{
namespace Z
{

/* Instruction Format
 *
 * Note this is different than the IA32 approach - this uses nibbles for the OpCode length, not bytes since
 * some instructions are 3 nibbles long
 * It also has an extra length field - the length of the entire instruction (not just the opcode)
 * The following are the different formats of op-codes (from the 390 POP)

   E Format
    __________________
   |      Op Code     |
   |__________________|
   0                 15

   RR Format
    ________ ____ ____
   |Op Code | R1 | R2 |
   |________|____|____|
   0         8   12  15

   RRE Format
    _________________ ________ ____ ____
   |     Op Code     |////////| R1 | R2 |
   |_________________|________|____|____|
   0                 16       24   28  31

   RRF Format
    ________________ ____ ____ ____ ____
   |    Op Code     | R1 |////| R3 | R2 |
   |________________|____|____|____|____|
   0                16   20   24   28  31

    ________________ ____ ____ ____ ____
   |    Op Code     | M3 |////| R1 | R2 |
   |________________|____|____|____|____|
   0                16   20   24   28  31

    ________________ ____ ____ ____ ____
   |    Op Code     | M3 | M4 | R1 | R2 |
   |________________|____|____|____|____|
   0                16   20   24   28  31

   The most up-to-date RRF formats combine a-b and c-e as the following:
   RRF-a-b format
    ________________ ____ ____ ____ ____
   |    Op Code     | R3 | M4*| R1 | R2 |
   |________________|____|____|____|____|
   0                16   20   24   28  31

   RRF-c-e format
    ________________ ____ ____ ____ ____
   |    Op Code     | M3*| M4*| R1 | R2 |
   |________________|____|____|____|____|
   0                16   20   24   28  31

   RX Format
    ________ ____ ____ ____ ____________
   |Op Code | R1 | X2 | B2 |     D2     |
   |________|____|____|____|____________|
   0         8   12   16   20          31

   RXE Format
    ________ ____ ____ ____ __/__ ________ ________
   |Op Code | R1 | X2 | B2 | D2  |////////|Op Code |
   |________|____|____|____|__/__|________|________|
   0         8   12   16   20    32       40      47

   RXY Format
    ________ ____ ____ ____ __/__ _______ ________
   |Op Code | R1 | X2 | B2 | D2  |D2 High|Op Code |
   |________|____|____|____|__/__|_______|________|
   0         8   12   16   20    32      40      47

   RXF Format
    ________ ____ ____ ____ _/__ ____ ____ ________
   |Op Code | R3 | X2 | B2 | D2 | R1 |////|Op Code |
   |________|____|____|____|_/__|____|____|________|
   0         8   12   16   20   32   36   40      47

   RS Format
    ________ ____ ____ ____ ____________
   |Op Code | R1 | R3 | B2 |     D2     |
   |________|____|____|____|____________|
   0         8   12   16   20          31

    ________ ____ ____ ____ ____________
   |Op Code | R1 | M3 | B2 |     D2     |
   |________|____|____|____|____________|
   0         8   12   16   20          31

   RSY Format
    ________ ____ ____ ____ ____________ ________
   |Op Code | R1 | R3 | B2 |     D2     |Op Code |
   |________|____|____|____|____________|________|
   0         8   12   16   20           40       47


   RSE Format
    ________ ____ ____ ____ __/__ ________ ________
   |Op Code | R1 | R3 | B2 | D2  |////////|Op Code |
   |________|____|____|____|__/__|________|________|
   0         8   12   16   20    32       40      47

    ________ ____ ____ ____ __/__ ________ ________
   |Op Code | R1 | M3 | B2 | D2  |////////|Op Code |
   |________|____|____|____|__/__|________|________|
   0         8   12   16   20    32       40      47

   RSL Format
    ________ ____ ____ ____ __/__ ________ ________
   |Op Code | L1 | // | B2 | D2  |////////|Op Code |
   |________|____|____|____|__/__|________|________|
   0         8   12   16   20    32       40      47

   RSI Format
    ________ ____ ____ _________________
   |Op Code | R1 | R3 |       I2        |
   |________|____|____|_________________|
   0         8   12   16               31

   RI Format
    ________ ____ ____ _________________
   |Op Code | R1 |OpCd|       I2        |
   |________|____|____|_________________|
   0         8   12   16               31

   RIL Format
    ________ ____ ____ ____ _______________________
   |Op Code | R1 |OpCd|            I2              |
   |________|____|____|____|_______________________|
   0         8   12   16   20                     47

   SI Format
    ________ _________ ____ ____________
   |Op Code |    I2   | B1 |     D1     |
   |________|_________|____|____________|
   0         8        16   20          31

   SIY Format
   ________ __________ ____ __/__ ____________________
   |Op Code |    I2   | B1 | D1 Low |D1 High| Op Code|
   |________|_________|____|__/_____|_______|________|
   0         8        16   20       32      40      47

   S Format
    __________________ ____ ____________
   |     Op Code      | B2 |     D2     |
   |__________________|____|____________|
   0                  16   20          31

   SS Format
    ________ _________ ____ ___/____ ____ ____/____
   |Op Code |    L    | B1 |   D1   | B2 |   D2    |
   |________|_________|____|___/____|____|____/____|
   0         8        16   20       32   36       47

    ________ ____ ____ ____ ___/____ ____ ____/____
   |Op Code | L1 | L2 | B1 |   D1   | B2 |   D2    |
   |________|____|____|____|___/____|____|____/____|
   0         8   12   16   20       32   36       47

    ________ ____ ____ ____ ___/____ ____ ____/____
   |Op Code | R1 | R3 | B1 |   D1   | B2 |   D2    |
   |________|____|____|____|___/____|____|____/____|
   0         8   12   16   20       32   36       47

 |  ________ ____ ____ ____ ___/____ ____ ____/____
 | |Op Code | R1 | R3 | B2 |   D2   | B4 |   D4    |
 | |________|____|____|____|___/____|____|____/____|
 | 0         8   12   16   20       32   36       47

   SSE Format
    __________________ ____ ___/____ ____ ____/____
   |     Op Code      | B1 |   D1   | B2 |   D2    |
   |__________________|____|___/____|____|____/____|
   0                  16   20       32   36       47

   RIE Format
    ________ ____ ____ ____/__ __ ________ ________
   |Op Code | R1 | R3 |    I2    |////////|Op Code |
   |________|____|____|____/_____|________|________|
   0         8   12   16         32       40      47


   RRR Format
    __________________ ____ ____ ____ ____
   |     Op Code      | R3 |////| R1 | R2 |
   |__________________|____|____|____|____|
   0                  16   20   24  28   31

   SSF Format
    ________ ____ ____ ____ ___/____ ____ ____/____
   |Op Code | R3 | Op | B1 |   D1   | B2 |   D2    |
   |________|____|____|____|___/____|____|____/____|
   0         8   12   16   20       32   36       47

*/
#define   PSEUDO        0
#define   DC_FORMAT     1
#define   E_FORMAT      2
#define   I_FORMAT      3
#define   IE_FORMAT     4
#define   MII_FORMAT    5
#define   RIa_FORMAT    7
#define   RIb_FORMAT    8
#define   RIc_FORMAT    9
#define   RIEa_FORMAT   11
#define   RIEb_FORMAT   12
#define   RIEc_FORMAT   13
#define   RIEd_FORMAT   14
#define   RIEe_FORMAT   15
#define   RIEf_FORMAT   16
#define   RIEg_FORMAT   17
#define   RILa_FORMAT   19
#define   RILb_FORMAT   20
#define   RILc_FORMAT   21
#define   RIS_FORMAT    22
#define   RR_FORMAT     23
#define   RRD_FORMAT    24
#define   RRE_FORMAT    25
#define   RRFa_FORMAT   29
#define   RRFb_FORMAT   30
#define   RRFc_FORMAT   31
#define   RRFd_FORMAT   32
#define   RRFe_FORMAT   33
#define   RRS_FORMAT    34
#define   RSa_FORMAT    36
#define   RSb_FORMAT    37
#define   RSI_FORMAT    38
#define   RSLa_FORMAT   40 
#define   RSLb_FORMAT   41
#define   RSYa_FORMAT   43
#define   RSYb_FORMAT   44
#define   RXa_FORMAT    46
#define   RXb_FORMAT    47
#define   RXE_FORMAT    48
#define   RXF_FORMAT    49
#define   RXYa_FORMAT   51
#define   RXYb_FORMAT   52
#define   S_FORMAT      53
#define   SI_FORMAT     54
#define   SIL_FORMAT    55
#define   SIY_FORMAT    56
#define   SMI_FORMAT    57
#define   SSa_FORMAT    60
#define   SSb_FORMAT    61
#define   SSc_FORMAT    62
#define   SSd_FORMAT    63
#define   SSe_FORMAT    64
#define   SSf_FORMAT    65
#define   SSE_FORMAT    66
#define   SSF_FORMAT    67
#define   VRIa_FORMAT   68
#define   VRIb_FORMAT   69
#define   VRIc_FORMAT   70
#define   VRId_FORMAT   71
#define   VRIe_FORMAT   72
#define   VRIf_FORMAT   73
#define   VRIg_FORMAT   74
#define   VRIh_FORMAT   75
#define   VRIi_FORMAT   76
#define   VRRa_FORMAT   77
#define   VRRb_FORMAT   78
#define   VRRc_FORMAT   79
#define   VRRd_FORMAT   80
#define   VRRe_FORMAT   81
#define   VRRf_FORMAT   82
#define   VRRg_FORMAT   83
#define   VRRh_FORMAT   84
#define   VRRi_FORMAT   85
#define   VRSa_FORMAT   86
#define   VRSb_FORMAT   87
#define   VRSc_FORMAT   88
#define   VRSd_FORMAT   89
#define   VRV_FORMAT    90
#define   VRX_FORMAT    91
#define   VSI_FORMAT    92

/* Instruction Properties (One hot encoding) */
#define S390OpProp_None                   static_cast<uint64_t>(0x0000000000000000ull)
#define S390OpProp_UsesTarget             static_cast<uint64_t>(0x0000000000000001ull)
#define S390OpProp_SingleFP               static_cast<uint64_t>(0x0000000000000002ull)
#define S390OpProp_DoubleFP               static_cast<uint64_t>(0x0000000000000004ull)
#define S390OpProp_SetsZeroFlag           static_cast<uint64_t>(0x0000000000000008ull)
#define S390OpProp_SetsSignFlag           static_cast<uint64_t>(0x0000000000000010ull)
// Available                              static_cast<uint64_t>(0x0000000000000020ull)
#define S390OpProp_SetsOverflowFlag       static_cast<uint64_t>(0x0000000000000040ull)
#define S390OpProp_BranchOp               static_cast<uint64_t>(0x0000000000000080ull)
#define S390OpProp_IsLoad                 static_cast<uint64_t>(0x0000000000000100ull)
#define S390OpProp_IsStore                static_cast<uint64_t>(0x0000000000000200ull)
// Available                              static_cast<uint64_t>(0x0000000000000400ull)
// Available                              static_cast<uint64_t>(0x0000000000000800ull)
// Available                              static_cast<uint64_t>(0x0000000000001000ull)
#define S390OpProp_Is64Bit                static_cast<uint64_t>(0x0000000000002000ull)
#define S390OpProp_Is32Bit                static_cast<uint64_t>(0x0000000000004000ull)
#define S390OpProp_Is32To64Bit            static_cast<uint64_t>(0x0000000000008000ull)
#define S390OpProp_IsCall                 static_cast<uint64_t>(0x0000000000010000ull)
#define S390OpProp_ReadsCC                static_cast<uint64_t>(0x0000000000020000ull)
#define S390OpProp_SetsCompareFlag        static_cast<uint64_t>(0x0000000000040000ull)
#define S390OpProp_SetsCC                 static_cast<uint64_t>(0x0000000000080000ull)
#define S390OpProp_LongDispSupported      static_cast<uint64_t>(0x0000000000100000ull)
#define S390OpProp_IsExtendedImmediate    static_cast<uint64_t>(0x0000000000200000ull)
#define S390OpProp_UsesRegPairForTarget   static_cast<uint64_t>(0x0000000000400000ull)
#define S390OpProp_UsesRegPairForSource   static_cast<uint64_t>(0x0000000000800000ull)
#define S390OpProp_UsesRegRangeForTarget  static_cast<uint64_t>(0x0000000001000000ull)
#define S390OpProp_IsRegCopy              static_cast<uint64_t>(0x0000000002000000ull)
#define S390OpProp_Trap                   static_cast<uint64_t>(0x0000000004000000ull)
// Available                              static_cast<uint64_t>(0x0000000008000000ull)
// Available                              static_cast<uint64_t>(0x0000000010000000ull)
#define S390OpProp_ReadsFPC               static_cast<uint64_t>(0x0000000020000000ull)
#define S390OpProp_SetsFPC                static_cast<uint64_t>(0x0000000040000000ull)
// Available                              static_cast<uint64_t>(0x0000000080000000ull)
#define S390OpProp_TargetHW               static_cast<uint64_t>(0x0000000100000000ull)
// Available                              static_cast<uint64_t>(0x0000000200000000ull)
#define S390OpProp_SrcHW                  static_cast<uint64_t>(0x0000000400000000ull)
// Available                              static_cast<uint64_t>(0x0000000800000000ull)
#define S390OpProp_Src2HW                 static_cast<uint64_t>(0x0000001000000000ull)
// Available                              static_cast<uint64_t>(0x0000002000000000ull)
#define S390OpProp_HasTwoMemoryReferences static_cast<uint64_t>(0x0000004000000000ull)
#define S390OpProp_ImplicitlyUsesGPR0     static_cast<uint64_t>(0x0000008000000000ull)
#define S390OpProp_ImplicitlyUsesGPR1     static_cast<uint64_t>(0x0000010000000000ull)
#define S390OpProp_ImplicitlyUsesGPR2     static_cast<uint64_t>(0x0000020000000000ull)
#define S390OpProp_ImplicitlySetsGPR0     static_cast<uint64_t>(0x0000040000000000ull)
#define S390OpProp_ImplicitlySetsGPR1     static_cast<uint64_t>(0x0000080000000000ull)
#define S390OpProp_ImplicitlySetsGPR2     static_cast<uint64_t>(0x0000100000000000ull)
#define S390OpProp_IsCompare              static_cast<uint64_t>(0x0000200000000000ull)
#define S390OpProp_UsesM3                 static_cast<uint64_t>(0x0000400000000000ull)
#define S390OpProp_UsesM4                 static_cast<uint64_t>(0x0000800000000000ull)
#define S390OpProp_UsesM5                 static_cast<uint64_t>(0x0001000000000000ull)
#define S390OpProp_UsesM6                 static_cast<uint64_t>(0x0002000000000000ull)
#define S390OpProp_HasExtendedMnemonic    static_cast<uint64_t>(0x0004000000000000ull)
#define S390OpProp_VectorStringOp         static_cast<uint64_t>(0x0008000000000000ull)
#define S390OpProp_VectorFPOp             static_cast<uint64_t>(0x0010000000000000ull)
// Available                              static_cast<uint64_t>(0x0020000000000000ull)
// Available                              static_cast<uint64_t>(0x0040000000000000ull)
// Available                              static_cast<uint64_t>(0x0080000000000000ull)
#define S390OpProp_SetsOperand1           static_cast<uint64_t>(0x0100000000000000ull)
#define S390OpProp_SetsOperand2           static_cast<uint64_t>(0x0200000000000000ull)
#define S390OpProp_SetsOperand3           static_cast<uint64_t>(0x0400000000000000ull)
#define S390OpProp_SetsOperand4           static_cast<uint64_t>(0x0800000000000000ull)
#define S390OpProp_ImplicitlySetsGPR3     static_cast<uint64_t>(0x1000000000000000ull)
#define S390OpProp_ImplicitlySetsGPR4     static_cast<uint64_t>(0x2000000000000000ull)
#define S390OpProp_ImplicitlySetsGPR5     static_cast<uint64_t>(0x4000000000000000ull)
// Available                              static_cast<uint64_t>(0x8000000000000000ull)

class InstOpCode: public OMR::InstOpCode
   {
   protected:

   InstOpCode():  OMR::InstOpCode(BAD)  {}
   InstOpCode(Mnemonic m): OMR::InstOpCode(m)  {}

   public:

   enum S390BranchCondition
      {
                        /* Instruction Type    |  Meaning (Operands)
                           ----------------------------------------- */
      COND_NOPR,        // Conditional Branch     No operation (RR)
      COND_NOP,         // Conditional Branch     No Operation (RX)
      COND_VGNOP,       // Virtual Guard          No Operation 
      COND_BRO,         // Relative Branch        Branch Relative on Overflow 
      COND_BRH,         // Relative Branch        Branch Relative on High
      COND_BRP,         // Relative Branch        Branch Relative on Plus 
      COND_BRL,         // Relative Branch        Branch Relative on A Low
      COND_BRM,         // Relative Branch        Branch Relative on Minus
      COND_BRNE,        // Relative Branch        Branch Relative on A Not Equal B
      COND_BRNZ,        // Relative Branch        Branch Relative on Not Zero
      COND_BRE,         // Relative Branch        Branch Relative on A Equal B 
      COND_BRZ,         // Relative Branch        Branch Relative on Zero 
      COND_BRNH,        // Relative Branch        Branch Relative on A Not High 
      COND_BRNL,        // Relative Branch        Branch Relative on A Not Low
      COND_BRNM,        // Relative Branch        Branch Relative on Not Minus 
      COND_BRNP,        // Relative Branch        Branch Relative on Not Plus
      COND_BRNO,        // Relative Branch        Branch Relative on No Overflow 
      COND_B,           // Conditional Branch     Unconditional Branch (RX)
      COND_BR,          // Branch on Condition    Unconditional Branch (RR)
      COND_BRU,         // Relative Branch        Unconditional Branch Relative 
      COND_BRUL,        // Relative Branch        Unconditional Branch Relative
      COND_BC,          // Conditional Branch     Branch on Condition (RX)  
      COND_BCR,         // Conditional Branch     Branch on Condition (RR)
      COND_BE,          // Conditional Branch     Branch on A Equal B (RX)
      COND_BER,         // Conditional Branch     Branch on A Equal B (RR) 
      COND_BH,          // Conditional Branch     Branch on A High (RX)
      COND_BHR,         // Conditional Branch     Branch on A High (RR)
      COND_BL,          // Conditional Branch     Branch on A Low (RX)
      COND_BLR,         // Conditional Branch     Branch on A Low (RR) 
      COND_BM,          // Conditional Branch     Branch on Minus (RX)
      COND_BMR,         // Conditional Branch     Branch on Minus (RR)
      COND_BNE,         // Conditional Branch     Branch on A Not Equal B (RX)
      COND_BNER,        // Conditional Branch     Branch on A Not Equal B (RR)
      COND_BNH,         // Conditional Branch     Branch on A Not High (RX)
      COND_BNHR,        // Conditional Branch     Branch on A Not High (RR) 
      COND_BNL,         // Conditional Branch     Branch on A Not Low (RX)
      COND_BNLR,        // Conditional Branch     Branch on A Not Low (RR) 
      COND_BNM,         // Conditional Branch     Branch on Not Minus (RX)
      COND_BNMR,        // Conditional Branch     Branch on Not Minus (RR)
      COND_BNO,         // Conditional Branch     Branch on No Overflow (RX)
      COND_BNOR,        // Conditional Branch     Branch on No Overflow (RR) 
      COND_BNP,         // Conditional Branch     Branch on Not Plus (RX) 
      COND_BNPR,        // Conditional Branch     Branch on Not Plus (RR)
      COND_BNZ,         // Conditional Branch     Branch on Not Zero (RX)
      COND_BNZR,        // Conditional Branch     Branch on Not Zero (RR)
      COND_BO,          // Conditional Branch     Branch on Overflow (RX) 
      COND_BOR,         // Conditional Branch     Branch on Overflow (RR)
      COND_BP,          // Conditional Branch     Branch on Plus (RX)
      COND_BPR,         // Conditional Branch     Branch on Plus (RR)
      COND_BRC,         // Relative Branch        Branch Relative on Condition               
      COND_BZ,          // Conditional Branch     Branch on Zero (RX)
      COND_BZR,         // Conditional Branch     Branch on Zero (RR)
      COND_MASK0,
      COND_MASK1,
      COND_MASK2,
      COND_MASK3,
      COND_MASK4,
      COND_MASK5,
      COND_MASK6,
      COND_MASK7,
      COND_MASK8,
      COND_MASK9,
      COND_MASK10,
      COND_MASK11,
      COND_MASK12,
      COND_MASK13,
      COND_MASK14,
      COND_MASK15,
      COND_CC0 = COND_MASK8,
      COND_CC1 = COND_MASK4,
      COND_CC2 = COND_MASK2,
      COND_CC3 = COND_MASK1,
      lastBranchCondition = COND_MASK15,
      S390NumBranchConditions = lastBranchCondition +1
      };

   /**
    *  \brief
    *      Defines various metadata of an instruction including the name, description, opcodes, format, the minimum
    *      architecture level set (ALS) which introduced the instruction, and the various properties which model the
    *      instruction in a way that the code generator understands.
    */
   struct OpCodeMetaData
      {
      /**
       *  \brief
       *      The instruction mnemonic.
       */
      OMR::InstOpCode::Mnemonic mnemonic;

      /**
       *  \brief
       *      The instruction mnemonic as defined by Principles of Operation.
       */
      const char* name;

      /**
       *  \brief
       *      The fully qualified instruction name as defined by Principles of Operation.
       */
      const char* description;

      /**
       *  \brief
       *      The instruction opcode which is at most two bytes in length.
       */
      uint8_t opcode[2];

      /**
       *  \brief
       *      The instruction format as defined by Principles of Operation.
       */
      uint8_t format;

      /**
       *  \brief
       *      The minimum architecture level set (ALS) which introduced this instruction.
       */
      CPU::Architecture minimumALS;

      /**
       *  \brief
       *      The properties describing the behavior of this instruction to the codegen.
       */
      uint64_t properties;
      };

   static const OpCodeMetaData metadata[NumOpCodes];

   /**
    * \brief
    *    Gets the minimum architecture level set (ALS) which introduced this instruction.
    *
    * \return
    *    The minimum ALS of this instruction.
    */
   CPU::Architecture getMinimumALS() const
      {
      return metadata[_mnemonic].minimumALS;
      }

   /* Queries for instruction properties */
   uint32_t hasBypass();
   uint32_t isAdmin();
   uint64_t isOperandHW(uint32_t i);
   uint64_t setsOperand(uint32_t opNum);

   uint64_t singleFPOp() {return metadata[_mnemonic].properties & S390OpProp_SingleFP;}
   uint64_t doubleFPOp() {return metadata[_mnemonic].properties & S390OpProp_DoubleFP;}
   uint64_t gprOp() {return (metadata[_mnemonic].properties & (S390OpProp_DoubleFP | S390OpProp_SingleFP)) == 0;}
   uint64_t fprOp() {return metadata[_mnemonic].properties & (S390OpProp_DoubleFP | S390OpProp_SingleFP);}

   uint64_t isBranchOp() {return metadata[_mnemonic].properties & S390OpProp_BranchOp;}
   uint64_t isTrap() {return (metadata[_mnemonic].properties & S390OpProp_Trap)!=0;}
   uint64_t isLoad() {return metadata[_mnemonic].properties & S390OpProp_IsLoad;}
   uint64_t isStore() {return metadata[_mnemonic].properties & S390OpProp_IsStore;}
   uint64_t isCall() {return metadata[_mnemonic].properties & S390OpProp_IsCall;}
   uint64_t isCompare() {return metadata[_mnemonic].properties & S390OpProp_IsCompare;}
   uint64_t isExtendedImmediate() {return metadata[_mnemonic].properties & S390OpProp_IsExtendedImmediate;}
   uint64_t usesTarget() {return metadata[_mnemonic].properties & S390OpProp_UsesTarget;}

   uint64_t is64bit() {return metadata[_mnemonic].properties & S390OpProp_Is64Bit;}
   uint64_t is32bit() {return metadata[_mnemonic].properties & S390OpProp_Is32Bit;}
   uint64_t is32to64bit() {return metadata[_mnemonic].properties & S390OpProp_Is32To64Bit;}

   uint64_t hasLongDispSupport() {return metadata[_mnemonic].properties & S390OpProp_LongDispSupported;}
   uint64_t usesRegPairForTarget() {return metadata[_mnemonic].properties & S390OpProp_UsesRegPairForTarget; }
   uint64_t usesRegPairForSource() {return metadata[_mnemonic].properties & S390OpProp_UsesRegPairForSource; }
   uint64_t usesRegRangeForTarget(){return metadata[_mnemonic].properties & S390OpProp_UsesRegRangeForTarget; }
   uint64_t canUseRegPairForTarget() {return usesRegPairForTarget() || usesRegRangeForTarget(); }
   uint64_t shouldUseRegPairForTarget() {return usesRegPairForTarget(); }

   uint64_t implicitlyUsesGPR0() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlyUsesGPR0; }
   uint64_t implicitlyUsesGPR1() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlyUsesGPR1; }
   uint64_t implicitlyUsesGPR2() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlyUsesGPR2; }

   uint64_t implicitlySetsGPR0() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR0; }
   uint64_t implicitlySetsGPR1() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR1; }
   uint64_t implicitlySetsGPR2() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR2; }
   uint64_t implicitlySetsGPR3() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR3; }
   uint64_t implicitlySetsGPR4() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR4; }
   uint64_t implicitlySetsGPR5() { return metadata[_mnemonic].properties & S390OpProp_ImplicitlySetsGPR5; }

   uint64_t setsOperand1() { return metadata[_mnemonic].properties & S390OpProp_SetsOperand1; }
   uint64_t setsOperand2() { return metadata[_mnemonic].properties & S390OpProp_SetsOperand2; }
   uint64_t setsOperand3() { return metadata[_mnemonic].properties & S390OpProp_SetsOperand3; }
   uint64_t setsOperand4() { return metadata[_mnemonic].properties & S390OpProp_SetsOperand4; }

   uint64_t setsCC() {return (setsZeroFlag() || setsSignFlag() || setsOverflowFlag() || setsCompareFlag() || setsCarryFlag() || (metadata[_mnemonic].properties & S390OpProp_SetsCC));}
   uint64_t readsCC() {return metadata[_mnemonic].properties & S390OpProp_ReadsCC;}

   uint64_t setsZeroFlag() {return metadata[_mnemonic].properties & S390OpProp_SetsZeroFlag;}
   uint64_t setsSignFlag() {return metadata[_mnemonic].properties & S390OpProp_SetsSignFlag;}
   uint64_t setsOverflowFlag() {return metadata[_mnemonic].properties & S390OpProp_SetsOverflowFlag;}
   uint64_t setsCompareFlag() {return metadata[_mnemonic].properties & S390OpProp_SetsCompareFlag;}
   uint64_t setsCarryFlag() { return setsZeroFlag(); }

   uint64_t isRegCopy() {return metadata[_mnemonic].properties & S390OpProp_IsRegCopy; }
   uint64_t hasTwoMemoryReferences() {return metadata[_mnemonic].properties & S390OpProp_HasTwoMemoryReferences;}

   uint64_t readsFPC() {return metadata[_mnemonic].properties & S390OpProp_ReadsFPC; }
   uint64_t setsFPC() {return metadata[_mnemonic].properties & S390OpProp_SetsFPC; }

   uint64_t isLabel() {return _mnemonic == LABEL;}

   uint64_t usesM3() {return metadata[_mnemonic].properties & S390OpProp_UsesM3;}
   uint64_t usesM4() {return metadata[_mnemonic].properties & S390OpProp_UsesM4;}
   uint64_t usesM5() {return metadata[_mnemonic].properties & S390OpProp_UsesM5;}
   uint64_t usesM6() {return metadata[_mnemonic].properties & S390OpProp_UsesM6;}
   uint64_t hasExtendedMnemonic() {return metadata[_mnemonic].properties & S390OpProp_HasExtendedMnemonic;}
   uint64_t isVectorStringOp() {return metadata[_mnemonic].properties & S390OpProp_VectorStringOp;}
   uint64_t isVectorFPOp() {return metadata[_mnemonic].properties & S390OpProp_VectorFPOp;}

   /* Static */
   static void copyBinaryToBufferWithoutClear(uint8_t *cursor, Mnemonic i_opCode);
   static void copyBinaryToBuffer(uint8_t *cursor, Mnemonic i_opCode);
   static uint8_t getInstructionLength(Mnemonic i_opCode); /* Using a table that maps values of bit 0-1 to instruction length from POP chapter 5,  topic: Instruction formats (page 5.5)*/
   static uint8_t  getInstructionFormat(Mnemonic i_opCode)  { return metadata[i_opCode].format; }
   static const uint8_t * getOpCodeBinaryRepresentation(Mnemonic i_opCode) { return metadata[i_opCode].opcode ;}


   /* Non-Static methods that calls static implementations */
   void copyBinaryToBufferWithoutClear(uint8_t *cursor){ copyBinaryToBufferWithoutClear(cursor, _mnemonic); }
   void copyBinaryToBuffer(uint8_t *cursor){ copyBinaryToBuffer(cursor, _mnemonic); }
   uint8_t getInstructionLength() { return getInstructionLength(_mnemonic);}
   uint8_t getInstructionFormat() { return getInstructionFormat(_mnemonic);}
   const uint8_t * getOpCodeBinaryRepresentation() { return getOpCodeBinaryRepresentation(_mnemonic);}

   /** \brief
     *    Gets the 20-bit displacement mnemonic which is equivalent to a 12-bit displacement mnemonic.
     *
     *  \param op
     *     The 12-bit displacement mnemonic for which to find an equivalent 20-bit displacement mnemonic.
     *
     *  \return
     *     The 20-bit displacement mnemonic if it exists, otherwise `BAD` mnemonic.
     */
   static Mnemonic getEquivalentLongDisplacementMnemonic(Mnemonic op);

   /* Static methods for getting the correct opCode depending on platform (32bit, 64bit, etc.) */
   static Mnemonic getLoadOnConditionRegOpCode();
   static Mnemonic getLoadAddressOpCode();
   static Mnemonic getLoadOpCode();
   static Mnemonic getLoadAndMaskOpCode();
   static Mnemonic getExtendedLoadOpCode();
   static Mnemonic getLoadRegOpCode();
   static Mnemonic getLoadRegOpCodeFromNode(TR::CodeGenerator *cg, TR::Node *node);
   static Mnemonic getLoadTestRegOpCode();
   static Mnemonic getLoadComplementOpCode();
   static Mnemonic getLoadHalfWordOpCode();
   static Mnemonic getLoadHalfWordImmOpCode();
   static Mnemonic getLoadPositiveOpCode();
   static Mnemonic getLoadMultipleOpCode();
   static Mnemonic getLoadNegativeOpCode();
   static Mnemonic getLoadAndTrapOpCode();
   static Mnemonic getExtendedStoreOpCode();
   static Mnemonic getStoreOpCode();
   static Mnemonic getStoreMultipleOpCode();
   static Mnemonic getCmpHalfWordImmOpCode();
   static Mnemonic getCmpHalfWordImmToMemOpCode();
   static Mnemonic getAndRegOpCode();
   static Mnemonic getAndOpCode();
   static Mnemonic getBranchOnCountOpCode();
   static Mnemonic getAddOpCode();
   static Mnemonic getAddRegOpCode();
   static Mnemonic getAddThreeRegOpCode();
   static Mnemonic getAddLogicalThreeRegOpCode();
   static Mnemonic getAddLogicalImmOpCode();
   static Mnemonic getAddLogicalRegRegImmediateOpCode();
   static Mnemonic getSubstractOpCode();
   static Mnemonic getSubstractRegOpCode();
   static Mnemonic getSubtractThreeRegOpCode();
   static Mnemonic getSubtractLogicalImmOpCode();
   static Mnemonic getSubtractLogicalThreeRegOpCode();
   static Mnemonic getMultiplySingleOpCode();
   static Mnemonic getMultiplySingleRegOpCode();
   static Mnemonic getOrOpCode();
   static Mnemonic getOrRegOpCode();
   static Mnemonic getOrThreeRegOpCode();
   static Mnemonic getXOROpCode();
   static Mnemonic getXORRegOpCode();
   static Mnemonic getXORThreeRegOpCode();
   static Mnemonic getCmpTrapOpCode();
   static Mnemonic getCmpImmOpCode();
   static Mnemonic getCmpImmTrapOpCode();
   static Mnemonic getCmpImmBranchRelOpCode();
   static Mnemonic getCmpLogicalTrapOpCode();
   static Mnemonic getCmpLogicalImmTrapOpCode();
   static Mnemonic getCmpOpCode();
   static Mnemonic getCmpRegOpCode();
   static Mnemonic getCmpRegAndBranchRelOpCode();
   static Mnemonic getCmpLogicalOpCode();
   static Mnemonic getCmpLogicalRegOpCode();
   static Mnemonic getCmpAndSwapOpCode();
   static Mnemonic getCmpLogicalImmOpCode();
   static Mnemonic getShiftLeftLogicalSingleOpCode();
   static Mnemonic getShiftRightLogicalSingleOpCode();
   static Mnemonic getAddHalfWordImmOpCode();
   static Mnemonic getAddHalfWordImmDistinctOperandOpCode();
   static Mnemonic getAddLogicalOpCode();
   static Mnemonic getAddLogicalRegOpCode();
   static Mnemonic getBranchOnIndexHighOpCode();
   static Mnemonic getBranchOnIndexEqOrLowOpCode();
   static Mnemonic getBranchRelIndexHighOpCode();
   static Mnemonic getBranchRelIndexEqOrLowOpCode();
   static Mnemonic getMoveHalfWordImmOpCodeFromStoreOpCode(Mnemonic storeOpCode);

   /*
    * Define 32->64 bit widening instructions on 64 bit architecture,
    * on 32 bit architecture, they are just regular instructions.
    */
   static Mnemonic getLoadWidenOpCode();
   static Mnemonic getLoadRegWidenOpCode();
   static Mnemonic getLoadTestRegWidenOpCode();
   static Mnemonic getAddWidenOpCode();
   static Mnemonic getAddRegWidenOpCode();
   static Mnemonic getSubstractWidenOpCode();
   static Mnemonic getSubStractRegWidenOpCode();
   static Mnemonic getCmpWidenOpCode();
   static Mnemonic getCmpRegWidenOpCode();
   static Mnemonic getCmpLogicalWidenOpCode();
   static Mnemonic getCmpLogicalRegWidenOpCode();
   static Mnemonic getLoadComplementRegWidenOpCode();
   static Mnemonic getLoadPositiveRegWidenOpCode();
   static Mnemonic getSubtractWithBorrowOpCode();

   /*  Define Golden Eagle instructions   */
   static Mnemonic getLoadTestOpCode();

   /*  Define z6 instructions             */
   static Mnemonic getStoreRelativeLongOpCode();
   static Mnemonic getLoadRelativeLongOpCode();
   static Mnemonic getMoveHalfWordImmOpCode();
   };
}
}
#endif /* OMR_Z_INSTOPCODE_INCL */
