/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef OMR_RV_MACHINE_INCL
#define OMR_RV_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINE_CONNECTOR
#define OMR_MACHINE_CONNECTOR
namespace OMR { namespace RV { class Machine; } }
namespace OMR { typedef OMR::RV::Machine MachineConnector; }
#else
#error OMR::RV::Machine expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRMachine.hpp"

#include "codegen/RealRegister.hpp"
#include "infra/Annotations.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Register; }

#define NUM_RV_GPR 32
#define MAX_RV_GLOBAL_GPRS 27 // excluding IP0, IP1, FP, LR, and SP
#define NUM_RV_FPR 32
#define MAX_RV_GLOBAL_FPRS 32

#define NUM_RV_MAXR 32

namespace OMR
{

namespace RV
{

class OMR_EXTENSIBLE Machine : public OMR::Machine
   {

public:

   /**
    * @brief Constructor
    * @param[in] cg : the TR::CodeGenerator object
    */
   Machine(TR::CodeGenerator *cg);

   /**
    * @brief Finds the best free register
    * @param[in] rk : register kind
    * @param[in] considerUnlatched : consider unlatched state or not
    * @return Free RealRegister
    */
   TR::RealRegister *findBestFreeRegister(TR_RegisterKinds rk,
                                            bool considerUnlatched = false);

   /**
    * @brief Frees the best register
    * @param[in] currentInstruction : current instruction
    * @param[in] virtualRegister : virtual register
    * @param[in] forced : register to be freed
    * @return Freed RealRegister
    */
   TR::RealRegister *freeBestRegister(TR::Instruction *currentInstruction,
                                        TR::Register *virtualRegister,
                                        TR::RealRegister *forced = NULL);

   /**
    * @brief Reverses the spill state
    * @param[in] currentInstruction : current instruction
    * @param[in] spilledRegister : spilled register
    * @param[in] targetRegister : target register
    * @return Target register
    */
   TR::RealRegister *reverseSpillState(TR::Instruction *currentInstruction,
                                         TR::Register *spilledRegister,
                                         TR::RealRegister *targetRegister = NULL);

   /**
    * @brief Assign a RealRegister for specified Register
    * @param[in] currentInstruction : current instruction
    * @param[in] virtualRegister : virtual register
    * @return Assigned RealRegister
    */
   TR::RealRegister *assignOneRegister(TR::Instruction *currentInstruction,
                                         TR::Register *virtualRegister);

   /**
    * @brief Coerce register assignment
    * @param[in] currentInstruction : current instruction
    * @param[in] virtualRegister : virtual register
    * @param[in] registerNumber : register number
    */
   void coerceRegisterAssignment(TR::Instruction *currentInstruction,
                                 TR::Register *virtualRegister,
                                 TR::RealRegister::RegNum registerNumber);

   /**
    * @brief Returns the "killed" state of Link Register
    * @return true if LR is killed in the method, false otherwise
    */
   bool getLinkRegisterKilled()
      {
      return _registerFile[TR::RealRegister::ra]->getHasBeenAssignedInMethod();
      }

   /**
    * @brief Changes the "killed" state of Link Register
    * @param[in] b : true if LR is killed in the method, false otherwise
    * @return The "killed" state set by the function
    */
   bool setLinkRegisterKilled(bool b)
      {
      return _registerFile[TR::RealRegister::ra]->setHasBeenAssignedInMethod(b);
      }

   /**
    * @brief Take snapshot of the register file
    */
   void takeRegisterStateSnapShot();

   /**
    * @brief Restore the register file from snapshot
    */
   void restoreRegisterStateFromSnapShot();

   /**
    * @brief Answers global register table
    * @return global register table
    */
   static uint32_t *getGlobalRegisterTable()
      { return _globalRegisterNumberToRealRegisterMap; }
   /**
    * @brief Answers global register number of last GPR
    * @return global register number
    */
   static TR_GlobalRegisterNumber getLastGlobalGPRRegisterNumber()
      { return MAX_RV_GLOBAL_GPRS - 1; }
   /**
    * @brief Answers global register number of last FPR
    * @return global register number
    */
   static TR_GlobalRegisterNumber getLastGlobalFPRRegisterNumber()
      { return MAX_RV_GLOBAL_GPRS + MAX_RV_GLOBAL_FPRS - 1; }

private:

   // For register snap shot
   uint16_t                   _registerFlagsSnapShot[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegState _registerStatesSnapShot[TR::RealRegister::NumRegisters];
   TR::Register               *_assignedRegisterSnapShot[TR::RealRegister::NumRegisters];

   void initializeRegisterFile();

   // Tactical GRA
   static uint32_t _globalRegisterNumberToRealRegisterMap[MAX_RV_GLOBAL_GPRS + MAX_RV_GLOBAL_FPRS];

   };
}
}
#endif
