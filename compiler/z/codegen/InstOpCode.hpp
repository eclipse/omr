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

#ifndef TR_INSTOPCODE_INCL
#define TR_INSTOPCODE_INCL

#include "codegen/OMRInstOpCode.hpp"

#include <stdint.h>          // for uint8_t, uint32_t
#include "infra/Assert.hpp"  // for TR_ASSERT

namespace TR
{

class InstOpCode: public OMR::InstOpCodeConnector
   {

public:

   InstOpCode() :
      OMR::InstOpCodeConnector() { }

   InstOpCode(Mnemonic m) :
      OMR::InstOpCodeConnector(m) { }

   /* OMR TODO: Delegating method; need to be removed and replace with getMnemonic */
   TR::InstOpCode::Mnemonic getOpCodeValue()                            { return _mnemonic; }
   TR::InstOpCode::Mnemonic setOpCodeValue(TR::InstOpCode::Mnemonic op) { return _mnemonic = op; }

   };
}

#ifdef OPS_MAX
#undef OPS_MAX
#endif
#define OPS_MAX    TR::InstOpCode::NumOpCodes
#define S390_MAX_BRANCH_MASK 16

inline TR::InstOpCode::S390BranchCondition
getBranchConditionForMask(uint32_t branchMask)
   {
   switch (branchMask)
      {
      case 0:                 return TR::InstOpCode::COND_MASK0 ;
      case 1:                 return TR::InstOpCode::COND_MASK1 ;
      case 2:                 return TR::InstOpCode::COND_MASK2 ;
      case 3:                 return TR::InstOpCode::COND_MASK3 ;
      case 4:                 return TR::InstOpCode::COND_MASK4 ;
      case 5:                 return TR::InstOpCode::COND_MASK5 ;
      case 6:                 return TR::InstOpCode::COND_MASK6 ;
      case 7:                 return TR::InstOpCode::COND_MASK7 ;
      case 8:                 return TR::InstOpCode::COND_MASK8 ;
      case 9:                 return TR::InstOpCode::COND_MASK9 ;
      case 10:                return TR::InstOpCode::COND_MASK10;
      case 11:                return TR::InstOpCode::COND_MASK11;
      case 12:                return TR::InstOpCode::COND_MASK12;
      case 13:                return TR::InstOpCode::COND_MASK13;
      case 14:                return TR::InstOpCode::COND_MASK14;
      case 15:                return TR::InstOpCode::COND_MASK15;
      default:
         TR_ASSERT(0, "Unknown branch mask specified");
         return TR::InstOpCode::COND_NOP;
      }
   }

inline uint8_t
getReverseBranchMask(uint8_t m)
   {
   switch (m)
      {
      case 0x2: return 0x4;
      case 0x4: return 0x2;
      case 0xA: return 0xC;
      case 0xC: return 0xA;
      default:  return m;
      }
   }

