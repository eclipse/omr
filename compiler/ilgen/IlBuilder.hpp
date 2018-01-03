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

#ifndef OMR_ILBUILDER_INCL
#define OMR_ILBUILDER_INCL

#ifndef TR_ILBUILDER_DEFINED
#define TR_ILBUILDER_DEFINED
#define PUT_OMR_ILBUILDER_INTO_TR
#endif // !defined(TR_ILBUILDER_DEFINED)

#include <cstddef>
#include <stdint.h>

#define TOSTR(x)     #x
#define LINETOSTR(x) TOSTR(x)

namespace OMR { class IlBuilderImpl; }

namespace TR { class IlBuilder; }
namespace TR { class IlBuilderImpl; }
namespace TR { class IlType; }
namespace TR { class IlValue; }
namespace TR { class MethodBuilder; }
namespace TR { class TypeDictionary; }
namespace TR { class VirtualMachineRegisterInStruct; }

namespace OMR
{

/*
 * IlBuilder Client API
 * TODO: put class documentation here
 * Constructors/Destructors at the top, other API grouped in categories, alphabetical in each category
 * All implementations, with the exception of constructors, are simply
 * delegations to the IlBuilderImpl object stored in _impl . There
 * is a 1-1 relationship between IlBuilder and IlBuilderImpl objects.
 * All IlBuilder objects are associated with their containing MethodBuilder
 * object and will be deleted when the MethodBuilder object is destructed.
 * IlBuilder objects should be allocated via OrphanBuilder(), so there are
 * no public constructors for this class.
 */

class IlBuilder
   {
   friend class MethodBuilder;
   friend class OMR::IlBuilderImpl;
   friend class VirtualMachineRegisterInStruct;

   public:
   /*
    * @brief public constructor to be used by client subclasses of IlBuilder
    * Normally OrphanBuilder() is used to allocate IlBuilder objects.
    */
   IlBuilder(TR::MethodBuilder *mb, TR::TypeDictionary *types);

   protected:
   /*
    * @brief constructor used by other JitBuilder classes (e.g. MethodBuilder)
    */
   IlBuilder(TR::IlBuilderImpl *impl, TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types);

   public:
   virtual ~IlBuilder();

   virtual bool buildIl()
      { return false; }

   // misc
   TR::IlValue *Copy(TR::IlValue *value);
   TR::IlBuilder *OrphanBuilder();

   // constants
   TR::IlValue *ConstAddress(const void * const value);
   TR::IlValue *ConstDouble(double value);
   TR::IlValue *ConstFloat(float value);
   TR::IlValue *ConstInteger(TR::IlType *intType, int64_t value);
   TR::IlValue *ConstInt8(int8_t value);
   TR::IlValue *ConstInt16(int16_t value);
   TR::IlValue *ConstInt32(int32_t value);
   TR::IlValue *ConstInt64(int64_t value);
   TR::IlValue *ConstString(const char * const value);

   TR::IlValue *Const(const void * const value)
      { return ConstAddress(value); }
   TR::IlValue *Const(double value)
      { return ConstDouble(value); }
   TR::IlValue *Const(float value)
      { return ConstFloat(value); }
   TR::IlValue *Const(int8_t value)
      { return ConstInt8(value); }
   TR::IlValue *Const(int16_t value)
      { return ConstInt16(value); }
   TR::IlValue *Const(int32_t value)
      { return ConstInt32(value); }
   TR::IlValue *Const(int64_t value)
      { return ConstInt64(value); }
   TR::IlValue *NullAddress();

