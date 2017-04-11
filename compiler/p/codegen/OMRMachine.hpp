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

#ifndef OMR_POWER_MACHINE_INCL
#define OMR_POWER_MACHINE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_MACHINEBASE_CONNECTOR
#define OMR_MACHINEBASE_CONNECTOR
namespace OMR { namespace Power { class Machine; } }
namespace OMR { typedef OMR::Power::Machine MachineConnector; }
#else
#error OMR::Power::Machine expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRMachine.hpp"
#include "infra/TRlist.hpp"
#include "codegen/RealRegister.hpp"         // for RealRegister, etc
#include "env/TRMemory.hpp"                 // for TR_AllocationKind, etc
#include "infra/Assert.hpp"                 // for TR_ASSERT

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
   TR::RealRegister   **_registerFile;
   TR::Register          *_registerAssociations[TR::RealRegister::NumRegisters];
   TR::CodeGenerator *_cg;

   void initialiseRegisterFile();

   int _4thLastGPRAssigned;
   int _3rdLastGPRAssigned;
   int _2ndLastGPRAssigned;
   int _lastGPRAssigned;

   uint16_t _lastPreservedFPRAvail, _lastPreservedVRFAvail;
   uint16_t _inUseFPREnd, _inUseVFREnd, _lastFPRAlloc, _lastVRFAlloc;

   int16_t numLockedGPRs;
   int16_t numLockedFPRs;
   int16_t numLockedVRFs;

   uint16_t _registerAllocationFPR[TR::RealRegister::LastAssignableVSR - TR::RealRegister::FirstVSR + 1]; // 64

   // For register snap shot
   uint16_t                    _registerFlagsSnapShot[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegState  _registerStatesSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_registerAssociationsSnapShot[TR::RealRegister::NumRegisters];
   TR::Register                *_assignedRegisterSnapShot[TR::RealRegister::NumRegisters];

   public:

   Machine(TR::CodeGenerator *cg);

   void initREGAssociations();

   void setNumberOfLockedRegisters(TR_RegisterKinds kind, int numLocked)
      {
      TR_ASSERT(numLocked >= 0, "Expecting number of locked registers to be >= 0");
      switch (kind)
         {
         case TR_GPR: numLockedGPRs = numLocked; break;
         case TR_FPR: numLockedFPRs = numLocked; break;
         case TR_VRF: numLockedVRFs = numLocked; break;
         default:
            TR_ASSERT(false, "Unknown register kind");
         }
      }

   int getNumberOfLockedRegisters(TR_RegisterKinds kind)
      {
      switch (kind)
         {
         case TR_GPR: TR_ASSERT(numLockedGPRs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedGPRs;
         case TR_FPR: TR_ASSERT(numLockedFPRs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedFPRs;
         case TR_VRF: TR_ASSERT(numLockedVRFs >= 0, "Expecting number of locked registers to be >= 0"); return numLockedVRFs;
         default:
            TR_ASSERT(false, "Unknown register kind");
            return -1;
         }
      }

   TR::Register *setVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum, TR::Register * virtReg);
   TR::Register *getVirtualAssociatedWithReal(TR::RealRegister::RegNum regNum);

   TR::RealRegister *getPPCRealRegister(TR::RealRegister::RegNum regNum)
      {
      return _registerFile[regNum];
      }

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

   TR::CodeGenerator *cg() {return _cg;}

   static uint8_t getGlobalGPRPartitionLimit() {return 12;}
   static uint8_t getGlobalFPRPartitionLimit() {return 12;}
   static uint8_t getGlobalCCRPartitionLimit() {return 3;}

   // Snap shot methods
   void takeRegisterStateSnapShot();
   void restoreRegisterStateFromSnapShot();

   TR::RealRegister **cloneRegisterFile(TR::RealRegister **registerFile, TR_AllocationKind allocKind = heapAlloc);
   TR::RealRegister **cloneRegisterFileByType(TR::RealRegister **registerFileClone, TR::RealRegister **registerFile,
                                                int32_t start, int32_t end, TR_RegisterKinds kind, TR_AllocationKind allocKind);
   TR::RealRegister **getRegisterFile() { return _registerFile; }
   TR::RealRegister **setRegisterFile(TR::RealRegister **r) { return _registerFile = r; }
   TR::RegisterDependencyConditions  *createCondForLiveAndSpilledGPRs(bool cleanRegState, TR::list<TR::Register*> *spilledRegisterList = NULL);

   void decFutureUseCountAndUnlatch(TR::Register *virtualRegister);
   void disassociateUnspilledBackingStorage();
   };
}
}
//
#endif
