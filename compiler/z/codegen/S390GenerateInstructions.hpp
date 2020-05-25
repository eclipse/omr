/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef GENERATEINSTRUCTION_INCL
#define GENERATEINSTRUCTION_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/Snippet.hpp"
#include "env/jittypes.h"
#include "il/Symbol.hpp"

#include "z/codegen/S390Instruction.hpp"

class TR_VirtualGuardSite;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterPair; }
namespace TR { class SymbolReference; }
namespace TR { class UnresolvedDataSnippet; }

//////////////////////////////////////////////////////////////////////////
// Generate Routines
//////////////////////////////////////////////////////////////////////////

TR::Instruction *generateInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic        op,
                   TR::Node               *n,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateS390LabelInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390LabelInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node                            *n,
                   TR::LabelSymbol                      *sym,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390LabelInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node                             *n,
                   TR::Snippet                          *s,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390LabelInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node                             *n,
                   TR::Snippet                          *s,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::InstOpCode::S390BranchCondition brCond,
                   TR::Node        *n,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

/** \brief
 *     Generates a branch instruction accepting a branch condition and a target register.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param op
 *     The mnemonic of the branch instruction.
 *
 *  \param node
 *     The node to which the generated instruction will be associated with.
 *
 *  \param compareOpCode
 *     The branch condition code.
 *
 *  \param targetReg
 *     The data to inline in the instruction stream.
 *
 *  \param preced
 *     The preceeding instruction to link the generated branch instruction with.
 *
 *  \return
 *     The generated instruction.
 */
TR::Instruction* generateS390BranchInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* node, TR::InstOpCode::S390BranchCondition compareOpCode, TR::Register* targetReg, TR::Instruction* preced = NULL);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::Register    *sourceRegister,
                   TR::Register    *targetRegister,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::RegisterPair    *sourceRegister,
                   TR::Register    *targetRegister,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::Register    *targetRegister,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::Register    *targetRegister,
                   TR::RegisterDependencyConditions *cond,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::InstOpCode::S390BranchCondition brCond,
                   TR::Node                            *n,
                   TR::LabelSymbol                      *sym,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::InstOpCode::S390BranchCondition brCond,
                   TR::Node                             *n,
                   TR::Snippet                          *s,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390BranchInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::InstOpCode::S390BranchCondition brCond,
                   TR::Node                             *n,
                   TR::Snippet                          *s,
                   TR::Instruction                     *preced = 0);

TR::Instruction * generateS390CompareAndBranchInstruction(
                   TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic compareOpCode,
                   TR::Node * node,
                   TR::Register * first,
                   TR::Register * second,
                   TR::InstOpCode::S390BranchCondition bc,
                   TR::LabelSymbol * branchDestination,
                   bool needsCC = true,
                   bool targetIsFarAndCold = false);

template <typename imm32Or64Bit>
TR::Instruction * generateS390CompareAndBranchInstruction(
                   TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic compareOpCode,
                   TR::Node * node,
                   TR::Register * first,
                   imm32Or64Bit second,
                   TR::InstOpCode::S390BranchCondition bc,
                   TR::LabelSymbol * branchDestination,
                   bool needsCC = true,
                   bool targetIsFarAndCold = false,
                   TR::Instruction        *preced = 0,
                   TR::RegisterDependencyConditions *cond = 0);

TR::Instruction * generateS390RegInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateS390RegInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *n,
                   TR::Register      *treg,
                   int8_t           secondConstant,
                   TR::Instruction   *preced = 0);

TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *n,
                   int8_t           firstConstant,
                   TR::Register      *sreg,
                   TR::Instruction   *preced = 0);

TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *n,
                   int8_t           firstConstant,
                   int8_t           secondConstant,
                   TR::Instruction   *preced = 0);

TR::Instruction * generateRRDInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::Register           *sreg2,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   uint8_t mask,
                   bool isMask3,
                   TR::Instruction * preced = 0);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::Register           *sreg2,
                   uint8_t                 mask,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateS390BranchPredictionPreloadInstruction(
                TR::CodeGenerator  *cg,
                TR::InstOpCode::Mnemonic    op,
                TR::Node           *n,
                TR::LabelSymbol     *sym,
                uint8_t           mask,
                TR::MemoryReference *mf3,
                TR::Instruction    *preced = NULL);
