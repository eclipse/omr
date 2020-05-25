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

#ifndef LLJB_METHODBUILDER_HPP
#define LLJB_METHODBUILDER_HPP

#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/VirtualMachineState.hpp"

#include "llvm/IR/Function.h"
#include "llvm/ADT/DenseMap.h"

namespace lljb
{

class Compiler;

class State : public TR::VirtualMachineState
   {
   public:

   virtual void Commit(TR::IlBuilder *b) override final {}

   virtual void Reload(TR::IlBuilder *b) override final {}

   virtual VirtualMachineState *MakeCopy() override final
      {
      return new State(*this);
      }

   virtual void MergeInto(TR::VirtualMachineState *other,
                        TR::IlBuilder *b) override final
      {
      }
   }; // class State

class MethodBuilder : public TR::MethodBuilder
   {

public:

   MethodBuilder(TR::TypeDictionary *td, llvm::Function &func, Compiler * compiler);

   ~MethodBuilder();

   static TR::IlType * getIlType(TR::TypeDictionary * td,llvm::Type * type);

   virtual bool buildIL() override;
   TR::IlValue * getIlValue(llvm::Value * value);
   char * getParamNameFromIndex(unsigned index);
   void mapIRtoIlValue(llvm::Value * irValue, TR::IlValue * ilValue);
   TR::BytecodeBuilder * getByteCodeBuilder(llvm::Value * value);
   void defineFunction(llvm::Function * func, std::size_t numParams, TR::IlType **paramTypes);
   void allocateLocal(llvm::Value * value, TR::IlType * allocatedType);
   char * getLocalNameFromValue(llvm::Value * value);
   char * getMemberNameFromIndex(unsigned index);

   /**
    * @brief Check whether this llvm::Value corresponds to a local variable that
    * we are going to store into or load from by checking the llvm::Value to local
    * variable name mapping
    *
    * @param value the source or dest llvm::Value
    * @return true if indirect load/store in cases of aggregates
    * @return false if value corresponds to a local variable
    */
   bool isIndirectLoadOrStore(llvm::Value * value);

private:

   void assignBuildersToBasicBlocks();
   void defineParameters();

   /**
    * @brief Generate a null-terminated parameter name using the index of the
    * parameter. For example, if paramIndex is 1, then the generated parameter
    * name would be "p1\0"
    *
    * @param paramIndex
    * @return const char*
    */
   char * stringifyParamIndex(unsigned paramIndex);

   llvm::Function& _function;
   llvm::DenseMap<llvm::Value *, TR::BytecodeBuilder * > _BBToBuilderMap;
   llvm::DenseMap<llvm::Value *, TR::IlValue *> _valueMap;
   llvm::DenseMap<unsigned, char *> _parameterMap;
   llvm::DenseMap<llvm::Function *, void *> _definedFunctions;
   Compiler * _compiler;
   llvm::DenseMap<llvm::Value *, char *> _localsMap;
   int32_t _numLocals;

   }; /* class MethodBuilder */

} /* namespace lljb */



#endif /* LLJB_METHODBUILDER_HPP */
