/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "codegen/InstOpCode.hpp"

#include "codegen/CodeGenerator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

const OMR::Z::InstOpCode::OpCodeMetaData OMR::Z::InstOpCode::metadata[NumOpCodes] =
   {
   #include "codegen/OMRInstOpCodeProperties.hpp"
   };

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
   return (_mnemonic == RET ||
           _mnemonic == ASSOCREGS ||
           _mnemonic == DEPEND ||
           _mnemonic == FENCE ||
           _mnemonic == PROC ||
           _mnemonic == DC ||
           _mnemonic == DC2 ||
           _mnemonic == DCB);
   }

uint64_t
OMR::Z::InstOpCode::setsOperand(uint32_t opNum)
   {
   if (opNum == 1)
      return metadata[_mnemonic].properties & S390OpProp_SetsOperand1;
   else if (opNum == 2)
      return metadata[_mnemonic].properties & S390OpProp_SetsOperand2;
   else if (opNum == 3)
      return metadata[_mnemonic].properties & S390OpProp_SetsOperand3;
   else if (opNum == 4)
      return metadata[_mnemonic].properties & S390OpProp_SetsOperand4;
   return false;
   }

uint64_t
OMR::Z::InstOpCode::isOperandHW(uint32_t i)
   {
   uint64_t mask = ((i==1)? S390OpProp_TargetHW : 0) | ((i==2)? S390OpProp_SrcHW : 0) | ((i==3)? S390OpProp_Src2HW : 0);
   return metadata[_mnemonic].properties & mask;
   }

/* Static Methods */

