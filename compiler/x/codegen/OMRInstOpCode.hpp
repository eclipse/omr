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

#ifndef OMR_X86_INSTOPCODE_INCL
#define OMR_X86_INSTOPCODE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTOPCODE_CONNECTOR
#define OMR_INSTOPCODE_CONNECTOR
namespace OMR { namespace X86 { class InstOpCode; } }
namespace OMR { typedef OMR::X86::InstOpCode InstOpCodeConnector; }
#else
#error OMR::X86::InstOpCode expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRInstOpCode.hpp"
#include "env/CPU.hpp"
#include "il/OMRDataTypes.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Register; }

#define IA32LongToShortBranchConversionOffset ((int)OMR::InstOpCode::JMP4 - (int)OMR::InstOpCode::JMP1)
#define IA32LengthOfShortBranch               2

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
#define IA32OpProp1_SIMDSingleSource          0x00000080
#define IA32OpProp1_DoubleWordSource          0x00000100
#define IA32OpProp1_DoubleWordTarget          0x00000200
#define IA32OpProp1_XMMSource                 0x00000400
#define IA32OpProp1_XMMTarget                 0x00000800
#define IA32OpProp1_PseudoOp                  0x00001000
#define IA32OpProp1_NeedsRepPrefix            0x00002000
#define IA32OpProp1_NeedsLockPrefix           0x00004000
#define IA32OpProp1_CallOp                    0x00010000
#define IA32OpProp1_SourceIsMemRef            0x00020000

// For cases when source operand can be a register or mem-ref
#define IA32OpProp1_SourceCanBeMemRef         0x00020000
#define IA32OpProp1_SourceRegIsImplicit       0x00040000
#define IA32OpProp1_TargetRegIsImplicit       0x00080000
#define IA32OpProp1_FusableCompare            0x00100000
#define IA32OpProp1_VectorIntMask             0x00200000
#define IA32OpProp1_ZMMTarget                 0x00400000
// Available                                  0x00800000
#define IA32OpProp1_YMMSource                 0x01000000
#define IA32OpProp1_YMMTarget                 0x02000000
#define IA32OpProp1_ZMMSource                 0x04000000
#define IA32OpProp1_VectorLongMask            0x10000000

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


// Flags for SIMD opcode encoding support, and required CPU feature flags.
// These flags will be added to SIMD opcodes and can be queried to verify
// vector operation support.

#define X86FeatureProp_Legacy                    0x00000001
#define X86FeatureProp_VEX128Supported           0x00000002
#define X86FeatureProp_VEX256Supported           0x00000004
#define X86FeatureProp_VEX_REQ_AVX512            0x00000008 // TODO: Support VEX instructions that require AVX-512
#define X86FeatureProp_EVEX128Supported          0x00000010
#define X86FeatureProp_EVEX256Supported          0x00000020
#define X86FeatureProp_EVEX512Supported          0x00000040
//      Reserved                                 0x00000080

#define X86FeatureProp_MinTargetSupported       (0x00000100 | X86FeatureProp_Legacy) // The opcode is supported by the OMR min target CPU
#define X86FeatureProp_SSE3Supported            (0x00000200 | X86FeatureProp_Legacy) // ISA supports instruction with SSE3
#define X86FeatureProp_SSE4Supported            (0x00000400 | X86FeatureProp_Legacy) // ISA supports instruction with SSE4
#define X86FeatureProp_SSE4_1Supported          (0x00000800 | X86FeatureProp_Legacy) // ISA supports instruction with SSE4.1
#define X86FeatureProp_LegacyMask               (0x00000F00 | X86FeatureProp_Legacy) // ISA supports instruction with legacy encoding

#define X86FeatureProp_AVX                       0x00001000
#define X86FeatureProp_V128_AVX2                 0x00002000
#define X86FeatureProp_V256_AVX2                 0x00004000
#define X86FeatureProp_FMA                       0x00008000
#define X86FeatureProp_AVX512F                   0x00010000
#define X86FeatureProp_AVX512VL                  0x00020000
#define X86FeatureProp_AVX512BW                  0x00040000
#define X86FeatureProp_AVX512DQ                  0x00080000
#define X86FeatureProp_AVX512CD                  0x00100000
#define X86FeatureProp_AVX512VBMI2               0x00200000
#define X86FeatureProp_AVX512BITALG              0x00400000
#define X86FeatureProp_AVX512VPOPCNTDQ           0x00800000

