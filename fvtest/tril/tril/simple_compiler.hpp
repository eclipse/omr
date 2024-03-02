/*******************************************************************************
 * Copyright IBM Corp. and others 2017
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

#ifndef JITBUILDER_COMPILER_HPP
#define JITBUILDER_COMPILER_HPP

#include "method_compiler.hpp"

namespace TR { class IlVerifier; } 

namespace Tril {

/**
 * @brief Concrete realization of MethodCompiler 
 */
class SimpleCompiler : public Tril::MethodCompiler {
    public:
        explicit SimpleCompiler(const ASTNode* methodNode)
            : MethodCompiler(methodNode) {}

        /**
         * @brief Compiles the Tril method
         * @return 0 on compilation success, an error code otherwise
         */
        int32_t compile() /* override */ ;

        /**
         * @brief Start compilation with a verifier. 
         * @param verifier The verifier to run. 
         * @return 0 on complilation success, an error code or exception otherwise. 
         */
        int32_t compileWithVerifier(TR::IlVerifier* verifier);

    protected:
        static TR::TypeDictionary* types;
};

} // namespace Tril

#endif // JITBUILDER_COMPILER_HPP
