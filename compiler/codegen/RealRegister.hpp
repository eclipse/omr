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

#ifndef TR_REAL_REGISTER_INCL
#define TR_REAL_REGISTER_INCL

#include "codegen/OMRRealRegister.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }

namespace TR
{

class OMR_EXTENSIBLE RealRegister : public OMR::RealRegisterConnector
   {
   public:

   RealRegister(TR::CodeGenerator *cg): OMR::RealRegisterConnector(cg) {}

   RealRegister(TR_RegisterKinds rk, uint16_t w, RegState s, RegNum rn, RegMask m, TR::CodeGenerator * cg):
      OMR::RealRegisterConnector(rk, w, s, rn, m, cg) {}

   };

}

inline TR::RealRegister *toRealRegister(TR::Register *r)
   {
   TR_ASSERT(r == NULL || r->getRealRegister() != NULL, "trying to convert a non-real register to a real register");
   return static_cast<TR::RealRegister *>(r);
   }

#endif