// VEX / EVEX forms (3-operands, except move instructions, etc.)
// Alias old flags with new flags to avoid rewriting the Opcodes file
#define X86FeatureProp_VEX128RequiresAVX              (X86FeatureProp_VEX128Supported  | X86FeatureProp_AVX            ) // VEX-128 encoded version requires AVX
#define X86FeatureProp_VEX128RequiresAVX2             (X86FeatureProp_VEX128Supported  | X86FeatureProp_V128_AVX2      ) // VEX-128 encoded version requires AVX
#define X86FeatureProp_VEX256RequiresAVX              (X86FeatureProp_VEX256Supported  | X86FeatureProp_AVX            ) // VEX-256 encoded version requires AVX
#define X86FeatureProp_VEX256RequiresAVX2             (X86FeatureProp_VEX256Supported  | X86FeatureProp_V256_AVX2      ) // VEX-256 encoded version requires AVX2
#define X86FeatureProp_EVEX128RequiresAVX512F         (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512F        ) // EVEX-128 encoded version requires AVX-512F
#define X86FeatureProp_EVEX128RequiresAVX512VL        (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512VL       ) // EVEX-128 encoded version requires AVX-512VL
#define X86FeatureProp_EVEX128RequiresAVX512BW        (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512BW       ) // EVEX-128 encoded version requires AVX-512BW
#define X86FeatureProp_EVEX128RequiresAVX512DQ        (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512DQ       ) // EVEX-128 encoded version requires AVX-512DQ
#define X86FeatureProp_EVEX128RequiresAVX512CD        (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512CD       ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX128RequiresAVX512VBMI2     (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512VBMI2    ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX128RequiresAVX512BITALG    (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512BITALG   ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX128RequiresAVX512VPOPCNTDQ (X86FeatureProp_EVEX128Supported | X86FeatureProp_AVX512VPOPCNTDQ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX256RequiresAVX512F         (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512F        ) // EVEX-256 encoded version requires AVX-512F
#define X86FeatureProp_EVEX256RequiresAVX512VL        (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512VL       ) // EVEX-256 encoded version requires AVX-512VL
#define X86FeatureProp_EVEX256RequiresAVX512BW        (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512BW       ) // EVEX-256 encoded version requires AVX-512BW
#define X86FeatureProp_EVEX256RequiresAVX512DQ        (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512DQ       ) // EVEX-256 encoded version requires AVX-512DQ
#define X86FeatureProp_EVEX256RequiresAVX512CD        (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512CD       ) // EVEX-256 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX256RequiresAVX512VBMI2     (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512VBMI2    ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX256RequiresAVX512BITALG    (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512BITALG   ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX256RequiresAVX512VPOPCNTDQ (X86FeatureProp_EVEX256Supported | X86FeatureProp_AVX512VPOPCNTDQ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX512RequiresAVX512F         (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512F        ) // EVEX-512 encoded version requires AVX-512F
#define X86FeatureProp_EVEX512RequiresAVX512BW        (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512BW       ) // EVEX-512 encoded version requires AVX-512BW
#define X86FeatureProp_EVEX512RequiresAVX512DQ        (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512DQ       ) // EVEX-512 encoded version requires AVX-512DQ
#define X86FeatureProp_EVEX512RequiresAVX512CD        (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512CD       ) // EVEX-512 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX512RequiresAVX512VBMI2     (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512VBMI2    ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX512RequiresAVX512BITALG    (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512BITALG   ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_EVEX512RequiresAVX512VPOPCNTDQ (X86FeatureProp_EVEX512Supported | X86FeatureProp_AVX512VPOPCNTDQ) // EVEX-128 encoded version requires AVX-512CD
#define X86FeatureProp_VEX128RequiresFMA              (X86FeatureProp_VEX128Supported  | X86FeatureProp_FMA            ) // VEX-128 encoded version requires AVX
#define X86FeatureProp_VEX256RequiresFMA              (X86FeatureProp_VEX256Supported  | X86FeatureProp_FMA            ) // VEX-128 encoded version requires AVX

typedef enum
   {
   IA32EFlags_OF = 0x01,
   IA32EFlags_SF = 0x02,
   IA32EFlags_ZF = 0x04,
   IA32EFlags_PF = 0x08,
   IA32EFlags_CF = 0x10
   } TR_EFlags;


namespace OMR
{

namespace X86
{

typedef enum
   {
   VEX_L128 = 0x0,
   VEX_L256 = 0x1,
   Default  = 0x2,
   Legacy   = 0x3,
   EVEX_L128 = 0x4,
   EVEX_L256 = 0x5,
   EVEX_L512 = 0x6,
   Bad       = 0x7
   } Encoding;

class InstOpCode: public OMR::InstOpCode
   {
   enum TR_OpCodeVEX_L : uint8_t
      {
      VEX_LZ   = 0x0,
      VEX_L128 = 0x0,
      VEX_L256 = 0x1,
      VEX_L512 = 0x2,
      VEX_L___ = 0x3, // Instruction does not support VEX encoding
      EVEX_L128 = 0x4,
      EVEX_L256 = 0x5,
      EVEX_L512 = 0x6,
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
      PREFIX_66_F2 = 0x4,
      PREFIX_66_F3 = 0x5,
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
      uint8_t vex_l : 3;
      uint8_t vex_v : 1;
      uint8_t prefixes : 3;
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
      inline bool isEvex() const
         {
         return vex_l >= EVEX_L128;
         }
      inline bool isEvex128() const
         {
           return vex_l == EVEX_L128;
         }
      inline bool isEvex256() const
         {
           return vex_l == EVEX_L256;
         }
      inline bool isEvex512() const
         {
           return vex_l == EVEX_L512;
         }
      // check if the instruction is X87
      inline bool isX87() const
         {
         return (prefixes == PREFIX___) && (opcode >= 0xd8) && (opcode <= 0xdf);
         }
      // check if the instruction has mandatory prefix(es)
      inline bool hasMandatoryPrefix() const
         {
         return prefixes != PREFIX___;
         }
      // check if the instruction is part of Group 7 OpCode Extension
      inline bool isGroup07() const
         {
         return (escape == ESCAPE_0F__) && (opcode == 0x01);
         }
      // TBuffer should only be one of the two: Estimator when calculating length, and Writer when generating binaries.
      template <class TBuffer> typename TBuffer::cursor_t encode(typename TBuffer::cursor_t cursor, OMR::X86::Encoding encoding, uint8_t rexbits) const;
      // finalize instruction prefix information, currently only in-use for AVX instructions for VEX.vvvv field
      void finalize(uint8_t* cursor) const;
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
   template <class TBuffer> inline typename TBuffer::cursor_t encode(typename TBuffer::cursor_t cursor, OMR::X86::Encoding encoding, uint8_t rex) const
      {
      return isPseudoOp() ? cursor : info().encode<TBuffer>(cursor, encoding, rex);
      }
   // Instructions from Group 7 OpCode Extensions need special handling as they requires specific low 3 bits of ModR/M byte
   inline void CheckAndFinishGroup07(uint8_t* cursor) const;

   static const OpCode_t _binaries[];
   static const uint32_t _properties[];
   static const uint32_t _properties1[];
   static const uint32_t _features[];

   protected:

   InstOpCode():  OMR::InstOpCode(OMR::InstOpCode::bad) {}
   InstOpCode(Mnemonic m): OMR::InstOpCode(m) {}

   public:

   struct OpCodeMetaData
      {
      };

   static const OpCodeMetaData metadata[NumOpCodes];

   inline const OpCode_t& info() const                   {return _binaries[_mnemonic]; }
   inline OMR::InstOpCode::Mnemonic getOpCodeValue() const           {return _mnemonic;}
   inline OMR::InstOpCode::Mnemonic setOpCodeValue(OMR::InstOpCode::Mnemonic op) {return (_mnemonic = op);}

