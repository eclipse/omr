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

#ifndef S390LINKAGE_INCL
#define S390LINKAGE_INCL

#include "codegen/TRzOSSystemLinkageBase.hpp"

#include <stdint.h>
#include "codegen/Linkage.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/SystemLinkage.hpp"
#include "cs2/arrayof.h"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"

class TR_EntryPoint;
namespace TR { class S390ConstantDataSnippet; }
namespace TR { class S390JNICallDataSnippet; }
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
template <class T> class List;

////////////////////////////////////////////////////////////////////////////////
//  TR::S390zOSSystemLinkage Definition
////////////////////////////////////////////////////////////////////////////////

enum TR_XPLinkFrameType
   {
   TR_XPLinkUnknownFrame,
   TR_XPLinkStackLeafFrame,
   TR_XPLinkNoStackLeafFrame,
   TR_XPLinkSmallFrame,
   TR_XPLinkIntermediateFrame,
   TR_XPLinkStackCheckFrame
   };

/**
 * XPLink call types whose value is encoded in NOP following call.
 * These values are encoded - so don't change
 */
enum TR_XPLinkCallTypes {
   TR_XPLinkCallType_BASR      =0,     ///< generic BASR
   TR_XPLinkCallType_BRAS7     =1,     ///< BRAS  r7,ep
   TR_XPLinkCallType_RESVD_2   =2,     //
   TR_XPLinkCallType_BRASL7    =3,     ///< BRASL r7,ep
   TR_XPLinkCallType_RESVD_4   =4,     //
   TR_XPLinkCallType_RESVD_5   =5,     //
   TR_XPLinkCallType_BALR1415  =6,     ///< BALR  r14,r15
   TR_XPLinkCallType_BASR33    =7      ///< BASR  r3,r3
   };

namespace TR {

class S390zOSSystemLinkage : public TR::ZOSBaseSystemLinkageConnector
   {
   TR::RealRegister::RegNum _environmentPointerRegister;

public:

   S390zOSSystemLinkage(TR::CodeGenerator * cg);

   virtual void generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
         intptrj_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
         TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint = true);

   virtual TR::Register * callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
      intptrj_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel, TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint = true);

   virtual TR::RealRegister::RegNum setEnvironmentPointerRegister (TR::RealRegister::RegNum r) { return _environmentPointerRegister = r; }
   virtual TR::RealRegister::RegNum getEnvironmentPointerRegister() { return _environmentPointerRegister; }
   virtual TR::RealRegister *getEnvironmentPointerRealRegister() {return getRealRegister(_environmentPointerRegister);}

   virtual int32_t getRegisterSaveOffset(TR::RealRegister::RegNum);

public:
   TR_XPLinkFrameType getFrameType() { return _frameType; }
   void setFrameType(enum TR_XPLinkFrameType type) { _frameType = type; }
   virtual bool getIsLeafRoutine() { return (getFrameType() == TR_XPLinkStackLeafFrame) || (getFrameType() == TR_XPLinkNoStackLeafFrame); }
   virtual int32_t getOutgoingParameterBlockSize();

   virtual TR::Register *loadEnvironmentIntoRegister(TR::Node * callNode, TR::RegisterDependencyConditions * deps, bool loadIntoEnvironmentReg);
   bool isAggregateReturnedInIntRegisters(int32_t aggregateLenth);

   uint32_t calculateInterfaceMappingFlags(TR::ResolvedMethodSymbol *method);

   static uint32_t shiftFloatParmDescriptorFlag(uint32_t fieldVal, int32_t floatParmNum)  { return (fieldVal) << (6*(3-floatParmNum)); } // accepts floatParmNum values 0,1,2,3
   bool updateFloatParmDescriptorFlags(uint32_t *parmDescriptorFields, TR::Symbol *funcSymbol, int32_t parmCount, int32_t argSize, TR::DataType dataType, int32_t *floatParmNum, uint32_t *lastFloatParmAreaOffset, uint32_t *parmAreaOffset);

   static uint32_t getFloatParmDescriptorFlag(uint32_t descriptorFields, int32_t floatParmNum)  { return  (descriptorFields >> (6*(3-floatParmNum))) & 0x3F; }
   uint32_t calculateReturnValueAdjustFlag(TR::DataType dataType, int32_t aggregateLength);
   static uint32_t isFloatDescriptorFlagUnprototyped(uint32_t flag)  { return flag == 0; }

   uint32_t calculateCallDescriptorFlags(TR::Node *callNode);

private:
   enum TR_XPLinkFrameType _frameType;

   };


////////////////////////////////////////////////////////////////////////////////
//  TR::S390zLinuxSystemLinkage Definition
////////////////////////////////////////////////////////////////////////////////
class S390zLinuxSystemLinkage : public TR::SystemLinkage
   {
   TR::RealRegister::RegNum _GOTPointerRegister;
public:

   S390zLinuxSystemLinkage(TR::CodeGenerator * cg);

   virtual void generateInstructionsForCall(TR::Node * callNode, TR::RegisterDependencyConditions * deps, intptrj_t targetAddress,
         TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel,
         TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint);
   virtual TR::Register * callNativeFunction(TR::Node * callNode, TR::RegisterDependencyConditions * dependencies,
      intptrj_t targetAddress, TR::Register * methodAddressReg, TR::Register * javaLitOffsetReg, TR::LabelSymbol * returnFromJNICallLabel, TR::S390JNICallDataSnippet * jniCallDataSnippet, bool isJNIGCPoint = true);

   virtual void setGOTPointerRegister (TR::RealRegister::RegNum r)         { _GOTPointerRegister = r; }
   virtual TR::RealRegister::RegNum getGOTPointerRegister()         { return _GOTPointerRegister; }
   virtual TR::RealRegister *getGOTPointerRealRegister() {return getRealRegister(_GOTPointerRegister);}
   virtual int32_t getRegisterSaveOffset(TR::RealRegister::RegNum);
   virtual void initParamOffset(TR::ResolvedMethodSymbol * method, int32_t stackIndex, List<TR::ParameterSymbol> *parameterList=0);

   virtual FrameType checkLeafRoutine(int32_t stackFrameSize, TR::Instruction **callInstruction = 0);

   virtual bool canDataTypeBePassedByReference(TR::DataType type);
   virtual bool isSymbolPassedByReference(TR::Symbol *sym);
   };

}

#endif
