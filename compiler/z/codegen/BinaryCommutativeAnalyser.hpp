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

#ifndef S390BINARYCOMMUTATIVEANALYSER_INCL
#define S390BINARYCOMMUTATIVEANALYSER_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/InstOpCode.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }

#define EvalChild1  0x01
#define EvalChild2  0x02
#define CopyReg1    0x04
#define CopyReg2    0x08
#define OpReg1Reg2  0x10
#define OpReg2Reg1  0x20
#define OpReg1Mem2  0x40
#define OpReg2Mem1  0x80
#define OpReg3Mem1  0x001
#define OpReg3Mem2  0x002
#define InvalidBCACEMap 0x004

class TR_S390BinaryCommutativeAnalyser : public TR_Analyser
   {
   static const uint8_t actionMap[NUM_ACTIONS];
   static const uint16_t clobEvalActionMap[NUM_ACTIONS];

   TR::CodeGenerator    *_cg;
   bool                 reversedOperands;

   public:

   TR_S390BinaryCommutativeAnalyser(TR::CodeGenerator *cg)
      : _cg(cg),
        reversedOperands(false)
      {}


   void genericAnalyser(TR::Node       *root,
                        TR::InstOpCode::Mnemonic regToRegOpCode,
                        TR::InstOpCode::Mnemonic memToRegOpCode,
                        TR::InstOpCode::Mnemonic copyOpCode,
                        bool           nonClobberingDestination = false,
                        TR::LabelSymbol *targetLabel = NULL,
                        TR::InstOpCode::S390BranchCondition fBranchOpCond = TR::InstOpCode::COND_NOP,
                        TR::InstOpCode::S390BranchCondition rBranchOpCond = TR::InstOpCode::COND_NOP);

   void integerAddAnalyser(TR::Node       *root,
                           TR::InstOpCode::Mnemonic regToRegOpCode,
                           TR::InstOpCode::Mnemonic memToRegOpCode,
                           TR::InstOpCode::Mnemonic copyOpCode);

   void floatBinaryCommutativeAnalyser(TR::Node       *root,
                        TR::InstOpCode::Mnemonic regToRegOpCode,
                        TR::InstOpCode::Mnemonic memToRegOpCode)
   {
   genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LER);
   }

   void doubleBinaryCommutativeAnalyser(TR::Node       *root,
                        TR::InstOpCode::Mnemonic regToRegOpCode,
                        TR::InstOpCode::Mnemonic memToRegOpCode)
   {
   genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LDR);
   }

   bool getReversedOperands()       {return reversedOperands;}
   bool setReversedOperands(bool b) {return reversedOperands = b;}
   bool notReversedOperands()       {return reversedOperands = ((reversedOperands == false) ? true : false);}

   bool getEvalChild1() {return (actionMap[getInputs()] & EvalChild1) ? true : false;}
   bool getEvalChild2() {return (actionMap[getInputs()] & EvalChild2) ? true : false;}

   bool conversionIsRemoved(TR::Node * root, TR::Node * &child);

   bool getCopyReg1();

   bool getCopyReg2();

   bool getOpReg1Reg2();

   bool getOpReg2Reg1();

   bool getOpReg1Mem2();

   bool getOpReg3Mem2();

   bool getOpReg2Mem1();

   bool getOpReg3Mem1();

   bool getCopyRegs();

   bool getInvalid();

   void remapInputs(TR::Node *, TR::Register *, TR::Node *, TR::Register *, bool nonClobberingDestination = false);

   TR::CodeGenerator *cg() {return _cg;}

   private:
   TR::Register* allocateAddSubRegister(TR::Node* node, TR::Register* src1Reg);


   };

#endif
