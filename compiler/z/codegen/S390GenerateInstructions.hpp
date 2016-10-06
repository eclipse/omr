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

#ifndef GENERATEINSTRUCTION_INCL
#define GENERATEINSTRUCTION_INCL

#include <stddef.h>                // for NULL
#include <stdint.h>                // for uint8_t, uint32_t, int8_t, etc
#include "codegen/InstOpCode.hpp"  // for InstOpCode, InstOpCode::Mnemonic, etc
#include "codegen/Snippet.hpp"     // for Snippet
#include "env/jittypes.h"          // for uintptrj_t
#include "il/Symbol.hpp"           // for Symbol, etc

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

TR::Instruction * generateS390CompareAndBranchInstruction(
                   TR::CodeGenerator * cg,
                   TR::InstOpCode::Mnemonic compareOpCode,
                   TR::Node * node,
                   TR::Register * first,
                   int32_t second,
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
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                 constForMR,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRXEInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::MemoryReference *mf,
                   uint8_t                mask3,
                   TR::Instruction     *preced = 0);

TR::Instruction * generateRXYInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                 *n,
                   TR::Register            *treg,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRXYbInstruction(
                   TR::CodeGenerator        *cg,
                   TR::InstOpCode::Mnemonic      op,
                   TR::Node                 *n,
                   uint8_t                   mask,
                   TR::MemoryReference    *mf,
                   TR::Instruction          *preced = 0);

TR::Instruction * generateRXYInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                 *n,
                   TR::RegisterPair         *regp,
                   TR::MemoryReference *mf,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRXYInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                 constForMR,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRXYbInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic     op,
                   TR::Node                *n,
                   uint8_t                  mask,
                   uint32_t                 constForMR,
                   TR::Instruction         *preced = 0);

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
                   char                   *data,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::SymbolReference     *sr,
                   uintptrj_t              imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::LabelSymbol          *label,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uintptrj_t              imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateRILInstruction(
                   TR::CodeGenerator       *cg,
                   TR::InstOpCode::Mnemonic         op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   TR::Snippet             *ts,
                   TR::Instruction         *preced);

TR::Instruction * generateSIInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::MemoryReference *mf,
                   uint32_t                imm,
                   TR::Instruction         *preced = 0);

TR::Instruction * generateSIYInstruction(
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

TR::Instruction * generateRSYInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node                *n,
                   TR::Register            *treg,
                   uint32_t                mask,
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

/****** VRS ******/
TR::Instruction * generateVRSaInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4      );   /* 4 bits */

TR::Instruction * generateVRSbInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,   /* VRF */
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4 = 0  );  /* 4 bits */

TR::Instruction * generateVRScInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * targetReg  ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask4      );  /* 4 bits */

/****** VRV ******/
TR::Instruction * generateVRVInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * sourceReg  ,
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask3      );  /* 4 bits */

/****** VRX ******/
TR::Instruction * generateVRXInstruction(
                      TR::CodeGenerator     * cg         ,
                      TR::InstOpCode::Mnemonic           op         ,
                      TR::Node              * n          ,
                      TR::Register            * reg        ,   /* VRF */
                      TR::MemoryReference * mr         ,
                      uint8_t                  mask3  = 0 ,   /* 4 bits */
                      TR::Instruction     * preced = NULL);

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
TR::Instruction *generateAndImmediate(
                   TR::CodeGenerator *cg,
                   TR::Node *node,
                   TR::Register *reg,
                   int32_t imm);

TR::Instruction *generateLogicalImmediate(
                   TR::CodeGenerator *cg,
                   TR::Node *node,
                   TR::InstOpCode::Mnemonic defaultOp,
                   TR::InstOpCode::Mnemonic lhOp,
                   TR::InstOpCode::Mnemonic llOp,
                   TR::Register *reg, int32_t imm);

TR::Instruction *generateOrImmediate(
                   TR::CodeGenerator *cg,
                   TR::Node *node,
                   TR::Register *reg,
                   int32_t imm);

#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *generateVirtualGuardNOPInstruction(
                   TR::CodeGenerator      *cg,
                   TR::Node                            *n,
                   TR_VirtualGuardSite         *site,
                   TR::RegisterDependencyConditions *cond,
                   TR::LabelSymbol                      *sym,
                   TR::Instruction                     *preced = 0);
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
                   uintptrj_t                              imm,
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
                   uintptrj_t                            imm,
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

TR::Instruction *
splitBaseRegisterIfNeeded(TR::MemoryReference *mf1,
                          TR::MemoryReference *mf2,
                          TR::CodeGenerator *cg,
                          TR::Node *node,
                          TR::Instruction *preced);

TR::Instruction *generateRuntimeInstrumentationInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node          *node,
                   TR::Register      *target = NULL,
                   TR::Instruction   *preced = NULL);

TR::Instruction *generateExtendedHighWordInstruction(
                   TR::Node * node,
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Register * targetReg,
                   TR::Register * srcReg,
                   int8_t imm8,
                   TR::Instruction *preced = 0);

TR::Instruction *
generateReplicateNodeInVectorReg(TR::Node * node, TR::CodeGenerator *cg, TR::Register * targetVRF, TR::Node * srcElementNode,
                                 int elementSize, TR::Register *zeroReg=NULL, TR::Instruction * preced=NULL);

void generateShiftAndKeepSelected64Bit(
      TR::Node * node, TR::CodeGenerator *cg,
      TR::Register * aFirstRegister, TR::Register * aSecondRegister, int aFromBit,
      int aToBit, int aShiftAmount, bool aClearOtherBits, bool aSetConditionCode);

void generateShiftAndKeepSelected31Bit(
      TR::Node * node, TR::CodeGenerator *cg,
      TR::Register * aFirstRegister, TR::Register * aSecondRegister, int aFromBit,
      int aToBit, int aShiftAmount, bool aClearOtherBits, bool aSetConditionCode);

TR::Instruction *generateZeroVector(TR::Node *node, TR::CodeGenerator *cg, TR::Register *vecZeroReg);
#ifdef DEBUG
#define TRACE_EVAL
#endif
#endif
