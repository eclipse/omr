/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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

// On zOS XLC linker can't handle files with same name at link time.
// This workaround with pragma is needed. What this does is essentially
// give a different name to the codesection (csect) for this file. So it
// doesn't conflict with another file with the same name.
#pragma csect(CODE,"OMRZCPUBase#C")
#pragma csect(STATIC,"OMRZCPUBase#S")
#pragma csect(TEST,"OMRZCPUBase#T")

#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CPU.hpp"

TR::CPU
OMR::Z::CPU::detect(OMRPortLibrary * const omrPortLib)
   {
   if (omrPortLib == NULL)
      return TR::CPU();

   OMRPORT_ACCESS_FROM_OMRPORT(omrPortLib);
   OMRProcessorDesc processorDescription;
   omrsysinfo_get_processor_description(&processorDescription);

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z10)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_DFP, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z196)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_HIGH_WORD, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_ZEC12)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_RI, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z13)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z14)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_GUARDED_STORAGE, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z15)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_Z16)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2, FALSE);
      }

   if (processorDescription.processor < OMR_PROCESSOR_S390_ZNEXT)
      {
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_3, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_PLO_EXTENSION, FALSE);
      omrsysinfo_processor_set_feature(&processorDescription, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_3, FALSE);
      }

   return TR::CPU(processorDescription);
   }

bool
OMR::Z::CPU::isAtLeast(OMRProcessorArchitecture p)
   {
#if defined(TR_HOST_S390) && (defined(J9ZOS390) || defined(LINUX))
   if (TR::Compiler->omrPortLib == NULL)
      return self()->isAtLeastOldAPI(p);

   return _processorDescription.processor >= p;
#endif
   return false;
   }

bool
OMR::Z::CPU::supportsFeature(uint32_t feature)
   {
   if (TR::Compiler->omrPortLib == NULL)
      return self()->supportsFeatureOldAPI(feature);

   OMRPORT_ACCESS_FROM_OMRPORT(TR::Compiler->omrPortLib);
   return TRUE == omrsysinfo_processor_has_feature(&_processorDescription, feature);
   }

bool
OMR::Z::CPU::isAtLeastOldAPI(OMRProcessorArchitecture p)
   {
   bool ans = false;
   switch(p)
      {
      case OMR_PROCESSOR_S390_UNKNOWN:
         ans = self()->getSupportsArch(TR::CPU::Unknown);
         break;
      case OMR_PROCESSOR_S390_Z900:
         ans = self()->getSupportsArch(TR::CPU::z900);
         break;
      case OMR_PROCESSOR_S390_Z990:
         ans = self()->getSupportsArch(TR::CPU::z990);
         break;
      case OMR_PROCESSOR_S390_Z9:
         ans = self()->getSupportsArch(TR::CPU::z9);
         break;
      case OMR_PROCESSOR_S390_Z10:
         ans = self()->getSupportsArch(TR::CPU::z10);
         break;
      case OMR_PROCESSOR_S390_Z196:
         ans = self()->getSupportsArch(TR::CPU::z196);
         break;
      case OMR_PROCESSOR_S390_ZEC12:
         ans = self()->getSupportsArch(TR::CPU::zEC12);
         break;
      case OMR_PROCESSOR_S390_Z13:
         ans = self()->getSupportsArch(TR::CPU::z13);
         break;
      case OMR_PROCESSOR_S390_Z14:
         ans = self()->getSupportsArch(TR::CPU::z14);
         break;
      case OMR_PROCESSOR_S390_Z15:
         ans = self()->getSupportsArch(TR::CPU::z15);
         break;
      case OMR_PROCESSOR_S390_Z16:
         ans = self()->getSupportsArch(TR::CPU::z16);
         break;
      case OMR_PROCESSOR_S390_ZNEXT:
         ans = self()->getSupportsArch(TR::CPU::zNext);
         break;
      default:
         TR_ASSERT_FATAL(false, "Unknown processor: %d!\n", p);
      }
   return ans;
   }

