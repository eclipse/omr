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

#ifndef METHOD_COMPILER_HPP
#define METHOD_COMPILER_HPP

#include "method_info.hpp"
#include "ilgen.hpp"

#include "ilgen/TypeDictionary.hpp"

namespace Tril {

/**
 * @brief An interface for compiling Tril methods
 *
 * The intention of this interface is to provide a standard way of compiling
 * Tril methods that hides the details of how compilation is actually done.
 */
class MethodCompiler {
    public:

        /**
         * @brief Constructs an intance of the Tril method compiler
         * @param methodNode is the AST node representing the Tril method to be compiled
         */
        explicit MethodCompiler(const ASTNode* methodNode)
            : _method(methodNode),
              _entry_point(NULL)
        {}

        virtual ~MethodCompiler() = default;

        /**
         * @brief Compiles the Tril method
         * @return a return code for the compiler
         *
         * This function is to be implemented by classes realizing this
         * interface. It is up to concrete implementations to decided how
         * compilation is done and what the meaning of the returned value is.
         *
         * If compilation succeeds, meaning that the compiled code is ready to
         * be executed, the entry point to the compiled body *must* be set
         * (using `setEntryPoint()`) before this function returns.
         *
         * If compilation fails for any reason, the entry point *must* be set to
         * null. Since this is its default value, ensuring that the entry point
         * is set if and only if compilation succeeds is enough to meet this
         * requirement.
         */
        virtual int32_t compile() = 0;

        /**
         * @brief Returns the entry point to the compiled body, or null if not yet compiled
         * @tparam T is the function pointer type of the entry point
         *
         * Specifying a type for T that is not a function pointer results in a
         * compilation error.
         */
        template <typename T>
        T getEntryPoint() {
#if !defined(J9ZOS390) && !defined(AIXPPC)
            static_assert( std::is_pointer<T>::value &&
                           std::is_function<typename std::remove_pointer<T>::type>::value,
                          "Attempted to get entry point using a non-function-pointer type.");
#endif

            return reinterpret_cast<T>(_entry_point);
        }

    protected:

        /**
         * @brief Sets the entry point to the compiled body
         *
         * This value can later be retrieved using `getEntryPoint<>()`
         */
        void setEntryPoint(void* entry_point) { _entry_point = entry_point; }

        /**
         * @brief Returns the MethodInfo object for the Tril method to be compiled
         */
        const Tril::MethodInfo& getMethodInfo() const { return _method; }

    private:
        Tril::MethodInfo _method;   // Tril method to be compiled
        void* _entry_point;         // entry point to the compiled body
};

} // namespace Tril

#endif // METHOD_COMPILER_HPP
