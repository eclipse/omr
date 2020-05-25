/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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

#ifndef OMR_ILBUILDER_INCL
#define OMR_ILBUILDER_INCL

#include <fstream>
#include <stdarg.h>
#include <string.h>
#include "ilgen/IlInjector.hpp"
#include "il/ILHelpers.hpp"

#include "ilgen/IlValue.hpp" // must go after IlInjector.hpp or TR_ALLOC isn't cleaned up

namespace OMR { class MethodBuilder; }

namespace TR { class Block; }
namespace TR { class BytecodeBuilder; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlBuilder; }
namespace TR { class ResolvedMethodSymbol; } 
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }
namespace TR { class VirtualMachineState; }

namespace TR { class IlType; }
namespace TR { class TypeDictionary; }

template <class T> class List;
template <class T> class ListAppender;

extern "C"
{
typedef bool (*ClientBuildILCallback)(void *clientObject);
typedef void * (*ClientAllocator)(void *implObject);
typedef void * (*ImplGetter)(void *client);
}

namespace OMR
{

typedef TR::ILOpCodes (*OpCodeMapper)(TR::DataType);

class IlBuilder : public TR::IlInjector
   {

protected:
   struct SequenceEntry
      {
      TR_ALLOC(TR_Memory::IlGenerator)

      SequenceEntry(TR::Block *block)
         {
         _isBlock = true;
         _block = block;
         }
      SequenceEntry(TR::IlBuilder *builder)
         {
         _isBlock = false;
         _builder = builder;
         }

      bool _isBlock;
      union
         {
         TR::Block *_block;
         TR::IlBuilder *_builder;
         };
      };

   SequenceEntry *blockEntry(TR::Block *block);
   SequenceEntry *builderEntry(TR::IlBuilder *builder);

public:
   TR_ALLOC(TR_Memory::IlGenerator)

   /**
    * @brief A class encapsulating the information needed for a switch-case
    *
    * This class encapsulates the different pieces needed to construct a Case
    * for IlBuilder's Switch() service. It's constructor is private, so instances
    * can only be created by calling IlBuilder::MakeCase().
    */
   class JBCase
      {
      public:
         void * client();
         void setClient(void * client) { _client = client; }
         static void setClientAllocator(ClientAllocator allocator) { _clientAllocator = allocator; }
         static void setGetImpl(ImplGetter getter) { _getImpl = getter; }

         /**
          * @brief Construct a new JBCase object.
          *
          * This constructor should not be called directly outside of this classs.
          * A call to `MakeCase()` should be used instead.
          *
          * @param v the value matched by the case
          * @param b the builder implementing the case body
          * @param f whether the case falls-through or not
          */
         JBCase(int32_t v, TR::IlBuilder *b, int32_t f)
             : _value(v), _builder(b), _fallsThrough(f), _client(NULL) {}

      private:
         int32_t _value;          // value matched by the case
         TR::IlBuilder *_builder; // builder for the case body
         int32_t _fallsThrough;   // whether the case falls-through
         void * _client;
         static ClientAllocator _clientAllocator;
         static ImplGetter _getImpl;

         friend class OMR::IlBuilder;
      };

   /**
    * @brief A class encapsulating the information needed for IfAnd and IfOr.
    *
    * This class encapsulates the value of the condition and the builder
    * object used generate the value (used to evaluate the condition).
    */
   class JBCondition
      {
      public:
         void * client();
         void setClient(void * client) { _client = client; }
         static void setClientAllocator(ClientAllocator allocator) { _clientAllocator = allocator; }
         static void setGetImpl(ImplGetter getter) { _getImpl = getter; }

         /**
          * @brief Construct a new JBCondition object.
          *
          * This constructor should not be called directly outside of the JitBuilder
          * implementation. A call to `MakeCondition()` should be used instead.
          *
          * @param conditionBuilder pointer to the builder used to generate the condition value
          * @param conditionValue the IlValue representing value for the condition
          */
         JBCondition(TR::IlBuilder *conditionBuilder, TR::IlValue *conditionValue)
            : _builder(conditionBuilder), _condition(conditionValue), _client(NULL) {}

      private:
         TR::IlBuilder *_builder; // builder used to generate the condition value
         TR::IlValue *_condition; // value for the condition
         void * _client;
         static ClientAllocator _clientAllocator;
         static ImplGetter _getImpl;

         friend class OMR::IlBuilder;
      };

   friend class OMR::MethodBuilder;

