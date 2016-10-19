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

#include "codegen/InstOpCode.hpp"

#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"                                         // for Node
#include "il/Node_inlines.hpp"

uint32_t
OMR::Z::InstOpCode::hasBypass()
   {
   switch(_mnemonic)
      {
      case L:
      case LR:
      case LGR:
      case LG:
      case LA:
      case LM:
      case LAY:
      case LMG:
      case LARL:
      case BXH:
      case BRXH:
      case BRXHG:
      case BRXLE:
      case BXLEG:
      case BRXLG:
         return true;
      }
   return false;
   }

uint32_t
OMR::Z::InstOpCode::isAdmin()
   {
   return (_mnemonic == DIRECTIVE ||
           _mnemonic == RET ||
           _mnemonic == ASSOCREGS ||
           _mnemonic == DEPEND ||
           _mnemonic == FENCE ||
           _mnemonic == SCHEDFENCE ||
           _mnemonic == PROC ||
           _mnemonic == DC ||
           _mnemonic == DC2 ||
           _mnemonic == LOCK ||
           _mnemonic == UNLOCK ||
           _mnemonic == ASM ||
           _mnemonic == DS ||
           _mnemonic == DCB);
   }

uint32_t
OMR::Z::InstOpCode::isHighWordInstruction()
   {
   return (_mnemonic == AHHHR ||
           _mnemonic == AHHLR ||
           _mnemonic == AIH ||
           _mnemonic == ALHHHR ||
           _mnemonic == ALHHLR ||
           _mnemonic == ALSIH ||
           _mnemonic == ALSIHN ||
           _mnemonic == BRCTH ||
           _mnemonic == CHF ||
           _mnemonic == CHHR ||
           _mnemonic == CHLR ||
           _mnemonic == CIH ||
           _mnemonic == CLHF ||
           _mnemonic == CLHHR ||
           _mnemonic == CLHLR ||
           _mnemonic == CLIH ||
           _mnemonic == LBH ||
           _mnemonic == LHH ||
           _mnemonic == LFH ||
           _mnemonic == LFHAT ||
           _mnemonic == LLCH ||
           _mnemonic == LLHH ||
           _mnemonic == RISBHG ||
           _mnemonic == RISBLG ||
           _mnemonic == STCH ||
           _mnemonic == STHH ||
           _mnemonic == STFH ||
           _mnemonic == SHHHR ||
           _mnemonic == SHHLR ||
           _mnemonic == SLHHHR ||
           _mnemonic == SLHHLR);
   }


uint64_t
OMR::Z::InstOpCode::setsOperand(uint32_t opNum)
   {
   if (opNum == 1)
      return properties[_mnemonic] & S390OpProp_SetsOperand1;
   else if (opNum == 2)
      return properties[_mnemonic] & S390OpProp_SetsOperand2;
   else if (opNum == 3)
      return properties[_mnemonic] & S390OpProp_SetsOperand3;
   else if (opNum == 4)
      return properties[_mnemonic] & S390OpProp_SetsOperand4;
   return false;
   }

uint64_t
OMR::Z::InstOpCode::isOperandHW(uint32_t i)
   {
   uint64_t mask = ((i==1)? S390OpProp_TargetHW : 0) | ((i==2)? S390OpProp_SrcHW : 0) | ((i==3)? S390OpProp_Src2HW : 0);
   return properties[_mnemonic] & mask;
   }

uint64_t
OMR::Z::InstOpCode::isOperandLW(uint32_t i)
    {
    uint64_t mask = ((i==1)? S390OpProp_TargetLW : 0) | ((i==2)? S390OpProp_SrcLW : 0) | ((i==3)? S390OpProp_Src2LW : 0);
    return properties[_mnemonic] & mask;
    }



/* Static Methods */

