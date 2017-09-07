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

#include <stdint.h>

#define ARMOpProp_HasRecordForm     0x00000001
#define ARMOpProp_SetsCarryFlag     0x00000002
#define ARMOpProp_SetsOverflowFlag  0x00000004
#define ARMOpProp_ReadsCarryFlag    0x00000008
#define ARMOpProp_BranchOp          0x00000040
#define ARMOpProp_VFP               0x00000080
#define ARMOpProp_DoubleFP          0x00000100
#define ARMOpProp_SingleFP          0x00000200
#define ARMOpProp_UpdateForm        0x00000400
#define ARMOpProp_Arch4             0x00000800
#define ARMOpProp_IsRecordForm      0x00001000
#define ARMOpProp_Label             0x00002000
#define ARMOpProp_Arch6             0x00004000

class TR_ARMOpCode
   {

   typedef uint32_t TR_OpCodeBinaryEntry;

   TR_ARMOpCodes                    _opCode;
   static const uint32_t             properties[ARMNumOpCodes];
   static const TR_OpCodeBinaryEntry binaryEncodings[ARMNumOpCodes];

   public:

   TR_ARMOpCode()                  : _opCode(ARMOp_bad) {}
   TR_ARMOpCode(TR_ARMOpCodes op) : _opCode(op)       {}

   TR_ARMOpCodes getOpCodeValue()                  {return _opCode;}
   TR_ARMOpCodes setOpCodeValue(TR_ARMOpCodes op) {return (_opCode = op);}
   TR_ARMOpCodes getRecordFormOpCodeValue() {return (TR_ARMOpCodes)(_opCode+1);}

   uint32_t isRecordForm() {return properties[_opCode] & ARMOpProp_IsRecordForm;}

   uint32_t hasRecordForm() {return properties[_opCode] & ARMOpProp_HasRecordForm;}

   uint32_t singleFPOp() {return properties[_opCode] & ARMOpProp_SingleFP;}

   uint32_t doubleFPOp() {return properties[_opCode] & ARMOpProp_DoubleFP;}

   uint32_t isVFPOp() {return properties[_opCode] & ARMOpProp_VFP;}

   uint32_t gprOp() {return (properties[_opCode] & (ARMOpProp_DoubleFP | ARMOpProp_SingleFP)) == 0;}

   uint32_t fprOp() {return (properties[_opCode] & (ARMOpProp_DoubleFP | ARMOpProp_SingleFP));}

   uint32_t readsCarryFlag() {return properties[_opCode] & ARMOpProp_ReadsCarryFlag;}

   uint32_t setsCarryFlag() {return properties[_opCode] & ARMOpProp_SetsCarryFlag;}

   uint32_t setsOverflowFlag() {return properties[_opCode] & ARMOpProp_SetsOverflowFlag;}

   uint32_t isBranchOp() {return properties[_opCode] & ARMOpProp_BranchOp;}

   uint32_t isLabel() {return properties[_opCode] & ARMOpProp_Label;}

   uint8_t *copyBinaryToBuffer(uint8_t *cursor)
	  {
	  *(uint32_t *)cursor = *(uint32_t *)&binaryEncodings[_opCode];
	  return cursor;
	  }
   };