   IlBuilder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
      : TR::IlInjector(types),
      _client(0),
      _clientCallbackBuildIL(0),
      _methodBuilder(methodBuilder),
      _sequence(0),
      _sequenceAppender(0),
      _entryBlock(0),
      _exitBlock(0),
      _count(-1),
      _partOfSequence(false),
      _connectedTrees(false),
      _comesBack(true),
      _isHandler(false)
      {
      }
   IlBuilder(TR::IlBuilder *source);
   virtual ~IlBuilder() { }

   void initSequence();

   virtual bool injectIL();
   virtual void setupForBuildIL();

   virtual bool isMethodBuilder()               { return false; }
   virtual TR::MethodBuilder *asMethodBuilder() { return NULL; }

   virtual bool isBytecodeBuilder()             { return false; }

   virtual TR::VirtualMachineState *initialVMState()         { return NULL; }
   virtual TR::VirtualMachineState *vmState()                { return NULL; }
   virtual void setVMState(TR::VirtualMachineState *vmState) { }

   //char *getName();

   void print(const char *title, bool recurse=false);
   void printBlock(TR::Block *block);

   TR::TreeTop *getFirstTree();
   TR::TreeTop *getLastTree();
   TR::Block *getEntry() { return _entryBlock; }
   TR::Block *getExit()  { return _exitBlock; }

   void setDoesNotComeBack() { _comesBack = false; }
   void setComesBack()       { _comesBack = true; }
   bool comesBack()          { return _comesBack; }

   bool blocksHaveBeenCounted() { return _count > -1; }

   TR::IlBuilder *createBuilderIfNeeded(TR::IlBuilder *builder);
   TR::IlBuilder *OrphanBuilder();
   TR::BytecodeBuilder *OrphanBytecodeBuilder(int32_t bcIndex=0, char *name=NULL);

   bool TraceEnabled_log();
   void TraceIL_log(const char *s, ...);

   // create a new local value (temporary variable)
   TR::IlValue *NewValue(TR::IlType *dt);

   TR::IlValue *Copy(TR::IlValue *value);

   // constants
   TR::IlValue *NullAddress();
   TR::IlValue *ConstInt8(int8_t value);
   TR::IlValue *ConstInt16(int16_t value);
   TR::IlValue *ConstInt32(int32_t value);
   TR::IlValue *ConstInt64(int64_t value);
   TR::IlValue *ConstFloat(float value);
   TR::IlValue *ConstDouble(double value);
   TR::IlValue *ConstAddress(const void * const value);
   TR::IlValue *ConstString(const char * const value);

   TR::IlValue *Const(int8_t value)             { return ConstInt8(value); }
   TR::IlValue *Const(int16_t value)            { return ConstInt16(value); }
   TR::IlValue *Const(int32_t value)            { return ConstInt32(value); }
   TR::IlValue *Const(int64_t value)            { return ConstInt64(value); }
   TR::IlValue *Const(float value)              { return ConstFloat(value); }
   TR::IlValue *Const(double value)             { return ConstDouble(value); }
   TR::IlValue *Const(const void * const value) { return ConstAddress(value); }

   TR::IlValue *ConstInteger(TR::IlType *intType, int64_t value);

