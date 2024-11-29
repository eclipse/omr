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

#ifndef OMR_CPU_INCL
#define OMR_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CPU_CONNECTOR
#define OMR_CPU_CONNECTOR
namespace OMR { class CPU; }
namespace OMR { typedef OMR::CPU CPUConnector; }
#endif

#include <string.h>
#include "env/Processors.hpp"
#include "infra/Annotations.hpp"
#include "omrport.h"

namespace TR { class CPU; }

namespace TR
{
// Processor endianness
//
enum Endianness
   {
   endian_big,
   endian_little,
   endian_unknown
   };


// Major CPU architecture
//
enum MajorArchitecture
   {
   arch_x86,
   arch_z,
   arch_power,
   arch_arm,
   arch_arm64,
   arch_riscv,
   arch_unknown
   };


// Minor CPU architecture
//
enum MinorArchitecture
   {
   m_arch_none,
   m_arch_i386,
   m_arch_amd64,
   m_arch_riscv32,
   m_arch_riscv64
   };

}


namespace OMR
{

class OMR_EXTENSIBLE CPU
   {
protected:

   /**
    * @brief Default constructor that defaults down to OMR minimum supported CPU and features
    */
   CPU();

   /**
    * @brief Constructor that initializes the cpu from processor description provided by user
    * @param[in] OMRProcessorDesc : the input processor description
    */
   CPU(const OMRProcessorDesc& processorDescription);

public:

   TR::CPU *self();

   /**
    * @brief Detects the underlying processor type and features using the port library and constructs a TR::CPU object
    * @param[in] omrPortLib : the port library
    * @return TR::CPU
    */
   static TR::CPU detect(OMRPortLibrary * const omrPortLib);

  /**
    * @brief Returns the processor type and features that will be used by portable AOT compilations
    * @param[in] omrPortLib : the port library
    * @return TR::CPU
    */
   static TR::CPU detectRelocatable(OMRPortLibrary * const omrPortLib);

   /**
    * @brief API to initialize platform specific target processor info if it exists
    * @param[in] force : force initialization even if the target processor info has already been initialized
    */
   static void initializeTargetProcessorInfo(bool force = false) {}

   TR_Processor setProcessor(TR_Processor p) { return(_processor = p); }

   // Processor identity and generation comparisons
   //
   TR_Processor id() { return _processor; }

   bool getSupportsHardwareSQRT() { return false; }
   bool getSupportsHardwareRound() { return false; }
   bool getSupportsHardwareCopySign() { return false; }
   bool hasPopulationCountInstruction() { return false; }
   bool hasBitCompressInstruction() { return false; }
   bool hasBitExpandInstruction() { return false; }
   bool supportsDecimalFloatingPoint() { return false; }
   bool hasFPU() { return true; }

   /**
    * @brief Determines whether 32bit integer rotate is available
    *
    * @details
    *    Returns true if 32bit integer rotate to left is available when requireRotateToLeft is true.
    *    Returns true if 32bit integer rotate (right or left) is available when requireRotateToLeft is false.
    *
    * @param requireRotateToLeft if true, returns true if rotate to left operation is available.
    */
   bool getSupportsHardware32bitRotate(bool requireRotateToLeft=false) { return false; }
   /**
    * @brief Determines whether 64bit integer rotate is available
    *
    * @details
    *    Returns true if 64bit integer rotate to left is available when requireRotateToLeft is true.
    *    Returns true if 64bit integer rotate (right or left) is available when requireRotateToLeft is false.
    *
    * @param requireRotateToLeft if true, returns true if rotate to left operation is available.
    */
   bool getSupportsHardware64bitRotate(bool requireRotateToLeft=false) { return false; }

   TR::Endianness endianness() { return _endianness; }
   void setEndianness(TR::Endianness e) { _endianness = e; }
   bool isBigEndian() { return _endianness == TR::endian_big; }
   bool isLittleEndian() { return _endianness == TR::endian_little; }

   TR::MajorArchitecture majorArch() { return _majorArch; }
   void setMajorArch(TR::MajorArchitecture a) { _majorArch = a; }
   bool isX86() { return _majorArch == TR::arch_x86; }
   bool isZ() { return _majorArch == TR::arch_z; }
   bool isPower() { return _majorArch == TR::arch_power; }
   bool isARM() { return _majorArch == TR::arch_arm; }
   bool isARM64() { return _majorArch == TR::arch_arm64; }
   bool isRISCV() { return _majorArch == TR::arch_riscv; }

   TR::MinorArchitecture minorArch() { return _minorArch; }
   void setMinorArch(TR::MinorArchitecture a) { _minorArch = a; }
   bool isI386() { return _minorArch == TR::m_arch_i386; }
   bool isAMD64() { return _minorArch == TR::m_arch_amd64; }

   /**
    * @brief Determines whether the Transactional Memory (TM) facility is available on the current processor.
    * @return false; this is the default answer unless overridden by an extending class.
    */
   bool supportsTransactionalMemoryInstructions() { return false; }

   /**
    * @brief Determines whether current processor is the same as the input processor type
    * @param[in] p : the input processor type
    * @return true when current processor is the same as the input processor type
    */
   bool is(OMRProcessorArchitecture p) { return _processorDescription.processor == p; }

   /**
    * @brief Determines whether current processor is equal or newer than the input processor type
    * @param[in] p : the input processor type
    * @return true when current processor is equal or newer than the input processor type
    */
   bool isAtLeast(OMRProcessorArchitecture p) { return _processorDescription.processor >= p; }

   /**
    * @brief Determines whether current processor is equal or older than the input processor type
    * @param[in] p : the input processor type
    * @return true when current processor is equal or newer than the input processor type
    */
   bool isAtMost(OMRProcessorArchitecture p) { return _processorDescription.processor <= p; }

   /**
    * @brief Retrieves current processor's processor description
    * @return processor description which includes processor type and processor features
    */
   OMRProcessorDesc getProcessorDescription() { return _processorDescription; }

   /**
    * @brief Determines whether current processor supports the input processor feature
    * @param[in] feature : the input processor feature
    * @return true when current processor supports the input processor feature
    */
   bool supportsFeature(uint32_t feature);

   /**
    * @brief Returns name of the current processor
    * @returns const char* string representing the name of the current processor
    */
   const char* getProcessorName() { return "Unknown Processor"; }

protected:
   OMRProcessorDesc _processorDescription;

private:
   TR_Processor _processor;

   TR::Endianness _endianness;
   TR::MajorArchitecture _majorArch;
   TR::MinorArchitecture _minorArch;
   };
}

#endif
