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

#ifndef OMR_ARM_CODEGENERATOR_UTILS_INCL
#define OMR_ARM_CODEGENERATOR_UTILS_INCL

#include "compiler/codegen/OMRCodeGeneratorUtils.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"


namespace TR
{

/**
 * @brief Creates a pre and post condition for the specified virtual and real register
 *
 * @param[in] dep : TR::RegisterDependencyConditions to add the dependencies to
 * @param[in] vreg : the virtual register.  If NULL then a new virtual register of kind rk will
 *                   be allocated
 * @param[in] rnum : the real register
 * @param[in] rk : the kind of register to allocate if one must be allocated.  Otherwise, this
 *                 parameter is ignored.
 * @param[in] cg : CodeGenerator object
 */
static inline void
addDependency(
      TR::RegisterDependencyConditions *dep,
      TR::Register *vreg,
      TR::RealRegister::RegNum rnum,
      TR_RegisterKinds rk,
      TR::CodeGenerator *cg)
   {
   if (vreg == NULL)
      {
      vreg = cg->allocateRegister(rk);
      }
   dep->addPreCondition(vreg, rnum);
   dep->addPostCondition(vreg, rnum);
   }

}

#endif

