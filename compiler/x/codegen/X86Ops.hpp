/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef X86OPS_INCL
#define X86OPS_INCL

#include <stdint.h>          // for uint32_t, uint8_t
#include "infra/Assert.hpp"  // for TR_ASSERT

namespace TR { class CodeGenerator; }
namespace TR { class Register; }

enum TR_X86OpCodes : uint32_t
   {
#define INSTRUCTION(name, mnemonic, binary, property0, property1) name
#include "codegen/X86Ops.ins"
#undef INSTRUCTION
   };

#define IA32LongToShortBranchConversionOffset ((int)JMP4 - (int)JMP1)
#define IA32LengthOfShortBranch               2

// Size-parameterized opcodes
//
template <TR_X86OpCodes Op64, TR_X86OpCodes Op32>
inline TR_X86OpCodes SizeParameterizedOpCode(bool is64Bit =
#ifdef TR_TARGET_64BIT
    true
#else
    false
#endif
)
   {
   return is64Bit ? Op64 : Op32;
   }

#define BSFRegReg      SizeParameterizedOpCode<BSF8RegReg      , BSF4RegReg      >
#define BSWAPReg       SizeParameterizedOpCode<BSWAP8Reg       , BSWAP4Reg       >
#define BSRRegReg      SizeParameterizedOpCode<BSR8RegReg      , BSR4RegReg      >
#define BTRegReg       SizeParameterizedOpCode<BT8RegReg       , BT4RegReg       >
#define CMOVBRegReg    SizeParameterizedOpCode<CMOVB8RegReg    , CMOVB4RegReg    >
#define CMOVARegMem    SizeParameterizedOpCode<CMOVA8RegMem    , CMOVA4RegMem    >
#define CMOVERegMem    SizeParameterizedOpCode<CMOVE8RegMem    , CMOVE4RegMem    >
#define CMOVERegReg    SizeParameterizedOpCode<CMOVE8RegReg    , CMOVE4RegReg    >
#define CMOVNERegMem   SizeParameterizedOpCode<CMOVNE8RegMem   , CMOVNE4RegMem   >
#define CMOVNERegReg   SizeParameterizedOpCode<CMOVNE8RegReg   , CMOVNE4RegReg   >
#define CMOVPRegMem    SizeParameterizedOpCode<CMOVP8RegMem    , CMOVP4RegMem    >
#define CMPRegImms     SizeParameterizedOpCode<CMP8RegImms     , CMP4RegImms     >
#define CMPRegImm4     SizeParameterizedOpCode<CMP8RegImm4     , CMP4RegImm4     >
#define CMPMemImms     SizeParameterizedOpCode<CMP8MemImms     , CMP4MemImms     >
#define CMPMemImm4     SizeParameterizedOpCode<CMP8MemImm4     , CMP4MemImm4     >
#define MOVRegReg      SizeParameterizedOpCode<MOV8RegReg      , MOV4RegReg      >
#define LEARegMem      SizeParameterizedOpCode<LEA8RegMem      , LEA4RegMem      >
#define LRegMem        SizeParameterizedOpCode<L8RegMem        , L4RegMem        >
#define SMemReg        SizeParameterizedOpCode<S8MemReg        , S4MemReg        >
#define SMemImm4       SizeParameterizedOpCode<S8MemImm4       , S4MemImm4       >
#define XRSMemImm4     SizeParameterizedOpCode<XRS8MemImm4     , XRS4MemImm4     >
#define XCHGRegReg     SizeParameterizedOpCode<XCHG8RegReg     , XCHG4RegReg     >
#define NEGReg         SizeParameterizedOpCode<NEG8Reg         , NEG4Reg         >
#define IMULRegReg     SizeParameterizedOpCode<IMUL8RegReg     , IMUL4RegReg     >
#define IMULRegMem     SizeParameterizedOpCode<IMUL8RegMem     , IMUL4RegMem     >
#define IMULRegRegImms SizeParameterizedOpCode<IMUL8RegRegImms , IMUL4RegRegImms >
#define IMULRegRegImm4 SizeParameterizedOpCode<IMUL8RegRegImm4 , IMUL4RegRegImm4 >
#define IMULRegMemImms SizeParameterizedOpCode<IMUL8RegMemImms , IMUL4RegMemImms >
#define IMULRegMemImm4 SizeParameterizedOpCode<IMUL8RegMemImm4 , IMUL4RegMemImm4 >
#define INCMem         SizeParameterizedOpCode<INC8Mem         , INC4Mem         >
#define INCReg         SizeParameterizedOpCode<INC8Reg         , INC4Reg         >
#define DECMem         SizeParameterizedOpCode<DEC8Mem         , DEC4Mem         >
#define DECReg         SizeParameterizedOpCode<DEC8Reg         , DEC4Reg         >
#define ADDMemImms     SizeParameterizedOpCode<ADD8MemImms     , ADD4MemImms     >
#define ADDRegImms     SizeParameterizedOpCode<ADD8RegImms     , ADD4RegImms     >
#define ADDRegImm4     SizeParameterizedOpCode<ADD8RegImm4     , ADD4RegImm4     >
#define ADCMemImms     SizeParameterizedOpCode<ADC8MemImms     , ADC4MemImms     >
#define ADCRegImms     SizeParameterizedOpCode<ADC8RegImms     , ADC4RegImms     >
#define ADCRegImm4     SizeParameterizedOpCode<ADC8RegImm4     , ADC4RegImm4     >
#define SUBMemImms     SizeParameterizedOpCode<SUB8MemImms     , SUB4MemImms     >
#define SUBRegImms     SizeParameterizedOpCode<SUB8RegImms     , SUB4RegImms     >
#define SBBMemImms     SizeParameterizedOpCode<SBB8MemImms     , SBB4MemImms     >
#define SBBRegImms     SizeParameterizedOpCode<SBB8RegImms     , SBB4RegImms     >
#define ADDMemImm4     SizeParameterizedOpCode<ADD8MemImm4     , ADD4MemImm4     >
#define ADDMemReg      SizeParameterizedOpCode<ADD8MemReg      , ADD4MemReg      >
#define ADDRegReg      SizeParameterizedOpCode<ADD8RegReg      , ADD4RegReg      >
#define ADDRegMem      SizeParameterizedOpCode<ADD8RegMem      , ADD4RegMem      >
#define LADDMemReg     SizeParameterizedOpCode<LADD8MemReg     , LADD4MemReg     >
#define LXADDMemReg    SizeParameterizedOpCode<LXADD8MemReg    , LXADD4MemReg    >
#define ADCMemImm4     SizeParameterizedOpCode<ADC8MemImm4     , ADC4MemImm4     >
#define ADCMemReg      SizeParameterizedOpCode<ADC8MemReg      , ADC4MemReg      >
#define ADCRegReg      SizeParameterizedOpCode<ADC8RegReg      , ADC4RegReg      >
#define ADCRegMem      SizeParameterizedOpCode<ADC8RegMem      , ADC4RegMem      >
#define SUBMemImm4     SizeParameterizedOpCode<SUB8MemImm4     , SUB4MemImm4     >
#define SUBRegImm4     SizeParameterizedOpCode<SUB8RegImm4     , SUB4RegImm4     >
#define SUBMemReg      SizeParameterizedOpCode<SUB8MemReg      , SUB4MemReg      >
#define SUBRegReg      SizeParameterizedOpCode<SUB8RegReg      , SUB4RegReg      >
#define SUBRegMem      SizeParameterizedOpCode<SUB8RegMem      , SUB4RegMem      >
#define SBBMemImm4     SizeParameterizedOpCode<SBB8MemImm4     , SBB4MemImm4     >
#define SBBRegImm4     SizeParameterizedOpCode<SBB8RegImm4     , SBB4RegImm4     >
#define SBBMemReg      SizeParameterizedOpCode<SBB8MemReg      , SBB4MemReg      >
#define SBBRegReg      SizeParameterizedOpCode<SBB8RegReg      , SBB4RegReg      >
#define SBBRegMem      SizeParameterizedOpCode<SBB8RegMem      , SBB4RegMem      >
#define ORRegImm4      SizeParameterizedOpCode<OR8RegImm4      , OR4RegImm4      >
#define ORRegImms      SizeParameterizedOpCode<OR8RegImms      , OR4RegImms      >
#define ORRegMem       SizeParameterizedOpCode<OR8RegMem       , OR4RegMem       >
#define XORRegMem      SizeParameterizedOpCode<XOR8RegMem      , XOR4RegMem      >
#define XORRegImms     SizeParameterizedOpCode<XOR8RegImms     , XOR4RegImms     >
#define XORRegImm4     SizeParameterizedOpCode<XOR8RegImm4     , XOR4RegImm4     >
#define CXXAcc         SizeParameterizedOpCode<CQOAcc          , CDQAcc          >
#define ANDRegImm4     SizeParameterizedOpCode<AND8RegImm4     , AND4RegImm4     >
#define ANDRegReg      SizeParameterizedOpCode<AND8RegReg      , AND4RegReg      >
#define ANDRegImms     SizeParameterizedOpCode<AND8RegImms     , AND4RegImms     >
#define ORRegReg       SizeParameterizedOpCode<OR8RegReg       , OR4RegReg       >
#define MOVRegImm4     SizeParameterizedOpCode<MOV8RegImm4     , MOV4RegImm4     >
#define IMULAccReg     SizeParameterizedOpCode<IMUL8AccReg     , IMUL4AccReg     >
#define IDIVAccMem     SizeParameterizedOpCode<IDIV8AccMem     , IDIV4AccMem     >
#define IDIVAccReg     SizeParameterizedOpCode<IDIV8AccReg     , IDIV4AccReg     >
#define DIVAccMem      SizeParameterizedOpCode<DIV8AccMem      , DIV4AccMem      >
#define DIVAccReg      SizeParameterizedOpCode<DIV8AccReg      , DIV4AccReg      >
#define NOTReg         SizeParameterizedOpCode<NOT8Reg         , NOT4Reg         >
#define POPCNTRegReg   SizeParameterizedOpCode<POPCNT8RegReg   , POPCNT4RegReg   >
#define ROLRegImm1     SizeParameterizedOpCode<ROL8RegImm1     , ROL4RegImm1     >
#define ROLRegCL       SizeParameterizedOpCode<ROL8RegCL       , ROL4RegCL       >
#define SHLMemImm1     SizeParameterizedOpCode<SHL8MemImm1     , SHL4MemImm1     >
#define SHLMemCL       SizeParameterizedOpCode<SHL8MemCL       , SHL4MemCL       >
#define SHLRegImm1     SizeParameterizedOpCode<SHL8RegImm1     , SHL4RegImm1     >
#define SHLRegCL       SizeParameterizedOpCode<SHL8RegCL       , SHL4RegCL       >
#define SARMemImm1     SizeParameterizedOpCode<SAR8MemImm1     , SAR4MemImm1     >
#define SARMemCL       SizeParameterizedOpCode<SAR8MemCL       , SAR4MemCL       >
#define SARRegImm1     SizeParameterizedOpCode<SAR8RegImm1     , SAR4RegImm1     >
#define SARRegCL       SizeParameterizedOpCode<SAR8RegCL       , SAR4RegCL       >
#define SHRMemImm1     SizeParameterizedOpCode<SHR8MemImm1     , SHR4MemImm1     >
#define SHRMemCL       SizeParameterizedOpCode<SHR8MemCL       , SHR4MemCL       >
#define SHRReg1        SizeParameterizedOpCode<SHR8Reg1        , SHR4Reg1        >
#define SHRRegImm1     SizeParameterizedOpCode<SHR8RegImm1     , SHR4RegImm1     >
#define SHRRegCL       SizeParameterizedOpCode<SHR8RegCL       , SHR4RegCL       >
#define TESTMemImm4    SizeParameterizedOpCode<TEST8MemImm4    , TEST4MemImm4    >
#define TESTRegImm4    SizeParameterizedOpCode<TEST8RegImm4    , TEST4RegImm4    >
#define TESTRegReg     SizeParameterizedOpCode<TEST8RegReg     , TEST4RegReg     >
#define TESTMemReg     SizeParameterizedOpCode<TEST8MemReg     , TEST4MemReg     >
#define XORRegReg      SizeParameterizedOpCode<XOR8RegReg      , XOR4RegReg      >
#define CMPRegReg      SizeParameterizedOpCode<CMP8RegReg      , CMP4RegReg      >
#define CMPRegMem      SizeParameterizedOpCode<CMP8RegMem      , CMP4RegMem      >
#define CMPMemReg      SizeParameterizedOpCode<CMP8MemReg      , CMP4MemReg      >
#define CMPXCHGMemReg  SizeParameterizedOpCode<CMPXCHG8MemReg  , CMPXCHG4MemReg  >
#define LCMPXCHGMemReg SizeParameterizedOpCode<LCMPXCHG8MemReg , LCMPXCHG4MemReg >
#define REPSTOS        SizeParameterizedOpCode<REPSTOSQ        , REPSTOSD        >
// Floating-point
#define MOVSMemReg     SizeParameterizedOpCode<MOVSDMemReg     , MOVSSMemReg     >
#define MOVSRegMem     SizeParameterizedOpCode<MOVSDRegMem     , MOVSSRegMem     >

