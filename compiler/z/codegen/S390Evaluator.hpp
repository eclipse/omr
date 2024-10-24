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

#ifndef TR_S390_TREE_EVALUATOR_INCL
#define TR_S390_TREE_EVALUATOR_INCL


#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "codegen/TreeEvaluator.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class RegisterPair; }
namespace TR { class TreeEvaluator; }


TR::Register *inlineShortReverseBytes(TR::Node *node, TR::CodeGenerator *cg);
TR::Register *inlineIntegerReverseBytes(TR::Node *node, TR::CodeGenerator *cg);
TR::Register *inlineLongReverseBytes(TR::Node *node, TR::CodeGenerator *cg);

TR::Register *
generateExtendedFloatConstantReg(TR::Node *node, TR::CodeGenerator *cg, int64_t constHi, int64_t constLo,
                                 TR::RegisterDependencyConditions ** depsPtr = NULL);

TR::Instruction* generateS390ImmOp(TR::CodeGenerator *cg,
                TR::InstOpCode::Mnemonic memOp,
                TR::Node *node,
                TR::Register *targetRegister,
                float value,
                TR::RegisterDependencyConditions *cond = 0,
                TR::Instruction * preced = 0);

TR::Instruction* generateS390ImmOp(TR::CodeGenerator *cg,
                TR::InstOpCode::Mnemonic memOp,
                TR::Node *node,
                TR::Register *targetRegister,
                double value,
                TR::RegisterDependencyConditions *cond = 0,
                TR::Instruction * preced = 0);

TR::Instruction* generateS390ImmOp(TR::CodeGenerator *cg,
                TR::InstOpCode::Mnemonic memOp,
                TR::Node *node,
                TR::Register *sourceRegister,
                TR::Register *targetRegister,
                int64_t value,
                TR::RegisterDependencyConditions *cond = 0,
                TR::Register * base=0,
                TR::Instruction * preced = 0);

TR::Instruction* generateS390ImmOp(TR::CodeGenerator *cg,
                TR::InstOpCode::Mnemonic memOp,
                TR::Node *node,
                TR::Register *srcRegister,
                TR::Register *targetRegister,
                int32_t value,
                TR::RegisterDependencyConditions *cond = 0,
                TR::Register * base=0,
                TR::Instruction * preced = 0);

TR::Instruction* generateS390ImmToRegister(TR::CodeGenerator * cg,
                TR::Node * node,
                TR::Register * targetRegister,
                intptr_t value,
                TR::Instruction * cursor);

/** \brief
 *     Generates instructions to materialize (load) a 32-bit constant value into a virtual register.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param node
 *     The node to which the generated instructions will be associated with.
 *
 *  \param value
 *     The value to load into \p targetRegister.
 *
 *  \param targetRegister
 *     The virtual register into which to load the \p value.
 *
 *  \param canSetConditionCode
 *     Determines whether the generated instructions are allowed set any condition code.
 *
 *  \param cursor
 *     The cursor instruction to append all generated instructions to.
 *
 *  \param dependencies
 *     The register dependency conditions to which newly allocated registers will be added to if they are needed. All new
 *     allocated registers will be appended as post conditions to \p cond with an AssignAny condition.
 *
 *  \param literalPoolRegister
 *     The literal pool register to use if the constant \p value needs to be stored in a literal pool entry.
 *
 *  \return
 *     The pointer to the last generated instruction to load the supplied \p value.
 */
TR::Instruction* generateLoad32BitConstant(TR::CodeGenerator* cg, TR::Node* node, int32_t value, TR::Register* targetRegister, bool canSetConditionCode, TR::Instruction* cursor = NULL, TR::RegisterDependencyConditions* dependencies = NULL, TR::Register* literalPoolRegister = NULL);

TR::Instruction * genLoadLongConstant(TR::CodeGenerator *cg, TR::Node *node, int64_t value, TR::Register *targetRegister, TR::Instruction *cursor=NULL,TR::RegisterDependencyConditions *cond = 0, TR::Register *base = 0);
TR::Instruction * genLoadAddressConstant(TR::CodeGenerator *cg, TR::Node *node, uintptr_t value, TR::Register *targetRegister, TR::Instruction *cursor=NULL,TR::RegisterDependencyConditions *cond = 0, TR::Register *base = 0);
TR::Instruction * genLoadAddressConstantInSnippet(TR::CodeGenerator *cg, TR::Node *node, uintptr_t value, TR::Register *targetRegister, TR::Instruction *cursor=NULL,TR::RegisterDependencyConditions *cond = 0, TR::Register *base = 0, bool isPICCandidate=false);
TR::Instruction * genLoadProfiledClassAddressConstant(TR::CodeGenerator *cg, TR::Node *node, TR_OpaqueClassBlock *clazz, TR::Register *targetRegister, TR::Instruction *cursor = 0,TR::RegisterDependencyConditions *cond = 0, TR::Register *base = 0);