void
OMR::Z::InstOpCode::copyBinaryToBufferWithoutClear(uint8_t *cursor, TR::InstOpCode::Mnemonic i_opCode)
  {
  cursor[0] = binaryEncodings[i_opCode].bytes[0];
  if( binaryEncodings[i_opCode].bytes[1] ) //Two byte opcode
    {
    switch (getInstructionFormat(i_opCode))
      {
      case RXE_FORMAT:
      case RXY_FORMAT:
      case RXF_FORMAT:
      case RSE_FORMAT:
      case RSY_FORMAT:
      case RSL_FORMAT:
      case RIE_FORMAT:
      case SIY_FORMAT:
      case RRS_FORMAT:
      case RIS_FORMAT:
      case VRIa_FORMAT:
      case VRIb_FORMAT:
      case VRIc_FORMAT:
      case VRId_FORMAT:
      case VRIe_FORMAT:
      case VRRa_FORMAT:
      case VRRb_FORMAT:
      case VRRc_FORMAT:
      case VRRd_FORMAT:
      case VRRe_FORMAT:
      case VRRf_FORMAT:
      case VRSa_FORMAT:
      case VRSb_FORMAT:
      case VRSc_FORMAT:
      case VRV_FORMAT:
      case VRX_FORMAT:
         cursor[5] = binaryEncodings[i_opCode].bytes[1];
         //second byte of opcode begins at bit 47 (sixth byte)
         break;
       default:
         cursor[1] = binaryEncodings[i_opCode].bytes[1];
         break;
       }
    }
  }

void
OMR::Z::InstOpCode::copyBinaryToBuffer(uint8_t *cursor, TR::InstOpCode::Mnemonic i_opCode)
  {
 /*
  * clear the stg for this instruction
  */
  for (int32_t i=0; i< getInstructionLength(i_opCode); ++i)
    {
    cursor[i] = 0;
    }
  copyBinaryToBufferWithoutClear(cursor,i_opCode);
  }

/* Using a table that maps values of bit 0-1 to instruction length from POP chapter 5,  topic: Instruction formats (page 5.5)*/
uint8_t
OMR::Z::InstOpCode::getInstructionLength(TR::InstOpCode::Mnemonic i_opCode)
  {
  if (binaryEncodings[i_opCode].bytes[0])
     {
     switch(binaryEncodings[i_opCode].bytes[0] & 0xC0)
       {
       case 0x00 : return 2;
       case 0x40 :
       case 0x80 : return 4;
       case 0xC0 : return 6;
       default   : return 0;
       }
     }
  else // not a real instruction e.g. DC, PSEUDO
     {
     switch(getInstructionFormat(i_opCode))
       {
       case DC_FORMAT:
          {
          if (i_opCode == DC2)   return 2;
          else                      return 4;
          }
       case PSEUDO : return 0;
       default     : return 0;
       }
     }
  }



/* Function Prototype */
#define GET_OPCODE_FROM_NODE(opKind, opcode64, opcode32) \
   TR::InstOpCode::Mnemonic\
   OMR::Z::InstOpCode::get ## opKind ## OpCodeFromNode(TR::Node *node) \
      {\
      if (TR::Compiler->target.is64Bit() && (node->getType().isInt64() || node->getType().isAddress()))\
         {\
         return opcode64;\
         }\
      return opcode32;\
      }

// getLoadOpCodeFromNode
GET_OPCODE_FROM_NODE(Load, TR::InstOpCode::LG, TR::InstOpCode::L)

// getExtendedLoadOpCodeFromNode
GET_OPCODE_FROM_NODE(ExtendedLoad, TR::InstOpCode::LG, TR::InstOpCode::LY)

// getLoadTestRegOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadTestReg, TR::InstOpCode::LTGR, TR::InstOpCode::LTR)

// getLoadComplementOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadComplement, TR::InstOpCode::LCGR, TR::InstOpCode::LCR)

// getLoadHalfWordOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadHalfWord, TR::InstOpCode::LGH, TR::InstOpCode::LH)

// getLoadHalfWordImmOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadHalfWordImm, TR::InstOpCode::LGHI, TR::InstOpCode::LHI)

// getLoadPositiveOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadPositive, TR::InstOpCode::LPGR, TR::InstOpCode::LPR)

