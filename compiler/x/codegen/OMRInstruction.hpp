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

#ifndef OMR_X86_INSTRUCTIONBASE_INCL
#define OMR_X86_INSTRUCTIONBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTRUCTION_CONNECTOR
#define OMR_INSTRUCTION_CONNECTOR
namespace OMR { namespace X86 { class Instruction; } }
namespace OMR { typedef OMR::X86::Instruction InstructionConnector; }
#else
#error OMR::X86::Instruction expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRInstruction.hpp"

#include <stddef.h>                                    // for NULL
#include <stdint.h>                                    // for int32_t, etc
#include "codegen/InstOpCode.hpp"                      // for InstOpCode, etc
#include "codegen/RegisterConstants.hpp"
#include "x/codegen/X86Ops.hpp"                        // for TR_X86OpCode, etc

namespace TR { class X86ImmInstruction;   }
namespace TR { class X86LabelInstruction; }
namespace TR { class X86RegInstruction;   }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
struct TR_VFPState;

namespace OMR
{

namespace X86
{
class EnlargementResult
   {
   int32_t _patchGrowth; // the minimum guaratneed size of increase - we know we will grow this much
   int32_t _encodingGrowth; // the maximum size of increase - we have to accomodate growth of up to this much
   public:
   EnlargementResult(int32_t patchGrowth, int32_t encodingGrowth)
      : _patchGrowth(patchGrowth), _encodingGrowth(encodingGrowth) { }
   EnlargementResult(const EnlargementResult &other)
      : _patchGrowth(other._patchGrowth), _encodingGrowth(other._encodingGrowth) { }
   int32_t getPatchGrowth() { return _patchGrowth; }
   int32_t getEncodingGrowth() { return _encodingGrowth; }
   };

class OMR_EXTENSIBLE Instruction : public OMR::Instruction
   {
   private:
   uint8_t _rexRepeatCount;

   protected:
#if defined(TR_TARGET_64BIT)
   void setRexRepeatCount(uint8_t count) { _rexRepeatCount = count; }
#else
   void setRexRepeatCount(uint8_t count) { TR_ASSERT(0, "Forcing a rex repetition does not make sense in IA32 mode!"); }
#endif

   protected:

   Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node);
   Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node = 0);
   void initialize(TR::CodeGenerator *cg = NULL, TR::RegisterDependencyConditions *cond = NULL, TR_X86OpCodes op = BADIA32Op, bool flag = false);

   public:

   virtual char *description() { return "X86"; }
   virtual Kind getKind() { return IsNotExtended; }

   TR_X86OpCode& getOpCode() { return _opcode; }
   TR_X86OpCodes getOpCodeValue() { return _opcode.getOpCodeValue(); }
   TR_X86OpCodes setOpCodeValue(TR_X86OpCodes op) { return _opcode.setOpCodeValue(op); }

   virtual bool isBranchOp() {return _opcode.isBranchOp();}
   virtual bool isLabel();
   virtual bool isRegRegMove();
   virtual bool isPatchBarrier();

