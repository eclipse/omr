/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "p/codegen/PPCHelperCallSnippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"

uint8_t *TR::PPCHelperCallSnippet::emitSnippetBody()
   {
   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   uint8_t             *gtrmpln, *trmpln;

   getSnippetLabel()->setCodeLocation(buffer);

   return genHelperCall(buffer);
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCHelperCallSnippet * snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();
   TR::LabelSymbol *restartLabel = snippet->getRestartLabel();

   if (snippet->getKind() == TR::Snippet::IsArrayCopyCall)
      {
      cursor = print(pOutFile, (TR::PPCArrayCopyCallSnippet *)snippet, cursor);
      }
   else
      {
      printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Helper Call Snippet");
      }

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(snippet->getDestination(), cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "%s \t" POINTER_PRINTF_FORMAT "\t\t; %s %s",
      restartLabel ? "bl" : "b", (intptr_t)cursor + distance, getName(snippet->getDestination()), info);

   if (restartLabel)
      {
      cursor += 4;
      printPrefix(pOutFile, NULL, cursor, 4);
      distance = *((int32_t *) cursor) & 0x03fffffc;
      distance = (distance << 6) >> 6;   // sign extend
      trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Restart", (intptr_t)cursor + distance);
      }
   }

uint32_t TR::PPCHelperCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return getHelperCallLength();
   }

uint8_t *TR::PPCHelperCallSnippet::genHelperCall(uint8_t *buffer)
   {
   intptr_t helperAddress = (intptr_t)getDestination()->getSymbol()->castToMethodSymbol()->getMethodAddress();

   if (cg()->directCallRequiresTrampoline(helperAddress, (intptr_t)buffer))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(getDestination()->getReferenceNumber(), (void *)buffer);

      TR_ASSERT_FATAL(cg()->comp()->target().cpu.isTargetWithinIFormBranchRange(helperAddress, (intptr_t)buffer),
                      "Helper address is out of range");
      }

   intptr_t distance = helperAddress - (intptr_t)buffer;

   // b|bl distance
   *(int32_t *)buffer = 0x48000000 | (distance & 0x03fffffc);
   if (_restartLabel != NULL)
      {
      *(int32_t *)buffer |= 0x00000001;
      }

   cg()->addProjectSpecializedRelocation(buffer,(uint8_t *)getDestination(), NULL, TR_HelperAddress,
                          __FILE__, __LINE__, getNode());
   buffer += 4;

   gcMap().registerStackMap(buffer, cg());

   if (_restartLabel != NULL)
      {
      int32_t returnDistance = _restartLabel->getCodeLocation() - buffer;
      *(int32_t *)buffer = 0x48000000 | (returnDistance & 0x03fffffc);
      buffer += 4;
      }

   return buffer;
   }

uint32_t TR::PPCHelperCallSnippet::getHelperCallLength()
   {
   return ((_restartLabel==NULL)?4:8);
   }

uint8_t *TR::PPCArrayCopyCallSnippet::emitSnippetBody()
   {
   TR::Node *node = getNode();
   TR_ASSERT(node->getOpCodeValue() == TR::arraycopy &&
          node->getChild(2)->getOpCode().isLoadConst(), "only valid for arraycopies with a constant length\n");

   uint8_t *buffer = cg()->getBinaryBufferCursor();
   getSnippetLabel()->setCodeLocation(buffer);
   TR::RealRegister *lengthReg = cg()->machine()->getRealRegister(_lengthRegNum);
   TR::Node *lengthNode = node->getChild(2);
   int64_t byteLen = (lengthNode->getType().isInt32() ?
                      lengthNode->getInt() : lengthNode->getLongInt());
   TR::InstOpCode opcode;

   // li lengthReg, #byteLen
   opcode.setOpCodeValue(TR::InstOpCode::li);
   buffer = opcode.copyBinaryToBuffer(buffer);
   lengthReg->setRegisterFieldRT((uint32_t *)buffer);
   TR_ASSERT(byteLen <= UPPER_IMMED,"byteLen too big to encode\n");
   *(int32_t *)buffer |= byteLen;
   buffer += 4;

   return TR::PPCHelperCallSnippet::genHelperCall(buffer);
   }

uint8_t*
TR_Debug::print(TR::FILE *pOutFile, TR::PPCArrayCopyCallSnippet *snippet, uint8_t *cursor)
   {
   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "ArrayCopy Helper Call Snippet");

   TR::RealRegister *lengthReg = _cg->machine()->getRealRegister(snippet->getLengthRegNum());

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, "li \t%s, %d", getName(lengthReg), *((int32_t *) cursor) & 0x0000ffff);
   cursor += 4;

   return cursor;
   }

uint32_t TR::PPCArrayCopyCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return TR::PPCHelperCallSnippet::getHelperCallLength() + 4;
   }
