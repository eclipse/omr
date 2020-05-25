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

#ifndef OMR_X86_LINKAGE_INCL
#define OMR_X86_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { namespace X86 { class Linkage; } }
namespace OMR { typedef OMR::X86::Linkage LinkageConnector; }
#endif

#include "compiler/codegen/OMRLinkage.hpp"

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "infra/Annotations.hpp"
#include "infra/Assert.hpp"
#include "x/codegen/X86Ops.hpp"

// linkage properties
#define CallerCleanup                              0x0001
#define RightToLeft                                0x0002
#define IntegersInRegisters                        0x0004
#define LongsInRegisters                           0x0008
#define FloatsInRegisters                          0x0010
#define EightBytePointers                          0x0020
#define EightByteParmSlots                         0x0040
#define LinkageRegistersAssignedByCardinalPosition 0x0080
#define CallerFrameAllocatesSpaceForLinkageRegs    0x0100
#define AlwaysDedicateFramePointerRegister         0x0200
#define NeedsThunksForIndirectCalls                0x0400
#define UsesPushesForPreservedRegs                 0x0800
#define ReservesOutgoingArgsInPrologue             0x1000
#define UsesRegsForHelperArgs                      0x2000

// register flags
#define Preserved                   0x01
#define IntegerReturn               0x02
#define IntegerArgument             0x04
#define FloatReturn                 0x08
#define FloatArgument               0x10
#define CallerAllocatesBackingStore 0x20
#define CalleeVolatile              0x40

class TR_FrontEnd;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class MethodSymbol; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

enum { NOT_LINKAGE = -1 };

typedef enum { Int4, Int8, Float4, Float8, NumMovDataTypes } TR_MovDataTypes;

typedef enum { MemReg, RegMem, RegReg, NumMovOperandTypes }  TR_MovOperandTypes;

namespace TR {

struct MovStatus // What kind of RegReg movs must be applied to a given register
   {
   // NOTE: sourceReg is not to be moved to targetReg.  Rather, we're building
   // a kind of doubly-linked list, with a TR::MovStatus for each register R such
   // that sourceReg will be moved to R and R will be moved to targetReg.
   //
   TR::RealRegister::RegNum sourceReg;        // Which reg will be copied over the given reg
   TR::RealRegister::RegNum targetReg;        // Which reg will receive the given reg's value
   TR_MovDataTypes          outgoingDataType; // Type of data originally in the given reg
   };

struct X86LinkageProperties
   {

   // Limits applicable to all linkages.  They must cover both integer and float registers.
   //
   enum
      {
      MaxArgumentRegisters = 30,
      MaxReturnRegisters   = 3,
      MaxVolatileRegisters = 30,
      MaxScratchRegisters  = 5
      };

   uint32_t                  _properties;
   uint32_t                  _registerFlags[TR::RealRegister::NumRegisters];

   TR::RealRegister::RegNum _preservedRegisters[TR::RealRegister::NumRegisters];
   TR::RealRegister::RegNum _argumentRegisters[MaxArgumentRegisters];
   TR::RealRegister::RegNum _returnRegisters[MaxReturnRegisters];
   TR::RealRegister::RegNum _volatileRegisters[MaxVolatileRegisters];
   TR::RealRegister::RegNum _scratchRegisters[MaxScratchRegisters];
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister;    // for icallVMprJavaSendStatic

   uint32_t                 _preservedRegisterMapForGC;
   TR::RealRegister::RegNum _framePointerRegister;
   TR::RealRegister::RegNum _methodMetaDataRegister;
   int8_t                   _offsetToFirstParm;
   int8_t                   _offsetToFirstLocal;           // Points immediately after the local with highest address

   uint8_t                 _numScratchRegisters;

   uint8_t                 _numberOfVolatileGPRegisters;
   uint8_t                 _numberOfVolatileXMMRegisters;
   uint8_t                 _numVolatileRegisters;

   uint8_t                 _numberOfPreservedGPRegisters;
   uint8_t                 _numberOfPreservedXMMRegisters;
   uint8_t                 _numPreservedRegisters;

   uint8_t                 _maxRegistersPreservedInPrologue;

   uint8_t                 _numIntegerArgumentRegisters;
   uint8_t                 _numFloatArgumentRegisters;

   uint8_t                 _firstIntegerArgumentRegister;
   uint8_t                 _firstFloatArgumentRegister;

   uint32_t             _allocationOrder[TR::RealRegister::NumRegisters];

