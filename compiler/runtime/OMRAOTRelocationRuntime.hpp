/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef OMR_AOT_RELOCATION_RUNTIME_INCL
#define OMR_AOT_RELOCATION_RUNTIME_INCL

#ifndef OMR_AOT_RELOCATION_RUNTIME_CONNECTOR
#define OMR_AOT_RELOCATION_RUNTIME_CONNECTOR
namespace OMR { class OMRAOTRelocationRuntime; }
namespace OMR { typedef OMR::OMRAOTRelocationRuntime OMRAOTRelocationRuntimeConnector; }
#endif

#include "runtime/TRRelocationRuntime.hpp"
#include <map>

namespace TR { class RelocationRuntime; }

namespace OMR 
{
/**
 * @brief OMRAOTRelocationRuntime holds a
 * lookup by name map that would allow relocation runtime to have named
 * symbol addresses and would allow linking external functions into runtime.
 * The keys being strings is an intermediate in-development solution is
 * supposed to change into something more suitable for a method
 * identification, Resolved Method being a candidate for replacement. For
 * now, maintaining the string being sufficient for a method verification
 * is left as the runtime constructor responsibility.
 */
class OMR_EXTENSIBLE OMRAOTRelocationRuntime : public TR::RelocationRuntime 
   {
public:
   TR_ALLOC(TR_Memory::Relocation);
   void * operator new(size_t);
   OMRAOTRelocationRuntime() : TR::RelocationRuntime() {}

   void *persistedItemAddress(const char *itemName);
   // Register persisted item allows keeping track of 
   // loaded items that AOTRelocation Runtime
   // might need to relocate, for example functions
   void registerPersistedItemAddress(const char *itemName, void *itemAddress);

protected:
   TR::RelocationRuntime* self();

private:
   /**The keys being strings is an intermediate in-development solution is
    * supposed to change into something more suitable for a method 
    * identification, Resolved Method being a candidate for replacement. For
    * now, maintaining the string being sufficient for a method verification 
    * is left as the runtime constructor responsibility.
    */
   std::map<std::string,void *> _itemLocation;
   };

}// namespace OMR

#endif // ifndef OMR_AOT_RELOCATION_RUNTIME_INCL
