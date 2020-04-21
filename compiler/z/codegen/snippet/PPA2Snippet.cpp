/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/SystemLinkagezOS.hpp"
#include "codegen/snippet/PPA2Snippet.hpp"

TR::PPA2Snippet::PPA2Snippet(TR::CodeGenerator* cg, TR::S390zOSSystemLinkage* linkage)
:
   TR::Snippet(cg, NULL, generateLabelSymbol(cg)),
   _linkage(linkage)
   {
   }

uint8_t*
TR::PPA2Snippet::emitSnippetBody()
   {
   uint8_t* cursor = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(cursor);

   // Member identifer
   *reinterpret_cast<uint8_t*>(cursor) = 0x03;
   cursor += sizeof(uint8_t);

   // Member subid
   *reinterpret_cast<uint8_t*>(cursor) = 0x00;
   cursor += sizeof(uint8_t);

   // Member defined
   *reinterpret_cast<uint8_t*>(cursor) = 0x22;
   cursor += sizeof(uint8_t);

   // Control level
   *reinterpret_cast<uint8_t*>(cursor) = 0x04;
   cursor += sizeof(uint8_t);

   // Signed offset from PPA2 to CELQSTRT for load module
   *reinterpret_cast<int32_t*>(cursor) = 0x00000000;
   cursor += sizeof(int32_t);

   // Signed offset from PPA2 to PPA4 (zero if not present)
   *reinterpret_cast<int32_t*>(cursor) = 0x00000000;
   cursor += sizeof(int32_t);

   // Signed offset from PPA2 to timestamp/version information (zero if not present)
   *reinterpret_cast<int32_t*>(cursor) = 0x00000000;
   cursor += sizeof(int32_t);

   // Signed offset from PPA2 to compilation unit's primary Entry Point (at the lowest address)
   *reinterpret_cast<int32_t*>(cursor) = 0x00000000;
   cursor += sizeof(int32_t);

   // Compilation flags
   *reinterpret_cast<uint16_t*>(cursor) = 0x8500;
   cursor += sizeof(uint16_t);

   // Reserved
   *reinterpret_cast<uint16_t*>(cursor) = 0x0000;
   cursor += sizeof(uint16_t);

   return cursor;
   }

uint32_t
TR::PPA2Snippet::getLength(int32_t estimatedSnippetStart)
   {
   return FIXED_AREA_SIZE;
   }
