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

#ifndef OMR_ILBUILDER_IMPL_INCL
#define OMR_ILBUILDER_IMPL_INCL

#ifndef TR_ILBUILDER_IMPL_DEFINED
#define TR_ILBUILDER_IMPL_DEFINED
#define PUT_OMR_ILBUILDER_IMPL_INTO_TR
#endif // !defined(TR_ILBUILDER_IMPL_DEFINED)

#include <fstream>
#include <stdarg.h>
#include <string.h>
#include "ilgen/IlInjector.hpp"
#include "il/ILHelpers.hpp"

#include "ilgen/IlValueImpl.hpp" // must go after IlInjector.hpp or TR_ALLOC isn't cleaned up

namespace OMR { class MethodBuilderImpl; }

namespace TR { class Block; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class ResolvedMethodSymbol; } 
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }

namespace TR { class IlBuilder; }
namespace TR { class IlBuilderImpl; }
namespace TR { class IlTypeImplImpl; }
namespace TR { class TypeDictionaryImpl; }

template <class T> class List;
template <class T> class ListAppender;

namespace OMR
{

typedef TR::ILOpCodes (*OpCodeMapper)(TR::DataType);


class IlBuilderImpl : public TR::IlInjector
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
      SequenceEntry(TR::IlBuilderImpl *builder)
         {
         _isBlock = false;
         _builder = builder;
         }

      bool _isBlock;
      union
         {
         TR::Block *_block;
         TR::IlBuilderImpl *_builder;
         };
      };

   SequenceEntry *blockEntry(TR::Block *block);
   SequenceEntry *builderEntry(TR::IlBuilderImpl *builder);

