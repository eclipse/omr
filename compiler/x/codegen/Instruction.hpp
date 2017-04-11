/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef OMR_INSTRUCTION_INCL
#define OMR_INSTRUCTION_INCL

#include "codegen/OMRInstruction.hpp"

namespace TR
{
class Instruction;

class OMR_EXTENSIBLE Instruction : public OMR::InstructionConnector
   {
   public:

   // TODO: need to fix the TR::InstOpCode initialization once TR::InstOpCode class is done

   /*
    * Generic constructors
    */
   inline Instruction(TR::Node *node, TR_X86OpCodes op, TR::CodeGenerator *cg);

   inline Instruction(TR_X86OpCodes op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg);


   /*
    * X86 specific constructors, need to call initializer to perform proper construction
    */
   inline Instruction(TR::RegisterDependencyConditions *cond, TR::Node *node, TR_X86OpCodes op, TR::CodeGenerator *cg);

   inline Instruction(TR::RegisterDependencyConditions *cond, TR_X86OpCodes op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg);

   };

}

#include "codegen/OMRInstruction_inlines.hpp"

TR::Instruction::Instruction(TR::Node *node, TR_X86OpCodes op, TR::CodeGenerator *cg) :
   OMR::InstructionConnector(cg, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->initialize();
   }

TR::Instruction::Instruction(TR_X86OpCodes op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg) :
   OMR::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::BAD)
   {
   self()->setOpCodeValue(op);
   self()->initialize();
   }

TR::Instruction::Instruction(TR::RegisterDependencyConditions *cond, TR::Node *node, TR_X86OpCodes op, TR::CodeGenerator *cg) :
   OMR::InstructionConnector(cg, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setDependencyConditions(cond);
   self()->initialize(cg, cond, op, true);
   }

TR::Instruction::Instruction(TR::RegisterDependencyConditions *cond, TR_X86OpCodes op, TR::Instruction *precedingInstruction, TR::CodeGenerator *cg) :
   OMR::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::BAD)
   {
   self()->setOpCodeValue(op);
   self()->setDependencyConditions(cond);
   self()->initialize(cg, cond, op);
   }


#endif
