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

#ifndef OMR_INSTRUCTION_INCL
#define OMR_INSTRUCTION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTRUCTION_CONNECTOR
#define OMR_INSTRUCTION_CONNECTOR
namespace OMR { class Instruction; }
namespace OMR { typedef OMR::Instruction InstructionConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/RegisterConstants.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/TRlist.hpp"

class TR_BitVector;
class TR_GCStackMap;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class LabelSymbol; }
namespace TR { class RealRegister; }
namespace TR { class Snippet; }

#ifndef TO_MASK
#define TO_MASK(b) (1<<(uint16_t)b)
#endif
#define INSTRUCTION_INDEX_INCREMENT 32

namespace OMR
{
/**
 * Instruction class
 *
 * OMR_EXTENSIBLE
*/
class OMR_EXTENSIBLE Instruction
   {
   protected:

   Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node = 0);
   Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node = 0);

   public:
   typedef uint32_t TCollectableReferenceMask;

   inline TR::Instruction* self();

   /*
    * Need to move them to FE specific files once more clean up is done.
    */
   virtual TR::Snippet *getSnippetForGC() { return 0; }
   virtual bool isVirtualGuardNOPInstruction() { return false; }

   virtual TR::LabelSymbol *getLabelSymbol() { return NULL; }

   typedef uint32_t TIndex;
   TR_ALLOC(TR_Memory::Instruction)

   /*
    * Instruction OpCodes
    */
   TR::InstOpCode& getOpCode()                       { return _opcode; }
   TR::InstOpCode::Mnemonic getOpCodeValue()                  { return _opcode.getMnemonic(); }
   void setOpCodeValue(TR::InstOpCode::Mnemonic op) { _opcode.setMnemonic(op); }

   /*
    * Instruction stream links.
    */
   TR::Instruction *getNext() { return _next; }
   void setNext(TR::Instruction *n) { _next = n; }

   TR::Instruction *getPrev() { return _prev; }
   void setPrev(TR::Instruction *p) { _prev = p; }

   void remove();
   TR::Instruction *move(TR::Instruction *newLocation);

   /*
    * Address of binary buffer where this instruction was encoded.  The binary buffer
    * is NULL if this instruction has not been encoded yet.
    */
   uint8_t *getBinaryEncoding() { return _binaryEncodingBuffer; }
   void setBinaryEncoding(uint8_t *be) { _binaryEncodingBuffer = be; }

   uint8_t getBinaryLength() { return _binaryLength; }
   void setBinaryLength(uint8_t length) { _binaryLength = length; }

   /*
    * Cached estimate of an instruction's worst case length in bytes.
    * Assumes that estimateBinaryLength() has been run before.
    */
   uint8_t getEstimatedBinaryLength() { return _estimatedBinaryLength; }
   void setEstimatedBinaryLength(uint8_t e) { _estimatedBinaryLength = e; }

   /*
    * Action to estimate the generated binary length of this instruction.
    */
   virtual int32_t estimateBinaryLength(int32_t currentEstimate) = 0;

   /*
    * The minimum length in bytes this instruction could be when encoded.
    */
   virtual uint8_t getBinaryLengthLowerBound() { return 0; }

   /*
    * Action to generate the binary representation of this instruction at
    * the current buffer cursor.
    */
   virtual uint8_t *generateBinaryEncoding() = 0;

   /**
    * @brief Expand this instruction prior to binary encoding.
    *
    * @returns the last instruction in the expansion or this instruction if no expansion was
    *          performed.
    */
   virtual TR::Instruction *expandInstruction() { return self(); }

   /*
    * Assign the specified register kind(s) on this instruction to real registers.
    */
   virtual void assignRegisters(TR_RegisterKinds kindsToBeAssigned) { return; }

   /*
    * Answers whether this instruction references the given virtual register.
    */
   virtual bool refsRegister(TR::Register *reg) { return false; }

   /*
    * Answers whether this instruction defines the given virtual register.
    */
   virtual bool defsRegister(TR::Register *reg) { return false; }

   /*
    * Answers whether this instruction uses the given virtual register.
    */
   virtual bool usesRegister(TR::Register *reg) { return false; }

   /*
    * Answers whether a dependency condition associated with this instruction
    * references the given virtual register.
    */
   virtual bool dependencyRefsRegister(TR::Register *reg) { return false; }

   /*
    * The IR node whence this instruction was created.
    */
   TR::Node *getNode() { return _node; }
   void setNode(TR::Node * n) { _node = n; }

   /*
    * Answers whether the instruction can be included in a guard patch point
    *
    * @param[in] cg : CodeGenerator object
    */
   virtual bool isPatchBarrier(TR::CodeGenerator *cg) { return false; }

   /*
    * A 24-bit value indicating an approximate ordering of instructions.  Note that for an
    * instruction 'I1' followed by an instruction 'I2' it is not guaranteed that
    * I2.index > I1.index, though it is usually true.
    */
   TIndex getIndex() { return _index & ~FlagsMask; }
   void setIndex(TIndex i) { TR_ASSERT((i & FlagsMask) == 0, "instruction flags overwrite"); _index = i | (_index & FlagsMask); }

   /*
    * Instruction kind.
    */
   enum Kind
      {
      #include "codegen/InstructionKindEnum.hpp"
      };

   virtual Kind getKind() = 0;

   /*
    * OMRTODO: TR specific?
    */
   void useRegister(TR::Register *reg);

   /*
    * Cached code generator object.
    * OMRTODO: should be retrievable from TLS.
    */
   TR::CodeGenerator *cg() { return _cg; }


   bool needsGCMap() { return (_index & TO_MASK(NeedsGCMapBit)) != 0; }
   void setNeedsGCMap(TCollectableReferenceMask regMask = 0xFFFFFFFF) { _gc._GCRegisterMask = regMask; _index |= TO_MASK(NeedsGCMapBit); }
   bool needsAOTRelocation() { return (_index & TO_MASK(NeedsAOTRelocation)) != 0; }
   void setNeedsAOTRelocation(bool v = true) { v ? _index |= TO_MASK(NeedsAOTRelocation) : _index &= ~TO_MASK(NeedsAOTRelocation); }

   /**
    * @brief Indicates instruction after which binary encoding should switch to a cold cache
    *
    * @returns true iff instruction is a last warm instruction
    */
   bool isLastWarmInstruction() { return (_index & TO_MASK(LastWarmInstruction)) != 0; }

   /**
    * @brief Identifies instruction as a last warm instruction.
    *        Instructions after that will be placed into a cold cache.
    *        Code that sets it is responsible for maintaining the correct value
    *        in case the instruction is removed or a new instruction is appended
    */
   void setLastWarmInstruction(bool v = true) { v ? _index |= TO_MASK(LastWarmInstruction) : _index &= ~TO_MASK(LastWarmInstruction); }

   TR_GCStackMap *getGCMap() { return _gc._GCMap; }
   TR_GCStackMap *setGCMap(TR_GCStackMap *map) { return (_gc._GCMap = map); }
   TCollectableReferenceMask getGCRegisterMask() { return _gc._GCRegisterMask; }

   TR_BitVector *getLiveLocals() { return _liveLocals; }
   TR_BitVector *setLiveLocals(TR_BitVector *v) { return (_liveLocals = v); }
   TR_BitVector *getLiveMonitors() { return _liveMonitors; }
   TR_BitVector *setLiveMonitors(TR_BitVector *v) { return (_liveMonitors = v); }

   bool    requiresAtomicPatching();

   int32_t getMaxPatchableInstructionLength() { return 0; }

   bool isMergeableGuard();

   protected:

   uint8_t *_binaryEncodingBuffer;
   uint8_t _binaryLength;
   uint8_t _estimatedBinaryLength;

   TR::InstOpCode _opcode;

   /*
    * A 32-bit field where the lower 24-bits contain an integer that represents an
    * approximate ordering of instructions.
    *
    * The upper 8 bits are available for use for flags.
    */
   TIndex _index;

   /*
    * Instruction flags encoded by their bit position.  Subclasses may use any
    * available bits between FirstBaseFlag and MaxBaseFlag-1 inclusive.
    */
   enum
      {
      // First possible bit for flags
      //
      FirstBaseFlag = 24,

      // Last possible bit for flags
      //
      MaxBaseFlag = 31,
      FlagsMask = 0xff000000,

      BeforeFirstBaseFlag = FirstBaseFlag-1,

      #include "codegen/InstructionFlagEnum.hpp"

      LastBaseFlag
      };

   static_assert(LastBaseFlag <= MaxBaseFlag+1, "OMR::Instruction too many flag bits for flag width");

   private:

   Instruction() {}

   TR::Instruction *_next;
   TR::Instruction *_prev;
   TR::Node *_node;
   TR::CodeGenerator *_cg;

   protected:
   TR_BitVector *_liveLocals;
   TR_BitVector *_liveMonitors;

   union TR_GCInfo
      {
      TCollectableReferenceMask _GCRegisterMask;
      TR_GCStackMap *_GCMap;
      TR_GCInfo()
         {
         _GCRegisterMask = 0;
         _GCMap = 0;
         }
      } _gc;


   };

}

#endif
