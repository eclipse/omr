/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include <stdint.h>
#include <cstring>
#include <limits>

#include "ilgen/JitBuilderRecorderBinaryBuffer.hpp"
#include "infra/Assert.hpp"

OMR::JitBuilderRecorderBinaryBuffer::JitBuilderRecorderBinaryBuffer(const TR::MethodBuilder *mb, const char *fileName)
   : TR::JitBuilderRecorder(mb, fileName), _buf()
   {
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Close()
   {
   TR::JitBuilderRecorder::Close();
   }

void
OMR::JitBuilderRecorderBinaryBuffer::String(const char * const string)
   {
   // length(int16) characters
   const size_t len = strlen(string);
   const int max = std::numeric_limits<int16_t>::max();
   TR_ASSERT_FATAL(len < max, "JBIL: binary String format limited to %d characters", max);

   Number((int16_t)len);
   for (int16_t i=0;i < len;i++)
      _buf.push_back((uint8_t)string[i]);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(int8_t num)
   {
   _buf.push_back(*(uint8_t *)&num);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(int16_t num)
   {
   _buf.push_back((uint8_t)num&0xFF);
   _buf.push_back((uint8_t)(num&0xFF00)>>8);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(int32_t signedNum)
   {
   uint32_t *num = (uint32_t *)&signedNum;
   uint8_t b0 = (uint8_t)  (*num & 0x000000FF);
   uint8_t b1 = (uint8_t) ((*num & 0x0000FF00) >> 8);
   uint8_t b2 = (uint8_t) ((*num & 0x00FF0000) >> 16);
   uint8_t b3 = (uint8_t) ((*num & 0xFF000000) >> 24);
   _buf.push_back(b0);
   _buf.push_back(b1);
   _buf.push_back(b2);
   _buf.push_back(b3);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(int64_t signedNum)
   {
   uint64_t *num = (uint64_t *)&signedNum;
   uint8_t b0 = (uint8_t)  (*num & 0x00000000000000FFUL);
   uint8_t b1 = (uint8_t) ((*num & 0x000000000000FF00UL) >> 8);
   uint8_t b2 = (uint8_t) ((*num & 0x0000000000FF0000UL) >> 16);
   uint8_t b3 = (uint8_t) ((*num & 0x00000000FF000000UL) >> 24);
   uint8_t b4 = (uint8_t) ((*num & 0x000000FF00000000UL) >> 32);
   uint8_t b5 = (uint8_t) ((*num & 0x0000FF0000000000UL) >> 40);
   uint8_t b6 = (uint8_t) ((*num & 0x00FF000000000000UL) >> 48);
   uint8_t b7 = (uint8_t) ((*num & 0xFF00000000000000UL) >> 56);
   _buf.push_back(b0);
   _buf.push_back(b1);
   _buf.push_back(b2);
   _buf.push_back(b3);
   _buf.push_back(b4);
   _buf.push_back(b5);
   _buf.push_back(b6);
   _buf.push_back(b7);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(float floatNum)
   {
   int32_t *num = (int32_t *)&floatNum;
   Number(*num);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Number(double doubleNum)
   {
   int64_t *num = (int64_t *)&doubleNum;
   Number(*num);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::ID(TypeID id)
   {
   if (_idSize == 8)
      {
      // all known IDs fit in 8 bits
      uint8_t idUnsigned = (uint8_t)id;
      int8_t *num = (int8_t *)&idUnsigned;
      Number(*num);
      }
   else if (_idSize == 16)
      {
      // all known IDs fit in 16 bits
      uint16_t idUnsigned = (uint16_t)id;
      int16_t *num = (int16_t *)&idUnsigned;
      Number(*num);
      }
   else if (_idSize == 32)
      {
      // TypeID can only hold 32-bits so that's the most we'll need
      uint32_t idUnsigned = (uint32_t)id;
      int32_t *num = (int32_t *)&idUnsigned;
      Number(*num);
      }
   else
      TR_ASSERT_FATAL(0, "Unexpected id size value %d", _idSize);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Statement(const char *s)
   {
   ID(lookupID(s));
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Type(const TR::IlType *type)
   {
   ID(lookupID(type));
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Value(const TR::IlValue *v)
   {
   ID(lookupID(v));
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Builder(const TR::IlBuilder *b)
   {
   ID(lookupID(b));
   }

void
OMR::JitBuilderRecorderBinaryBuffer::Location(const void *location)
   {
   int64_t num = (int64_t)location;
   Number(num);
   }

void
OMR::JitBuilderRecorderBinaryBuffer::EndStatement()
   {
   }