   // arithmetic
   TR::IlValue *Add(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *AddWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *And(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Div(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index);
   TR::IlValue *Mul(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Or(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *ShiftL(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *ShiftL(TR::IlValue *v, int8_t amount)
      { return ShiftL(v, ConstInt8(amount)); }
   TR::IlValue *ShiftR(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *ShiftR(TR::IlValue *v, int8_t amount)
      { return ShiftR(v, ConstInt8(amount)); }
   TR::IlValue *Sub(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *SubWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *UnsignedShiftR(TR::IlValue *v, int8_t amount)
      { return UnsignedShiftR(v, ConstInt8(amount)); }
   TR::IlValue *Xor(TR::IlValue *left, TR::IlValue *right);

   // compares
   TR::IlValue *EqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *LessOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *LessThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *GreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *GreaterThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *NotEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedLessOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedLessThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedGreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedGreaterThan(TR::IlValue *left, TR::IlValue *right);

   // conversions
   TR::IlValue *ConvertTo(TR::IlType *t, TR::IlValue *v);
   TR::IlValue *UnsignedConvertTo(TR::IlType *t, TR::IlValue *v);

   // memory
   TR::IlValue *AtomicAdd(TR::IlValue *baseAddress, TR::IlValue  *value);
   TR::IlValue *CreateLocalArray(int32_t numElements, TR::IlType *elementType);
   TR::IlValue *CreateLocalStruct(TR::IlType *structType);
   TR::IlValue *Load(const char *name);
   TR::IlValue *LoadAt(TR::IlType *dt, TR::IlValue *address);
   TR::IlValue *LoadIndirect(const char *type, const char *field, TR::IlValue *object);
   void Store(const char *name, TR::IlValue *value);
   void StoreAt(TR::IlValue *address, TR::IlValue *value);
   void StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value);
   void StoreOver(TR::IlValue *dest, TR::IlValue *value);
   void Transaction(TR::IlBuilder **persistentFailureBuilder, TR::IlBuilder **transientFailureBuilder, TR::IlBuilder **fallThroughBuilder);
   void TransactionAbort();

   /**
    * `StructFieldAddress` and `UnionFieldAddress` are two functions that
    * provide a workaround for a limitation in JitBuilder. Becuase `TR::IlValue`
    * cannot represent an object type (only pointers to objects), there is no
    * way to generate a load for a field that is itself an object. The workaround
    * is to use the field's address instead. This is not an elegent solution and
    * should be revisited.
    */
   TR::IlValue *StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValue *obj);
   TR::IlValue *UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValue *obj);

   // vector memory
   TR::IlValue *VectorLoad(const char *name);
   TR::IlValue *VectorLoadAt(TR::IlType *dt, TR::IlValue *address);
   void VectorStore(const char *name, TR::IlValue *value);
   void VectorStoreAt(TR::IlValue *address, TR::IlValue *value);

   // control
   void AppendBuilder(TR::IlBuilder *builder);
   TR::IlValue *Call(const char *name, int32_t numArgs, ...);
   TR::IlValue *Call(const char *name, int32_t numArgs, TR::IlValue **argValues);
   TR::IlValue *ComputedCall(const char *name, int32_t numArgs, ...);
   TR::IlValue *ComputedCall(const char *name, int32_t numArgs, TR::IlValue **args);
   void DoWhileLoop(const char *exitCondition,
                    TR::IlBuilder **body,
                    TR::IlBuilder **breakBuilder = NULL,
                    TR::IlBuilder **continueBuilder = NULL);
   void DoWhileLoopWithBreak(const char *exitCondition,
                             TR::IlBuilder **body,
                             TR::IlBuilder **breakBuilder)
      { DoWhileLoop(exitCondition, body, breakBuilder); }
   void DoWhileLoopWithContinue(const char *exitCondition,
                                TR::IlBuilder **body,
                                TR::IlBuilder **continueBuilder)
      { DoWhileLoop(exitCondition, body, NULL, continueBuilder); }
   void Goto(TR::IlBuilder **dest);
   void Goto(TR::IlBuilder *dest);
   /* @brief creates an AND nest of short-circuited conditions, for each term pass an IlBuilder containing the condition and the IlValue that computes the condition */
   void IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ... );
   void IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpEqualZero(TR::IlBuilder *target, TR::IlValue *condition);
   void IfCmpLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpNotEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpNotEqualZero(TR::IlBuilder *target, TR::IlValue *condition);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   /* @brief creates an OR nest of short-circuited conditions, for each term pass an IlBuilder containing the condition and the IlValue that computes the condition */
   void IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ... );
   virtual void IfThen(TR::IlBuilder **thenPath, TR::IlValue *condition)
      { IfThenElse(thenPath, NULL, condition); }
   void IfThenElse(TR::IlBuilder **thenPath,
                   TR::IlBuilder **elsePath,
                   TR::IlValue *condition);
   void ForLoop(bool countsUp,
                const char *indVar,
                TR::IlBuilder **body,
                TR::IlBuilder **breakBuilder,
                TR::IlBuilder **continueBuilder,
                TR::IlValue *initial,
                TR::IlValue *iterateWhile,
                TR::IlValue *increment);
   void ForLoopDown(const char *indVar,
                    TR::IlBuilder **body,
                    TR::IlValue *initial,
                    TR::IlValue *iterateWhile,
                    TR::IlValue *increment)
      { ForLoop(false, indVar, body, NULL, NULL, initial, iterateWhile, increment); }
   void ForLoopUp(const char *indVar,
                  TR::IlBuilder **body,
                  TR::IlValue *initial,
                  TR::IlValue *iterateWhile,
                  TR::IlValue *increment)
      { ForLoop(true, indVar, body, NULL, NULL, initial, iterateWhile, increment); }
   void ForLoopWithBreak(bool countsUp,
                         const char *indVar,
                         TR::IlBuilder **body,
                         TR::IlBuilder **breakBody,
                         TR::IlValue *initial,
                         TR::IlValue *iterateWhile,
                         TR::IlValue *increment)
      { ForLoop(countsUp, indVar, body, breakBody, NULL, initial, iterateWhile, increment); }
   void ForLoopWithContinue(bool countsUp,
                            const char *indVar,
                            TR::IlBuilder **body,
                            TR::IlBuilder **continueBody,
                            TR::IlValue *initial,
                            TR::IlValue *iterateWhile,
                            TR::IlValue *increment)
      { ForLoop(countsUp, indVar, body, NULL, continueBody, initial, iterateWhile, increment); }
   void Return();
   void Return(TR::IlValue *value);
   void Switch(const char *selectionVar,
               TR::IlBuilder **defaultBuilder,
               uint32_t numCases,
               int32_t *caseValues,
               TR::IlBuilder **caseBuilders,
               bool *caseFallsThrough);
   void Switch(const char *selectionVar,
               TR::IlBuilder **defaultBuilder,
               uint32_t numCases,
               ...);
   void WhileDoLoop(const char *exitCondition,
                    TR::IlBuilder **body,
                    TR::IlBuilder **breakBuilder = NULL,
                    TR::IlBuilder **continueBuilder = NULL);
   void WhileDoLoopWithBreak(const char *exitCondition,
                             TR::IlBuilder **body,
                             TR::IlBuilder **breakBuilder)
      { WhileDoLoop(exitCondition, body, breakBuilder); }
   void WhileDoLoopWithContinue(const char *exitCondition,
                                TR::IlBuilder **body,
                                TR::IlBuilder **continueBuilder)
      { WhileDoLoop(exitCondition, body, NULL, continueBuilder); }

   protected:

   /**
    * @brief Access to this IlBuilder's implementation object.
    *  Subclasses will implement impl() but return more derived types,
    *  so impl() is purposefully not virtual.
    */
   TR::IlBuilderImpl *impl()
      { return _impl; }

   /**
    * @brief return MethodBuilder that owns this IlBuilder object
    */
   TR::MethodBuilder *methodBuilder()
      { return _mb; }

   /**
    * @brief creates an IlBuilder object to wrap an implementation object
    */
   TR::IlBuilder *orphanBuilder(TR::IlBuilderImpl *bi);

   /*
    * @brief MethodBuilder client object that "owns" this IlBuilder object
    * Used primarily to track IlBuilder object allocations at the MethodBuilder
    */
   TR::MethodBuilder *_mb;

   /*
    * @brief implementation object corresponding to this client object (1-1 mapping)
    */
   TR::IlBuilderImpl *_impl;

   /**
    * @brief Convenience pointers for referring to primitive types
    */
   TR::IlType * NoType;
   TR::IlType * Int8;
   TR::IlType * Int16;
   TR::IlType * Int32;
   TR::IlType * Int64;
   TR::IlType * Word;
   TR::IlType * Float;
   TR::IlType * Double;
   TR::IlType * Address;
   TR::IlType * VectorInt8;
   TR::IlType * VectorInt16;
   TR::IlType * VectorInt32;
   TR::IlType * VectorInt64;
   TR::IlType * VectorFloat;
   TR::IlType * VectorDouble;

   };

} // namespace OMR



#if defined(PUT_OMR_ILBUILDER_INTO_TR)

namespace TR
{
   class IlBuilder : public OMR::IlBuilder
      {
      public:
         IlBuilder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
            : OMR::IlBuilder(methodBuilder, types)
            { }

         IlBuilder(TR::IlBuilderImpl *impl, TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
            : OMR::IlBuilder(impl, methodBuilder, types)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILBUILDER_INTO_TR)

#endif // !defined(OMR_ILBUILDER_INCL)