// Size and carry-parameterized opcodes
//
inline TR_X86OpCodes AddMemImms (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCMemImms(is64Bit) : ADDMemImms(is64Bit) ; }
inline TR_X86OpCodes AddRegImms (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCRegImms(is64Bit) : ADDRegImms(is64Bit) ; }
inline TR_X86OpCodes AddRegImm4 (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCRegImm4(is64Bit) : ADDRegImm4(is64Bit) ; }
inline TR_X86OpCodes AddMemImm4 (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCMemImm4(is64Bit) : ADDMemImm4(is64Bit) ; }
inline TR_X86OpCodes AddMemReg  (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCMemReg(is64Bit)  : ADDMemReg(is64Bit)  ; }
inline TR_X86OpCodes AddRegReg  (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCRegReg(is64Bit)  : ADDRegReg(is64Bit)  ; }
inline TR_X86OpCodes AddRegMem  (bool is64Bit, bool isWithCarry)  { return isWithCarry   ? ADCRegMem(is64Bit)  : ADDRegMem(is64Bit)  ; }

inline TR_X86OpCodes SubMemImms (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBMemImms(is64Bit) : SUBMemImms(is64Bit) ; }
inline TR_X86OpCodes SubRegImms (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBRegImms(is64Bit) : SUBRegImms(is64Bit) ; }
inline TR_X86OpCodes SubRegImm4 (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBRegImm4(is64Bit) : SUBRegImm4(is64Bit) ; }
inline TR_X86OpCodes SubMemImm4 (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBMemImm4(is64Bit) : SUBMemImm4(is64Bit) ; }
inline TR_X86OpCodes SubMemReg  (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBMemReg(is64Bit)  : SUBMemReg(is64Bit)  ; }
inline TR_X86OpCodes SubRegReg  (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBRegReg(is64Bit)  : SUBRegReg(is64Bit)  ; }
inline TR_X86OpCodes SubRegMem  (bool is64Bit, bool isWithBorrow) { return isWithBorrow  ? SBBRegMem(is64Bit)  : SUBRegMem(is64Bit)  ; }

