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

#ifndef OMR_ARM_SNIPPET_INCL
#define OMR_ARM_SNIPPET_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SNIPPET_CONNECTOR
#define OMR_SNIPPET_CONNECTOR

namespace OMR {
namespace ARM {
class Snippet;
}

typedef OMR::ARM::Snippet SnippetConnector;
} // namespace OMR
#else
#error OMR::ARM::Snippet expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/codegen/OMRSnippet.hpp"

namespace TR {
class CodeGenerator;
class LabelSymbol;
class Node;
} // namespace TR

namespace OMR {
class Logger;
} // namespace OMR

namespace OMR { namespace ARM {

class OMR_EXTENSIBLE Snippet : public OMR::Snippet {
public:
    Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint);

    Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label);

    enum Kind {
        IsUnknown,
        IsCall,
        IsUnresolvedCall,
        IsVirtual,
        IsVirtualUnresolved,
        IsInterfaceCall,
        IsStackCheckFailure,
        IsUnresolvedData,
        IsRecompilation,
        IsHelperCall,
        IsMonitorEnter,
        IsMonitorExit,
        numKinds
    };

    virtual Kind getKind() { return IsUnknown; }

    /**
     * @copydoc OMR::Snippet::printName(OMR::Logger *)
     */
    void printName(OMR::Logger *log);
};

}} // namespace OMR::ARM

#endif
