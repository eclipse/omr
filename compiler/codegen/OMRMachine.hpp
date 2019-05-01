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

#ifndef OMR_MACHINE_INCL
#define OMR_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINE_CONNECTOR
#define OMR_MACHINE_CONNECTOR
namespace OMR { class Machine; }
namespace OMR { typedef OMR::Machine MachineConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "codegen/RealRegister.hpp"
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Machine; }

namespace OMR
{

class OMR_EXTENSIBLE Machine
   {

   int16_t numLockedGPRs;
   int16_t numLockedFPRs;
   int16_t numLockedVRFs;

   public:

   TR_ALLOC(TR_Memory::Machine)

   Machine(TR::CodeGenerator *cg)
      :
      _cg(cg),
      numLockedGPRs(-1),
      numLockedFPRs(-1),
      numLockedVRFs(-1)
      {
      }

   inline TR::Machine * self();

   /**
    * @brief Converts RegNum to RealRegister
    * @param[in] regNum : register number
    * @return RealRegister for specified register number
    */
   TR::RealRegister *getRealRegister(TR::RealRegister::RegNum regNum)
      {
      return _registerFile[regNum];
      }

   /**
    * \return : the cached TR::CodeGenerator object
    */
   TR::CodeGenerator *cg() {return _cg;}

   /** \brief
    *     Sets the number of locked registers of a specific register kind for use as a cache lookup.
    *
    *  \param kind
    *     The register kind to count.
    *
    *  \param numLocked
    *     The number of registers of the respective kind that are in the locked state.
    *
    *  \return
    *     The number of registers of the respective kind that are in the locked state.
    */
   int16_t setNumberOfLockedRegisters(TR_RegisterKinds kind, int16_t numLocked)
      {
      TR_ASSERT(numLocked >= 0, "Expecting number of locked registers to be >= 0");
      switch (kind)
         {
         case TR_GPR: numLockedGPRs = numLocked; return numLockedGPRs;
         case TR_FPR: numLockedFPRs = numLocked; return numLockedFPRs;
         case TR_VRF: numLockedVRFs = numLocked; return numLockedVRFs;
         default:
            TR_ASSERT_FATAL(false, "Unknown register kind");
            return -1;
         }
      }

   /** \brief
    *     Gets the number of locked registers of a specific register kind.
    *
    *  \param kind
    *     The register kind to count.
    *
    *  \return
    *     The number of registers of the respective kind that are in the locked state.
    */
   int16_t getNumberOfLockedRegisters(TR_RegisterKinds kind)
      {
      switch (kind)
         {
         case TR_GPR: TR_ASSERT(numLockedGPRs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedGPRs;
         case TR_FPR: TR_ASSERT(numLockedFPRs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedFPRs;
         case TR_VRF: TR_ASSERT(numLockedVRFs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedVRFs;
         default:
            TR_ASSERT_FATAL(false, "Unknown register kind");
            return -1;
         }
      }

   /**
    * \brief Retrieve a pointer to the register file
    */
   TR::RealRegister **registerFile() { return _registerFile; }

   /**
    * \brief Retrieves the TR::RealRegister object for the given real register
    *
    * \param[in] rn : the desired real register
    *
    * \return : the desired TR::RealRegister object
    */
   TR::RealRegister *realRegister(TR::RealRegister::RegNum rn) { return _registerFile[rn]; }

private:

   TR::CodeGenerator *_cg;

protected:

   TR::RealRegister *_registerFile[TR::RealRegister::NumRegisters];

   };
}

#endif
