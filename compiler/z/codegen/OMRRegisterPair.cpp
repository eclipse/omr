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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"OMRZRegPairBase#C")
#pragma csect(STATIC,"OMRZRegPairBase#S")
#pragma csect(TEST,"OMRZRegPairBase#T")

#include "z/codegen/OMRRegisterPair.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/SymbolReference.hpp"
#include "optimizer/Structure.hpp"

OMR::Z::RegisterPair::RegisterPair(TR::Register *lo, TR::Register *ho):
   OMR::RegisterPair(lo, ho)
   {
   // highword RA HPR upgrade
   if (lo)
      lo->setIsNotHighWordUpgradable(true);
   if (ho)
      ho->setIsNotHighWordUpgradable(true);
   }


TR::Register *
OMR::Z::RegisterPair::setLowOrder(TR::Register *lo, TR::CodeGenerator *codeGen)
   {
   // highword RA HPR upgrade
   if (lo) lo->setIsNotHighWordUpgradable(true);

   // call base class implementation
   return OMR::RegisterPair::setLowOrder(lo, codeGen);
   }


TR::Register *
OMR::Z::RegisterPair::setHighOrder(TR::Register *ho, TR::CodeGenerator *codeGen)
   {
   // highword RA HPR upgrade
   if (ho) ho->setIsNotHighWordUpgradable(true);

   // call base class implementation
   return OMR::RegisterPair::setHighOrder(ho, codeGen);
   }