TR::Instruction *generateS390BranchPredictionRelativePreloadInstruction(
                TR::CodeGenerator  *cg,
                TR::InstOpCode::Mnemonic    op,
                TR::Node           *n,
                TR::LabelSymbol     *sym,
                uint8_t           mask,
                TR::SymbolReference *sym3,
                TR::Instruction    *preced = NULL);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   uint8_t                 mask,
                   bool                    isMask3,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   uint8_t                 mask,
                   bool                    isMask3,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::Register           *sreg2,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   uint8_t                 mask3,
                   uint8_t                 mask4,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::Register           *sreg2,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRXInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRXInstruction(
                   TR::CodeGenerator* cg, 
                   TR::InstOpCode::Mnemonic op, 
                   TR::Node* n, 
                   uint8_t mask, 
                   TR::MemoryReference* mf, 
                   TR::Instruction* preced = 0);

TR::Instruction * generateRXEInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::MemoryReference *mf,
                   uint8_t                mask3,
                   TR::Instruction     *preced = 0);

TR::Instruction * generateRXFInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::Register           *sreg,
                   TR::MemoryReference *mf,
                   TR::Instruction        *preced = 0);

TR::Instruction * generateRIInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRIInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRIInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   int32_t                 imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRIInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   char                    *data,
                   TR::Instruction         *preced = 0);

/** \brief
 *     Generates a new S390RILInstruction object.
 *
 *     RIL type instructions have 2 different kinds of immediates. One is a "pure" immediate, meaning
 *     the immediate is used as is, usually as an integer for an instruction like AFI. The other is a
 *     relative immediate used to calculate an address in instructions like BRASL and LARL.
 *
 *     This function is to be used for instructions with relative immediates. A fatal assert will trigger
 *     if this function is used with a mnemonic which has a pure immediate.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param op
 *     The opcode mnemonic of the instruction to be generated.
 *
 *  \param n
 *     The node for which an instruction is being generated.
 *
 *  \param treg
 *     The target register for the instruction.
 *
 *  \param sr
 *     The symbol reference to be used.
 *
 *  \param addr
 *     The address to be used in the generated instruction. An absolute address needs to be used.
 *     Binary Encoding will automatically change the absolute address to the relative offset format
 *     needed by the instruction.
 *
 *  \param preced
 *     The preceeding instruction.
 *
 *  \return
 *     The generated S390RILInstruction.
 */
TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   TR::SymbolReference      *sr,
                   void                     *addr,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   TR::LabelSymbol          *label,
                   TR::Instruction          *preced = 0);

/** \brief
 *     Generates a new S390RILInstruction object.
 *
 *     RIL type instructions have 2 different kinds of immediates. One is a "pure" immediate, meaning
 *     the immediate is used as is, usually as an integer for an instruction like AFI. The other is a
 *     relative immediate used to calculate an address in instructions like BRASL and LARL.
 *
 *     This function is to be used for instructions with pure immediates. A fatal assert will trigger
 *     if this function is used with a mnemonic which has a relative immediate.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param op
 *     The opcode mnemonic of the instruction to be generated.
 *
 *  \param n
 *     The node for which an instruction is being generated.
 *
 *  \param treg
 *     The target register for the instruction.
 *
 *  \param imm
 *     The pure immediate to be used.
 *
 *  \param preced
 *     The preceeding instruction.
 *
 *  \return
 *     The generated S390RILInstruction.
 */
TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   uint32_t                  imm,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   int32_t                   imm,
                   TR::Instruction          *preced = 0);

