/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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


#ifndef OMR_AOTSTORAGE_INCL
#define OMR_AOTSTORAGE_INCL



#include "infra/Annotations.hpp"
#include "env/AOTMethodHeader.hpp"
#include "env/jittypes.h"

namespace TR { class AOTStorage; }
namespace TR { class AOTMethodHeader; }

namespace OMR
{
/**
 * @brief The class is supposed to be inherited by
 * project level classes that implement three methods: store Entry, load Entry,
 * and allocate Entry, with entry being a Method Header. This will allow OMR
 * AOT infrastructure to work with the preserved data
 */
class AOTStorage
   {
public:
   /**
    * @brief method to load data for the
    * selected method, identified by a key
    * @param key
    * @return uint8_t* a pointer to the memory region
    * that contains the AOTMethodHeader
    */
   virtual TR::AOTMethodHeader* loadEntry(const char* key);

   /**
    * @brief method to store data for the
    * selected method header
    *
    * @param key an identifier
    * @param data pointer to data
    * @param size size of the data vector
    */
   virtual void storeEntry(const char* methodName, TR::AOTMethodHeader* header);

   /**
    * @brief Should return a pointer to the memory allocated for persisting data
    *
    * @param size: number of bytes requested
    * @return uintptr_t*  pointer to memory region
    */
   virtual uint8_t * allocateEntry(uintptr_t size);

   };

} // namespace OMR

#endif //OMR_AOTSTORAGE_INCL
