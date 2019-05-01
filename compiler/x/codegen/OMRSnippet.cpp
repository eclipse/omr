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

#include "codegen/Snippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/RestartSnippet.hpp"
#include "x/codegen/X86Ops.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"

namespace TR { class X86BoundCheckWithSpineCheckSnippet; }
namespace TR { class X86CallSnippet; }
namespace TR { class X86CheckFailureSnippet; }
namespace TR { class X86CheckFailureSnippetWithResolve; }
namespace TR { class X86DivideCheckSnippet; }
namespace TR { class X86FPConvertToIntSnippet; }
namespace TR { class X86FPConvertToLongSnippet; }
namespace TR { class X86ForceRecompilationSnippet; }
namespace TR { class X86GuardedDevirtualSnippet; }
namespace TR { class X86PicDataSnippet; }
namespace TR { class X86RecompilationSnippet; }
namespace TR { class X86SpineCheckSnippet; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }


OMR::X86::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label,
      bool isGCSafePoint) :
   OMR::Snippet(cg, node, label, isGCSafePoint)
   {
   }


OMR::X86::Snippet::Snippet(
      TR::CodeGenerator *cg,
      TR::Node *node,
      TR::LabelSymbol *label) :
   OMR::Snippet(cg, node, label)
   {
   }


const char *
TR_Debug::getNamex(TR::Snippet *snippet)
   {
   switch (snippet->getKind())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsCall:
         return "Call Snippet";
         break;
      case TR::Snippet::IsVPicData:
         return "VPic Data";
         break;
      case TR::Snippet::IsIPicData:
         return "IPic Data";
         break;
      case TR::Snippet::IsForceRecompilation:
         return "Force Recompilation Snippet";
         break;
      case TR::Snippet::IsRecompilation:
         return "Recompilation Snippet";
         break;
#endif
      case TR::Snippet::IsCheckFailure:
         return "Check Failure Snippet";
         break;
      case TR::Snippet::IsCheckFailureWithResolve:
         return "Check Failure Snippet with Resolve Call";
         break;
      case TR::Snippet::IsBoundCheckWithSpineCheck:
         return "Bound Check with Spine Check Snippet";
         break;
      case TR::Snippet::IsSpineCheck:
         return "Spine Check Snippet";
         break;
      case TR::Snippet::IsConstantData:
         return "Constant Data Snippet";
         break;
      case TR::Snippet::IsData:
         return "Data Snippet";
      case TR::Snippet::IsDivideCheck:
         return "Divide Check Snippet";
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsGuardedDevirtual:
         return "Guarded Devirtual Snippet";
         break;
#endif
      case TR::Snippet::IsHelperCall:
         return "Helper Call Snippet";
         break;
      case TR::Snippet::IsFPConversion:
         return "FP Conversion Snippet";
         break;
      case TR::Snippet::IsFPConvertToInt:
         return "FP Convert To Int Snippet";
         break;
      case TR::Snippet::IsFPConvertToLong:
         return "FP Convert To Long Snippet";
         break;
      case TR::Snippet::IsUnresolvedDataIA32:
      case TR::Snippet::IsUnresolvedDataAMD64:
         return "Unresolved Data Snippet";
         break;
      case TR::Snippet::IsRestart:
      default:
         TR_ASSERT(0, "unexpected snippet kind: %d", snippet->getKind());
         return "Unknown snippet kind";
      }
   }

void
TR_Debug::printx(TR::FILE *pOutFile, TR::Snippet *snippet)
   {
   // TODO:AMD64: Clean up these #ifdefs
   if (pOutFile == NULL)
      return;
   switch (snippet->getKind())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsCall:
         print(pOutFile, (TR::X86CallSnippet *)snippet);
         break;
      case TR::Snippet::IsIPicData:
      case TR::Snippet::IsVPicData:
         print(pOutFile, (TR::X86PicDataSnippet *)snippet);
         break;
      case TR::Snippet::IsCheckFailure:
         print(pOutFile, (TR::X86CheckFailureSnippet *)snippet);
         break;
      case TR::Snippet::IsCheckFailureWithResolve:
         print(pOutFile, (TR::X86CheckFailureSnippetWithResolve *)snippet);
         break;
      case TR::Snippet::IsBoundCheckWithSpineCheck:
         print(pOutFile, (TR::X86BoundCheckWithSpineCheckSnippet *)snippet);
         break;
      case TR::Snippet::IsSpineCheck:
         print(pOutFile, (TR::X86SpineCheckSnippet *)snippet);
         break;
      case TR::Snippet::IsForceRecompilation:
         print(pOutFile, (TR::X86ForceRecompilationSnippet  *)snippet);
         break;
      case TR::Snippet::IsRecompilation:
         print(pOutFile, (TR::X86RecompilationSnippet *)snippet);
         break;
#endif
      case TR::Snippet::IsDivideCheck:
         print(pOutFile, (TR::X86DivideCheckSnippet  *)snippet);
         break;
#ifdef J9_PROJECT_SPECIFIC
      case TR::Snippet::IsGuardedDevirtual:
         print(pOutFile, (TR::X86GuardedDevirtualSnippet  *)snippet);
         break;
#endif
      case TR::Snippet::IsHelperCall:
         print(pOutFile, (TR::X86HelperCallSnippet  *)snippet);
         break;
      case TR::Snippet::IsFPConvertToInt:
         print(pOutFile, (TR::X86FPConvertToIntSnippet  *)snippet);
         break;
      case TR::Snippet::IsFPConvertToLong:
         print(pOutFile, (TR::X86FPConvertToLongSnippet  *)snippet);
         break;
      case TR::Snippet::IsUnresolvedDataIA32:
         print(pOutFile, (TR::UnresolvedDataSnippet *)snippet);
         break;
#ifdef TR_TARGET_64BIT
      case TR::Snippet::IsUnresolvedDataAMD64:
         print(pOutFile, (TR::UnresolvedDataSnippet *)snippet);
         break;
#endif
      case TR::Snippet::IsRestart:
         TR_ASSERT(0, "unexpected snippet kind: %d", snippet->getKind());
         break;
      default:
         snippet->print(pOutFile, this);
         break;
      }
   }

int32_t
TR_Debug::printRestartJump(TR::FILE *pOutFile, TR::X86RestartSnippet * snippet, uint8_t *bufferPos)
   {
   int32_t size = snippet->estimateRestartJumpLength(JMP4, bufferPos - (uint8_t*)snippet->cg()->getBinaryBufferStart());
   printPrefix(pOutFile, NULL, bufferPos, size);
   printLabelInstruction(pOutFile, "jmp", snippet->getRestartLabel());
   return size;
   }

int32_t
TR_Debug::printRestartJump(TR::FILE *pOutFile, TR::X86RestartSnippet * snippet, uint8_t *bufferPos, int32_t branchOp, const char *branchOpName)
   {
   int32_t size = snippet->estimateRestartJumpLength((TR_X86OpCodes) branchOp, bufferPos - (uint8_t*)snippet->cg()->getBinaryBufferStart());
   printPrefix(pOutFile, NULL, bufferPos, size);
   printLabelInstruction(pOutFile, branchOpName, snippet->getRestartLabel());
   return size;
   }