/** \brief
 *     Generates a new S390RILInstruction object.
 *
 *     RIL type instructions have 2 different kinds of immediates. One is a "pure" immediate, meaning
 *     the immediate is used as is, usually as an integer for an instruction like AFI. The other is a
 *     relative immediate used to calculate an address in instructions like BRASL and LARL.
 *
 *     This function is to be used for instructions with relative immediates. A fatal assert will trigger
 *     if this function is used with a mnemonic which has a pure immediate.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param op
 *     The opcode mnemonic of the instruction to be generated.
 *
 *  \param n
 *     The node for which an instruction is being generated.
 *
 *  \param treg
 *     The target register for the instruction.
 *
 *  \param sr
 *     The symbol reference to be used.
 *
 *  \param addr
 *     The address to be used in the generated instruction. An absolute address needs to be used.
 *     Binary Encoding will automatically change the absolute address to the relative offset format
 *     needed by the instruction.
 *
 *  \param preced
 *     The preceeding instruction.
 *
 *  \return
 *     The generated S390RILInstruction.
 */
TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   void                     *addr,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   uint32_t                  mask,
                   void                     *addr,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic  op,
                   TR::Node                 *n,
                   TR::Register             *treg,
                   TR::Snippet              *ts,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateSIInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::MemoryReference *mf,
                   uint32_t                imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSILInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::MemoryReference *mf,
                   uint32_t                imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *n,
                   TR::Instruction   *preced = 0);

TR::Instruction * generateRSLInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   uint16_t                len,
                   TR::MemoryReference *mf1,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSLbInstruction(
                        TR::CodeGenerator * cg,
                        TR::InstOpCode::Mnemonic op,
                        TR::Node * n,
                        TR::Register *reg,
                        uint16_t length,
                        TR::MemoryReference *mf1,
                        uint8_t mask,
                        TR::Instruction * preced = 0);

TR::Instruction * generateSS1Instruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   uint16_t                len,
                   TR::MemoryReference *mf1,
                   TR::MemoryReference *mf2,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSS1Instruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   uint16_t                len,
                   TR::MemoryReference *mf1,
                   TR::MemoryReference *mf2,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSS1WithImplicitGPRsInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   uint16_t                len,
                   TR::MemoryReference *mf1,
                   TR::MemoryReference *mf2,
                   TR::RegisterDependencyConditions *cond,
                   TR::Register *           implicitRegSrc0,
                   TR::Register *           implicitRegSrc1,
                   TR::Register *           implicitRegTrg0,
                   TR::Register *           implicitRegTrg1,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSS2Instruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   uint16_t                len,
                   TR::MemoryReference *mf1,
                   uint16_t                len2,
                   TR::MemoryReference *mf2,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSS3Instruction(TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   uint32_t len,
                   TR::MemoryReference * mf1,
                   TR::MemoryReference * mf2,
                   uint32_t roundAmount = 0,
                   TR::Instruction * preced = 0);

TR::Instruction * generateSS3Instruction(TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   uint32_t len,
                   TR::MemoryReference * mf1,
                   int32_t shiftAmount,
                   uint32_t roundAmount = 0,
                   TR::Instruction * preced = 0);

TR::Instruction * generateSS4Instruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * lengthReg,
                                        TR::MemoryReference * mf1,
                                        TR::MemoryReference * mf2,
                                        TR::Register * sourceKeyReg,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5Instruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::Register * op3Reg,
                                        TR::MemoryReference * mf4,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5Instruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5Instruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::MemoryReference * mf4,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::Register * op3Reg,
                                        TR::MemoryReference * mf4,
                                        TR::Register * implicitRegSrc0,
                                        TR::Register * implicitRegSrc1,
                                        TR::Register * implicitRegTrg0,
                                        TR::Register * implicitRegTrg1,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::Register * implicitRegSrc0,
                                        TR::Register * implicitRegSrc1,
                                        TR::Register * implicitRegTrg0,
                                        TR::Register * implicitRegTrg1,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSS5WithImplicitGPRsInstruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node * n,
                                        TR::Register * op1Reg,
                                        TR::MemoryReference * mf2,
                                        TR::MemoryReference * mf4,
                                        TR::Register * implicitRegSrc0,
                                        TR::Register * implicitRegSrc1,
                                        TR::Register * implicitRegTrg0,
                                        TR::Register * implicitRegTrg1,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSSEInstruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node *n,
                                        TR::MemoryReference *mf1,
                                        TR::MemoryReference *mf2,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateSSFInstruction(TR::CodeGenerator *cg,
                                        TR::InstOpCode::Mnemonic op,
                                        TR::Node *n,
                                        TR::RegisterPair *regp,
                                        TR::MemoryReference *mf1,
                                        TR::MemoryReference *mf2,
                                        TR::Instruction * preced = 0);

