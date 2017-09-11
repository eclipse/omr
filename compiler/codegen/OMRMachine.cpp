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

#pragma csect(CODE,"OMRMachine#C")
#pragma csect(STATIC,"OMRMachine#S")
#pragma csect(TEST,"OMRMachine#T")


#include "codegen/OMRMachine.hpp"

#include "codegen/Machine.hpp"      // for TR::Machine
#include "codegen/Machine_inlines.hpp"

uint8_t
OMR::Machine::getNumberOfGPRs()
   {
   return self()->getNumberOfRegisters(TR_GPR);
   }

uint8_t
OMR::Machine::getNumberOfFPRs()
   {
   return self()->getNumberOfRegisters(TR_FPR);
   }

uint8_t
OMR::Machine::setNumberOfGPRs(uint8_t numIntRegs)
   {
   return self()->setNumberOfRegisters(TR_GPR,numIntRegs);
   }

uint8_t
OMR::Machine::setNumberOfFPRs(uint8_t numFPRegs)
   {
   return self()->setNumberOfRegisters(TR_FPR,numFPRegs);
   }