// getLoadNegativeOpCodeFromNode
GET_OPCODE_FROM_NODE(LoadNegative, TR::InstOpCode::LNGR, TR::InstOpCode::LNR)

// getExtendedStoreOpCodeFromNode
GET_OPCODE_FROM_NODE(ExtendedStore, TR::InstOpCode::STG, TR::InstOpCode::STY)

// getStoreOpCodeFromNode
GET_OPCODE_FROM_NODE(Store, TR::InstOpCode::STG, TR::InstOpCode::ST)

// getCmpHalfWordImmOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpHalfWordImm, TR::InstOpCode::CGHI, TR::InstOpCode::CHI)

// getCmpHalfWordImmToMemOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpHalfWordImmToMem, TR::InstOpCode::CGHSI, TR::InstOpCode::CHSI)

// getAndRegOpCodeFromNode
GET_OPCODE_FROM_NODE(AndReg, TR::InstOpCode::NGR, TR::InstOpCode::NR)

// getAndOpCodeFromNode
GET_OPCODE_FROM_NODE(And, TR::InstOpCode::NG, TR::InstOpCode::N)

// getBranchOnCountOpCodeFromNode
GET_OPCODE_FROM_NODE(BranchOnCount, TR::InstOpCode::BCTG, TR::InstOpCode::BCT)

// getAddOpCodeFromNode
GET_OPCODE_FROM_NODE(Add, TR::InstOpCode::AG, TR::InstOpCode::A)

// getAddRegOpCodeFromNode
GET_OPCODE_FROM_NODE(AddReg, TR::InstOpCode::AGR, TR::InstOpCode::AR)

// getSubstractOpCodeFromNode
GET_OPCODE_FROM_NODE(Substract, TR::InstOpCode::SG, TR::InstOpCode::S)

// getSubstractRegOpCodeFromNode
GET_OPCODE_FROM_NODE(SubstractReg, TR::InstOpCode::SGR, TR::InstOpCode::SR)

// getSubtractThreeRegOpCodeFromNode
GET_OPCODE_FROM_NODE(SubtractThreeReg, TR::InstOpCode::SGRK, TR::InstOpCode::SRK)

// getSubtractLogicalImmOpCodeFromNode
GET_OPCODE_FROM_NODE(SubtractLogicalImm, TR::InstOpCode::SLGFI, TR::InstOpCode::SLFI)

// getMultiplySingleOpCodeFromNode
GET_OPCODE_FROM_NODE(MultiplySingle, TR::InstOpCode::MSG, TR::InstOpCode::MS)

// getMultiplySingleRegOpCodeFromNode
GET_OPCODE_FROM_NODE(MultiplySingleReg, TR::InstOpCode::MSGR, TR::InstOpCode::MSR)

// getOrOpCodeFromNode
GET_OPCODE_FROM_NODE(Or, TR::InstOpCode::OG, TR::InstOpCode::O)

// getOrRegOpCodeFromNode
GET_OPCODE_FROM_NODE(OrReg, TR::InstOpCode::OGR, TR::InstOpCode::OR)

// getXOROpCodeFromNode
GET_OPCODE_FROM_NODE(XOR, TR::InstOpCode::XG, TR::InstOpCode::X)

// getXORRegOpCodeFromNode
GET_OPCODE_FROM_NODE(XORReg, TR::InstOpCode::XGR, TR::InstOpCode::XR)

// getCmpTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpTrap, TR::InstOpCode::CGRT, TR::InstOpCode::CRT)

// getCmpWidenTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpWidenTrap, TR::InstOpCode::CGFRT, TR::InstOpCode::CRT)

// getCmpImmTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpImmTrap, TR::InstOpCode::CGIT, TR::InstOpCode::CIT)

// getCmpLogicalTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalTrap, TR::InstOpCode::CLGRT, TR::InstOpCode::CLRT)

// getCmpLogicalWidenTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalWidenTrap, TR::InstOpCode::CLGFRT, TR::InstOpCode::CLRT)

// getCmpLogicalImmTrapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalImmTrap, TR::InstOpCode::CLGIT, TR::InstOpCode::CLFIT)

