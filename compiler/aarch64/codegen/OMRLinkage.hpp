/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#ifndef OMR_ARM64_LINKAGE_INCL
#define OMR_ARM64_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { namespace ARM64 { class Linkage; } }
namespace OMR { typedef OMR::ARM64::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "env/TRMemory.hpp"

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

class ARM64MemoryArgument
   {
   public:
   TR_ALLOC(TR_Memory::ARM64MemoryArgument)

   TR::Register *argRegister;
   TR::MemoryReference *argMemory;
   TR::InstOpCode::Mnemonic opCode;
   };

// linkage properties
#define CallerCleanup       0x01
#define RightToLeft         0x02
#define IntegersInRegisters 0x04
#define LongsInRegisters    0x08
#define FloatsInRegisters   0x10

// register flags
#define Preserved                   0x01
#define IntegerReturn               0x02
#define IntegerArgument             0x04
#define FloatReturn                 0x08
#define FloatArgument               0x10
#define CallerAllocatesBackingStore 0x20
#define ARM64_Reserved              0x40

struct ARM64LinkageProperties
   {
   uint32_t _properties;
   uint32_t _registerFlags[TR::RealRegister::NumRegisters];
   uint8_t _numIntegerArgumentRegisters;
   uint8_t _firstIntegerArgumentRegister;
   uint8_t _numFloatArgumentRegisters;
   uint8_t _firstFloatArgumentRegister;
   TR::RealRegister::RegNum _argumentRegisters[TR::RealRegister::NumRegisters];
   uint8_t _firstIntegerReturnRegister;
   uint8_t _firstFloatReturnRegister;
   TR::RealRegister::RegNum _returnRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numAllocatableIntegerRegisters;
   uint8_t _numAllocatableFloatRegisters;
   uint32_t _allocationOrder[TR::RealRegister::NumRegisters];
   uint32_t _preservedRegisterMapForGC;
   TR::RealRegister::RegNum _methodMetaDataRegister;
   TR::RealRegister::RegNum _stackPointerRegister;
   TR::RealRegister::RegNum _framePointerRegister;
   TR::RealRegister::RegNum _computedCallTargetRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister; // for icallVMprJavaSendStatic
   uint8_t _numberOfDependencyGPRegisters;
   int8_t _offsetToFirstParm;
   int8_t _offsetToFirstLocal;

   uint32_t getNumIntArgRegs() const {return _numIntegerArgumentRegisters;}

   uint32_t getNumFloatArgRegs() const {return _numFloatArgumentRegisters;}

   uint32_t getProperties() const {return _properties;}

   uint32_t getCallerCleanup() const {return (_properties & CallerCleanup);}

   uint32_t getRightToLeft() const {return (_properties & RightToLeft);}

   uint32_t getIntegersInRegisters() const {return (_properties & IntegersInRegisters);}

   uint32_t getLongsInRegisters() const {return (_properties & LongsInRegisters);}

   uint32_t getFloatsInRegisters() const {return (_properties & FloatsInRegisters);}

   uint32_t getRegisterFlags(TR::RealRegister::RegNum regNum) const
      {
      return _registerFlags[regNum];
      }

   uint32_t getPreserved(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & Preserved);
      }