   inline uint32_t modifiesTarget()                const {return _properties[_mnemonic] & IA32OpProp_ModifiesTarget;}
   inline uint32_t modifiesSource()                const {return _properties[_mnemonic] & IA32OpProp_ModifiesSource;}
   inline uint32_t usesTarget()                    const {return _properties[_mnemonic] & IA32OpProp_UsesTarget;}
   inline uint32_t singleFPOp()                    const {return _properties[_mnemonic] & IA32OpProp_SingleFP;}
   inline uint32_t doubleFPOp()                    const {return _properties[_mnemonic] & IA32OpProp_DoubleFP;}
   inline uint32_t gprOp()                         const {return (_properties[_mnemonic] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP)) == 0;}
   inline uint32_t fprOp()                         const {return (_properties[_mnemonic] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP));}
   inline uint32_t hasByteImmediate()              const {return _properties[_mnemonic] & IA32OpProp_ByteImmediate;}
   inline uint32_t hasShortImmediate()             const {return _properties[_mnemonic] & IA32OpProp_ShortImmediate;}
   inline uint32_t hasIntImmediate()               const {return _properties[_mnemonic] & IA32OpProp_IntImmediate;}
   inline uint32_t hasLongImmediate()              const {return _properties1[_mnemonic] & IA32OpProp1_LongImmediate;}
   inline uint32_t hasSignExtendImmediate()        const {return _properties[_mnemonic] & IA32OpProp_SignExtendImmediate;}
   inline uint32_t testsZeroFlag()                 const {return _properties[_mnemonic] & IA32OpProp_TestsZeroFlag;}
   inline uint32_t modifiesZeroFlag()              const {return _properties[_mnemonic] & IA32OpProp_ModifiesZeroFlag;}
   inline uint32_t testsSignFlag()                 const {return _properties[_mnemonic] & IA32OpProp_TestsSignFlag;}
   inline uint32_t modifiesSignFlag()              const {return _properties[_mnemonic] & IA32OpProp_ModifiesSignFlag;}
   inline uint32_t testsCarryFlag()                const {return _properties[_mnemonic] & IA32OpProp_TestsCarryFlag;}
   inline uint32_t modifiesCarryFlag()             const {return _properties[_mnemonic] & IA32OpProp_ModifiesCarryFlag;}
   inline uint32_t testsOverflowFlag()             const {return _properties[_mnemonic] & IA32OpProp_TestsOverflowFlag;}
   inline uint32_t modifiesOverflowFlag()          const {return _properties[_mnemonic] & IA32OpProp_ModifiesOverflowFlag;}
   inline uint32_t testsParityFlag()               const {return _properties[_mnemonic] & IA32OpProp_TestsParityFlag;}
   inline uint32_t modifiesParityFlag()            const {return _properties[_mnemonic] & IA32OpProp_ModifiesParityFlag;}
   inline uint32_t hasByteSource()                 const {return _properties[_mnemonic] & IA32OpProp_ByteSource;}
   inline uint32_t hasByteTarget()                 const {return _properties[_mnemonic] & IA32OpProp_ByteTarget;}
   inline uint32_t hasShortSource()                const {return _properties[_mnemonic] & IA32OpProp_ShortSource;}
   inline uint32_t hasShortTarget()                const {return _properties[_mnemonic] & IA32OpProp_ShortTarget;}
   inline uint32_t hasIntSource()                  const {return _properties[_mnemonic] & IA32OpProp_IntSource;}
   inline uint32_t hasIntTarget()                  const {return _properties[_mnemonic] & IA32OpProp_IntTarget;}
   inline uint32_t hasLongSource()                 const {return _properties1[_mnemonic] & IA32OpProp1_LongSource;}
   inline uint32_t hasLongTarget()                 const {return _properties1[_mnemonic] & IA32OpProp1_LongTarget;}
   inline uint32_t hasDoubleWordSource()           const {return _properties1[_mnemonic] & IA32OpProp1_DoubleWordSource;}
   inline uint32_t hasDoubleWordTarget()           const {return _properties1[_mnemonic] & IA32OpProp1_DoubleWordTarget;}
   inline uint32_t hasXMMSource()                  const {return _properties1[_mnemonic] & IA32OpProp1_XMMSource;}
   inline uint32_t isSingleSourceSIMDOperation()   const {return _properties1[_mnemonic] & IA32OpProp1_SIMDSingleSource;}
   inline uint32_t hasXMMTarget()                  const {return _properties1[_mnemonic] & IA32OpProp1_XMMTarget;}
   inline uint32_t hasYMMSource()                  const {return _properties1[_mnemonic] & IA32OpProp1_YMMSource;}
   inline uint32_t hasYMMTarget()                  const {return _properties1[_mnemonic] & IA32OpProp1_YMMTarget;}
   inline uint32_t hasZMMSource()                  const {return _properties1[_mnemonic] & IA32OpProp1_ZMMSource;}
   inline uint32_t hasZMMTarget()                  const {return _properties1[_mnemonic] & IA32OpProp1_ZMMTarget;}
   inline uint32_t isPseudoOp()                    const {return _properties1[_mnemonic] & IA32OpProp1_PseudoOp;}
   inline uint32_t needsRepPrefix()                const {return _properties1[_mnemonic] & IA32OpProp1_NeedsRepPrefix;}
   inline uint32_t needsLockPrefix()               const {return _properties1[_mnemonic] & IA32OpProp1_NeedsLockPrefix;}
   inline uint32_t clearsUpperBits()               const {return hasIntTarget() && modifiesTarget();}
   inline uint32_t setsUpperBits()                 const {return hasLongTarget() && modifiesTarget();}
   inline uint32_t hasTargetRegisterInOpcode()     const {return _properties[_mnemonic] & IA32OpProp_TargetRegisterInOpcode;}
   inline uint32_t hasTargetRegisterInModRM()      const {return _properties[_mnemonic] & IA32OpProp_TargetRegisterInModRM;}
   inline uint32_t hasTargetRegisterIgnored()      const {return _properties[_mnemonic] & IA32OpProp_TargetRegisterIgnored;}
   inline uint32_t hasSourceRegisterInModRM()      const {return _properties[_mnemonic] & IA32OpProp_SourceRegisterInModRM;}
   inline uint32_t hasSourceRegisterIgnored()      const {return _properties[_mnemonic] & IA32OpProp_SourceRegisterIgnored;}
   inline uint32_t isBranchOp()                    const {return _properties[_mnemonic] & IA32OpProp_BranchOp;}
   inline uint32_t hasRelativeBranchDisplacement() const { return isBranchOp() || isCallImmOp(); }
   inline uint32_t isShortBranchOp()               const {return isBranchOp() && hasByteImmediate();}
   inline uint32_t testsSomeFlag()                 const {return _properties[_mnemonic] & IA32OpProp_TestsSomeFlag;}
   inline uint32_t modifiesSomeArithmeticFlags()   const {return _properties[_mnemonic] & IA32OpProp_SetsSomeArithmeticFlag;}
   inline uint32_t setsCCForCompare()              const {return _properties1[_mnemonic] & IA32OpProp1_SetsCCForCompare;}
   inline uint32_t setsCCForTest()                 const {return _properties1[_mnemonic] & IA32OpProp1_SetsCCForTest;}
   inline uint32_t isShiftOp()                     const {return _properties1[_mnemonic] & IA32OpProp1_ShiftOp;}
   inline uint32_t isRotateOp()                    const {return _properties1[_mnemonic] & IA32OpProp1_RotateOp;}
   inline uint32_t isPushOp()                      const {return _properties1[_mnemonic] & IA32OpProp1_PushOp;}
   inline uint32_t isPopOp()                       const {return _properties1[_mnemonic] & IA32OpProp1_PopOp;}
   inline uint32_t isCallOp()                      const {return isCallOp(_mnemonic); }
   inline uint32_t isCallImmOp()                   const {return isCallImmOp(_mnemonic); }
   inline uint32_t supportsLockPrefix()            const {return _properties1[_mnemonic] & IA32OpProp1_SupportsLockPrefix;}
   inline bool     isConditionalBranchOp()         const {return isBranchOp() && testsSomeFlag();}
   inline uint32_t hasPopInstruction()             const {return _properties[_mnemonic] & IA32OpProp_HasPopInstruction;}
   inline uint32_t isPopInstruction()              const {return _properties[_mnemonic] & IA32OpProp_IsPopInstruction;}
   inline uint32_t sourceOpTarget()                const {return _properties[_mnemonic] & IA32OpProp_SourceOpTarget;}
   inline uint32_t hasDirectionBit()               const {return _properties[_mnemonic] & IA32OpProp_HasDirectionBit;}
   inline uint32_t isIA32Only()                    const {return _properties1[_mnemonic] & IA32OpProp1_IsIA32Only;}
   inline uint32_t sourceIsMemRef()                const {return _properties1[_mnemonic] & IA32OpProp1_SourceIsMemRef;}
   inline uint32_t targetRegIsImplicit()           const { return _properties1[_mnemonic] & IA32OpProp1_TargetRegIsImplicit;}
   inline uint32_t sourceRegIsImplicit()           const { return _properties1[_mnemonic] & IA32OpProp1_SourceRegIsImplicit;}
   inline uint32_t isFusableCompare()              const { return _properties1[_mnemonic] & IA32OpProp1_FusableCompare; }
   inline bool     isEvexInstruction()             const { return _binaries[_mnemonic].vex_l >> 2 == 1; }
   inline uint32_t getInstructionFeatureHints()    const { return _features[_mnemonic]; }

