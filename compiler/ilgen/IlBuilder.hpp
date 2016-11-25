/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/


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

namespace OMR { class MethodBuilder; }

namespace TR { class Block; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlBuilder; }
namespace TR { class ResolvedMethodSymbol; } 
namespace TR { class SymbolReference; }
namespace TR { class SymbolReferenceTable; }

namespace TR { typedef TR::SymbolReference IlValue; }
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
      _haveReplayName(false),
      _rpILCpp(0),
      _isHandler(false)
      {
      }
   IlBuilder(TR::IlBuilder *source);

   void initSequence();

   virtual bool injectIL();
   virtual void setupForBuildIL();

   virtual bool isMethodBuilder()               { return false; }
   virtual TR::MethodBuilder *asMethodBuilder() { return NULL; }

   virtual bool isBytecodeBuilder()             { return false; }

   char *getName();

   void print(const char *title, bool recurse=false);
   void printBlock(TR::Block *block);

   TR::TreeTop *getFirstTree();
   TR::TreeTop *getLastTree();
   TR::Block *getEntry() { return _entryBlock; }
   TR::Block *getExit()  { return _exitBlock; }

   void setDoesNotComeBack() { _comesBack = false; }
   bool comesBack()          { return _comesBack; }

   bool blocksHaveBeenCounted() { return _count > -1; }

   TR::IlBuilder *createBuilderIfNeeded(TR::IlBuilder *builder);
   TR::IlBuilder *OrphanBuilder();
   bool TraceEnabled_log();
   void TraceIL_log(const char *s, ...);

   // constants
   TR::IlValue *NullAddress();
   TR::IlValue *ConstInt8(int8_t value);
   TR::IlValue *ConstInt16(int16_t value);
   TR::IlValue *ConstInt32(int32_t value);
   TR::IlValue *ConstInt64(int64_t value);
   TR::IlValue *ConstFloat(float value);
   TR::IlValue *ConstDouble(double value);
   TR::IlValue *ConstAddress(void* value);
   TR::IlValue *ConstString(const char *value);
   TR::IlValue *ConstzeroValueForValue(TR::IlValue *v);

   // arithmetic
   TR::IlValue *Add(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *UnsignedAddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *Sub(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right);
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
   TR::IlValue *GreaterThan(TR::IlValue *left, TR::IlValue *right);
   TR::IlValue *ConvertTo(TR::IlType *t, TR::IlValue *v);

   // memory
   TR::IlValue *CreateLocalArray(int32_t numElements, TR::IlType *elementType);
   TR::IlValue *CreateLocalStruct(TR::IlType *structType);
   TR::IlValue *Load(const char *name);
   void Store(const char *name, TR::IlValue *value);
   void StoreAt(TR::IlValue *address, TR::IlValue *value);
   TR::IlValue *LoadAt(TR::IlType *dt, TR::IlValue *address);
   TR::IlValue *LoadIndirect(const char *type, const char *field, TR::IlValue *object);
   TR::IlValue *StoreField(const char *name, TR::IlValue *object, TR::IlValue *value);
   TR::IlValue *IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index);
   TR::IlValue *AtomicIntegerAdd(TR::IlValue * object, TR::IlValue * offset, TR::IlValue * value);
   TR::IlValue *AtomicIntegerAdd(TR::IlValue * object, TR::IlValue * value);
   
   // vector memory
   TR::IlValue *VectorLoad(const char *name);
   TR::IlValue *VectorLoadAt(TR::IlType *dt, TR::IlValue *address);
   void VectorStore(const char *name, TR::IlValue *value);
   void VectorStoreAt(TR::IlValue *address, TR::IlValue *value);

   // control
   void StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value);
   void AppendBuilder(TR::IlBuilder *builder);
   TR::IlValue *Call(const char *name, int32_t numArgs, ...);
   TR::IlValue *Call(const char *name, int32_t numArgs, TR::IlValue ** paramValues);
   void Goto(TR::IlBuilder **dest);
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

   void IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition);
   void IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
   void IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right);
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
   TR::MethodBuilder           * _methodBuilder;
   List<SequenceEntry>         * _sequence;
   ListAppender<SequenceEntry> * _sequenceAppender;
   TR::Block                   * _entryBlock;
   TR::Block                   * _exitBlock;
   int32_t                       _count;
   bool                          _partOfSequence;
   bool                          _connectedTrees;
   bool                          _comesBack;
   bool                          _isHandler;

   bool                          _haveReplayName;
   std::fstream                * _rpILCpp;
   char                          _replayName[21];
   char                          _rpLine[256];

   virtual bool buildIL() { return true; }

   TR::IlValue *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::IlValue *v);
   TR::IlValue *newValue(TR::DataType dt);
   TR::IlValue *newValue(TR::IlType *dt);
   void defineValue(const char *name, TR::IlType *dt);

   TR::Node *loadValue(TR::IlValue *v);
   void storeNode(TR::IlValue *dest, TR::Node *v);
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
   TR::IlValue *compareOp(TR_ComparisonTypes ct, bool needUnsigned, TR::IlValue *left, TR::IlValue *right);

   void ifCmpNotEqualZero(TR::IlValue *condition, TR::Block *target);
   void ifCmpNotEqual(TR::IlValue *condition, TR::IlValue *zero, TR::Block *target);
   void ifCmpEqualZero(TR::IlValue *condition, TR::Block *target);
   void ifCmpEqual(TR::IlValue *condition, TR::IlValue *zero, TR::Block *target);
   void ifCmpLessThan(TR::IlValue *condition, TR::IlValue *zero, TR::Block *target);
   void ifCmpGreaterThan(TR::IlValue *condition, TR::IlValue *zero, TR::Block *target);

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

   TR::Node *genOverflowCHKTreeTop(TR::Node *operationNode);
   void appendExceptionHandler(TR::Block *blockThrowsException, TR::IlBuilder **builder, uint32_t catchType);
   TR::IlValue *operationWithOverflow(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode, TR::IlBuilder **handler);
   virtual void setHandlerInfo(uint32_t catchType);
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
