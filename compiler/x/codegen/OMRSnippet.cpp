/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/Snippet.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "ras/Logger.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/RestartSnippet.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"

namespace TR {
class X86BoundCheckWithSpineCheckSnippet;
class X86CallSnippet;
class X86CheckFailureSnippet;
class X86CheckFailureSnippetWithResolve;
class X86DivideCheckSnippet;
class X86FPConvertToIntSnippet;
class X86FPConvertToLongSnippet;
class X86ForceRecompilationSnippet;
class X86GuardedDevirtualSnippet;
class X86PicDataSnippet;
class X86RecompilationSnippet;
class X86SpineCheckSnippet;
class LabelSymbol;
class Node;
} // namespace TR

OMR::X86::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint)
    : OMR::Snippet(cg, node, label, isGCSafePoint)
{}

OMR::X86::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label)
    : OMR::Snippet(cg, node, label)
{}

void TR_Debug::printx(OMR::Logger *log, TR::Snippet *snippet)
{
    switch (snippet->getKind()) {
#ifdef J9_PROJECT_SPECIFIC
        case TR::Snippet::IsCall:
            print(log, (TR::X86CallSnippet *)snippet);
            break;
        case TR::Snippet::IsIPicData:
        case TR::Snippet::IsVPicData:
            print(log, (TR::X86PicDataSnippet *)snippet);
            break;
        case TR::Snippet::IsCheckFailure:
            print(log, (TR::X86CheckFailureSnippet *)snippet);
            break;
        case TR::Snippet::IsCheckFailureWithResolve:
            print(log, (TR::X86CheckFailureSnippetWithResolve *)snippet);
            break;
        case TR::Snippet::IsBoundCheckWithSpineCheck:
            print(log, (TR::X86BoundCheckWithSpineCheckSnippet *)snippet);
            break;
        case TR::Snippet::IsSpineCheck:
            print(log, (TR::X86SpineCheckSnippet *)snippet);
            break;
        case TR::Snippet::IsForceRecompilation:
            print(log, (TR::X86ForceRecompilationSnippet *)snippet);
            break;
        case TR::Snippet::IsRecompilation:
            print(log, (TR::X86RecompilationSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsDivideCheck:
            print(log, (TR::X86DivideCheckSnippet *)snippet);
            break;
#ifdef J9_PROJECT_SPECIFIC
        case TR::Snippet::IsGuardedDevirtual:
            print(log, (TR::X86GuardedDevirtualSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsHelperCall:
            print(log, (TR::X86HelperCallSnippet *)snippet);
            break;
        case TR::Snippet::IsFPConvertToInt:
            print(log, (TR::X86FPConvertToIntSnippet *)snippet);
            break;
        case TR::Snippet::IsFPConvertToLong:
            print(log, (TR::X86FPConvertToLongSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedDataIA32:
            print(log, (TR::UnresolvedDataSnippet *)snippet);
            break;
#ifdef TR_TARGET_64BIT
        case TR::Snippet::IsUnresolvedDataAMD64:
            print(log, (TR::UnresolvedDataSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsRestart:
            TR_ASSERT(0, "unexpected snippet kind: %d", snippet->getKind());
            break;
        default:
            snippet->print(log, this);
            break;
    }
}

int32_t TR_Debug::printRestartJump(OMR::Logger *log, TR::X86RestartSnippet *snippet, uint8_t *bufferPos)
{
    int32_t size = snippet->estimateRestartJumpLength(OP::JMP4,
        static_cast<int32_t>(bufferPos - (uint8_t *)snippet->cg()->getBinaryBufferStart()));
    printPrefix(log, NULL, bufferPos, size);
    printLabelInstruction(log, "jmp", snippet->getRestartLabel());
    return size;
}

int32_t TR_Debug::printRestartJump(OMR::Logger *log, TR::X86RestartSnippet *snippet, uint8_t *bufferPos,
    int32_t branchOp, const char *branchOpName)
{
    int32_t size = snippet->estimateRestartJumpLength((OP::Mnemonic)branchOp,
        static_cast<int32_t>(bufferPos - (uint8_t *)snippet->cg()->getBinaryBufferStart()));
    printPrefix(log, NULL, bufferPos, size);
    printLabelInstruction(log, branchOpName, snippet->getRestartLabel());
    return size;
}

void OMR::X86::Snippet::printName(OMR::Logger *log)
{
    const char *name;

    switch (getKind()) {
        case TR::Snippet::IsCheckFailure:
            name = "Check Failure Snippet";
            break;
        case TR::Snippet::IsCheckFailureWithResolve:
            name = "Check Failure Snippet with Resolve Call";
            break;
        case TR::Snippet::IsBoundCheckWithSpineCheck:
            name = "Bound Check with Spine Check Snippet";
            break;
        case TR::Snippet::IsSpineCheck:
            name = "Spine Check Snippet";
            break;
        case TR::Snippet::IsConstantData:
            name = "Constant Data Snippet";
            break;
        case TR::Snippet::IsData:
            name = "Data Snippet";
            break;
        case TR::Snippet::IsDivideCheck:
            name = "Divide Check Snippet";
            break;
        case TR::Snippet::IsHelperCall:
            name = "Helper Call Snippet";
            break;
        case TR::Snippet::IsFPConversion:
            name = "FP Conversion Snippet";
            break;
        case TR::Snippet::IsFPConvertToInt:
            name = "FP Convert To Int Snippet";
            break;
        case TR::Snippet::IsFPConvertToLong:
            name = "FP Convert To Long Snippet";
            break;
        case TR::Snippet::IsUnresolvedDataIA32:
        case TR::Snippet::IsUnresolvedDataAMD64:
            name = "Unresolved Data Snippet";
            break;
        case TR::Snippet::IsRestart:
            name = "Restart Snippet";
            break;
        default:
            name = "Unknown snippet kind";
            break;
    }

    log->prints(name);
}
