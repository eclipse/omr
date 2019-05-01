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

#ifndef TR_INSTOPCODE_INCL
#define TR_INSTOPCODE_INCL

#include "codegen/OMRInstOpCode.hpp"

#include <stdint.h>
#include "infra/Assert.hpp"

namespace TR
{

class InstOpCode: public OMR::InstOpCodeConnector
   {

public:

   InstOpCode() :
      OMR::InstOpCodeConnector() { }

   InstOpCode(Mnemonic m) :
      OMR::InstOpCodeConnector(m) { }

   const char* getMnemonicName() const                                  { return TR::InstOpCode::metadata[_mnemonic].name; }
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
      case TR::InstOpCode::COND_NOPR        :
      case TR::InstOpCode::COND_VGNOP       : return 0x00 ;
      case TR::InstOpCode::COND_MASK1       :
      case TR::InstOpCode::COND_BO          :
      case TR::InstOpCode::COND_BOR         :
      case TR::InstOpCode::COND_BRO         : return 0x01 ;
      case TR::InstOpCode::COND_MASK2       :
      case TR::InstOpCode::COND_BH          :
      case TR::InstOpCode::COND_BHR         :
      case TR::InstOpCode::COND_BP          :
      case TR::InstOpCode::COND_BPR         :
      case TR::InstOpCode::COND_BRH         :
      case TR::InstOpCode::COND_BRP         : return 0x02 ;
      case TR::InstOpCode::COND_MASK3       : return 0x03 ;
      case TR::InstOpCode::COND_MASK4       :
      case TR::InstOpCode::COND_BL          :
      case TR::InstOpCode::COND_BLR         :
      case TR::InstOpCode::COND_BM          :
      case TR::InstOpCode::COND_BMR         :
      case TR::InstOpCode::COND_BRL         :
      case TR::InstOpCode::COND_BRM         : return 0x04 ;
      case TR::InstOpCode::COND_MASK5       : return 0x05 ;
      case TR::InstOpCode::COND_MASK6       :
      case TR::InstOpCode::COND_BNE         :
      case TR::InstOpCode::COND_BNER        :
      case TR::InstOpCode::COND_BNZ         :
      case TR::InstOpCode::COND_BNZR        : return 0x06 ;
      case TR::InstOpCode::COND_MASK7       :
      case TR::InstOpCode::COND_BRNE        :
      case TR::InstOpCode::COND_BRNZ        : return 0x07 ;
      case TR::InstOpCode::COND_MASK8       :
      case TR::InstOpCode::COND_BE          :
      case TR::InstOpCode::COND_BER         :
      case TR::InstOpCode::COND_BRE         :
      case TR::InstOpCode::COND_BRZ         :
      case TR::InstOpCode::COND_BZ          :
      case TR::InstOpCode::COND_BZR         : return 0x08 ;
      case TR::InstOpCode::COND_MASK9       : return 0x09 ;
      case TR::InstOpCode::COND_MASK10      :
      case TR::InstOpCode::COND_BNL         :
      case TR::InstOpCode::COND_BNLR        :
      case TR::InstOpCode::COND_BNM         :
      case TR::InstOpCode::COND_BNMR        : return 0x0A ;
      case TR::InstOpCode::COND_MASK11      :
      case TR::InstOpCode::COND_BRNL        :
      case TR::InstOpCode::COND_BRNM        : return 0x0B ;
      case TR::InstOpCode::COND_MASK12      :
      case TR::InstOpCode::COND_BNH         :
      case TR::InstOpCode::COND_BNHR        :
      case TR::InstOpCode::COND_BNP         :
      case TR::InstOpCode::COND_BNPR        : return 0x0C ;
      case TR::InstOpCode::COND_MASK13      :
      case TR::InstOpCode::COND_BRNH        :
      case TR::InstOpCode::COND_BRNP        : return 0x0D ;
      case TR::InstOpCode::COND_MASK14      :
      case TR::InstOpCode::COND_BNO         :
      case TR::InstOpCode::COND_BNOR        :
      case TR::InstOpCode::COND_BRNO        : return 0x0E ;
      case TR::InstOpCode::COND_MASK15      :
      case TR::InstOpCode::COND_B           :
      case TR::InstOpCode::COND_BC          :
      case TR::InstOpCode::COND_BCR         :
      case TR::InstOpCode::COND_BR          :
      case TR::InstOpCode::COND_BRC         :
      case TR::InstOpCode::COND_BRU         :
      case TR::InstOpCode::COND_BRUL        : return 0x0F ;
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
         uint8_t mask = getMaskForBranchCondition(bc);
         uint8_t newMask = getReverseBranchMask(mask & 0xe) | (mask & 0x1);
         if (mask == newMask) return bc;
         else                 return getBranchConditionForMask(newMask);
         }
      }
   }

#endif /* TR_Z_INSTOPCODE_INCL */
