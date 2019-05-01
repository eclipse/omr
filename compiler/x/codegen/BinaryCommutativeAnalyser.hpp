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

#ifndef IA32BINARYCOMMUTATIVEANALYSER_INCL
#define IA32BINARYCOMMUTATIVEANALYSER_INCL

#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/CodeGenerator.hpp"
#include "x/codegen/X86Ops.hpp"

namespace TR { class Machine; }
namespace TR { class Node; }

#define EvalChild1  0x01
#define EvalChild2  0x02
#define CopyReg1    0x04
#define CopyReg2    0x08
#define OpReg1Reg2  0x10
#define OpReg2Reg1  0x20
#define OpReg1Mem2  0x40
#define OpReg2Mem1  0x80

class TR_X86BinaryCommutativeAnalyser  : public TR_Analyser
   {
   static const uint8_t _actionMap[NUM_ACTIONS];
   TR::CodeGenerator *_cg;
   bool _reversedOperands;
   TR::Machine *_machine;

   public:

   TR_X86BinaryCommutativeAnalyser(TR::CodeGenerator *cg)
      : _cg(cg),
        _machine(cg->machine()),
        _reversedOperands(false)
      {}

   void genericAnalyserWithExplicitOperands(TR::Node      *root,
                                            TR::Node      *firstChild,
                                            TR::Node      *secondChild,
                                            TR_X86OpCodes regRegOpCode,
                                            TR_X86OpCodes regMemOpCode,
                                            TR_X86OpCodes copyOpCode,
                                            bool          nonClobberingDestination = false);

   void genericAnalyser(TR::Node      *root,
                        TR_X86OpCodes regRegOpCode,
                        TR_X86OpCodes regMemOpCode,
                        TR_X86OpCodes copyOpCode,
                        bool          nonClobberingDestination = false);

   TR::Register *genericAnalyserImpl(TR::Node      *root,
                                     TR::Node      *firstChild,
                                     TR::Node      *secondChild,
                                     TR_X86OpCodes regRegOpCode,
                                     TR_X86OpCodes regMemOpCode,
                                     TR_X86OpCodes copyOpCode,
                                     bool          nonClobberingDestination);

   void genericLongAnalyser(TR::Node       *root,
                            TR_X86OpCodes lowRegRegOpCode,
                            TR_X86OpCodes highRegRegOpCode,
                            TR_X86OpCodes lowRegMemOpCode,
                            TR_X86OpCodes lowRegMemOpCode2Byte,
                            TR_X86OpCodes lowRegMemOpCode1Byte,
                            TR_X86OpCodes highRegMemOpCode,
                            TR_X86OpCodes copyOpCode);

   void integerAddAnalyser(TR::Node       *root,
                           TR_X86OpCodes regRegOpCode,
                           TR_X86OpCodes regMemOpCode,
                           bool needsEflags = false,
                           TR::Node       *carry = 0);// 0 by default

   void integerAddAnalyserWithExplicitOperands(TR::Node       *root,
                                               TR::Node *firstChild,
                                               TR::Node *secondChild,
                                               TR_X86OpCodes regRegOpCode,
                                               TR_X86OpCodes regMemOpCode,
                                               bool needsEflags = false, 
                                               TR::Node *carry = 0);

   TR::Register* integerAddAnalyserImpl(TR::Node       *root,
                           TR::Node *firstChild,
                           TR::Node *secondChild,
                           TR_X86OpCodes regRegOpCode,
                           TR_X86OpCodes regMemOpCode,
                           bool needsEflags, 
                           TR::Node *carry);

   void longAddAnalyserWithExplicitOperands(TR::Node *root, TR::Node *firstChild, TR::Node *secondChild);
   void longAddAnalyser(TR::Node *root);
   TR::Register* longAddAnalyserImpl(TR::Node *root, TR::Node *&firstChild, TR::Node *&secondChild);

   void longMultiplyAnalyser(TR::Node *root);
   void longDualMultiplyAnalyser(TR::Node *root);

   bool getReversedOperands()       {return _reversedOperands;}
   bool setReversedOperands(bool b) {return (_reversedOperands = b);}
   bool notReversedOperands()       {return (_reversedOperands = ((_reversedOperands == false) ? true : false));}

   bool getEvalChild1() {return (_actionMap[getInputs()] & EvalChild1) ? true : false;}
   bool getEvalChild2() {return (_actionMap[getInputs()] & EvalChild2) ? true : false;}
   bool getCopyReg1()   {return (_actionMap[getInputs()] & CopyReg1)   ? true : false;}
   bool getCopyReg2()   {return (_actionMap[getInputs()] & CopyReg2)   ? true : false;}
   bool getOpReg1Reg2() {return (_actionMap[getInputs()] & OpReg1Reg2) ? true : false;}
   bool getOpReg2Reg1() {return (_actionMap[getInputs()] & OpReg2Reg1) ? true : false;}
   bool getOpReg1Mem2() {return (_actionMap[getInputs()] & OpReg1Mem2) ? true : false;}
   bool getOpReg2Mem1() {return (_actionMap[getInputs()] & OpReg2Mem1) ? true : false;}
   bool getCopyRegs()   {return (_actionMap[getInputs()] & (CopyReg1 | CopyReg2)) ? true : false;}

   };

#endif
