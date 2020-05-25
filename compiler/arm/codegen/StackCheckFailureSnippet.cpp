/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/StackCheckFailureSnippet.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "codegen/GCStackAtlas.hpp" /* @@ */

uint8_t *storeArgumentItem(TR_ARMOpCodes op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg)
   {
   TR::RealRegister *stackPtr = cg->machine()->getRealRegister(cg->getLinkage()->getProperties().getStackPointerRegister());
   TR_ARMOpCode       opCode(op);
   opCode.copyBinaryToBuffer(buffer);
   reg->setRegisterFieldRD(toARMCursor(buffer));
   stackPtr->setRegisterFieldRN(toARMCursor(buffer));
   *toARMCursor(buffer) |= offset & 0x00000fff;  // assume offset is +ve
   *toARMCursor(buffer) |= 1 << 24;  // set P bit to be offset addressing
   *toARMCursor(buffer) |= 1 << 23;  // set U bit for +ve
   *toARMCursor(buffer) |= ARMConditionCodeAL << 28;  // set condition
   return(buffer+4);
   }

uint8_t *loadArgumentItem(TR_ARMOpCodes op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "fix loadArgumentItem");
   TR::RealRegister *stackPtr = cg->machine()->getRealRegister(cg->getLinkage()->getProperties().getStackPointerRegister());
   TR_ARMOpCode       opCode(op);
   opCode.copyBinaryToBuffer(buffer);
   reg->setRegisterFieldRT(toARMCursor(buffer));
   stackPtr->setRegisterFieldRD(toARMCursor(buffer));
   *toARMCursor(buffer) |= offset & 0x0000ffff;
   return(buffer+4);
   }

uint8_t *TR::ARMStackCheckFailureSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();

#ifdef J9_PROJECT_SPECIFIC
   TR::ResolvedMethodSymbol *bodySymbol = cg()->comp()->getJittedMethodSymbol();
   TR::Machine         *machine    = cg()->machine();
   TR::SymbolReference    *sofRef     = cg()->comp()->getSymRefTab()->findOrCreateStackOverflowSymbolRef(bodySymbol);

   uint8_t *bufferStart = buffer;
   uint8_t *returnLocation;

   bool saveLR = (cg()->getSnippetList().size() <= 1 &&
                  !bodySymbol->isEHAware()              &&
                  !machine->getLinkRegisterKilled()) ? true : false;

   getSnippetLabel()->setCodeLocation(buffer);

   // Pass cg()->getFrameSizeInBytes() to jitStackOverflow in a scratch
   // register (R11).
   //
   // If (frameSize - offset) cannot be represented in a rotated 8-bit number,
   // then the prologue must have already loaded it into R11; otherwise,
   // the snippet needs to load the number into R11.
   //
   const TR::ARMLinkageProperties &linkage = cg()->getLinkage()->getProperties();
   uint32_t frameSize = cg()->getFrameSizeInBytes();
   int32_t offset = linkage.getOffsetToFirstLocal();
   uint32_t base, rotate;

   if (constantIsImmed8r(frameSize, &base, &rotate))
      {
      // mov R11, frameSize
      *(int32_t *)buffer = 0xe3a0b << 12 | (rotate ? 16 - (rotate >> 1) : 0) << 8 | base & 0xFF;
      buffer+=4;
      }
   else if (!constantIsImmed8r(frameSize-offset, &base, &rotate) && constantIsImmed8r(-offset, &base, &rotate))
      {
      // sub R11, R11, -offset
      *(int32_t *)buffer = 0xe24bb << 12 | ((rotate ? 16 - (rotate >> 1) : 0) << 8) | base & 0xFF;
      buffer+=4;
      }
   else
      {
      // R11 <- cg()->getFrameSizeInBytes() by mov, add, add
      // There is no need in another add, since a stack frame will never be that big.
      *(int32_t *)buffer = 0xe3a0b << 12 | frameSize & 0xFF;
      buffer+=4;
      *(int32_t *)buffer = 0xe28bb << 12 | 12 << 8 | (frameSize >> 8 & 0xFF);
      buffer+=4;
      *(int32_t *)buffer = 0xe28bb << 12 | 8 << 8 | (frameSize >> 16 & 0xFF);
      buffer+=4;
      }

   // add R7, R7, R11
   *(int32_t *)buffer = 0xe087700b;
   buffer+=4;

   if (saveLR)
      {
      // str [R7, 0], R14
      *(int32_t *)buffer = 0xe587e000;
      buffer += 4;
      }

   *(int32_t *)buffer = encodeHelperBranchAndLink(sofRef, buffer, getNode(), cg());
   buffer += 4;
   returnLocation = buffer;

   if (saveLR)
      {
      // ldr R14, [R7, 0]
      *(int32_t *)buffer = 0xe597e000;
      buffer += 4;
      }

   // sub R7, R7, R11 ; assume that jitStackOverflow does not clobber R11
   *(int32_t *)buffer = 0xe047700b;
   buffer+=4;

   // b restartLabel ; assuming it is less than 26 bits away.
   *(int32_t *)buffer = 0xEA000000 | encodeBranchDistance((uintptr_t) buffer, (uintptr_t) getReStartLabel()->getCodeLocation());

   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // only the arg references are live at this point
      TR_GCStackMap *map = atlas->getParameterMap();

      // set the GC map
      gcMap().setStackMap(map);
      }
   gcMap().registerStackMap(returnLocation, cg());
#endif

   return buffer + 4;
   }

uint32_t TR::ARMStackCheckFailureSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR::ResolvedMethodSymbol *bodySymbol=cg()->comp()->getJittedMethodSymbol();
   int32_t length = 20; // mov/sub, add, bl, sub, b
   uint32_t base, rotate;
   uint32_t frameSize = cg()->getFrameSizeInBytes();
//   const TR::ARMLinkageProperties &linkage = cg()->getLinkage()->getProperties();
//   uint32_t offset = linkage.getOffsetToFirstLocal();
   if (!constantIsImmed8r(frameSize, &base, &rotate)) /* &&
       (constantIsImmed8r(frameSize-offset, &base, &rotate) ||
        !constantIsImmed8r(-offset, &base, &rotate)))*/
      {
      length += 8; // add, add
      }

   if (cg()->getSnippetList().size() <=1 &&
       !bodySymbol->isEHAware()             &&
       !cg()->machine()->getLinkRegisterKilled())
      {
      length += 8; // str, ldr
      }

   return length;
   }
