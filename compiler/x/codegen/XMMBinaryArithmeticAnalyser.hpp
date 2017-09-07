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

#ifndef X86XMMBINARYARITHMETICANALYSER_INCL
#define X86XMMBINARYARITHMETICANALYSER_INCL

#include <stdint.h>              // for uint8_t
#include "x/codegen/X86Ops.hpp"  // for TR_X86OpCodes

namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class Register; }

// Exponent bias for scaling operands of double precision multiplies and
// divides in strictfp mode.
//
// DOUBLE_EXPONENT_SCALE = -(16383-1023)
//
#if !defined(_LONG_LONG) && !defined(LINUX)
#define XMM_DOUBLE_EXPONENT_SCALE 0xc0ce000000000000L
#else
#define XMM_DOUBLE_EXPONENT_SCALE 0xc0ce000000000000LL
#endif


// Total possible action sets based on the characteristics of the node children.
//
#define NUM_XMM_ACTION_SETS 128


class TR_X86XMMBinaryArithmeticAnalyser
   {
   public:

   void genericXMMAnalyser(TR::Node *root);

   // Available arithmetic instruction packages.
   //
   enum XMMArithPackages
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
      kNumXMMArithPackages = kDDIVpackage+1
      };

   static uint8_t getX86XMMOpPackage(TR::Node *node);

   TR_X86XMMBinaryArithmeticAnalyser(TR::CodeGenerator *codeGen)
      : _codeGen(codeGen),
        _package(kBADpackage),
        _inputs(0) {}

   TR_X86XMMBinaryArithmeticAnalyser(uint8_t package, TR::CodeGenerator *codeGen)
      : _codeGen(codeGen),
        _package(package),
        _inputs(0) {}

   TR_X86XMMBinaryArithmeticAnalyser(TR::Node *node, TR::CodeGenerator *codeGen)
      : _codeGen(codeGen),
        _package(getX86XMMOpPackage(node)),
        _inputs(0) {}

   uint8_t setPackage(uint8_t p)  {return _package = p;}
   uint8_t getPackage()           {return _package;}

   // Individual instruction variants within an instruction package.
   //
   enum XMMArithVariants
      {
      kOpRegReg   = 0,
      kOpRegMem   = 1,
      kNumXMMArithVariants = kOpRegMem+1
      };

   TR_X86OpCodes getRegRegOp()
      {
      return _opCodePackage[_package][kOpRegReg];
      }

   TR_X86OpCodes getRegMemOp()
      {
      return _opCodePackage[_package][kOpRegMem];
      }

   // Possible actions based on the characteristics of the operands.
   //
   enum XMMArithActions
      {
      kEvalTarget  = 0x01,
      kEvalSource  = 0x02,
      kCopyTarget  = 0x04,
      kRegReg      = 0x08,
      kRegMem      = 0x10,
      kResetInputs = 0x20,
      kReverse     = 0x40
      };

   bool evaluatesTarget() {return (_actionMap[_inputs] & kEvalTarget) ? true : false;}
   bool evaluatesSource() {return (_actionMap[_inputs] & kEvalSource) ? true : false;}
   bool copiesTargetReg() {return (_actionMap[_inputs] & kCopyTarget) ? true : false;}
   bool mustUseRegRegOp() {return (_actionMap[_inputs] & kRegReg) ? true : false;}
   bool canUseRegMemOp()  {return (_actionMap[_inputs] & kRegMem) ? true : false;}
   bool resetsInputs()    {return (_actionMap[_inputs] & kResetInputs) ? true : false;}
   bool isReversed()      {return (_actionMap[_inputs] & kReverse) ? true : false;}

   // Operand characteristics
   //
   enum XMMArithInputs
      {
      kClob2       = 0x01,
      kMem2        = 0x02,
      kReg2        = 0x04,
      kClob1       = 0x08,
      kMem1        = 0x10,
      kReg1        = 0x20,
      kCommutative = 0x40
      };

   void setClob1()       {_inputs |= kClob1;}
   void setClob2()       {_inputs |= kClob2;}
   void setMem1()        {_inputs |= kMem1;}
   void setMem2()        {_inputs |= kMem2;}
   void setReg1()        {_inputs |= kReg1;}
   void setReg2()        {_inputs |= kReg2;}
   void setCommutative() {_inputs |= kCommutative;}
   bool isCommutative()  {return (_inputs & kCommutative) ? true : false;}

   void setInputs(TR::Node     *firstChild,
                  TR::Register *firstRegister,
                  TR::Node     *secondChild,
                  TR::Register *secondRegister);

   private:

   static const uint8_t        _actionMap[NUM_XMM_ACTION_SETS];
   static const TR_X86OpCodes _opCodePackage[kNumXMMArithPackages][kNumXMMArithVariants];
   uint8_t                     _package;
   uint8_t                     _inputs;
   TR::CodeGenerator *       _codeGen;
   };

#endif
