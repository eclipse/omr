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

#ifndef OMR_RELOCATION_TARGET_INCL
#define OMR_RELOCATION_TARGET_INCL

#ifndef OMR_RELOCATION_TARGET_CONNECTOR
#define OMR_RELOCATION_TARGET_CONNECTOR
namespace OMR { class RelocationTarget; }
namespace OMR { typedef OMR::RelocationTarget RelocationTargetConnector; }
#endif


#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "env/JitConfig.hpp"

namespace TR
{
class RelocationRecord;
class RelocationRuntime;
class RelocationTarget;
}

namespace OMR
{

/**
 * @brief The relocation target is a class that is an abstraction over
 * relocatable code and data used in JIT compilation. By using this class,
 * Relocation Record and its subclasses can have platform-independent
 * implementations.
 */
class  OMR_EXTENSIBLE RelocationTarget
   {
public:
   TR_ALLOC(TR_Memory::Relocation)

   RelocationTarget(TR::RelocationRuntime *reloRuntime)
      {
      _reloRuntime = reloRuntime;
      }

   TR::RelocationRuntime *reloRuntime()                                   { return _reloRuntime; }

   uint8_t loadUnsigned8b(uint8_t *address)                      { return *address; }
   void storeUnsigned8b(uint8_t value, uint8_t *address)	    { *address = value; }
   int8_t loadSigned8b(uint8_t *address)                         { return *(int8_t *) address; }
   void storeSigned8b(int8_t value, uint8_t *address)            { *(int8_t *)address = value; }
   uint16_t loadUnsigned16b(uint8_t *address)                    { return *(uint16_t *) address; }
   void storeUnsigned16b(uint16_t value, uint8_t *address)        { *(uint16_t *)address = value; }
   int16_t loadSigned16b(uint8_t *address)                       { return *(int16_t *) address; }
   void storeSigned16b(int16_t value, uint8_t *address)          { *(int16_t *)address = value; }
   uint32_t loadUnsigned32b(uint8_t *address)                    { return *(uint32_t *) address; }
   void storeUnsigned32b(uint32_t value, uint8_t *address)       { *(uint32_t *)address = value; }
   int32_t loadSigned32b(uint8_t *address)                       { return *(int32_t *) address; }
   void storeSigned32b(int32_t value, uint8_t *address)          { *(int32_t *)address = value; }
   /**
    * @brief Load a pointer value from a binary template.
    *
    * @param address - pointer to binary template
    * @return uint8_t* - address that is stored
    */
   uint8_t *loadPointer(uint8_t *address)                        { return *(uint8_t**) address; }
   /**
    * @brief Store a pointer value into address to the binary template.
    *
    * @param value - value to store
    * @param address - where to store within binary template.
    */
   void storePointer(uint8_t *value, uint8_t *address)           { *(uint8_t**)address = value; }


protected:
   TR::RelocationTarget* self();

private:
   TR::RelocationRuntime *_reloRuntime;
   };

} // namespace OMR

#endif   // OMR_RELOCATION_TARGET_INCL
