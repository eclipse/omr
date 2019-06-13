/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef METHOD_INFO_HPP
#define METHOD_INFO_HPP

#include "il/DataTypes.hpp"
#include "ilgen.hpp"
#include "compiler_util.hpp"

#include <vector>
#include <string>
#include <stdexcept>

namespace Tril {

/**
 * @brief Class for extracting infromation about a Tril method from its AST node
 */
class MethodInfo {
    public:

        /**
         * @brief Constructs a MethodInfo object from a Tril AST node
         * @param methodNode is the Tril AST node
         */
        explicit MethodInfo(const ASTNode* methodNode) : _methodNode(methodNode) {
            auto returnTypeArg = _methodNode->getArgByName("return");
            _returnType = getTRDataTypes(returnTypeArg->getValue()->getString());
            _argTypes = parseArgTypes(_methodNode); 

            auto nameArg = _methodNode->getArgByName("name");
            if (nameArg != NULL) {
                _name = nameArg->getValue()->get<ASTValue::String_t>();
            }
        }

        /**
         * @brief Returns the name of the Tril method
         */
        const std::string& getName() const { return _name; }

        /**
         * @brief Retruns the AST node representing the Tril method
         */
        const ASTNode* getASTNode() const { return _methodNode; }

        /**
         * @brief Returns the AST representation of the method's body
         */
        const ASTNode* getBodyAST() const { return _methodNode->getChildren(); }

        /**
         * @brief Returns the return type of the method
         */
        TR::DataTypes getReturnType() const { return _returnType; }

        /**
         * @brief Returns a vector with the argument types of the method
         */
        const std::vector<TR::DataTypes>& getArgTypes() const { return _argTypes; }

        /**
         * @brief Returns the number of arguments the Tril method takes
         */
        std::size_t getArgCount() const { return _argTypes.size(); }

    private:
        const ASTNode* _methodNode;
        std::string _name;
        TR::DataTypes _returnType;
        std::vector<TR::DataTypes> _argTypes;
};

} // namespace Tril

#endif // METHOD_INFO_HPP
