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

