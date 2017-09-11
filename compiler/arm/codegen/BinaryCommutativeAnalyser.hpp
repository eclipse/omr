/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef ARMBINARYCOMMUTATIVEANALYSER_INCL
#define ARMBINARYCOMMUTATIVEANALYSER_INCL

#include "codegen/ARMOps.hpp"
#include "codegen/Register.hpp"
#include "il/Node.hpp"
#include "codegen/Analyser.hpp"

namespace TR { class CodeGenerator; }

#define EvalChild1  0x01
#define EvalChild2  0x02
#define CopyReg1    0x04
#define CopyReg2    0x08
#define OpReg1Reg2  0x10
#define OpReg2Reg1  0x20
#define OpReg1Mem2  0x40
#define OpReg2Mem1  0x80

class TR_ARMBinaryCommutativeAnalyser : public TR_Analyser
   {
   static const uint8_t actionMap[NUM_ACTIONS];
   TR::CodeGenerator *_cg;
   bool reversedOperands;

   public:

   TR_ARMBinaryCommutativeAnalyser(TR::CodeGenerator *cg) : _cg(cg), reversedOperands(false) {}

   void genericAnalyser(TR::Node       *root,
                        TR_ARMOpCodes regToRegOpCode,
                        TR_ARMOpCodes copyOpCode,
                        bool           nonClobberingDestination = false);

   void genericLongAnalyser(TR::Node       *root,
                            TR_ARMOpCodes lowRegToRegOpCode,
                            TR_ARMOpCodes highRegToRegOpCode,
                            TR_ARMOpCodes lowMemToRegOpCode,
                            TR_ARMOpCodes highMemToRegOpCode,
                            TR_ARMOpCodes copyOpCode);

   void integerAddAnalyser(TR::Node       *root,
                           TR_ARMOpCodes regToRegOpCode);

   void longAddAnalyser(TR::Node *root);

   bool getReversedOperands()       {return reversedOperands;}
   bool setReversedOperands(bool b) {return (reversedOperands = b);}
   bool notReversedOperands()       {return (reversedOperands = ((reversedOperands == false) ? true : false));}

   bool getEvalChild1() {return (actionMap[getInputs()] & EvalChild1) ? true : false;}
   bool getEvalChild2() {return (actionMap[getInputs()] & EvalChild2) ? true : false;}
   bool getCopyReg1()   {return (actionMap[getInputs()] & CopyReg1)   ? true : false;}
   bool getCopyReg2()   {return (actionMap[getInputs()] & CopyReg2)   ? true : false;}
   bool getOpReg1Reg2() {return (actionMap[getInputs()] & OpReg1Reg2) ? true : false;}
   bool getOpReg2Reg1() {return (actionMap[getInputs()] & OpReg2Reg1) ? true : false;}
   bool getOpReg1Mem2() {return (actionMap[getInputs()] & OpReg1Mem2) ? true : false;}
   bool getOpReg2Mem1() {return (actionMap[getInputs()] & OpReg2Mem1) ? true : false;}
   bool getCopyRegs()   {return (actionMap[getInputs()] & (CopyReg1 | CopyReg2)) ? true : false;}

   TR::CodeGenerator *cg() {return _cg;}

   };

#endif