inline uint8_t
getMaskForBranchCondition (TR::InstOpCode::S390BranchCondition branchCond)
   {
   //TR_ASSERT(branchCond <= lastBranchCondition , "ERROR: unknown branch condition\n");
   switch (branchCond)
      {
      case TR::InstOpCode::COND_MASK0       :
      case TR::InstOpCode::COND_NOP         :
      case TR::InstOpCode::COND_VGNOP       : return 0x00 ;
      case TR::InstOpCode::COND_MASK1       :
      case TR::InstOpCode::COND_BO          :
      case TR::InstOpCode::COND_BOR         : return 0x10 ;
      case TR::InstOpCode::COND_MASK2       :
      case TR::InstOpCode::COND_BH          :
      case TR::InstOpCode::COND_BHR         :
      case TR::InstOpCode::COND_BP          :
      case TR::InstOpCode::COND_BPR         : return 0x20 ;
      case TR::InstOpCode::COND_MASK3       : return 0x30 ;
      case TR::InstOpCode::COND_MASK4       :
      case TR::InstOpCode::COND_BL          :
      case TR::InstOpCode::COND_BLR         :
      case TR::InstOpCode::COND_BM          :
      case TR::InstOpCode::COND_BMR         :
      case TR::InstOpCode::COND_BMRC        : return 0x40 ;
      case TR::InstOpCode::COND_MASK5       : return 0x50 ;
      case TR::InstOpCode::COND_MASK6       :
      case TR::InstOpCode::COND_BNE         :
      case TR::InstOpCode::COND_BNER        :
      case TR::InstOpCode::COND_BNZ         :
      case TR::InstOpCode::COND_BNZR        : return 0x60 ;
      case TR::InstOpCode::COND_MASK7       : return 0x70 ;
      case TR::InstOpCode::COND_MASK8       :
      case TR::InstOpCode::COND_BE          :
      case TR::InstOpCode::COND_BER         :
      case TR::InstOpCode::COND_BZ          :
      case TR::InstOpCode::COND_BZR         : return 0x80 ;
      case TR::InstOpCode::COND_MASK9       : return 0x90 ;
      case TR::InstOpCode::COND_MASK10      :
      case TR::InstOpCode::COND_BNL         :
      case TR::InstOpCode::COND_BNLR        :
      case TR::InstOpCode::COND_BNM         :
      case TR::InstOpCode::COND_BNMR        : return 0xA0 ;
      case TR::InstOpCode::COND_MASK11      : return 0xB0 ;
      case TR::InstOpCode::COND_MASK12      :
      case TR::InstOpCode::COND_BNH         :
      case TR::InstOpCode::COND_BNHR        :
      case TR::InstOpCode::COND_BNP         :
      case TR::InstOpCode::COND_BNPR        : return 0xC0 ;
      case TR::InstOpCode::COND_MASK13      : return 0xD0 ;
      case TR::InstOpCode::COND_MASK14      :
      case TR::InstOpCode::COND_BNO         :
      case TR::InstOpCode::COND_BNOR        : return 0xE0 ;
      case TR::InstOpCode::COND_MASK15      :
      case TR::InstOpCode::COND_BC          :
      case TR::InstOpCode::COND_BCR         :
      case TR::InstOpCode::COND_BRC         : return 0xF0 ;
      default:
         TR_ASSERT(0, "Unknown branch instruction specified");
         return 0;
      }
   }

/**
 * Gives the reverse branch condition when the order of the 2 operands for a compare
 * operation is reversed.  For example "a > b" become "b < a", "a >= b" becomes "b <= a"
 * basically, this corresponds to flipping the 2 middle bits of the branch condition mask
 * and this method gives the corresponding branch conditions to accomplish that.
 */
inline TR::InstOpCode::S390BranchCondition
getReverseBranchCondition(TR::InstOpCode::S390BranchCondition bc)
   {
   switch (bc)
      {
      case TR::InstOpCode::COND_BNH:
         return TR::InstOpCode::COND_BNL;
      case TR::InstOpCode::COND_BNHR:
         return TR::InstOpCode::COND_BNLR;
      case TR::InstOpCode::COND_BNP:
         return TR::InstOpCode::COND_BNM;
      case TR::InstOpCode::COND_BNL:
         return TR::InstOpCode::COND_BNH;
      case TR::InstOpCode::COND_BNLR:
         return TR::InstOpCode::COND_BNHR;
      case TR::InstOpCode::COND_BNM:
         return TR::InstOpCode::COND_BNP;
      case TR::InstOpCode::COND_BNMR:
         return TR::InstOpCode::COND_BNP;
      case TR::InstOpCode::COND_BL:
         return TR::InstOpCode::COND_BH;
      case TR::InstOpCode::COND_BLR:
         return TR::InstOpCode::COND_BHR;
      case TR::InstOpCode::COND_BM:
         return TR::InstOpCode::COND_BP;
      case TR::InstOpCode::COND_BMR:
         return TR::InstOpCode::COND_BPR;
      case TR::InstOpCode::COND_BH:
         return TR::InstOpCode::COND_BL;
      case TR::InstOpCode::COND_BHR:
         return TR::InstOpCode::COND_BLR;
      case TR::InstOpCode::COND_BP:
         return TR::InstOpCode::COND_BM;
      case TR::InstOpCode::COND_BPR:
         return TR::InstOpCode::COND_BMR;
      default:
         {
         uint8_t mask = (getMaskForBranchCondition(bc)>>4);
         uint8_t newMask = getReverseBranchMask(mask & 0xe) | (mask & 0x1);
         if (mask == newMask) return bc;
         else                 return getBranchConditionForMask(newMask);
         }
      }
   }

#endif /* TR_Z_INSTOPCODE_INCL */
