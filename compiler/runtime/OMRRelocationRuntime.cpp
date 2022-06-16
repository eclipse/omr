/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#include "infra/Assert.hpp"
#include "runtime/TRRelocationRuntime.hpp"
#include "runtime/TRRelocationRecord.hpp"
#include "runtime/TRRelocationTarget.hpp"
#include "env/AOTMethodHeader.hpp"
#include "infra/STLUtils.hpp"

/**
 * @brief Construct a new OMR::RelocationRuntime::RelocationRuntime object
 * The RelocationRuntime, as well as RelocationTarget classes are ported
 * from OpenJ9 into OMR. RelocationRuntime is responsible for temporarily
 * storing relocations at the stage of their creation (at compile time).
 * During the load-time the relocation runtime creates relocationRecord
 * groups that iterate over relocation buffer and apply the actual relocations.
 * RelocationRuntime also holds reference to an architecture specific relocationTarget
 *  (that was transformed into an extensible class).
 */
OMR::RelocationRuntime::RelocationRuntime():
   _itemLocation(str_comparator)
   {
#if defined(TR_HOST_X86)
   #if defined(TR_HOST_64BIT)
   _reloTarget =  (new (PERSISTENT_NEW) TR::RelocationTarget(self()));
   #else
   TR_UNIMPLEMENTED();
   #endif
#else
   TR_UNIMPLEMENTED();
#endif

   if (_reloTarget == NULL)
      {
      TR_ASSERT(0, "Was unable to create a reloTarget, cannot use Relocations");
      }

   _relocationStatus = AOTRelocationStatus::RelocationNoError;
   }

TR::RelocationRuntime *
OMR::RelocationRuntime::self()
   {
   return static_cast<TR::RelocationRuntime*>(this);
   }

void
OMR::RelocationRuntime::relocateMethodAndData(TR::AOTMethodHeader *aotMethodHeaderEntry)
   {
   if (aotMethodHeaderEntry->getCompiledCodeStart()!= NULL)
      {
      _relocationStatus = AOTRelocationStatus::RelocationNoError;
      }
   else
      {
      _relocationStatus = AOTRelocationStatus::RelocationErrorNoCode;
      }

   if (_relocationStatus == RelocationNoError)
      {
      TR::RelocationRecordBinaryTemplate * binaryReloRecords = aotMethodHeaderEntry->getRelocationsStart();
      TR::RelocationRecordGroup reloGroup(binaryReloRecords);
      if ( aotMethodHeaderEntry->getRelocationsSize() != 0)
         {
         reloGroup.applyRelocations(self(),self()->reloTarget(), aotMethodHeaderEntry->getCompiledCodeStart());
         }
      }

   TR_ASSERT (_relocationStatus==AOTRelocationStatus::RelocationNoError, "Relocations cannot be applied with the error code %i ", _relocationStatus);

   }

void *
OMR::RelocationRuntime::runtimeItemAddress(const char *itemName)
   {
   auto it = _itemLocation.find(itemName);
   if (it == _itemLocation.end())
      {
      return NULL;
      }
   return _itemLocation[itemName];
   }

void
OMR::RelocationRuntime::registerRuntimeItemAddress(const char *itemName, void *itemAddress)
   {
   _itemLocation[itemName] = itemAddress;
   }
