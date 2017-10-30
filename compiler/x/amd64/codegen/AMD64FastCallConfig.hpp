/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "codegen/X86FastCallLinkage.hpp"

// Windows: Microsoft x64 calling convention
// Linux:   System V AMD64 ABI.

const TR::RealRegister::RegNum TR::X86FastCallCallSite::IntParamRegisters[] =
   {
#ifdef WINDOWS
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
#else
   TR::RealRegister::edi,
   TR::RealRegister::esi,
   TR::RealRegister::edx,
   TR::RealRegister::ecx,
#endif
   TR::RealRegister::r8,
   TR::RealRegister::r9,
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::FloatParamRegisters[] =
   {
   TR::RealRegister::xmm0,
   TR::RealRegister::xmm1,
   TR::RealRegister::xmm2,
   TR::RealRegister::xmm3,
#ifndef WINDOWS
   TR::RealRegister::xmm4,
   TR::RealRegister::xmm5,
   TR::RealRegister::xmm6,
   TR::RealRegister::xmm7,
#endif
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::CallerSavedRegisters[] =
   {
   TR::RealRegister::eax,
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
#ifndef WINDOWS
   TR::RealRegister::edi,
   TR::RealRegister::esi,
#endif
   TR::RealRegister::r8,
   TR::RealRegister::r9,
   TR::RealRegister::r10,
   TR::RealRegister::r11,
   TR::RealRegister::xmm0,
   TR::RealRegister::xmm1,
   TR::RealRegister::xmm2,
   TR::RealRegister::xmm3,
   TR::RealRegister::xmm4,
   TR::RealRegister::xmm5,
   TR::RealRegister::xmm6,
   TR::RealRegister::xmm7,
   TR::RealRegister::xmm8,
   TR::RealRegister::xmm9,
   TR::RealRegister::xmm10,
   TR::RealRegister::xmm11,
   TR::RealRegister::xmm12,
   TR::RealRegister::xmm13,
   TR::RealRegister::xmm14,
   TR::RealRegister::xmm15,
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::CalleeSavedRegisters[] =
   {
   TR::RealRegister::ebx,
#ifdef WINDOWS
   TR::RealRegister::edi,
   TR::RealRegister::esi,
#endif
   TR::RealRegister::ebp,
   TR::RealRegister::esp,
   TR::RealRegister::r12,
   TR::RealRegister::r13,
   TR::RealRegister::r14,
   TR::RealRegister::r15,
   };

const bool   TR::X86FastCallCallSite::CalleeCleanup = false;
const size_t TR::X86FastCallCallSite::StackSlotSize = 8;

// Windows X86-64 requires caller reserves shadow space for parameters passed via registers
#ifdef WINDOWS
const bool TR::X86FastCallCallSite::RegisterParameterShadowOnStack = true;
#else
const bool TR::X86FastCallCallSite::RegisterParameterShadowOnStack = false;
#endif