// getCmpOpCodeFromNode
GET_OPCODE_FROM_NODE(Cmp, TR::InstOpCode::CG, TR::InstOpCode::C)

// getCmpRegOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpReg, TR::InstOpCode::CGR, TR::InstOpCode::CR)

// getCmpLogicalOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogical, TR::InstOpCode::CLG, TR::InstOpCode::CL)

// getCmpLogicalRegOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalReg, TR::InstOpCode::CLGR, TR::InstOpCode::CLR)

// getCmpAndSwapOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpAndSwap, TR::InstOpCode::CSG, TR::InstOpCode::CS)

// getCmpLogicalImmOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalImm, TR::InstOpCode::CLGFI, TR::InstOpCode::CLFI)

// getShiftLeftLogicalSingleOpCodeFromNode
GET_OPCODE_FROM_NODE(ShiftLeftLogicalSingle, TR::InstOpCode::SLLG, TR::InstOpCode::SLL)

// getAddHalfWordImmOpCodeFromNode
GET_OPCODE_FROM_NODE(AddHalfWordImm, TR::InstOpCode::AGHI, TR::InstOpCode::AHI)

// getAddLogicalOpCodeFromNode
GET_OPCODE_FROM_NODE(AddLogical, TR::InstOpCode::ALG, TR::InstOpCode::AL)

// getAddLogicalRegOpCodeFromNode
GET_OPCODE_FROM_NODE(AddLogicalReg, TR::InstOpCode::ALGR, TR::InstOpCode::ALR)

// getCmpWidenOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpWiden, TR::InstOpCode::CGF, TR::InstOpCode::C)

// getCmpRegWidenOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpRegWiden, TR::InstOpCode::CGFR, TR::InstOpCode::CR)

// getCmpLogicalWidenOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalWiden, TR::InstOpCode::CLGF, TR::InstOpCode::CL)

// getCmpLogicalRegWidenOpCodeFromNode
GET_OPCODE_FROM_NODE(CmpLogicalRegWiden, TR::InstOpCode::CLGFR, TR::InstOpCode::CLR)


TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadOnConditionRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LOCGR : TR::InstOpCode::LOCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAddressOpCode() { return 0 ? TR::InstOpCode::LAE : TR::InstOpCode::LA; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAndMaskOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LZRG : TR::InstOpCode::LZRF; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getExtendedLoadOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::LY; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadComplementOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LCGR : TR::InstOpCode::LCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadHalfWordOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGH : TR::InstOpCode::LH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadHalfWordImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadPositiveOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LPGR : TR::InstOpCode::LPR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadMultipleOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LMG : TR::InstOpCode::LM; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadNegativeOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LNGR : TR::InstOpCode::LNR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAndTrapOpCode() { return TR::Compiler->target.is64Bit()? TR::InstOpCode::LGAT : TR::InstOpCode::LAT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getExtendedStoreOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::STG : TR::InstOpCode::STY; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::STG : TR::InstOpCode::ST; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreMultipleOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::STMG : TR::InstOpCode::STM; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpHalfWordImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGHI : TR::InstOpCode::CHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpHalfWordImmToMemOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGHSI : TR::InstOpCode::CHSI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAndRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::NGR : TR::InstOpCode::NR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAndOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::NG : TR::InstOpCode::N; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnCountOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::BCTG : TR::InstOpCode::BCT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AG : TR::InstOpCode::A; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AGR : TR::InstOpCode::AR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AGRK : TR::InstOpCode::ARK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::ALGRK : TR::InstOpCode::ALRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalRegRegImmediateOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::ALGHSIK : TR::InstOpCode::ALHSIK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SG : TR::InstOpCode::S; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SGR : TR::InstOpCode::SR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SGRK : TR::InstOpCode::SRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractLogicalThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SLGRK : TR::InstOpCode::SLRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractLogicalImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SLGFI : TR::InstOpCode::SLFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMultiplySingleOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::MSG : TR::InstOpCode::MS; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMultiplySingleRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::MSGR : TR::InstOpCode::MSR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::OG : TR::InstOpCode::O; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::OGR : TR::InstOpCode::OR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::OGRK : TR::InstOpCode::ORK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXOROpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::XG : TR::InstOpCode::X; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXORRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::XGR : TR::InstOpCode::XR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXORThreeRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::XGRK : TR::InstOpCode::XRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGRT : TR::InstOpCode::CRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpWidenTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGFRT : TR::InstOpCode::CRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGFI : TR::InstOpCode::CFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpImmTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGIT : TR::InstOpCode::CIT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGRT : TR::InstOpCode::CLRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalWidenTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGFRT : TR::InstOpCode::CLRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalImmTrapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGIT : TR::InstOpCode::CLFIT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CG : TR::InstOpCode::C; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGR : TR::InstOpCode::CR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLG : TR::InstOpCode::CL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGR : TR::InstOpCode::CLR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpAndSwapOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CSG : TR::InstOpCode::CS; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGFI : TR::InstOpCode::CLFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getShiftLeftLogicalSingleOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SLLG : TR::InstOpCode::SLL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getShiftRightLogicalSingleOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SRLG : TR::InstOpCode::SRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddHalfWordImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::ALG : TR::InstOpCode::AL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalRegOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::ALGR : TR::InstOpCode::ALR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnIndexHighOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::BXHG : TR::InstOpCode::BXH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnIndexEqOrLowOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::BXLEG : TR::InstOpCode::BXLE; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchRelIndexHighOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::BRXHG : TR::InstOpCode::BRXH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchRelIndexEqOrLowOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::BRXLG : TR::InstOpCode::BRXLE; }


TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGF : TR::InstOpCode::L; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGFR : TR::InstOpCode::LR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTGFR : TR::InstOpCode::LTR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AGF : TR::InstOpCode::A; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::AGFR : TR::InstOpCode::AR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SGF : TR::InstOpCode::S; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubStractRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SGFR : TR::InstOpCode::SR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGF : TR::InstOpCode::C; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CGFR : TR::InstOpCode::CR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGF : TR::InstOpCode::CL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::CLGFR : TR::InstOpCode::CLR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadComplementRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LCGFR : TR::InstOpCode::LCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadPositiveRegWidenOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LPGFR : TR::InstOpCode::LPR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractWithBorrowOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::SLBGR : TR::InstOpCode::SLBR; }

/*  Golden Eagle instructions   */
TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LTG : TR::InstOpCode::LT; }

/*  z6 instructions             */
TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreRelativeLongOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::STGRL : TR::InstOpCode::STRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRelativeLongOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::LGRL : TR::InstOpCode::LRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMoveHalfWordImmOpCode() { return TR::Compiler->target.is64Bit() ? TR::InstOpCode::MVGHI : TR::InstOpCode::MVHI; }


TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegOpCodeFromNode(TR::CodeGenerator *cg, TR::Node *node)
   {
   if (node->getType().isAddress())
      {
      if (TR::Compiler->target.is64Bit())
         return TR::InstOpCode::LGR;
      else
         return TR::InstOpCode::LR;
      }

   if ((TR::Compiler->target.is64Bit() || cg->use64BitRegsOn32Bit()) && node->getType().isInt64())
      {
      return TR::InstOpCode::LGR;
      }
   return TR::InstOpCode::LR;
   }


TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMoveHalfWordImmOpCodeFromStoreOpCode(TR::InstOpCode::Mnemonic storeOpCode)
   {
   TR::InstOpCode::Mnemonic mvhiOpCode = TR::InstOpCode::BAD;
   if (storeOpCode == TR::InstOpCode::ST)
      mvhiOpCode = TR::InstOpCode::MVHI;
   else if (storeOpCode == TR::InstOpCode::STG)
      mvhiOpCode = TR::InstOpCode::MVGHI;
   else if (storeOpCode == TR::InstOpCode::STH)
      mvhiOpCode = TR::InstOpCode::MVHHI;

   return mvhiOpCode;
   }
