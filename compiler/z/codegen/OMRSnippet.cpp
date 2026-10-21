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

#pragma csect(CODE, "OMRZSnippet#C")
#pragma csect(STATIC, "OMRZSnippet#S")
#pragma csect(TEST, "OMRZSnippet#T")

#include <stddef.h>
#include <stdint.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "ras/Debug.hpp"
#include "ras/Logger.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"

namespace TR {
class Node;
}

OMR::Z::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint)
    : OMR::Snippet(cg, node, label, isGCSafePoint)
    , _codeBaseOffset(-1)
    , _pad_bytes(0)
    , _snippetDestAddr(0)
    , _zflags(0)
{
    self()->setNeedLitPoolBasePtr();
}

OMR::Z::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label)
    : OMR::Snippet(cg, node, label)
    , _codeBaseOffset(-1)
    , _pad_bytes(0)
    , _snippetDestAddr(0)
    , _zflags(0)
{
    self()->setNeedLitPoolBasePtr();
}

/**
 * Load the vm thread value into GPR13
 * @todo okay to assume that we can destroy previous contents of r13? is register volatile?
 */
uint8_t *OMR::Z::Snippet::generateLoadVMThreadInstruction(TR::CodeGenerator *cg, uint8_t *cursor) { return cursor; }

uint32_t OMR::Z::Snippet::getLoadVMThreadInstructionLength(TR::CodeGenerator *cg) { return 0; }

/**
 * Helper method to insert Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param cursor           Current binary encoding cursor
 * @param op               Runtime Instrumentation opcode: TR::InstOpCode::RION or TR::InstOpCode::RIOFF
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 Updated binary encoding cursor after RI hook generation.
 */
uint8_t *OMR::Z::Snippet::generateRuntimeInstrumentationOnOffInstruction(TR::CodeGenerator *cg, uint8_t *cursor,
    TR::InstOpCode::Mnemonic op, bool isPrivateLinkage)
{
    return cursor;
}

/**
 * Helper method to query the length of Runtime Instrumentation Hooks RION or RIOFF in snippet
 *
 * @param cg               Code Generator Pointer
 * @param isPrivateLinkage Whether the snippet is involved in a private JIT linkage (i.e. Call Helper to JITCODE)
 * @return                 The length of RION or RIOFF encoding if generated.  Zero otherwise.
 */
uint32_t OMR::Z::Snippet::getRuntimeInstrumentationOnOffInstructionLength(TR::CodeGenerator *cg, bool isPrivateLinkage)
{
    return 0;
}

#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/ConstantDataSnippet.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"

void TR_Debug::printz(OMR::Logger *log, TR::Snippet *snippet)
{
    switch (snippet->getKind()) {
        case TR::Snippet::IsHelperCall:
            snippet->print(log, this);
            break;
#if J9_PROJECT_SPECIFIC
        case TR::Snippet::IsCall:
            snippet->print(log, this);
            break;
        case TR::Snippet::IsForceRecomp:
            print(log, (TR::S390ForceRecompilationSnippet *)snippet);
            break;
        case TR::Snippet::IsForceRecompData:
            print(log, (TR::S390ForceRecompilationDataSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedCall:
            print(log, (TR::S390UnresolvedCallSnippet *)snippet);
            break;
        case TR::Snippet::IsVirtual:
            print(log, (TR::S390VirtualSnippet *)snippet);
            break;
        case TR::Snippet::IsVirtualUnresolved:
            print(log, (TR::S390VirtualUnresolvedSnippet *)snippet);
            break;
        case TR::Snippet::IsInterfaceCall:
            print(log, (TR::S390InterfaceCallSnippet *)snippet);
            break;
        case TR::Snippet::IsStackCheckFailure:
            print(log, (TR::S390StackCheckFailureSnippet *)snippet);
            break;
        case TR::Snippet::IsInterfaceCallData:
            print(log, (TR::J9S390InterfaceCallDataSnippet *)snippet);
            break;
#endif
        case TR::Snippet::IsConstantData:
        case TR::Snippet::IsWritableData:
        case TR::Snippet::IsEyeCatcherData:
            print(log, (TR::S390ConstantDataSnippet *)snippet);
            break;
        case TR::Snippet::IsUnresolvedData:
            print(log, (TR::UnresolvedDataSnippet *)snippet);
            break;

        case TR::Snippet::IsConstantInstruction:
            print(log, (TR::S390ConstantInstructionSnippet *)snippet);
            break;
        case TR::Snippet::IsRestoreGPR7:
            print(log, (TR::S390RestoreGPR7Snippet *)snippet);
            break;

        default:
            snippet->print(log, this);
    }
}