void
OMR::Z::InstOpCode::copyBinaryToBufferWithoutClear(uint8_t *cursor, TR::InstOpCode::Mnemonic i_opCode)
  {
  cursor[0] = metadata[i_opCode].opcode[0];

  // Second opcode being non-zero indicates a two-byte opcode the second of which is always the last byte of the instruction
  if (metadata[i_opCode].opcode[1] != 0)
     {
     switch (getInstructionFormat(i_opCode))
        {
        case RIEa_FORMAT:
        case RIEb_FORMAT:
        case RIEc_FORMAT:
        case RIEd_FORMAT:
        case RIEe_FORMAT:
        case RIEf_FORMAT:
        case RIEg_FORMAT:
        case RIS_FORMAT:
        case RRS_FORMAT:
        case RSLa_FORMAT:
        case RSLb_FORMAT:
        case RSYa_FORMAT:
        case RSYb_FORMAT:
        case RXE_FORMAT:
        case RXF_FORMAT:
        case RXYa_FORMAT:
        case RXYb_FORMAT:
        case SIY_FORMAT:
        case VRIa_FORMAT:
        case VRIb_FORMAT:
        case VRIc_FORMAT:
        case VRId_FORMAT:
        case VRIe_FORMAT:
        case VRIf_FORMAT:
        case VRIg_FORMAT:
        case VRIh_FORMAT:
        case VRIi_FORMAT:
        case VRRa_FORMAT:
        case VRRb_FORMAT:
        case VRRc_FORMAT:
        case VRRd_FORMAT:
        case VRRe_FORMAT:
        case VRRf_FORMAT:
        case VRRg_FORMAT:
        case VRRh_FORMAT:
        case VRRi_FORMAT:
        case VRSa_FORMAT:
        case VRSb_FORMAT:
        case VRSc_FORMAT:
        case VRSd_FORMAT:
        case VRV_FORMAT:
        case VRX_FORMAT:
        case VSI_FORMAT:
           cursor[5] = metadata[i_opCode].opcode[1];
           break;

        default:
           cursor[1] = metadata[i_opCode].opcode[1];
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
  if (metadata[i_opCode].opcode[0])
     {
     switch(metadata[i_opCode].opcode[0] & 0xC0)
       {
       case 0x00 : return 2;
       case 0x40 :
       case 0x80 : return 4;
       case 0xC0 : return 6;
       default:
          break;
       }
     }
  else // not a real instruction e.g. DC, PSEUDO
     {
     switch(getInstructionFormat(i_opCode))
        {
        case E_FORMAT:
           {
           return 2;
           }

        case DC_FORMAT:
           {
           return i_opCode == DC2 ? 2 : 4;
           }

       default:
          break;
        }
     }

  return 0;
  }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getEquivalentLongDisplacementMnemonic(TR::InstOpCode::Mnemonic op)
   {
   switch (op)
      {
      case TR::InstOpCode::A:
         return TR::InstOpCode::AY;
      case TR::InstOpCode::AL:
         return TR::InstOpCode::ALY;
      case TR::InstOpCode::AH:
         return TR::InstOpCode::AHY;
      case TR::InstOpCode::C:
         return TR::InstOpCode::CY;
      case TR::InstOpCode::CH:
         return TR::InstOpCode::CHY;
      case TR::InstOpCode::CL:
         return TR::InstOpCode::CLY;
      case TR::InstOpCode::IC:
         return TR::InstOpCode::ICY;
      case TR::InstOpCode::L:
         return TR::InstOpCode::LY;
      case TR::InstOpCode::LA:
         return TR::InstOpCode::LAY;
      case TR::InstOpCode::LAE:
         return TR::InstOpCode::LAEY;
      case TR::InstOpCode::LRA:
         return TR::InstOpCode::LRAY;
      case TR::InstOpCode::LH:
         return TR::InstOpCode::LHY;
      case TR::InstOpCode::MS:
         return TR::InstOpCode::MSY;
      case TR::InstOpCode::M:
         return TR::InstOpCode::MFY;
      case TR::InstOpCode::MH:
         return TR::InstOpCode::MHY;
      case TR::InstOpCode::N:
         return TR::InstOpCode::NY;
      case TR::InstOpCode::NI:
         return TR::InstOpCode::NIY;
      case TR::InstOpCode::O:
         return TR::InstOpCode::OY;
      case TR::InstOpCode::OI:
         return TR::InstOpCode::OIY;
      case TR::InstOpCode::S:
         return TR::InstOpCode::SY;
      case TR::InstOpCode::SH:
         return TR::InstOpCode::SHY;
      case TR::InstOpCode::SL:
         return TR::InstOpCode::SLY;
      case TR::InstOpCode::ST:
         return TR::InstOpCode::STY;
      case TR::InstOpCode::STC:
         return TR::InstOpCode::STCY;
      case TR::InstOpCode::STH:
         return TR::InstOpCode::STHY;
      case TR::InstOpCode::X:
         return TR::InstOpCode::XY;
      case TR::InstOpCode::XI:
         return TR::InstOpCode::XIY;
      case TR::InstOpCode::LE:
         return TR::InstOpCode::LEY;
      case TR::InstOpCode::LD:
         return TR::InstOpCode::LDY;
      case TR::InstOpCode::STE:
         return TR::InstOpCode::STEY;
      case TR::InstOpCode::STD:
         return TR::InstOpCode::STDY;
      case TR::InstOpCode::LM:
         return TR::InstOpCode::LMY;
      case TR::InstOpCode::STM:
         return TR::InstOpCode::STMY;
      case TR::InstOpCode::STCM:
         return TR::InstOpCode::STCMY;
      case TR::InstOpCode::ICM:
         return TR::InstOpCode::ICMY;
      case TR::InstOpCode::TM:
         return TR::InstOpCode::TMY;
      case TR::InstOpCode::MVI:
         return TR::InstOpCode::MVIY;
      case TR::InstOpCode::CLI:
         return TR::InstOpCode::CLIY;
      case TR::InstOpCode::CVB:
         return TR::InstOpCode::CVBY;
      case TR::InstOpCode::CVD:
         return TR::InstOpCode::CVDY;
      case TR::InstOpCode::CS:
         return TR::InstOpCode::CSY;
      case TR::InstOpCode::CDS:
         return TR::InstOpCode::CDSY;
      default:
         return TR::InstOpCode::BAD;
      }
   }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadOnConditionRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LOCGR : TR::InstOpCode::LOCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAddressOpCode() { return 0 ? TR::InstOpCode::LAE : TR::InstOpCode::LA; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::L; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAndMaskOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LZRG : TR::InstOpCode::LZRF; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getExtendedLoadOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LG : TR::InstOpCode::LY; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadComplementOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LCGR : TR::InstOpCode::LCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadHalfWordOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGH : TR::InstOpCode::LH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadHalfWordImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGHI : TR::InstOpCode::LHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadPositiveOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LPGR : TR::InstOpCode::LPR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadMultipleOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LMG : TR::InstOpCode::LM; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadNegativeOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LNGR : TR::InstOpCode::LNR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadAndTrapOpCode() { return TR::comp()->target().is64Bit()? TR::InstOpCode::LGAT : TR::InstOpCode::LAT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getExtendedStoreOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::STG : TR::InstOpCode::STY; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::STG : TR::InstOpCode::ST; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreMultipleOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::STMG : TR::InstOpCode::STM; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpHalfWordImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGHI : TR::InstOpCode::CHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpHalfWordImmToMemOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGHSI : TR::InstOpCode::CHSI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAndRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::NGR : TR::InstOpCode::NR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAndOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::NG : TR::InstOpCode::N; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnCountOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::BCTG : TR::InstOpCode::BCT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AG : TR::InstOpCode::A; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGR : TR::InstOpCode::AR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGRK : TR::InstOpCode::ARK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::ALGRK : TR::InstOpCode::ALRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::ALGFI : TR::InstOpCode::ALFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalRegRegImmediateOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::ALGHSIK : TR::InstOpCode::ALHSIK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SG : TR::InstOpCode::S; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SGR : TR::InstOpCode::SR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SGRK : TR::InstOpCode::SRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractLogicalThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SLGRK : TR::InstOpCode::SLRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractLogicalImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SLGFI : TR::InstOpCode::SLFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMultiplySingleOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::MSG : TR::InstOpCode::MS; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMultiplySingleRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::MSGR : TR::InstOpCode::MSR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::OG : TR::InstOpCode::O; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::OGR : TR::InstOpCode::OR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getOrThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::OGRK : TR::InstOpCode::ORK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXOROpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::XG : TR::InstOpCode::X; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXORRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::XGR : TR::InstOpCode::XR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getXORThreeRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::XGRK : TR::InstOpCode::XRK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpTrapOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGRT : TR::InstOpCode::CRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGFI : TR::InstOpCode::CFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpImmTrapOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGIT : TR::InstOpCode::CIT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpImmBranchRelOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGIJ : TR::InstOpCode::CIJ; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalTrapOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGRT : TR::InstOpCode::CLRT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalImmTrapOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGIT : TR::InstOpCode::CLFIT; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CG : TR::InstOpCode::C; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGR : TR::InstOpCode::CR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLG : TR::InstOpCode::CL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGR : TR::InstOpCode::CLR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpAndSwapOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CSG : TR::InstOpCode::CS; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGFI : TR::InstOpCode::CLFI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getShiftLeftLogicalSingleOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SLLG : TR::InstOpCode::SLL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getShiftRightLogicalSingleOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SRLG : TR::InstOpCode::SRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddHalfWordImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddHalfWordImmDistinctOperandOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGHIK : TR::InstOpCode::AHIK; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::ALG : TR::InstOpCode::AL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddLogicalRegOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::ALGR : TR::InstOpCode::ALR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnIndexHighOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::BXHG : TR::InstOpCode::BXH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchOnIndexEqOrLowOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::BXLEG : TR::InstOpCode::BXLE; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchRelIndexHighOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::BRXHG : TR::InstOpCode::BRXH; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getBranchRelIndexEqOrLowOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::BRXLG : TR::InstOpCode::BRXLE; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGF : TR::InstOpCode::L; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGFR : TR::InstOpCode::LR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LTGFR : TR::InstOpCode::LTR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGF : TR::InstOpCode::A; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getAddRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::AGFR : TR::InstOpCode::AR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubstractWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SGF : TR::InstOpCode::S; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubStractRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SGFR : TR::InstOpCode::SR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGF : TR::InstOpCode::C; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGFR : TR::InstOpCode::CR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpRegAndBranchRelOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CGRJ : TR::InstOpCode::CRJ; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGF : TR::InstOpCode::CL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getCmpLogicalRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::CLGFR : TR::InstOpCode::CLR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadComplementRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LCGFR : TR::InstOpCode::LCR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadPositiveRegWidenOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LPGFR : TR::InstOpCode::LPR; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getSubtractWithBorrowOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::SLBGR : TR::InstOpCode::SLBR; }

/*  Golden Eagle instructions   */
TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadTestOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LTG : TR::InstOpCode::LT; }

/*  z6 instructions             */
TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getStoreRelativeLongOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::STGRL : TR::InstOpCode::STRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRelativeLongOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::LGRL : TR::InstOpCode::LRL; }

TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getMoveHalfWordImmOpCode() { return TR::comp()->target().is64Bit() ? TR::InstOpCode::MVGHI : TR::InstOpCode::MVHI; }


TR::InstOpCode::Mnemonic
OMR::Z::InstOpCode::getLoadRegOpCodeFromNode(TR::CodeGenerator *cg, TR::Node *node)
   {
   if (node->getType().isAddress())
      {
      return cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
      }

   return node->getType().isInt64() ? TR::InstOpCode::LGR : TR::InstOpCode::LR;
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
