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

#include "x/codegen/DivideCheckSnippet.hpp"

#include <stddef.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "ras/Debug.hpp"
#include "codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Ops_inlines.hpp"
#include "env/IO.hpp"
#include "env/CompilerEnv.hpp"

uint8_t *TR::X86DivideCheckSnippet::emitSnippetBody()
   {
   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);

   TR::RealRegister *realDivisorReg  = toRealRegister(_divideInstruction->getSourceRegister());
   TR::RealRegister *realDividendReg = toRealRegister(_divideInstruction->getTargetRegister());

   // CMP realDivisorReg, -1
   //
   uint8_t rexPrefix = cg()->comp()->target().is64Bit() ? realDivisorReg->rexBits(TR::RealRegister::REX_B, false) : 0;
   buffer = TR_X86OpCode(CMPRegImms(_divOp.isLong())).binary(buffer, rexPrefix);
   realDivisorReg->setRMRegisterFieldInModRM(buffer-1);
   *buffer++ = -1;

   // JNE divideLabel
   //
   buffer = genRestartJump(JNE4, buffer, _divideLabel);

   TR::Machine *machine = cg()->machine();
   if (_divOp.isDiv() && realDividendReg->getRegisterNumber() != TR::RealRegister::eax)
      {
      // MOV eax, realDividendReg
      //
      if (cg()->comp()->target().is64Bit())
         {
         uint8_t rexPrefix = realDividendReg->rexBits(TR::RealRegister::REX_R, false);
         if (_divOp.isLong())
            rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
         if (rexPrefix)
            *buffer++ = rexPrefix;
         }
      *buffer++ = 0x89;
      *buffer = 0xC0;
      realDividendReg->setRegisterFieldInModRM(buffer); // source
      buffer++;
      }

   if (_divOp.isRem())
      {
      // XOR edx, edx
      //
      if (cg()->comp()->target().is64Bit())
         {
         uint8_t rexPrefix = 0;
         if (_divOp.isLong())
            rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
         if (rexPrefix)
            *buffer++ = rexPrefix;
         }
      *buffer++ = 0x31;
      *buffer++ = 0xD2;
      }

   // jmp restartLabel
   //
   return genRestartJump(buffer);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::X86DivideCheckSnippet  * snippet) // TODO:FIX THIS!!!
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   TR::RealRegister *realDivisorReg  = toRealRegister(snippet->getDivideInstruction()->getSourceRegister());
   TR::RealRegister *realDividendReg = toRealRegister(snippet->getDivideInstruction()->getTargetRegister());

   bool isLong = snippet->getType().isInt64();

   int32_t cmpSize = 6;
#if defined(TR_TARGET_64BIT)
   if (comp()->target().is64Bit())
      {
      uint8_t rexPrefix = realDivisorReg->rexBits(TR::RealRegister::REX_B, false);
      if (isLong)
         rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
      if (rexPrefix)
         cmpSize++;
      }
#endif
   printPrefix(pOutFile, NULL, bufferPos, cmpSize);
   trfprintf(pOutFile, "cmp\t%s, -1", getName(realDivisorReg)); // TODO: Might be 64-bit register
   bufferPos += cmpSize;

   int32_t size = snippet->estimateRestartJumpLength(JNE4, bufferPos - (uint8_t*)0, snippet->getDivideLabel());
   printPrefix(pOutFile, NULL, bufferPos, size);
   printLabelInstruction(pOutFile, "jne", snippet->getDivideLabel());
   bufferPos += size;

   if (snippet->getOpCode().isDiv() && realDividendReg->getRegisterNumber() != TR::RealRegister::eax)
      {
      int32_t movSize = 2;
#if defined(TR_TARGET_64BIT)
      if (comp()->target().is64Bit())
         {
         uint8_t rexPrefix = realDividendReg->rexBits(TR::RealRegister::REX_R, false);
         if (isLong)
            rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
         if (rexPrefix)
            movSize++;
         }
#endif
      printPrefix(pOutFile, NULL, bufferPos, movSize);
      trfprintf(pOutFile, "mov\teax, %s", getName(realDividendReg)); // TODO: Might be rax
      bufferPos += movSize;
      }

   if (snippet->getOpCode().isRem())
      {
      int32_t xorSize = 2;
      if (isLong)
         xorSize++;
      printPrefix(pOutFile, NULL, bufferPos, xorSize);
      trfprintf(pOutFile, "xor\tedx, edx"); // TODO: Might be rdx
      bufferPos += xorSize;
      }

   printRestartJump(pOutFile, snippet, bufferPos);
   }



uint32_t TR::X86DivideCheckSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR::RealRegister *realDivisorReg  = toRealRegister(_divideInstruction->getSourceRegister());
   TR::RealRegister *realDividendReg = toRealRegister(_divideInstruction->getTargetRegister());

   uint32_t fixedLength = 6;

   TR::Machine *machine = cg()->machine();
   if (cg()->comp()->target().is64Bit())
      {
      uint8_t rexPrefix = realDivisorReg->rexBits(TR::RealRegister::REX_B, false);
      if (_divOp.isLong())
         rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
      if (rexPrefix)
         fixedLength++;
      }
   uint32_t jumpLength = estimateRestartJumpLength(JNE4,
                                                   estimatedSnippetStart + fixedLength + 2,
                                                   _divideLabel);

   if (_divOp.isDiv() && realDividendReg->getRegisterNumber() != TR::RealRegister::eax)
      {
      fixedLength += 2;
      if (cg()->comp()->target().is64Bit())
         {
         uint8_t rexPrefix = realDividendReg->rexBits(TR::RealRegister::REX_R, false);
         if (_divOp.isLong())
            rexPrefix |= TR::RealRegister::REX | TR::RealRegister::REX_W;
         if (rexPrefix)
            fixedLength++;
         }
      }

   if (_divOp.isRem())
      {
      fixedLength += 2;
      }

   jumpLength += estimateRestartJumpLength(estimatedSnippetStart + jumpLength + fixedLength + 2);

   return fixedLength + jumpLength;
   }
