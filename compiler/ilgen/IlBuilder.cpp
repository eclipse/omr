/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
#include "ilgen/IlType.hpp"
#include "ilgen/IlValue.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/IlBuilderImpl.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"


OMR::IlBuilder::IlBuilder(TR::MethodBuilder *mb, TR::TypeDictionary *types)
   : _mb(mb),
     _impl(TR::IlBuilderImpl::allocate(mb->impl(), types->impl())),
     NoType(types->NoType),
     Int8(types->Int8),
     Int16(types->Int16),
     Int32(types->Int32),
     Int64(types->Int64),
     Word(types->Word),
     Float(types->Float),
     Double(types->Double),
     Address(types->Address),
     VectorInt8(types->VectorInt8),
     VectorInt16(types->VectorInt16),
     VectorInt32(types->VectorInt32),
     VectorInt64(types->VectorInt64),
     VectorFloat(types->VectorFloat),
     VectorDouble(types->VectorDouble)
   {
   _impl->setClient(static_cast<TR::IlBuilder *>(this));
   }

OMR::IlBuilder::IlBuilder(TR::IlBuilderImpl *impl, TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
   : _mb(methodBuilder),
     _impl(impl),
     NoType(types->NoType),
     Int8(types->Int8),
     Int16(types->Int16),
     Int32(types->Int32),
     Int64(types->Int64),
     Word(types->Word),
     Float(types->Float),
     Double(types->Double),
     Address(types->Address),
     VectorInt8(types->VectorInt8),
     VectorInt16(types->VectorInt16),
     VectorInt32(types->VectorInt32),
     VectorInt64(types->VectorInt64),
     VectorFloat(types->VectorFloat),
     VectorDouble(types->VectorDouble)
   {
   }

OMR::IlBuilder::~IlBuilder()
   {
   if (_impl)
      {
      //delete impl();
      }
   }

TR::IlValue *
OMR::IlBuilder::Copy(TR::IlValue *value)
   {
   return impl()->Copy(value->impl());
   }

TR::IlBuilder *
OMR::IlBuilder::OrphanBuilder()
   {
   TR::IlBuilderImpl *bi = impl()->OrphanBuilderImpl();
   return orphanBuilder(bi);
   }

TR::IlBuilder *
OMR::IlBuilder::orphanBuilder(TR::IlBuilderImpl *bi)
   {
   TR::TypeDictionary *types = _mb->_types;
   TR::IlBuilder *b = new TR::IlBuilder(bi, _mb, types);
   bi->setClient(b);
   _mb->newBuilder(b);
   return b;
   }

/**
 * Constants
**/

TR::IlValue *
OMR::IlBuilder::NullAddress()
   {
   return impl()->NullAddress();
   }

TR::IlValue *
OMR::IlBuilder::ConstInt8(int8_t value)
   {
   return impl()->ConstInt8(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstInt16(int16_t value)
   {
   return impl()->ConstInt16(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstInt32(int32_t value)
   {
   return impl()->ConstInt32(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstInt64(int64_t value)
   {
   return impl()->ConstInt64(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstFloat(float value)
   {
   return impl()->ConstFloat(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstDouble(double value)
   {
   return impl()->ConstDouble(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstAddress(const void * const value)
   {
   return impl()->ConstAddress(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstString(const char * const value)
   {
   return impl()->ConstString(value);
   }

TR::IlValue *
OMR::IlBuilder::ConstInteger(TR::IlType *intType, int64_t value)
   {
   return impl()->ConstInteger(intType->impl(), value);
   }


   // arithmetic
TR::IlValue *
OMR::IlBuilder::Add(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Add(left->impl(), right->impl());
   }

#define ILBUILDER_PARM_SETUP(ptrImpl, ptrArg, parmArg) \
   TR::IlBuilderImpl *ptrImpl = NULL;                  \
   TR::IlBuilderImpl **ptrArg = NULL;                  \
   if (parmArg)                                        \
      {                                                \
      ptrArg = &ptrImpl;                               \
      if (*parmArg)                                    \
         ptrImpl = (*parmArg)->impl();                  \
      }

#define ILBUILDER_PARM_RETURN(ptrImpl, parmArg) \
   if (parmArg)                                 \
      *parmArg = ptrImpl->client();
   
TR::IlValue *
OMR::IlBuilder::AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(handlerImpl, handlerArg, handler);
   TR::IlValue *value = impl()->AddWithOverflow(handlerArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(handlerImpl, handler);
   return value;
   }

TR::IlValue *
OMR::IlBuilder::AddWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(handlerImpl, handlerArg, handler);
   TR::IlValue *value = impl()->AddWithUnsignedOverflow(handlerArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(handlerImpl, handler);
   return value;
   }

TR::IlValue *
OMR::IlBuilder::Sub(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Sub(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(handlerImpl, handlerArg, handler);
   TR::IlValue *value = impl()->SubWithOverflow(handlerArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(handlerImpl, handler);
   return value;
   }

TR::IlValue *
OMR::IlBuilder::SubWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(handlerImpl, handlerArg, handler);
   TR::IlValue *value = impl()->SubWithUnsignedOverflow(handlerArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(handlerImpl, handler);
   return value;
   }

TR::IlValue *
OMR::IlBuilder::Mul(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Mul(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(handlerImpl, handlerArg, handler);
   TR::IlValue *value = impl()->MulWithOverflow(handlerArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(handlerImpl, handler);
   return value;
   }

/* TODO: should have a WithZeroCheck variant here */
TR::IlValue *
OMR::IlBuilder::Div(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Div(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::And(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->And(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::Or(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Or(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::Xor(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->Xor(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::ShiftL(TR::IlValue *v, TR::IlValue *amount)
   {
   return impl()->ShiftL(v->impl(), amount->impl());
   }

TR::IlValue *
OMR::IlBuilder::ShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   return impl()->ShiftR(v->impl(), amount->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   return impl()->UnsignedShiftR(v->impl(), amount->impl());
   }

TR::IlValue *
OMR::IlBuilder::NotEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->NotEqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::EqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->EqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::LessThan(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->LessThan(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedLessThan(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->UnsignedLessThan(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::LessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->LessOrEqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedLessOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->UnsignedLessOrEqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::GreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->GreaterThan(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedGreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->UnsignedGreaterThan(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::GreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->GreaterOrEqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedGreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   return impl()->UnsignedGreaterOrEqualTo(left->impl(), right->impl());
   }

TR::IlValue *
OMR::IlBuilder::ConvertTo(TR::IlType *t, TR::IlValue *v)
   {
   return impl()->ConvertTo(t->impl(), v->impl());
   }

TR::IlValue *
OMR::IlBuilder::UnsignedConvertTo(TR::IlType *t, TR::IlValue *v)
   {
   return impl()->UnsignedConvertTo(t->impl(), v->impl());
   }


// memory operations
TR::IlValue *
OMR::IlBuilder::CreateLocalArray(int32_t numElements, TR::IlType *elementType)
  {
  return impl()->CreateLocalArray(numElements, elementType->impl());
  }

TR::IlValue *
OMR::IlBuilder::CreateLocalStruct(TR::IlType *structType)
  {
  return impl()->CreateLocalStruct(structType->impl());
  }

TR::IlValue *
OMR::IlBuilder::Load(const char *name)
  {
  return impl()->Load(name);
  }

void 
OMR::IlBuilder::Store(const char *name, TR::IlValue *value)
  {
  impl()->Store(name, value->impl());
  }

void
OMR::IlBuilder::StoreOver(TR::IlValue *dest, TR::IlValue *value)
  {
  impl()->StoreOver(dest->impl(), value->impl());
  }

TR::IlValue *
OMR::IlBuilder::LoadAt(TR::IlType *dt, TR::IlValue *address)
  {
  return impl()->LoadAt(dt->impl(), address->impl());
  }

void 
OMR::IlBuilder::StoreAt(TR::IlValue *address, TR::IlValue *value)
  {
  impl()->StoreAt(address->impl(), value->impl());
  }

TR::IlValue *
OMR::IlBuilder::LoadIndirect(const char *type, const char *field, TR::IlValue *object)
  {
  return impl()->LoadIndirect(type, field, object->impl());
  }

void 
OMR::IlBuilder::StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value)
  {
  impl()->StoreIndirect(type, field, object->impl(), value->impl());
  }

TR::IlValue *
OMR::IlBuilder::IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index)
  {
  return impl()->IndexAt(dt->impl(), base->impl(), index->impl());
  }

/** \brief
 *     The service is used to atomically increase memory location \p baseAddress by amount of \p value. 
 *
 *  \param value
 *     The amount to increase for the memory location.
 *
 *  \note
 *     This service currently only supports Int32/Int64. 
 *
 *  \return
 *     The old value at the location \p baseAddress.
 */

TR::IlValue *
OMR::IlBuilder::AtomicAdd(TR::IlValue *baseAddress, TR::IlValue * value)
  {
  return impl()->AtomicAdd(baseAddress->impl(), value->impl());
  }

/**
 * \brief
 *  The service is for generating a treetop for transaction begin when the user needs to use transactional memory.
 *  At the end of transaction path, service will automatically generate transaction end instruction.
 *
 * \verbatim
 *   
 *    +---------------------------------+
 *    |<block_$tstart>                  |
 *    |==============                   |
 *    | TSTART                          |
 *    |    |                            |
 *    |    |_ <block_$persistentFailure>|
 *    |    |_ <block_$transientFailure> |
 *    |    |_ <block_$transaction>      |+------------------------+----------------------------------+
 *    |                                 |                         |                                  |
 *    +---------------------------------+                         |                                  |
 *                       |                                        |                                  |
 *                       v                                        V                                  V
 *    +-----------------------------------------+   +-------------------------------+    +-------------------------+     
 *    |<block_$transaction>                     |   |<block_$persistentFailure>     |    |<block_$transientFailure>|      
 *    |====================                     |   |===========================    |    |=========================|
 *    |     add (For illustration, we take add  |   |AtomicAdd (For illustration, we|    |   ...                   |
 *    |     ... as an example. User could       |   |...       take atomicAdd as an |    |   ...                   |
 *    |     ... replace it with other non-atomic|   |...       example. User could  |    |   ...                   |
 *    |     ... operations.)                    |   |...       replace it with other|    |   ...                   |
 *    |     ...                                 |   |...       atomic operations.)  |    |   ...                   |
 *    |     TEND                                |   |...                            |    |   ...                   |
 *    |goto --> block_$merge                    |   |goto->block_$merge             |    |goto->block_$merge       |
 *    +----------------------------------------+    +-------------------------------+    +-------------------------+
 *                      |                                          |                                 |
 *                      |                                          |                                 |
 *                      |------------------------------------------+---------------------------------+
 *                      |
 *                      v             
 *              +----------------+     
 *              | block_$merge   |     
 *              | ============== |   
 *              |      ...       |
 *              +----------------+   
 *
 * \endverbatim 
 *
 * \structure & \param value
 *     tstart
 *       |
 *       ----persistentFailure // This is a permanent failure, try atomic way to do it instead
 *       |
 *       ----transientFailure // Temporary failure, user can retry 
 *       |
 *       ----transaction // Success, use general(non-atomic) way to do it
 *                |
 *                ---- non-atomic operations
 *                |
 *                ---- ...
 *                |
 *                ---- tend
 *
 * \note
 *      If user's platform doesn't support TM, go to persistentFailure path directly/
 *      In this case, current IlBuilder walks around transientFailureBuilder and transactionBuilder
 *      and goes to persistentFailureBuilder.
 *      
 *      _currentBuilder
 *          |
 *          ->Goto()
 *              |   transientFailureBuilder 
 *              |   transactionBuilder
 *              |-->persistentFailurie
 */
void 
OMR::IlBuilder::Transaction(TR::IlBuilder **persistentFailureBuilder,
                            TR::IlBuilder **transientFailureBuilder,
                            TR::IlBuilder **fallThroughBuilder)
  {
   ILBUILDER_PARM_SETUP(persistentFailureBuilderImpl, persistentFailureBuilderArg, persistentFailureBuilder);
   ILBUILDER_PARM_SETUP(transientFailureBuilderImpl, transientFailureBuilderArg, transientFailureBuilder);
   ILBUILDER_PARM_SETUP(fallThroughBuilderImpl, fallThroughBuilderArg, fallThroughBuilder);
   impl()->Transaction(persistentFailureBuilderArg, transientFailureBuilderArg, fallThroughBuilderArg);
   ILBUILDER_PARM_RETURN(persistentFailureBuilderImpl, persistentFailureBuilder);
   ILBUILDER_PARM_RETURN(transientFailureBuilderImpl, transientFailureBuilder);
   ILBUILDER_PARM_RETURN(fallThroughBuilderImpl, fallThroughBuilder);
  }

void 
OMR::IlBuilder::TransactionAbort()
  {
  impl()->TransactionAbort();
  }


TR::IlValue *
OMR::IlBuilder::StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValue* obj)
  {
  return impl()->StructFieldInstanceAddress(structName, fieldName, obj->impl());
  }

TR::IlValue *
OMR::IlBuilder::UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValue* obj)
  {
  return impl()->UnionFieldInstanceAddress(unionName, fieldName, obj->impl());
  }

// vector memory
TR::IlValue *
OMR::IlBuilder::VectorLoad(const char *name)
   {
   return impl()->VectorLoad(name);
   }

TR::IlValue *
OMR::IlBuilder::VectorLoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   return impl()->VectorLoadAt(dt->impl(), address->impl());
   }

void
OMR::IlBuilder::VectorStore(const char *name, TR::IlValue *value)
   {
   impl()->VectorStore(name, value->impl());
   }

void
OMR::IlBuilder::VectorStoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   impl()->VectorStoreAt(address->impl(), value->impl());
   }


// control
void
OMR::IlBuilder::AppendBuilder(TR::IlBuilder *builder)
   {
   impl()->AppendBuilder(builder->impl());
   }

TR::IlValue *
OMR::IlBuilder::Call(const char *name, int32_t numArgs, ...)
   {
   va_list args;
   va_start(args, numArgs);
   TR::IlValue ** argValues = new TR::IlValue *[numArgs];
   for (int32_t a=0;a < numArgs;a++)
      {
      argValues[a] = va_arg(args, TR::IlValue *);
      }
   va_end(args);
   TR::IlValue *value = Call(name, numArgs, argValues);
   delete[] argValues;
   return value;
   }

TR::IlValue *
OMR::IlBuilder::Call(const char *name, int32_t numArgs, TR::IlValue **argValues)
   {
   return impl()->Call(name, numArgs, reinterpret_cast<TR::IlValueImpl **>(argValues));
   }

TR::IlValue *
OMR::IlBuilder::ComputedCall(const char *name, int32_t numArgs, ...)
   {
   va_list args;
   va_start(args, numArgs);
   TR::IlValue ** argValues = new TR::IlValue *[numArgs];
   for (int32_t a=0;a < numArgs;a++)
      {
      argValues[a] = va_arg(args, TR::IlValue *);
      }
   va_end(args);
   TR::IlValue *value = ComputedCall(name, numArgs, argValues);
   delete[] argValues;
   return value;
   }

TR::IlValue *
OMR::IlBuilder::ComputedCall(const char *name, int32_t numArgs, TR::IlValue **args)
   {
   return impl()->ComputedCall(name, numArgs, reinterpret_cast<TR::IlValueImpl **>(args));
   }

void
OMR::IlBuilder::Goto(TR::IlBuilder **dest)
   {
   ILBUILDER_PARM_SETUP(destImpl, destArg, dest);
   impl()->Goto(destArg);
   ILBUILDER_PARM_RETURN(destImpl, dest);
   }

void
OMR::IlBuilder::Goto(TR::IlBuilder *dest)
   {
   impl()->Goto(dest->impl());
   }

void
OMR::IlBuilder::Return()
   {
   impl()->Return();
   }

void
OMR::IlBuilder::Return(TR::IlValue *value)
   {
   impl()->Return(value->impl());
   }

void
OMR::IlBuilder::ForLoop(bool countsUp,
                        const char *indVar,
                        TR::IlBuilder **body,
                        TR::IlBuilder **breakBuilder,
                        TR::IlBuilder **continueBuilder,
                        TR::IlValue *initial,
                        TR::IlValue *iterateWhile,
                        TR::IlValue *increment)
   {
   ILBUILDER_PARM_SETUP(bodyImpl, bodyArg, body);
   ILBUILDER_PARM_SETUP(breakBuilderImpl, breakBuilderArg, breakBuilder);
   ILBUILDER_PARM_SETUP(continueBuilderImpl, continueBuilderArg, continueBuilder);
   impl()->ForLoop(countsUp, indVar, bodyArg, breakBuilderArg, continueBuilderArg, initial->impl(), iterateWhile->impl(), increment->impl());
   ILBUILDER_PARM_RETURN(bodyImpl, body);
   ILBUILDER_PARM_RETURN(breakBuilderImpl, breakBuilder);
   ILBUILDER_PARM_RETURN(continueBuilderImpl, continueBuilder);
   }

void
OMR::IlBuilder::WhileDoLoop(const char *exitCondition,
                            TR::IlBuilder **body,
                            TR::IlBuilder **breakBuilder,
                            TR::IlBuilder **continueBuilder)
   {
   ILBUILDER_PARM_SETUP(bodyImpl, bodyArg, body);
   ILBUILDER_PARM_SETUP(breakBuilderImpl, breakBuilderArg, breakBuilder);
   ILBUILDER_PARM_SETUP(continueBuilderImpl, continueBuilderArg, continueBuilder);
   impl()->WhileDoLoop(exitCondition, bodyArg, breakBuilderArg, continueBuilderArg);
   ILBUILDER_PARM_RETURN(bodyImpl, body);
   ILBUILDER_PARM_RETURN(breakBuilderImpl, breakBuilder);
   ILBUILDER_PARM_RETURN(continueBuilderImpl, continueBuilder);
   }

void
OMR::IlBuilder::DoWhileLoop(const char *exitCondition,
                            TR::IlBuilder **body,
                            TR::IlBuilder **breakBuilder,
                            TR::IlBuilder **continueBuilder)
   {
   ILBUILDER_PARM_SETUP(bodyImpl, bodyArg, body);
   ILBUILDER_PARM_SETUP(breakBuilderImpl, breakBuilderArg, breakBuilder);
   ILBUILDER_PARM_SETUP(continueBuilderImpl, continueBuilderArg, continueBuilder);
   impl()->DoWhileLoop(exitCondition, bodyArg, breakBuilderArg, continueBuilderArg);
   ILBUILDER_PARM_RETURN(bodyImpl, body);
   ILBUILDER_PARM_RETURN(breakBuilderImpl, breakBuilder);
   ILBUILDER_PARM_RETURN(continueBuilderImpl, continueBuilder);
   }

void
OMR::IlBuilder::IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpNotEqualZero(targetArg, condition->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpNotEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   impl()->IfCmpNotEqualZero(target->impl(), condition->impl());
   }

void
OMR::IlBuilder::IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpNotEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpNotEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpNotEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpEqualZero(targetArg, condition->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpEqualZero(TR::IlBuilder *target, TR::IlValue *condition)
   {
   impl()->IfCmpEqualZero(target->impl(), condition->impl());
   }

void
OMR::IlBuilder::IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpLessThan(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpLessThan(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpUnsignedLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpUnsignedLessThan(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpUnsignedLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpUnsignedLessThan(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpLessOrEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpLessOrEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpUnsignedLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpUnsignedLessOrEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpUnsignedLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpUnsignedLessOrEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpGreaterThan(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpGreaterThan(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpUnsignedGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpUnsignedGreaterThan(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpUnsignedGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpUnsignedGreaterThan(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpGreaterOrEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpGreaterOrEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfCmpUnsignedGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   ILBUILDER_PARM_SETUP(targetImpl, targetArg, target);
   impl()->IfCmpUnsignedGreaterOrEqual(targetArg, left->impl(), right->impl());
   ILBUILDER_PARM_RETURN(targetImpl, target);
   }

void
OMR::IlBuilder::IfCmpUnsignedGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right)
   {
   impl()->IfCmpUnsignedGreaterOrEqual(target->impl(), left->impl(), right->impl());
   }

void
OMR::IlBuilder::IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ... )
   {
   ILBUILDER_PARM_SETUP(allTrueBuilderImpl, allTrueBuilderArg, allTrueBuilder);
   ILBUILDER_PARM_SETUP(anyFalseBuilderImpl, anyFalseBuilderArg, anyFalseBuilder);

   va_list terms;
   va_start(terms, numTerms);
   TR::IlBuilderImpl ** termBuilders = new TR::IlBuilderImpl *[numTerms >> 1];
   TR::IlValueImpl ** termValues = new TR::IlValueImpl *[numTerms >> 1];
   for (int32_t t=0;t < numTerms;t++)
      {
      termBuilders[t] = (va_arg(terms, TR::IlBuilder *))->impl();
      termValues[t] = (va_arg(terms, TR::IlValue *))->impl();
      }
   va_end(terms);

   impl()->IfAnd(allTrueBuilderArg, anyFalseBuilderArg, numTerms, termBuilders, termValues);

   delete[] termBuilders;
   delete[] termValues;

   ILBUILDER_PARM_RETURN(allTrueBuilderImpl, allTrueBuilder);
   ILBUILDER_PARM_RETURN(anyFalseBuilderImpl, anyFalseBuilder);
   }

void
OMR::IlBuilder::IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ... )
   {
   ILBUILDER_PARM_SETUP(anyTrueBuilderImpl, anyTrueBuilderArg, anyTrueBuilder);
   ILBUILDER_PARM_SETUP(allFalseBuilderImpl, allFalseBuilderArg, allFalseBuilder);

   va_list terms;
   va_start(terms, numTerms);
   TR::IlBuilderImpl ** termBuilders = new TR::IlBuilderImpl *[numTerms >> 1];
   TR::IlValueImpl ** termValues = new TR::IlValueImpl *[numTerms >> 1];
   for (int32_t t=0;t < numTerms;t++)
      {
      termBuilders[t] = (va_arg(terms, TR::IlBuilder *))->impl();
      termValues[t] = (va_arg(terms, TR::IlValue *))->impl();
      }
   va_end(terms);

   impl()->IfOr(anyTrueBuilderArg, allFalseBuilderArg, numTerms, termBuilders, termValues);

   delete[] termBuilders;
   delete[] termValues;

   ILBUILDER_PARM_RETURN(anyTrueBuilderImpl, anyTrueBuilder);
   ILBUILDER_PARM_RETURN(allFalseBuilderImpl, allFalseBuilder);
   }

void
OMR::IlBuilder::IfThenElse(TR::IlBuilder **thenPath,
                           TR::IlBuilder **elsePath,
                           TR::IlValue *condition)
   {
   ILBUILDER_PARM_SETUP(thenPathImpl, thenPathArg, thenPath);
   ILBUILDER_PARM_SETUP(elsePathImpl, elsePathArg, elsePath);
   impl()->IfThenElse(thenPathArg, elsePathArg, condition->impl());
   ILBUILDER_PARM_RETURN(thenPathImpl, thenPath);
   ILBUILDER_PARM_RETURN(elsePathImpl, elsePath);
   }

void
OMR::IlBuilder::Switch(const char *selectionVar,
                       TR::IlBuilder **defaultBuilder,
                       uint32_t numCases,
                       ...)
   {
   int32_t *caseValues = new int32_t[numCases];

   // caseBuilders remembers the double pointers passed in, so we don't have to do another va_list iteration afterwards
   TR::IlBuilder ***caseBuilders = new TR::IlBuilder **[numCases];

   TR::IlBuilder **caseBuilderArray = new TR::IlBuilder *[numCases];
   bool *caseFallsThrough = new bool[numCases];

   va_list cases;
   va_start(cases, numCases);
   for (int32_t c=0;c < numCases;c++)
      {
      caseValues[c] = va_arg(cases, int32_t);
      caseBuilders[c] = va_arg(cases, TR::IlBuilder **);

      caseBuilderArray[c] = *caseBuilders[c];
      caseFallsThrough[c] = (va_arg(cases, int32_t) != 0);
      }
   va_end(cases);

   Switch(selectionVar, defaultBuilder, numCases, caseValues, caseBuilderArray, caseFallsThrough);

   // if any of the entries in caseBuilderArray changed, update the corresponding parameter
   for (int32_t c=0;c < numCases;c++)
      {
      *(caseBuilders[c]) = caseBuilderArray[c];
      }

   delete[] caseFallsThrough;
   delete[] caseBuilders;
   delete[] caseValues;
   }

void
OMR::IlBuilder::Switch(const char *selectionVar,
                       TR::IlBuilder **defaultBuilder,
                       uint32_t numCases,
                       int32_t *caseValues,
                       TR::IlBuilder **caseBuilders,
                       bool *caseFallsThrough)
   {
   ILBUILDER_PARM_SETUP(defaultBuilderImpl, defaultBuilderArg, defaultBuilder);
   TR::IlBuilderImpl **  caseBuilderImpls = new TR::IlBuilderImpl *[numCases];
   for (int32_t c=0;c < numCases;c++)
      {
      if (caseBuilders[c] == NULL)
         caseBuilderImpls[c] = NULL;
      else
         caseBuilderImpls[c] = caseBuilders[c]->impl();
      }

   impl()->Switch(selectionVar, defaultBuilderArg, numCases, caseValues, caseBuilderImpls, caseFallsThrough);

   // if any builders were changed, update their clients in caseBuilders
   for (int32_t c=0;c < numCases;c++)
      {
      caseBuilders[c] = caseBuilderImpls[c]->client();
      }

   delete[] caseBuilderImpls;
   ILBUILDER_PARM_RETURN(defaultBuilderImpl, defaultBuilder);
   }
