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

#include "lljb/Compiler.hpp"
#include "lljb/MethodBuilder.hpp"

#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdio>
#include <ctime>
#include <cstring>

#if defined(OSX) || defined(LINUX)
#include <unistd.h>
#endif

#include <string>
#include <vector>

namespace lljb
{

Compiler::Compiler(Module * module)
    :
   _typeDictionary(),
   _module(module)
   {
   defineTypes();
   }

void
Compiler::compile()
   {
   for (auto func = _module->funcIterBegin(); func != _module->funcIterEnd() ; func++)
      {
      if (!(*func).isDeclaration())
         mapCompiledFunction(&*func,(compileMethod(*func)));
      }
   }

JittedFunction
Compiler::getJittedCodeEntry()
   {
   llvm::Function * entryFunction = _module->getMainFunction();
   assert(entryFunction && "entry function not found");
   return (JittedFunction) getFunctionAddress(entryFunction);
   }

void *
Compiler::compileMethod(llvm::Function &func)
   {
   MethodBuilder methodBuilder(&_typeDictionary, func, this);
   void * result = 0;
   methodBuilder.Compile(&result);
   return result;
   }

void
Compiler::mapCompiledFunction(llvm::Function * llvmFunc, void * entry)
   {
   _compiledFunctionMap[llvmFunc] = entry;
   }

void *
Compiler::getFunctionAddress(llvm::Function * func)
   {
   void * entry = _compiledFunctionMap[func];
   if (!entry)
      {
      if (func->getName().equals("printf")) entry = (void *) &printf;
      else if (func->getName().equals("putc")) entry = (void *) &putc;
      else if (func->getName().equals("clock_gettime")) entry = (void *) &clock_gettime;
#if defined(OSX) || defined(LINUX)
      else if (func->getName().contains("usleep")) entry = (void *) &usleep;
#endif
      else if (func->getName().equals("strcpy")) entry = (void *) &strcpy;
      else if (func->getName().equals("strlen")) entry = (void *) &strlen;
      else
         {
         llvm::outs() << "function being looked up:" << func->getName() << "\n";
         assert( 0 && "function not found");
         }
      }
   return entry;
   }

char *
Compiler::stringifyObjectMemberIndex(unsigned memberIndex)
   {
   // All structs will share the same name for their members. First check if a member for an
   // index has already been generated before creating a new one.
   char * memberString = _objectMemberMap[memberIndex];
   if (memberString) return memberString;

   std::string indexString = std::to_string(memberIndex);
   memberString = new char[indexString.length()+2];
   sprintf(memberString, "m%d",memberIndex);
   _objectMemberMap[memberIndex] = memberString;
   return memberString;
   }

void
Compiler::defineTypes()
   {
   std::vector<llvm::StructType*> structTypes = _module->getLLVMModule()->getIdentifiedStructTypes();
   for (auto structType : structTypes)
      {
      llvm::StringRef structName = structType->getName();
      _typeDictionary.DefineStruct(structName.data());
      unsigned elIndex = 0;
      for (auto elIter = structType->element_begin(); elIter != structType->element_end(); ++elIter)
         {
         llvm::Type * fieldType = structType->getElementType(elIndex);
         if (fieldType->isAggregateType())
            {
            _typeDictionary.DefineField(
               structName.data(),
               stringifyObjectMemberIndex(elIndex),
               _typeDictionary.PointerTo(MethodBuilder::getIlType(
                  &_typeDictionary,
                  fieldType)));
            }
         else
            {
               _typeDictionary.DefineField(
                  structName.data(),
                  stringifyObjectMemberIndex(elIndex),
                  MethodBuilder::getIlType(
                     &_typeDictionary,
                     fieldType));
            }
         elIndex++;
         }
      _typeDictionary.CloseStruct(structName.data());
      }
   }

char *
Compiler::getObjectMemberNameFromIndex(unsigned index)
   {
   return _objectMemberMap[index];
   }

Compiler::~Compiler()
   {
   _objectMemberMap.clear();
   }


} // namespace lljb
