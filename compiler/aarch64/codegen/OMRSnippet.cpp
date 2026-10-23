/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
#include "codegen/CodeGenerator.hpp"
#include "ras/Logger.hpp"

OMR::ARM64::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint)
    : OMR::Snippet(cg, node, label, isGCSafePoint)
{}

OMR::ARM64::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label)
    : OMR::Snippet(cg, node, label)
{}

void OMR::ARM64::Snippet::printName(OMR::Logger *log)
{
    const char *name;

    switch (getKind()) {
        case TR::Snippet::IsCall:
            name = "Call Snippet";
            break;
        case TR::Snippet::IsUnresolvedCall:
            name = "Unresolved Call Snippet";
            break;
        case TR::Snippet::IsVirtualUnresolved:
            name = "Unresolved Virtual Call Snippet";
            break;
        case TR::Snippet::IsInterfaceCall:
            name = "Interface Call Snippet";
            break;
        case TR::Snippet::IsStackCheckFailure:
            name = "Stack Check Failure Snippet";
            break;
        case TR::Snippet::IsForceRecompilation:
            name = "Force Recompilation Snippet";
            break;
        case TR::Snippet::IsUnresolvedData:
            name = "Unresolved Data Snippet";
            break;
        case TR::Snippet::IsConstantData:
            name = "Constant Data Snippet";
            break;
        case TR::Snippet::IsRecompilation:
            name = "Recompilation Snippet";
            break;
        case TR::Snippet::IsHelperCall:
            name = "Helper Call Snippet";
            break;
        case TR::Snippet::IsMonitorEnter:
            name = "MonitorEnter Inc Counter";
            break;
        case TR::Snippet::IsMonitorExit:
            name = "MonitorExit Dec Counter";
            break;
        case TR::Snippet::IsHeapAlloc:
            name = "Heap Alloc Snippet";
            break;
        default:
            name = "<unknown snippet>";
            break;
    }

    log->prints(name);
}
