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

#ifndef OMR_Power_TREE_EVALUATOR_INCL
#define OMR_Power_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace Power { class TreeEvaluator; } }
namespace OMR { typedef OMR::Power::TreeEvaluator TreeEvaluatorConnector; }
#else
#error OMR::Power::TreeEvaluator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRTreeEvaluator.hpp"

#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "runtime/Runtime.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::TreeEvaluator
   {

   public:

   static TR::Register *BadILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *floadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *astoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *istoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *areturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *athrowEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *callEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *baddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *saddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *smulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iudivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ludivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iuremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *snegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *borEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifscmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifsucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *MethodEnterHookEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *MethodExitHookEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *PassThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // mask evaluators
   static TR::Register *mAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mmAnyTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mmAllTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mTrueCountEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mFirstTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mLastTrueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mToLongBitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mLongBitsToMaskEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *b2mEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2mEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2mEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2mEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *v2mEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *m2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *m2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *m2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *m2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *m2vEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // vector evaluators
   static TR::Register *vabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vbitselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vconvEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *vmabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmfmaEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmindexVectorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmloadiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmnotEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmorUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionAddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionAndEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionFirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionMaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionMinEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionMulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionOrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionOrUncheckedEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmreductionXorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmstoreiEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmfirstNonZeroEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *vpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vexpandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vrolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmrolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *mcompressEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmbitswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmcompressbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmexpandbitsEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   // vector evaluator helpers
   static TR::Register *vgetelemDirectMoveHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vgetelemMemoryMoveHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulInt8Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulInt16Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulInt32Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulInt64Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivInt32Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegIntHelper(TR::Node *node, TR::CodeGenerator *cg, TR::DataType type);
   static TR::Register *vnegFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vabsIntHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic shift, TR::InstOpCode::Mnemonic add);
   static TR::Register *visetelemHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdsetelemHelper(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *f2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2luEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2buEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2luEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2buEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newvalueEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *anewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *variableNewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *variableNewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *multianewarrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *contigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *discontigarraylengthEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acalliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *calliEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *luaddhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lusubhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lumulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *CaseEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ResolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ResolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *OverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *UnsignedOverflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKwithSpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *SpineCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *allocationFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *loadFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *storeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fullFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *butestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sutestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmnnoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iuaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *luaddcEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lusubbEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bztestnsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *isatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iiatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilatomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *branchEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ffloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dceilEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fceilEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lumaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iuminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *luminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ihbitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilbitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inolzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inotzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ipopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lhbitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *llbitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lnolzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lnotzEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lpopcntEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lbitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *floadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg); // ibm@59591
   static TR::Register *istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);  // ibm@59591
   static TR::Register *dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *returnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *laddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *faddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *daddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *isubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dualMulHelper64(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulHelper32(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulEvaluator(TR::Node * node, TR::CodeGenerator *cg);
   static TR::Register *lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *labsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ishrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sshrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *irolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lrolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *landEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *candEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *corEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ificmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifiucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifiucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifiucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifiucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *scmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tableEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gprRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gprRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *setmemoryEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool arrayCheckNeeded);
   static TR::Register *arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmplenEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareIntsForEquality(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareIntsForEquality(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node,
                                              TR::CodeGenerator *cg, bool isHint=false, bool likeliness=false);
   static TR::Register *compareIntsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::Node *node, TR::CodeGenerator *cg,
                                           bool isSigned);
   static TR::Register *compareIntsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node,
                                           TR::CodeGenerator *cg, bool isSigned, bool isHint=false, bool likeliness=false);
   static TR::Register *compareLongsForEquality(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *compareLongsForEquality(TR::InstOpCode::Mnemonic branchOp, TR::LabelSymbol *dstLabel, TR::Node *node,
                                               TR::CodeGenerator *cg, bool isHint=false, bool likeliness=false);
   static TR::Register *compareLongsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::Mnemonic reversedBranchOp, TR::Node *node, TR::CodeGenerator *cg,
                                            bool isSigned);
   static TR::Register *compareLongsForOrder(TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::Mnemonic reversedBranchOp, TR::LabelSymbol *dstLabel,
                                            TR::Node *node, TR::CodeGenerator *cg, bool isSigned, bool isHint=false, bool likeliness=false);

   static TR::Register *cmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *integerBitCount(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *longBitCount(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *conditionalHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *flushEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineVectorUnaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorBitSelectOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *acmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *maxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *minEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *strcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *strcFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *xfRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *xdRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node *node, bool needsResolve, TR::CodeGenerator *cg);
   static TR::Instruction *generateNullTestInstructions(
         TR::CodeGenerator *cg,
         TR::Register *trgReg,
         TR::Node *node,
         bool nullPtrSymRefRequired = false);

   // Common evaluator routines
   static void postSyncConditions(
         TR::Node *node,
         TR::CodeGenerator *cg,
         TR::Register *dataReg,
         TR::MemoryReference *memRef,
         TR::InstOpCode::Mnemonic syncOp,
         bool lazyVolatile = false);
   static TR::Register *performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg);

   // VM dependent routines

   static void preserveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);
   static void restoreTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);
   static TR::Register *retrieveTOCRegister(TR::Node *node, TR::CodeGenerator *cg, TR::RegisterDependencyConditions *dependencies);

   static bool stopUsingCopyReg(TR::Node* node, TR::Register*& reg, TR::CodeGenerator* cg);

   static TR::Instruction* generateHelperBranchAndLinkInstruction(
         TR_RuntimeHelper helperIndex,
         TR::Node* node,
         TR::RegisterDependencyConditions* conditions,
         TR::CodeGenerator* cg);

   private:

   static TR::Register *int2dbl(
         TR::Node * node,
         TR::Register *srcReg,
         bool canClobber,
         TR::CodeGenerator *cg);

   static TR::Register *long2dbl(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *long2float(TR::Node *node, TR::CodeGenerator *cg);

   };

}
}

#endif
