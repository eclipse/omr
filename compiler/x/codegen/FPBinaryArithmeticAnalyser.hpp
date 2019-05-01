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

#ifndef IA32FPBINARYARITHMETICANALYSER_INCL
#define IA32FPBINARYARITHMETICANALYSER_INCL

#include <stdint.h>
#include "il/ILOpCodes.hpp"
#include "x/codegen/X86Ops.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class Register; }

// Exponent bias for scaling operands of double precision multiplies and
// divides in strictfp mode.
//
// DOUBLE_EXPONENT_SCALE = -(16383-1023)
//
#if !defined(_LONG_LONG) && !defined(LINUX)
#define DOUBLE_EXPONENT_SCALE 0xc0ce000000000000L
#else
#define DOUBLE_EXPONENT_SCALE 0xc0ce000000000000LL
#endif


// Total possible action sets based on the characteristics of the node children.
//
#define NUM_ACTION_SETS 256


class TR_X86FPBinaryArithmeticAnalyser
   {
   public:

   // Available arithmetic instruction packages.
   //
   enum EFPPackages
      {
      kBADpackage  = 0,
      kFADDpackage = 1,
      kDADDpackage = 2,
      kFMULpackage = 3,
      kDMULpackage = 4,
      kFSUBpackage = 5,
      kDSUBpackage = 6,
      kFDIVpackage = 7,
      kDDIVpackage = 8,
      kNumFPPackages = kDDIVpackage+1
      };

   // Individual instruction variants within an instruction package.
   //
   enum EFPArithVariants
      {
      kOpReg1Reg2   = 0,
      kOpReg2Reg1   = 1,
      kOpReg1Mem2   = 2,
      kOpReg2Mem1   = 3,
      kOpReg1ConvS2 = 4,
      kOpReg1ConvI2 = 5,
      kOpReg2ConvS1 = 6,
      kOpReg2ConvI1 = 7,
      kNumFPArithVariants = kOpReg2ConvI1+1
      };

   static uint8_t getIA32FPOpPackage(TR::Node *node);

   uint8_t setPackage(uint8_t p)  {return _package = p;}
   uint8_t getPackage()           {return _package;}

   uint8_t getOpsReversed()       {return (_actionMap[_inputs] & kReverse) ? 1 : 0;}

   TR_X86OpCodes getRegRegOp()   {return getOpsReversed() ? _opCodePackage[_package][kOpReg2Reg1] :
                                                             _opCodePackage[_package][kOpReg1Reg2];}

   TR_X86OpCodes getRegMemOp()   {return getOpsReversed() ? _opCodePackage[_package][kOpReg2Mem1] :
                                                             _opCodePackage[_package][kOpReg1Mem2];}

   TR_X86OpCodes getRegConvSOp() {return getOpsReversed() ? _opCodePackage[_package][kOpReg2ConvS1] :
                                                             _opCodePackage[_package][kOpReg1ConvS2];}

   TR_X86OpCodes getRegConvIOp() {return getOpsReversed() ? _opCodePackage[_package][kOpReg2ConvI1] :
                                                             _opCodePackage[_package][kOpReg1ConvI2];}

   // Possible actions based on the characteristics of the operands.
   //
   enum EFPActions
      {
      kEvalTarget = 0x01,
      kEvalSource = 0x02,
      kCopyTarget = 0x04,
      kReg1Reg2   = 0x08,
      kReg1Mem2   = 0x10,
      kReg1Conv2  = 0x20,
      kReverse    = 0x40
      };

   bool isEvalTarget() {return (_actionMap[_inputs] & kEvalTarget) ? true : false;}
   bool isEvalSource() {return (_actionMap[_inputs] & kEvalSource) ? true : false;}
   bool isCopyReg()    {return (_actionMap[_inputs] & kCopyTarget) ? true : false;}
   bool isOpRegReg()   {return (_actionMap[_inputs] & kReg1Reg2) ? true : false;}
   bool isOpRegMem()   {return (_actionMap[_inputs] & kReg1Mem2) ? true : false;}
   bool isOpRegConv()  {return (_actionMap[_inputs] & kReg1Conv2) ? true : false;}

   TR_X86FPBinaryArithmeticAnalyser(TR::CodeGenerator *cg)
      : _cg(cg),
        _package(kBADpackage),
        _inputs(0) {}

   TR_X86FPBinaryArithmeticAnalyser(uint8_t package, TR::CodeGenerator *cg)
      : _cg(cg),
        _package(package),
        _inputs(0) {}

   TR_X86FPBinaryArithmeticAnalyser(TR::Node *root, TR::CodeGenerator *cg)
      : _cg(cg),
        _package(getIA32FPOpPackage(root)),
        _inputs(0) {}

   uint8_t getIA32FPOpPackage(TR::ILOpCodes op);

   // Operand characteristics
   //
   enum Einputs
      {
      kConv2 = 0x01,
      kClob2 = 0x02,
      kMem2  = 0x04,
      kReg2  = 0x08,
      kConv1 = 0x10,
      kClob1 = 0x20,
      kMem1  = 0x40,
      kReg1  = 0x80
      };

   void setReg1()  {_inputs |= kReg1;}
   void setReg2()  {_inputs |= kReg2;}
   void setMem1()  {_inputs |= kMem1;}
   void setMem2()  {_inputs |= kMem2;}
   void setClob1() {_inputs |= kClob1;}
   void setClob2() {_inputs |= kClob2;}
   void setConv1() {_inputs |= kConv1;}
   void setConv2() {_inputs |= kConv2;}

   void setInputs(TR::Node     *firstChild,
                  TR::Register *firstRegister,
                  TR::Node     *secondChild,
                  TR::Register *secondRegister);

   void genericFPAnalyser(TR::Node *root);
   bool isIntToFPConversion(TR::Node *child);

   private:

   static const uint8_t        _actionMap[NUM_ACTION_SETS];
   static const TR_X86OpCodes _opCodePackage[kNumFPPackages][kNumFPArithVariants];
   TR::CodeGenerator *          _cg;
   uint8_t                     _package;
   uint8_t                     _inputs;
   };

#endif
