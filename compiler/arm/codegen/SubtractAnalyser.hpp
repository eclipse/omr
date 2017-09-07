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

#ifndef ARMSUBTRACTANALYSER_INCL
#define ARMSUBTRACTANALYSER_INCL

#include "codegen/Analyser.hpp"

#include <stdint.h>
#include "arm/codegen/ARMOps.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }

#define EvalChild1   0x01
#define EvalChild2   0x02
#define CopyReg1     0x04
#define SubReg1Reg2  0x08
#define SubReg3Reg2  0x10
#define SubReg1Mem2  0x20
#define SubReg3Mem2  0x40

class TR_ARMSubtractAnalyser : public TR_Analyser
   {
   TR::CodeGenerator *_cg;
   static const uint8_t actionMap[NUM_ACTIONS];

   public:

   TR_ARMSubtractAnalyser(TR::CodeGenerator *cg) : _cg(cg) {}

   void integerSubtractAnalyser(TR::Node       *root,
                                TR_ARMOpCodes regToRegOpCode,
                                TR_ARMOpCodes memToRegOpCode);

   void longSubtractAnalyser(TR::Node *root);

   bool getEvalChild1()  {return (actionMap[getInputs()] & EvalChild1)  ? true : false;}
   bool getEvalChild2()  {return (actionMap[getInputs()] & EvalChild2)  ? true : false;}
   bool getCopyReg1()    {return (actionMap[getInputs()] & CopyReg1)    ? true : false;}
   bool getSubReg1Reg2() {return (actionMap[getInputs()] & SubReg1Reg2) ? true : false;}
   bool getSubReg3Reg2() {return (actionMap[getInputs()] & SubReg3Reg2) ? true : false;}
   bool getSubReg1Mem2() {return (actionMap[getInputs()] & SubReg1Mem2) ? true : false;}
   bool getSubReg3Mem2() {return (actionMap[getInputs()] & SubReg3Mem2) ? true : false;}

   TR::CodeGenerator *cg() {return _cg;}

   };

#endif