public:
   TR_ALLOC(TR_Memory::IlGenerator)

   friend class OMR::MethodBuilderImpl;

   IlBuilderImpl(TR::MethodBuilderImpl *methodBuilder, TR::TypeDictionaryImpl *types);
   IlBuilderImpl(TR::IlBuilderImpl *source);
   virtual ~IlBuilderImpl()
      { }
   void operator delete(void * ptr)
      { }

   /*
    * Client API support for IlBuilder, in alphabetical order within categories
    */

   // create a new local value (temporary variable)
   TR::IlValueImpl *Copy(TR::IlValueImpl *value);
   TR::IlBuilderImpl *OrphanBuilderImpl();

   // constants
   TR::IlValueImpl *NullAddress();
   TR::IlValueImpl *ConstInt8(int8_t value);
   TR::IlValueImpl *ConstInt16(int16_t value);
   TR::IlValueImpl *ConstInt32(int32_t value);
   TR::IlValueImpl *ConstInt64(int64_t value);
   TR::IlValueImpl *ConstFloat(float value);
   TR::IlValueImpl *ConstDouble(double value);
   TR::IlValueImpl *ConstAddress(const void * const value);
   TR::IlValueImpl *ConstString(const char * const value);
   TR::IlValueImpl *ConstZeroValueForValue(TR::IlValueImpl *v);
   TR::IlValueImpl *ConstInteger(TR::IlTypeImpl *intType, int64_t value);

   // arithmetic
   TR::IlValueImpl *Add(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *AddWithOverflow(TR::IlBuilderImpl **handler, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *AddWithUnsignedOverflow(TR::IlBuilderImpl **handler, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *And(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *Div(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *IndexAt(TR::IlTypeImpl *dt, TR::IlValueImpl *base, TR::IlValueImpl *index);
   TR::IlValueImpl *Mul(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *MulWithOverflow(TR::IlBuilderImpl **handler, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *Or(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *ShiftL(TR::IlValueImpl *v, TR::IlValueImpl *amount);
   TR::IlValueImpl *ShiftR(TR::IlValueImpl *v, TR::IlValueImpl *amount);
   TR::IlValueImpl *Sub(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *SubWithOverflow(TR::IlBuilderImpl **handler, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *SubWithUnsignedOverflow(TR::IlBuilderImpl **handler, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *UnsignedShiftR(TR::IlValueImpl *v, TR::IlValueImpl *amount);
   TR::IlValueImpl *Xor(TR::IlValueImpl *left, TR::IlValueImpl *right);

   // comparisons
   TR::IlValueImpl *EqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *GreaterOrEqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *GreaterThan(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *LessOrEqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *LessThan(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *NotEqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *UnsignedGreaterOrEqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *UnsignedGreaterThan(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *UnsignedLessOrEqualTo(TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *UnsignedLessThan(TR::IlValueImpl *left, TR::IlValueImpl *right);

   // conversions
   TR::IlValueImpl *ConvertTo(TR::IlTypeImpl *t, TR::IlValueImpl *v);
   TR::IlValueImpl *UnsignedConvertTo(TR::IlTypeImpl *t, TR::IlValueImpl *v);

   // memory
   TR::IlValueImpl *AtomicAdd(TR::IlValueImpl *baseAddress, TR::IlValueImpl * value);
   TR::IlValueImpl *CreateLocalArray(int32_t numElements, TR::IlTypeImpl *elementType);
   TR::IlValueImpl *CreateLocalStruct(TR::IlTypeImpl *structType);
   TR::IlValueImpl *Load(const char *name);
   TR::IlValueImpl *LoadAt(TR::IlTypeImpl *dt, TR::IlValueImpl *address);
   TR::IlValueImpl *LoadIndirect(const char *type, const char *field, TR::IlValueImpl *object);
   void Store(const char *name, TR::IlValueImpl *value);
   void StoreAt(TR::IlValueImpl *address, TR::IlValueImpl *value);
   void StoreOver(TR::IlValueImpl *dest, TR::IlValueImpl *value);
   void StoreIndirect(const char *type, const char *field, TR::IlValueImpl *object, TR::IlValueImpl *value);
   void Transaction(TR::IlBuilderImpl **persistentFailureBuilder, TR::IlBuilderImpl **transientFailureBuilder, TR::IlBuilderImpl **fallThroughBuilder);
   void TransactionAbort();

   /**
    * `StructFieldAddress` and `UnionFieldAddress` are two functions that
    * provide a workaround for a limitation in JitBuilder. Becuase `TR::IlValueImpl`
    * cannot represent an object type (only pointers to objects), there is no
    * way to generate a load for a field that is itself an object. The workaround
    * is to use the field's address instead. This is not an elegent solution and
    * should be revisited.
    */
   TR::IlValueImpl *StructFieldInstanceAddress(const char* structName, const char* fieldName, TR::IlValueImpl* obj);
   TR::IlValueImpl *UnionFieldInstanceAddress(const char* unionName, const char* fieldName, TR::IlValueImpl* obj);

   // vector memory
   TR::IlValueImpl *VectorLoad(const char *name);
   TR::IlValueImpl *VectorLoadAt(TR::IlTypeImpl *dt, TR::IlValueImpl *address);
   void VectorStore(const char *name, TR::IlValueImpl *value);
   void VectorStoreAt(TR::IlValueImpl *address, TR::IlValueImpl *value);

   // control
   void AppendBuilder(TR::IlBuilderImpl *builder);
   TR::IlValueImpl *Call(const char *name, int32_t numArgs, TR::IlValueImpl **argValues);
   TR::IlValueImpl *ComputedCall(const char *name, int32_t numArgs, TR::IlValueImpl **args);
   void Goto(TR::IlBuilderImpl **dest);
   void Goto(TR::IlBuilderImpl *dest);
   void Return();
   void Return(TR::IlValueImpl *value);
   virtual void ForLoop(bool countsUp,
                const char *indVar,
                TR::IlBuilderImpl **body,
                TR::IlBuilderImpl **breakBuilder,
                TR::IlBuilderImpl **continueBuilder,
                TR::IlValueImpl *initial,
                TR::IlValueImpl *iterateWhile,
                TR::IlValueImpl *increment);
   void WhileDoLoop(const char *exitCondition,
                    TR::IlBuilderImpl **body,
                    TR::IlBuilderImpl **breakBuilder = NULL,
                    TR::IlBuilderImpl **continueBuilder = NULL);
   void DoWhileLoop(const char *exitCondition,
                    TR::IlBuilderImpl **body,
                    TR::IlBuilderImpl **breakBuilder = NULL,
                    TR::IlBuilderImpl **continueBuilder = NULL);
   /* @brief creates an AND nest of short-circuited conditions, */
   /*        for each term pass an IlBuilderImpl containing the condition */
   /*        and the IlValueImpl that computes the condition */
   void IfAnd(TR::IlBuilderImpl **allTrueBuilder,
              TR::IlBuilderImpl **anyFalseBuilder,
              int32_t numTerms,
              TR::IlBuilderImpl **caseBuilders,
              TR::IlValueImpl **caseValues);
   void IfCmpEqualZero(TR::IlBuilderImpl **target, TR::IlValueImpl *condition);
   void IfCmpEqualZero(TR::IlBuilderImpl *target, TR::IlValueImpl *condition);
   void IfCmpEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpLessOrEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpLessOrEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpLessThan(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpLessThan(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpGreaterOrEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpGreaterOrEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpGreaterThan(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpGreaterThan(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpNotEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpNotEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpNotEqualZero(TR::IlBuilderImpl **target, TR::IlValueImpl *condition);
   void IfCmpNotEqualZero(TR::IlBuilderImpl *target, TR::IlValueImpl *condition);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedGreaterOrEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedGreaterThan(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedLessOrEqual(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedLessThan(TR::IlBuilderImpl **target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   void IfCmpUnsignedLessThan(TR::IlBuilderImpl *target, TR::IlValueImpl *left, TR::IlValueImpl *right);
   /* @brief creates an OR nest of short-circuited conditions, */
   /*       for each term pass an IlBuilderImpl containing the condition */
   /*       and the IlValueImpl that computes the condition */
   void IfOr(TR::IlBuilderImpl **anyTrueBuilder,
             TR::IlBuilderImpl **allFalseBuilder,
             int32_t numTerms,
             TR::IlBuilderImpl **caseBuilders,
             TR::IlValueImpl **caseValues);
   void IfThenElse(TR::IlBuilderImpl **thenPath,
                   TR::IlBuilderImpl **elsePath,
                   TR::IlValueImpl *condition);
   void Switch(const char *selectionVar,
               TR::IlBuilderImpl **defaultBuilder,
               uint32_t numCases,
               int32_t *caseValues,
               TR::IlBuilderImpl **caseBuilders,
               bool *caseFallsThrough);


   /*
    * Public API for *Impl classes
    */

   static TR::IlBuilderImpl *allocate(TR::MethodBuilderImpl *mb, TR::TypeDictionaryImpl *types);

   void setClient(TR::IlBuilder *b)
      { _clientBuilder = b; }

   TR::IlBuilder *client()
      { return _clientBuilder; }

   TR::IlBuilderImpl *orphanBuilderImpl();

   void initSequence();

   virtual bool injectIL();
   virtual void setupForBuildIL();

   virtual bool isMethodBuilder()                   { return false; }
   virtual TR::MethodBuilderImpl *asMethodBuilder() { return NULL; }

   virtual bool isBytecodeBuilder()                 { return false; }

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

   TR::IlBuilderImpl *createBuilderIfNeeded(TR::IlBuilderImpl *builder);
   bool TraceEnabled_log();
   void TraceIL_log(const char *s, ...);

   TR::IlValueImpl *genCall(TR::SymbolReference *methodSymRef, int32_t numArgs, TR::IlValueImpl ** paramValues, bool isDirectCall = true);

protected:

   /**
    * @brief corresponding IlBuilder (client API) object (must be 1-1 relationship)
    */
   TR::IlBuilder               * _clientBuilder;

   /**
    * @brief MethodBuilderImpl parent for this IlBuilderImpl object
    */
   TR::MethodBuilderImpl       * _methodBuilder;

   /**
    * @brief sequence of TR::Blocks and other TR::IlBuilderImpl objects that should execute when control reaches this IlBuilderImpl
    */
   List<SequenceEntry>         * _sequence;

   /**
    * @brief used to track the end of the current sequence
    */
   ListAppender<SequenceEntry> * _sequenceAppender;

   /**
    * @brief each IlBuilderImpl object is like a mini-CFG, with its own unique entry block
    */
   TR::Block                   * _entryBlock;

   /**
    * @brief each IlBuilderImpl object is like a mini-CFG, with its own unique exit block
    */
   TR::Block                   * _exitBlock;

   /**
    * @brief counter for how many TR::Blocks are needed to represent everything inside this IlBuilderImpl object
    */
   int32_t                       _count;

   /**
    * @brief has this IlBuilderImpl object been made part of some other builder's sequence
    */
   bool                          _partOfSequence;

   /**
    * @brief has connectTrees been run on this IlBuilderImpl object yet
    */
   bool                          _connectedTrees;

   /**
    * @brief returns true if there is a control edge to this IlBuilderImpl's exit block
    */
   bool                          _comesBack;

   /**
    * @brief returns true if this IlBuilderImpl object is a handler for an exception edge
    */
   bool                          _isHandler;

   virtual bool buildIL() { return true; }

   TR::SymbolReference *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::SymbolReference *v);
   TR::IlValueImpl *newValue(TR::IlTypeImpl *dt, TR::Node *n=NULL);
   TR::IlValueImpl *newValue(TR::DataType dt, TR::Node *n=NULL);
   void defineValue(const char *name, TR::IlTypeImpl *dt);

   TR::Node *loadValue(TR::IlValueImpl *v);
   void storeNode(TR::SymbolReference *symRef, TR::Node *v);
   void indirectStoreNode(TR::Node *addr, TR::Node *v);
   TR::IlValueImpl *indirectLoadNode(TR::IlTypeImpl *dt, TR::Node *addr, bool isVectorLoad=false);

   TR::Node *zero(TR::DataType dt);
   TR::Node *zero(TR::IlTypeImpl *dt);
   TR::Node *zeroNodeForValue(TR::IlValueImpl *v);
   TR::IlValueImpl *zeroForValue(TR::IlValueImpl *v);

   TR::IlValueImpl *unaryOp(TR::ILOpCodes op, TR::IlValueImpl *v);
   void doVectorConversions(TR::Node **leftPtr, TR::Node **rightPtr);
   TR::IlValueImpl *binaryOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::Node *binaryOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValueImpl *binaryOpFromOpMap(OpCodeMapper mapOp, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *binaryOpFromOpCode(TR::ILOpCodes op, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::Node *shiftOpNodeFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValueImpl *shiftOpFromNodes(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode);
   TR::IlValueImpl *shiftOpFromOpMap(OpCodeMapper mapOp, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *compareOp(TR_ComparisonTypes ct, bool needUnsigned, TR::IlValueImpl *left, TR::IlValueImpl *right);
   TR::IlValueImpl *convertTo(TR::IlTypeImpl *t, TR::IlValueImpl *v, bool needUnsigned);

   void ifCmpCondition(TR_ComparisonTypes ct, bool isUnsignedCmp, TR::IlValueImpl *left, TR::IlValueImpl *right, TR::Block *target);
   void ifCmpNotEqualZero(TR::IlValueImpl *condition, TR::Block *target);
   void ifCmpEqualZero(TR::IlValueImpl *condition, TR::Block *target);

   void integerizeAddresses(TR::IlValueImpl **leftPtr, TR::IlValueImpl **rightPtr);

   void appendGoto(TR::Block *destBlock);

   virtual void appendBlock(TR::Block *block = 0, bool addEdge=true);
   void appendNoFallThroughBlock(TR::Block *block = 0)
      {
      appendBlock(block, false);
      }

   TR::Block *emptyBlock();
   
   virtual uint32_t countBlocks();

   void pullInBuilderTrees(TR::IlBuilderImpl *builder,
                           uint32_t *currentBlock,
                           TR::TreeTop **firstTree,
                           TR::TreeTop **newLastTree);

   // BytecodeBuilderImpl needs this interface (with currentBlock), but IlBuilderImpl doesn't so just ignore the parameter
   virtual bool connectTrees(uint32_t *currentBlock) { return connectTrees(); }

   virtual bool connectTrees();

   TR::Node *genOverflowCHKTreeTop(TR::Node *operationNode, TR::ILOpCodes overflow);
   TR::ILOpCodes getOpCode(TR::IlValueImpl *leftValue, TR::IlValueImpl *rightValue);
   void appendExceptionHandler(TR::Block *blockThrowsException, TR::IlBuilderImpl **builder, uint32_t catchType);
   TR::IlValueImpl *genOperationWithOverflowCHK(TR::ILOpCodes op,
                                                TR::Node *leftNode,
                                                TR::Node *rightNode,
                                                TR::IlBuilderImpl **handler,
                                                TR::ILOpCodes overflow);
   virtual void setHandlerInfo(uint32_t catchType);
   TR::IlValueImpl **processCallArgs(TR::Compilation *comp, int numArgs, va_list args);
   };

} // namespace OMR



#if defined(PUT_OMR_ILBUILDER_IMPL_INTO_TR)

namespace TR
{
   class IlBuilderImpl : public OMR::IlBuilderImpl
      {
      public:
         IlBuilderImpl(TR::MethodBuilderImpl *methodBuilder, TR::TypeDictionaryImpl *types)
            : OMR::IlBuilderImpl(methodBuilder, types)
            { }

         IlBuilderImpl(TR::IlBuilderImpl *source)
            : OMR::IlBuilderImpl(source)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_ILBUILDER_IMPL_INTO_TR)

#endif // !defined(OMR_ILBUILDER_IMPL_INCL)