   uint32_t _OutgoingArgAlignment;

   uint32_t getProperties() const {return _properties;}

   uint32_t getCallerCleanup() const {return (_properties & CallerCleanup);}
   uint32_t passArgsRightToLeft() const {return (_properties & RightToLeft);}
   uint32_t getIntegersInRegisters() const {return (_properties & IntegersInRegisters);}
   uint32_t getLongsInRegisters() const {return (_properties & LongsInRegisters);}
   uint32_t getFloatsInRegisters() const {return (_properties & FloatsInRegisters);}
   uint32_t getAlwaysDedicateFramePointerRegister() const {return (_properties & AlwaysDedicateFramePointerRegister);}
   uint32_t getNeedsThunksForIndirectCalls() const {return (_properties & NeedsThunksForIndirectCalls);}
   uint32_t getUsesPushesForPreservedRegs() const {return (_properties & UsesPushesForPreservedRegs);}
   uint32_t getReservesOutgoingArgsInPrologue() const {return (_properties & ReservesOutgoingArgsInPrologue);}
   uint32_t getUsesRegsForHelperArgs() const {return (_properties & UsesRegsForHelperArgs);}

   uint8_t  getPointerSize() const {return (_properties & EightBytePointers)? 8 : 4;}
   uint8_t  getPointerShift() const {return (_properties & EightBytePointers)? 3 : 2;}

   uint8_t  getParmSlotSize() const {return (_properties & EightByteParmSlots)? 8 : 4;}
   uint8_t  getParmSlotShift() const {return (_properties & EightByteParmSlots)? 3 : 2;}

   uint8_t  getGPRWidth() const { return getPointerSize();}
   uint8_t  getRetAddressWidth() const {return getGPRWidth(); }

   uint32_t getLinkageRegistersAssignedByCardinalPosition() const
      {
      return (_properties & LinkageRegistersAssignedByCardinalPosition);
      }
   uint32_t getCallerFrameAllocatesSpaceForLinkageRegisters() const
      {
      return (_properties & CallerFrameAllocatesSpaceForLinkageRegs);
      }

   uint32_t getRegisterFlags(TR::RealRegister::RegNum regNum)          const {return _registerFlags[regNum];}
   uint32_t isPreservedRegister(TR::RealRegister::RegNum regNum)       const {return (_registerFlags[regNum] & Preserved);}
   uint32_t isCalleeVolatileRegister(TR::RealRegister::RegNum regNum)  const {return (_registerFlags[regNum] & CalleeVolatile);}
   uint32_t isIntegerReturnRegister(TR::RealRegister::RegNum regNum)   const {return (_registerFlags[regNum] & IntegerReturn);}
   uint32_t isIntegerArgumentRegister(TR::RealRegister::RegNum regNum) const {return (_registerFlags[regNum] & IntegerArgument);}
   uint32_t isFloatReturnRegister(TR::RealRegister::RegNum regNum)     const {return (_registerFlags[regNum] & FloatReturn);}
   uint32_t isFloatArgumentRegister(TR::RealRegister::RegNum regNum)   const {return (_registerFlags[regNum] & FloatArgument);}
   uint32_t doesCallerAllocatesBackingStore(TR::RealRegister::RegNum regNum) const {return (_registerFlags[regNum] & CallerAllocatesBackingStore);}

   uint32_t getKilledAndNonReturn(TR::RealRegister::RegNum regNum) const {return ((_registerFlags[regNum] & (Preserved | IntegerReturn | FloatReturn)) == 0);}
   uint32_t getPreservedRegisterMapForGC() const {return _preservedRegisterMapForGC;}

   TR::RealRegister::RegNum getPreservedRegister(uint32_t index) const {return _preservedRegisters[index];}

   TR::RealRegister::RegNum getArgument(uint32_t index) const {return _argumentRegisters[index];}

   TR::RealRegister::RegNum getIntegerReturnRegister()  const {return _returnRegisters[0];}
   TR::RealRegister::RegNum getLongLowReturnRegister()  const {return _returnRegisters[0];}
   TR::RealRegister::RegNum getLongHighReturnRegister() const {return _returnRegisters[2];}
   TR::RealRegister::RegNum getFloatReturnRegister()    const {return _returnRegisters[1];}
   TR::RealRegister::RegNum getDoubleReturnRegister()   const {return _returnRegisters[1];}
   TR::RealRegister::RegNum getFramePointerRegister()   const {return _framePointerRegister;}
   TR::RealRegister::RegNum getMethodMetaDataRegister() const {return _methodMetaDataRegister;}

