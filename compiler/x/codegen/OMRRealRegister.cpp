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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "codegen/Machine.hpp"        // for Machine
#include "codegen/RealRegister.hpp"   // for RealRegister, etc

TR::RealRegister *
OMR::X86::RealRegister::regMaskToRealRegister(TR_RegisterMask mask, TR_RegisterKinds rk, TR::CodeGenerator *cg)
   {
   TR::RealRegister::RegNum rr;

   int32_t bitPos = TR::RealRegister::getBitPosInMask(mask);

   if (rk == TR_GPR)
      rr = TR::RealRegister::FirstGPR;
   else if (rk == TR_X87)
      rr = TR::RealRegister::FirstFPR;
   else if (rk == TR_FPR || rk == TR_VRF)
      rr = TR::RealRegister::FirstXMMR;
   else
      TR_ASSERT(false, "Invalid TR_RegisterKinds value passed to OMR::X86::RealRegister::regMaskToRealRegister()");

   return cg->machine()->getX86RealRegister(TR::RealRegister::RegNum(rr+bitPos));
   }

TR_RegisterMask
OMR::X86::RealRegister::getAvailableRegistersMask(TR_RegisterKinds rk)
   {
   if (rk == TR_GPR)
      return TR::RealRegister::AvailableGPRMask;
   else if (rk == TR_X87)
      return TR::RealRegister::AvailableFPRMask;
   else if (rk == TR_FPR || rk == TR_VRF)
      return TR::RealRegister::AvailableXMMRMask;
   else // MMX: not used
      return 0;
   }

TR::RealRegister::RegMask
OMR::X86::RealRegister::getRealRegisterMask(TR_RegisterKinds rk, TR::RealRegister::RegNum idx)
   {
   if (rk == TR_GPR)
      return TR::RealRegister::gprMask(idx);
   else if (rk == TR_X87)
      return TR::RealRegister::fprMask(idx);
   else if (rk == TR_FPR || rk == TR_VRF)
      return TR::RealRegister::xmmrMask(idx);
   else // MMX: not used
      return TR::RealRegister::mmrMask(idx);
   }
