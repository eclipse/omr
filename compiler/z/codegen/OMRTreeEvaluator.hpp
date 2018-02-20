/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef OMR_Z_TREE_EVALUATOR_INCL
#define OMR_Z_TREE_EVALUATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_TREE_EVALUATOR_CONNECTOR
#define OMR_TREE_EVALUATOR_CONNECTOR
namespace OMR { namespace Z { class TreeEvaluator; } }
namespace OMR { typedef OMR::Z::TreeEvaluator TreeEvaluatorConnector; }
#else
#error OMR::Z::TreeEvaluator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRTreeEvaluator.hpp"

#include "codegen/InstOpCode.hpp"          // for InstOpCode, etc
#include "il/DataTypes.hpp"                // for DataTypes
#include "il/ILOpCodes.hpp"                // for ILOpCodes

class TR_OpaquePseudoRegister;
class TR_PseudoRegister;
class TR_StorageReference;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class RegisterDependencyConditions; }

enum tableKind          // for tableEvaluator
   {
   AddressTable32bit,
   AddressTable64bitIntLookup,
   AddressTable64bitLongLookup,
   RelativeTable32bit,
   RelativeTable64bitIntLookup,
   RelativeTable64bitLongLookup
   };

enum ArrayTranslateFlavor
   {
   CharToByte,
   ByteToChar,
   ByteToByte,
   CharToChar
   };

enum TR_FoldType
   {
   TR_FoldNone,
   TR_FoldIf,
   TR_FoldLookup,
   TR_FoldNoIPM
   };


namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE TreeEvaluator: public OMR::TreeEvaluator
   {
   public:

   static TR::Register *badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *asmEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *branchEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *ibranchEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *mbranchEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *floadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *astoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ilstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *igotoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *returnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ifcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *idcallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *libmFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *libxloptFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fenceEvaluator(TR::Node * node, TR::CodeGenerator *cg);
   static TR::Register *treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aiaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aladdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *addrAddHelper(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *mulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *dualMulHelper64(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulHelper32(TR::Node * node, TR::Node * lmulNode, TR::Node * lumulhNode, TR::CodeGenerator * cg);
   static TR::Register *dualMulEvaluator(TR::Node * node, TR::CodeGenerator *cg);

   static TR::Register *lmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lmulhEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *floatRemHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *sremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *labsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *snegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *integerRolEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *cushrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *landEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   /** \brief
    *     This is a helper function that will try to use a rotate instruction
    *     to perform an 'land' (Long AND) operation. It will handle cases
    *     where the second child of the Long AND is a lconst node, and the 
    *     lconst value is a sequence of contiguous 1's surrounded by 0's or vice versa.
    *     For example, the lconst value can be:
    *     00011111111000, 1111000000011111, 0100000  
    *
    *     For example, consider the following tree:
    *
    *     land
    *        i2l
    *           iload parm=0
    *        lconst 0x0000FFFFFFFF0000
    *
    *     We can either materialize the constant in a register or use the NIHF+NILF instructions 
    *     to AND the most and least significant 32 bit portions in the first operand. These 
    *     are both more expensive than generating a RISBG instruction.
    *
    *     This is because in the above case we can perform the AND operation with the 
    *     following single RISBG instruction:
    *
    *     RISBG R2,R1,33,175,0 //R1 holds parm 0
    *
    *     In the above instruction the value of 175 comes from
    *     the sum of 128 and 47. 47 represents the ending position
    *     of the bit range to preserve, and 128 is there to set the 
    *     zero bit. The instruction is saying to take the value in R1 and
    *     do the following:
    *        -preserve the bits from 33 - 47 (inclusive)
    *        -zero the remaining bits
    *        -rotate the preserved bits by a factor of 0(hence no rotation)
    *        -store the result in R2
    * 
    *     Note: RISBG is non-destructive (unlike the AND immediate instructions). 
    *     So the original value in R1 is preserved.
    *
    *   \param node
    *      The land node
    *
    *   \param cg
    *      The code generator used to generate the instructions
    *
    *   \return
    *      The target register for the instruction, or NULL if generating a 
    *      RISBG instruction is not suitable
    *
    */
   static TR::Register *tryToReplaceLongAndWithRotateInstruction(TR::Node * node, TR::CodeGenerator * cg);

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
   static TR::Register *dceilEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *dfloorEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *fceilEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *ffloorEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *floorCeilEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic regToRegOpCode, int8_t mask);
   template <bool isDstTypeSigned, uint32_t newSize>
   static TR::Register *narrowCastEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   template <bool isSourceTypeSigned, uint32_t srcSize, uint32_t numberOfExtendBits>
   static TR::Register *extendCastEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   template<uint32_t otherSize, bool dirToAddr>
   static TR::Register *addressCastEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *l2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *a2oEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *b2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *s2dEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2fEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *iflcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iflcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
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
   static TR::Register *fcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static bool isCandidateForButestEvaluation(TR::Node *node);
   static bool isCandidateForCompareEvaluation(TR::Node * node);
   static bool isSingleRefUnevalAndCompareOrBu2iOverCompare(TR::Node * node);
   static TR::InstOpCode::S390BranchCondition getBranchConditionFromCompareOpCode(TR::ILOpCodes opCode);
   static TR::InstOpCode::S390BranchCondition mapBranchConditionToLOCRCondition(TR::InstOpCode::S390BranchCondition incomingBc);
   static TR::InstOpCode::Mnemonic getCompareOpFromNode(TR::CodeGenerator *cg, TR::Node *node);
   static TR::Register *ternaryEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static TR::Register *dternaryEvaluator(TR::Node * node, TR::CodeGenerator * cg);
   static bool treeContainsAllOtherUsesForNode(TR::Node *condition, TR::Node *trueVal);
   static int32_t countReferencesInTree(TR::Node *treeNode, TR::Node *node);
   static TR::Register *NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *barrierFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static void tableEvaluatorCaseLabelHelper(TR::Node * node, TR::CodeGenerator * cg, tableKind tableKindToBeEvaluated, int32_t numBranchTableEntries, TR::Register * selectorReg, TR::Register * branchTableReg, TR::Register *reg1  );
   static TR::Register *tableEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *NULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ZEROCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *resolveAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *vselEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *vdmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *vincEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vdecEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vl2vdEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcomEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vDivOrRemHelper(TR::Node *node, TR::CodeGenerator *cg, bool isDivision);
   static TR::Register *vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vremEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vectorElementShiftHelper(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vrandEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vconstEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *getvelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineVectorUnaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorTernaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
   static TR::Register *inlineVectorSetElementHelper(TR::Node *node, TR::CodeGenerator *cg, int8_t size, bool isPromote);
   
   static TR::Register *bitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   
   static TR::Register *tryToReuseInputVectorRegs(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Node* DAAAddressPointer (TR::Node* callNode, TR::CodeGenerator* cg);

   static void         createDAACondDeps(TR::Node * node, TR::RegisterDependencyConditions * daaDeps,
                                              TR::Instruction * daaInstr, TR::CodeGenerator * cg);
   static void         addToRegDep(TR::RegisterDependencyConditions * daaDeps, TR::Register * reg,
                                   bool mightUseRegPair);

   static TR::Register *passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   /** \brief
    *     Evaluates a primitive or reference arraycopy node. A primitive arraycopy has 3 children and a reference
    *     arraycopy has 5 children.
    *
    *     The z Systems code generator requires that <c>isForwardArrayCopy()</c> really means that a forward array copy
    *     is safe and that if it is not set it really means that a backward array copy is safe. The optimizer should
    *     have generated the correct tests around the arraycopy to enforce this assumption.
    *
    *     Furthermore note that the z Systems code generator does not need to check for a particular frequency on the
    *     length, again because the optimizer will have generated a test for a particular length, if one is quite
    *     common, and there will be two versions of the arraycopy call; one with a constant length and one with a
    *     variable length.
    *
    *  \param node
    *     The arraycopy node.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    *
    *  \param byteSrcNode
    *     The source address of the array copy.
    *
    *  \param byteDstNode
    *     The destination address of the array copy.
    *
    *  \param byteLenNode
    *     The number of bytes to copy.
    *
    *  \return
    *     <c>NULL</c>
    */
   static TR::Register *arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   /** \brief
    *     Evaluates a primitive arraycopy node by generating an MVC memory-memory copy for a forward arraycopy and a
    *     loop based on the stride length for a backward arraycopy.

    *  \param node
    *     The primitive arraycopy node.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    *
    *  \param byteSrcNode
    *     The source address of the array copy.
    *
    *  \param byteDstNode
    *     The destination address of the array copy.
    *
    *  \param byteLenNode
    *     The number of bytes to copy.
    */
   static void primitiveArraycopyEvaluator(TR::Node* node, TR::CodeGenerator* cg, TR::Node* byteSrcNode, TR::Node* byteDstNode, TR::Node* byteLenNode);

   /** \brief
    *     Evaluates a reference arraycopy node by generating an MVC memory-memory copy for a forward arraycopy and a
    *     loop based on the reference size for a backward arraycopy. For runtimes which support it, this function will
    *     also generate write barrier checks on \p byteSrcObjNode and \p byteDstObjNode.
    *
    *  \param node
    *     The reference arraycopy node.
    *
    *  \param cg
    *     The code generator used to generate the instructions.
    *
    *  \param byteSrcNode
    *     The source address of the array copy.
    *
    *  \param byteDstNode
    *     The destination address of the array copy.
    *
    *  \param byteLenNode
    *     The number of bytes to copy.
    *
    *  \param byteSrcObjNode
    *     The runtime object which represents the source array.
    *
    *  \param byteDstObjNode
    *     The runtime object which represents the destination array.
    *
    *  \note
    *     The reference size must be explicitly specified on the \p node via the <c>getArrayCopyElementType</c> API.
    */
   static void referenceArraycopyEvaluator(TR::Node* node, TR::CodeGenerator* cg, TR::Node* byteSrcNode, TR::Node* byteDstNode, TR::Node* byteLenNode, TR::Node* byteSrcObjNode, TR::Node* byteDstObjNode);

   static TR::Register *arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *o2xEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *x2oEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register * evaluateLengthMinusOneForMemoryCmp(TR::Node *node, TR::CodeGenerator *cg, bool clobberEvaluate, bool &lenMinusOne);

   static TR::Register *inlineIfArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool& inlined);

   static TR::Register *arraycmpHelper(TR::Node *node,
                                      TR::CodeGenerator *cg,
                                      bool isWideChar,
                                      bool isEqualCmp = false,
                                      int32_t cmpValue = 0,
                                      TR::LabelSymbol *compareTarget = NULL,
                                      TR::Node *ificmpNode = NULL,
                                      bool needResultReg = true,
                                      bool return102 = false,
                                      TR::InstOpCode::S390BranchCondition *returnCond = NULL);
   static TR::Register *arraycmpSIMDHelper(TR::Node *node,
                                      TR::CodeGenerator *cg,
                                      TR::LabelSymbol *compareTarget = NULL,
                                      TR::Node *ificmpNode = NULL,
                                      bool needResultReg = true,
                                      bool return102 = false);

   static TR::Register *arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraytranslateEncodeSIMDEvaluator(TR::Node *node, TR::CodeGenerator *cg, ArrayTranslateFlavor convType);
   static TR::Register *arraytranslateDecodeSIMDEvaluator(TR::Node *node, TR::CodeGenerator *cg, ArrayTranslateFlavor convType);
   static TR::Register *arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *long2StringEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bitOpMemEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *rsloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *riloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *rlloadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *rsstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ristoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *rlstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *arraycmpEvaluatorWithPad(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *lcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2aEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *bu2sEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2iEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *minEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *maxEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *composeEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *aletEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *evaluateNULLCHKWithPossibleResolve(TR::Node * node, bool needsResolve, TR::CodeGenerator * cg);

   static bool genNullTestForCompressedPointers(TR::Node *node, TR::CodeGenerator *cg, TR::Register *targetRegister, TR::LabelSymbol * &skipAdd);

   static TR::Register *getstackEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *deallocaEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg);


   static TR::Register *loadAutoOffsetEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *condCodeEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *trtEvaluator(TR::Node *node, TR::CodeGenerator *cg);

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


   static TR::Register *butestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *inlineIfButestEvaluator(TR::Node * node, TR::CodeGenerator * cg, TR::Node * cmpOpNode, TR::Node * cmpValNode);
   static TR::Register *inlineIfBifEvaluator(TR::Node * ifNode, TR::CodeGenerator * cg, TR::Node * cmpOpNode, TR::Node * cmpValNode);


   static TR::Register *getpmEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *setpmEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *dexpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fexpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *dnintEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fnintEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *memcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *ixfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *lxfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *fxfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg);

// VM dependent routines

   static TR::Register       *performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg);

   static TR::Instruction* genLoadForObjectHeaders      (TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor);
   static TR::Instruction* genLoadForObjectHeadersMasked(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor);

   static TR::Register       *inlineIfTestDataClassHelper(TR::Node *node, TR::CodeGenerator *cg, bool &inlined);
   static void         evaluateRegLoads(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register *vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg);

   static TR::Register *z990PopCountHelper(TR::Node *node, TR::CodeGenerator *cg, TR::Register *inReg, TR::Register *outReg);

   static TR::Register *getConditionCodeOrFold(TR::Node *node, TR::CodeGenerator *cg, TR_FoldType foldType, TR::Node *parent);
   static TR::Register *getConditionCodeOrFoldIf(TR::Node *node, TR::CodeGenerator *cg, bool isFoldedIf, TR::Node *ificmpNode);
   static TR::Register *foldLookup(TR::CodeGenerator *cg, TR::Node *lookupNode);

   private:

   static void         commonButestEvaluator(TR::Node *node, TR::CodeGenerator *cg);
   static TR::Register* ifFoldingHelper(TR::Node *node, TR::CodeGenerator *cg, bool &handledBIF);
   static bool         isCandidateForBIFEvaluation(TR::Node * node);
   };

}

}

#endif
