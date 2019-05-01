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

#ifndef OMR_POWER_MACHINE_INCL
#define OMR_POWER_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINE_CONNECTOR
#define OMR_MACHINE_CONNECTOR
namespace OMR { namespace Power { class Machine; } }
namespace OMR { typedef OMR::Power::Machine MachineConnector; }
#else
#error OMR::Power::Machine expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRMachine.hpp"
#include "infra/TRlist.hpp"
#include "codegen/RealRegister.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
template <typename ListKind> class List;

#define NUM_PPC_GPR 32
#define NUM_PPC_FPR 32
#define NUM_PPC_CCR 8

#define NUM_PPC_MAXR 32
#define UPPER_IMMED ((1 << 15) - 1)
#define LOWER_IMMED (-(1 << 15))

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE Machine : public OMR::Machine
   {
   TR::Register          *_registerAssociations[TR::RealRegister::NumRegisters];

   void initializeRegisterFile();

   int _4thLastGPRAssigned;
   int _3rdLastGPRAssigned;
   int _2ndLastGPRAssigned;
   int _lastGPRAssigned;

   uint16_t _lastPreservedFPRAvail, _lastPreservedVRFAvail;
   uint16_t _inUseFPREnd, _inUseVFREnd, _lastFPRAlloc, _lastVRFAlloc;

   uint16_t _registerAllocationFPR[TR::RealRegister::LastAssignableVSR - TR::RealRegister::FirstVSR + 1]; // 64

   // For register snap shot
   uint16_t                    _registerFlagsSnapShot[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegState  _registerStatesSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_registerAssociationsSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_assignedRegisterSnapShot[TR::RealRegister::NumRegisters];

   public:

   Machine(TR::CodeGenerator *cg);

   void initREGAssociations();

   TR::Register *setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register * virtReg);
   TR::Register *getVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum);

   TR::RealRegister *findBestFreeRegister(TR::Instruction *currentInstruction,
                                            TR_RegisterKinds rk,
					    bool excludeGPR0 = false,
                                            bool considerUnlatched = false,
                                            TR::Register *virtualReg = NULL);

   TR::RealRegister *freeBestRegister(TR::Instruction  *currentInstruction,
                                        TR::Register     *virtReg,
					TR::RealRegister *forced = NULL,
					bool excludeGPR0 = false);

   TR::RealRegister *reverseSpillState(TR::Instruction     *currentInstruction,
                                         TR::Register        *spilledRegister,
                                         TR::RealRegister *targetRegister = NULL,
					 bool excludeGPR0 = false);

   TR::RealRegister *assignOneRegister(TR::Instruction     *currentInstruction,
                                         TR::Register        *virtReg,
                                         bool                excludeGPR0);

   void coerceRegisterAssignment(TR::Instruction                           *currentInstruction,
                                 TR::Register                              *virtualRegister,
                                 TR::RealRegister::RegNum registerNumber);

   bool getLinkRegisterKilled()
      {return _registerFile[TR::RealRegister::lr]->getHasBeenAssignedInMethod();}

   bool setLinkRegisterKilled(bool b);

   static uint8_t getGlobalGPRPartitionLimit() {return 12;}
   static uint8_t getGlobalFPRPartitionLimit() {return 12;}
   static uint8_t getGlobalCCRPartitionLimit() {return 3;}

   // Snap shot methods
   void takeRegisterStateSnapShot();
   void restoreRegisterStateFromSnapShot();

   TR::RealRegister **cloneRegisterFile(TR::RealRegister **registerFile, TR_AllocationKind allocKind = heapAlloc);
   TR::RealRegister **cloneRegisterFileByType(TR::RealRegister **registerFileClone, TR::RealRegister **registerFile,
                                                int32_t start, int32_t end, TR_RegisterKinds kind, TR_AllocationKind allocKind);
   TR::RegisterDependencyConditions  *createCondForLiveAndSpilledGPRs(bool cleanRegState, TR::list<TR::Register*> *spilledRegisterList = NULL);

   void decFutureUseCountAndUnlatch(TR::Register *virtualRegister);
   void disassociateUnspilledBackingStorage();
   };
}
}
#endif