   OMR::X86::Encoding getSIMDEncoding(TR::CPU *target, TR::VectorLength vl)
      {
      uint32_t flags = getInstructionFeatureHints();
      TR_ASSERT_FATAL(flags, "Missing CPU feature flags for the instruction");

      bool supported;

      switch (vl)
         {
         case TR::VectorLength128:
            if (flags & X86FeatureProp_EVEX128Supported)
               {
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512F);

               if (supported && (flags & X86FeatureProp_AVX512VL))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512VL);
               if (supported && (flags & X86FeatureProp_AVX512BW))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512BW);
               if (supported && (flags & X86FeatureProp_AVX512DQ))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512DQ);
               if (supported && (flags & X86FeatureProp_AVX512CD))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512CD);
               if (supported && (flags & X86FeatureProp_AVX512VBMI2))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VBMI2);
               if (supported && (flags & X86FeatureProp_AVX512BITALG))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_BITALG);
               if (supported && (flags & X86FeatureProp_AVX512VPOPCNTDQ))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VPOPCNTDQ);

               if (supported)
                    return OMR::X86::EVEX_L128;
               }

            if (flags & X86FeatureProp_VEX128Supported)
               {
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX);

               if (supported && (flags & X86FeatureProp_V128_AVX2))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX2);

               if (supported && (flags & X86FeatureProp_FMA))
                  supported = target->supportsFeature(OMR_FEATURE_X86_FMA);

               if (supported)
                   return OMR::X86::VEX_L128;
               }

            if ((flags & X86FeatureProp_SSE4_1Supported) && target->supportsFeature(OMR_FEATURE_X86_SSE4_1))
               return OMR::X86::Legacy;

            if ((flags & X86FeatureProp_SSE3Supported) && target->supportsFeature(OMR_FEATURE_X86_SSE3))
               return OMR::X86::Legacy;

            if (flags & X86FeatureProp_MinTargetSupported)
               return OMR::X86::Legacy;

            break;
         case TR::VectorLength256:
            if (flags & X86FeatureProp_EVEX256Supported)
               {
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512F);

               if (supported && (flags & X86FeatureProp_AVX512VL))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512VL);
               if (supported && (flags & X86FeatureProp_AVX512BW))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512BW);
               if (supported && (flags & X86FeatureProp_AVX512DQ))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512DQ);
               if (supported && (flags & X86FeatureProp_AVX512CD))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512CD);
               if (supported && (flags & X86FeatureProp_AVX512VBMI2))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VBMI2);
               if (supported && (flags & X86FeatureProp_AVX512BITALG))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_BITALG);
               if (supported && (flags & X86FeatureProp_AVX512VPOPCNTDQ))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VPOPCNTDQ);

               if (supported)
                  return OMR::X86::EVEX_L256;
               }

            if (flags & X86FeatureProp_VEX256Supported)
               {
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX);

               if (supported && (flags & X86FeatureProp_V256_AVX2))
                  supported = target->supportsFeature(OMR_FEATURE_X86_AVX2);

               if (supported && (flags & X86FeatureProp_FMA))
                  supported = target->supportsFeature(OMR_FEATURE_X86_FMA);

               if (supported)
                   return OMR::X86::VEX_L256;
               }

               break;
         case TR::VectorLength512:
            supported = (flags & X86FeatureProp_EVEX512Supported) && target->supportsFeature(OMR_FEATURE_X86_AVX512F);

            if (supported && (flags & X86FeatureProp_AVX512BW))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512BW);
            if (supported && (flags & X86FeatureProp_AVX512DQ))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512DQ);
            if (supported && (flags & X86FeatureProp_AVX512DQ))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512DQ);
            if (supported && (flags & X86FeatureProp_AVX512CD))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512CD);
            if (supported && (flags & X86FeatureProp_AVX512VBMI2))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VBMI2);
            if (supported && (flags & X86FeatureProp_AVX512BITALG))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_BITALG);
            if (supported && (flags & X86FeatureProp_AVX512VPOPCNTDQ))
               supported = target->supportsFeature(OMR_FEATURE_X86_AVX512_VPOPCNTDQ);

            if (supported)
               return OMR::X86::EVEX_L512;

            break;
         default:
            break;
         }

      return OMR::X86::Encoding::Bad;
      }

   inline bool isSetRegInstruction() const
      {
      switch(_mnemonic)
         {
         case OMR::InstOpCode::SETA1Reg:
         case OMR::InstOpCode::SETAE1Reg:
         case OMR::InstOpCode::SETB1Reg:
         case OMR::InstOpCode::SETBE1Reg:
         case OMR::InstOpCode::SETE1Reg:
         case OMR::InstOpCode::SETNE1Reg:
         case OMR::InstOpCode::SETG1Reg:
         case OMR::InstOpCode::SETGE1Reg:
         case OMR::InstOpCode::SETL1Reg:
         case OMR::InstOpCode::SETLE1Reg:
         case OMR::InstOpCode::SETS1Reg:
         case OMR::InstOpCode::SETNS1Reg:
         case OMR::InstOpCode::SETPO1Reg:
         case OMR::InstOpCode::SETPE1Reg:
            return true;
         default:
            return false;
         }
      }

   uint8_t length(OMR::X86::Encoding encoding, uint8_t rex = 0) const;
   uint8_t* binary(uint8_t* cursor, OMR::X86::Encoding encoding, uint8_t rex = 0) const;
   void finalize(uint8_t* cursor) const;
   void convertLongBranchToShort()
      { // input must be a long branch in range OMR::InstOpCode::JA4 - OMR::InstOpCode::JMP4
      if (((int)_mnemonic >= (int)OMR::InstOpCode::JA4) && ((int)_mnemonic <= (int)OMR::InstOpCode::JMP4))
         _mnemonic = (OMR::InstOpCode::Mnemonic)((int)_mnemonic - IA32LongToShortBranchConversionOffset);
      }

   inline uint8_t getModifiedEFlags() const {return getModifiedEFlags(_mnemonic); }
   inline uint8_t getTestedEFlags()   const {return getTestedEFlags(_mnemonic); }

   void trackUpperBitsOnReg(TR::Register *reg, TR::CodeGenerator *cg);

   inline static uint32_t singleFPOp(OMR::InstOpCode::Mnemonic op)           {return _properties[op] & IA32OpProp_SingleFP;}
   inline static uint32_t doubleFPOp(OMR::InstOpCode::Mnemonic op)           {return _properties[op] & IA32OpProp_DoubleFP;}
   inline static uint32_t gprOp(OMR::InstOpCode::Mnemonic op)                {return (_properties[op] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP)) == 0;}
   inline static uint32_t fprOp(OMR::InstOpCode::Mnemonic op)                {return (_properties[op] & (IA32OpProp_DoubleFP | IA32OpProp_SingleFP));}
   inline static uint32_t testsZeroFlag(OMR::InstOpCode::Mnemonic op)        {return _properties[op] & IA32OpProp_TestsZeroFlag;}
   inline static uint32_t modifiesZeroFlag(OMR::InstOpCode::Mnemonic op)     {return _properties[op] & IA32OpProp_ModifiesZeroFlag;}
   inline static uint32_t testsSignFlag(OMR::InstOpCode::Mnemonic op)        {return _properties[op] & IA32OpProp_TestsSignFlag;}
   inline static uint32_t modifiesSignFlag(OMR::InstOpCode::Mnemonic op)     {return _properties[op] & IA32OpProp_ModifiesSignFlag;}
   inline static uint32_t testsCarryFlag(OMR::InstOpCode::Mnemonic op)       {return _properties[op] & IA32OpProp_TestsCarryFlag;}
   inline static uint32_t modifiesCarryFlag(OMR::InstOpCode::Mnemonic op)    {return _properties[op] & IA32OpProp_ModifiesCarryFlag;}
   inline static uint32_t testsOverflowFlag(OMR::InstOpCode::Mnemonic op)    {return _properties[op] & IA32OpProp_TestsOverflowFlag;}
   inline static uint32_t modifiesOverflowFlag(OMR::InstOpCode::Mnemonic op) {return _properties[op] & IA32OpProp_ModifiesOverflowFlag;}
   inline static uint32_t testsParityFlag(OMR::InstOpCode::Mnemonic op)      {return _properties[op] & IA32OpProp_TestsParityFlag;}
   inline static uint32_t modifiesParityFlag(OMR::InstOpCode::Mnemonic op)   {return _properties[op] & IA32OpProp_ModifiesParityFlag;}
   inline static uint32_t isCallOp(OMR::InstOpCode::Mnemonic op)             {return _properties1[op] & IA32OpProp1_CallOp;}
   inline static uint32_t isCallImmOp(OMR::InstOpCode::Mnemonic op)          {return op == OMR::InstOpCode::CALLImm4 || op == OMR::InstOpCode::CALLREXImm4; }

   inline static uint8_t getModifiedEFlags(OMR::InstOpCode::Mnemonic op)
      {
      uint8_t flags = 0;
      if (modifiesOverflowFlag(op)) flags |= IA32EFlags_OF;
      if (modifiesSignFlag(op))     flags |= IA32EFlags_SF;
      if (modifiesZeroFlag(op))     flags |= IA32EFlags_ZF;
      if (modifiesParityFlag(op))   flags |= IA32EFlags_PF;
      if (modifiesCarryFlag(op))    flags |= IA32EFlags_CF;
      return flags;
      }

   inline static uint8_t getTestedEFlags(OMR::InstOpCode::Mnemonic op)
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

