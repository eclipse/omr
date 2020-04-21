/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "CallConverter.hpp"
#include "ilgen.hpp"

namespace Tril {

TR::Node* CallConverter::impl(const ASTNode* tree, IlGenState* state) {
    TR::Node* node = NULL;
    auto childCount = tree->getChildCount();
    auto opcode = OpCodeTable(tree->getName());
    if (opcode.isCall()) {
        auto compilation = TR::comp();

        const auto addressArg = tree->getArgByName("address");
        if (addressArg == NULL) {
            TraceIL("  Found call without required address associated\n");
            throw CallGenError("Found call without required address associated");
        }
        /* I don't want to extend the ASTValue type system to include pointers at this moment,
        * so for now, we do the reinterpret_cast to pointer type from long
        */
#if defined(OMR_ENV_DATA64)
            TraceIL("  is call with target 0x%016llX", addressArg->getValue()->get<uintptr_t>());
#else
            TraceIL("  is call with target 0x%08lX", addressArg->getValue()->get<uintptr_t>());
#endif /* OMR_ENV_DATA64 */
            const auto targetAddress = reinterpret_cast<void*>(addressArg->getValue()->get<uintptr_t>());

            /* To generate a call, will create a ResolvedMethodSymbol, but we need to know the
            * signature. The return type is intuitable from the call node, but the arguments
            * must be provided explicitly, hence the args list, that mimics the args list of
            * (method ...)
            */
            const auto argList = parseArgTypes(tree);
            auto argIlTypes = std::vector<TR::IlType*>(argList.size());
            auto output = argIlTypes.begin();
            for (auto iter = argList.begin(); iter != argList.end(); iter++, output++) {
                *output = state->getTypes()->PrimitiveType(*iter);
            }
            auto returnIlType = state->getTypes()->PrimitiveType(opcode.getType());
            TR::ResolvedMethod* method = new (compilation->trHeapMemory()) TR::ResolvedMethod("file",
                                                                                            "line",
                                                                                            "name",
                                                                                            argIlTypes.size(),
                                                                                            &argIlTypes[0],
                                                                                            returnIlType,
                                                                                            targetAddress,
                                                                                            0);
            
            TR::SymbolReference *methodSymRef = state->symRefTab()->findOrCreateStaticMethodSymbol(JITTED_METHOD_INDEX, -1, method);

            /* Default linkage is always system, unless overridden */
            TR_LinkageConventions linkageConvention = TR_System;

            /* Calls can have a customized linkage */
            const auto* linkageArg= tree->getArgByName("linkage");
            if (linkageArg != NULL) {
                const auto* linkageString = linkageArg->getValue()->getString();
                linkageConvention = convertStringToLinkage(linkageString);
                if (linkageConvention == TR_None) {
                    TraceIL("  failed to find customized linkage %s, aborting parsing\n", linkageString);
                    std::string linkageString_str(linkageString);
                    throw CallGenError("Failed to find customized linkage " + linkageString_str + ", aborting parsing");              
                }
                TraceIL("  customizing linakge of call to %s (linkageConvention=%d)\n", linkageString, linkageConvention);
            }

            /* Set linkage explicitly */
            methodSymRef->getSymbol()->castToMethodSymbol()->setLinkage(linkageConvention);
            return TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, methodSymRef);
     }
     else {
        return NULL;
     }
}

} // namespace Tril