TR::Instruction * generateS390MemInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateS390MemInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   int8_t                memAccessMode,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateS390MemInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   int8_t                constantField,
                   int8_t                memAccessMode,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);


TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   TR::Register                         *sreg,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction * generateRRInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *n,
                   TR::Instruction   *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::RegisterPair        *treg,
                   TR::RegisterPair        *sreg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                imm,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                mask,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *freg,
                   TR::Register            *lreg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::RegisterPair        *regp,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Register            *sreg,
                   uint32_t                imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRSWithImplicitPairStoresInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::RegisterPair        *treg,
                   TR::RegisterPair        *sreg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRRSInstruction(
                   TR::CodeGenerator       * cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                * n,
                   TR::Register            * treg,
                   TR::Register            * sreg,
                   TR::MemoryReference * mf,
                   TR::InstOpCode::S390BranchCondition mask,
                   TR::Instruction         * preced = 0);

TR::Instruction * generateRREInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRREInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Register            *sreg,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRREInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Register            *sreg,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRIEInstruction(
                   TR::CodeGenerator       * cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                * n,
                   TR::Register            * treg,
                   TR::Register            * sreg,
                   TR::LabelSymbol          * branch,
                   TR::InstOpCode::S390BranchCondition mask,
                   TR::Instruction         * preced = 0);

TR::Instruction * generateRIEInstruction(
                   TR::CodeGenerator       * cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                * n,
                   TR::Register            * treg,
                   TR::Register            * sreg,
                   int8_t                 immOne,
                   int8_t                 immTwo,
                   int8_t                 immThree,
                   TR::Instruction         * preced = 0);

TR::Instruction * generateRIEInstruction(
                   TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   TR::Register * treg,
                   int8_t immCompare,
                   TR::LabelSymbol * branch,
                   TR::InstOpCode::S390BranchCondition mask,
                   TR::Instruction * preced = 0);

TR::Instruction * generateRIEInstruction(
                   TR::CodeGenerator* cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node* n,
                   TR::Register * treg,
                   int16_t sourceImmediate,
                   TR::InstOpCode::S390BranchCondition branchCondition,
                   TR::Instruction *preced = 0);

TR::Instruction * generateRIEInstruction(
                   TR::CodeGenerator* cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node* n,
                   TR::Register * treg,
                   TR::Register * sreg,
                   int16_t sourceImmediate,
                   TR::Instruction *preced = 0);

TR::Instruction * generateRISInstruction(
                  TR::CodeGenerator       * cg,
                  TR::InstOpCode::Mnemonic           op,
                  TR::Node                * n,
                  TR::Register            * leftSide,
                  int8_t                   immCompare,
                  TR::MemoryReference * branch,
                  TR::InstOpCode::S390BranchCondition   mask,
                  TR::Instruction         * preced = 0);

/************************************************************ Vector Instructions ************************************************************/
/****** VRI ******/
TR::Instruction * generateVRIaInstruction(
                      TR::CodeGenerator    * cg          ,
                      TR::InstOpCode::Mnemonic          op          ,
                      TR::Node             * n           ,
                      TR::Register           * targetReg   ,
                      uint16_t                constantImm2,   /* 16 bits */
                      uint8_t                 mask3      );

TR::Instruction * generateVRIbInstruction(
                      TR::CodeGenerator    * cg          ,
                      TR::InstOpCode::Mnemonic          op          ,
                      TR::Node             * n           ,
                      TR::Register           * targetReg   ,   /*  8 or 16 bits */
                      uint8_t                 constantImm2,   /* 12 bits */
                      uint8_t                 constantImm3,   /*  4 bits */
                      uint8_t                 mask4       );