#ifdef TR_TARGET_64BIT
#define TARGET_PARAMETERIZED_OPCODE(name, op64, op32) \
      static inline OMR::InstOpCode::Mnemonic name(bool is64Bit = true) { return is64Bit ? op64 : op32; }
#else
#define TARGET_PARAMETERIZED_OPCODE(name, op64, op32) \
      static inline OMR::InstOpCode::Mnemonic name(bool is64Bit = false) { return is64Bit ? op64 : op32; }
#endif

      TARGET_PARAMETERIZED_OPCODE(BSFRegReg      , BSF8RegReg      , BSF4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(BSWAPReg       , BSWAP8Reg       , BSWAP4Reg       )
      TARGET_PARAMETERIZED_OPCODE(BSRRegReg      , BSR8RegReg      , BSR4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(BTRegReg       , BT8RegReg       , BT4RegReg       )
      TARGET_PARAMETERIZED_OPCODE(CMOVBERegReg   , CMOVBE8RegReg   , CMOVBE4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(CMOVARegReg    , CMOVA8RegReg    , CMOVA4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMOVAERegReg   , CMOVAE8RegReg   , CMOVAE4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(CMOVBRegReg    , CMOVB8RegReg    , CMOVB4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMOVARegMem    , CMOVA8RegMem    , CMOVA4RegMem    )
      TARGET_PARAMETERIZED_OPCODE(CMOVERegMem    , CMOVE8RegMem    , CMOVE4RegMem    )
      TARGET_PARAMETERIZED_OPCODE(CMOVERegReg    , CMOVE8RegReg    , CMOVE4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMOVNERegMem   , CMOVNE8RegMem   , CMOVNE4RegMem   )
      TARGET_PARAMETERIZED_OPCODE(CMOVNERegReg   , CMOVNE8RegReg   , CMOVNE4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(CMOVGERegMem   , CMOVGE8RegMem   , CMOVGE4RegMem   )
      TARGET_PARAMETERIZED_OPCODE(CMOVGRegReg    , CMOVG8RegReg    , CMOVG4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMOVGERegReg   , CMOVGE8RegReg   , CMOVGE4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(CMOVLRegReg    , CMOVL8RegReg    , CMOVL4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMOVLERegReg   , CMOVLE8RegReg   , CMOVLE4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(CMOVPRegMem    , CMOVP8RegMem    , CMOVP4RegMem    )
      TARGET_PARAMETERIZED_OPCODE(CMOVSRegReg    , CMOVS8RegReg    , CMOVS4RegReg    )
      TARGET_PARAMETERIZED_OPCODE(CMPRegImms     , CMP8RegImms     , CMP4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(CMPRegImm4     , CMP8RegImm4     , CMP4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(CMPMemImms     , CMP8MemImms     , CMP4MemImms     )
      TARGET_PARAMETERIZED_OPCODE(CMPMemImm4     , CMP8MemImm4     , CMP4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(MOVRegReg      , MOV8RegReg      , MOV4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(LEARegMem      , LEA8RegMem      , LEA4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(LRegMem        , L8RegMem        , L4RegMem        )
      TARGET_PARAMETERIZED_OPCODE(SMemReg        , S8MemReg        , S4MemReg        )
      TARGET_PARAMETERIZED_OPCODE(SMemImm4       , S8MemImm4       , S4MemImm4       )
      TARGET_PARAMETERIZED_OPCODE(XRSMemImm4     , XRS8MemImm4     , XRS4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(XCHGRegReg     , XCHG8RegReg     , XCHG4RegReg     )
      TARGET_PARAMETERIZED_OPCODE(NEGReg         , NEG8Reg         , NEG4Reg         )
      TARGET_PARAMETERIZED_OPCODE(IMULRegReg     , IMUL8RegReg     , IMUL4RegReg     )
      TARGET_PARAMETERIZED_OPCODE(IMULRegMem     , IMUL8RegMem     , IMUL4RegMem     )
      TARGET_PARAMETERIZED_OPCODE(IMULRegRegImms , IMUL8RegRegImms , IMUL4RegRegImms )
      TARGET_PARAMETERIZED_OPCODE(IMULRegRegImm4 , IMUL8RegRegImm4 , IMUL4RegRegImm4 )
      TARGET_PARAMETERIZED_OPCODE(IMULRegMemImms , IMUL8RegMemImms , IMUL4RegMemImms )
      TARGET_PARAMETERIZED_OPCODE(IMULRegMemImm4 , IMUL8RegMemImm4 , IMUL4RegMemImm4 )
      TARGET_PARAMETERIZED_OPCODE(INCMem         , INC8Mem         , INC4Mem         )
      TARGET_PARAMETERIZED_OPCODE(INCReg         , INC8Reg         , INC4Reg         )
      TARGET_PARAMETERIZED_OPCODE(DECMem         , DEC8Mem         , DEC4Mem         )
      TARGET_PARAMETERIZED_OPCODE(DECReg         , DEC8Reg         , DEC4Reg         )
      TARGET_PARAMETERIZED_OPCODE(ADDMemImms     , ADD8MemImms     , ADD4MemImms     )
      TARGET_PARAMETERIZED_OPCODE(ADDRegImms     , ADD8RegImms     , ADD4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(ADDRegImm4     , ADD8RegImm4     , ADD4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(ADCMemImms     , ADC8MemImms     , ADC4MemImms     )
      TARGET_PARAMETERIZED_OPCODE(ADCRegImms     , ADC8RegImms     , ADC4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(ADCRegImm4     , ADC8RegImm4     , ADC4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(SUBMemImms     , SUB8MemImms     , SUB4MemImms     )
      TARGET_PARAMETERIZED_OPCODE(SUBRegImms     , SUB8RegImms     , SUB4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(SBBMemImms     , SBB8MemImms     , SBB4MemImms     )
      TARGET_PARAMETERIZED_OPCODE(SBBRegImms     , SBB8RegImms     , SBB4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(ADDMemImm4     , ADD8MemImm4     , ADD4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(ADDMemReg      , ADD8MemReg      , ADD4MemReg      )
      TARGET_PARAMETERIZED_OPCODE(ADDRegReg      , ADD8RegReg      , ADD4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(ADDRegMem      , ADD8RegMem      , ADD4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(LADDMemReg     , LADD8MemReg     , LADD4MemReg     )
      TARGET_PARAMETERIZED_OPCODE(LXADDMemReg    , LXADD8MemReg    , LXADD4MemReg    )
      TARGET_PARAMETERIZED_OPCODE(ADCMemImm4     , ADC8MemImm4     , ADC4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(ADCMemReg      , ADC8MemReg      , ADC4MemReg      )
      TARGET_PARAMETERIZED_OPCODE(ADCRegReg      , ADC8RegReg      , ADC4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(ADCRegMem      , ADC8RegMem      , ADC4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(SUBMemImm4     , SUB8MemImm4     , SUB4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(SUBRegImm4     , SUB8RegImm4     , SUB4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(SUBMemReg      , SUB8MemReg      , SUB4MemReg      )
      TARGET_PARAMETERIZED_OPCODE(SUBRegReg      , SUB8RegReg      , SUB4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(SUBRegMem      , SUB8RegMem      , SUB4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(SBBMemImm4     , SBB8MemImm4     , SBB4MemImm4     )
      TARGET_PARAMETERIZED_OPCODE(SBBRegImm4     , SBB8RegImm4     , SBB4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(SBBMemReg      , SBB8MemReg      , SBB4MemReg      )
      TARGET_PARAMETERIZED_OPCODE(SBBRegReg      , SBB8RegReg      , SBB4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(SBBRegMem      , SBB8RegMem      , SBB4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(ORRegImm4      , OR8RegImm4      , OR4RegImm4      )
      TARGET_PARAMETERIZED_OPCODE(ORRegImms      , OR8RegImms      , OR4RegImms      )
      TARGET_PARAMETERIZED_OPCODE(ORRegMem       , OR8RegMem       , OR4RegMem       )
      TARGET_PARAMETERIZED_OPCODE(XORRegMem      , XOR8RegMem      , XOR4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(XORRegImms     , XOR8RegImms     , XOR4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(XORRegImm4     , XOR8RegImm4     , XOR4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(CXXAcc         , CQOAcc          , CDQAcc          )
      TARGET_PARAMETERIZED_OPCODE(ANDRegImm4     , AND8RegImm4     , AND4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(ANDRegReg      , AND8RegReg      , AND4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(ANDRegImms     , AND8RegImms     , AND4RegImms     )
      TARGET_PARAMETERIZED_OPCODE(ORRegReg       , OR8RegReg       , OR4RegReg       )
      TARGET_PARAMETERIZED_OPCODE(MOVRegImm4     , MOV8RegImm4     , MOV4RegImm4     )
      TARGET_PARAMETERIZED_OPCODE(IMULAccReg     , IMUL8AccReg     , IMUL4AccReg     )
      TARGET_PARAMETERIZED_OPCODE(IDIVAccMem     , IDIV8AccMem     , IDIV4AccMem     )
      TARGET_PARAMETERIZED_OPCODE(IDIVAccReg     , IDIV8AccReg     , IDIV4AccReg     )
      TARGET_PARAMETERIZED_OPCODE(DIVAccMem      , DIV8AccMem      , DIV4AccMem      )
      TARGET_PARAMETERIZED_OPCODE(DIVAccReg      , DIV8AccReg      , DIV4AccReg      )
      TARGET_PARAMETERIZED_OPCODE(NOTReg         , NOT8Reg         , NOT4Reg         )
      TARGET_PARAMETERIZED_OPCODE(POPCNTRegReg   , POPCNT8RegReg   , POPCNT4RegReg   )
      TARGET_PARAMETERIZED_OPCODE(ROLRegImm1     , ROL8RegImm1     , ROL4RegImm1     )
      TARGET_PARAMETERIZED_OPCODE(ROLRegCL       , ROL8RegCL       , ROL4RegCL       )
      TARGET_PARAMETERIZED_OPCODE(SHLMemImm1     , SHL8MemImm1     , SHL4MemImm1     )
      TARGET_PARAMETERIZED_OPCODE(SHLMemCL       , SHL8MemCL       , SHL4MemCL       )
      TARGET_PARAMETERIZED_OPCODE(SHLRegImm1     , SHL8RegImm1     , SHL4RegImm1     )
      TARGET_PARAMETERIZED_OPCODE(SHLRegCL       , SHL8RegCL       , SHL4RegCL       )
      TARGET_PARAMETERIZED_OPCODE(SARMemImm1     , SAR8MemImm1     , SAR4MemImm1     )
      TARGET_PARAMETERIZED_OPCODE(SARMemCL       , SAR8MemCL       , SAR4MemCL       )
      TARGET_PARAMETERIZED_OPCODE(SARRegImm1     , SAR8RegImm1     , SAR4RegImm1     )
      TARGET_PARAMETERIZED_OPCODE(SARRegCL       , SAR8RegCL       , SAR4RegCL       )
      TARGET_PARAMETERIZED_OPCODE(SHRMemImm1     , SHR8MemImm1     , SHR4MemImm1     )
      TARGET_PARAMETERIZED_OPCODE(SHRMemCL       , SHR8MemCL       , SHR4MemCL       )
      TARGET_PARAMETERIZED_OPCODE(SHRReg1        , SHR8Reg1        , SHR4Reg1        )
      TARGET_PARAMETERIZED_OPCODE(SHRRegImm1     , SHR8RegImm1     , SHR4RegImm1     )
      TARGET_PARAMETERIZED_OPCODE(SHRRegCL       , SHR8RegCL       , SHR4RegCL       )
      TARGET_PARAMETERIZED_OPCODE(TESTMemImm4    , TEST8MemImm4    , TEST4MemImm4    )
      TARGET_PARAMETERIZED_OPCODE(TESTRegImm4    , TEST8RegImm4    , TEST4RegImm4    )
      TARGET_PARAMETERIZED_OPCODE(TESTRegReg     , TEST8RegReg     , TEST4RegReg     )
      TARGET_PARAMETERIZED_OPCODE(TESTMemReg     , TEST8MemReg     , TEST4MemReg     )
      TARGET_PARAMETERIZED_OPCODE(XORRegReg      , XOR8RegReg      , XOR4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(CMPRegReg      , CMP8RegReg      , CMP4RegReg      )
      TARGET_PARAMETERIZED_OPCODE(CMPRegMem      , CMP8RegMem      , CMP4RegMem      )
      TARGET_PARAMETERIZED_OPCODE(CMPMemReg      , CMP8MemReg      , CMP4MemReg      )
      TARGET_PARAMETERIZED_OPCODE(CMPXCHGMemReg  , CMPXCHG8MemReg  , CMPXCHG4MemReg  )
      TARGET_PARAMETERIZED_OPCODE(LCMPXCHGMemReg , LCMPXCHG8MemReg , LCMPXCHG4MemReg )
      TARGET_PARAMETERIZED_OPCODE(REPSTOS        , REPSTOSQ        , REPSTOSD        )
      // Floating-point
      TARGET_PARAMETERIZED_OPCODE(MOVSMemReg     , MOVSDMemReg     , MOVSSMemReg     )
      TARGET_PARAMETERIZED_OPCODE(MOVSRegMem     , MOVSDRegMem     , MOVSSRegMem     )
      // FMA
      TARGET_PARAMETERIZED_OPCODE(VFMADD231SRegRegReg, VFMADD231SDRegRegReg, VFMADD231SSRegRegReg)
      TARGET_PARAMETERIZED_OPCODE(VFMADD213PRegRegReg, VFMADD213PDRegRegReg, VFMADD213PSRegRegReg)

#define TARGET_CARRY_PARAMETERIZED_OPCODE(name, op64, op32) \
      static inline OMR::InstOpCode::Mnemonic name(bool is64Bit, bool isWithCarry) { return isWithCarry ? op64(is64Bit) : op32(is64Bit); }

      // Size and carry-parameterized opcodes
      //
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddMemImms , ADCMemImms , ADDMemImms)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddRegImms , ADCRegImms , ADDRegImms)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddRegImm4 , ADCRegImm4 , ADDRegImm4)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddMemImm4 , ADCMemImm4 , ADDMemImm4)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddMemReg  , ADCMemReg  , ADDMemReg)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddRegReg  , ADCRegReg  , ADDRegReg)
      TARGET_CARRY_PARAMETERIZED_OPCODE(AddRegMem  , ADCRegMem  , ADDRegMem)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubMemImms , SBBMemImms , SUBMemImms)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubRegImms , SBBRegImms , SUBRegImms)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubRegImm4 , SBBRegImm4 , SUBRegImm4)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubMemImm4 , SBBMemImm4 , SUBMemImm4)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubMemReg  , SBBMemReg  , SUBMemReg)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubRegReg  , SBBRegReg  , SUBRegReg)
      TARGET_CARRY_PARAMETERIZED_OPCODE(SubRegMem  , SBBRegMem  , SUBRegMem)

#ifdef TR_TARGET_64BIT
#define SIZE_PARAMETERIZED_OPCODE(name, op64, op32, op16, op8)              \
      static inline OMR::InstOpCode::Mnemonic name(int32_t size = 8)        \
         {                                                                  \
         OMR::InstOpCode::Mnemonic op = OMR::InstOpCode::bad;               \
         switch (size)                                                      \
            {                                                               \
            case 8: return op64;                                            \
            case 4: return op32;                                            \
            case 2: return op16;                                            \
            case 1: return op8;                                             \
            default:                                                        \
               TR_ASSERT_FATAL(false, "Unsupported operand size %d", size); \
               return OMR::InstOpCode::bad;                                 \
            }                                                               \
         }
#else
#define SIZE_PARAMETERIZED_OPCODE(name, op64, op32, op16, op8)              \
      static inline OMR::InstOpCode::Mnemonic name(int32_t size = 4)        \
         {                                                                  \
         OMR::InstOpCode::Mnemonic op = OMR::InstOpCode::bad;               \
         switch (size)                                                      \
            {                                                               \
            case 8: return op64;                                            \
            case 4: return op32;                                            \
            case 2: return op16;                                            \
            case 1: return op8;                                             \
            default:                                                        \
               TR_ASSERT_FATAL(false, "Unsupported operand size %d", size); \
               return OMR::InstOpCode::bad;                                 \
            }                                                               \
         }
#endif

      // Data type based opcodes
      SIZE_PARAMETERIZED_OPCODE(MovRegReg      , MOV8RegReg      , MOV4RegReg      , MOV2RegReg      , MOV1RegReg  )
      SIZE_PARAMETERIZED_OPCODE(IMulRegReg     , IMUL8RegReg     , IMUL4RegReg     , IMUL2RegReg     , IMUL1AccReg )
      SIZE_PARAMETERIZED_OPCODE(IMulRegMem     , IMUL8RegMem     , IMUL4RegMem     , IMUL2RegMem     , IMUL1AccMem )
      SIZE_PARAMETERIZED_OPCODE(IMulRegRegImms , IMUL8RegRegImms , IMUL4RegRegImms , IMUL2RegRegImms , bad         )
      SIZE_PARAMETERIZED_OPCODE(IMulRegRegImm4 , IMUL8RegRegImm4 , IMUL4RegRegImm4 , IMUL2RegRegImm2 , bad         )
      SIZE_PARAMETERIZED_OPCODE(IMulRegMemImms , IMUL8RegMemImms , IMUL4RegMemImms , IMUL2RegMemImms , bad         )
      SIZE_PARAMETERIZED_OPCODE(IMulRegMemImm4 , IMUL8RegMemImm4 , IMUL4RegMemImm4 , IMUL2RegMemImm2 , bad         )

   };
}
}
#endif
