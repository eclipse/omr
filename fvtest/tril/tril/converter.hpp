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

#ifndef ASTTOTRNODE_HPP
#define ASTTOTRNODE_HPP

#include "ast.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "state.hpp"

#define TraceEnabled    (state->comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(state->comp(), m, ##__VA_ARGS__);}}

namespace Tril {

class ASTToTRNode {
    public:
        /**
         * @brief Given an AST structure and corresponding IlGenState, returns the basic TR::Node
         * @param tree the AST structure
         * @param state the object that encapsulating the IL generator state
         * @return TR::Node represented by the AST
         */
        TR::Node* convert(const ASTNode* tree, IlGenState* state) {
            auto n = impl(tree, state);
            if (n == NULL && next != NULL) {
                return next->convert(tree, state);
            } else {
                return n;
            }
        }
        /**
         * @brief Constructs a ASTNode to TRNode
         * @param n is the next object that gets a chance to convert the AST node if the current on fails
         */
        explicit ASTToTRNode(ASTToTRNode* n = NULL) : next(n) {}

    protected:
        /**
         * @brief Generates the TR::Node based on the opcode type of the given AST structure
         * @param tree the AST structure
         * @param state the object that encapsulating the IL generator state
         * @return TR::Node represented by the AST
         */
        virtual TR::Node* impl(const ASTNode* tree, IlGenState* state) = 0;
    
    private:
        ASTToTRNode* next;
};

class ILGenError {
    private:
        std::string message;

    public:
        explicit ILGenError(std::string message) : message(message) {}

        const std::string &what() { return message; }
};

}
#endif