TR::Instruction * generateVRIcInstruction(
                      TR::CodeGenerator    * cg          ,
                      TR::InstOpCode::Mnemonic          op          ,
                      TR::Node             * n           ,
                      TR::Register           * targetReg   ,
                      TR::Register           * sourceReg3  ,   /* 8 or 16 bits */
                      uint16_t                constantImm2,   /* 4 bits       */
                      uint8_t                 mask4       );

TR::Instruction * generateVRIdInstruction(
                      TR::CodeGenerator    * cg          ,
                      TR::InstOpCode::Mnemonic          op          ,
                      TR::Node             * n           ,
                      TR::Register           * targetReg   ,
                      TR::Register           * sourceReg2  ,
                      TR::Register           * sourceReg3  ,
                      uint8_t                 constantImm4,   /* 8 bit  */
                      uint8_t                 mask5       );  /* 4 bits */

TR::Instruction * generateVRIeInstruction(
                      TR::CodeGenerator    * cg          ,
                      TR::InstOpCode::Mnemonic          op          ,
                      TR::Node             * n           ,
                      TR::Register           * targetReg   ,
                      TR::Register           * sourceReg2  ,
                      uint16_t                constantImm3,   /* 12 bits  */
                      uint8_t                 mask5       ,   /*  4 bits */
                      uint8_t                 mask4       );  /*  4 bits */


TR::Instruction * generateVRIfInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      TR::Register           * sourceReg3,
                      uint8_t                constantImm4,   /* 8 bits  */
                      uint8_t                 mask5   );     /* 4 bits */



TR::Instruction * generateVRIgInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      uint8_t                constantImm3,   /* 8 bits  */
                      uint8_t                constantImm4,   /* 8 bits  */
                      uint8_t                 mask5   );     /* 4 bits  */


TR::Instruction * generateVRIhInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      uint16_t               constantImm2,   /* 16 bits  */
                      uint8_t                constantImm3 ); /* 4 bits  */


TR::Instruction * generateVRIiInstruction(
                      TR::CodeGenerator    * cg,
                      TR::InstOpCode::Mnemonic   op,
                      TR::Node               * n,
                      TR::Register           * targetReg,
                      TR::Register           * sourceReg2,
                      uint8_t                constantImm3,   /* 8 bits  */
                      uint8_t                 mask4);        /* 4 bits  */

/****** VRR ******/
TR::Instruction * generateVRRaInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      uint8_t                  mask5      ,   /* 4 bits */
                      uint8_t                  mask4      ,   /* 4 bits */
                      uint8_t                  mask3      ,   /* 4 bits */
                      TR::Instruction     * preced     = NULL);

TR::Instruction * generateVRRaInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Instruction     * preced     = NULL);

TR::Instruction * generateVRRbInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Register            * sourceReg3 ,
                      uint8_t                  mask5      ,   /* 4 bits */
                      uint8_t                  mask4      );  /* 4 bits */

TR::Instruction * generateVRRcInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Register            * sourceReg3 ,
                      uint8_t                  mask6      ,   /* 4 bits */
                      uint8_t                  mask5      ,   /* 4 bits */
                      uint8_t                  mask4      );  /* 4 bits */

TR::Instruction * generateVRRcInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Register            * sourceReg3 ,
                      uint8_t                  mask4      );  /* 4 bits */

TR::Instruction * generateVRRdInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Register            * sourceReg3 ,
                      TR::Register            * sourceReg4 ,
                      uint8_t                  mask6 = 0  ,   /* 4 bits */
                      uint8_t                  mask5 = 0  );  /* 4 bits */

TR::Instruction * generateVRReInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,
                      TR::Register            * sourceReg3 ,
                      TR::Register            * sourceReg4 ,
                      uint8_t                  mask6 = 0  ,   /* 4 bits */
                      uint8_t                  mask5 = 0  );  /* 4 bits */

TR::Instruction * generateVRRfInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg2 ,   /* GPR */
                      TR::Register            * sourceReg3 );  /* GPR */