   TR::RegisterDependencyConditions *getDependencyConditions() {return _conditions;}
   TR::RegisterDependencyConditions *setDependencyConditions(TR::RegisterDependencyConditions *cond) { return (_conditions = cond); }

   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned);
   virtual bool refsRegister(TR::Register *reg);
   virtual bool defsRegister(TR::Register *reg);
   virtual bool usesRegister(TR::Register *reg);
   virtual bool dependencyRefsRegister(TR::Register *reg);

   virtual uint8_t *generateBinaryEncoding();
   virtual int32_t  estimateBinaryLength(int32_t currentEstimate);
   virtual uint8_t* generateOperand(uint8_t* cursor) { return cursor; }
   virtual bool     needsRepPrefix();
   virtual bool     needsLockPrefix();
   virtual EnlargementResult enlarge(int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement) { return EnlargementResult(0, 0); }

   enum TR_UpperHalfRefConditions
      {
      // These should be the first two so that booleans work properly
      TR_never,
      TR_always,

      // Some other conditions under which the top half of a register might be used
      TR_if64bitSource,
      TR_ifUses64bitTarget,
      TR_ifUses64bitSourceOrTarget, // A conservative default for when to setIsUpperHalfDead(true).  Shouldn't really be needed.
      TR_ifModifies32or64bitTarget,
      TR_ifModifies32or64bitSource,
      };

   bool registerRefKindApplies(TR_UpperHalfRefConditions when);
   void setIsUpperHalfDead(TR::Register *reg, bool value, TR_UpperHalfRefConditions when=TR_always);
   void aboutToAssignUsedRegister(TR::Register *reg, TR_UpperHalfRefConditions usesUpperHalf=TR_ifUses64bitSourceOrTarget);
   void aboutToAssignDefdRegister(TR::Register *reg, TR_UpperHalfRefConditions defsUpperHalf=TR_never);

   void aboutToAssignRegister(TR::Register *reg, TR_UpperHalfRefConditions usesUpperHalf=TR_ifUses64bitSourceOrTarget, TR_UpperHalfRefConditions defsUpperHalf=TR_never);

   void aboutToAssignMemRef(TR::MemoryReference *memref);

   void aboutToAssignRegDeps(TR_UpperHalfRefConditions usesUpperHalf=TR_ifUses64bitSourceOrTarget, TR_UpperHalfRefConditions defsUpperHalf=TR_never);

   virtual TR::X86RegInstruction *getIA32RegInstruction() { return NULL; }

   virtual TR::X86LabelInstruction *getIA32LabelInstruction() { return NULL; }

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   // The following safe virtual downcast method is used under debug only
   // for assertion checking.
   //
   virtual TR::X86ImmInstruction *getIA32ImmInstruction() { return NULL; }

   virtual uint32_t getNumOperandReferencedGPRegisters() { return 0; }
   virtual uint32_t getNumOperandReferencedFPRegisters() { return 0; }
   uint32_t totalReferencedGPRegisters(TR::CodeGenerator *);
   uint32_t totalReferencedFPRegisters(TR::CodeGenerator *);
#endif

   // AMD64-specific REX prefix calculations
   // Note the polymorphism
   //

   // Number of unnecessary REX bytes for instruction expansion and/or padding
   uint8_t rexRepeatCount();
   // Only repeated REX bytes for expansion/padding are generated
   uint8_t *generateRepeatedRexPrefix(uint8_t *cursor);

#if defined(TR_TARGET_64BIT)
   uint8_t operandSizeRexBits();
   // Each subclass should override this as necessary
   virtual uint8_t rexBits();
#else
   uint8_t rexBits() { return 0; }