   int32_t getOffsetToFirstParm() const {return _offsetToFirstParm;}

   int32_t getOffsetToFirstLocal() const {return _offsetToFirstLocal;}

   uint8_t getNumIntegerArgumentRegisters() const {return _numIntegerArgumentRegisters;}
   uint8_t getNumFloatArgumentRegisters()   const {return _numFloatArgumentRegisters;}
   uint8_t getNumPreservedRegisters()       const {return _numPreservedRegisters;}
   uint8_t getNumVolatileRegisters()        const {return _numVolatileRegisters;}
   uint8_t getNumScratchRegisters()         const {return _numScratchRegisters;}

   uint32_t *getRegisterAllocationOrder() const {return (uint32_t *)_allocationOrder;}

   uint32_t getOutgoingArgAlignment() const   {return _OutgoingArgAlignment;}
   uint32_t setOutgoingArgAlignment(uint32_t s)   {return (_OutgoingArgAlignment = s);}

   TR::RealRegister::RegNum getIntegerArgumentRegister(uint8_t index) const
      {
      TR_ASSERT(index < getNumIntegerArgumentRegisters(), "assertion failure");
      return _argumentRegisters[_firstIntegerArgumentRegister + index];
      }

   TR::RealRegister::RegNum getFloatArgumentRegister(uint8_t index) const
      {
      TR_ASSERT(index < getNumFloatArgumentRegisters(), "assertion failure");
      return _argumentRegisters[_firstFloatArgumentRegister + index];
      }

   TR::RealRegister::RegNum getArgumentRegister(uint8_t index, bool isFloat) const
      {
      return isFloat? getFloatArgumentRegister(index) : getIntegerArgumentRegister(index);
      }

   TR::RealRegister::RegNum getIntegerScratchRegister(uint8_t index) const
      {
      TR_ASSERT(index < getNumScratchRegisters(), "assertion failure");
      return _scratchRegisters[index];
      }

   TR::RealRegister::RegNum getVTableIndexArgumentRegister() const
      {
      return _vtableIndexArgumentRegister;
      }

   TR::RealRegister::RegNum getJ9MethodArgumentRegister() const
      {
      return _j9methodArgumentRegister;
      }

   uint8_t getMaxRegistersPreservedInPrologue() const { return _maxRegistersPreservedInPrologue; }

   uint32_t getNumberOfVolatileGPRegisters()  const {return _numberOfVolatileGPRegisters;}
   uint32_t getNumberOfVolatileXMMRegisters() const {return _numberOfVolatileXMMRegisters;}

   uint32_t getNumberOfPreservedGPRegisters()  const {return _numberOfPreservedGPRegisters;}
   uint32_t getNumberOfPreservedXMMRegisters() const {return _numberOfPreservedXMMRegisters;}

   };

}

namespace OMR
{

namespace X86
{

class OMR_EXTENSIBLE Linkage : public OMR::Linkage
   {
   public:

   virtual bool needsFrameDeallocation() { return false; }
   virtual TR::Instruction *deallocateFrameIfNeeded(TR::Instruction *cursor, int32_t size) {return NULL;}

   virtual uint32_t getRightToLeft();

   virtual void mapStack(TR::ResolvedMethodSymbol *method);
   void mapCompactedStack(TR::ResolvedMethodSymbol *method);

   virtual void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex);
   void mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t size, uint32_t &stackIndex);

   virtual TR::Instruction *copyStackParametersToLinkageRegisters(TR::Instruction *procEntryInstruction)
      {
      // Unless overridden, assume jitted code takes parameters on stack, so
      // interpreter entry point needs no special code; thus, it's the same as
      // the jitted->jitted entry point.
      //
      return procEntryInstruction;
      }

   // Generates code to copy arguments to/from the outgoing argument area
   // from/to registers prior to a method call.
   // (load = stack->reg, store = reg->stack)
   //
   uint8_t *loadArguments(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea, bool isReturnAddressOnStack);

   uint8_t *storeArguments(TR::Node *callNode, uint8_t *cursor, bool calculateSizeOnly, int32_t *sizeOfFlushArea);

   TR::Instruction *loadArguments(TR::Instruction *prev, TR::ResolvedMethodSymbol *methodSymbol);

