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

#include "aarch64/codegen/ConstantDataSnippet.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/Node_inlines.hpp"
#include "ras/Debug.hpp"
#include "codegen/Relocation.hpp"

namespace TR { class Node; }

TR::ARM64ConstantDataSnippet::ARM64ConstantDataSnippet(TR::CodeGenerator *cg, TR::Node * n, void *c, size_t size, TR_ExternalRelocationTargetKind reloType)
   : TR::Snippet(cg, n, TR::LabelSymbol::create(cg->trHeapMemory(),cg), false),
     _data(size, 0, getTypedAllocator<uint8_t>(TR::comp()->allocator())),
     _reloType(reloType)
   {
   if (c)
      memcpy(_data.data(), c, size);
   else
      memset(_data.data(), 0, size);
   }


void
TR::ARM64ConstantDataSnippet::addMetaDataForCodeAddress(uint8_t *cursor)
   {
   TR::Compilation *comp = cg()->comp();
   if (cg()->profiledPointersRequireRelocation())
      {
      auto reloType = _reloType;
      auto node = getNode();
      TR::SymbolType symbolKind = TR::SymbolType::typeClass;
      switch (reloType)
         {
         case TR_RamMethod:
         case TR_MethodPointer:
            symbolKind = TR::SymbolType::typeMethod;
            // intentional fall through
         case TR_ClassPointer:
            AOTcgDiag2(comp, "add relocation (%d) cursor=%x\n", reloType, cursor);
            if (cg()->comp()->getOption(TR_UseSymbolValidationManager))
               {
               TR_ASSERT_FATAL(getData<uint8_t *>(), "Static Sym can not be NULL");
               cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                    getData<uint8_t *>(),
                                                                                    reinterpret_cast<uint8_t *>(symbolKind),
                                                                                    TR_SymbolFromManager,
                                                                                    cg()),
                                                                                    __FILE__, __LINE__,
                                                                                    node);
               }
            else
               {
               cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                    reinterpret_cast<uint8_t *>(node),
                                                                                    reloType,
                                                                                    cg()),
                                                                                    __FILE__, __LINE__,
                                                                                    node);
               }
            break;

         default:
            break;
         }
      }
   else
      {
      if (std::find(comp->getSnippetsToBePatchedOnClassRedefinition()->begin(), comp->getSnippetsToBePatchedOnClassRedefinition()->end(), this) != comp->getSnippetsToBePatchedOnClassRedefinition()->end())
         {
         cg()->jitAddPicToPatchOnClassRedefinition(getData<void *>(), static_cast<void *>(cursor));
         }

      if (std::find(comp->getSnippetsToBePatchedOnClassUnload()->begin(), comp->getSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getSnippetsToBePatchedOnClassUnload()->end())
         {
         cg()->jitAddPicToPatchOnClassUnload(getData<void *>(), static_cast<void *>(cursor));
         }

      if (std::find(comp->getMethodSnippetsToBePatchedOnClassUnload()->begin(), comp->getMethodSnippetsToBePatchedOnClassUnload()->end(), this) != comp->getMethodSnippetsToBePatchedOnClassUnload()->end())
         {
         auto classPointer = cg()->fe()->createResolvedMethod(cg()->trMemory(), getData<TR_OpaqueMethodBlock *>(), comp->getCurrentMethod())->classOfMethod();
         cg()->jitAddPicToPatchOnClassUnload(static_cast<void *>(classPointer), static_cast<void *>(cursor));
         }
      }
   }


uint8_t *TR::ARM64ConstantDataSnippet::emitSnippetBody()
   {
   uint8_t *cursor = cg()->getBinaryBufferCursor();

   // align to 8 bytes
   if (getDataSize() % 8 == 0)
      {
      cursor = (uint8_t*)(((intptr_t)(cursor + 7)) & (~7));
      }

   getSnippetLabel()->setCodeLocation(cursor);

   memcpy(cursor, getRawData(), getDataSize());

   addMetaDataForCodeAddress(cursor);


   cursor += getDataSize();

   return cursor;
   }

void TR::ARM64ConstantDataSnippet::print(TR::FILE* pOutFile, TR_Debug* debug)
   {
   if (pOutFile == NULL)
      return;

   uint8_t *bufferPos = getSnippetLabel()->getCodeLocation();

   debug->printSnippetLabel(pOutFile, getSnippetLabel(), bufferPos, debug->getName(this));
   debug->printPrefix(pOutFile, NULL, bufferPos, getDataSize());

   switch (getDataSize())
      {
      case 2:
         trfprintf(pOutFile, "0x%04x | %d",
                   0xffff & (int32_t)getData<int16_t>(),
                   (int32_t)getData<int16_t>());
         break;
      case 4:
         trfprintf(pOutFile, "0x%08x | %d | float %g",
                   getData<int32_t>(),
                   getData<int32_t>(),
                   getData<float>());
         break;
      case 8:
         trfprintf(pOutFile, "0x%016llx | %lld | double %g",
                   getData<int64_t>(),
                   getData<int64_t>(),
                   getData<double>());
         break;
      default:
         trfprintf(pOutFile, "VECTOR VALUE");
         break;
      }

   }
