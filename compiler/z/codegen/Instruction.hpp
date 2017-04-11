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

   /*
    * Generic constructors
    */
   Instruction(TR::InstOpCode::Mnemonic    op,
            TR::Node          *n,
            TR::CodeGenerator *cg) :
            OMR::InstructionConnector(cg, op, n) {}

   Instruction(TR::InstOpCode::Mnemonic    op,
               TR::Node          *n,
               TR::Instruction   *precedingInstruction,
               TR::CodeGenerator *cg):
               OMR::InstructionConnector(cg, precedingInstruction, op, n) {}

   /*
    * Z specific constructors, need to call initializer to perform proper construction
    */
   inline Instruction(TR::Instruction *precedingInstruction,
                      TR::InstOpCode::Mnemonic op,
                      TR::RegisterDependencyConditions *cond,
                      TR::CodeGenerator *cg);

   inline Instruction(TR::InstOpCode::Mnemonic    op,
                      TR::Node          *n,
                      TR::RegisterDependencyConditions *cond,
                      TR::CodeGenerator *cg);


   inline Instruction(TR::InstOpCode::Mnemonic    op,
                      TR::Node          *n,
                      TR::RegisterDependencyConditions * cond,
                      TR::Instruction   *precedingInstruction,
                      TR::CodeGenerator *cg);

   };

}

#include "codegen/OMRInstruction_inlines.hpp"

TR::Instruction::Instruction(TR::Instruction *precedingInstruction,
                             TR::InstOpCode::Mnemonic op,
                             TR::RegisterDependencyConditions *cond,
                             TR::CodeGenerator                    *cg) :
                             OMR::InstructionConnector(cg, precedingInstruction, op) { self()->initialize(precedingInstruction, true, cond, false); }

TR::Instruction::Instruction(TR::InstOpCode::Mnemonic    op,
                             TR::Node          *n,
                             TR::RegisterDependencyConditions *cond,
                             TR::CodeGenerator *cg) :
                             OMR::InstructionConnector(cg, op, n) { self()->initialize(NULL, false, cond, true); }


TR::Instruction::Instruction(TR::InstOpCode::Mnemonic    op,
                             TR::Node          *n,
                             TR::RegisterDependencyConditions * cond,
                             TR::Instruction   *precedingInstruction,
                             TR::CodeGenerator *cg) :
                             OMR::InstructionConnector(cg, precedingInstruction, op, n) { self()->initialize(precedingInstruction, true, cond, true); }

#endif /* TR_INSTRUCTION_INCL */
