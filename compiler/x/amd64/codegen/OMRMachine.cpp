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

#include "x/amd64/codegen/OMRMachine.hpp"

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

bool OMR::X86::AMD64::Machine::_disableNewPickRegister, OMR::X86::AMD64::Machine::_dnprIsInitialized=false;

OMR::X86::AMD64::Machine::Machine(TR::CodeGenerator *cg)
   : OMR::X86::Machine
      (
      AMD64_NUM_GPR, // TODO:AMD64: What do these actually do? Possibly
      AMD64_NUM_FPR, // clean up TR_Machine to remove them.
      cg,
      _registerAssociationsStorage,
      TR::Machine::enableNewPickRegister()? (AMD64_MAX_GLOBAL_GPRS      - TR::Machine::numGPRRegsWithheld(cg)) : 8,
      TR::Machine::enableNewPickRegister()? (AMD64_MAX_8BIT_GLOBAL_GPRS - TR::Machine::numRegsWithheld(cg)) : 8,
      TR::Machine::enableNewPickRegister()? (AMD64_MAX_GLOBAL_FPRS      - TR::Machine::numRegsWithheld(cg)) : 8,
      _xmmGlobalRegisterStorage,
      _globalRegisterNumberToRealRegisterMapStorage
      )
   {};

uint8_t OMR::X86::AMD64::Machine::numGPRRegsWithheld(TR::CodeGenerator *cg)
   {
   return cg->comp()->getOption(TR_DisableRegisterPressureSimulation)? 2 : 0;
   }

uint8_t OMR::X86::AMD64::Machine::numRegsWithheld(TR::CodeGenerator *cg)
   {
   return cg->comp()->getOption(TR_DisableRegisterPressureSimulation)? 2 : 0;
   }

bool OMR::X86::AMD64::Machine::enableNewPickRegister()
   {
   return !TR::Machine::disableNewPickRegister();
   }
