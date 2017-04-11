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

