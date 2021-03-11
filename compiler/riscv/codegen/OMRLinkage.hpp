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

#ifndef OMR_RV_LINKAGE_INCL
#define OMR_RV_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { namespace RV { class Linkage; } }
namespace OMR { typedef OMR::RV::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "env/TRMemory.hpp"

class TR_FrontEnd;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

class RVMemoryArgument
   {
   public:
   // @@ TR_ALLOC(TR_Memory::RVMemoryArgument)

   TR::Register *argRegister;
   TR::MemoryReference *argMemory;
   TR::InstOpCode::Mnemonic opCode;
   };

#define FOR_EACH_REGISTER(machine, block)                                        \
   for (auto regNum = TR::RealRegister::FirstRealRegister;                       \
             regNum <= TR::RealRegister::LastRealRegister;                       \
             regNum++)                                                           \
      {                                                                          \
      TR::RealRegister *reg = machine->getRealRegister(regNum);                  \
      { block; }                                                                 \
      }                                                                          \

#define FOR_EACH_RESERVED_REGISTER(machine, props, block)                        \
   FOR_EACH_REGISTER(machine,                                                    \
   if (props._registerFlags[regNum] & Reserved)                                  \
      { block; }                                                                 \
   )

#define FOR_EACH_CALLEE_SAVED_REGISTER(machine, props, block)                    \
   FOR_EACH_REGISTER(machine,                                                    \
   if (props._registerFlags[regNum] == Preserved)                                \
      { block; }                                                                 \
   )

#define FOR_EACH_ASSIGNED_CALLEE_SAVED_REGISTER(machine, props, block)           \
   FOR_EACH_CALLEE_SAVED_REGISTER(machine, props,                                \
   if (reg->getHasBeenAssignedInMethod())                                        \
      { block; }                                                                 \
   )


struct RVLinkageProperties : public OMR::LinkageProperties
   {
   };
}; // namespace TR

namespace OMR
{
namespace RV
{
class OMR_EXTENSIBLE Linkage : public OMR::Linkage
   {
   public:

   /**
    * @brief Constructor
    * @param[in] cg : CodeGenerator
    */
   Linkage (TR::CodeGenerator *cg) : OMR::Linkage(cg) {}

   /**
    * @brief Parameter has to be on stack or not
    * @param[in] parm : parameter symbol
    * @return true if the parameter has to be on stack, false otherwise
    */
   virtual bool hasToBeOnStack(TR::ParameterSymbol *parm);
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
    * @brief Returns a MemoryReference for an outgoing argument
    * @param[in] argMemReg : register pointing to address for the outgoing argument
    * @param[in] argReg : register for the argument
    * @param[in] opCode : instruction OpCode for store to memory
    * @param[out] memArg : struct holding memory argument information
    * @return MemoryReference for the argument
    */
   virtual TR::MemoryReference *getOutgoingArgumentMemRef(TR::Register *argMemReg, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::RVMemoryArgument &memArg);

   /**
    * @brief Saves arguments
    * @param[in] cursor : instruction cursor
    */
   virtual TR::Instruction *saveArguments(TR::Instruction *cursor); // may need more parameters
   /**
    * @brief Loads up arguments
    * @param[in] cursor : instruction cursor
    */
   virtual TR::Instruction *loadUpArguments(TR::Instruction *cursor);
   /**
    * @brief Flushes arguments
    * @param[in] cursor : instruction cursor
    */
   virtual TR::Instruction *flushArguments(TR::Instruction *cursor);
   /**
    * @brief Sets parameter linkage register index
    * @param[in] method : method symbol
    */
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);

   /**
    * @brief Pushes integer argument to register
    * @return the register holding the argument
    */
   TR::Register *pushIntegerWordArg(TR::Node *child);
   /**
    * @brief Pushes address argument to register
    * @return the register holding the argument
    */
   TR::Register *pushAddressArg(TR::Node *child);
   /**
    * @brief Pushes long argument to register
    * @return the register holding the argument
    */
   TR::Register *pushLongArg(TR::Node *child);
   /**
    * @brief Pushes float argument to register
    * @return the register holding the argument
    */
   TR::Register *pushFloatArg(TR::Node *child);
   /**
    * @brief Pushes double argument to register
    * @return the register holding the argument
    */
   TR::Register *pushDoubleArg(TR::Node *child);

   /**
    * @brief Gets RV linkage properties
    * @return Linkage properties
    */
   virtual const TR::RVLinkageProperties& getProperties() = 0;

   /**
    * @brief Gets the number of argument registers
    * @param[in] kind : register kind
    * @return number of argument registers of specified kind
    */
   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind);

   /**
    * @brief Builds method arguments
    * @param[in] node : caller node
    * @param[in] dependencies : register dependency conditions
    */
   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies) = 0;

   /**
    * @brief Builds direct dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode) = 0;
   /**
    * @brief Builds indirect dispatch to method
    * @param[in] node : caller node
    */
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;

   };
} // RV
} // OMR

#endif