   TR::Instruction *storeArguments(TR::Instruction *prev, TR::ResolvedMethodSymbol *methodSymbol);

   virtual void copyLinkageInfoToParameterSymbols()
      {
      // Unless overridden, just leave all parameter linkage registers at the
      // default of -1.
      }

   virtual void copyGlRegDepsToParameterSymbols(TR::Node *bbStart, TR::CodeGenerator *cg)
      {
      // Unless overridden, just leave all parameter assigned registers at the
      // default of -1.
      }

   virtual const ::TR::X86LinkageProperties& getProperties() = 0;

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind);

   virtual void createPrologue(TR::Instruction *cursor) = 0;

   virtual void createEpilogue(TR::Instruction *cursor) = 0;

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;

   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs) = 0;

   // Something (eg. recompilation or preexistence) may need the first
   // instruction of a method to be a certain size so it can be safely
   // overwritten without leaving another thread executing in the middle of the
   // new instruction.
   //
   uint8_t getMinimumFirstInstructionSize() { return _minimumFirstInstructionSize; }
   void ensureMinimumFirstInstructionSize(uint8_t mifs){ _minimumFirstInstructionSize = std::max(_minimumFirstInstructionSize, mifs); }

   bool isFloat(TR_MovDataTypes mdt)
      {
      return (mdt == Float4 || mdt == Float8);
      }

   TR_RegisterKinds movRegisterKind(TR_MovDataTypes mdt);

   TR_MovDataTypes movType(TR::DataType type)
      {
      switch(type)
         {
         case TR::Double:
            return Float8;
         case TR::Float:
            return Float4;
         case TR::Int64:
            return Int8;
         case TR::Address:
            return OMR::X86::Linkage::getTargetFromComp().is64Bit() ? Int8 : Int4;
         default:
            return Int4;
         }
      }

   TR_MovDataTypes paramMovType(TR::ParameterSymbol *param);

   TR_MovDataTypes fullRegisterMovType(TR::Register *reg)
      {
      switch(reg->getKind())
         {
         case TR_GPR:
            return OMR::X86::Linkage::getTargetFromComp().is64Bit() ? Int8 : Int4;
         case TR_FPR:
            return Float8;
         default:
            TR_ASSERT(0, "unsupported register kind");
            return Int8;
         }
      }

   static inline TR_X86OpCodes movOpcodes(TR_MovOperandTypes operandType, TR_MovDataTypes dataType)
      {
      TR_ASSERT(OMR::X86::Linkage::getTargetFromComp().is64Bit() || dataType != Int8, "MOV Int8 should not occur on X86-32");
      return _movOpcodes[operandType][dataType];
      }

   void alignLocalObjectWithCollectedFields(uint32_t & stackIndex) {}
   void alignLocalObjectWithoutCollectedFields(uint32_t & stackIndex) {}

   protected:

   Linkage(TR::CodeGenerator *cg);

   void stopUsingKilledRegisters(TR::RegisterDependencyConditions  *deps, TR::Register *returnRegister);
   void associatePreservedRegisters(TR::RegisterDependencyConditions  *deps, TR::Register *returnRegister);
   void coerceFPReturnValueToXMMR(TR::Node *, TR::RegisterDependencyConditions  *, TR::MethodSymbol *, TR::Register *);
   TR::Register *findReturnRegisterFromDependencies(TR::Node *, TR::RegisterDependencyConditions  *, TR::MethodSymbol *, bool);

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method);

   virtual uint8_t *flushArguments(TR::Node  *callNode,
                                   uint8_t  *cursor,
                                   bool      calculateSizeOnly,
                                   int32_t  *sizeOfFlushArea,
                                   bool      isReturnAddressOnStack,
                                   bool      isLoad)
      {
      // Unless overridden, assume that all parameters are already in the right
      // place, so no extra code needs to be generated to store them.
      return cursor;
      }

   virtual TR::Instruction *flushArguments(TR::Instruction          *prev,
                                          TR::ResolvedMethodSymbol *methodSymbol,
                                          bool                     isReturnAddressOnStack,
                                          bool                     isLoad)
      {
      // Unless overridden, assume that all parameters are already in the right
      // place, so no extra code needs to be generated to store them.
      return prev;
      }

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method) { }

   private:

   static TR::Environment& getTargetFromComp();

   static TR_X86OpCodes _movOpcodes[NumMovOperandTypes][NumMovDataTypes];
   uint8_t              _minimumFirstInstructionSize;

   };
}
}

#endif
