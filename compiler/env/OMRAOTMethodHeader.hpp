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
#ifndef OMR_AOT_METHOD_HEADER
#define OMR_AOT_METHOD_HEADER

#ifndef OMR_AOTMETHODHEADER_CONNECTOR
#define OMR_AOTMETHODHEADER_CONNECTOR
namespace OMR { class AOTMethodHeader; }
namespace OMR { typedef OMR::AOTMethodHeader AOTMethodHeaderConnector; }
#endif

#include "infra/Annotations.hpp"
#include "env/jittypes.h"

namespace TR
{
   class AOTMethodHeader;
   class RelocationRecordBinaryTemplate;
}

namespace OMR
{

/**
 * @brief  The AOTMethodHeader files
 * are extensible class hierarchy proposed to be used for serialization and
 * deserialization of relocations buffer to and from the storage, alongside
 * of the compiled code. This class is supposed to be extended for caching
 * more data on the level of compiled code.
 *
 */
class OMR_EXTENSIBLE AOTMethodHeader
   {
public:
   /**
    * at compile time, the constructor runs with four arguments,
    * compiledCodeStart,  compiledCodeSize,  and relocationsStart, relocationsSize
    * at load time we don't know anything, so we run constructor with no
    * parameters and the values from cache are derived.
    * This is one possible implementation, for a cache with contiguous
    * code and relocations data stored along them
    */
   AOTMethodHeader(uint8_t *compiledCodeStart, uint32_t compiledCodeSize, TR::RelocationRecordBinaryTemplate *binaryTemplateStart, uint32_t relocationsSize);
   // This constructor could be used for creation of an AOT Method header from a cache message
   AOTMethodHeader(uint8_t *serializedMethodData);

   /**
    * @brief Serializes contents of the AOT Method Header
    * to a specified memory location
    * @param buffer
    * The memory location to serialize AOTMethodHeader into
    * @param bufferSize
    * The memory the size of the buffer allocated for serialization
    */
   void serializeMethod(uint8_t *buffer, size_t bufferSize);

   /**
    * @brief Method that computes the size of the header
    * based on its internal "size" fields
    *
    * @return size_t
    */
   size_t sizeOfSerializedVersion();

   /**
    * @brief Set the Compiled Code Start
    *
    */
   void setCompiledCodeStart(uint8_t*);
   /**
    * @brief Get the Compiled Code Start
    *
    * @return uint8_t*
    */
   uint8_t* getCompiledCodeStart();

   /**
    * @brief Set the Compiled Code Size object
    *
    */
   void setCompiledCodeSize(uint32_t);

   /**
    * @brief Get the Compiled Code Size
    *
    * @return uint32_t
    */
   uint32_t getCompiledCodeSize();

   /**
    * @brief Sets the _relocationsBinaryTemplate field
    */
   void setRelocationsStart(TR::RelocationRecordBinaryTemplate*);

   /**
    * @brief Get the _relocationsBinaryTemplate field
    *
    * @return TR::RelocationRecordBinaryTemplate*
    */
   TR::RelocationRecordBinaryTemplate* getRelocationsStart();

   void setRelocationsSize(uint32_t);
   uint32_t getRelocationsSize();

protected:
   TR::AOTMethodHeader* self();

private:
   uint8_t *_compiledCodeStart;
   uint32_t _compiledCodeSize;
   TR::RelocationRecordBinaryTemplate *_relocationsBinaryTemplate;
   uint32_t _relocationsSize;

   };
} // namespace OMR
#endif //#ifndef OMR_AOT_METHOD_HEADER
