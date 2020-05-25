/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_Z_CPU_INCL
#define OMR_Z_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CPU_CONNECTOR
#define OMR_CPU_CONNECTOR
namespace OMR { namespace Z { class CPU; } }
namespace OMR { typedef OMR::Z::CPU CPUConnector; }
#else
#error OMR::Z::CPU expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/env/OMRCPU.hpp"
#include "env/jittypes.h"
#include "env/ProcessorInfo.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace OMR
{

namespace Z
{

class OMR_EXTENSIBLE CPU : public OMR::CPU
   {
   public:

   enum Architecture
      {
      Unknown = 0,
      z900,
      z990,
      z9,
      z10,
      z196,
      zEC12,
      z13,
      z14,
      z15,
      zNext,
      };

   /** \brief
    *     Gets the name of the processor given the four digit machine id.
    */
   static const char* getProcessorName(int32_t machineId);

   static TR::CPU detect(OMRPortLibrary * const omrPortLib);

   public:

   bool getSupportsArch(Architecture arch);

   bool setSupportsArch(Architecture arch);

   bool getSupportsHardwareSQRT();

   bool hasPopulationCountInstruction();
   
   /** \brief
    *     Determines whether the High-Word facility is available on the current processor.
    */
   bool getSupportsHighWordFacility();

   /** \brief
    *     Determines whether the High-Word facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the High-Word facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsHighWordFacility(bool value);
   
   /** \brief
    *     Determines whether the Decimal Floating Point (DFP) facility is available on the current processor.
    */
   bool getSupportsDecimalFloatingPointFacility();
   
   /** \brief
    *     Determines whether the Decimal Floating Point (DFP) facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Decimal Floating Point facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsDecimalFloatingPointFacility(bool value);
   
   /** \brief
    *     Determines whether the Floating Point Extension (FPE) facility is available on the current processor.
    */
   bool getSupportsFloatingPointExtensionFacility();
   
   /** \brief
    *     Determines whether the Floating Point Extension (FPE) facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Floating Point Extension facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsFloatingPointExtensionFacility(bool value);
   
   /** \brief
    *     Determines whether the Transactional Memory (TM) facility is available on the current processor.
    */
   bool getSupportsTransactionalMemoryFacility();

   /** \brief
    *     Determines whether the Transactional Memory (TM) facility is available on the current processor.
    *     Alias of getSupportsTransactionalMemoryFacility() as a platform agnostic query.
    */
   bool supportsTransactionalMemoryInstructions();
   
   /** \brief
    *     Determines whether the Transactional Memory (TM) facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Transactional Memory facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsTransactionalMemoryFacility(bool value);
   
   /** \brief
    *     Determines whether the Runtime Instrumentation (RI) facility is available on the current processor.
    */
   bool getSupportsRuntimeInstrumentationFacility();
   
   /** \brief
    *     Determines whether the Runtime Instrumentation (RI) facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Runtime Instrumentation facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsRuntimeInstrumentationFacility(bool value);
   
   /** \brief
    *     Determines whether the Vector facility is available on the current processor.
    */
   bool getSupportsVectorFacility();
   
   /** \brief
    *     Determines whether the Vector facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Vector facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsVectorFacility(bool value);
   
   /** \brief
    *     Determines whether the Vector Packed Decimal facility is available on the current processor.
    */
   bool getSupportsVectorPackedDecimalFacility();
   
   /** \brief
    *     Determines whether the Vector Packed Decimal facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Vector Packed Decimal facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsVectorPackedDecimalFacility(bool value);
   
   /** \brief
    *     Determines whether the Miscellaneous Instruction Extensions 2 (MIE2) facility is available on the current
    *     processor.
    */
   bool getSupportsMiscellaneousInstructionExtensions2Facility();

   /** \brief
    *     Determines whether the Miscellaneous Instruction Extensions 2 (MIE2) facility is available on the current
    *     processor.
    *
    *  \param value
    *     Determines whether the Miscellaneous Instruction Extensions 2 facility is available (if \c true) or not (if
    *     \c false).
    */
   bool setSupportsMiscellaneousInstructionExtensions2Facility(bool value);


   /** \brief
    *     Determines whether the Miscellaneous Instruction Extensions 3 (MIE3) facility is available on the current
    *     processor.
    */
   bool getSupportsMiscellaneousInstructionExtensions3Facility();
   
   /** \brief
    *     Determines whether the Miscellaneous Instruction Extensions 3 (MIE3) facility is available on the current
    *     processor.
    *
    *  \param value
    *     Determines whether the Miscellaneous Instruction Extensions 3 facility is available (if \c true) or not (if 
    *     \c false).
    */
   bool setSupportsMiscellaneousInstructionExtensions3Facility(bool value);
   
   /** \brief
    *     Determines whether the Vector Enhancement 2 facility is available on the current processor.
    */
   bool getSupportsVectorFacilityEnhancement2();
   
   /** \brief
    *     Determines whether the Vector Enhancement 2 facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Vector Enhancement 2 facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsVectorFacilityEnhancement2(bool value);

   /** \brief
    *     Determines whether the Vector Enhancement 1 facility is available on the current processor.
    */
   bool getSupportsVectorFacilityEnhancement1();
   
   /** \brief
    *     Determines whether the Vector Enhancement 1 facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Vector Enhancement 1 facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsVectorFacilityEnhancement1(bool value);
   
   /** \brief
    *     Determines whether the Vector Packed Decimal facility is available on the current processor.
    */
   bool getSupportsVectorPackedDecimalEnhancementFacility();
   
   /** \brief
    *     Determines whether the Vector Packed Decimal facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Vector Packed Decimal facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsVectorPackedDecimalEnhancementFacility(bool value);
   
   /** \brief
    *     Determines whether the Guarded Storage (GS) facility is available on the current processor.
    */
   bool getSupportsGuardedStorageFacility();
   
   /** \brief
    *     Determines whether the Guarded Storage (GS) facility is available on the current processor.
    *
    *  \param value
    *     Determines whether the Guarded Storage facility is available (if \c true) or not (if \c false).
    */
   bool setSupportsGuardedStorageFacility(bool value);
   
   /**
    * @brief Answers whether the distance between a target and source address
    *        is within the reachable displacement range for a branch relative
    *        RIL-format instruction.
    *
    * @param[in] : targetAddress : the address of the target
    *
    * @param[in] : sourceAddress : the address of the branch relative RIL-format
    *                 instruction from which the displacement range is measured.
    *
    * @return true if the target is within range; false otherwise.
    */
   bool isTargetWithinBranchRelativeRILRange(intptr_t targetAddress, intptr_t sourceAddress);

   protected:

   CPU() : OMR::CPU(), _supportedArch(z9) {}
   CPU(const OMRProcessorDesc& processorDescription) : OMR::CPU(processorDescription) {}

   enum
      {
      // Available                             = 0x00000001,
      HasResumableTrapHandler                  = 0x00000002,
      HasFixedFrameC_CallingConvention         = 0x00000004,
      SupportsScaledIndexAddressing            = 0x00000080,
      S390SupportsDFP                          = 0x00000100,
      S390SupportsFPE                          = 0x00000200,
      S390SupportsHPR                          = 0x00001000,
      IsInZOSSupervisorState                   = 0x00008000,
      S390SupportsTM                           = 0x00010000,
      S390SupportsRI                           = 0x00020000,
      S390SupportsVectorFacility               = 0x00040000,
      S390SupportsVectorPackedDecimalFacility  = 0x00080000,
      S390SupportsGuardedStorageFacility       = 0x00100000,
      S390SupportsSideEffectAccessFacility     = 0x00200000,
      S390SupportsMIE2                         = 0x00400000,
      S390SupportsMIE3                         = 0x00800000,
      S390SupportsVectorFacilityEnhancement2   = 0x01000000,
      S390SupportsVectorPDEnhancementFacility  = 0x02000000,
      S390SupportsVectorFacilityEnhancement1   = 0x04000000,
      };

   Architecture _supportedArch;

   flags32_t _flags;
   };

}

}

#endif
