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
   static TR::Register *dloadHelper(TR::Node *node, TR::CodeGenerator *cg, TR::Register *reg, TR::InstOpCode::Mnemonic op);
   static TR::Register *bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *buloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *commonByteLoadEvaluator(TR::Node *node, bool signExtend, TR::CodeGenerator *cg);
   static TR::Register *sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg); // ibm@59591
   static TR::Register *istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *asubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dualMulHelper64(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulHelper32(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulEvaluator(TR::Node * node, TR::CodeGenerator *cg);
   static TR::Register *lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *smulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *imulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *l2buEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpneuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifacmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *fcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gprRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gprRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *setmemoryEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *reverseStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *viremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *viminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vimaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnotEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vbitselectEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vpermEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, int nBit);
   static TR::Register *vicmpanyHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, int nBit);
   static TR::Register *vicmpalleqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpallleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanyeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanyneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanygtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanygeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanyltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vicmpanyleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vigetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *getvelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *getvelemDirectMoveHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *getvelemMemoryMoveHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *visetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vimergeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdnmsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdmsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulInt32Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivInt32Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdivDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegInt32Helper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegFloatHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegDoubleHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdminEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdmergeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdselEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpallHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *vdcmpanyHelper(TR::Node *node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *vdcmpalleqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpallgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpallgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpallleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpallltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpanyeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpanygeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpanygtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpanyleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdcmpanyltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdlogEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vl2vdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineVectorUnaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorBitSelectOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static bool inlineVectorCompareBranch(TR::Node * node, TR::CodeGenerator *cg, bool isHint, bool likeliness);
   static TR::Register *inlineVectorCompareAllOrAnyOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic vcmpOp, TR::InstOpCode::Mnemonic branchOp);
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
   static TR::Register *iexpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lexpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dexpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ixfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dxfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dintEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnintEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *getstackEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deallocaEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idozEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dfloorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *libmFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *maxEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *minEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *strcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *strcFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *xfRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *xdRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg);

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

   static void genBranchSequence(TR::Node* node, TR::Register* src1Reg, TR::Register* src2Reg, TR::Register* trgReg,
      TR::InstOpCode::Mnemonic branchOpCode, TR::InstOpCode::Mnemonic cmpOpCode, TR::CodeGenerator* cg);

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