#endif

   // Adjust the VFP state to reflect the execution of this instruction.
   //
   virtual void adjustVFPState(TR_VFPState *state, TR::CodeGenerator *cg);

   virtual TR::Register *getTargetRegister() { return NULL; }
   virtual TR::Register *getSourceRegister() { return NULL; }
   virtual TR::Register *getSourceRightRegister() { return NULL; }
   virtual TR::MemoryReference *getMemoryReference() { return NULL; }

   int32_t getMaxPatchableInstructionLength() { return 10; }

   protected:

   // Facility for subclasses to compute effect of a call on the VFP state.
   // If _opcode->isCallOp(), adds vfpAdjustmentForCall; else, calls TR::Instruction ::adjustVFPState.
   //
   void adjustVFPStateForCall(TR_VFPState *state, int32_t vfpAdjustmentForCall, TR::CodeGenerator *cg);
   void clobberRegsForRematerialisation();

   private:

   TR_X86OpCode _opcode;
   TR::RegisterDependencyConditions *_conditions;
   void assumeValidInstruction();

   public:

   struct ModRM
      {
      uint8_t rm : 3;
      uint8_t reg : 3;
      uint8_t mod : 2;
      inline ModRM() {}
      inline ModRM(uint8_t opcode)
         {
         rm = 0;
         reg = opcode;
         mod = 0x3;
         }
      inline ModRM(const ModRM& other)
         {
         rm = other.rm;
         reg = other.reg;
         mod = other.mod;
         }
      inline operator uint8_t() const
         {
         return *((uint8_t*)this);
         }
      inline uint8_t Reg(uint8_t R = 0) const
         {
         return (R << 3) | (0x7 & reg);
         }
      inline uint8_t RM(uint8_t B = 0) const
         {
         TR_ASSERT(mod == 0x3, "ModRM is not in register mode");
         return (B << 3) | (0x7 & rm);
         }
      inline ModRM* setMod(uint8_t mod = 0x03) // 0b11
         {
         this->mod = mod;
         return this;
         }
      inline ModRM* setBase()
         {
         return setMod(0x00); // 0b00
         }
      inline ModRM* setBaseDisp8()
         {
         return setMod(0x01); // 0b01
         }
      inline ModRM* setBaseDisp32()
         {
         return setMod(0x02); // 0b10
         }
      inline ModRM* setIndexOnlyDisp32()
         {
         rm = 0x05; // 0b101
         return setMod(0x00); // 0b00
         }
      inline ModRM* setHasSIB()
         {
         rm = 0x04; // 0b100
         return this;
         }
      };
   struct SIB
      {
      uint8_t base : 3;
      uint8_t index : 3;
      uint8_t scale : 2;
      inline operator uint8_t() const
         {
         return *((uint8_t*)this);
         }
      inline SIB* setScale(uint8_t scale = 0)
         {
         this->scale = scale;
         return this;
         }
      inline SIB* setNoIndex()
         {
         index = 0x04; // 0b100
         return this;
         }
      inline SIB* setIndexDisp32()
         {
         base = 0x05; // 0b101
         return this;
         }
      };
   struct REX
      {
      uint8_t B : 1;
      uint8_t X : 1;
      uint8_t R : 1;
      uint8_t W : 1;
      uint8_t _padding : 4;
      inline REX(uint8_t val = 0)
         {
         *((uint8_t*)this) = val;
         _padding = 0x4;
         }
      inline uint8_t value() const
         {
         return 0x0f & *((uint8_t*)this);
         }
      };
   template<size_t VEX_SIZE>
   struct VEX
      {
      VEX() {TR_ASSERT(false, "INVALID VEX PREFIX");}
      };
   };

   template<>
   struct Instruction::VEX<3>
      {
      // Byte 0: C4
      uint8_t escape;
      // Byte 1
      uint8_t m : 5;
      uint8_t B : 1;
      uint8_t X : 1;
      uint8_t R : 1;
      // Byte 2
      uint8_t p : 2;
      uint8_t L : 1;
      uint8_t v : 4;
      uint8_t W : 1;
      // Byte 3: opcode
      uint8_t opcode;
      // Byte 4: ModRM
      ModRM   modrm;

      inline VEX() {}
      inline VEX(const REX& rex, uint8_t ModRMOpCode) : modrm(ModRMOpCode)
         {
         escape = '\xC4';
         R = ~rex.R;
         X = ~rex.X;
         B = ~rex.B;
         W = rex.W;
         v = 0xf; //0b1111
         }
      inline bool CanBeShortened() const
         {
         return X && B && !W && (m == 1);
         }
      inline uint8_t Reg() const
         {
         return modrm.Reg(~R);
         }
      inline uint8_t RM() const
         {
         return modrm.RM(~B);
         }
      };
   template<>
   struct Instruction::VEX<2>
      {
      // Byte 0: C5
      uint8_t escape;
      // Byte 1
      uint8_t p : 2;
      uint8_t L : 1;
      uint8_t v : 4;
      uint8_t R : 1;
      // Byte 2: opcode
      uint8_t opcode;
      // Byte 3: ModRM
      ModRM   modrm;

      inline VEX() {}
      inline VEX(const VEX<3>& other) : modrm(other.modrm)
         {
         escape = '\xC5';
         p = other.p;
         L = other.L;
         v = other.v;
         R = other.R;
         opcode = other.opcode;
         }
      inline uint8_t Reg() const
         {
         return modrm.Reg(~R);
         }
      inline uint8_t RM() const
         {
         return modrm.RM();
         }
      };
}

}

#endif