   uint32_t getIntegerReturn(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & IntegerReturn);
      }

   uint32_t getIntegerArgument(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & IntegerArgument);
      }

   uint32_t getFloatReturn(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & FloatReturn);
      }

   uint32_t getFloatArgument(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & FloatArgument);
      }

   uint32_t getCallerAllocatesBackingStore(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & CallerAllocatesBackingStore);
      }

   uint32_t getKilledAndNonReturn(TR::RealRegister::RegNum regNum) const
      {
      return ((_registerFlags[regNum] & (Preserved | IntegerReturn | FloatReturn)) == 0);
      }

   // get the indexth integer argument register
   TR::RealRegister::RegNum getIntegerArgumentRegister(uint32_t index) const
      {
      return _argumentRegisters[_firstIntegerArgumentRegister+index];
      }

   // get the indexth float argument register
   TR::RealRegister::RegNum getFloatArgumentRegister(uint32_t index) const
      {
      return _argumentRegisters[_firstFloatArgumentRegister+index];
      }

   // get the indexth integer return register
   TR::RealRegister::RegNum getIntegerReturnRegister(uint32_t index) const
      {
      return _returnRegisters[_firstIntegerReturnRegister+index];
      }

   // get the indexth float return register
   TR::RealRegister::RegNum getFloatReturnRegister(uint32_t index) const
      {
      return _returnRegisters[_firstFloatReturnRegister+index];
      }

   TR::RealRegister::RegNum getArgument(uint32_t index) const
      {
      return _argumentRegisters[index];
      }

   TR::RealRegister::RegNum getReturnRegister(uint32_t index) const
      {
      return _returnRegisters[index];
      }

   TR::RealRegister::RegNum getIntegerReturnRegister() const
      {
      return _returnRegisters[_firstIntegerReturnRegister];
      }

   TR::RealRegister::RegNum getLongReturnRegister() const
      {
      return _returnRegisters[_firstIntegerReturnRegister];
      }

   TR::RealRegister::RegNum getFloatReturnRegister() const
      {
      return _returnRegisters[_firstFloatReturnRegister];
      }

   TR::RealRegister::RegNum getDoubleReturnRegister() const
      {
      return _returnRegisters[_firstFloatReturnRegister];
      }

   int32_t getNumAllocatableIntegerRegisters() const
      {
      return _numAllocatableIntegerRegisters;
      }

   int32_t getNumAllocatableFloatRegisters() const
      {
      return _numAllocatableFloatRegisters;
      }

   uint32_t *getRegisterAllocationOrder() const
      {
      return (uint32_t *) _allocationOrder;
      }

   uint32_t getPreservedRegisterMapForGC() const
      {
      return _preservedRegisterMapForGC;
      }

   TR::RealRegister::RegNum getMethodMetaDataRegister() const
      {
      return _methodMetaDataRegister;
      }

   TR::RealRegister::RegNum getStackPointerRegister() const
      {
      return _stackPointerRegister;
      }

   TR::RealRegister::RegNum getFramePointerRegister() const
      {
      return _framePointerRegister;
      }

   TR::RealRegister::RegNum getComputedCallTargetRegister() const
      {
      return _computedCallTargetRegister;
      }

   TR::RealRegister::RegNum getVTableIndexArgumentRegister() const
      {
      return _vtableIndexArgumentRegister;
      }

   TR::RealRegister::RegNum getJ9MethodArgumentRegister() const
      {
      return _j9methodArgumentRegister;
      }

   int32_t getOffsetToFirstParm() const {return _offsetToFirstParm;}

   int32_t getOffsetToFirstLocal() const {return _offsetToFirstLocal;}

   uint32_t getNumberOfDependencyGPRegisters() const {return _numberOfDependencyGPRegisters;}
   };

}

namespace OMR
{
namespace ARM64
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
    * @brief Initializes ARM64 RealRegister linkage
    */
   virtual void initARM64RealRegisterLinkage();

   /**
    * @brief Returns a MemoryReference for an outgoing argument
    * @param[in] argMemReg : register pointing to address for the outgoing argument
    * @param[in] argReg : register for the argument
    * @param[in] opCode : instruction OpCode for store to memory
    * @param[out] memArg : struct holding memory argument information
    * @return MemoryReference for the argument
    */
   virtual TR::MemoryReference *getOutgoingArgumentMemRef(TR::Register *argMemReg, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::ARM64MemoryArgument &memArg);

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
    * @brief Gets ARM64 linkage properties
    * @return Linkage properties
    */
   virtual const TR::ARM64LinkageProperties& getProperties() = 0;

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

   /**
    * @brief Stores parameters passed in linkage registers to the stack where the
    *        method body expects to find them.
    *
    * @param[in] cursor : the instruction cursor to begin inserting copy instructions
    * @param[in] parmsHaveBeenStored : true if the parameters have been stored to the stack
    *
    * @return The instruction cursor after copies inserted.
    */
   TR::Instruction *copyParametersToHomeLocation(TR::Instruction *cursor, bool parmsHaveBeenStored = false);
   };
} // ARM64
} // TR

#endif
