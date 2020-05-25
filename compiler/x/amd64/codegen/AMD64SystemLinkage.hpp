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

#ifndef AMD64SYSTEMLINKAGE_INCL
#define AMD64SYSTEMLINKAGE_INCL

#include "x/codegen/X86SystemLinkage.hpp"

#include <stdint.h>
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "il/DataTypes.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class ResolvedMethodSymbol; }

namespace TR {

class AMD64SystemLinkage : public TR::X86SystemLinkage
   {
   public:


   virtual void setUpStackSizeForCallNode(TR::Node* node);

   protected:
   AMD64SystemLinkage(TR::CodeGenerator *cg) : TR::X86SystemLinkage(cg) {}

   virtual int32_t layoutParm(TR::Node *parmNode, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult &layoutResult);
   virtual int32_t layoutParm(TR::ParameterSymbol *paramSymbol, int32_t &dataCursor, uint16_t &intReg, uint16_t &floatRrgs, TR::parmLayoutResult&);
   virtual uint32_t getAlignment(TR::DataType type);

   virtual int32_t buildArgs(TR::Node *callNode, TR::RegisterDependencyConditions *deps);

   virtual TR::Register *buildIndirectDispatch(TR::Node *callNode);
   virtual TR::Register *buildDirectDispatch(TR::Node *callNode, bool spillFPRegs);

   TR::Register *buildVolatileAndReturnDependencies(TR::Node *callNode, TR::RegisterDependencyConditions *deps);

   virtual TR::RealRegister* getSingleWordFrameAllocationRegister() { return machine()->getRealRegister(TR::RealRegister::r11); }

   private:
   bool layoutTypeInRegs(TR::DataType type, uint16_t &intReg, uint16_t &floatReg, TR::parmLayoutResult&);

   };


class AMD64Win64FastCallLinkage : public virtual TR::AMD64SystemLinkage
   {
   // Register                  Status        Use
   // --------                  ------        ---
   // RAX                       Volatile      Return value register.
   // RCX                       Volatile      First integer argument.
   // RDX                       Volatile      Second integer argument.
   // R8                        Volatile      Third integer argument.
   // R9                        Volatile      Fourth integer argument.
   // R10:R11                   Volatile      Must be preserved as required by caller; used in syscall/sysret instructions.
   // R12:R15                   Nonvolatile   Must be preserved by called function.
   // RDI                       Nonvolatile   Must be preserved by called function.
   // RSI                       Nonvolatile   Must be preserved by called function.
   // RBX                       Nonvolatile   Must be preserved by called function.
   // RBP                       Nonvolatile   Can be used as a frame pointer. Must be preserved by called function.
   // RSP                       Nonvolatile   Stack Pointer.
   // XMM0                      Volatile      First FP argument.
   // XMM1                      Volatile      Second FP argument.
   // XMM2                      Volatile      Third FP argument.
   // XMM3                      Volatile      Fourth FP argument.
   // XMM4:XMM5                 Volatile      Must be preserved as required by caller.
   // XMM6:XMM15 (low 64 bits)  Nonvolatile   Must be preserved by called function.
   // XMM6:XMM15 (high 64 bits) Volatile      Must be preserved as required by caller.
   //
   // Arguments evaluation order: left to right
   // Argument cleanup done by  : caller
   //
   // In the case of the nonvolatile XMM registers, only the lower 64 bits must be preserved by the called function.
   // The 128-bit values that are stored in the XMM register should not be registered across calls.
   //
   // Stack must be 16-byte aligned.
   //

   public:

   AMD64Win64FastCallLinkage(TR::CodeGenerator *cg);
   };


class AMD64ABILinkage : public virtual TR::AMD64SystemLinkage
   {
   // Register                  Status        Use
   // --------                  ------        ---
   // RAX                       Volatile      Temporary register; with variable arguments passes information about the
   //                                         number of SSE registers used; 1st return register.
   // RBX                       Nonvolatile   Callee-saved register; optionally used as base pointer
   // RCX                       Volatile      Used to pass 4th integer argument to functions
   // RDX                       Volatile      Used to pass 3rd argument to functions; 2nd return register
   // RSP                       Nonvolatile   Stack pointer
   // RBP                       Nonvolatile   Callee-saved register; optionally used as frame pointer
   // RSI                       Volatile      Used to pass 2nd argument to functions
   // RDI                       Volatile      Used to pass 1st argument to functions
   // R8                        Volatile      Used to pass 5th argument to functions
   // R9                        Volatile      Used to pass 6th argument to functions
   // R10                       Volatile      Temporary register, used for passing a function's static chain pointer
   // R11                       Volatile      Temporary register
   // R12:R15                   Nonvolatile   Callee-saved registers
   // XMM0:XMM1                 Volatile      Used to pass and return floating point arguments.
   // XMM2:XMM7                 Volatile      Used to pass floating point arguments
   // XMM8:XMM15                Volatile      Temporary registers
   //
   // Arguments evaluation order: left to right
   // Argument cleanup done by  : caller
   //
   // If there is no register available anymore for any eightbyte of an argument, the
   // whole argument is passed on the stack. If registers have already been assigned for
   // some eightbytes of this argument, those assignments get reverted.
   //
   // Left-to-right : %rdi, %rsi, %rdx, %rcx, %r8 and %r9
   // Left-to-right : %xmm0 to %xmm7
   //
   // Once registers are assigned, the arguments passed in memory are pushed on
   // the stack in reversed (right-to-left) order.
   //

   public:

   AMD64ABILinkage(TR::CodeGenerator *cg);

   virtual void mapIncomingParms(TR::ResolvedMethodSymbol *method, uint32_t &stackIndex);

   };

}

#endif