TR::MemoryReference * sstoreHelper(TR::Node * node, TR::CodeGenerator * cg, bool isReversed=false);
TR::MemoryReference * istoreHelper(TR::Node * node, TR::CodeGenerator * cg, bool isReversed=false);
TR::MemoryReference * lstoreHelper64(TR::Node * node, TR::CodeGenerator * cg, bool isReversed=false);

TR::Register * iloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, bool isReversed=false);
TR::Register * lloadHelper64(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, bool isReversed=false);
TR::Register * sloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, bool isReversed=false);

TR::Register *getLitPoolBaseReg(TR::Node *node, TR::CodeGenerator * cg);

enum LoadForm
   {
   RegReg,
   MemReg
   // Could also add other forms like the Load-and-Add, Load and test, load high, load immediate, etc.
   };





template <uint32_t numberOfBits>
TR::Register * genericLoad(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister);

template <uint32_t numberOfBits, uint32_t numberOfExtendBits, enum LoadForm form>
#if defined(__IBMCPP__) || defined(__ibmxl__) || defined(__open_xl__)
inline
#endif
TR::Register * genericLoadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend);

#ifndef _MSC_VER
//Use of above templates is static to the TreeEvaluator.cpp object. Here are the template instantiations available to other objects:
extern template TR::Register * genericLoadHelper<32, 32, MemReg>
      (TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend);
extern template TR::Register * genericLoadHelper<64, 64, MemReg>
      (TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend);
#endif

TR::Instruction * generateLoadLiteralPoolAddress(TR::CodeGenerator * cg, TR::Node * node, TR::Register * treg);

TR::Register * generateS390CompareBool(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition fBranchOpCond,
   TR::InstOpCode::S390BranchCondition rBranchOpCond, bool isUnorderedOK = false);
TR::Register * generateS390CompareBranch(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition fBranchOpCond,
   TR::InstOpCode::S390BranchCondition rBranchOpCond, bool isUnorderedOK = false);


int32_t getVectorElementSize(TR::Node *node);
int32_t getVectorElementSizeMask(TR::Node *node);
int32_t getVectorElementSizeMask(int8_t size);

#ifndef _MSC_VER
// Because these templates are not defined in headers, but the cpp TreeEvaluator.cpp, suppress implicit instantiation
// using these extern definitions. Unfortunately, Visual Studio likes doing things differently, so for VS, this chunk
// is copied to OMRTreeEvaluator.cpp
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,   8>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false,  8>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,  16>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false, 16>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,  32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false, 32>(TR::Node * node, TR::CodeGenerator * cg);

extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,   8, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,   8, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false,  8, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false,  8, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  16, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  16, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 16, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 16, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  32, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  32, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 32, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 32, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  64, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 64, 64>(TR::Node * node, TR::CodeGenerator * cg);

extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator< 8, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator< 8, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<16, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<16, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<32, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<32, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<64, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<64, false>(TR::Node * node, TR::CodeGenerator * cg);
#endif

TR::Register *getConditionCode(TR::Node *node, TR::CodeGenerator *cg, TR::Register *programRegister = NULL);
TR::RegisterDependencyConditions *getGLRegDepsDependenciesFromIfNode(TR::CodeGenerator *cg, TR::Node* ificmpNode);

class TR_S390ComputeCC : public TR::TreeEvaluator
   {
   public:
   static bool setCarryBorrow(TR::Node *flagNode, bool invertValue, TR::CodeGenerator *cg);
   static void computeCC(TR::Node *node, TR::Register *ccReg, TR::CodeGenerator *cg);
   static void computeCCLogical(TR::Node *node, TR::Register *ccReg, TR::Register *targetReg, TR::CodeGenerator *cg, bool is64Bit=false);
   private:
   static void saveHostCC(TR::Node *node, TR::Register *ccReg, TR::CodeGenerator *cg);
   };

TR::InstOpCode::S390BranchCondition getStandardIfBranchConditionForArraycmp(TR::Node * ifxcmpXXNode, TR::CodeGenerator *cg);

TR::InstOpCode::S390BranchCondition getStandardIfBranchCondition(TR::ILOpCodes opCode, int64_t compareVal);
TR::InstOpCode::S390BranchCondition getButestBranchCondition(TR::ILOpCodes opCode, int32_t compareVal);

bool canUseNodeForFusedMultiply(TR::Node *node);
bool generateFusedMultiplyAddIfPossible(TR::CodeGenerator *cg, TR::Node *addNode, TR::InstOpCode::Mnemonic op, TR::InstOpCode::Mnemonic negateOp = TR::InstOpCode::bad);

#endif