// Property flags
//
#define IA32OpProp_ModifiesTarget               0x00000001
#define IA32OpProp_ModifiesSource               0x00000002
#define IA32OpProp_UsesTarget                   0x00000004
#define IA32OpProp_SingleFP                     0x00000008
#define IA32OpProp_DoubleFP                     0x00000010
#define IA32OpProp_ByteImmediate                0x00000020
#define IA32OpProp_ShortImmediate               0x00000040
#define IA32OpProp_IntImmediate                 0x00000080
#define IA32OpProp_SignExtendImmediate          0x00000100
#define IA32OpProp_TestsZeroFlag                0x00000200
#define IA32OpProp_ModifiesZeroFlag             0x00000400
#define IA32OpProp_TestsSignFlag                0x00000800
#define IA32OpProp_ModifiesSignFlag             0x00001000
#define IA32OpProp_TestsCarryFlag               0x00002000
#define IA32OpProp_ModifiesCarryFlag            0x00004000
#define IA32OpProp_TestsOverflowFlag            0x00008000
#define IA32OpProp_ModifiesOverflowFlag         0x00010000
#define IA32OpProp_ByteSource                   0x00020000
#define IA32OpProp_ByteTarget                   0x00040000
#define IA32OpProp_ShortSource                  0x00080000
#define IA32OpProp_ShortTarget                  0x00100000
#define IA32OpProp_IntSource                    0x00200000
#define IA32OpProp_IntTarget                    0x00400000
#define IA32OpProp_TestsParityFlag              0x00800000
#define IA32OpProp_ModifiesParityFlag           0x01000000
#define IA32OpProp_TargetRegisterInOpcode       0x04000000
#define IA32OpProp_TargetRegisterInModRM        0x08000000
#define IA32OpProp_TargetRegisterIgnored        0x10000000
#define IA32OpProp_SourceRegisterInModRM        0x20000000
#define IA32OpProp_SourceRegisterIgnored        0x40000000
#define IA32OpProp_BranchOp                     0x80000000

