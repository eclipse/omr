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

#ifndef OMR_LINKAGE_INCL
#define OMR_LINKAGE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LINKAGE_CONNECTOR
#define OMR_LINKAGE_CONNECTOR
namespace OMR { class Linkage; }
namespace OMR { typedef OMR::Linkage LinkageConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/RegisterConstants.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Annotations.hpp"

class TR_BitVector;
class TR_FrontEnd;
class TR_HeapMemory;
class TR_Memory;
class TR_StackMemory;
namespace TR { class AutomaticSymbol; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Linkage; }
namespace TR { class Machine; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
template <class T> class List;

// linkage properties
// TODO: convert to enum
#define CallerCleanup               0x01
#define RightToLeft                 0x02
#define IntegersInRegisters         0x04
#define LongsInRegisters            0x08
#define FloatsInRegisters           0x10
#define SmallIntParmsAlignedRight   0x20

// register flags
// TODO: convert to enum
#define Preserved                   0x01
#define IntegerReturn               0x02
#define IntegerArgument             0x04
#define FloatReturn                 0x08
#define FloatArgument               0x10
#define CallerAllocatesBackingStore 0x20
#define Reserved                    0x40
#define VectorReturn                0x80
#define VectorArgument             0x100

namespace OMR
{

struct OMR_EXTENSIBLE LinkageProperties
   {
   uint32_t _properties;
   uint32_t _registerFlags[TR::RealRegister::NumRegisters];
   uint8_t _numIntegerArgumentRegisters;
   uint8_t _firstIntegerArgumentRegister;
   uint8_t _numFloatArgumentRegisters;
   uint8_t _firstFloatArgumentRegister;
   TR::RealRegister::RegNum _argumentRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numIntegerReturnRegisters;
   uint8_t _firstIntegerReturnRegister;
   uint8_t _numFloatReturnRegisters;
   uint8_t _firstFloatReturnRegister;
   TR::RealRegister::RegNum _returnRegisters[TR::RealRegister::NumRegisters];
   uint8_t _numAllocatableIntegerRegisters;
   uint8_t _numAllocatableFloatRegisters;
   TR::RealRegister::RegNum _methodMetaDataRegister;
   TR::RealRegister::RegNum _stackPointerRegister;
   TR::RealRegister::RegNum _framePointerRegister;
   TR::RealRegister::RegNum _computedCallTargetRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _vtableIndexArgumentRegister; // for icallVMprJavaSendPatchupVirtual
   TR::RealRegister::RegNum _j9methodArgumentRegister; // for icallVMprJavaSendStatic
   uint8_t _numberOfDependencyGPRegisters;
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

   TR::RealRegister::RegNum getMethodMetaDataRegister() const
      {
      return _methodMetaDataRegister;
      }
   TR::RealRegister::RegNum getVMThreadRegister() const
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

   int32_t getOffsetToFirstLocal() const {return _offsetToFirstLocal;}

   uint32_t getNumberOfDependencyGPRegisters() const {return _numberOfDependencyGPRegisters;}

   /**
    * @brief Initialize derived properties from register flags. This *must* be called
    * after _registerFlags are populated.
    */
   void initialize();
   };


class OMR_EXTENSIBLE Linkage
   {
   public:

   inline TR::Linkage* self();

   /**
    * @return Cached CodeGenerator object
    */
   inline TR::CodeGenerator *cg();

   /**
    * @return Machine object from cached CodeGenerator
    */
   inline TR::Machine *machine();

   /**
    * @return Compilation object from cached CodeGenerator
    */
   inline TR::Compilation *comp();

   /**
    * @return FrontEnd object from cached CodeGenerator
    */
   inline TR_FrontEnd *fe();

   /**
    * @return TR_Memory object from cached CodeGenerator
    */
   inline TR_Memory *trMemory();

   /**
    * @return TR_HeapMemory object
    */
   inline TR_HeapMemory trHeapMemory();

   /**
    * @return TR_StackMemory object
    */
   inline TR_StackMemory trStackMemory();

   TR_ALLOC(TR_Memory::Linkage)

   Linkage(TR::CodeGenerator *cg) : _cg(cg) { }

   virtual void createPrologue(TR::Instruction *cursor) = 0;
   virtual void createEpilogue(TR::Instruction *cursor) = 0;

   virtual uint32_t getRightToLeft() = 0;
   virtual bool     hasToBeOnStack(TR::ParameterSymbol *parm);
   virtual void     mapStack(TR::ResolvedMethodSymbol *method) = 0;
   virtual void     mapSingleAutomatic(TR::AutomaticSymbol *p, uint32_t &stackIndex) = 0;

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method, List<TR::ParameterSymbol> &parm)
      {
      TR_UNIMPLEMENTED();
      }

   virtual void setParameterLinkageRegisterIndex(TR::ResolvedMethodSymbol *method)
      {
      TR_UNIMPLEMENTED();
      }

   virtual int32_t numArgumentRegisters(TR_RegisterKinds kind) = 0;

   virtual TR_RegisterKinds argumentRegisterKind(TR::Node *argumentNode);

   /**
    * @brief Perform operations required by this linkage once the code generator
    *    binary encoding phase has completed.  Linkage implementors should override
    *    this function as necessary.
    */
   virtual void performPostBinaryEncoding() { }

   /** @brief
    *    Gets the offset (in number of bytes) from the stack frame pointer to the location on the stack where the first
    *    (closest to the frame pointer) parameter is located. 
    *
    *  @details
    *    For example given the following stack frame layout, for a stack which grows towards 0x0000 (we subtract the
    *    stack pointer for each additional frame), and assuming a call to a function with 4 parameters:
    *
    *    @code
    *        0x04E8     +--------+   <- stack pointer
    *                   |  ....  |
    *        0x0518     +--------+   <- frame pointer
    *                   |  ....  |
    *        0x0550     +--------+   <- offset to first parm relative to the frame pointer (0x0550 - 0x0518 = 0x0038)
    *                   |  ARG4  |
    *        0x0558     +--------+
    *                   |  ARG3  |
    *        0x0560     +--------+
    *                   |  ARG2  |
    *        0x0568     +--------+
    *                   |  ARG1  |
    *        0x0570     +--------+
    *    @endcode
    *
    *    The offset returned by this function (0x0038 in the above example) may not be the first argument (in argument
    *    order) passed by the caller on the stack. It is up to the linkage to make use of this function to initialize
    *    parameter offsets depending on the order in which the caller passes the arguments to the callee.
    */
   inline int32_t getOffsetToFirstParm() const;

   /** @brief
    *    Sets the offset (in number of bytes) from the stack frame pointer to the location on the stack where the first
    *    (closest to the frame pointer) parameter is located. 
    */
   inline int32_t setOffsetToFirstParm(int32_t offset);

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
    *    If this API returns 0 then `entryPointFromInterpetedMethod` must also return 0.
    *
    * @return The entry point for compiled methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromCompiledMethod() { TR_UNIMPLEMENTED(); return 0; }

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
    *    If this API returns 0 then `entryPointFromCompiledMethod` must also return 0.
    *
    * @return The entry point for interpreted methods to use; 0 if the entry point is unknown
    */
   virtual intptr_t entryPointFromInterpretedMethod() { TR_UNIMPLEMENTED(); return 0; }

protected:

   TR::CodeGenerator *_cg;

   int32_t _offsetToFirstParm;
   };
}

#endif
