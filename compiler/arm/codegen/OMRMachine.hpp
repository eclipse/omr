/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef OMR_ARM_MACHINE_INCL
#define OMR_ARM_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINE_CONNECTOR
#define OMR_MACHINE_CONNECTOR
namespace OMR { namespace ARM { class Machine; } }
namespace OMR { typedef OMR::ARM::Machine MachineConnector; }
#else
#error OMR::ARM::Machine expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRMachine.hpp"

#include "codegen/RealRegister.hpp"
#include "infra/TRlist.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }

#define NUM_ARM_GPR  16
#define NUM_ARM_MAXR 16
#define MAX_ARM_GLOBAL_GPRS       10
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
#define NUM_ARM_FPR  16
#define MAX_ARM_GLOBAL_FPRS       16
#else
#define NUM_ARM_FPR  8
#define MAX_ARM_GLOBAL_FPRS       8
#endif
#define UPPER_IMMED12  ((1 << 12) - 1)
#define LOWER_IMMED12  (-(1 << 12) + 1)

namespace OMR
{

namespace ARM
{

class OMR_EXTENSIBLE Machine : public OMR::Machine
   {

   static uint32_t       _globalRegisterNumberToRealRegisterMap[MAX_ARM_GLOBAL_GPRS + MAX_ARM_GLOBAL_FPRS];

   void initializeRegisterFile();

   // For register snap shot
   uint16_t                    _registerFlagsSnapShot[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegState  _registerStatesSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_registerAssociationsSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_assignedRegisterSnapShot[TR::RealRegister::NumRegisters];


   public:

   Machine(TR::CodeGenerator *cg);


   TR::RealRegister *findBestFreeRegister(TR_RegisterKinds rk,
                                            bool excludeGPR0 = false,
                                            bool considerUnlatched = false,
                                            bool isSinglePrecision = false);

   TR::RealRegister *freeBestRegister(TR::Instruction  *currentInstruction,
                                        TR_RegisterKinds rk,
                                        TR::RealRegister *forced = NULL,
                                        bool excludeGPR0 = false,
                                        bool isSinglePrecision = false);

   TR::RealRegister *reverseSpillState(TR::Instruction     *currentInstruction,
                                         TR::Register        *spilledRegister,
                                         TR::RealRegister *targetRegister = NULL,
                                         bool excludeGPR0 = false,
                                         bool isSinglePrecision = false);

   void coerceRegisterAssignment(TR::Instruction                           *currentInstruction,
                                 TR::Register                              *virtualRegister,
                                 TR::RealRegister::RegNum registerNumber);

   bool getLinkRegisterKilled()
      {
      return _registerFile[TR::RealRegister::gr14]->getHasBeenAssignedInMethod();
      }

   bool setLinkRegisterKilled(bool b)
      {
      return _registerFile[TR::RealRegister::gr14]->setHasBeenAssignedInMethod(b);
      }

   // Snap shot methods
   void takeRegisterStateSnapShot();
   void restoreRegisterStateFromSnapShot();
   TR::RegisterDependencyConditions  *createCondForLiveAndSpilledGPRs(bool cleanRegState, TR::list<TR::Register*> *spilledRegisterList = NULL);

   TR::RealRegister *assignSingleRegister(TR::Register *virtualRegister, TR::Instruction *currentInstruction);

   // TODO:Are these two still used? Are values correct?  What are they?
   static uint8_t getGlobalGPRPartitionLimit() {return 1;}
   static uint8_t getGlobalFPRPartitionLimit() {return 1;}

   static uint32_t *getGlobalRegisterTable()
      {
      return _globalRegisterNumberToRealRegisterMap;
      }

   static TR_GlobalRegisterNumber getLastGlobalGPRRegisterNumber()
         {return MAX_ARM_GLOBAL_GPRS - 1;}

   static TR_GlobalRegisterNumber getLast8BitGlobalGPRRegisterNumber()
         {return MAX_ARM_GLOBAL_GPRS - 1;}

   static TR_GlobalRegisterNumber getLastGlobalFPRRegisterNumber()
         {return MAX_ARM_GLOBAL_GPRS + MAX_ARM_GLOBAL_FPRS - 1;}
   };

}
}
#endif
