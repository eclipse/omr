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
#include "codegen/snippet/PPA1Snippet.hpp"

TR::PPA1Snippet::PPA1Snippet(TR::CodeGenerator* cg, TR::S390zOSSystemLinkage* linkage)
:
   TR::Snippet(cg, NULL, generateLabelSymbol(cg)),
   _linkage(linkage)
   {
   }

uint8_t*
TR::PPA1Snippet::emitSnippetBody()
   {
   uint8_t* cursor = cg()->getBinaryBufferCursor();

   // TODO: We should not have to do this here. This should be done by the caller.
   getSnippetLabel()->setCodeLocation(cursor);

   // Version
   *reinterpret_cast<uint8_t*>(cursor) = 0x02;
   cursor += sizeof(uint8_t);

   // Signature
   *reinterpret_cast<uint8_t*>(cursor) = 0xCE;
   cursor += sizeof(uint8_t);

   // Saved GPR mask
   *reinterpret_cast<uint16_t*>(cursor) = _linkage->getGPRSaveMask();
   cursor += sizeof(uint16_t);

   cg()->addRelocation(new (cg()->trHeapMemory()) PPA1OffsetToPPA2Relocation(cursor, _linkage->getPPA2Snippet()->getSnippetLabel()));

   // Signed offset to PPA2 from start of PPA1
   *reinterpret_cast<int32_t*>(cursor) = 0x00000000;
   cursor += sizeof(int32_t);

   uint8_t flags1 = 0x00;

   if (cg()->comp()->target().is64Bit())
      {
      flags1 |= 0x80;
      }

   // Flags 1
   *reinterpret_cast<uint8_t*>(cursor) = flags1;
   cursor += sizeof(uint8_t);

   uint8_t flags2 = 0x80;

   // Flags 2
   *reinterpret_cast<uint8_t*>(cursor) = flags2;
   cursor += sizeof(uint8_t);

   uint8_t flags3 = 0x00;

   // Flags 3
   *reinterpret_cast<uint8_t*>(cursor) = flags3;
   cursor += sizeof(uint8_t);

   uint8_t flags4 = 0x00;

   // Flags 4
   *reinterpret_cast<uint8_t*>(cursor) = flags4;
   cursor += sizeof(uint8_t);
      
   // Length/4 of parameters
   *reinterpret_cast<uint16_t*>(cursor) = _linkage->getIncomingParameterBlockSize() / 4;
   cursor += sizeof(uint16_t);

   // TODO: We need to pre-calculate this and store it in the linkage. There are several places which use such a
   // value. Those need to also be improved.
   TR::Instruction* prologueBegin = cg()->getFirstInstruction();

   while (prologueBegin != NULL && prologueBegin->getOpCodeValue() != TR::InstOpCode::PROC)
      {
      prologueBegin = prologueBegin->getNext();
      }

   TR::Instruction* prologueEnd = prologueBegin;

   while (prologueEnd != NULL && prologueEnd->getOpCodeValue() != TR::InstOpCode::FENCE)
      {
      prologueEnd = prologueEnd->getNext();
      }
      
   size_t prologueSize = prologueEnd->getBinaryEncoding() - prologueBegin->getBinaryEncoding();

   TR_ASSERT_FATAL(prologueSize <= 510, "XPLink prologue size (%d) is > 510 bytes", prologueSize);
      
   // Length/2 of prologue
   *reinterpret_cast<uint8_t*>(cursor) = prologueSize / 2;
   cursor += sizeof(uint8_t);

   size_t offsetToStackPointerUpdate = 0;

   if (_linkage->getStackPointerUpdateLabel() != NULL)
      {
      offsetToStackPointerUpdate = _linkage->getStackPointerUpdateLabel()->getInstruction()->getBinaryEncoding() - prologueBegin->getBinaryEncoding();
      }

   // We must encode this value in 4 bits as the most significant 4 bits are used for alloca register
   TR_ASSERT_FATAL(offsetToStackPointerUpdate <= 30, "Offset to stack pointer update from the entry point (%d) is > 30 bytes", offsetToStackPointerUpdate);
      
   // Alloca register and offset/2 to stack pointer update
   *reinterpret_cast<uint8_t*>(cursor) = offsetToStackPointerUpdate / 2;
   cursor += sizeof(uint8_t);
      
   // Length of code section
   *reinterpret_cast<uint32_t*>(cursor) = cg()->getAppendInstruction()->getBinaryEncoding() - cg()->getFirstInstruction()->getBinaryEncoding();
   cursor += sizeof(uint32_t);

   return cursor;
   }

uint32_t
TR::PPA1Snippet::getLength(int32_t estimatedSnippetStart)
   {
   return FIXED_AREA_SIZE;
   }

TR::PPA1Snippet::PPA1OffsetToPPA2Relocation::PPA1OffsetToPPA2Relocation(uint8_t* cursor, TR::LabelSymbol* ppa2)
   :
      TR::LabelRelocation(cursor, ppa2)
   {
   }

void
TR::PPA1Snippet::PPA1OffsetToPPA2Relocation::apply(TR::CodeGenerator* cg)
   {
   uint8_t* cursor = getUpdateLocation();

   // The 0x04 is the static distance from the field which we need to relocate to the start of the PPA1
   *reinterpret_cast<int32_t*>(cursor) = static_cast<int32_t>(getLabel()->getCodeLocation() - (cursor - 0x04));
   }
