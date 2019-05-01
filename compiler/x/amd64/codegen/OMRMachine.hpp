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

#ifndef OMR_AMD64_MACHINE_INCL
#define OMR_AMD64_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINE_CONNECTOR
#define OMR_MACHINE_CONNECTOR
namespace OMR { namespace X86 { namespace AMD64 { class Machine; } } }
namespace OMR { typedef OMR::X86::AMD64::Machine MachineConnector; }
#else
#error OMR::X86::AMD64::Machine expected to be a primary connector, but an OMR connector is already defined
#endif

#include "x/codegen/OMRMachine.hpp"

#include <stdint.h>
namespace TR { class CodeGenerator; }
namespace TR { class RealRegister; }
namespace TR { class Register; }
namespace TR { class Machine; }

namespace OMR
{

namespace X86
{

namespace AMD64
{

class OMR_EXTENSIBLE Machine : public OMR::X86::Machine
   {

   enum
      {
      AMD64_NUM_GPR              = 16,
      AMD64_NUM_FPR              = 8,  // x87 registers
      AMD64_NUM_XMMR             = 16,
      AMD64_MAX_GLOBAL_GPRS      = 14,
      AMD64_MAX_8BIT_GLOBAL_GPRS = AMD64_MAX_GLOBAL_GPRS,
      AMD64_MAX_GLOBAL_FPRS      = 16,
      };

   TR::Register         *_registerAssociationsStorage[TR::RealRegister::NumRegisters];
   TR::Register         *_xmmGlobalRegisterStorage[AMD64_NUM_XMMR];
   uint32_t _globalRegisterNumberToRealRegisterMapStorage[AMD64_MAX_GLOBAL_GPRS + AMD64_MAX_GLOBAL_FPRS];

   public:

   Machine(TR::CodeGenerator *cg);

   static uint8_t numGPRRegsWithheld(TR::CodeGenerator *cg);
   static uint8_t numRegsWithheld(TR::CodeGenerator *cg);

   static bool disableNewPickRegister()
      {
      if (!_dnprIsInitialized)
         {
         _dnprIsInitialized = true;
         }
      return _disableNewPickRegister;
      }

   static bool enableNewPickRegister();

   private:

   static bool _disableNewPickRegister, _dnprIsInitialized;

   };

} // namespace AMD64

} // namespace X86

} // namespace OMR

#endif
