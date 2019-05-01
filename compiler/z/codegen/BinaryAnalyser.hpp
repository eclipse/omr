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

#ifndef S390BINARYANALYSER_INCL
#define S390BINARYANALYSER_INCL

#include <stdint.h>
#include "codegen/Analyser.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"

namespace TR { class Node; }
namespace TR { class Register; }

#define EvalChild1   0x01
#define EvalChild2   0x02
#define CopyReg1     0x04
#define BinaryReg1Reg2  0x08
#define BinaryReg3Reg2  0x10
#define BinaryReg1Mem2  0x20
#define BinaryReg3Mem2  0x40
#define InvalidBACEMap  0x80

class TR_S390BinaryAnalyser : public TR_Analyser
   {

   static const uint8_t  actionMap[NUM_ACTIONS];
   static const uint8_t  clobEvalActionMap[NUM_ACTIONS];

   TR::CodeGenerator *_cg;

   public:

   TR_S390BinaryAnalyser(TR::CodeGenerator *cg) : _cg(cg) {}

   void genericAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode,
                                TR::InstOpCode::Mnemonic copyOpCode);

   void intBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {

     TR::InstOpCode::Mnemonic loadOp = TR::InstOpCode::getLoadRegOpCode();

     genericAnalyser(root,regToRegOpCode,memToRegOpCode,loadOp);
     }

   void longBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LGR);
     }

   void floatBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LER);
     }


   void doubleBinaryAnalyser(TR::Node       *root,
                                TR::InstOpCode::Mnemonic regToRegOpCode,
                                TR::InstOpCode::Mnemonic memToRegOpCode)
     {
     genericAnalyser(root,regToRegOpCode,memToRegOpCode,TR::InstOpCode::LDR);
     }

   void longSubtractAnalyser(TR::Node *root);

   bool getEvalChild1()  {return (actionMap[getInputs()] & EvalChild1)  ? true : false;}
   bool getEvalChild2()  {return (actionMap[getInputs()] & EvalChild2)  ? true : false;}

   bool getCopyReg1()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & CopyReg1)    ? true : false;
         }
      return (actionMap[getInputs()] & CopyReg1)    ? true : false;
      }

   bool getBinaryReg1Reg2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg1Reg2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg1Reg2) ? true : false;
      }

   bool getBinaryReg3Reg2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg3Reg2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg3Reg2) ? true : false;
      }

   bool getBinaryReg1Mem2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg1Mem2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg1Mem2) ? true : false;
      }

   bool getBinaryReg3Mem2()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & BinaryReg3Mem2) ? true : false;
         }
      return (actionMap[getInputs()] & BinaryReg3Mem2) ? true : false;
      }

   bool getInvalid()
      {
      if (!cg()->useClobberEvaluate())
         {
         return (clobEvalActionMap[getInputs()] & InvalidBACEMap) ? true : false;
         }
      return false;
      }

   TR::CodeGenerator *cg()               {return _cg;}

   private:

   void remapInputs(TR::Node *, TR::Register *, TR::Node *, TR::Register *);
   };

#endif
