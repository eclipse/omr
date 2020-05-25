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

#ifndef X86SYSTEMLINKAGE_INCL
#define X86SYSTEMLINKAGE_INCL

#include <stdint.h>
#include "codegen/Linkage.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "il/DataTypes.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

typedef struct parmLayoutResult
   {
   enum statusEnum
      {
      IN_LINKAGE_REG = 0x1,
      IN_LINKAGE_REG_PAIR = 0x2, // Only on AMD64 SysV ABI: a struct might take 2 linkage registers.
      ON_STACK = 0x4,
      };
   uint8_t abstract;
   uint32_t offset;
   struct
      {
      TR_RegisterKinds regKind;
      uint16_t regIndex;
      } regs[2];
   parmLayoutResult()
      {
      abstract = (statusEnum)0;
      offset =0;
      regs[0].regKind = regs[1].regKind = (TR_RegisterKinds)-1;
      regs[0].regIndex = regs[1].regIndex = (uint16_t)-1;
      }
   } parmLayoutResult;

class X86SystemLinkage : public TR::Linkage
   {
   protected:
   X86SystemLinkage(TR::CodeGenerator *cg);

   TR::X86LinkageProperties _properties;

   TR::Instruction* copyParametersToHomeLocation(TR::Instruction *cursor);

   TR::Instruction* savePreservedRegisters(TR::Instruction *cursor);
   TR::Instruction* restorePreservedRegisters(TR::Instruction *cursor);

   virtual void createPrologue(TR::Instruction *cursor);
   virtual void createEpilogue(TR::Instruction *cursor);

   int32_t computeMemoryArgSize(TR::Node *callNode, int32_t first, int32_t last, int8_t direction);
   int32_t getParameterStartingPos(int32_t &dataCursor, uint32_t align);
   int32_t layoutTypeOnStack(TR::DataType, int32_t&, TR::parmLayoutResult&);
   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps) = 0;
   virtual uint32_t getAlignment(TR::DataType) = 0;
   virtual int32_t layoutParm(TR::Node *parmNode, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult &layoutResult) = 0;
   virtual int32_t layoutParm(TR::ParameterSymbol *paramSymbol, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatRrgs, TR::parmLayoutResult&) = 0;

   virtual TR::Register* buildVolatileAndReturnDependencies(TR::Node*, TR::RegisterDependencyConditions*) = 0;

   /**
    * @brief Returns a register appropriate for allocating/de-allocating small stack frames
    *
    * When the size of a new stack frame that is the same size as a word on the
    * platform, the frame can be simply allocated/de-allocated by pushing/popping
    * a volatile, non-return register. This function returns a register that
    * is guarenteed to be safe for such uses.
    */
   virtual TR::RealRegister* getSingleWordFrameAllocationRegister() = 0;

   public:

   const TR::X86LinkageProperties& getProperties();

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode) = 0;
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs) = 0;

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method);
   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method, uint32_t &stackIndex) { mapIncomingParms(method); };


   virtual void copyLinkageInfoToParameterSymbols();
   virtual void copyGlRegDepsToParameterSymbols(TR::Node *bbStart, TR::CodeGenerator *cg);

   virtual void setUpStackSizeForCallNode(TR::Node *node) = 0;

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

inline TR::X86SystemLinkage *toX86SystemLinkage(TR::Linkage *l) {return (TR::X86SystemLinkage *)l;}

}

#endif
