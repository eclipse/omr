/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef TR_RV_INSTOPCODE_INCL
#define TR_RV_INSTOPCODE_INCL

#include "codegen/OMRInstOpCode.hpp"
#include "infra/Assert.hpp"

namespace TR
{

class InstOpCode: public OMR::InstOpCodeConnector
   {
   public:

   /**
    * @brief Constructor
    */
   InstOpCode() : OMR::InstOpCodeConnector(bad) {}
   /**
    * @brief Constructor
    * @param[in] m : mnemonic
    */
   InstOpCode(TR::InstOpCode::Mnemonic m) : OMR::InstOpCodeConnector(m)
      {
#ifdef TR_RISCV_RV_SOURCE_COMPAT
      TR_ASSERT((uint32_t)m < (uint32_t)TR::InstOpCode::cbzw || (uint32_t)m >= (uint32_t)TR::InstOpCode::proc,
               "Invalid RISC-V opcode (AArch64 opcode, perhaps?)");
#endif
      }

   /**
    * @brief For given branch opcode, return 'reversed' opcode. For example, for BEQ return
    * BNE, for BLT return BGE and so on. Assumes passed opcode is a branch opcode.
    *
    * @param[in] opcode : opcode to be reversed
    * @return Reversed opcode
    */
   static TR::InstOpCode::Mnemonic reversedBranchOpCode(TR::InstOpCode::Mnemonic opcode)
      {
      switch (opcode)
         {
         case TR::InstOpCode::_beq:
            return TR::InstOpCode::_bne;
         case TR::InstOpCode::_bne:
            return TR::InstOpCode::_beq;
         case TR::InstOpCode::_blt:
            return TR::InstOpCode::_bge;
         case TR::InstOpCode::_bltu:
            return TR::InstOpCode::_bgeu;
         case TR::InstOpCode::_bge:
            return TR::InstOpCode::_blt;
         case TR::InstOpCode::_bgeu:
            return TR::InstOpCode::_bltu;
         default:
            TR_ASSERT_FATAL(false, "Not a branch opcode: %d", opcode);
            return TR::InstOpCode::bad;
         }
      }
   };

}
#endif