// Since floating point instructions do not have immediate or byte forms,
// these flags can be overloaded.
//
#define IA32OpProp_HasPopInstruction            0x00000020
#define IA32OpProp_IsPopInstruction             0x00000040
#define IA32OpProp_SourceOpTarget               0x00000080
#define IA32OpProp_HasDirectionBit              0x00000100

#define IA32OpProp1_PushOp                    0x00000001
#define IA32OpProp1_PopOp                     0x00000002
#define IA32OpProp1_ShiftOp                   0x00000004
#define IA32OpProp1_RotateOp                  0x00000008
#define IA32OpProp1_SetsCCForCompare          0x00000010
#define IA32OpProp1_SetsCCForTest             0x00000020
#define IA32OpProp1_SupportsLockPrefix        0x00000040
#define IA32OpProp1_DoubleWordSource          0x00000100
#define IA32OpProp1_DoubleWordTarget          0x00000200
#define IA32OpProp1_XMMSource                 0x00000400
#define IA32OpProp1_XMMTarget                 0x00000800
#define IA32OpProp1_PseudoOp                  0x00001000
#define IA32OpProp1_NeedsRepPrefix            0x00002000
#define IA32OpProp1_NeedsLockPrefix           0x00004000
#define IA32OpProp1_CallOp                    0x00010000
#define IA32OpProp1_SourceIsMemRef            0x00020000
#define IA32OpProp1_SourceRegIsImplicit       0x00040000
#define IA32OpProp1_TargetRegIsImplicit       0x00080000
#define IA32OpProp1_FusableCompare            0x00100000
#define IA32OpProp1_NeedsXacquirePrefix       0x00400000
#define IA32OpProp1_NeedsXreleasePrefix       0x00800000
////////////////////
//
// AMD64 flags
//
#define IA32OpProp1_LongSource                0x80000000
#define IA32OpProp1_LongTarget                0x40000000
#define IA32OpProp1_LongImmediate             0x20000000 // MOV and DQ only

// For instructions not supported on AMD64
//
#define IA32OpProp1_IsIA32Only                0x08000000


// All testing properties, used to distinguish a conditional branch from an
// unconditional branch
//
#define IA32OpProp_TestsSomeFlag          (IA32OpProp_TestsZeroFlag     | \
                                           IA32OpProp_TestsSignFlag     | \
                                           IA32OpProp_TestsCarryFlag    | \
                                           IA32OpProp_TestsOverflowFlag | \
                                           IA32OpProp_TestsParityFlag)