bool
OMR::Z::CPU::supportsFeatureOldAPI(uint32_t feature)
   {
   bool supported = false;
   switch(feature)
      {
      case OMR_FEATURE_S390_HIGH_WORD:
         supported = self()->getSupportsHighWordFacility();
         break;
      case OMR_FEATURE_S390_DFP:
         supported = self()->getSupportsDecimalFloatingPointFacility();
         break;
      case OMR_FEATURE_S390_FPE:
         supported = self()->getSupportsFloatingPointExtensionFacility();
         break;
      case OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY:
         supported = self()->getSupportsConstrainedTransactionalExecutionFacility();
         break;
      case OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY:
         supported = self()->getSupportsTransactionalExecutionFacility();
      case OMR_FEATURE_S390_RI:
         supported = self()->getSupportsRuntimeInstrumentationFacility();
         break;
      case OMR_FEATURE_S390_VECTOR_FACILITY:
         supported = self()->getSupportsVectorFacility();
         break;
      case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL:
         supported = self()->getSupportsVectorPackedDecimalFacility();
         break;
      case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3:
         supported = self()->getSupportsMiscellaneousInstructionExtensions3Facility();
         break;
      case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2:
         supported = self()->getSupportsVectorFacilityEnhancement2();
         break;
      case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY:
         supported = self()->getSupportsVectorPackedDecimalEnhancementFacility();
         break;
      case OMR_FEATURE_S390_GUARDED_STORAGE:
         supported = self()->getSupportsGuardedStorageFacility();
         break;
      case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2:
         supported = self()->getSupportsMiscellaneousInstructionExtensions2Facility();
         break;
      case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1:
         supported = self()->getSupportsVectorFacilityEnhancement1();
         break;
      case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2:
         supported = self()->getSupportsVectorPackedDecimalEnhancementFacility2();
         break;
      case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_4:
         supported = self()->getSupportsMiscellaneousInstructionExtensionsFacility4();
         break;
      case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_3:
         supported = self()->getSupportsVectorFacilityEnhancement3();
         break;
      case OMR_FEATURE_S390_PLO_EXTENSION:
         supported = self()->getSupportsPLOExtensionFacility();
         break;
      case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_3:
         supported = self()->getSupportsVectorPackedDecimalEnhancementFacility3();
         break;
      default:
         TR_ASSERT_FATAL(false, "Unknown processor feature: %d!\n", feature);
      }
   return supported;
   }

const char*
OMR::Z::CPU::getProcessorName()
   {
   const char* result = "";

   switch(_processorDescription.processor)
      {
      case OMR_PROCESSOR_S390_UNKNOWN:
         result = "Unknown";
         break;
      case OMR_PROCESSOR_S390_Z900:
         result = "z900";
         break;
      case OMR_PROCESSOR_S390_Z990:
         result = "z990";
         break;
      case OMR_PROCESSOR_S390_Z9:
         result = "z9";
         break;
      case OMR_PROCESSOR_S390_Z10:
         result = "z10";
         break;
      case OMR_PROCESSOR_S390_Z196:
         result = "z196";
         break;
      case OMR_PROCESSOR_S390_ZEC12:
         result = "zEC12";
         break;
      case OMR_PROCESSOR_S390_Z13:
         result = "z13";
         break;
      case OMR_PROCESSOR_S390_Z14:
         result = "z14";
         break;
      case OMR_PROCESSOR_S390_Z15:
         result = "z15";
         break;
      case OMR_PROCESSOR_S390_Z16:
         result = "z16";
         break;
      case OMR_PROCESSOR_S390_ZNEXT:
         result = "zNext";
         break;
      default:
         result = "Invalid Architecture type!";
         break;
      }
   return result;
   }

bool
OMR::Z::CPU::getSupportsArch(Architecture arch)
   {
   return _supportedArch >= arch;
   }

bool
OMR::Z::CPU::getSupportsHardwareSQRT()
   {
   return true;
   }

bool
OMR::Z::CPU::hasPopulationCountInstruction()
   {
   return self()->supportsFeature(OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3);
   }

bool
OMR::Z::CPU::getSupportsHighWordFacility()
   {
   return _flags.testAny(S390SupportsHPR);
   }

void
OMR::Z::CPU::setSupportsHighWordFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsHPR);
      }
   else
      {
      _flags.reset(S390SupportsHPR);
      }
   }

bool
OMR::Z::CPU::getSupportsDecimalFloatingPointFacility()
   {
   return _flags.testAny(S390SupportsDFP);
   }

void
OMR::Z::CPU::setSupportsDecimalFloatingPointFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsDFP);
      }
   else
      {
      _flags.reset(S390SupportsDFP);
      }
   }

bool
OMR::Z::CPU::getSupportsFloatingPointExtensionFacility()
   {
   return _flags.testAny(S390SupportsFPE);
   }

void
OMR::Z::CPU::setSupportsFloatingPointExtensionFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsFPE);
      }
   else
      {
      _flags.reset(S390SupportsFPE);
      }
   }

bool
OMR::Z::CPU::getSupportsTransactionalExecutionFacility()
   {
   return _flags.testAny(OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY);
   }

bool
OMR::Z::CPU::supportsTransactionalMemoryInstructions()
   {
   return self()->supportsFeature(OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY);
   }


void
OMR::Z::CPU::setSupportsTransactionalExecutionFacility(bool value)
   {
   if (value)
      {
      _flags.set(OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY);
      }
   else
      {
      _flags.reset(OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY);
      }
   }

bool
OMR::Z::CPU::getSupportsConstrainedTransactionalExecutionFacility()
   {
   return _flags.testAny(OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY);
   }

void
OMR::Z::CPU::setSupportsConstrainedTransactionalExecutionFacility(bool value)
   {
   if (value)
      {
      _flags.set(OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY);
      }
   else
      {
      _flags.reset(OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY);
      }
   }

bool
OMR::Z::CPU::getSupportsRuntimeInstrumentationFacility()
   {
   return _flags.testAny(S390SupportsRI);
   }

