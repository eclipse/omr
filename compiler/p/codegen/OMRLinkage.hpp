/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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


// linkage properties
#define CallerCleanup             0x01
#define RightToLeft               0x02
#define IntegersInRegisters       0x04
#define LongsInRegisters          0x08
#define FloatsInRegisters         0x10
#define SmallIntParmsAlignedRight 0x20

// register flags
#define Preserved                   0x01
#define IntegerReturn               0x02
#define IntegerArgument             0x04
#define FloatReturn                 0x08
#define FloatArgument               0x10
#define CallerAllocatesBackingStore 0x20
#define PPC_Reserved                0x40

struct PPCLinkageProperties
   {
   uint32_t _properties;
   uint32_t _registerFlags[TR::RealRegister::NumRegisters];
   uint8_t _numIntegerArgumentRegisters;
   uint8_t _firstIntegerArgumentRegister;
   uint8_t _numFloatArgumentRegisters;
   uint8_t _firstFloatArgumentRegister;
   uint8_t _numVectorArgumentRegisters;
   uint8_t _firstVectorArgumentRegister;
   TR::RealRegister::RegNum _argumentRegisters[TR::RealRegister::NumRegisters];
   uint8_t _firstIntegerReturnRegister;
   uint8_t _firstFloatReturnRegister;
   uint8_t _firstVectorReturnRegister;
   TR::RealRegister::RegNum _returnRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numAllocatableIntegerRegisters;
   uint8_t _firstAllocatableIntegerArgumentRegister;
   uint8_t _lastAllocatableIntegerVolatileRegister;
   uint8_t _numAllocatableFloatRegisters;
   uint8_t _firstAllocatableFloatArgumentRegister;
   uint8_t _lastAllocatableFloatVolatileRegister;
   uint8_t _numAllocatableVectorRegisters;
   uint8_t _firstAllocatableVectorArgumentRegister;
   uint8_t _lastAllocatableVectortVolatileRegister;
   uint8_t _numAllocatableCCRegisters;
   uint32_t _allocationOrder[TR::RealRegister::NumRegisters];
   uint32_t _preservedRegisterMapForGC;
   TR::RealRegister::RegNum _methodMetaDataRegister;
   TR::RealRegister::RegNum _normalStackPointerRegister;
   TR::RealRegister::RegNum _alternateStackPointerRegister;
   TR::RealRegister::RegNum _TOCBaseRegister;
   TR::RealRegister::RegNum _computedCallTargetRegister;  // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister;    // for icallVMprJavaSendStatic
   uint8_t _numberOfDependencyGPRegisters;
   int8_t _offsetToFirstLocal;

   uint32_t getNumIntArgRegs() const {return _numIntegerArgumentRegisters;}

   uint32_t getNumFloatArgRegs() const {return _numFloatArgumentRegisters;}

   uint32_t getNumVectorArgRegs() const {return _numVectorArgumentRegisters;}

   uint32_t getProperties() const {return _properties;}

   uint32_t getCallerCleanup() const {return (_properties & CallerCleanup);}

   uint32_t getRightToLeft() const {return (_properties & RightToLeft);}

   uint32_t getIntegersInRegisters() const {return (_properties & IntegersInRegisters);}

   uint32_t getLongsInRegisters() const {return (_properties & LongsInRegisters);}

   uint32_t getFloatsInRegisters() const {return (_properties & FloatsInRegisters);}

   uint32_t getSmallIntParmsAlignedRight() const { return (_properties & SmallIntParmsAlignedRight); }

   uint32_t getRegisterFlags(TR::RealRegister::RegNum regNum) const
      {
      return _registerFlags[regNum];
      }

   uint32_t getPreserved(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & Preserved);
      }

   uint32_t getReserved(TR::RealRegister::RegNum regNum) const
      {
      return (_registerFlags[regNum] & PPC_Reserved);
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

   // get the indexth vector argument register
   TR::RealRegister::RegNum getVectorArgumentRegister(uint32_t index) const
      {
      return _argumentRegisters[_firstVectorArgumentRegister+index];
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

   // get the indexth vector return register
   TR::RealRegister::RegNum getVectorReturnRegister(uint32_t index) const
      {
      return _returnRegisters[_firstVectorReturnRegister+index];
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
      return _returnRegisters[0];
      }

   // for 32-bit use only
   TR::RealRegister::RegNum getLongLowReturnRegister() const
      {
      return _returnRegisters[1];
      }

   // for 32-bit use only
   TR::RealRegister::RegNum getLongHighReturnRegister() const
      {
      return _returnRegisters[0];
      }

   // for 64-bit use only
   TR::RealRegister::RegNum getLongReturnRegister() const
      {
      return _returnRegisters[0];
      }

   TR::RealRegister::RegNum getFloatReturnRegister() const
      {
      return _returnRegisters[_firstFloatReturnRegister];
      }

   TR::RealRegister::RegNum getDoubleReturnRegister() const
      {
      return _returnRegisters[_firstFloatReturnRegister];
      }

   TR::RealRegister::RegNum getVectorReturnRegister() const
      {
      return _returnRegisters[_firstVectorReturnRegister];
      }

   int32_t getNumAllocatableIntegerRegisters() const
      {
      return _numAllocatableIntegerRegisters;
      }

   int32_t getFirstAllocatableIntegerArgumentRegister() const
      {
      return _firstAllocatableIntegerArgumentRegister;
      }

   int32_t getLastAllocatableIntegerVolatileRegister() const
      {
      return _lastAllocatableIntegerVolatileRegister;
      }

   int32_t getNumAllocatableFloatRegisters() const
      {
      return _numAllocatableFloatRegisters;
      }

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

   TR::RealRegister::RegNum getNormalStackPointerRegister() const
      {
      return _normalStackPointerRegister;
      }

   TR::RealRegister::RegNum getAlternateStackPointerRegister() const
      {
      return _alternateStackPointerRegister;
      }

   TR::RealRegister::RegNum getTOCBaseRegister() const
      {
      return _TOCBaseRegister;
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

   int32_t getOffsetToFirstLocal() const {return _offsetToFirstLocal;}

   uint32_t getNumberOfDependencyGPRegisters() const {return _numberOfDependencyGPRegisters;}
   };

}

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
   virtual TR::Instruction *flushOrLoadUpArguments(TR::Instruction *cursor, bool flush);
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
