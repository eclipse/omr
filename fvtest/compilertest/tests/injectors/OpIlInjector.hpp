/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef TEST_OPILINJECTOR_INCL
#define TEST_OPILINJECTOR_INCL

#include "ilgen/IlInjector.hpp"

#include <stdint.h>
#include "env/jittypes.h"

namespace TR { class Node; }
namespace TR { class TypeDictionary; }

namespace TestCompiler
{
typedef enum ParmType { ParmInteger, ParmLong, ParmShort, ParmByte, ParmAddress, ParmDouble, ParmFloat } ParmType;

typedef union ParmValue
{
   int32_t parmInt;
   int64_t parmLong;
   int16_t parmShort;
   int8_t parmByte;
   uintptr_t parmAddress;
   double parmDouble;
   float parmFloat;
} ParmValue;

typedef struct ParmNode
{
   ParmType type;
   ParmValue value;

   ParmNode(ParmType t) : type(t)  {}
} ParmNode;

class OpIlInjector : public TR::IlInjector
   {
   public:
   OpIlInjector(TR::TypeDictionary *types, TestDriver *test, TR::ILOpCodes opCode)
      : TR::IlInjector(types, test),
        _opCode(opCode),
        _numOptArgs(0),
        _dataType(TR::DataTypes::NoType),
        _conditionalDataType(TR::DataTypes::NoType)
      {
      setDataType();
      }
   TR_ALLOC(TR_Memory::IlGenerator)

   virtual bool injectIL() = 0;

   virtual void iconstParm(uint32_t slot, int32_t value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "iconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmInteger);
      pnode->value.parmInt = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void lconstParm(uint32_t slot, int64_t value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "lconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmLong);
      pnode->value.parmLong = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void bconstParm(uint32_t slot, int8_t value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "bconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmByte);
      pnode->value.parmByte = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void sconstParm(uint32_t slot, int16_t value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "sconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmShort);
      pnode->value.parmShort = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void aconstParm(uint32_t slot, uintptr_t value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "aconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmAddress);
      pnode->value.parmAddress = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void dconstParm(uint32_t slot, double value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "dconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmDouble);
      pnode->value.parmDouble = value;
      _optArgs[slot-1] = pnode;
      }
   virtual void fconstParm(uint32_t slot, float value)
      {
      TR_ASSERT(slot > 0 && slot <= _numOptArgs, "fconstParm: slot %d should be greater than 0 and less than _numOptArgs %d\n", slot, _numOptArgs);
      ParmNode *pnode = new ParmNode(ParmFloat);
      pnode->value.parmFloat = value;
      _optArgs[slot-1] = pnode;
      }

   protected:
   bool isOpCodeSupported();
   TR::Node *parm(uint32_t slot);
   void initOptArgs(uint32_t numOptArgs)
      {
      if (numOptArgs == 0) return;
      _numOptArgs = numOptArgs;
      _optArgs = new ParmNode *[_numOptArgs];
      for (int32_t i = 0; i < _numOptArgs; ++i)
         _optArgs[i] = NULL;
      }

   TR::ILOpCodes _opCode;
   TR::DataType _dataType; // datatype of OpCode child/children
   TR::DataType _conditionalDataType; // return type of Select opcodes

   ParmNode **_optArgs;  // holds args that are not on stack params
   uint32_t _numOptArgs;

   private:
   void setDataType();
   };

} // namespace TestCompiler

#endif // !defined(TEST_OPILINJECTOR_INCL)