TR::Instruction * generateVRRgInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  );  /* Vector register */


TR::Instruction * generateVRRhInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,    /* Vector register */
                      TR::Register            * sourceReg2 ,    /* Vector register */
                      uint8_t                   mask3);         /* 4 bits*/


TR::Instruction * generateVRRiInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic  op         ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,    /* GPR */
                      TR::Register            * sourceReg2 ,    /* VRF */
                      uint8_t                   mask3      ,    /* 4 bits*/
                      uint8_t                   mask4 = 0);

/****** VRS ******/
TR::Instruction * generateVRSaInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4     ,   /* 4 bits */
                      TR::Instruction         * preced = NULL);

TR::Instruction * generateVRSbInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,   /* VRF */
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4 = 0  ,  /* 4 bits */
                      TR::Instruction         * preced = NULL);

TR::Instruction * generateVRScInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4      ,  /* 4 bits */
                      TR::Instruction         * preced = NULL);

TR::Instruction * generateVRSdInstruction(
                      TR::CodeGenerator       * cg         ,
                      TR::InstOpCode::Mnemonic op          ,
                      TR::Node                * n          ,
                      TR::Register            * targetReg  ,   /* VRF */
                      TR::Register            * sourceReg3 ,   /* GPR R3 */
                      TR::MemoryReference     * mr         ,
                      TR::Instruction         * preced = NULL);

/****** VRV ******/
TR::Instruction * generateVRVInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask3      ,  /* 4 bits */
                      TR::Instruction* preced = NULL);

/****** VRX ******/
TR::Instruction * generateVRXInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * reg        ,   /* VRF */
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask3  = 0 ,   /* 4 bits */
                      TR::Instruction     * preced = NULL);


/****** VSI ******/
TR::Instruction * generateVSIInstruction(
                      TR::CodeGenerator      * cg         ,
                      TR::InstOpCode::Mnemonic op         ,
                      TR::Node               * n          ,
                      TR::Register           * reg        ,   /* VRF */
                      TR::MemoryReference    * mr         ,
                      uint8_t                  imm3   = 0,    /* 8 bits */
                      TR::Instruction        * preced = NULL);

/************************************************************ Misc Instructions ************************************************************/
TR::Instruction *generateS390ImmSymInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   uint32_t                            imm,
                   TR::SymbolReference                 *sr,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateS390PseudoInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390PseudoInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   TR::RegisterDependencyConditions *cond,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = 0);
TR::Instruction *generateS390PseudoInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   int32_t regNum,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = 0);

TR::Instruction *generateS390PseudoInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   TR::RegisterDependencyConditions *cond,
                   int32_t regNum,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = 0);
TR::Instruction *generateS390DebugCounterBumpInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node * n,
                   TR::Snippet* cas,
                   int32_t d = 1,
                   TR::Instruction *preced = 0);

TR::Instruction *generateShiftRightImmediate(
                  TR::CodeGenerator *cg,
                  TR::Node *node,
                  TR::Register *trgReg,
                  TR::Register *srcReg,
                  int32_t imm,
                  TR::Instruction *preced = 0);

#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *generateVirtualGuardNOPInstruction(
                   TR::CodeGenerator      *cg,
                   TR::Node                            *n,
                   TR_VirtualGuardSite         *site,
                   TR::RegisterDependencyConditions *cond,
                   TR::LabelSymbol                      *sym,
                   TR::Instruction                     *preced = 0);

bool
canThrowDecimalOverflowException(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op);

void
generateS390DAAExceptionRestoreSnippet(TR::CodeGenerator* cg,
                                       TR::Node* n,
                                       TR::Instruction* instr,
                                       TR::InstOpCode::Mnemonic op,
                                       bool hasNOP);

#endif

//////////////////////////////////////////////////////////////////////////
//  Helper Routines
//////////////////////////////////////////////////////////////////////////


TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   int32_t                              imm,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                      *preced = 0,
                   TR::Register                         *base = 0,
                   bool                                isPICCandidate = false);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   uintptr_t                              imm,
                   int32_t                              reloType = 0,
                   TR::RegisterDependencyConditions *cond = 0,
                   TR::Instruction                      *preced = 0,
                   TR::Register                         *base = 0);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   TR::Snippet                      *snippet,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                      *preced = 0,
                   TR::Register                         *base = 0);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   int64_t                              imm,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                      *preced = 0,
                   TR::Register                         *base = 0,
                   bool                                isPICCandidate = false);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   uintptr_t                            imm,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                      *preced = 0,
                   TR::Register                         *base = 0,
                   bool                                isPICCandidate = false);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   float                                fmm,
                   TR::Instruction                      *preced = 0);

TR::Instruction *generateRegLitRefInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   double                               dmm,
                   TR::Instruction                      *preced = 0);

TR::Instruction *generateSnippetCall(
                   TR::CodeGenerator *cg,
                   TR::Node                             *n,
                   TR::Snippet                          *s,
                   TR::RegisterDependencyConditions *cond,
                   TR::SymbolReference                  *callSymRef,
                   TR::Instruction                      *preced = 0);


TR::Instruction *generateDirectCall(
                   TR::CodeGenerator *cg,
                   TR::Node                             *n,
                   bool                                myself,
                   TR::SymbolReference                  *callSymRef,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                      *preced = 0);

/** \brief
 *     Generates a data constant in the instruction stream.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param op
 *     The mnemonic of the data constant pseudo instruction.
 *
 *  \param node
 *     The node with which the generated instruction will be associated.
 *
 *  \param data
 *     The data constant to inline in the instruction stream.
 *
 *  \param preced
 *     The preceeding instruction to link the generated data constant instruction with.
 *
 *  \return
 *     The generated instruction.
 */
TR::Instruction* generateDataConstantInstruction(TR::CodeGenerator* cg, TR::InstOpCode::Mnemonic op, TR::Node* node, uint32_t data, TR::Instruction* preced = NULL);

TR::Instruction *generateRegUnresolvedSym(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                             *n,
                   TR::Register                         *treg,
                   TR::SymbolReference                  *symRef,
                   TR::UnresolvedDataSnippet        *uds,
                   TR::Instruction                      *preced = 0);

TR::Register * establishLiteralPoolAddressIfRequired(
         TR::CodeGenerator * cg,
         TR::Node * node);

TR::Instruction *generateLoadLiteralPoolAddress(
         TR::CodeGenerator        *cg,
         TR::Node                 *node,
         TR::Register             *treg);


TR::Instruction *generateEXDispatch(
        TR::Node * node,
        TR::CodeGenerator *cg,
        TR::Register * maskReg,
        TR::Register * useThisLitPoolReg,
        TR::Instruction * instr,
        TR::Instruction * preced = 0,
        TR::RegisterDependencyConditions *deps = NULL);

TR::Instruction *generateEXDispatch(
        TR::Node * node,
        TR::CodeGenerator *cg,
        TR::Register * maskReg,
        TR::Instruction * instr,
        TR::Instruction * preced = 0,
        TR::RegisterDependencyConditions *deps = NULL);

TR::Instruction *generateS390EInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic        op,
                   TR::Node               *n,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateS390EInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic        op,
                   TR::Node               *n,
                   TR::Register           * tgt,
                   TR::Register           * tgt2,
                   TR::Register           * src,
                   TR::Register           * src2,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction        *preced = NULL);

TR::Instruction * generateS390IInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   uint8_t          imm,
                   TR::Node          *n,
                   TR::Instruction   *preced = NULL);

TR::Instruction * generateSerializationInstruction(
                   TR::CodeGenerator *cg,
                   TR::Node          *n,
                   TR::Instruction   *preced = NULL);

TR::Instruction * generateS390IEInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   uint8_t          imm1,
                   uint8_t          imm2,
                   TR::Node          *n,
                   TR::Instruction   *preced = NULL);

TR::Instruction *generateRuntimeInstrumentationInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *node,
                   TR::Register      *target = NULL,
                   TR::Instruction   *preced = NULL);

