/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Machine_inlines.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

bool OMR::X86::AMD64::Machine::_disableNewPickRegister, OMR::X86::AMD64::Machine::_dnprIsInitialized = false;

OMR::X86::AMD64::Machine::Machine(TR::CodeGenerator *cg)
    : OMR::X86::Machine(cg)
{
    // Initialize fields in Machine superclass
    //
    _registerAssociations = _registerAssociationsStorage;
    _xmmGlobalRegisters = _xmmGlobalRegisterStorage;
    _globalRegisterNumberToRealRegisterMap = _globalRegisterNumberToRealRegisterMapStorage;

    // Initialize register limits
    //
    _firstGPR = TR::RealRegister::eax;

    if (cg->enableExtendedGPRs()) {
        _lastGPR = TR::RealRegister::r31;
        _lastAssignableGPR = TR::RealRegister::r31;
        _last8BitGPR = TR::RealRegister::r31;
    } else {
        _lastGPR = TR::RealRegister::r15;
        _lastAssignableGPR = TR::RealRegister::r15;
        _last8BitGPR = TR::RealRegister::r15;
    }

    // Note: this total does not include RSP, nor should it
    //
    _numAssignableGPRs = static_cast<uint8_t>(_lastAssignableGPR - _firstGPR);

    _firstXMMR = TR::RealRegister::xmm0;

    if (cg->enableAVX512ExtendedRegs()) {
        _lastXMMR = TR::RealRegister::xmm31;
        _lastAssignableXMMR = TR::RealRegister::xmm31;
    } else {
        _lastXMMR = TR::RealRegister::xmm15;
        _lastAssignableXMMR = TR::RealRegister::xmm15;
    }
    _numAssignableXMMRs = static_cast<uint8_t>(_lastAssignableXMMR - _firstXMMR);

    _firstVMR = TR::RealRegister::k0;
    _lastVMR = TR::RealRegister::k7;
    _lastAssignableVMR = TR::RealRegister::k7;
    _numAssignableVMRs = static_cast<uint8_t>(_lastAssignableVMR - _firstVMR);

    if (TR::Machine::enableNewPickRegister()) {
        _numGlobalGPRs = AMD64_MAX_GLOBAL_GPRS - TR::Machine::numGPRRegsWithheld(cg);
        _numGlobal8BitGPRs = AMD64_MAX_8BIT_GLOBAL_GPRS - TR::Machine::numRegsWithheld(cg);
        _numGlobalFPRs = AMD64_MAX_GLOBAL_FPRS - TR::Machine::numRegsWithheld(cg);
    } else {
        _numGlobalGPRs = 8;
        _numGlobal8BitGPRs = 8;
        _numGlobalFPRs = 8;
    }
}

void OMR::X86::AMD64::Machine::initialize() { self()->OMR::X86::Machine::initialize(); }

uint8_t OMR::X86::AMD64::Machine::numGPRRegsWithheld(TR::CodeGenerator *cg)
{
    return cg->comp()->getOption(TR_DisableRegisterPressureSimulation) ? 2 : 0;
}

uint8_t OMR::X86::AMD64::Machine::numRegsWithheld(TR::CodeGenerator *cg)
{
    return cg->comp()->getOption(TR_DisableRegisterPressureSimulation) ? 2 : 0;
}

bool OMR::X86::AMD64::Machine::enableNewPickRegister() { return !TR::Machine::disableNewPickRegister(); }
