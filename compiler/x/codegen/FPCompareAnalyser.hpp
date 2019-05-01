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

#ifndef IA32FPCOMPAREANALYSER_INCL
#define IA32FPCOMPAREANALYSER_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "infra/Assert.hpp"
#include "x/codegen/X86Ops.hpp"

namespace TR { class Machine; }
namespace TR { class Node; }
namespace TR { class Register; }

#define NUM_FPCOMPARE_ACTION_SETS  128

#define fpEvalChild1               0x01
#define fpEvalChild2               0x02
#define fpCmpReg1Reg2              0x04
#define fpCmpReg2Reg1              0x08
#define fpCmpReg1Mem2              0x10
#define fpCmpReg2Mem1              0x20
#define fpNoOperandSwapping        0x40

class TR_X86FPCompareAnalyser
   {
   public:

   TR_X86FPCompareAnalyser(TR::CodeGenerator *cg)
      : _cg(cg), _machine(cg->machine()),
      _reversedOperands(false),
      _inputs(0) {}

   virtual TR::Register *fpCompareAnalyser(TR::Node       *root,
                                          TR_X86OpCodes cmpRegRegOpCode,
                                          TR_X86OpCodes cmpRegMemOpCode,
                                          TR_X86OpCodes cmpiRegRegOpCode,
                                          bool           useFCOMIInstructions);

   // Possible actions based on the characteristics of the operands.
   //
   enum EFPActions
      {
      kEvalChild1   = 0x01,
      kEvalChild2   = 0x02,
      kCmpReg1Reg2  = 0x04,
      kCmpReg2Reg1  = 0x08,
      kCmpReg1Mem2  = 0x10,
      kCmpReg2Mem1  = 0x20
      };

   // Operand characteristics
   //
   enum Einputs
      {
      kClob2    = 0x01,
      kMem2     = 0x02,
      kReg2     = 0x04,
      kClob1    = 0x08,
      kMem1     = 0x10,
      kReg1     = 0x20,
      kNoOpSwap = 0x40
      };

   void setReg1()              {_inputs |= kReg1;}
   void setReg2()              {_inputs |= kReg2;}
   void setMem1()              {_inputs |= kMem1;}
   void setMem2()              {_inputs |= kMem2;}
   void setClob1()             {_inputs |= kClob1;}
   void setClob2()             {_inputs |= kClob2;}
   void setNoOperandSwapping() {_inputs |= kNoOpSwap;}

   bool getEvalChild1()        {return (_actionMap[_inputs] & kEvalChild1)  ? true : false;}
   bool getEvalChild2()        {return (_actionMap[_inputs] & kEvalChild2)  ? true : false;}
   bool getCmpReg1Reg2()       {return (_actionMap[_inputs] & kCmpReg1Reg2) ? true : false;}
   bool getCmpReg2Reg1()       {return (_actionMap[_inputs] & kCmpReg2Reg1) ? true : false;}
   bool getCmpReg1Mem2()       {return (_actionMap[_inputs] & kCmpReg1Mem2) ? true : false;}
   bool getCmpReg2Mem1()       {return (_actionMap[_inputs] & kCmpReg2Mem1) ? true : false;}

   bool getReversedOperands()       {return _reversedOperands;}
   bool setReversedOperands(bool b) {return (_reversedOperands = b);}
   bool notReversedOperands()       {return (_reversedOperands = ((_reversedOperands == false) ? true : false));}

   void setInputs(TR::Node     *firstChild,
                  TR::Register *firstRegister,
                  TR::Node     *secondChild,
                  TR::Register *secondRegister,
                  bool         disallowMemoryFormInstructions,
                  bool         disallowOperandSwapping);

   protected:

   TR::CodeGenerator *   _cg;
   bool                 _reversedOperands;
   uint8_t              _inputs;
   TR::Machine *_machine;

   static const uint8_t _actionMap[NUM_FPCOMPARE_ACTION_SETS];
   };

class TR_IA32XMMCompareAnalyser : public TR_X86FPCompareAnalyser
   {
   public:

   TR_IA32XMMCompareAnalyser(TR::CodeGenerator *cg) : TR_X86FPCompareAnalyser(cg) {}

   virtual TR::Register *fpCompareAnalyser(TR::Node       *root,
                                          TR_X86OpCodes cmpRegRegOpCode,
                                          TR_X86OpCodes cmpRegMemOpCode,
                                          TR_X86OpCodes cmpiRegRegOpCode,
                                          bool           useFCOMIInstructions)
      {
      TR_ASSERT(0, "TR_IA32XMMCompareAnalyser::fpCompareAnalyser() should not be called\n");
      return NULL;
      }

   TR::Register *xmmCompareAnalyser(TR::Node        *root,
                                   TR_X86OpCodes  cmpRegRegOpCode,
                                   TR_X86OpCodes  cmpRegMemOpCode);

   };
#endif