TR::Instruction *
generateReplicateNodeInVectorReg(TR::Node * node, TR::CodeGenerator *cg, TR::Register * targetVRF, TR::Node * srcElementNode,
                                 int elementSize, TR::Register *zeroReg=NULL, TR::Instruction * preced=NULL);

/**
 * \param node
 *    The node being evaluated.
 *
 * \param cg
 *    The code generator used to generate the instructions.
 * 
 * \param targetRegister
 *    The register where the specific shifted bits from sourceRegister will be placed to the corresponding 
 *    location. The remaining bits will be zeroed out.
 * 
 * \param sourceRegister
 *    The register which will be shifted. The selected shifted bits in sourceRegister will be placed to 
 *    targetRegister, and the sourceRegister itself will stay unchanged. 
 * 
 * \param fromBit
 *    Indicates the starting bit position of the selected range of bits in targetRegister and in sourceRegister after shifting. 
 * 
 * \param toBit
 *    Indicates the ending bit position of the selected range of bits in targetRegister and in sourceRegister after shifting. 
 * 
 * \param shiftAmount
 *    Indicates the amount of bits that sourceRegister is shifted to the left. 
 * 
 * \example
 *    fromBit = 10;   toBit = 20;   shiftAmount = 15;
 * 
 *                              Bit 10    Bit 20
 *    Before:                     |          |
 *                     0          V          V                                               63
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+     
 *    sourceRegister: |10010000 00100011 01001000 11000100 00011010 00111000 00101011 00010000|
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+
 *      
 *                     0                                                                     63
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+     
 *    targetRegister: |10000110 01101010 00111000 11011110 11000011 01111000 00100011 00000000|
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+
 *    
 *    The function first makes a copy of sourceRegister into the targetRegister and then shifts the targetRegister left
 *    by the shiftAmount (15 bits):
 * 
 *                              Bit 10    Bit 20
 *                                |          |
 *                     0          V          V                                               63
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+     
 *    targetRegister: |10100100 01100010 00001101 00011100 00010101 10001000 00000000 00000000|
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+
 * 
 *    The function then selects the bits in rage [fromBit, toBit] inclusive of the shifted value and all other non-
 *    selected bits are zeroed out:
 * 
 *                              Bit 10    Bit 20
 *    After:                      |          |
 *                     0          V          V                                               63
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+     
 *    targetRegister: |00000000 00100010 00001000 00000000 00000000 00000000 00000000 00000000|
 *                    +--------+--------+--------+--------+--------+--------+--------+--------+
 *
 *    Note the value of sourceRegister will remain unchanged.
*/
void generateShiftThenKeepSelected64Bit(
      TR::Node * node, TR::CodeGenerator *cg,
      TR::Register * targetRegister, TR::Register * sourceRegister, int fromBit,
      int toBit, int shiftAmount);

void generateShiftThenKeepSelected31Bit(
      TR::Node * node, TR::CodeGenerator *cg,
      TR::Register * targetRegister, TR::Register * sourceRegister, int fromBit,
      int toBit, int shiftAmount);

TR::Instruction *generateZeroVector(TR::Node *node, TR::CodeGenerator *cg, TR::Register *vecZeroReg);

/** \brief
 *     Generates an alignment NOP (no-operation) pseudo-instruction in the instruction stream which will expand into
 *     an appropriate number of NOP instructions at binary encoding time to satisfy the specified alignment.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param node
 *     The node with which the generated instruction will be associated.
 *
 *  \param alignment
 *     The alignment at which the next instruction in the instruction stream will be encoded at. For example specifying
 *     a value of 16 will ensure that the binary encoding location of the subsequent instruction will be 16-byte
 *     aligned, i.e. the low order four bits will be zero.
 *
 *  \param preced
 *     The preceeding instruction to link the generated data constant instruction with.
 *
 *  \return
 *     The generated instruction.
 */
TR::Instruction*
generateAlignmentNopInstruction(TR::CodeGenerator *cg, TR::Node *node, uint32_t alignment, TR::Instruction *preced = NULL);

#endif
