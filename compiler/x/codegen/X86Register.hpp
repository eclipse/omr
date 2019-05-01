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

#ifndef X86REGISTER_INCL
#define X86REGISTER_INCL

#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"

class TR_X86FPStackRegister;
namespace TR { class CodeGenerator; }
namespace TR { class Register; }

// TODO:AMD64: Re-evaluate the safety of this function, because not
// all Registers are FPStackRegisters.
inline TR_X86FPStackRegister * toX86FPStackRegister(TR::Register *r)
   {
   return (TR_X86FPStackRegister *)r;
   }

class TR_X86FPStackRegister : public TR::RealRegister
   {
   public:

   typedef enum
      {
      fpStackEmpty      = -1,
      fp0               = 0,
      fpFirstStackReg   = fp0,
      fp1               = 1,
      fp2               = 2,
      fp3               = 3,
      fp4               = 4,
      fp5               = 5,
      fp6               = 6,
      fp7               = 7,
      fpLastStackReg    = fp7,
      fpStackFull       = 8,
      NumRegisters      = fp7 + 1
      } TR_X86FPStackRegisters;

   TR_X86FPStackRegister(RegState s, TR_X86FPStackRegisters rn, TR::RealRegister::RegNum ri, TR::CodeGenerator *cg) :
      TR::RealRegister(TR_X87, 0, s, ri, TR::RealRegister::noRegMask, cg), _fpStackRegisterNumber(rn) {}

   TR_X86FPStackRegisters getFPStackRegisterNumber() {return _fpStackRegisterNumber;}
   TR_X86FPStackRegisters setFPStackRegisterNumber(TR_X86FPStackRegisters rn) {return (_fpStackRegisterNumber = rn);}

   private:

   TR_X86FPStackRegisters  _fpStackRegisterNumber;

   };

#endif
