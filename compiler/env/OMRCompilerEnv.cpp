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

#include "env/CompilerEnv.hpp"
#include "env/Environment.hpp"
#include "env/CPU.hpp"
#include "env/defines.h"


OMR::CompilerEnv::CompilerEnv(
   TR::RawAllocator raw,
   const TR::PersistentAllocatorKit &persistentAllocatorKit,
   OMRPortLibrary * const portLib
   ) :
      rawAllocator(raw),
      _initialized(false),
      _persistentAllocator(persistentAllocatorKit),
      regionAllocator(_persistentAllocator),
      omrPortLib(portLib)
   {
   }


TR::CompilerEnv *OMR::CompilerEnv::self()
   {
   return static_cast<TR::CompilerEnv *>(this);
   }


void
OMR::CompilerEnv::initialize()
   {
   self()->initializeHostEnvironment();

   self()->initializeTargetEnvironment();

   self()->initializeRelocatableTargetEnvironment();

   om.initialize();

   _initialized = true;
   }


void
OMR::CompilerEnv::initializeHostEnvironment()
   {

   // Host processor bitness
   //
#ifdef TR_HOST_64BIT
   host.setBitness(TR::bits_64);
#elif TR_HOST_32BIT
   host.setBitness(TR::bits_32);
#else
   host.setBitness(TR::bits_unknown);
#endif

   // Initialize the host CPU by querying the host processor
   //
   host.cpu = TR::CPU::detect(TR::Compiler->omrPortLib);

   // Host major operating system
   //
#if HOST_OS == OMR_LINUX
   host.setMajorOS(TR::os_linux);
#elif HOST_OS == OMR_AIX
   host.setMajorOS(TR::os_aix);
#elif HOST_OS == OMR_WINDOWS
   host.setMajorOS(TR::os_windows);
#elif HOST_OS == OMR_ZOS
   host.setMajorOS(TR::os_zos);
#elif HOST_OS == OMR_MACOS
   host.setMajorOS(TR::os_osx);
#elif HOST_OS == OMR_BSD
   host.setMajorOS(TR::os_bsd);
#else
   host.setMajorOS(TR::os_unknown);
#endif

   host.setNumberOfProcessors(2);
   host.setSMP(true);
   }


// Projects are encouraged to over-ride this function in their project
// extension.
//
// By default, the target will be initialized to the same environment
// as the host.
//
void
OMR::CompilerEnv::initializeTargetEnvironment()
   {

   // Target processor bitness
   //
#ifdef TR_TARGET_64BIT
   target.setBitness(TR::bits_64);
#elif TR_TARGET_32BIT
   target.setBitness(TR::bits_32);
#else
   target.setBitness(TR::bits_unknown);
#endif

   // Initialize the target CPU by querying the host processor
   //
   target.cpu = TR::CPU::detect(TR::Compiler->omrPortLib);
   TR::CPU::initializeTargetProcessorInfo();

   // Target major operating system
   //
#if HOST_OS == OMR_LINUX
   target.setMajorOS(TR::os_linux);
#elif HOST_OS == OMR_AIX
   target.setMajorOS(TR::os_aix);
#elif HOST_OS == OMR_WINDOWS
   target.setMajorOS(TR::os_windows);
#elif HOST_OS == OMR_ZOS
   target.setMajorOS(TR::os_zos);
#elif HOST_OS == OMR_MACOS
   target.setMajorOS(TR::os_osx);
#elif HOST_OS == OMR_BSD
   target.setMajorOS(TR::os_bsd);
#else
   target.setMajorOS(TR::os_unknown);
#endif

   target.setNumberOfProcessors(2);
   target.setSMP(true);
   }


void
OMR::CompilerEnv::initializeRelocatableTargetEnvironment()
   {

   // Target processor bitness
   //
#ifdef TR_TARGET_64BIT
   relocatableTarget.setBitness(TR::bits_64);
#elif TR_TARGET_32BIT
   relocatableTarget.setBitness(TR::bits_32);
#else
   relocatableTarget.setBitness(TR::bits_unknown);
#endif

   // Initialize the relocatable target CPU by querying the host processor
   //
   relocatableTarget.cpu = TR::CPU::detect(TR::Compiler->omrPortLib);

   // Target major operating system
   //
#if HOST_OS == OMR_LINUX
   relocatableTarget.setMajorOS(TR::os_linux);
#elif HOST_OS == OMR_AIX
   relocatableTarget.setMajorOS(TR::os_aix);
#elif HOST_OS == OMR_WINDOWS
   relocatableTarget.setMajorOS(TR::os_windows);
#elif HOST_OS == OMR_ZOS
   relocatableTarget.setMajorOS(TR::os_zos);
#elif HOST_OS == OMR_OSX
   relocatableTarget.setMajorOS(TR::os_osx);
#else
   relocatableTarget.setMajorOS(TR::os_unknown);
#endif

   relocatableTarget.setNumberOfProcessors(2);
   relocatableTarget.setSMP(true);
   }
