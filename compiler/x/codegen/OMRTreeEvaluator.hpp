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

#ifndef OMR_X86_TREE_EVALUATOR_INCL
#define OMR_X86_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace X86 { class TreeEvaluator; } }
namespace OMR { typedef OMR::X86::TreeEvaluator TreeEvaluatorConnector; }
#endif

#include "compiler/codegen/OMRTreeEvaluator.hpp"

#include <stddef.h>                       // for NULL
#include <stdint.h>                       // for int32_t, etc
#include "codegen/RegisterConstants.hpp"
#include "env/jittypes.h"                 // for intptrj_t
#include "il/ILOpCodes.hpp"               // for ILOpCodes
#include "runtime/Runtime.hpp"
#include "x/codegen/X86Ops.hpp"

namespace TR { class X86MemInstruction;         }
namespace TR { class X86RegImmInstruction;      }
namespace TR { class X86RegInstruction;         }
class TR_X86ScratchRegisterManager;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class SymbolReference; }

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::TreeEvaluator
   {

   public:

   static TR::Register *ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *floadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tableEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *returnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iternaryEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *faddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *daddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *baddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *saddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *caddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *csubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *snegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *candEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *borEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *corEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *c2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareFloatAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareDoubleAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareFloatAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareDoubleAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *butestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sutestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *atomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareFloatEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareDoubleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *overflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *minmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *zccAddSubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   typedef TR::Register *(* EvaluatorComputesCarry)(TR::Node *node, TR::CodeGenerator *codeGen, bool computesCarry);
   // routines for integers (or addresses) that can fit in one register
   // (see also the integerPair*Evaluator functions)
   static TR::Register *integerStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerMulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerDualMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerAddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerSubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerDualAddOrSubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerDivOrRemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerNegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerAbsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerShrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerShlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerRolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerUshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerIfCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerIfCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerIfCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerIfCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unsignedIntegerIfCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   
   static TR::Register *tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   // routines for both float and double
   static TR::Register *fpReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fpRemEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // routines for floating point values that can fit in one GPR
   static TR::Register *floatingPointStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // For ILOpCode that can be translated to single SSE/AVX instructions
   static TR::Register *FloatingPointAndVectorBinaryArithmeticEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // SIMD evaluators
   static TR::Register *SIMDRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SIMDRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SIMDloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SIMDstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SIMDsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SIMDgetvelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *icmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bztestnsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // VM dependent routines
   static TR::Register *VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmergenewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMarrayStoreCheckArrayCopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static bool VMinlineCallEvaluator(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg);
   static TR::Instruction *VMtestForReferenceArray(TR::Node *, TR::Register *objectReg, TR::CodeGenerator *cg);
   static bool genNullTestSequence(TR::Node *node, TR::Register *opReg, TR::Register *targetReg, TR::CodeGenerator *cg);
   static TR::Register *VMifArrayCmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Instruction *insertLoadConstant(TR::Node *node,
                                             TR::Register *target,
                                             intptrj_t value,
                                             TR_RematerializableTypes type,
                                             TR::CodeGenerator *cg,
                                             TR::Instruction *currentInstruction = NULL);

   static TR::Instruction *insertLoadMemory(TR::Node *node,
                                           TR::Register *target,
                                           TR::MemoryReference *sourceMR,
                                           TR_RematerializableTypes type,
                                           TR::CodeGenerator *cg,
                                           TR::Instruction *currentInstruction = NULL);

   static void padUnresolvedDataReferences(
      TR::Node            *node,
      TR::SymbolReference &symRef,
      TR::CodeGenerator   *cg);

   static void insertPrecisionAdjustment(TR::Register *reg, TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *coerceFPRToXMMR(TR::Node *node, TR::Register *fpRegister, TR::CodeGenerator *cg);
   static TR::Register *coerceXMMRToFPR(TR::Node *node, TR::Register *fpRegister, TR::CodeGenerator *cg);
   static void coerceFPOperandsToXMMRs(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *constLengthArrayCopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   enum
      {
      logicalRegRegOp     = 0,
      logicalRegMemOp     = 1,
      logicalCopyOp       = 2,
      logicalByteImmOp    = 3,
      logicalImmOp        = 4,
      logicalByteMemImmOp = 5,
      logicalMemImmOp     = 6,
      logicalMemRegOp     = 7,
      logicalNotOp        = 8,

      numLogicalOpForms,
      lastLogicalOpForm = numLogicalOpForms-1,
      };

   static uint8_t shiftMask(bool is64Bit){ return is64Bit ? 0x3f : 0x1f; }

   // zEmulator int and long rotates are also always masked by 0x3f but it is correct to mask int rotates by 0x1f
   // as the part of the rotate above 31 is a nop essentially and should be masked off
   static uint8_t rotateMask(bool is64Bit){ return is64Bit? 0x3f : 0x1f; }

   static bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg);

   static intptrj_t integerConstNodeValue(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Block *getOverflowCatchBlock(TR::Node *node, TR::CodeGenerator *cg);

   static void genArithmeticInstructionsForOverflowCHK(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *performCall(TR::Node *node, bool isIndirect, bool spillFPRegs, TR::CodeGenerator *cg);

   static TR::Register *loadConstant(TR::Node *node, intptrj_t value, TR_RematerializableTypes t, TR::CodeGenerator *cg, TR::Register *targetRegister = NULL);

   protected:

   static TR::Register *performSimpleAtomicMemoryUpdate(TR::Node* node, TR_X86OpCodes op, TR::CodeGenerator* cg);
   static TR::Register *performHelperCall(TR::Node *node, TR::SymbolReference *helperSymRef, TR::ILOpCodes helperCallOpCode, bool spillFPRegs, TR::CodeGenerator *cg);
   static TR::Register *performIload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg);
   static TR::Register *performFload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg);
   static TR::Register *performDload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg);
   static TR::Register *negEvaluatorHelper(TR::Node *node, TR_X86OpCodes RegInstr, TR::CodeGenerator *cg);
   static TR::Register *logicalEvaluator(TR::Node *node, TR_X86OpCodes package[], TR::CodeGenerator *cg);
   static TR::Register *fpBinaryArithmeticEvaluator(TR::Node *node, bool isFloat, TR::CodeGenerator *cg);
   static TR::Register *bcmpEvaluator(TR::Node *node, TR_X86OpCodes setOp, TR::CodeGenerator *cg);
   static TR::Register *cmp2BytesEvaluator(TR::Node *node, TR_X86OpCodes setOp, TR::CodeGenerator *cg);

   static TR::Register *loadMemory(TR::Node *node, TR::MemoryReference  *sourceMR, TR_RematerializableTypes type, bool markImplicitExceptionPoint, TR::CodeGenerator *cg);
   static TR::Register *conversionAnalyser(TR::Node *node, TR_X86OpCodes memoryToRegisterOp, TR_X86OpCodes registerToRegisterOp, TR::CodeGenerator *cg);
   static TR::Register *fpConvertToInt(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg);
   static TR::Register *fpConvertToLong(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg);
   static TR::Register *generateBranchOrSetOnFPCompare(TR::Node *node, TR::Register *accRegister, bool generateBranch, TR::CodeGenerator *cg);
   static TR::Register *generateFPCompareResult(TR::Node *node, TR::Register *accRegister, TR::CodeGenerator *cg);
   static bool canUseFCOMIInstructions(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareFloatOrDoubleForOrder(TR::Node *, TR_X86OpCodes, TR_X86OpCodes, TR_X86OpCodes, TR_X86OpCodes, TR_X86OpCodes, bool, TR::CodeGenerator *);
   static void removeLiveDiscardableStatics(TR::CodeGenerator *cg);

   static bool analyseAddForLEA(TR::Node *node, TR::CodeGenerator *cg);
   static bool analyseSubForLEA(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *signedIntegerDivOrRemAnalyser(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluatorHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::X86MemInstruction *generateMemoryShift(TR::Node *node, TR_X86OpCodes immShiftOpCode, TR_X86OpCodes regShiftOpCode, TR::CodeGenerator *cg);
   static TR::X86RegInstruction *generateRegisterShift(TR::Node *node, TR_X86OpCodes immShiftOpCode, TR_X86OpCodes regShiftOpCode,TR::CodeGenerator *cg);
   static TR::Instruction *compareGPMemoryToImmediate(TR::Node *node, TR::MemoryReference  *mr, int32_t value, TR::CodeGenerator *cg);
   static void compareGPRegisterToImmediate(TR::Node *node, TR::Register *cmpRegister, int32_t value, TR::CodeGenerator *cg);
   static void compareGPRegisterToImmediateForEquality(TR::Node *node, TR::Register *cmpRegister, int32_t value, TR::CodeGenerator *cg);
   static void compareGPRegisterToConstantForEquality(TR::Node * node, int32_t value, TR::Register *cmpRegister, TR::CodeGenerator *cg);
   static void setupProfiledGuardRelocation(TR::X86RegImmInstruction *cmpInstruction, TR::Node *node, TR_ExternalRelocationTargetKind reloKind);
   static void compareIntegersForEquality(TR::Node *node, TR::CodeGenerator *cg);
   static void compareIntegersForOrder(TR::Node *node, TR::CodeGenerator *cg);
   static void compareIntegersForOrder(TR::Node *node, TR::Node *firstChild, TR::Node *secondChild, TR::CodeGenerator *cg);
   static void compareBytesForOrder(TR::Node *node, TR::CodeGenerator *cg);
   static void compare2BytesForOrder(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerEqualityHelper(TR::Node *node, TR_X86OpCodes setOp, TR::CodeGenerator *cg);
   static TR::Register *integerOrderHelper(TR::Node *node, TR_X86OpCodes setOp, TR::CodeGenerator *cg);
   static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needResolution, TR::CodeGenerator *cg);
   static TR::Register *commonFPRemEvaluator( TR::Node *node, TR::CodeGenerator *cg, bool isDouble);
   static TR::Register *generateLEAForLoadAddr(TR::Node *node, TR::MemoryReference *memRef, TR::SymbolReference *symRef,TR::CodeGenerator *cg, bool isInternalPointer);

   static bool constNodeValueIs32BitSigned(TR::Node *node, intptrj_t *value, TR::CodeGenerator *cg);

   static TR::Register *intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg);

   enum
      {
      bandOpPackage = 0,
      candOpPackage = 1,
      sandOpPackage = 1,
      iandOpPackage = 2,
      landOpPackage = 3,
      borOpPackage  = 4,
      corOpPackage  = 5,
      sorOpPackage  = 5,
      iorOpPackage  = 6,
      lorOpPackage  = 7,
      bxorOpPackage = 8,
      cxorOpPackage = 9,
      sxorOpPackage = 9,
      ixorOpPackage = 10,
      lxorOpPackage = 11,

      numLogicalOpPackages,
      lastOpPackage = numLogicalOpPackages-1
      };

   static TR_X86OpCodes _logicalOpPackage[numLogicalOpPackages][numLogicalOpForms];

   static bool generateIAddOrSubForOverflowCheck(TR::Node *compareNode, TR::CodeGenerator *cg);
   static bool generateLAddOrSubForOverflowCheck(TR::Node *compareNode, TR::CodeGenerator *cg);

   static TR::Register *SSE2ArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SSE2ArraycmpLenEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SSE2IfArraycmpEvaluator(TR::Node *node, TR::LabelSymbol *targetLabel, bool branchonEqual, TR::CodeGenerator *cg);

   static bool stopUsingCopyRegAddr(TR::Node *node, TR::Register*& reg, TR::CodeGenerator *cg);
   static bool stopUsingCopyRegInteger(TR::Node *node, TR::Register*& reg, TR::CodeGenerator *cg);

   };

}
}

#endif