   // arithmetic
   TR::IlValue *Add(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *AddWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Sub(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *SubWithUnsignedOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Mul(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Div(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedDiv(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Rem(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedRem(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *And(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Or(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Xor(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *ShiftL(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *ShiftL(TR::IlValue *v, int8_t amount)                { return ShiftL(v, ConstInt8(amount)); }
   TR::IlValue *ShiftR(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *ShiftR(TR::IlValue *v, int8_t amount)                { return ShiftR(v, ConstInt8(amount)); }
   TR::IlValue *UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount);
   TR::IlValue *UnsignedShiftR(TR::IlValue *v, int8_t amount)        { return UnsignedShiftR(v, ConstInt8(amount)); }
   TR::IlValue *NotEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *EqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *LessThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedLessThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *LessOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedLessOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *GreaterThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedGreaterThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *GreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedGreaterOrEqualTo(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *ConvertTo(TR::IlType *t, TR::IlValue *v);
   TR::IlValue *UnsignedConvertTo(TR::IlType *t, TR::IlValue *v);
   TR::IlValue *Negate(TR::IlValue *v);

   /**
    * @brief Convert the bit representation of an IlValue to a given type
    * @param type is the target type of the conversion
    * @param value is the value to be converted
    * @return the IlValue converted to the specified type
    *
    * This service allows the bit representation of a floating-point
    * IlValue to be converted to an integer IlValue and vice versa.
    * The floating-point and integer types must be of the same bit-width.
    */
   TR::IlValue* ConvertBitsTo(TR::IlType* type, TR::IlValue* value);

   // memory
   TR::IlValue *CreateLocalArray(int32_t numElements, TR::IlType *elementType);
   TR::IlValue *CreateLocalStruct(TR::IlType *structType);
   TR::IlValue *Load(const char *name);
   void Store(const char *name, TR::IlValue *value);
   void StoreOver(TR::IlValue *dest, TR::IlValue *value);
   TR::IlValue *LoadAt(TR::IlType *dt, TR::IlValue *address);
   void StoreAt(TR::IlValue *address, TR::IlValue *value);
   TR::IlValue *LoadIndirect(const char *type, const char *field, TR::IlValue *object);
   void StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value);
   TR::IlValue *IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index);
   TR::IlValue *AtomicAdd(TR::IlValue *baseAddress, TR::IlValue * value);
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
   TR::IlValue *StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValue* obj);
   TR::IlValue *UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValue* obj);

   // vector memory
   TR::IlValue *VectorLoad(const char *name);
   TR::IlValue *VectorLoadAt(TR::IlType *dt, TR::IlValue *address);
   void VectorStore(const char *name, TR::IlValue *value);
   void VectorStoreAt(TR::IlValue *address, TR::IlValue *value);

   // control
   void AppendBuilder(TR::IlBuilder *builder);

   /**
    * @brief Call a function via a TR::MethodBuilder object, possibly inlining it
    * @param calleeMB the target function's TR::MethodBuilder object
    * @param numArgs the number of actual arguments for the method
    * @param ... arguments to pass to the function, provided as TR::IlValue pointers
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *Call(TR::MethodBuilder *calleeMB, int32_t numArgs, ...);

   /**
    * @brief Call a function via a TR::MethodBuilder object, possibly inlining it
    * @param calleeMB the target function's TR::MethodBuilder object
    * @param numArgs the number of actual arguments for the method
    * @param argValues array of arguments to pass to the function
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *Call(TR::MethodBuilder *calleeMB, int32_t numArgs, TR::IlValue **argValues);

   /**
    * @brief Call a function via its name using a list of arguments
    * @param name the name of the target function
    * @param numArgs the number of actual arguments for the method
    * @param ... arguments to pass to the function, provided as TR::IlValue pointers
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *Call(const char *name, int32_t numArgs, ...);

   /**
    * @brief Call a function via its name using an array of arguments
    * @param name the name of the target function
    * @param numArgs the number of actual arguments for the method
    * @param argValues array of arguments to pass to the function
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *Call(const char *name, int32_t numArgs, TR::IlValue **argValues);

   /**
    * @brief Call a function by address using a list of arguments (and providing a name)
    * @param name the name of the target function (used primarily in logs)
    * @param numArgs the number of actual arguments for the method
    * @param ... arguments to pass to the function, provided as TR::IlValue pointers, with the first argument providing the function address
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *ComputedCall(const char *name, int32_t numArgs, ...);

   /**
    * @brief Call a function by address using an array of arguments (and providing a name)
    * @param name the name of the target function
    * @param numArgs the number of actual arguments for the method
    * @param argValues array of arguments to pass to the function, with the first argument providing the function address
    * @returns the TR::IlValue corresponding to the called function's return value or NULL if return type is None
    */
   TR::IlValue *ComputedCall(const char *name, int32_t numArgs, TR::IlValue **args);

   TR::IlValue *genCall(TR::SymbolReference *methodSymRef, int32_t numArgs, TR::IlValue ** paramValues, bool isDirectCall = true);
   void Goto(TR::IlBuilder **dest);
   void Goto(TR::IlBuilder *dest);
   void Return();
   void Return(TR::IlValue *value);
   virtual void ForLoop(bool countsUp,
                const char *indVar,
                TR::IlBuilder **body,
                TR::IlBuilder **breakBuilder,
                TR::IlBuilder **continueBuilder,
                TR::IlValue *initial,
                TR::IlValue *iterateWhile,
                TR::IlValue *increment);

   void ForLoopUp(const char *indVar,
                  TR::IlBuilder **body,
                  TR::IlValue *initial,
                  TR::IlValue *iterateWhile,
                  TR::IlValue *increment)
      {
      ForLoop(true, indVar, body, NULL, NULL, initial, iterateWhile, increment);
      }

   void ForLoopDown(const char *indVar,
                    TR::IlBuilder **body,
                    TR::IlValue *initial,
                    TR::IlValue *iterateWhile,
                    TR::IlValue *increment)
      {
      ForLoop(false, indVar, body, NULL, NULL, initial, iterateWhile, increment);
      }

   void ForLoopWithBreak(bool countsUp,
                         const char *indVar,
                         TR::IlBuilder **body,
                         TR::IlBuilder **breakBody,
                         TR::IlValue *initial,
                         TR::IlValue *iterateWhile,
                         TR::IlValue *increment)
      {
      ForLoop(countsUp, indVar, body, breakBody, NULL, initial, iterateWhile, increment);
      }

   void ForLoopWithContinue(bool countsUp,
                            const char *indVar,
                            TR::IlBuilder **body,
                            TR::IlBuilder **continueBody,
                            TR::IlValue *initial,
                            TR::IlValue *iterateWhile,
                            TR::IlValue *increment)
      {
      ForLoop(countsUp, indVar, body, NULL, continueBody, initial, iterateWhile, increment);
      }

   virtual void WhileDoLoop(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder = NULL, TR::IlBuilder **continueBuilder = NULL);
   void WhileDoLoopWithBreak(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder)
      {
      WhileDoLoop(exitCondition, body, breakBuilder);
      }

   void WhileDoLoopWithContinue(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **continueBuilder)
      {
      WhileDoLoop(exitCondition, body, NULL, continueBuilder);
      }

   virtual void DoWhileLoop(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder = NULL, TR::IlBuilder **continueBuilder = NULL);
   void DoWhileLoopWithBreak(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder)
      {
      DoWhileLoop(exitCondition, body, breakBuilder);
      }
   void DoWhileLoopWithContinue(const char *exitCondition, TR::IlBuilder **body, TR::IlBuilder **continueBuilder)
      {
      DoWhileLoop(exitCondition, body, NULL, continueBuilder);
      }

   /* @brief creates an AND nest of short-circuited conditions, for each term pass a JBCondition instance */
   void IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, JBCondition **terms);
   void IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ... );
   /* @brief creates an OR nest of short-circuited conditions, for each term pass a JBCondition instance */
   void IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, JBCondition **terms);
   void IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ... );

   /**
    * @brief Construct an Instance of JBCondition.
    *
    * @param conditionBuilder the Builder used to generate the condition value
    * @param conditionValue the IlValue instance representing the condition value
    * @return JBCondition* pointer to the constructed JBCondition instance
    */
   JBCondition * MakeCondition(TR::IlBuilder *conditionBuilder, TR::IlValue *conditionValue);

   void IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpNotEqualZero(TR::IlBuilder *target, TR::IlValue *condition);
   void IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpNotEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpEqualZero(TR::IlBuilder *target, TR::IlValue *condition);
   void IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilder *target, TR::IlValue *left, TR::IlValue *right);

   void IfThenElse(TR::IlBuilder **thenPath,
                   TR::IlBuilder **elsePath,
                   TR::IlValue *condition);
   virtual void IfThen(TR::IlBuilder **thenPath, TR::IlValue *condition)
      {
      IfThenElse(thenPath, NULL, condition);
      }

   /**
    * @brief Generates a lookup switch-case control flow structure
    *
    * @param selectorValue the IlValue to switch on.
    * @param defaultBuilder the builder for the default case.
    * @param numCases the number of cases.
    * @param cases array of pointers to JBCase instances corresponding to each case.
    */
   void Switch(TR::IlValue *selectorValue,
               TR::IlBuilder **defaultBuilder,
               uint32_t numCases,
               JBCase** cases);

   /**
    * @brief Generates a lookup switch-case control flow structure (vararg overload).
    *
    * Instead of taking an array of pointers to JBCase instances, this overload
    * takes a pointer to each instance as a separate varargs argument.
    *
    * @param selectorValue the IlValue to switch on.
    * @param defaultBuilder the builder for the default case.
    * @param numCases the number of cases.
    * @param ... the list of pointers to JBCase instances corresponding to each case.
    */
   void Switch(TR::IlValue *selectorValue,
               TR::IlBuilder **defaultBuilder,
               uint32_t numCases,
               ...);

   /**
    * @brief Generates a table switch-case control flow structure.
    *
    * @param selectorValue the IlValue to switch on.
    * @param defaultBuilder the builder for the default case.
    * @param generateBoundsCheck generate the bounds check or not for the range of the cases
    * @param numCases the number of cases.
    * @param cases array of pointers to JBCase instances corresponding to each case.
    */
   void TableSwitch(TR::IlValue * selectorValue,
               TR::IlBuilder **defaultBuilder,
               bool generateBoundsCheck,
               uint32_t numCases,
               JBCase** cases);

   /**
    * @brief Generates a table switch-case control flow structure (IlValue and vararg overload).
    *
    * Instead of taking an array of pointers to JBCase instances, this overload
    * takes a pointer to each instance as a separate varargs argument.
    *
    * @param selectorValue the IlValue to switch on.
    * @param defaultBuilder the builder for the default case.
    * @param generateBoundsCheck generate the bounds check or not for the range of the cases
    * @param numCases the number of cases.
    * @param ... the list of pointers to JBCase instances corresponding to each case.
    */
   void TableSwitch(TR::IlValue * selectorValue,
               TR::IlBuilder **defaultBuilder,
               bool generateBoundsCheck,
               uint32_t numCases,
               ...);

   /**
    * @brief Construct an instance for JBCase.
    *
    * @param caseValue the value matched by the case.
    * @param caseBuilder pointer to pointer to IlBuilder for the case (automatically allocated if pointed-to pointer is null).
    * @param caseFallsThrough flag specifying whether the case falls-through to the next case.
    * @return JBCase* pointer to instance of JBCase representing the specified case.
    */
   JBCase * MakeCase(int32_t caseValue,
                     TR::IlBuilder **caseBuilder,
                     int32_t caseFallsThrough);

   // select
   /**
    * @brief Service to select a value based on a condition without branching
    *
    * @param condition the condition value
    * @param trueValue value to select if true
    * @param falseValue value to select if false
    * @return TR::IlValue* IlValue corresponding to the selected value
    */
   TR::IlValue * Select(TR::IlValue * condition, TR::IlValue * trueValue, TR::IlValue * falseValue);

   /**
    * @brief associates this object with a particular client object
    */
   void setClient(void *client)
      {
      _client = client;
      }

   /**
    * @brief returns the client object associated with this object, allocating it if necessary
    */
   void *client();

   /**
    * @brief Set the ClientCallback buildIL function
    * 
    * @param callback function pointer to the buildIL() callback for the client
    */
   void setClientCallback_buildIL(void *callback)
      {
      _clientCallbackBuildIL = (ClientBuildILCallback)callback;
      }

   /**
    * @brief Set the Client Allocator function
    * 
    * @param allocator function pointer to the client object allocator
    */
   static void setClientAllocator(ClientAllocator allocator)
      {
      _clientAllocator = allocator;
      }

   /**
    * @brief Set the Get Impl function
    *
    * @param getter function pointer to the impl getter
    */
   static void setGetImpl(ImplGetter getter)
      {
      _getImpl = getter;
      }

protected:

   /**
    * @brief pointer to a client object that corresponds to this object
    */
   void                        * _client;
 
   /**
    * @brief pointer to buildIL callback function for this object
    * usually NULL, but client objects can set this via setBuildILCallback() to be called
    * when buildIL is called on this object
    */
   ClientBuildILCallback         _clientCallbackBuildIL;

   /**
    * @brief pointer to allocator function for this object.
    *
    * Clients must set this pointer using setClientAllocator().
    * When this allocator is called, a pointer to the current
    * class (this) will be passed as argument. The expected
    * returned value is a pointer to the base type of the
    * client object.
    */
   static ClientAllocator        _clientAllocator;

   /**
    * @brief pointer to impl getter function
    *
    * Clients must set this pointer using setImplGetter().
    * When called with an instance of a client object,
    * the function must return the corresponding
    * implementation object
    */
   static ImplGetter             _getImpl;

   /**
    * @brief MethodBuilder parent for this IlBuilder object
    */
   TR::MethodBuilder           * _methodBuilder;

   /**
    * @brief sequence of TR::Blocks and other TR::IlBuilder objects that should execute when control reaches this IlBuilder
    */
   List<SequenceEntry>         * _sequence;

   /**
    * @brief used to track the end of the current sequence
    */
   ListAppender<SequenceEntry> * _sequenceAppender;

   /**
    * @brief each IlBuilder object is like a mini-CFG, with its own unique entry block
    */
   TR::Block                   * _entryBlock;

   /**
    * @brief each IlBuilder object is like a mini-CFG, with its own unique exit block
    */
   TR::Block                   * _exitBlock;

   /**
    * @brief counter for how many TR::Blocks are needed to represent everything inside this IlBuilder object
    */
   int32_t                       _count;

   /**
    * @brief has this IlBuilder object been made part of some other builder's sequence
    */
   bool                          _partOfSequence;

   /**
    * @brief has connectTrees been run on this IlBuilder object yet
    */
   bool                          _connectedTrees;

   /**
    * @brief returns true if there is a control edge to this IlBuilder's exit block
    */
   bool                          _comesBack;

   /**
    * @brief returns true if this IlBuilder object is a handler for an exception edge
    */
   bool                          _isHandler;

   virtual bool buildIL()
      {
      if (_clientCallbackBuildIL)
         return (*_clientCallbackBuildIL)(client());
      return true;
      }

   TR::SymbolReference *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::SymbolReference *v);
   TR::IlValue *newValue(TR::IlType *dt, TR::Node *n=NULL);
   TR::IlValue *newValue(TR::DataType dt, TR::Node *n=NULL);
   void defineValue(const char *name, TR::IlType *dt);

   TR::Node *loadValue(TR::IlValue *v);
   void storeNode(TR::SymbolReference *symRef, TR::Node *v);
   void indirectStoreNode(TR::Node *addr, TR::Node *v);
   TR::IlValue *indirectLoadNode(TR::IlType *dt, TR::Node *addr, bool isVectorLoad=false);

   TR::Node *zero(TR::DataType dt);
   TR::Node *zero(TR::IlType *dt);
   TR::Node *zeroNodeForValue(TR::IlValue *v);
   TR::IlValue *zeroForValue(TR::IlValue *v);

   TR::IlValue *unaryOp(TR::ILOpCodes op, TR::IlValue *v);
   void doVectorConversions(TR::Node **leftPtr, TR::Node **rightPtr);
   TR::IlValue *widenIntegerTo32Bits(TR::IlValue *v);
   TR::IlValue *widenIntegerTo32BitsUnsigned(TR::IlValue *v);
   TR::IlValue *binaryOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::Node *binaryOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *binaryOpFromOpMap(OpCodeMapper mapOp, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *binaryOpFromOpCode(TR::ILOpCodes op, TR::IlValue *left, TR::IlValue *right);
   TR::Node *shiftOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *shiftOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *shiftOpFromOpMap(OpCodeMapper mapOp, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *compareOp(TR_ComparisonTypes ct, bool needUnsigned, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *convertTo(TR::DataType typeTo, TR::IlValue *v, bool needUnsigned);

   void ifCmpCondition(TR_ComparisonTypes ct, bool isUnsignedCmp, TR::IlValue *left, TR::IlValue *right, TR::Block *target);
   void ifCmpNotEqualZero(TR::IlValue *condition, TR::Block *target);
   void ifCmpEqualZero(TR::IlValue *condition, TR::Block *target);

   void integerizeAddresses(TR::IlValue **leftPtr, TR::IlValue **rightPtr);

   void appendGoto(TR::Block *destBlock);

   virtual void appendBlock(TR::Block *block = 0, bool addEdge=true);
   void appendNoFallThroughBlock(TR::Block *block = 0)
      {
      appendBlock(block, false);
      }

   TR::Block *emptyBlock();
   
   virtual uint32_t countBlocks();

   void pullInBuilderTrees(TR::IlBuilder *builder,
                           uint32_t *currentBlock,
                           TR::TreeTop **firstTree,
                           TR::TreeTop **newLastTree);

   // BytecodeBuilder needs this interface, but IlBuilders doesn't so just ignore the parameter
   virtual bool connectTrees(uint32_t *currentBlock) { return connectTrees(); }

   virtual bool connectTrees();

   TR::Node *genOverflowCHKTreeTop(TR::Node *operationNode, TR::ILOpCodes overflow);
   TR::ILOpCodes getOpCode(TR::IlValue *leftValue, TR::IlValue *rightValue);
   void appendExceptionHandler(TR::Block *blockThrowsException, TR::IlBuilder **builder, uint32_t catchType);
   TR::IlValue *genOperationWithOverflowCHK(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode, TR::IlBuilder **handler, TR::ILOpCodes overflow);
   virtual void setHandlerInfo(uint32_t catchType);
   TR::IlValue **processCallArgs(TR::Compilation *comp, int numArgs, va_list args);
   JBCase **createCaseArray(uint32_t numCases, va_list arg);
   void generateSwitchCases(TR::Node *switchNode, TR::Node *defaultNode, TR::IlBuilder **defaultBuilder, uint32_t numCases, JBCase **cases);
   };

} // namespace OMR

#endif // !defined(OMR_ILBUILDER_INCL)
