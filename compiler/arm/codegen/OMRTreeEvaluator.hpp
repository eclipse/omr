/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef OMR_ARM_TREE_EVALUATOR_INCL
#define OMR_ARM_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace ARM { class TreeEvaluator; } }
namespace OMR { typedef OMR::ARM::TreeEvaluator TreeEvaluatorConnector; }
#else
#error OMR::ARM::TreeEvaluator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRTreeEvaluator.hpp"

#include "arm/codegen/InstOpCode.hpp"
#include "codegen/ARMOps.hpp"
#include "codegen/ARMConditionCode.hpp"
#include "il/ILOps.hpp"
#include "runtime/Runtime.hpp"

namespace OMR { class MemoryReference; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }

namespace OMR
{

namespace ARM
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::TreeEvaluator
   {
public:
   static TR::Register *badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *cloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ireturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *returnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if J9_PROJECT_SPECIFIC
   static TR::Register *instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *laddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *faddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *daddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *baddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *saddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *caddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *isubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ssubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *csubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *smulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ldivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *snegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *labsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ishlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cshlEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *bandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *candEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *borEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *corEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ixorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *iflucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *iffcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
   static TR::Register *iffcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iffcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
   static TR::Register *ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
   static TR::Register *fcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
   static TR::Register *dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *acmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
   static TR::Register *fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *maxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *minEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   static TR::Register *fmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   static TR::Register *NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *tableEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *DIVCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCopyBNDCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayStoreCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ArrayCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *commonLoadEvaluator(TR::Node *node, TR_ARMOpCodes memoryToRegisterOp, int32_t memSize, TR::CodeGenerator *cg);
   static TR::Register *commonStoreEvaluator(TR::Node *node, TR_ARMOpCodes memToRegOp, int32_t memSize, TR::CodeGenerator *cg);
   static TR::Register *commonConstEvaluator(TR::Node *node, int32_t value, TR::CodeGenerator *cg);
#if J9_PROJECT_SPECIFIC
   static void        VMwrtbarEvaluator(TR::Node *node, TR::Register *srcReg, TR::Register *dstReg, TR::Register *flagReg, bool needDeps, TR::CodeGenerator *cg);
   static TR::Register *VMmonentEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMmonexitEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif
   // VM-dependent routines
#if J9_PROJECT_SPECIFIC
   static TR::Register *VMcheckcastEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMifInstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMinstanceOfEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *VMnewEvaluator(TR::Node *node, TR::CodeGenerator *cg);
#endif


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

#ifdef J9_PROJECT_SPECIFIC
   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *dstReg, TR::Register *srcReg, TR::Instruction *prev=0);
   static TR::Instruction *generateVFTMaskInstruction(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::Instruction *prev=0);
#endif

   static bool stopUsingCopyReg(TR::Node* node, TR::Register*& reg, TR::CodeGenerator* cg);

   static TR::Instruction *compareIntsForEquality(TR_ARMConditionCode  branchType,
                                              TR::Node             *node,
                                              TR::CodeGenerator    *cg,
                                              TR::SymbolReference  *sr = NULL);
   static TR::Register *performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg);

   protected:
   static TR::Register *conversionAnalyser(TR::Node          *node,
                                          TR_ARMOpCodes    memoryToRegisterOp,
                                          bool needSignExtend,
                                          int dstBits,
                                          TR::CodeGenerator *cg);

   };

}
}

static void constLengthArrayCopyEvaluator(TR::Node *node, int32_t byteLen, TR::Register *src, TR::Register *dst, TR::CodeGenerator *cg);
static void generateSignOrZeroExtend(TR::Node *node, TR::Register *dst, TR::Register *src, bool needSignExtend, int32_t bitsInDst, TR::CodeGenerator *cg);
void simplifyANDRegImm(TR::Register *trgReg, TR::Register *srcReg, int32_t value);
static TR::Register *ishiftAnalyser(TR::Node *node, TR::CodeGenerator *cg,TR_ARMOpCodes regOp);

#endif