void
OMR::Z::CPU::setSupportsRuntimeInstrumentationFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsRI);
      }
   else
      {
      _flags.reset(S390SupportsRI);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorFacility()
   {
   return _flags.testAny(S390SupportsVectorFacility);
   }

void
OMR::Z::CPU::setSupportsVectorFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorFacility);
      }
   else
      {
      _flags.reset(S390SupportsVectorFacility);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorPackedDecimalFacility()
   {
   return _flags.testAny(S390SupportsVectorPackedDecimalFacility);
   }

void
OMR::Z::CPU::setSupportsVectorPackedDecimalFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorPackedDecimalFacility);
      }
   else
      {
      _flags.reset(S390SupportsVectorPackedDecimalFacility);
      }
   }

bool
OMR::Z::CPU::getSupportsGuardedStorageFacility()
   {
   return _flags.testAny(S390SupportsGuardedStorageFacility);
   }

void
OMR::Z::CPU::setSupportsGuardedStorageFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsGuardedStorageFacility);
      }
   else
      {
      _flags.reset(S390SupportsGuardedStorageFacility);
      }
   }

bool
OMR::Z::CPU::getSupportsMiscellaneousInstructionExtensions2Facility()
   {
   return _flags.testAny(S390SupportsMIE2);
   }

void
OMR::Z::CPU::setSupportsMiscellaneousInstructionExtensions2Facility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsMIE2);
      }
   else
      {
      _flags.reset(S390SupportsMIE2);
      }
   }

bool
OMR::Z::CPU::getSupportsMiscellaneousInstructionExtensions3Facility()
   {
   return _flags.testAny(S390SupportsMIE3);
   }

void
OMR::Z::CPU::setSupportsMiscellaneousInstructionExtensions3Facility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsMIE3);
      }
   else
      {
      _flags.reset(S390SupportsMIE3);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorFacilityEnhancement2()
   {
   return _flags.testAny(S390SupportsVectorFacilityEnhancement2);
   }

void
OMR::Z::CPU::setSupportsVectorFacilityEnhancement2(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorFacilityEnhancement2);
      }
   else
      {
      _flags.reset(S390SupportsVectorFacilityEnhancement2);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorFacilityEnhancement1()
   {
   return _flags.testAny(S390SupportsVectorFacilityEnhancement1);
   }

void
OMR::Z::CPU::setSupportsVectorFacilityEnhancement1(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorFacilityEnhancement1);
      }
   else
      {
      _flags.reset(S390SupportsVectorFacilityEnhancement1);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorPackedDecimalEnhancementFacility()
   {
   return _flags.testAny(S390SupportsVectorPDEnhancementFacility);
   }

void
OMR::Z::CPU::setSupportsVectorPackedDecimalEnhancementFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorPDEnhancementFacility);
      }
   else
      {
      _flags.reset(S390SupportsVectorPDEnhancementFacility);
      }
   }

bool
OMR::Z::CPU::isTargetWithinBranchRelativeRILRange(intptr_t targetAddress, intptr_t sourceAddress)
   {
   return (targetAddress == sourceAddress + ((intptr_t)((int32_t)((targetAddress - sourceAddress) / 2))) * 2) &&
            (targetAddress % 2 == 0);
   }

bool
OMR::Z::CPU::getSupportsVectorPackedDecimalEnhancementFacility2()
   {
   return _flags.testAny(S390SupportsVectorPDEnhancementFacility2);
   }

void
OMR::Z::CPU::setSupportsVectorPackedDecimalEnhancementFacility2(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorPDEnhancementFacility2);
      }
   else
      {
      _flags.reset(S390SupportsVectorPDEnhancementFacility2);
      }
   }

bool
OMR::Z::CPU::getSupportsMiscellaneousInstructionExtensionsFacility4()
   {
   return _flags.testAny(S390SupportsMIE4);
   }

void
OMR::Z::CPU::setSupportsMiscellaneousInstructionExtensionsFacility4(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsMIE4);
      }
   else
      {
      _flags.reset(S390SupportsMIE4);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorFacilityEnhancement3()
   {
   return _flags.testAny(S390SupportsVectorFacilityEnhancement3);
   }

void
OMR::Z::CPU::setSupportsVectorFacilityEnhancement3(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorFacilityEnhancement3);
      }
   else
      {
      _flags.reset(S390SupportsVectorFacilityEnhancement3);
      }
   }

bool
OMR::Z::CPU::getSupportsPLOExtensionFacility()
   {
   return _flags.testAny(S390SupportsPLO);
   }

void
OMR::Z::CPU::setSupportsPLOExtensionFacility(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsPLO);
      }
   else
      {
      _flags.reset(S390SupportsPLO);
      }
   }

bool
OMR::Z::CPU::getSupportsVectorPackedDecimalEnhancementFacility3()
   {
   return _flags.testAny(S390SupportsVectorPDEnhancementFacility3);
   }

void
OMR::Z::CPU::setSupportsVectorPackedDecimalEnhancementFacility3(bool value)
   {
   if (value)
      {
      _flags.set(S390SupportsVectorPDEnhancementFacility3);
      }
   else
      {
      _flags.reset(S390SupportsVectorPDEnhancementFacility3);
      }
   }
