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

#ifndef OMR_X86_SNIPPET_INCL
#define OMR_X86_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR

namespace OMR {
namespace X86 {
class Snippet;
}

typedef OMR::X86::Snippet SnippetConnector;
} // namespace OMR
#else
#error OMR::X86::Snippet expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRSnippet.hpp"
#include "env/CompilerEnv.hpp"

namespace TR {
class X86GuardedDevirtualSnippet;
class CodeGenerator;
class LabelSymbol;
class Node;
} // namespace TR

namespace OMR {
class Logger;
} // namespace OMR

namespace OMR { namespace X86 {

class OMR_EXTENSIBLE Snippet : public OMR::Snippet {
public:
    Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint);

    Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label);

    enum Kind {
        IsUnknown,
        IsCall,
        IsUnresolvedCall,
        IsIPicData,
        IsVPicData,
        IsUnresolvedVTableSlot,
        IsVirtualPIC,
        IsCheckFailure,
        IsCheckFailureWithResolve,
        IsBoundCheckWithSpineCheck,
        IsSpineCheck,
        IsConstantData,
        IsData,
        IsRecompilation,
        IsRestart,
        IsDivideCheck,
        IsForceRecompilation,
        IsGuardedDevirtual,
        IsHelperCall,
        IsFPConversion,
        IsFPConvertToInt,
        IsFPConvertToLong,
        IsUnresolvedDataIA32,
        IsUnresolvedDataAMD64,
        numKinds
    };

    virtual Kind getKind() { return IsUnknown; }

    /**
     * @copydoc OMR::Snippet::printName(OMR::Logger *)
     */
    void printName(OMR::Logger *log);
};

}} // namespace OMR::X86

inline const char *commentString() { return TR::Compiler->target.isLinux() ? "#" : ";"; }

inline const char *hexPrefixString() { return TR::Compiler->target.isLinux() ? "0x" : "0"; }

inline const char *hexSuffixString() { return TR::Compiler->target.isLinux() ? "" : "h"; }

inline const char *dbString() { return TR::Compiler->target.isLinux() ? ".byte" : "db"; }

inline const char *dwString() { return TR::Compiler->target.isLinux() ? ".short" : "dw"; }

inline const char *ddString() { return TR::Compiler->target.isLinux() ? ".int" : "dd"; }

inline const char *dqString() { return TR::Compiler->target.isLinux() ? ".quad" : "dq"; }

#endif
