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

#ifndef TR_PROCESSORS_INCL
#define TR_PROCESSORS_INCL

namespace TR
{

enum Processor
   {

   UnknownProcessor,

   FirstARMProcessorMark,
      #include "arm/env/ARMProcessorEnum.hpp"
   LastARMProcessorMark,

   FirstPowerProcessorMark,
      #include "p/env/PPCProcessorEnum.hpp"
   LastPowerProcessorMark,

   FirstZProcessorMark,
      #include "z/env/S390ProcessorEnum.hpp"
   LastZProcessorMark,

   FirstX86ProcessorMark,
      #include "x/env/X86ProcessorEnum.hpp"
   LastX86ProcessorMark,

   NumARMProcessors = LastARMProcessorMark-FirstARMProcessorMark-1,
   NumPowerProcessors = LastPowerProcessorMark-FirstPowerProcessorMark-1,
   NumZProcessors = LastZProcessorMark-FirstZProcessorMark-1,
   NumX86Processors = LastX86ProcessorMark-FirstX86ProcessorMark-1

   };

}

enum TR_Processor
   {

   TR_NullProcessor,
   TR_FirstProcessor,

   // 390 Processors
   TR_First390Processor = TR_FirstProcessor,
   TR_Default390Processor = TR_FirstProcessor,
   TR_s370,
   TR_s370gp1,
   TR_s370gp2,
   TR_s370gp3,
   TR_s370gp4,
   TR_s370gp5,
   TR_s370gp6,
   TR_s370gp7,
   TR_s370gp8,
   TR_s370gp9,
   TR_s370gp10,
   TR_s370gp11,
   TR_s370gp12,
   TR_s370gp13,
   TR_s370gp14,
   TR_Last390Processor = TR_s370gp14,

   // ARM Processors
   TR_FirstARMProcessor,
   TR_DefaultARMProcessor = TR_FirstARMProcessor,
   TR_ARMv6,
   TR_LastARMProcessor,
   TR_ARMv7 = TR_LastARMProcessor,

   // PPC Processors
   // This list is in order
   TR_FirstPPCProcessor,
   TR_DefaultPPCProcessor = TR_FirstPPCProcessor,
   TR_PPCrios1,
   TR_PPCpwr403,
   TR_PPCpwr405,
   TR_PPCpwr440,
   TR_PPCpwr601,
   TR_PPCpwr602,
   TR_PPCpwr603,
   TR_PPC82xx,
   TR_PPC7xx,
   TR_PPCpwr604,
   // The following processors support SQRT in hardware
   TR_FirstPPCHwSqrtProcessor,
   TR_PPCrios2 = TR_FirstPPCHwSqrtProcessor, // double precision sqrt only
   TR_PPCpwr2s,                              // basically the same as a rios2
   // The following processors are 64-bit implementations
   TR_FirstPPC64BitProcessor,
   TR_PPCpwr620 = TR_FirstPPC64BitProcessor,
   TR_PPCpwr630,
   TR_PPCnstar,
   TR_PPCpulsar,
   // The following processors support the PowerPC AS architecture
   // PPC AS includes the new branch hint 'a' and 't' bits
   TR_FirstPPCASProcessor,
   TR_PPCgp = TR_FirstPPCASProcessor,
   TR_PPCgr,
   // The following processors support VMX
   TR_FirstPPCVMXProcessor,
   TR_PPCgpul = TR_FirstPPCVMXProcessor,
   TR_FirstPPCHwRoundProcessor, // supports hardware FP rounding
   TR_FirstPPCHwCopySignProcessor = TR_FirstPPCHwRoundProcessor, // supports hardware FP copy sign
   TR_PPCp6 = TR_FirstPPCHwCopySignProcessor,
   TR_PPCatlas,
   TR_PPCbalanced,
   TR_PPCcellpx,
   // The following processors support VSX
   TR_FirstPPCVSXProcessor,
   TR_PPCp7 = TR_FirstPPCVSXProcessor,
   TR_PPCp8,
   TR_LastPPCProcessor,
   TR_PPCp9 = TR_LastPPCProcessor,

   // X86 Processors
   TR_FirstX86Processor,
   TR_DefaultX86Processor = TR_FirstX86Processor,
   TR_X86ProcessorIntelPentium,
   TR_X86ProcessorIntelP6,
   TR_X86ProcessorIntelPentium4,
   TR_X86ProcessorIntelCore2,
   TR_X86ProcessorIntelTulsa,
   TR_X86ProcessorAMDK5,
   TR_X86ProcessorAMDK6,
   TR_X86ProcessorAMDAthlonDuron,
   TR_LastX86Processor,
   TR_X86ProcessorAMDOpteron = TR_LastX86Processor,

   TR_LastProcessor = TR_LastX86Processor,

   TR_NumberOfProcessors
   };

#endif
