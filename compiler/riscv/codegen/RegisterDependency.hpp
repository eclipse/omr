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

#ifndef TR_REGISTER_DEPENDENCY_INCL
#define TR_REGISTER_DEPENDENCY_INCL

#include "codegen/OMRRegisterDependency.hpp"

namespace TR
{

class RegisterDependencyConditions : public OMR::RegisterDependencyConditionsConnector
   {
   public:

   /**
    * @brief Constructor
    */
   RegisterDependencyConditions() : OMR::RegisterDependencyConditionsConnector () {}

   /**
    * @brief Constructor
    * @param[in] numPreConds : # of pre-conditions
    * @param[in] numPostConds : # of post-conditions
    * @param[in] m : memory
    */
   RegisterDependencyConditions(uint16_t numPreConds, uint16_t numPostConds, TR_Memory * m) :
      OMR::RegisterDependencyConditionsConnector(numPreConds, numPostConds, m) {}

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator object
    * @param[in] node : node
    * @param[in] extranum : additional # of conditions
    * @param[in] cursorPtr : instruction cursor
    */
   RegisterDependencyConditions(TR::CodeGenerator *cg, TR::Node *node, uint32_t extranum, TR::Instruction **cursorPtr=NULL) :
      OMR::RegisterDependencyConditionsConnector(cg, node, extranum, cursorPtr) {}

   };
} // TR

/**
 * @brief Generates RegisterDependencyConditions
 * @param[in] cg : CodeGenerator object
 * @param[in] node : node
 * @param[in] extranum : additional # of conditions
 * @param[in] cursorPtr : instruction cursor
 * @return generated RegisterDependencyConditions
 */
inline TR::RegisterDependencyConditions *
generateRegisterDependencyConditions(TR::CodeGenerator *cg, TR::Node *node, uint32_t extranum, TR::Instruction **cursorPtr=NULL)
   {
   return new (cg->trHeapMemory()) TR::RegisterDependencyConditions(cg, node, extranum, cursorPtr);
   }

#endif
