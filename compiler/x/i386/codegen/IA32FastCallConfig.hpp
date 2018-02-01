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

// Windows: fastcall with integer parameters only
// Linux:   fastcall + sseregparm

const TR::RealRegister::RegNum TR::X86FastCallCallSite::IntParamRegisters[] =
   {
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::FloatParamRegisters[] =
   {
   TR::RealRegister::xmm0,
   TR::RealRegister::xmm1,
   TR::RealRegister::xmm2,
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::CallerSavedRegisters[] =
   {
   TR::RealRegister::eax,
   TR::RealRegister::ecx,
   TR::RealRegister::edx,
   TR::RealRegister::edi,
   TR::RealRegister::esi,
   TR::RealRegister::xmm0,
   TR::RealRegister::xmm1,
   TR::RealRegister::xmm2,
   TR::RealRegister::xmm3,
   TR::RealRegister::xmm4,
   TR::RealRegister::xmm5,
   TR::RealRegister::xmm6,
   TR::RealRegister::xmm7,
   };

const TR::RealRegister::RegNum TR::X86FastCallCallSite::CalleeSavedRegisters[] =
   {
   TR::RealRegister::ebx,
   TR::RealRegister::edi,
   TR::RealRegister::esi,
   TR::RealRegister::ebp,
   TR::RealRegister::esp,
   };

const bool   TR::X86FastCallCallSite::CalleeCleanup = true;
const size_t TR::X86FastCallCallSite::StackSlotSize = 4;
const bool   TR::X86FastCallCallSite::RegisterParameterShadowOnStack = false;
