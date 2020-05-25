/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef OMR_RV_INSTRUCTION_INCL
#define OMR_RV_INSTRUCTION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_INSTRUCTION_CONNECTOR
#define OMR_INSTRUCTION_CONNECTOR
namespace OMR { namespace RV { class Instruction; } }
namespace OMR { typedef OMR::RV::Instruction InstructionConnector; }
#else
#error OMR::RV::Instruction expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRInstruction.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class BtypeInstruction; }

namespace OMR
{

namespace RV
{

class OMR_EXTENSIBLE Instruction : public OMR::Instruction
   {

   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    * @param[in] op : opcode
    * @param[in] node : node
    */
   Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node = NULL);
   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    * @param[in] precedingInstruction : preceding instruction
    * @param[in] op : opcode
    * @param[in] node : node
    */
   Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node = NULL);
   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    * @param[in] op : opcode
    * @param[in] cond : register dependency conditions
    * @param[in] node : node
    */
   Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node = NULL);
   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    * @param[in] precedingInstruction : preceding instruction
    * @param[in] op : opcode
    * @param[in] cond : register dependency conditions
    * @param[in] node : node
    */
   Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node = NULL);

   /**
    * @brief Instruction description string
    * @return description string
    */
   virtual char *description() { return "RV"; }

   /**
    * @brief Gets instruction kind
    * @return instruction kind
    */
   virtual Kind getKind() { return IsNotExtended; }

   /**
    * @brief Estimates binary length
    * @param[in] currentEstimate : current estimated length
    * @return estimated binary length
    */
   virtual int32_t estimateBinaryLength(int32_t currentEstimate);

   /**
    * @brief Generates binary encoding of the instruction
    * @return instruction cursor
    */
   virtual uint8_t *generateBinaryEncoding();

   /**
    * @brief Gets the opcode of the instruction
    * @return opcode of the instruction
    */
   TR::InstOpCode& getOpCode() {return _opcode;}

   /**
    * @brief Removes this instruction from instruction list
    */
   void remove();

   /**
    * @brief Answers if this instruction is a label or not
    * @return true if this instruction is a label, false otherwise
    */
   virtual bool isLabel() { return _opcode.getMnemonic() == TR::InstOpCode::label; }

   /**
    * @brief Gets the register dependency conditions
    * @return register dependency conditions
    */
   virtual TR::RegisterDependencyConditions *getDependencyConditions()
      {
      return _conditions;
      }
   /**
    * @brief Sets the register dependency conditions
    * @param[in] cond : register dependency conditions
    * @return register dependency conditions
    */
   TR::RegisterDependencyConditions *setDependencyConditions(TR::RegisterDependencyConditions *cond)
      {
      return (_conditions = cond);
      }

   /**
    * @brief Gets the basic block index
    * @return basic block index
    */
   int32_t getBlockIndex()            { return _blockIndex; }
   /**
    * @brief Sets the basic block index
    * @param[in] i : basic block index
    * @return basic block index
    */
   void setBlockIndex(int32_t i)   { _blockIndex = i; }

   /**
    * @brief Sets GCMap mask
    * @param[in] cg : CodeGenerator
    * @param[in] mask : GCMap mask
    */
   void RVNeedsGCMap(TR::CodeGenerator *cg, uint32_t mask);

   /**
    * @brief Assigns registers
    * @param[in] kindToBeAssigned : register kind
    */
   virtual void assignRegisters(TR_RegisterKinds kindToBeAssigned);

   /**
    * @brief Answers whether this instruction references the given virtual register
    * @param[in] reg : virtual register
    * @return true when the instruction references the virtual register
    */
   virtual bool refsRegister(TR::Register *reg);

   /**
    * @brief Answers whether this instruction defines the given virtual register
    * @param[in] reg : virtual register
    * @return true when the instruction defines the virtual register
    */
   virtual bool defsRegister(TR::Register *reg);

   /**
    * @brief Answers whether this instruction uses the given virtual register
    * @param[in] reg : virtual register
    * @return true when the instruction uses the virtual register
    */
   virtual bool usesRegister(TR::Register *reg);

   /**
    * @brief Answers whether a dependency condition associated with this instruction references the given virtual register
    * @param[in] reg : virtual register
    * @return true when a dependency condtion references the virtual register
    */
   virtual bool dependencyRefsRegister(TR::Register *reg);

   /*
    * Maps to TIndex in Instruction. Here we set values specific to RV CodeGen.
    *
    * A 32-bit field where the lower 24-bits contain an integer that represents an
    * approximate ordering of instructions.
    *
    * The upper 8 bits are used for flags.
    * Instruction flags encoded by their bit position.  Subclasses may use any
    * available bits between LastBaseFlag and MaxBaseFlag inclusive.
    */
   enum
      {
      WillBePatched        = 0x08000000
      };

   /**
    * @brief Answers the status of WillBePatched flag
    * @return true when WillBePatched flag is set
    */
   bool willBePatched() {return (_index & WillBePatched) != 0; }

   /**
    * @brief Sets WillBePatched flag
    * @param[in] v : flag status
    */
   void setWillBePatched(bool v = true) { v ? _index |= WillBePatched : _index &= ~WillBePatched; }

   /**
    * @brief: It this is a B-type instruction, return it. Otherwise return NULL.
    *
    * @return: B-type instruction or NULL
    */
   virtual TR::BtypeInstruction *getBtypeInstruction();

   private:
      int32_t _blockIndex;
      TR::RegisterDependencyConditions *_conditions;
   };

} // RV

} // OMR

#endif
