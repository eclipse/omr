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

#include <fstream>
#include <stdarg.h>
#include <string.h>
#include "ilgen/IlInjector.hpp"
#include "il/ILHelpers.hpp"

#include "ilgen/IlValue.hpp" // must go after IlInjector.hpp or TR_ALLOC isn't cleaned up

namespace OMR { class MethodBuilder; }

namespace TR { class Block; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlBuilder; }
namespace TR { class ResolvedMethodSymbol; } 
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }

namespace TR { class IlType; }
namespace TR { class TypeDictionary; }

template <class T> class List;
template <class T> class ListAppender;

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

   friend class OMR::MethodBuilder;

   IlBuilder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
      : TR::IlInjector(types),
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
   TR::IlValue *Call(const char *name, int32_t numArgs, ...);
   TR::IlValue *Call(const char *name, int32_t numArgs, TR::IlValue **argValues);
   TR::IlValue *ComputedCall(const char *name, int32_t numArgs, ...);
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

   /* @brief creates an AND nest of short-circuited conditions, for each term pass an IlBuilder containing the condition and the IlValue that computes the condition */
   void IfAnd(TR::IlBuilder **allTrueBuilder, TR::IlBuilder **anyFalseBuilder, int32_t numTerms, ... );
   /* @brief creates an OR nest of short-circuited conditions, for each term pass an IlBuilder containing the condition and the IlValue that computes the condition */
   void IfOr(TR::IlBuilder **anyTrueBuilder, TR::IlBuilder **allFalseBuilder, int32_t numTerms, ... );

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

protected:

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

   virtual bool buildIL() { return true; }

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
   TR::IlValue *binaryOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::Node *binaryOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *binaryOpFromOpMap(OpCodeMapper mapOp, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *binaryOpFromOpCode(TR::ILOpCodes op, TR::IlValue *left, TR::IlValue *right);
   TR::Node *shiftOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *shiftOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValue *shiftOpFromOpMap(OpCodeMapper mapOp, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *compareOp(TR_ComparisonTypes ct, bool needUnsigned, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *convertTo(TR::IlType *t, TR::IlValue *v, bool needUnsigned);

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

         IlBuilder(TR::IlBuilder *source)
            : OMR::IlBuilder(source)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILBUILDER_INTO_TR)

#endif // !defined(OMR_ILBUILDER_INCL)
