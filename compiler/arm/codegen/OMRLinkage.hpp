/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef OMR_ARM_LINKAGE_INCL
#define OMR_ARM_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR

namespace OMR { namespace ARM { class Linkage; } }
namespace OMR { typedef OMR::ARM::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"
#include "codegen/ARMOps.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RealRegister.hpp"
#include "infra/Annotations.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }

namespace TR {

class ARMMemoryArgument
   {
   public:
   TR_ALLOC(TR_Memory::ARMMemoryArgument)

   TR::Register           *argRegister;
   TR::MemoryReference *argMemory;
   TR_ARMOpCodes          opCode;

//   void *operator new[](size_t s, int flag) {return jitStackAlloc(s);}
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

struct ARMLinkageProperties
   {
   uint32_t                                _properties;
   uint32_t                                _registerFlags[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegNum _preservedRegisters[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegNum _argumentRegisters[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegNum _returnRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numAllocatableIntegerRegisters;
   uint8_t _numAllocatableFloatRegisters;
   uint32_t                                _preservedRegisterMapForGC;
   TR::RealRegister::RegNum _framePointerRegister;
   TR::RealRegister::RegNum _methodMetaDataRegister;
   TR::RealRegister::RegNum _stackPointerRegister;
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister;    // for icallVMprJavaSendStatic

   uint8_t                                 _numberOfDependencyGPRegisters;
   int8_t                                  _offsetToFirstParm;
   int8_t                                  _offsetToFirstLocal;

   uint8_t                                 _numIntegerArgumentRegisters;
   uint8_t                                 _firstIntegerArgumentRegister;
   uint8_t                                 _numFloatArgumentRegisters;
   uint8_t                                 _firstFloatArgumentRegister;
   uint8_t                                 _firstIntegerReturnRegister;
   uint8_t                                 _firstFloatReturnRegister;

   static bool                             _isBigEndian;

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

   // get the indexth float argument register
   TR::RealRegister::RegNum getFloatReturnRegister(uint32_t index) const
      {
      return _returnRegisters[_firstFloatReturnRegister+index];
      }

   TR::RealRegister::RegNum getPreserved(uint32_t index) const
      {
      return _preservedRegisters[index];
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

   TR::RealRegister::RegNum getLongLowReturnRegister() const
      {
      return _returnRegisters[_isBigEndian ? 1 : 0];
      }

   TR::RealRegister::RegNum getLongHighReturnRegister() const
      {
      return _returnRegisters[_isBigEndian ? 0 : 1];
      }

   TR::RealRegister::RegNum getFloatReturnRegister() const
      {
      return _returnRegisters[2];
      }

   TR::RealRegister::RegNum getDoubleReturnRegister() const
      {
      return _returnRegisters[2];
      }

   int32_t getNumAllocatableIntegerRegisters() const
      {
      return _numAllocatableIntegerRegisters;
      }


   int32_t getNumAllocatableFloatRegisters() const
      {
      return _numAllocatableFloatRegisters;
      }

   uint32_t getPreservedRegisterMapForGC() const
      {
      return _preservedRegisterMapForGC;
      }

   TR::RealRegister::RegNum getFramePointerRegister() const
      {
      return _framePointerRegister;
      }

   TR::RealRegister::RegNum getMethodMetaDataRegister() const
      {
      return _methodMetaDataRegister;
      }

   TR::RealRegister::RegNum getStackPointerRegister() const
      {
      return _stackPointerRegister;
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

   void setEndianness(bool isBigEndian) {_isBigEndian = isBigEndian;}

   };

}

namespace OMR
{
namespace ARM
{
class OMR_EXTENSIBLE Linkage : public OMR::Linkage
   {
   public:

   Linkage (TR::CodeGenerator *cg) : OMR::Linkage(cg) {}

   virtual bool hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   virtual void initARMRealRegisterLinkage();

   virtual TR::MemoryReference *getOutgoingArgumentMemRef(int32_t               totalParmAreaSize,
                                                            int32_t               argOffset,
                                                            TR::Register          *argReg,
                                                            TR_ARMOpCodes         opCode,
                                                            TR::ARMMemoryArgument &memArg) = 0;
   virtual TR::Instruction *saveArguments(TR::Instruction *cursor);
   virtual TR::Instruction *flushArguments(TR::Instruction *cursor);
   virtual TR::Instruction *loadUpArguments(TR::Instruction *cursor);
   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method);

   TR::Register *pushJNIReferenceArg(TR::Node *child);
   TR::Register *pushIntegerWordArg(TR::Node *child);
   TR::Register *pushAddressArg(TR::Node *child);
   TR::Register *pushLongArg(TR::Node *child);
   TR::Register *pushFloatArg(TR::Node *child);
   TR::Register *pushDoubleArg(TR::Node *child);

   virtual TR::ARMLinkageProperties& getProperties() = 0;

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind);

   virtual int32_t buildArgs(TR::Node                            *callNode,
                             TR::RegisterDependencyConditions *dependencies,
                             TR::Register*                       &vftReg,
                             bool                                virtualCall) = 0;

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode) = 0;
   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;

   protected:

   TR::Register *buildARMLinkageDirectDispatch(TR::Node *callNode, bool isSystem = false);

   // The semantics of the fourth parameter depends on the linkage
   // conventions chosen (third parameter). System and helper linkage
   // calls are never virtual; private and helper linkage calls are
   // never JNI.
   int32_t buildARMLinkageArgs(TR::Node                            *callNode,
                               TR::RegisterDependencyConditions *dependencies,
                               TR::Register*                       &vftReg,
                               TR_LinkageConventions               conventions,
                               bool                                isVirtualOrJNI);
   };
}
}

#endif
