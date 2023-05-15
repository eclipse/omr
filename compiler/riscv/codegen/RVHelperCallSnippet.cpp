/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "codegen/RVHelperCallSnippet.hpp"
#include "codegen/RVInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *
TR::RVHelperCallSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();
   intptr_t distance = (intptr_t)(getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress()) - (intptr_t)cursor;

   getSnippetLabel()->setCodeLocation(cursor);

   if (!VALID_UJTYPE_IMM(distance))
      {
      distance = TR::CodeCacheManager::instance()->findHelperTrampoline(getDestination()->getReferenceNumber(), (void *)cursor) - (intptr_t)cursor;
      TR_ASSERT_FATAL(VALID_UJTYPE_IMM(distance), "Trampoline too far away.");
      }

   *(int32_t *)cursor = TR_RISCV_UJTYPE(TR::InstOpCode::_jal, cg()->machine()->getRealRegister(OMR::RealRegister::ra) , distance);

   cg()->addExternalRelocation(
      TR::ExternalRelocation::create(
         cursor,
         (uint8_t *)getDestination(),
         TR_HelperAddress,
         cg()),
      __FILE__,
      __LINE__,
      getNode());
   cursor += RISCV_INSTRUCTION_LENGTH;

   gcMap().registerStackMap(cursor, cg());

   if (_restartLabel != NULL)
      {
      distance = (intptr_t)(_restartLabel->getCodeLocation()) - (intptr_t)cursor;
      if (VALID_UJTYPE_IMM(distance))
         {
         // b distance
         *(int32_t *)cursor = TR_RISCV_UJTYPE(TR::InstOpCode::_jal, cg()->machine()->getRealRegister(OMR::RealRegister::zero), distance >> 2);
         cursor += RISCV_INSTRUCTION_LENGTH;
         }
      else
         {
         TR_ASSERT_FATAL(false, "Target too far away. Not supported yet");
         }
      }

   return cursor;
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::RVHelperCallSnippet * snippet)
   {
   uint8_t *bufferPos = snippet->getSnippetLabel()->getCodeLocation();
   auto restartLabel = snippet->getRestartLabel();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), bufferPos, getName(snippet));

   char *info = "";
   intptr_t target = (intptr_t)(snippet->getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress()) ;
   int32_t distance;
   if (isBranchToTrampoline(snippet->getDestination(), bufferPos, distance))
      {
      target = (intptr_t)distance + (intptr_t)bufferPos;
      info = " Through trampoline";
      TR_ASSERT_FATAL(VALID_UJTYPE_IMM(distance), "Trampoline too far away.");
      }

   printPrefix(pOutFile, NULL, bufferPos, 4);
   trfprintf(pOutFile, "jal \tra, " POINTER_PRINTF_FORMAT "\t\t; %s%s",
      target, getName(snippet->getDestination()), info);

   if (restartLabel != NULL)
      {
      bufferPos += RISCV_INSTRUCTION_LENGTH;
      intptr_t restartLocation = (intptr_t)restartLabel->getCodeLocation();
      if (VALID_UJTYPE_IMM((intptr_t)restartLocation - (intptr_t)bufferPos))
         {
         printPrefix(pOutFile, NULL, bufferPos, 4);
         trfprintf(pOutFile, "jal \tzero, " POINTER_PRINTF_FORMAT "\t\t; Back to ", restartLocation);
         print(pOutFile, restartLabel);
         }
      else
         {
         TR_ASSERT_FATAL(false, "Target too far away. Not supported yet");
         }
      }
   }

uint32_t
TR::RVHelperCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return ((_restartLabel == NULL) ? 1 : 2) * RISCV_INSTRUCTION_LENGTH;
   }
