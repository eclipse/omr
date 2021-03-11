/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef OMR_POWER_LINKAGE_INCL
#define OMR_POWER_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { namespace Power { class Linkage; } }
namespace OMR { typedef OMR::Power::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RegisterConstants.hpp"
#include "compile/CompilationTypes.hpp"
#include "env/TRMemory.hpp"
#include "infra/Annotations.hpp"

#include <string.h> // TODO: remove once linkage props are made static

namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
template <class T> class List;

namespace TR {

class PPCMemoryArgument
   {
   public:
   TR_ALLOC(TR_Memory::PPCMemoryArgument)

   TR::Register           *argRegister;
   TR::MemoryReference *argMemory;
   TR::InstOpCode::Mnemonic          opCode;
   };


struct PPCLinkageProperties : public OMR::LinkageProperties
   {
   /**
    * @brief Initialize derived properties from register flags. This *must* be called
    * after _registerFlags are populated.
    */
   void initialize();

   // TODO: remove once linkage props are made static
   PPCLinkageProperties() { memset(this, 0, sizeof(this)); }

   // POWER specific properties follows
   uint8_t _numVectorArgumentRegisters;
   uint8_t _firstVectorArgumentRegister;
   uint8_t _firstVectorReturnRegister;
   uint8_t _numAllocatableVectorRegisters;

   // TODO: document the exact meaning of this member. How this is different
   // from _argumentRegisters[_firstIntegerArgumentRegister]?
   TR::RealRegister::RegNum _firstAllocatableIntegerArgumentRegister;
   TR::RealRegister::RegNum _lastAllocatableIntegerVolatileRegister;

   // TODO: document the exact meaning of this member. How this is different
   // from _argumentRegisters[_firstFloatArgumentRegister]?
   TR::RealRegister::RegNum _firstAllocatableFloatArgumentRegister;
   TR::RealRegister::RegNum _lastAllocatableFloatVolatileRegister;

   // TODO: document the exact meaning of this member. How this is different
   // from _argumentRegisters[_firstVectorArgumentRegister]?
   TR::RealRegister::RegNum _firstAllocatableVectorArgumentRegister;
   TR::RealRegister::RegNum _lastAllocatableVectortVolatileRegister;

   uint8_t _numAllocatableCCRegisters;
   uint32_t _allocationOrder[TR::RealRegister::NumRegisters];
   uint32_t _preservedRegisterMapForGC;

   TR::RealRegister::RegNum _TOCBaseRegister;

   uint32_t getNumVectorArgRegs() const {return _numVectorArgumentRegisters;}

   uint32_t getSmallIntParmsAlignedRight() const { return (_properties & SmallIntParmsAlignedRight); }

   uint32_t getReserved(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & Reserved);
      }

   // get the indexth vector argument register
   TR::RealRegister::RegNum getVectorArgumentRegister(uint32_t index) const
      {
      return _argumentRegisters[_firstVectorArgumentRegister+index];
      }

   // get the indexth vector return register
   TR::RealRegister::RegNum getVectorReturnRegister(uint32_t index) const
      {
      return _returnRegisters[_firstVectorReturnRegister+index];
      }

   TR::RealRegister::RegNum getVectorReturnRegister() const
      {
      return _returnRegisters[_firstVectorReturnRegister];
      }

   // TODO: see comment on _firstAllocatableIntegerArgumentRegister
   int32_t getFirstAllocatableIntegerArgumentRegister() const
      {
      return _firstAllocatableIntegerArgumentRegister;
      }

   int32_t getLastAllocatableIntegerVolatileRegister() const
      {
      return _lastAllocatableIntegerVolatileRegister;
      }

   // TODO: see comment on _firstAllocatableFloatArgumentRegister
   int32_t getFirstAllocatableFloatArgumentRegister() const
      {
      return _firstAllocatableFloatArgumentRegister;
      }

   int32_t getLastAllocatableFloatVolatileRegister() const
      {
      return _lastAllocatableFloatVolatileRegister;
      }

   int32_t getNumAllocatableVectorRegisters() const
      {
      return _numAllocatableVectorRegisters;
      }

   uint32_t *getRegisterAllocationOrder() const
      {
      return (uint32_t*) _allocationOrder;
      }

   TR::RealRegister::RegNum getTOCBaseRegister() const
      {
      return _TOCBaseRegister;
      }


   };

} // namespace TR

namespace OMR
{
namespace Power
{
class OMR_EXTENSIBLE Linkage : public OMR::Linkage
   {
   public:

   Linkage (TR::CodeGenerator *cg) : OMR::Linkage(cg) {}

   virtual bool hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   virtual void initPPCRealRegisterLinkage();
   virtual TR::MemoryReference *getOutgoingArgumentMemRef(int32_t argSize, TR::Register *argReg, TR::InstOpCode::Mnemonic opCode, TR::PPCMemoryArgument &memArg, uint32_t len);
   virtual TR::Instruction *saveArguments(TR::Instruction *cursor, bool fsd=false, bool saveOnly=false);
   virtual TR::Instruction *saveArguments(TR::Instruction *cursor, bool fsd, bool saveOnly, List<TR::ParameterSymbol> &parmList);
   virtual TR::Instruction *loadUpArguments(TR::Instruction *cursor);
   virtual TR::Instruction *flushArguments(TR::Instruction *cursor);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);
   TR::Register *pushIntegerWordArg(TR::Node *child);

   TR::Register *pushAddressArg(TR::Node *child);

   TR::Register *pushThis(TR::Node *child);

   TR::Register *pushLongArg(TR::Node *child);

   TR::Register *pushFloatArg(TR::Node *child);

   TR::Register *pushDoubleArg(TR::Node *child);

   virtual const TR::PPCLinkageProperties& getProperties() = 0;

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind);

   virtual int32_t buildArgs(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies) = 0;

   virtual void buildVirtualDispatch(
         TR::Node *callNode,
         TR::RegisterDependencyConditions *dependencies,
         uint32_t sizeOfArguments) = 0;

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode) = 0;

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;

   TR_ReturnInfo getReturnInfoFromReturnType(TR::DataType);
   };
}
}

#endif
