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

#ifndef OMR_RELOCATION_RUNTIME_INCL
#define OMR_RELOCATION_RUNTIME_INCL

#include "codegen/Relocation.hpp"
#include "runtime/Runtime.hpp"
#include "env/OMRCPU.hpp"

#ifndef OMR_RELOCATION_RUNTIME_CONNECTOR
#define OMR_RELOCATION_RUNTIME_CONNECTOR
namespace OMR { class RelocationRuntime; }
namespace OMR { typedef OMR::RelocationRuntime RelocationRuntimeConnector; }
#endif


namespace TR
{
   class RelocationRecord;
   class RelocationTarget;
   class RelocationRecordBinaryTemplate;
   class AOTMethodHeader;
   class RelocationRuntime;
} // namespace TR

namespace OMR
{
/**
 * @brief The RelocationRuntime and a class extending it, as well as RelocationTarget
 * classes are ported from OpenJ9 into OMR.
 * RelocationRuntime is responsible for temporarily storing relocations at
 * the stage of their creation (at compile time). During the load-time the
 * relocation runtime creates relocationRecord groups that iterate over
 * relocation buffer and apply the actual relocations. RelocationRuntime
 * also holds reference to an architecture specific relocationTarget
 * (that was transformed into an extensible class).
 */
class OMR_EXTENSIBLE RelocationRuntime
   {
public:
   TR_ALLOC(TR_Memory::Relocation)
   RelocationRuntime();
   TR::RelocationTarget *reloTarget()                           { return _reloTarget; }
   void relocateMethodAndData(TR::AOTMethodHeader *cacheEntry);

   /**
    * @brief Returns an address of runtime item -
    * loaded items that AOTRelocation Runtime
    * might need to relocate, for example functions
    * @param itemName
    * @return void* - pointer to the address
    */
   void *runtimeItemAddress(const char *itemName);
   /**
    * Register persisted item allows keeping track of
    * loaded items that AOTRelocation Runtime
    * might need to relocate, for example functions
    */
   void registerRuntimeItemAddress(const char *itemName, void *itemAddress);

protected:
   typedef enum
      {
      RelocationNoError = 1,
      RelocationErrorNoCode = 2
      } AOTRelocationStatus;
   AOTRelocationStatus  _relocationStatus;

   TR::RelocationRuntime *self();
   TR::RelocationTarget *_reloTarget;
   /**
    * @brief std::map requires an implementation of comparators
    * for types that are not parts of the standard c++ library
    */
   typedef bool (*StrComparator)(const char *, const char*);
   /**The keys being strings is an intermediate in-development solution is
    * supposed to change into something more suitable for a method
    * identification, Resolved Method being a candidate for replacement. For
    * now, maintaining the string being sufficient for a method verification
    * is left as the runtime constructor responsibility.
    */
   typedef std::map<const char *,void *,StrComparator> RuntimeItemToLocationMap;
   RuntimeItemToLocationMap _itemLocation;
   };

} //namespace OMR

#endif   // OMR_RELOCATION_RUNTIME_INCL