#define IA32OpProp_SetsSomeArithmeticFlag (IA32OpProp_ModifiesZeroFlag     | \
                                           IA32OpProp_ModifiesSignFlag     | \
                                           IA32OpProp_ModifiesCarryFlag    | \
                                           IA32OpProp_ModifiesOverflowFlag)


typedef enum
   {
   IA32EFlags_OF = 0x01,
   IA32EFlags_SF = 0x02,
   IA32EFlags_ZF = 0x04,
   IA32EFlags_PF = 0x08,
   IA32EFlags_CF = 0x10
   } TR_EFlags;


class TR_X86OpCode
   {
   enum TR_OpCodeVEX_L : uint8_t
      {
      VEX_L128 = 0x0,
      VEX_L256 = 0x1,
      VEX_L512 = 0x2,
      VEX_L___ = 0x3, // Instruction does not support VEX encoding
      };
   enum TR_OpCodeVEX_v : uint8_t
      {
      VEX_vNONE = 0x0,
      VEX_vReg_ = 0x1,
      };
   enum TR_InstructionREX_W : uint8_t
      {
      REX__ = 0x0,
      REX_W = 0x1,
      };
   enum TR_OpCodePrefix : uint8_t
      {
      PREFIX___ = 0x0,
      PREFIX_66 = 0x1,
      PREFIX_F3 = 0x2,
      PREFIX_F2 = 0x3,
      };
   enum TR_OpCodeEscape : uint8_t
      {
      ESCAPE_____ = 0x0,
      ESCAPE_0F__ = 0x1,
      ESCAPE_0F38 = 0x2,
      ESCAPE_0F3A = 0x3,
      };
   enum TR_OpCodeModRM : uint8_t
      {
      ModRM_NONE = 0x0,
      ModRM_RM__ = 0x1,
      ModRM_MR__ = 0x2,
      ModRM_EXT_ = 0x3,
      };
   enum TR_OpCodeImmediate : uint8_t
      {
      Immediate_0 = 0x0,
      Immediate_1 = 0x1,
      Immediate_2 = 0x2,
      Immediate_4 = 0x3,
      Immediate_8 = 0x4,
      Immediate_S = 0x7,
      };
   struct OpCode_t
      {
      uint8_t vex_l : 2;
      uint8_t vex_v : 1;
      uint8_t prefixes : 2;
      uint8_t rex_w : 1;
      uint8_t escape : 2;
      uint8_t opcode;
      uint8_t modrm_opcode : 3;
      uint8_t modrm_form : 2;
      uint8_t immediate_size : 3;
      inline uint8_t ImmediateSize() const
         {
         switch (immediate_size)
            {
            case Immediate_0:
               return 0;
            case Immediate_S:
            case Immediate_1:
               return 1;
            case Immediate_2:
               return 2;
            case Immediate_4:
               return 4;
            case Immediate_8:
               return 8;
            }
            TR_ASSERT_FATAL(false, "IMPOSSIBLE TO REACH HERE.");
         }
      // check if the instruction can be encoded as AVX
      inline bool supportsAVX() const
         {
         return vex_l != VEX_L___;
         }
      // check if the instruction is X87
      inline bool isX87() const
         {
         return (prefixes == PREFIX___) && (opcode >= 0xd8) && (opcode <= 0xdf);
         }
      // check if the instruction has mandatory prefix(es)
      inline bool hasMandatoryPrefix() const
         {
         return prefixes == PREFIX___;
         }
      // check if the instruction is part of Group 7 OpCode Extension
      inline bool isGroup07() const
         {
         return (escape == ESCAPE_0F__) && (opcode == 0x01);
         }
      // TBuffer should only be one of the two: Estimator when calculating length, and Writer when generating binaries.
      template <class TBuffer> inline typename TBuffer::cursor_t encode(typename TBuffer::cursor_t cursor, uint8_t rexbits) const;
      // finalize instruction prefix information, currently only in-use for AVX instructions for VEX.vvvv field
      inline void finalize(uint8_t* cursor) const;
      inline bool allowsAVX() const;
      };
   template <typename TCursor>
   class BufferBase
      {
      public:
      typedef TCursor cursor_t;
      inline operator cursor_t() const
         {
         return cursor;
         }
      protected:
      inline BufferBase(cursor_t cursor) : cursor(cursor) {}
      cursor_t cursor;
      };
   // helper class to calculate length
   class Estimator : public BufferBase<uint8_t>
      {
      public:
      inline Estimator(cursor_t size) : BufferBase<cursor_t>(size) {}
      template <typename T> void inline append(T binaries)
         {
         cursor += sizeof(T);
         }
      };
   // helper class to write binaries
   class Writer : public BufferBase<uint8_t*>
      {
      public:
      inline Writer(cursor_t cursor) : BufferBase<cursor_t>(cursor) {}
      template <typename T> void inline append(T binaries)
         {
         *((T*)cursor) = binaries;
         cursor+= sizeof(T);
         }
      };
   // TBuffer should only be one of the two: Estimator when calculating length, and Writer when generating binaries.
   template <class TBuffer> inline typename TBuffer::cursor_t encode(typename TBuffer::cursor_t cursor, uint8_t rex) const
      {
      return isPseudoOp() ? cursor : info().encode<TBuffer>(cursor, rex);
      }
   // Instructions from Group 7 OpCode Extensions need special handling as they requires specific low 3 bits of ModR/M byte
   inline void CheckAndFinishGroup07(uint8_t* cursor) const;

   TR_X86OpCodes         _opCode;
   static const OpCode_t _binaries[];
   static const uint32_t _properties[];
   static const uint32_t _properties1[];

   public:

   inline TR_X86OpCode()                 : _opCode(BADIA32Op) {}
   inline TR_X86OpCode(TR_X86OpCodes op) : _opCode(op)        {}

   inline const OpCode_t& info() const                   {return _binaries[_opCode]; }
   inline TR_X86OpCodes getOpCodeValue() const           {return _opCode;}
   inline TR_X86OpCodes setOpCodeValue(TR_X86OpCodes op) {return (_opCode = op);}

   inline uint32_t modifiesTarget()                const {return _properties[_opCode] & IA32OpProp_ModifiesTarget;}
   inline uint32_t modifiesSource()                const {return _properties[_opCode] & IA32OpProp_ModifiesSource;}
   inline uint32_t usesTarget()                    const {return _properties[_opCode] & IA32OpProp_UsesTarget;}
   inline uint32_t singleFPOp()                    const {return _properties[_opCode] & IA32OpProp_SingleFP;}
   inline uint32_t doubleFPOp()                    const {return _properties[_opCode] & IA32OpProp_DoubleFP;}
   inline uint32_t gprOp()                         const {return (_properties[_opCode] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP)) == 0;}
   inline uint32_t fprOp()                         const {return (_properties[_opCode] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP));}
   inline uint32_t hasByteImmediate()              const {return _properties[_opCode] & IA32OpProp_ByteImmediate;}
   inline uint32_t hasShortImmediate()             const {return _properties[_opCode] & IA32OpProp_ShortImmediate;}
   inline uint32_t hasIntImmediate()               const {return _properties[_opCode] & IA32OpProp_IntImmediate;}
   inline uint32_t hasLongImmediate()              const {return _properties1[_opCode] & IA32OpProp1_LongImmediate;}
   inline uint32_t hasSignExtendImmediate()        const {return _properties[_opCode] & IA32OpProp_SignExtendImmediate;}
   inline uint32_t testsZeroFlag()                 const {return _properties[_opCode] & IA32OpProp_TestsZeroFlag;}
   inline uint32_t modifiesZeroFlag()              const {return _properties[_opCode] & IA32OpProp_ModifiesZeroFlag;}
   inline uint32_t testsSignFlag()                 const {return _properties[_opCode] & IA32OpProp_TestsSignFlag;}
   inline uint32_t modifiesSignFlag()              const {return _properties[_opCode] & IA32OpProp_ModifiesSignFlag;}
   inline uint32_t testsCarryFlag()                const {return _properties[_opCode] & IA32OpProp_TestsCarryFlag;}
   inline uint32_t modifiesCarryFlag()             const {return _properties[_opCode] & IA32OpProp_ModifiesCarryFlag;}
   inline uint32_t testsOverflowFlag()             const {return _properties[_opCode] & IA32OpProp_TestsOverflowFlag;}
   inline uint32_t modifiesOverflowFlag()          const {return _properties[_opCode] & IA32OpProp_ModifiesOverflowFlag;}
   inline uint32_t testsParityFlag()               const {return _properties[_opCode] & IA32OpProp_TestsParityFlag;}
   inline uint32_t modifiesParityFlag()            const {return _properties[_opCode] & IA32OpProp_ModifiesParityFlag;}
   inline uint32_t hasByteSource()                 const {return _properties[_opCode] & IA32OpProp_ByteSource;}
   inline uint32_t hasByteTarget()                 const {return _properties[_opCode] & IA32OpProp_ByteTarget;}
   inline uint32_t hasShortSource()                const {return _properties[_opCode] & IA32OpProp_ShortSource;}
   inline uint32_t hasShortTarget()                const {return _properties[_opCode] & IA32OpProp_ShortTarget;}
   inline uint32_t hasIntSource()                  const {return _properties[_opCode] & IA32OpProp_IntSource;}
   inline uint32_t hasIntTarget()                  const {return _properties[_opCode] & IA32OpProp_IntTarget;}
   inline uint32_t hasLongSource()                 const {return _properties1[_opCode] & IA32OpProp1_LongSource;}
   inline uint32_t hasLongTarget()                 const {return _properties1[_opCode] & IA32OpProp1_LongTarget;}
   inline uint32_t hasDoubleWordSource()           const {return _properties1[_opCode] & IA32OpProp1_DoubleWordSource;}
   inline uint32_t hasDoubleWordTarget()           const {return _properties1[_opCode] & IA32OpProp1_DoubleWordTarget;}
   inline uint32_t hasXMMSource()                  const {return _properties1[_opCode] & IA32OpProp1_XMMSource;}
   inline uint32_t hasXMMTarget()                  const {return _properties1[_opCode] & IA32OpProp1_XMMTarget;}
   inline uint32_t isPseudoOp()                    const {return _properties1[_opCode] & IA32OpProp1_PseudoOp;}
   inline uint32_t needsRepPrefix()                const {return _properties1[_opCode] & IA32OpProp1_NeedsRepPrefix;}
   inline uint32_t needsLockPrefix()               const {return _properties1[_opCode] & IA32OpProp1_NeedsLockPrefix;}
   inline uint32_t needsXacquirePrefix()           const {return _properties1[_opCode] & IA32OpProp1_NeedsXacquirePrefix;}
   inline uint32_t needsXreleasePrefix()           const {return _properties1[_opCode] & IA32OpProp1_NeedsXreleasePrefix;}
   inline uint32_t clearsUpperBits()               const {return hasIntTarget() && modifiesTarget();}
   inline uint32_t setsUpperBits()                 const {return hasLongTarget() && modifiesTarget();}
   inline uint32_t hasTargetRegisterInOpcode()     const {return _properties[_opCode] & IA32OpProp_TargetRegisterInOpcode;}
   inline uint32_t hasTargetRegisterInModRM()      const {return _properties[_opCode] & IA32OpProp_TargetRegisterInModRM;}
   inline uint32_t hasTargetRegisterIgnored()      const {return _properties[_opCode] & IA32OpProp_TargetRegisterIgnored;}
   inline uint32_t hasSourceRegisterInModRM()      const {return _properties[_opCode] & IA32OpProp_SourceRegisterInModRM;}
   inline uint32_t hasSourceRegisterIgnored()      const {return _properties[_opCode] & IA32OpProp_SourceRegisterIgnored;}
   inline uint32_t isBranchOp()                    const {return _properties[_opCode] & IA32OpProp_BranchOp;}
   inline uint32_t hasRelativeBranchDisplacement() const { return isBranchOp() || isCallImmOp(); }
   inline uint32_t isShortBranchOp()               const {return isBranchOp() && hasByteImmediate();}
   inline uint32_t testsSomeFlag()                 const {return _properties[_opCode] & IA32OpProp_TestsSomeFlag;}
   inline uint32_t modifiesSomeArithmeticFlags()   const {return _properties[_opCode] & IA32OpProp_SetsSomeArithmeticFlag;}
   inline uint32_t setsCCForCompare()              const {return _properties1[_opCode] & IA32OpProp1_SetsCCForCompare;}
   inline uint32_t setsCCForTest()                 const {return _properties1[_opCode] & IA32OpProp1_SetsCCForTest;}
   inline uint32_t isShiftOp()                     const {return _properties1[_opCode] & IA32OpProp1_ShiftOp;}
   inline uint32_t isRotateOp()                    const {return _properties1[_opCode] & IA32OpProp1_RotateOp;}
   inline uint32_t isPushOp()                      const {return _properties1[_opCode] & IA32OpProp1_PushOp;}
   inline uint32_t isPopOp()                       const {return _properties1[_opCode] & IA32OpProp1_PopOp;}
   inline uint32_t isCallOp()                      const {return isCallOp(_opCode); }
   inline uint32_t isCallImmOp()                   const {return isCallImmOp(_opCode); }
   inline uint32_t supportsLockPrefix()            const {return _properties1[_opCode] & IA32OpProp1_SupportsLockPrefix;}
   inline bool     isConditionalBranchOp()         const {return isBranchOp() && testsSomeFlag();}
   inline uint32_t hasPopInstruction()             const {return _properties[_opCode] & IA32OpProp_HasPopInstruction;}
   inline uint32_t isPopInstruction()              const {return _properties[_opCode] & IA32OpProp_IsPopInstruction;}
   inline uint32_t sourceOpTarget()                const {return _properties[_opCode] & IA32OpProp_SourceOpTarget;}
   inline uint32_t hasDirectionBit()               const {return _properties[_opCode] & IA32OpProp_HasDirectionBit;}
   inline uint32_t isIA32Only()                    const {return _properties1[_opCode] & IA32OpProp1_IsIA32Only;}
   inline uint32_t sourceIsMemRef()                const {return _properties1[_opCode] & IA32OpProp1_SourceIsMemRef;}
   inline uint32_t targetRegIsImplicit()           const { return _properties1[_opCode] & IA32OpProp1_TargetRegIsImplicit;}
   inline uint32_t sourceRegIsImplicit()           const { return _properties1[_opCode] & IA32OpProp1_SourceRegIsImplicit;}
   inline uint32_t isFusableCompare()              const { return _properties1[_opCode] & IA32OpProp1_FusableCompare; }

   inline bool isSetRegInstruction() const
      {
      switch(_opCode)
         {
         case SETA1Reg:
         case SETAE1Reg:
         case SETB1Reg:
         case SETBE1Reg:
         case SETE1Reg:
         case SETNE1Reg:
         case SETG1Reg:
         case SETGE1Reg:
         case SETL1Reg:
         case SETLE1Reg:
         case SETS1Reg:
         case SETNS1Reg:
         case SETPO1Reg:
         case SETPE1Reg:
            return true;
         default:
            return false;
         }
      }

   inline uint8_t length(uint8_t rex = 0) const;
   inline uint8_t* binary(uint8_t* cursor, uint8_t rex = 0) const;
   inline void finalize(uint8_t* cursor) const;
   void convertLongBranchToShort()
      { // input must be a long branch in range JA4 - JMP4
      if (((int)_opCode >= (int)JA4) && ((int)_opCode <= (int)JMP4))
         _opCode = (TR_X86OpCodes)((int)_opCode - IA32LongToShortBranchConversionOffset);
      }

   inline uint8_t getModifiedEFlags() const {return getModifiedEFlags(_opCode); }
   inline uint8_t getTestedEFlags()   const {return getTestedEFlags(_opCode); }

   void trackUpperBitsOnReg(TR::Register *reg, TR::CodeGenerator *cg);

   inline static uint32_t singleFPOp(TR_X86OpCodes op)           {return _properties[op] & IA32OpProp_SingleFP;}
   inline static uint32_t doubleFPOp(TR_X86OpCodes op)           {return _properties[op] & IA32OpProp_DoubleFP;}
   inline static uint32_t gprOp(TR_X86OpCodes op)                {return (_properties[op] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP)) == 0;}
   inline static uint32_t fprOp(TR_X86OpCodes op)                {return (_properties[op] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP));}
   inline static uint32_t testsZeroFlag(TR_X86OpCodes op)        {return _properties[op] & IA32OpProp_TestsZeroFlag;}
   inline static uint32_t modifiesZeroFlag(TR_X86OpCodes op)     {return _properties[op] & IA32OpProp_ModifiesZeroFlag;}
   inline static uint32_t testsSignFlag(TR_X86OpCodes op)        {return _properties[op] & IA32OpProp_TestsSignFlag;}
   inline static uint32_t modifiesSignFlag(TR_X86OpCodes op)     {return _properties[op] & IA32OpProp_ModifiesSignFlag;}
   inline static uint32_t testsCarryFlag(TR_X86OpCodes op)       {return _properties[op] & IA32OpProp_TestsCarryFlag;}
   inline static uint32_t modifiesCarryFlag(TR_X86OpCodes op)    {return _properties[op] & IA32OpProp_ModifiesCarryFlag;}
   inline static uint32_t testsOverflowFlag(TR_X86OpCodes op)    {return _properties[op] & IA32OpProp_TestsOverflowFlag;}
   inline static uint32_t modifiesOverflowFlag(TR_X86OpCodes op) {return _properties[op] & IA32OpProp_ModifiesOverflowFlag;}
   inline static uint32_t testsParityFlag(TR_X86OpCodes op)      {return _properties[op] & IA32OpProp_TestsParityFlag;}
   inline static uint32_t modifiesParityFlag(TR_X86OpCodes op)   {return _properties[op] & IA32OpProp_ModifiesParityFlag;}
   inline static uint32_t isCallOp(TR_X86OpCodes op)             {return _properties1[op] & IA32OpProp1_CallOp;}
   inline static uint32_t isCallImmOp(TR_X86OpCodes op)          {return op == CALLImm4 || op == CALLREXImm4; }

   inline static uint8_t getModifiedEFlags(TR_X86OpCodes op)
      {
      uint8_t flags = 0;
      if (modifiesOverflowFlag(op)) flags |= IA32EFlags_OF;
      if (modifiesSignFlag(op))     flags |= IA32EFlags_SF;
      if (modifiesZeroFlag(op))     flags |= IA32EFlags_ZF;
      if (modifiesParityFlag(op))   flags |= IA32EFlags_PF;
      if (modifiesCarryFlag(op))    flags |= IA32EFlags_CF;
      return flags;
      }

   inline static uint8_t getTestedEFlags(TR_X86OpCodes op)
      {
      uint8_t flags = 0;
      if (testsOverflowFlag(op)) flags |= IA32EFlags_OF;
      if (testsSignFlag(op))     flags |= IA32EFlags_SF;
      if (testsZeroFlag(op))     flags |= IA32EFlags_ZF;
      if (testsParityFlag(op))   flags |= IA32EFlags_PF;
      if (testsCarryFlag(op))    flags |= IA32EFlags_CF;
      return flags;
      }

#if defined(DEBUG)
   const char *getOpCodeName(TR::CodeGenerator *cg);
   const char *getMnemonicName(TR::CodeGenerator *cg);
#endif
   };

#endif
