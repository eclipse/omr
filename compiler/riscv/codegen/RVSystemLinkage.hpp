/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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

#ifndef OMR_RV_SYSTEMLINKAGE_INCL
#define OMR_RV_SYSTEMLINKAGE_INCL

#include "codegen/Linkage.hpp"

#include <stdint.h>
#include "codegen/Register.hpp"

namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
template <class T> class List;

namespace TR {

struct RVSystemLinkageProperties : public RVLinkageProperties {
   RVSystemLinkageProperties();
};

class RVSystemLinkage : public TR::Linkage
   {
   protected:

   static const TR::RVSystemLinkageProperties _properties;

   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    */
   RVSystemLinkage(TR::CodeGenerator *cg);

   /**
    * @brief Gets linkage properties
    * @return Linkage properties
    */
   virtual const TR::RVLinkageProperties& getProperties();

   /**
    * @brief Gets the RightToLeft flag
    * @return RightToLeft flag
    */
   virtual uint32_t getRightToLeft();
   /**
    * @brief Maps symbols to locations on stack
    * @param[in] method : method for which symbols are mapped on stack
    */
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   /**
    * @brief Maps an automatic symbol to an index on stack
    * @param[in] p : automatic symbol
    * @param[in/out] stackIndex : index on stack
    */
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   /**
    * @brief Initializes RV RealRegister linkage
    */
   virtual void initRVRealRegisterLinkage();

   /**
    * @brief Creates method prologue
    * @param[in] cursor : instruction cursor
    */
   virtual void createPrologue(TR::Instruction *cursor);
   /**
    * @brief Creates method prologue
    * @param[in] cursor : instruction cursor
    */
   virtual void createPrologue(TR::Instruction *cursor, List<TR::ParameterSymbol> &parm);

   /**
    * @brief Creates method epilogue
    * @param[in] cursor : instruction cursor
    */
   virtual void createEpilogue(TR::Instruction *cursor);

   /**
    * @brief Builds method arguments
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    */
   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies);


   /**
    * @brief Builds dispatch to method (either direct or indirect)
    * @param[in] node : caller node
    */
   virtual TR::Register *buildDispatch(TR::Node *callNode);

   /**
    * @brief Builds direct dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode)
      {
      return buildDispatch(callNode);
      }

   /**
    * @brief Builds indirect dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode)
      {
      return buildDispatch(callNode);
      }

   /**
    * @brief Provides the entry point in a method to use when that method is invoked
    *        from a method compiled with the same linkage.
    *
    * @details
    *    When asked on the method currently being compiled, this API will return 0 if
    *    asked before code memory has been allocated.
    *
    *    The compiled method entry point may be the same as the interpreter entry point.
    *
    * @return The entry point for compiled methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromCompiledMethod();

   /**
    * @brief Provides the entry point in a method to use when that method is invoked
    *        from an interpreter using the same linkage.
    *
    * @details
    *    When asked on the method currently being compiled, this API will return 0 if
    *    asked before code memory has been allocated.
    *
    *    The compiled method entry point may be the same as the interpreter entry point.
    *
    * @return The entry point for interpreted methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromInterpretedMethod();

   };

}

#endif
