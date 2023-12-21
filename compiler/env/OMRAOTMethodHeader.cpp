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
#include "env/AOTMethodHeader.hpp"
#include "infra/Assert.hpp"
#include "runtime/TRRelocationRecord.hpp"
#include <stdlib.h>
#include <string.h>

TR::AOTMethodHeader*
OMR::AOTMethodHeader::self()
   {
   return reinterpret_cast<TR::AOTMethodHeader*> (this);
   }

OMR::AOTMethodHeader::AOTMethodHeader(uint8_t* compiledCodeStart, uint32_t compiledCodeSize, TR::RelocationRecordBinaryTemplate *relocationsBinaryTemplate, uint32_t relocationsSize):
      _compiledCodeStart(compiledCodeStart),
      _compiledCodeSize(compiledCodeSize),
      _relocationsSize(relocationsSize),
      _relocationsBinaryTemplate(relocationsBinaryTemplate)
      {};

void
OMR::AOTMethodHeader::serializeMethod(uint8_t* buffer,size_t bufferSize)
   {
   size_t allocSize = self()->sizeOfSerializedVersion();
   TR_ASSERT(bufferSize >= allocSize, "Insufficient memory allocated for AOT Method serialization.\n");

   uint8_t *ptr = buffer;

   // Copy the size of the header to the buffer
   memcpy(ptr,&allocSize,sizeof(size_t));

   // Skip the bytes just copied
   ptr += sizeof(size_t);

   uint32_t compiledCodeSize = self()->getCompiledCodeSize();
   // Copy the size of compiled code to the buffer
   *reinterpret_cast<uint32_t*>(ptr) = compiledCodeSize;

   // Skip the bytes just copied
   ptr += sizeof(uint32_t);

   // Copy the compiled code to the buffer
   memcpy(ptr,self()->getCompiledCodeStart(),compiledCodeSize);

   // Skip the bytes just copied
   ptr += compiledCodeSize;

   uint32_t relocationsSize = self()->getRelocationsSize();
   // Copy the size of relocations data to the buffer
   memcpy(ptr,&relocationsSize,sizeof(uint32_t));

   // Skip the bytes just copied
   ptr += sizeof(uint32_t);
   // Copy the relocations data to the buffer
   memcpy(ptr,reinterpret_cast<uint8_t*>(self()->getRelocationsStart()),relocationsSize);
   // Now the buffer contains all the data relevant to the
   // method header.
   }

OMR::AOTMethodHeader::AOTMethodHeader(uint8_t* serializedMethodData)
   {
   // This is an examplar implementation of a creating AOTMethodHeader
   // class from the raw data received from the storage container
   // The first sizeof(uintptr_t) bytes
   // stores the size of the AOTMethodHeader , we will use them
   // to assert the size computed by the header function matches
   // the stored size

   size_t storedSize = *((size_t*) serializedMethodData);
   serializedMethodData += sizeof(size_t);

   // The next sizeof(uint32_t) bytes contain the
   // size of compiled code
   self()->setCompiledCodeSize(*((uint32_t*) serializedMethodData));
   serializedMethodData += sizeof(uint32_t);

   // Right after the compliedCodeSize, the codeSize of bytes
   // are the code itself, we should record that as a part
   // of a header
   self()->setCompiledCodeStart(serializedMethodData);

   // We should skip size of compiled code in bytes
   // to get to next element of the header
   serializedMethodData += self()->getCompiledCodeSize();

   // That is relocation size, occupying the next uint32_t bytes
   self()->setRelocationsSize(*((uint32_t*) serializedMethodData));

   // We should skip the sizeof(uint32_t) in bytes to get
   // to the relocations information
   serializedMethodData += sizeof(uint32_t);

   // We take note of the relocationsStart to use the
   // relocations related information in future
   self()->setRelocationsStart(reinterpret_cast<TR::RelocationRecordBinaryTemplate*>(serializedMethodData));
   }

size_t
OMR::AOTMethodHeader::sizeOfSerializedVersion()
   {
   // An examplar computation of size  of serialized version
   // of an AOTMethodHeader, required for memory allocation purposes
   // The size should generally be composed of
   // A size_t to hold whole message size,
   // 2 elements of type uint32_t to store the size of the compiled code and
   // relocation data
   // compiledCodeSize of bytes (uint8_t) for compiledcode,
   // RelocationsSize of bytes (uint8_t) for relocation data.
   return sizeof(size_t) + 2*sizeof(uint32_t)+self()->getCompiledCodeSize()+self()->getRelocationsSize();
   }

void
OMR::AOTMethodHeader::setCompiledCodeStart(uint8_t* codeLocation)
   {
   _compiledCodeStart=codeLocation;
   }

uint8_t*
OMR::AOTMethodHeader::getCompiledCodeStart()
   {
   return _compiledCodeStart;
   }

void
OMR::AOTMethodHeader::setCompiledCodeSize(uint32_t codeSize)
   {
   _compiledCodeSize = codeSize;
   }

uint32_t
OMR::AOTMethodHeader::getCompiledCodeSize()
   {
   return _compiledCodeSize;
   }

void
OMR::AOTMethodHeader::setRelocationsStart(TR::RelocationRecordBinaryTemplate *reloStart)
   {
    _relocationsBinaryTemplate = reloStart;
   }

TR::RelocationRecordBinaryTemplate*
OMR::AOTMethodHeader::getRelocationsStart()
   {
   return  _relocationsBinaryTemplate;
   }

void
OMR::AOTMethodHeader::setRelocationsSize(uint32_t size)
   {
   _relocationsSize = size;
   };

uint32_t
OMR::AOTMethodHeader::getRelocationsSize()
   {
   return _relocationsSize;
   }