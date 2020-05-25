/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef LLJB_COMPILER_HPP
#define LLJB_COMPILER_HPP

#include "lljb/Module.hpp"
#include "ilgen/TypeDictionary.hpp"

#include "llvm/ADT/DenseMap.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace lljb
{

typedef int32_t (*JittedFunction)();

class Compiler
   {

public:

   Compiler(Module * module);

   ~Compiler();

   /**
    * @brief Compile each function in the module
    *
    */
   void compile();

   /**
    * @brief Get the pointer to the entry function in the module.
    * If there are more than one function in the module, then the address
    * corresponds to "main"
    *
    * @return JittedFunction
    */
   JittedFunction getJittedCodeEntry();

   /**
    * @brief create a mapping between the llvm::Function in a module to
    * the adress in the codecache with the compiled method
    *
    * @param llvmFunc the llvm::Function *
    * @param entry the void *
    */
   void mapCompiledFunction(llvm::Function * llvmFunc, void * entry);

   /**
    * @brief Get the address of the compiled function corresponding to the
    * llvm::Function of the compiled module
    *
    * @param func the llvm::Function *
    * @return void * the address of the compiled function
    */
   void * getFunctionAddress(llvm::Function * func);

   /**
    * @brief Get the name of an aggregate's field from its index.
    *
    * @param index
    * @return char*
    */
   char * getObjectMemberNameFromIndex(unsigned index);

   private:

   /**
    * @brief Define Structs in the llvm Module
    *
    */
   void defineTypes();

   /**
    * @brief construct OMR IL for an llvm::Function and const
    *
    * @param func
    * @return void*
    */
   void * compileMethod(llvm::Function &func);

   /**
    * @brief Generate a null-terminated parameter name using the index of an
    * aggregate type's member. This is needed because we refer to object fields
    * using their names. Example of generated name if member index is 1: "m1\0"
    *
    * @param memberIndex
    * @return char*
    */
   char * stringifyObjectMemberIndex(unsigned memberIndex);

   TR::TypeDictionary _typeDictionary;
   llvm::DenseMap<llvm::Function *, void *> _compiledFunctionMap;
   llvm::DenseMap<unsigned, char *> _objectMemberMap;
   Module * _module;
   };

} /** namespace lljb */

#endif /* LLJB_COMPILER_HPP */
