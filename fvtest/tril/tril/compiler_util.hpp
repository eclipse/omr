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

#ifndef COMPILER_UTIL_HPP
#define COMPILER_UTIL_HPP
#include <stdexcept>
#include "il/DataTypes.hpp"
#include "ast.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "infra/Flags.hpp"

namespace Tril { 
/**
 * @brief Gets the TR::DataTypes value from the data type's name
 * @param name is the name of the data type as a string
 * @return the TR::DataTypes value corresponding to the specified name
 */
static TR::DataTypes getTRDataTypes(const std::string& name) {
   if (name == "Int8") return TR::Int8;
   else if (name == "Int16") return TR::Int16;
   else if (name == "Int32") return TR::Int32;
   else if (name == "Int64") return TR::Int64;
   else if (name == "Address") return TR::Address;
   else if (name == "Float") return TR::Float;
   else if (name == "Double") return TR::Double;
   else if (name == "VectorInt8") return TR::VectorInt8;
   else if (name == "VectorInt16") return TR::VectorInt16;
   else if (name == "VectorInt32") return TR::VectorInt32;
   else if (name == "VectorInt64") return TR::VectorInt64;
   else if (name == "VectorFloat") return TR::VectorFloat;
   else if (name == "VectorDouble") return TR::VectorDouble;
   else if (name == "NoType") return TR::NoType;
   else {
      throw std::runtime_error(static_cast<const std::string&>(std::string("Unknown type name: ").append(name)));
   }
}

/**
 * @brief Return a parsed array of DataTypes from a node with an 
 *        "args" list.
 * @param node is the node being processed.
 * @return the std::vector of TR::DataTypes corresponding to the 
 *         args=[Type1,Type2,...] attached to the ASTNode. 
 */
static std::vector<TR::DataTypes> parseArgTypes(const ASTNode* node) { 

   std::vector<TR::DataTypes> argTypes;
   auto argTypesArg = node->getArgByName("args");
   if (argTypesArg != NULL) {
      auto typeValue = argTypesArg->getValue();
      while (typeValue != NULL) {
         argTypes.push_back(getTRDataTypes(typeValue->getString()));
         typeValue = typeValue->next;
      }
   }

   return argTypes;
}

/**
 * @brief Return a parsed 32-bit flags value from a node with a
 *        "flags" list.
 * @param node is the node being processed.
 * @return a flags32_t representing the flags that the ASTNode's
 *         "flags" list specified should be set.
 */
static OMR::flags32_t parseFlags(const ASTNode* node) {
   OMR::flags32_t flags = 0;

   auto flagsArg = node->getArgByName("flags");
   if (flagsArg != NULL) {
      auto flagsValue = flagsArg->getValue();
      while (flagsValue != NULL) {
         if (flagsValue->getInteger() >= 0 && flagsValue->getInteger() < 32)
            flags.set(1u << flagsValue->getInteger());
         flagsValue = flagsValue->next;
      }
   }

   return flags;
}

/**
 * @brief  Convert a linkage convention name to a LinkageConvention 
 *         appropriate for setting on a MethodSymbol. 
 *
 * @return the linkage convention, or TR_None if the name is not 
 *         recognized. 
 */
static TR_LinkageConventions convertStringToLinkage(const char * linkageName) { 
   std::string ln = linkageName;  
   if (ln == "system") {
      return TR_System;
   }

   // Not found
   return TR_None; 
}

}

#endif

