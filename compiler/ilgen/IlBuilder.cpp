/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
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
 *******************************************************************************/

#include "ilgen/IlBuilder.hpp"

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/FrontEnd.hpp"
#include "il/Block.hpp"
#include "il/SymbolReference.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"

#define OPT_DETAILS "O^O ILBLD: "

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

//#define REPLAY(x)            { x; }
#define REPLAY(x)            { }
#define ILB_REPLAY(...)	     { REPLAY({sprintf(_rpLine, ##__VA_ARGS__); (*_rpILCpp) << "\t" << _rpLine << std::endl;}) }
#define REPLAY_VALUE(v)      ((v)->getSymbol()->getAutoSymbol()->getName())
#define REPLAY_TYPE(t)       ((t)->getName())
#define REPLAY_BUILDER(b)    ((b)->getName())
#define REPLAY_BOOL(b)       ( b? "true" : "false")


// IlBuilder is a class designed to help build Testarossa IL quickly without
// a lot of knowledge of the intricacies of commoned references, symbols,
// symbol references, or blocks. You can add operations to an IlBuilder via
// a set of services, for example: Add, Sub, Load, Store. These services
// operate on TR::IlValues, which currently correspond to SymbolReferences.
// However, IlBuilders also incorporate a notion of "symbols" that are
// identified by strings (i.e. arbitrary symbolic names).
//
// So Load("a") returns a TR::IlValue. Load("b") returns a different
// TR::IlValue. The value of a+b can be computed by Add(Load("a"),Load("b")),
// and that value can be stored into c by Store("c", Add(Load("a"),Load("b"))).
//
// More complicated services exist to construct control flow by linking
// together sets of IlBuilder objects. A simple if-then construct, for
// example, can be built with the following code:
//    TR::IlValue *condition = NotEqualToZero(Load("a"));
//    TR::IlBuilder *then = NULL;
//    IfThen(&then, condition);
//    then->Return(then->ConstInt32(0));
//
//
// An IlBuilder is really a sequence of Blocks and other IlBuilders, but an
// IlBuilder is also a sequence of TreeTops connecting together a set of
// Blocks that are arranged in the CFG. Another way to think of an IlBuilder
// is as representing the code needed to execute a particular code path
// (which may itself have sub control flow handled by embedded IlBuilders).
// All IlBuilders exist within the single control flow graph, but the full
// control flow graph will not be connected together until all IlBuilder
// objects have had injectIL() called.


namespace OMR
{

IlBuilder::IlBuilder(TR::IlBuilder *source)
   : TR::IlInjector(source),
   _methodBuilder(source->_methodBuilder),
   _sequence(0),
   _sequenceAppender(0),
   _entryBlock(0),
   _exitBlock(0),
   _count(-1),
   _partOfSequence(false),
   _connectedTrees(false),
   _comesBack(true),
   _haveReplayName(false),
   _rpILCpp(0)
   {
   }

bool
IlBuilder::TraceEnabled_log(){
    bool traceEnabled = _comp->getOption(TR_TraceILGen);
    return traceEnabled;
}

void
IlBuilder::TraceIL_log(const char* s, ...){
    va_list argp;
    va_start (argp, s);
    traceMsgVarArgs(_comp, s, argp);
    va_end(argp);
}

void
IlBuilder::initSequence()
   {
   _sequence = new (_comp->trMemory()->trHeapMemory()) List<SequenceEntry>(_comp->trMemory());
   _sequenceAppender = new (_comp->trMemory()->trHeapMemory()) ListAppender<SequenceEntry>(_sequence);
   }

bool
IlBuilder::injectIL()
   {
   TraceIL("original entry %p\n", cfg()->getStart());
   TraceIL("original exit %p\n", cfg()->getEnd());

   setupForBuildIL();

   if (!buildIL())
      return false;

   bool rc = connectTrees();
   comp()->dumpMethodTrees("after connectTrees");
   cfg()->removeUnreachableBlocks();
   comp()->dumpMethodTrees("after removing unreachable blocks");
   return rc;
   }

void
IlBuilder::setupForBuildIL()
   {
   initSequence();
   appendBlock(NULL, false);
   _entryBlock = _currentBlock;
   _exitBlock = emptyBlock();
   }


char *
IlBuilder::getName()
   {
   if (!_haveReplayName)
      {
      sprintf(_replayName, "B_%p", this);
      _haveReplayName = true;
      }
   return _replayName;
   }


void
IlBuilder::print(const char *title, bool recurse)
   {
   if (!TraceEnabled)
      return;

   if (title != NULL)
      TraceIL("[ %p ] %s\n", this, title);
   TraceIL("[ %p ] _methodBuilder %p\n", this, _methodBuilder);
   TraceIL("[ %p ] _entryBlock %p\n", this, _entryBlock);
   TraceIL("[ %p ] _exitBlock %p\n", this, _exitBlock);
   TraceIL("[ %p ] _connectedBlocks %d\n", this, _connectedTrees);
   TraceIL("[ %p ] _comesBack %d\n", this, _comesBack);
   TraceIL("[ %p ] Sequence:\n", this);
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      if (entry->_isBlock)
         {
         printBlock(entry->_block);
         }
      else
         {
         if (recurse)
            {
            TraceIL("[ %p ] Inner builder %p", this, entry->_builder);
            entry->_builder->print(NULL, recurse);
            }
         else
            {
            TraceIL("[ %p ] Builder %p\n", this, entry->_builder);
            }
         }
      }
   }

void
IlBuilder::printBlock(TR::Block *block)
   {
   if (!TraceEnabled)
      return;

   TraceIL("[ %p ] Block %p\n", this, block);
   TR::TreeTop *tt = block->getEntry();
   while (tt != block->getExit())
      {
      comp()->getDebug()->print(comp()->getOutFile(), tt);
      tt = tt->getNextTreeTop();
      }
   comp()->getDebug()->print(comp()->getOutFile(), tt);
   }

TR::IlValue *
IlBuilder::lookupSymbol(const char *name)
   {
   TR_ASSERT(_methodBuilder, "cannot look up symbols in an IlBuilder that has no MethodBuilder");
   return _methodBuilder->lookupSymbol(name);
   }

void
IlBuilder::defineSymbol(const char *name, TR::IlValue *v)
   {
   TR_ASSERT(_methodBuilder, "cannot define symbols in an IlBuilder that has no MethodBuilder");
   _methodBuilder->defineSymbol(name, v);
   }

void
IlBuilder::defineValue(const char *name, TR::IlType *dt)
   {
   TR::SymbolReference *newSymRef = symRefTab()->createTemporary(methodSymbol(), dt->getPrimitiveType());
   defineSymbol(name, newSymRef);
   }

TR::IlValue *
IlBuilder::newValue(TR::DataType dt)
   {
   TR::SymbolReference *newSymRef = symRefTab()->createTemporary(methodSymbol(), dt);
   char *name = (char *) _comp->trMemory()->allocateHeapMemory(5 * sizeof(char));
   sprintf(name, "_T%-3d", newSymRef->getCPIndex());
   newSymRef->getSymbol()->getAutoSymbol()->setName(name);
   defineSymbol(name, newSymRef);
   return newSymRef;
   }

TR::IlValue *
IlBuilder::newValue(TR::IlType *dt)
   {
   return newValue(dt->getPrimitiveType());
   }

TR::TreeTop *
IlBuilder::getFirstTree()
   {
   TR_ASSERT(_blocks, "_blocks not created yet");
   TR_ASSERT(_blocks[0], "_blocks[0] not created yet");
   return _blocks[0]->getEntry();
   }

TR::TreeTop *
IlBuilder::getLastTree()
   {
   TR_ASSERT(_blocks, "_blocks not created yet");
   TR_ASSERT(_blocks[_numBlocks], "_blocks[0] not created yet");
   return _blocks[_numBlocks]->getExit();
   }

uint32_t
IlBuilder::countBlocks()
   {
   // only count each block once
   if (_count > -1)
      return _count;

   TraceIL("[ %p ] TR::IlBuilder::countBlocks 0 at entry\n", this);

   _count = 0; // prevent recursive counting; will be updated to real value before returning

   uint32_t count=0;
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      if (entry->_isBlock)
         count++;
      else
         count += entry->_builder->countBlocks();
      }

   if (this != _methodBuilder)
      {
      // exit block isn't in the sequence except for method builders
      //            gets tacked in late by connectTrees
      count++;
      }

   TraceIL("[ %p ] TR::IlBuilder::countBlocks %d\n", this, count);

   _count = count;
   return count;
   }

void
IlBuilder::pullInBuilderTrees(TR::IlBuilder *builder,
                              uint32_t *currentBlock,
                              TR::TreeTop **firstTree,
                              TR::TreeTop **newLastTree)
   {
   TraceIL("\n[ %p ] Calling connectTrees on inner builder %p\n", this, builder);
   builder->connectTrees();
   TraceIL("\n[ %p ] Returned from connectTrees on %p\n", this, builder);

   TR::Block **innerBlocks = builder->blocks();
   uint32_t innerNumBlocks = builder->numBlocks();

   uint32_t copyBlock = *currentBlock;
   for (uint32_t i=0;i < innerNumBlocks;i++, copyBlock++)
      {
      if (TraceEnabled)
         {
         TraceIL("[ %p ] copying inner block B%d to B%d\n", this, i, copyBlock);
         printBlock(innerBlocks[i]);
         }
      _blocks[copyBlock] = innerBlocks[i];
      }

   *currentBlock += innerNumBlocks;
   *firstTree = innerBlocks[0]->getEntry();
   *newLastTree = innerBlocks[innerNumBlocks-1]->getExit();
   }

bool
IlBuilder::connectTrees()
   {
   // don't do this more than once per builder object
   if (_connectedTrees)
      return true;

   _connectedTrees = true;

   TraceIL("[ %p ] TR::IlBuilder::connectTrees():\n", this);

   // figure out how many blocks we need
   uint32_t numBlocks = countBlocks();
   TraceIL("[ %p ] Total %d blocks\n", this, numBlocks);

   allocateBlocks(numBlocks);
   TraceIL("[ %p ] Allocated _blocks %p\n", this, _blocks);

   // now add all the blocks from the sequence into the _blocks array
   // and connect the trees into a single sequence
   uint32_t currentBlock = 0;
   TR::Block **blocks = _blocks;
   TR::TreeTop *lastTree = NULL;

   TraceIL("[ %p ] Connecting trees one entry at a time:\n", this);
   ListIterator<SequenceEntry> iter(_sequence);
   for (SequenceEntry *entry = iter.getFirst(); !iter.atEnd(); entry = iter.getNext())
      {
      TraceIL("[ %p ] currentBlock = %d\n", this, currentBlock);
      TraceIL("[ %p ] lastTree = %p\n", this, lastTree);
      TR::TreeTop *firstTree = NULL;
      TR::TreeTop *newLastTree = NULL;
      if (entry->_isBlock)
         {
         if (TraceEnabled)
            {
            TraceIL("[ %p ] Block entry %p becomes block B%d\n", this, entry->_block, currentBlock);
            printBlock(entry->_block);
            }
         blocks[currentBlock++] = entry->_block;
         firstTree = entry->_block->getEntry();
         newLastTree = entry->_block->getExit();
         }
      else
         {
         TR::IlBuilder *builder = entry->_builder;
         pullInBuilderTrees(builder, &currentBlock, &firstTree, &newLastTree);
         }

      TraceIL("[ %p ] First tree is %p [ node %p ]\n", this, firstTree, firstTree->getNode());
      TraceIL("[ %p ] Last tree will be %p [ node %p ]\n", this, newLastTree, newLastTree->getNode());

      // connect the trees
      if (lastTree)
         {
         TraceIL("[ %p ] Connecting tree %p [ node %p ] to new tree %p [ node %p ]\n", this, lastTree, lastTree->getNode(), firstTree, firstTree->getNode());
         lastTree->join(firstTree);
         }

      lastTree = newLastTree;
      }

   if (_methodBuilder != this)
      {
      // non-method builders need to append the "EXIT" block trees
      // (method builders have EXIT as the special block 1)
      TraceIL("[ %p ] Connecting last tree %p [ node %p ] to exit block entry %p [ node %p ]\n", this, lastTree, lastTree->getNode(), _exitBlock->getEntry(), _exitBlock->getEntry()->getNode());
      lastTree->join(_exitBlock->getEntry());

      // also add exit block to blocks array and add an edge from last block to EXIT if the builder comes back (i.e. doesn't return or branch off somewhere)
      if (comesBack())
         cfg()->addEdge(blocks[currentBlock-1], getExit());

      blocks[currentBlock++] = _exitBlock;
      TraceIL("[ %p ] exit block %d is %p (%p -> %p)\n", this, _exitBlock->getNumber(), _exitBlock->getEntry(), _exitBlock->getExit());
      lastTree = _exitBlock->getExit();

      _numBlocks = currentBlock;
      }

   TraceIL("[ %p ] last tree %p [ node %p ]\n", this, lastTree, lastTree->getNode());
   lastTree->setNextTreeTop(NULL);
   TraceIL("[ %p ] blocks[%d] exit tree is %p\n", this, currentBlock-1, blocks[currentBlock-1]->getExit());

   return true;
   }

TR::Node *
IlBuilder::zero(TR::DataType dt)
   {
   switch (dt)
      {
      case TR::Int8 :  return TR::Node::bconst(0);
      case TR::Int16 : return TR::Node::sconst(0);
      default :         return TR::Node::create(TR::ILOpCode::constOpCode(dt), 0, 0);
      }
   }

TR::Node *
IlBuilder::zero(TR::IlType *dt)
   {
   return zero(dt->getPrimitiveType());
   }

TR::Node *
IlBuilder::zeroNodeForValue(TR::IlValue *v)
   {
   return zero(v->getSymbol()->getDataType());
   }

TR::IlValue *
IlBuilder::zeroForValue(TR::IlValue *v)
   {
   TR::IlValue *returnValue = newValue(v->getSymbol()->getDataType());
   storeNode(returnValue, zeroNodeForValue(v));
   return returnValue;
   }


TR::IlBuilder *
IlBuilder::OrphanBuilder()
   {
   TR::IlBuilder *orphan = new (comp()->trHeapMemory()) TR::IlBuilder(_methodBuilder, _types);
   orphan->initialize(_details, _methodSymbol, _fe, _symRefTab);
   orphan->setupForBuildIL();
   ILB_REPLAY("%s = %s->OrphanBuilder();", REPLAY_BUILDER(orphan), REPLAY_BUILDER(this));
   return orphan;
   }


TR::Block *
IlBuilder::emptyBlock()
   {
   TR::Block *empty = TR::Block::createEmptyBlock(NULL, _comp);
   cfg()->addNode(empty);
   return empty;
   }


TR::IlBuilder *
IlBuilder::createBuilderIfNeeded(TR::IlBuilder *builder)
   {
   if (builder == NULL)
      builder = OrphanBuilder();
   return builder;
   }

IlBuilder::SequenceEntry *
IlBuilder::blockEntry(TR::Block *block)
   {
   return new (_comp->trMemory()->trHeapMemory()) IlBuilder::SequenceEntry(block);
   }

IlBuilder::SequenceEntry *
IlBuilder::builderEntry(TR::IlBuilder *builder)
   {
   return new (_comp->trMemory()->trHeapMemory()) IlBuilder::SequenceEntry(builder);
   }

void
IlBuilder::appendBlock(TR::Block *newBlock, bool addEdge)
   {
   if (newBlock == NULL)
      {
      newBlock = emptyBlock();
      }

   _sequenceAppender->add(blockEntry(newBlock));

   if (_currentBlock && addEdge)
      {
      // if current block does not fall through to new block, use appendNoFallThroughBlock() rather than appendBlock()
      cfg()->addEdge(_currentBlock, newBlock);
      }

   // subsequent IL should generate to appended block
   _currentBlock = newBlock;
   }

void
IlBuilder::AppendBuilder(TR::IlBuilder *builder)
   {
   ILB_REPLAY("%s->AppendBuilder(%s);", REPLAY_BUILDER(this), REPLAY_BUILDER(builder));
   TR_ASSERT(builder->_partOfSequence == false, "builder cannot be in two places");

   builder->_partOfSequence = true;
   _sequenceAppender->add(builderEntry(builder));
   if (_currentBlock != NULL)
      {
      cfg()->addEdge(_currentBlock, builder->getEntry());
      _currentBlock = NULL;
      }

   TraceIL("IlBuilder[ %p ]::AppendBuilder %p\n", this, builder);

   // when trees are connected, exit block will swing down to become last trees in builder, so it can fall through to this block we're about to create
   // need to add edge explicitly because of this exit block sleight of hand
   appendNoFallThroughBlock();
   cfg()->addEdge(builder->getExit(), _currentBlock);
   }

TR::Node *
IlBuilder::loadValue(TR::IlValue *v)
   {
   return TR::Node::createLoad(v);
   }

void
IlBuilder::storeNode(TR::IlValue *dest, TR::Node *v)
   {
   appendBlock();
   genTreeTop(TR::Node::createStore(dest, v));
   }

void
IlBuilder::indirectStoreNode(TR::Node *addr, TR::Node *v)
   {
   TR::DataType dt = v->getDataType();
   TR::SymbolReference *storeSymRef = symRefTab()->findOrCreateArrayShadowSymbolRef(dt, addr);
   TR::ILOpCodes storeOp = comp()->il.opCodeForIndirectArrayStore(dt);
   genTreeTop(TR::Node::createWithSymRef(storeOp, 2, addr, v, 0, storeSymRef));
   }

TR::IlValue *
IlBuilder::indirectLoadNode(TR::IlType *dt, TR::Node *addr, bool isVectorLoad)
   {
   TR_ASSERT(dt->isPointer(), "indirectLoadNode must apply to pointer type");
   TR::IlType *baseType = dt->baseType();
   TR::DataType primType = baseType->getPrimitiveType();
   TR::DataType symRefType = primType;
   if (isVectorLoad)
      symRefType = symRefType.scalarToVector();

   TR::SymbolReference *storeSymRef = symRefTab()->findOrCreateArrayShadowSymbolRef(symRefType, addr);

   TR::ILOpCodes loadOp = comp()->il.opCodeForIndirectArrayLoad(primType);
   if (isVectorLoad)
      {
      loadOp = TR::ILOpCode::convertScalarToVector(loadOp);
      baseType = _types->PrimitiveType(symRefType);
      }

   TR::Node *loadNode = TR::Node::createWithSymRef(loadOp, 1, 1, addr, storeSymRef);

   TR::IlValue *loadValue = newValue(baseType);
   storeNode(loadValue, loadNode);
   return loadValue;
   }

void
IlBuilder::Store(const char *varName, TR::IlValue *value)
   {
   ILB_REPLAY("%s->Store(\"%s\", %s);", REPLAY_BUILDER(this), varName, REPLAY_VALUE(value));

   if (!_methodBuilder->symbolDefined(varName))
      _methodBuilder->defineValue(varName, _types->PrimitiveType(value->getSymbol()->getDataType()));
   TR::IlValue *sym = lookupSymbol(varName);

   TraceIL("IlBuilder[ %p ]::Store %s %d gets %d\n", this, varName, sym->getCPIndex(), value->getCPIndex());
   storeNode(sym, loadValue(value));
   }

void
IlBuilder::VectorStore(const char *varName, TR::IlValue *value)
   {
   ILB_REPLAY("%s->Store(\"%s\", %s);", REPLAY_BUILDER(this), varName, REPLAY_VALUE(value));

   TR::Node *valueNode = loadValue(value);
   TR::DataType dt = valueNode->getDataType();
   if (!dt.isVector())
      {
      valueNode = TR::Node::create(TR::vsplats, 1, valueNode);
      dt = dt.scalarToVector();
      }

   if (!_methodBuilder->symbolDefined(varName))
      _methodBuilder->defineValue(varName, _types->PrimitiveType(dt));
   TR::IlValue *sym = lookupSymbol(varName);

   TraceIL("IlBuilder[ %p ]::VectorStore %s %d gets %d\n", this, varName, sym->getCPIndex(), value->getCPIndex());
   storeNode(sym, valueNode);
   }

void
IlBuilder::StoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   ILB_REPLAY("%s->StoreAt(%s, %s);", REPLAY_BUILDER(this), REPLAY_VALUE(address), REPLAY_VALUE(value));

   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "StoreAt needs an address operand");

   TraceIL("IlBuilder[ %p ]::StoreAt address %d gets %d\n", this, address->getCPIndex(), value->getCPIndex());
   indirectStoreNode(loadValue(address), loadValue(value));
   }

void
IlBuilder::VectorStoreAt(TR::IlValue *address, TR::IlValue *value)
   {
   ILB_REPLAY("%s->VectorStoreAt(%s, %s);", REPLAY_BUILDER(this), REPLAY_VALUE(address), REPLAY_VALUE(value));

   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "StoreAt needs an address operand");

   TraceIL("IlBuilder[ %p ]::VectorStoreAt address %d gets %d\n", this, address->getCPIndex(), value->getCPIndex());

   TR::Node *valueNode = loadValue(value);

   if (!valueNode->getDataType().isVector())
      valueNode = TR::Node::create(TR::vsplats, 1, valueNode);

   indirectStoreNode(loadValue(address), valueNode);
   }

TR::IlValue *
IlBuilder::CreateLocalArray(int32_t numElements, TR::IlType *elementType)
   {
   uint32_t size = numElements * elementType->getSize();
   TR::SymbolReference *localArraySymRef = symRefTab()->createLocalPrimArray(size,
                                                                             methodSymbol(),
                                                                             8 /*FIXME: JVM-specific - byte*/);
   localArraySymRef->setStackAllocatedArrayAccess();

   TR::Node *arrayAddress = TR::Node::createWithSymRef(TR::loadaddr, 0, localArraySymRef);
   TR::IlValue *arrayAddressValue = newValue(TR::Address);
   storeNode(arrayAddressValue, arrayAddress);

   ILB_REPLAY("%s = %s->CreateLocalArray(%d, %s);", REPLAY_VALUE(arrayAddressValue), REPLAY_BUILDER(this), numElements, REPLAY_TYPE(elementType));

   TraceIL("IlBuilder[ %p ]::CreateLocalArray array allocated %d bytes, address in %d\n", this, size, arrayAddressValue->getCPIndex());
   return arrayAddressValue;

   }

TR::IlValue *
IlBuilder::CreateLocalStruct(TR::IlType *structType)
   {
   //similar to CreateLocalArray except writing a method in StructType to get the struct size
   uint32_t size = structType->getSize();
   TR::SymbolReference *localStructSymRef = symRefTab()->createLocalPrimArray(size,
                                                                             methodSymbol(),
                                                                             8 /*FIXME: JVM-specific - byte*/);
   localStructSymRef->setStackAllocatedArrayAccess();

   TR::Node *structAddress = TR::Node::createWithSymRef(TR::loadaddr, 0, localStructSymRef);
   TR::IlValue *structAddressValue = newValue(TR::Address);
   storeNode(structAddressValue, structAddress);

   ILB_REPLAY("%s = %s->CreateLocalStruct(%s);", REPLAY_VALUE(structAddressValue), REPLAY_BUILDER(this), REPLAY_TYPE(newStructType));

   TraceIL("IlBuilder[ %p ]::CreateLocalStruct struct allocated %d bytes, address in %d\n", this, size, structAddressValue->getCPIndex());
   return structAddressValue;
   }

void
IlBuilder::StoreIndirect(const char *type, const char *field, TR::IlValue *object, TR::IlValue *value)
  {
  ILB_REPLAY("%s->StoreIndirect(\"%s\", \"%s\", %s, %s);", REPLAY_BUILDER(this), type, field, REPLAY_VALUE(object), REPLAY_VALUE(value));

  TR::SymbolReference *symRef = (TR::SymbolReference*)_types->FieldReference(type, field);
  TR::DataType fieldType = symRef->getSymbol()->getDataType();
  TraceIL("IlBuilder[ %p ]::StoreIndirect %s.%s (%d) into (%d)\n", this, type, field, value->getCPIndex(), object->getCPIndex());
  TR::ILOpCodes storeOp = comp()->il.opCodeForIndirectStore(fieldType);
  genTreeTop(TR::Node::createWithSymRef(storeOp, 2, loadValue(object), loadValue(value), 0, symRef));
  }

TR::IlValue *
IlBuilder::Load(const char *name)
   {
   TR::IlValue *nameSym = lookupSymbol(name);
   // looks awful much of the time, but we need to anchor the load in case symbol's value is changed later
   // copy propagation should clean it up
   appendBlock();
   TR::IlValue *returnValue = newValue(nameSym->getSymbol()->getDataType());
   TraceIL("IlBuilder[ %p ]::%d is Load %s (%d)\n", this, returnValue->getCPIndex(), name, nameSym->getCPIndex());
   storeNode(returnValue, TR::Node::createLoad(nameSym));

   ILB_REPLAY("%s = %s->Load(\"%s\");", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), name)
   return returnValue;
   }

TR::IlValue *
IlBuilder::VectorLoad(const char *name)
   {
   TR::IlValue *nameSymRef = lookupSymbol(name);
   appendBlock();
   TR::DataType returnType = nameSymRef->getSymbol()->getDataType();
   TR_ASSERT(returnType.isVector(), "VectorLoad must load symbol with a vector type");
   TR::IlValue *returnValue = newValue(returnType);
   TraceIL("IlBuilder[ %p ]::%d is VectorLoad %s (%d)\n", this, returnValue->getCPIndex(), name, nameSymRef->getCPIndex());

   TR::Node *loadNode = TR::Node::createWithSymRef(0, TR::comp()->il.opCodeForDirectLoad(returnType), 0, nameSymRef);
   storeNode(returnValue, loadNode);

   ILB_REPLAY("%s = %s->Load(\"%s\");", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), name)
   return returnValue;
   }

TR::IlValue *
IlBuilder::LoadIndirect(const char *type, const char *field, TR::IlValue *object)
   {
   TR::SymbolReference *symRef = (TR::SymbolReference *)_types->FieldReference(type, field);
   TR::DataType fieldType = symRef->getSymbol()->getDataType();
   TR::IlValue *returnValue = newValue(fieldType);
   TraceIL("IlBuilder[ %p ]::%d is LoadIndirect %s.%s from (%d)\n", this, returnValue->getCPIndex(), type, field, object->getCPIndex());
   storeNode(returnValue, TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(fieldType), 1, loadValue(object), 0, symRef));
   ILB_REPLAY("%s = %s->LoadIndirect(%s, %s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), type, field, REPLAY_VALUE(object));
   return returnValue;
   }

TR::IlValue *
IlBuilder::LoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "LoadAt needs an address operand");
   TraceIL("IlBuilder[ %p ]::LoadAt type %d address %d\n", this, dt->getPrimitiveType(), address->getCPIndex());
   TR::IlValue *returnValue = indirectLoadNode(dt, loadValue(address));
   ILB_REPLAY("%s = %s->LoadAt(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(address));
   return returnValue;
   }

TR::IlValue *
IlBuilder::VectorLoadAt(TR::IlType *dt, TR::IlValue *address)
   {
   TR_ASSERT(address->getSymbol()->getDataType() == TR::Address, "LoadAt needs an address operand");
   TraceIL("IlBuilder[ %p ]::LoadAt type %d address %d\n", this, dt->getPrimitiveType(), address->getCPIndex());
   TR::IlValue *returnValue = indirectLoadNode(dt, loadValue(address), true);
   ILB_REPLAY("%s = %s->LoadAt(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(address));
   return returnValue;
   }

TR::IlValue *
IlBuilder::IndexAt(TR::IlType *dt, TR::IlValue *base, TR::IlValue *index)
   {
   TR::IlType *elemType = dt->baseType();
   TR_ASSERT(elemType != NULL, "IndexAt should be called with pointer type");

   TR::Node *baseNode = TR::Node::createLoad(base);
   TR::Node *indexNode = TR::Node::createLoad(index);
   TR::Node *elemSizeNode;
   TR::ILOpCodes addOp, mulOp;
   TR::DataType indexType = indexNode->getSymbol()->getDataType();
   if (TR::Compiler->target.is64Bit())
      {
      if (indexType != TR::Int64)
         {
         TR::ILOpCodes op = TR::DataType::getDataTypeConversion(indexType, TR::Int64);
         indexNode = TR::Node::create(op, 1, indexNode);
         }
      elemSizeNode = TR::Node::lconst(elemType->getSize());
      addOp = TR::aladd;
      mulOp = TR::lmul;
      }
   else
      {
      TR::DataType targetType = TR::Int32;
      if (indexType != targetType)
         {
         TR::ILOpCodes op = TR::DataType::getDataTypeConversion(indexType, targetType);
         indexNode = TR::Node::create(op, 1, indexNode);
         }
      elemSizeNode = TR::Node::iconst(elemType->getSize());
      addOp = TR::aiadd;
      mulOp = TR::imul;
      }

   TR::Node *offsetNode = TR::Node::create(mulOp, 2, indexNode, elemSizeNode);
   TR::Node *addrNode = TR::Node::create(addOp, 2, baseNode, offsetNode);

   TR::IlValue *address = newValue(Address);
   storeNode(address, addrNode);

   ILB_REPLAY("%s = %s->IndexAt(%s, %s, %s);", REPLAY_VALUE(address), REPLAY_BUILDER(this), REPLAY_TYPE(dt), REPLAY_VALUE(base), REPLAY_VALUE(index));

   return address;
   }

TR::IlValue *
IlBuilder::NullAddress()
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Address);
   storeNode(returnValue, TR::Node::aconst(0));
   ILB_REPLAY("%s = %s->NullAddress();", REPLAY_VALUE(address), REPLAY_BUILDER(this));
   TraceIL("IlBuilder[ %p ]::%d is NullAddress\n", this, returnValue->getCPIndex());
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt8(int8_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Int8);
   storeNode(returnValue, TR::Node::bconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt8 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt8(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt16(int16_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Int16);
   storeNode(returnValue, TR::Node::sconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt16 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt16(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt32(int32_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Int32);
   storeNode(returnValue, TR::Node::iconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt32 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt32(%d);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstInt64(int64_t value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Int64);
   storeNode(returnValue, TR::Node::lconst(value));
   TraceIL("IlBuilder[ %p ]::%d is ConstInt64 %d\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstInt64(%ld);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstFloat(float value)
   {
   appendBlock();
   TR::Node *fconstNode = TR::Node::create(0, TR::fconst, 0);
   fconstNode->setFloat(value);
   TR::IlValue *returnValue = newValue(Float);
   storeNode(returnValue, fconstNode);
   TraceIL("IlBuilder[ %p ]::%d is ConstFloat %f\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstFloat(%f);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstDouble(double value)
   {
   appendBlock();
   TR::Node *dconstNode = TR::Node::create(0, TR::dconst, 0);
   dconstNode->setDouble(value);
   TR::IlValue *returnValue = newValue(Double);
   storeNode(returnValue, dconstNode);
   TraceIL("IlBuilder[ %p ]::%d is ConstDouble %lf\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstDouble(%lf);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstString(const char* value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Address);
   storeNode(returnValue, TR::Node::aconst((uintptrj_t)value));
   TraceIL("IlBuilder[ %p ]::%d is ConstString %p\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstString(%p);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }

TR::IlValue *
IlBuilder::ConstAddress(void* value)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(Address);
   storeNode(returnValue, TR::Node::aconst((uintptrj_t)value));
   TraceIL("IlBuilder[ %p ]::%d is ConstAddress %p\n", this, returnValue->getCPIndex(), value);
   ILB_REPLAY("%s = %s->ConstAddress(%p);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), value);
   return returnValue;
   }


TR::IlValue *
IlBuilder::ConvertTo(TR::IlType *t, TR::IlValue *v)
   {
   appendBlock();
   TR::IlValue *convertedValue = newValue(t);
   TR::DataType t1 = v->getSymbol()->getDataType();
   TR::DataType t2 = t->getPrimitiveType();
   TR::ILOpCodes convertOp = TR::DataType::getDataTypeConversion(v->getSymbol()->getDataType(), t->getPrimitiveType());
   TR::Node *result = TR::Node::create(convertOp, 1, loadValue(v));
   storeNode(convertedValue, result);
   ILB_REPLAY("%s = %s->ConvertTo(%s, %s);", REPLAY_VALUE(convertedValue), REPLAY_BUILDER(this), REPLAY_TYPE(t), REPLAY_VALUE(value));
   return convertedValue;
   }

TR::IlValue *
IlBuilder::unaryOp(TR::ILOpCodes op, TR::IlValue *v)
   {
   appendBlock();
   TR::IlValue *returnValue = newValue(v->getSymbol()->getDataType());
   TR::Node *valueNode = loadValue(v);
   TR::Node *result = TR::Node::create(op, 1, valueNode);
   storeNode(returnValue, result);
   return returnValue;
   }

void
IlBuilder::doVectorConversions(TR::Node **leftPtr, TR::Node **rightPtr)
   {
   TR::Node *    left  = *leftPtr;
   TR::DataType lType = left->getDataType();

   TR::Node *    right = *rightPtr;
   TR::DataType rType = right->getDataType();

   if (lType.isVector() && !rType.isVector())
      *rightPtr = TR::Node::create(TR::vsplats, 1, right);

   if (!lType.isVector() && rType.isVector())
      *leftPtr = TR::Node::create(TR::vsplats, 1, left);
   }

TR::Node*
IlBuilder::binaryOpNodeFromNodes(TR::ILOpCodes op,
                             TR::Node *leftNode,
                             TR::Node *rightNode) 
   {
   TR::DataType leftType = leftNode->getDataType();
   TR::DataType rightType = rightNode->getDataType();
   bool isAddressBump = ((leftType == TR::Address) &&
                            (rightType == TR::Int32 || rightType == TR::Int64));
   bool isRevAddressBump = ((rightType == TR::Address) &&
                               (leftType == TR::Int32 || leftType == TR::Int64));
   TR_ASSERT(leftType == rightType || isAddressBump || isRevAddressBump, "binaryOp requires both left and right operands to have same type or one is address and other is Int32/64");

   if (isRevAddressBump) // swap them
      {
      TR::Node *save = leftNode;
      leftNode = rightNode;
      rightNode = save;
      }
   
   return TR::Node::create(op, 2, leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::binaryOpFromNodes(TR::ILOpCodes op,
                             TR::Node *leftNode,
                             TR::Node *rightNode) 
   {
   TR::Node *result = binaryOpNodeFromNodes(op, leftNode, rightNode);
   TR::IlValue *returnValue = newValue(result->getDataType());
   storeNode(returnValue, result);
   return returnValue;
   } 

TR::IlValue *
IlBuilder::binaryOpFromOpMap(OpCodeMapper mapOp,
                             TR::IlValue *left,
                             TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);

   doVectorConversions(&leftNode, &rightNode);

   TR::DataType leftType = leftNode->getDataType();
   return binaryOpFromNodes(mapOp(leftType), leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::binaryOpFromOpCode(TR::ILOpCodes op,
                              TR::IlValue *left,
                              TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   //doVectorConversions(&left, &right);
   return binaryOpFromNodes(op, leftNode, rightNode);
   }

TR::IlValue *
IlBuilder::compareOp(TR_ComparisonTypes ct,
                     bool needUnsigned,
                     TR::IlValue *left,
                     TR::IlValue *right)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes op = TR::ILOpCode::compareOpCode(leftNode->getDataType(), ct, needUnsigned);
   return binaryOpFromOpCode(op, left, right);
   }

TR::IlValue *
IlBuilder::NotEqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpNE, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is NotEqualTo %d != %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->NotEqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

void
IlBuilder::Goto(TR::IlBuilder **dest)
   {
   TraceIL("IlBuilder[ %p ]::Goto %p\n", this, *dest);
   *dest = createBuilderIfNeeded(*dest);
   appendGoto((*dest)->getEntry());
   setDoesNotComeBack();

   ILB_REPLAY("%s->Goto(%s);", REPLAY_BUILDER(this), REPLAY_BUILDER(dest));
   }

void
IlBuilder::Return()
   {
   ILB_REPLAY("%s->Return();", REPLAY_BUILDER(this));
   TraceIL("IlBuilder[ %p ]::Return\n", this);
   appendBlock();
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(TR::NoType));
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   setDoesNotComeBack();
   }

void
IlBuilder::Return(TR::IlValue *value)
   {
   ILB_REPLAY("%s->Return(%s);", REPLAY_BUILDER(this), REPLAY_VALUE(value));
   TraceIL("IlBuilder[ %p ]::Return %d\n", this, value->getCPIndex());
   appendBlock();
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(value->getSymbol()->getDataType()), 1, loadValue(value));
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   setDoesNotComeBack();
   }

TR::IlValue *
IlBuilder::Sub(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::subtractOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Sub %d - %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Sub(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

static TR::ILOpCodes addOpCode(TR::DataType type)
   {
   return TR::ILOpCode::addOpCode(type, TR::Compiler->target.is64Bit());
   }

static TR::ILOpCodes unsignedAddOpCode(TR::DataType type)
   {
   return TR::ILOpCode::unsignedAddOpCode(type, TR::Compiler->target.is64Bit());
   }

TR::IlValue *
IlBuilder::Add(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue = NULL;
   if (left->getSymbol()->getDataType() == TR::Address)
      {
      if (right->getSymbol()->getDataType() == TR::Int32)
         returnValue = binaryOpFromNodes(TR::aiadd, loadValue(left), loadValue(right));
      else if (right->getSymbol()->getDataType() == TR::Int64)
         returnValue = binaryOpFromNodes(TR::aladd, loadValue(left), loadValue(right));
      }
   if (returnValue == NULL)
      returnValue = binaryOpFromOpMap(addOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Add %d + %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Add(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

/*
 * blockThrowsException:
 * ....
 * goto blockAfterExceptionHandler 
 * Handler:
 * ....
 * goto blockAfterExceptionHandler 
 * blockAfterExceptionHandler:
 */
void
IlBuilder::appendExceptionHandler(TR::Block *blockThrowsException, TR::IlBuilder **handler, uint32_t catchType)
   {
   //split block after overflowCHK, and add an goto to blockAfterExceptionHandler
   appendBlock();
   TR::Block *blockWithGoto = _currentBlock;
   TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
   genTreeTop(gotoNode);
   _currentBlock = NULL;
   _currentBlockNumber = -1;
 
   //append handler, add exception edge and merge edge
   *handler = createBuilderIfNeeded(*handler);
   TR_ASSERT(*handler != NULL, "exception handler cannot be NULL\n");
   (*handler)->_isHandler = true;
   cfg()->addExceptionEdge(blockThrowsException, (*handler)->getEntry());
   AppendBuilder(*handler);
   (*handler)->setHandlerInfo(catchType);

   TR::Block *blockAfterExceptionHandler = _currentBlock;
   TraceIL("blockAfterExceptionHandler block_%d, blockWithGoto block_%d \n", blockAfterExceptionHandler->getNumber(), blockWithGoto->getNumber());
   gotoNode->setBranchDestination(blockAfterExceptionHandler->getEntry());
   cfg()->addEdge(blockWithGoto, blockAfterExceptionHandler);
   }

void
IlBuilder::setHandlerInfo(uint32_t catchType)
   {
   TR::Block *catchBlock = getEntry();
   catchBlock->setIsCold();
   catchBlock->setHandlerInfoWithOutBCInfo(catchType, comp()->getInlineDepth(), -1, _methodSymbol->getResolvedMethod(), comp());
   }

TR::Node*
IlBuilder::genOverflowCHKTreeTop(TR::Node *operationNode)
   {
   TR::Node *overflowChkNode = TR::Node::createWithRoomForOneMore(TR::OverflowCHK, 3, symRefTab()->findOrCreateOverflowCheckSymbolRef(_methodSymbol), operationNode, operationNode->getFirstChild(), operationNode->getSecondChild());
   overflowChkNode->setOverflowCHKOperation(operationNode->getOpCodeValue());
   genTreeTop(overflowChkNode);
   return overflowChkNode;
   }

TR::IlValue *
IlBuilder::operationWithOverflow(TR::ILOpCodes op, TR::Node *leftNode, TR::Node *rightNode, TR::IlBuilder **handler)
   {
   /*
    * BB1:
    *    overflowCHK
    *       operation(add/sub/mul)
    *          child1
    *          child2
    *       =>child1
    *       =>child2
    *    store
    *       => operation
    * BB2:
    *    goto BB3 
    * Handler:
    *    ...
    * BB3:
    *    continue
    */
   TR::Node *operationNode = binaryOpNodeFromNodes(op, leftNode, rightNode);
   TR::Node *overflowChkNode = genOverflowCHKTreeTop(operationNode);

   TR::Block *blockWithOverflowCHK = _currentBlock;
   TR::IlValue *resultValue = newValue(operationNode->getDataType());
   genTreeTop(TR::Node::createStore(resultValue, operationNode));

   appendExceptionHandler(blockWithOverflowCHK, handler, TR::Block::CanCatchOverflowCheck);
   return resultValue;
   }

TR::IlValue *
IlBuilder::AddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes op;
   if (left->getSymbol()->getDataType() == TR::Address)
      {
      if (right->getSymbol()->getDataType() == TR::Int32)
         op = TR::aiadd;
      else if (right->getSymbol()->getDataType() == TR::Int64)
         op = TR::aladd;
      else
         TR_ASSERT(0, "the right child type must be either TR::Int32 or TR::Int64 when the left child of Add is TR::Address\n");
      }
   else op = addOpCode(leftNode->getDataType());

   TR::IlValue *addValue = operationWithOverflow(op, leftNode, rightNode, handler);
   TraceIL("IlBuilder[ %p ]::%d is AddWithOverflow %d + %d\n", this, addValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   //ILB_REPLAY("%s = %s->AddWithOverflow(%s, %s);", REPLAY_VALUE(addValue), REPLAY_BUILDER(this), REPLAY_BUILDER(*handler), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return addValue;
   }

TR::IlValue *
IlBuilder::UnsignedAddWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::ILOpCodes op;
   if (left->getSymbol()->getDataType() == TR::Address)
      {    
      if (right->getSymbol()->getDataType() == TR::Int32)
         op = TR::aiuadd;
      else if (right->getSymbol()->getDataType() == TR::Int64)
         op = TR::aluadd;
      else 
         TR_ASSERT(0, "the right child type must be either TR::Int32 or TR::Int64 when the left child of Add is TR::Address\n");
      }    
   else op = unsignedAddOpCode(leftNode->getDataType());

   TR::IlValue *addValue = operationWithOverflow(op, leftNode, rightNode, handler);
   TraceIL("IlBuilder[ %p ]::%d is UnsignedAddWithOverflow %d + %d\n", this, addValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   //ILB_REPLAY("%s = %s->UnsignedAddWithOverflow(%s, %s, %s);", REPLAY_VALUE(addValue), REPLAY_BUILDER(this), REPLAY_PTRTOBUILDER(handler), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return addValue;
   }

TR::IlValue *
IlBuilder::SubWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::IlValue *subValue = operationWithOverflow(TR::ILOpCode::subtractOpCode(leftNode->getDataType()), leftNode, rightNode, handler);
   TraceIL("IlBuilder[ %p ]::%d is SubWithOverflow %d + %d\n", this, subValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   return subValue;
   }

TR::IlValue *
IlBuilder::MulWithOverflow(TR::IlBuilder **handler, TR::IlValue *left, TR::IlValue *right)
   {
   appendBlock(); 

   TR::Node *leftNode = loadValue(left);
   TR::Node *rightNode = loadValue(right);
   TR::IlValue *mulValue = operationWithOverflow(TR::ILOpCode::multiplyOpCode(leftNode->getDataType()), leftNode, rightNode, handler);
   TraceIL("IlBuilder[ %p ]::%d is MulWithOverflow %d + %d\n", this, mulValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   return mulValue;
   }

TR::IlValue *
IlBuilder::Mul(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::multiplyOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Mul %d * %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Mul(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Div(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::divideOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Div %d / %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Div(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::And(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::andOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is And %d & %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->And(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Or(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::orOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Or %d | %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Or(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Xor(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::xorOpCode, left, right);
   TraceIL("IlBuilder[ %p ]::%d is Xor %d ^ %d\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->Xor(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::ShiftL(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::shiftLeftOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is shr %d << %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->ShiftL(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

TR::IlValue *
IlBuilder::ShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::shiftRightOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is shr %d << %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->ShiftR(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

TR::IlValue *
IlBuilder::UnsignedShiftR(TR::IlValue *v, TR::IlValue *amount)
   {
   TR::IlValue *returnValue=binaryOpFromOpMap(TR::ILOpCode::unsignedShiftRightOpCode, v, amount);
   TraceIL("IlBuilder[ %p ]::%d is shr %d << %d\n", this, returnValue->getCPIndex(), v->getCPIndex(), amount->getCPIndex());
   ILB_REPLAY("%s = %s->UnsignedShiftR(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(v), REPLAY_VALUE(amount));
   return returnValue;
   }

TR::IlValue *
IlBuilder::EqualTo(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpEQ, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is EqualTo %d == %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->EqualTo(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::LessThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpLT, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is LessThan %d < %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->LessThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::GreaterThan(TR::IlValue *left, TR::IlValue *right)
   {
   TR::IlValue *returnValue=compareOp(TR_cmpGT, false, left, right);
   TraceIL("IlBuilder[ %p ]::%d is GreaterThan %d < %d?\n", this, returnValue->getCPIndex(), left->getCPIndex(), right->getCPIndex());
   ILB_REPLAY("%s = %s->GreaterThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   return returnValue;
   }

TR::IlValue *
IlBuilder::Call(const char *functionName, int32_t numArgs, ...)
   {
   TR::IlValue ** argValues = (TR::IlValue **) _comp->trMemory()->allocateHeapMemory(numArgs * sizeof(TR::IlValue *));
   va_list args;

   va_start(args, numArgs);
   for (int32_t a=0;a < numArgs;a++)
      {
      argValues[a] = va_arg(args, TR::IlValue*);
      }
   va_end(args);

   return Call(functionName, numArgs, argValues);
   }

TR::IlValue *
IlBuilder::Call(const char *functionName, int32_t numArgs, TR::IlValue ** argValues)
   {
   //   ILB_REPLAY("%s = %s->GreaterThan(%s, %s);", REPLAY_VALUE(returnValue), REPLAY_BUILDER(this), REPLAY_VALUE(left), REPLAY_VALUE(right));
   // TODO: figure out Call REPLAY

   TraceIL("IlBuilder[ %p ]::Call %s\n", this, functionName);
   appendBlock();

   TR::ResolvedMethod *resolvedMethod = _methodBuilder->lookupFunction(functionName);
   TR::DataType returnType = resolvedMethod->returnType();

   // treat as "Static" (so no receiver expected) and use a direct call opcode
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateMethodSymbol(JITTED_METHOD_INDEX, -1, resolvedMethod, TR::MethodSymbol::Static);
   TR::Node *callNode = TR::Node::createWithSymRef(TR::ILOpCode::getDirectCall(returnType), numArgs, methodSymRef);

   // TODO: should really verify argument types here
   int32_t childIndex = 0;
   for (int32_t a=0;a < numArgs;a++)
      {
      TR::IlValue *arg = argValues[a];
      callNode->setAndIncChild(childIndex++, loadValue(arg));
      }

   if (returnType != TR::NoType)
      {
      TR::IlValue *returnValue = newValue(callNode->getDataType());
      storeNode(returnValue, callNode);
      return returnValue;
      }

   // call with no return value needs to be anchored
   genTreeTop(callNode);

   return NULL;
   }

TR::IlValue *
IlBuilder::AtomicIntegerAdd(TR::IlValue * object, TR::IlValue * offset, TR::IlValue * value)
   {
   TraceIL("IlBuilder[ %p ]::AtomicIntegerAdd (%d, %d, %d)\n", this, object->getCPIndex(), offset->getCPIndex(), value->getCPIndex());
   appendBlock();

   TR::DataType returnType = TR::Int32;
   
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateCodeGenInlinedHelper(TR::SymbolReferenceTable::atomicAdd32BitSymbol); //lock add
   TR::Node *callNode = TR::Node::createWithSymRef(TR::ILOpCode::getDirectCall(returnType), 3, methodSymRef);

   callNode->setAndIncChild(0, loadValue(object));
   callNode->setAndIncChild(1, loadValue(offset));
   callNode->setAndIncChild(2, loadValue(value));

   genTreeTop(callNode); 
   TR::IlValue *returnValue = newValue(callNode->getDataType());
   genTreeTop(TR::Node::createStore(returnValue, callNode)); 
   
   return returnValue; 
   }

TR::IlValue *
IlBuilder::AtomicIntegerAdd(TR::IlValue * object, TR::IlValue * value)
   {
   TraceIL("IlBuilder[ %p ]::AtomicIntegerAdd (%d, %d)\n", this, object->getCPIndex(), value->getCPIndex());
   appendBlock();

   TR::DataType returnType = TR::Int32;
   
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateCodeGenInlinedHelper(TR::SymbolReferenceTable::atomicAdd32BitSymbol); //lock add
   TR::Node *callNode = TR::Node::createWithSymRef(TR::ILOpCode::getDirectCall(returnType), 2, methodSymRef);

   callNode->setAndIncChild(0, loadValue(object));
   callNode->setAndIncChild(1, loadValue(value));

   genTreeTop(callNode); 
   TR::IlValue *returnValue = newValue(callNode->getDataType());
   genTreeTop(TR::Node::createStore(returnValue, callNode)); 
   
   return returnValue;    
   }

void
IlBuilder::IfCmpNotEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpNotEqualZero(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(condition));
   TraceIL("IlBuilder[ %p ]::IfCmpNotEqualZero %d? -> B%d\n", this, condition->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpNotEqualZero(condition, (*target)->getEntry());
   }

void
IlBuilder::IfCmpNotEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpNotEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpNotEqual %d == %d? -> B%d\n", this, left->getCPIndex(), right->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpNotEqual(left, right, (*target)->getEntry());
   }

void
IlBuilder::IfCmpEqualZero(TR::IlBuilder **target, TR::IlValue *condition)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpEqualZero(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(condition));
   TraceIL("IlBuilder[ %p ]::IfCmpEqualZero %d == 0? -> B%d\n", this, condition->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpEqualZero(condition, (*target)->getEntry());
   }

void
IlBuilder::IfCmpEqual(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpEqual(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpEqual %d == %d? -> B%d\n", this, left->getCPIndex(), right->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpEqual(left, right, (*target)->getEntry());
   }

void
IlBuilder::IfCmpLessThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpLessThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpLessThan %d < %d? -> B%d\n", this, left->getCPIndex(), right->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpLessThan(left, right, (*target)->getEntry());
   }

void
IlBuilder::IfCmpGreaterThan(TR::IlBuilder **target, TR::IlValue *left, TR::IlValue *right)
   {
   *target = createBuilderIfNeeded(*target);
   ILB_REPLAY("%s->IfCmpGreaterThan(%s, %s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   TraceIL("IlBuilder[ %p ]::IfCmpGreatThan %d < %d? -> B%d\n", this, left->getCPIndex(), right->getCPIndex(), (*target)->getEntry()->getNumber());
   ifCmpGreaterThan(left, right, (*target)->getEntry());
   }

void
IlBuilder::ifCmpNotEqualZero(TR::IlValue *condition, TR::Block *target)
   {
   ifCmpNotEqual(condition, zeroForValue(condition), target);
   }

void
IlBuilder::ifCmpNotEqual(TR::IlValue *left, TR::IlValue *right, TR::Block *target)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   ifjump(comp()->il.opCodeForIfCompareNotEquals(leftNode->getDataType()),
          leftNode,
          loadValue(right),
          target);
   appendBlock();
   }

void
IlBuilder::ifCmpEqualZero(TR::IlValue *condition, TR::Block *target)
   {
   ifCmpEqual(condition, zeroForValue(condition), target);
   }

void
IlBuilder::ifCmpEqual(TR::IlValue *left, TR::IlValue *right, TR::Block *target)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   ifjump(comp()->il.opCodeForIfCompareEquals(leftNode->getDataType()),
          leftNode,
          loadValue(right),
          target);
   appendBlock();
   }

void
IlBuilder::ifCmpLessThan(TR::IlValue *left, TR::IlValue *right, TR::Block *target)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   ifjump(comp()->il.opCodeForIfCompareLessThan(leftNode->getDataType()),
          leftNode,
          loadValue(right),
          target);
   appendBlock();
   }

void
IlBuilder::ifCmpGreaterThan(TR::IlValue *left, TR::IlValue *right, TR::Block *target)
   {
   appendBlock();
   TR::Node *leftNode = loadValue(left);
   ifjump(comp()->il.opCodeForIfCompareGreaterThan(leftNode->getDataType()),
          leftNode,
          loadValue(right),
          target);
   appendBlock();
   }

void
IlBuilder::appendGoto(TR::Block *destBlock)
   {
   appendBlock();
   gotoBlock(destBlock);
   }

/* Flexible builder for if...then...else structures
 * Basically 3 ways to call it:
 *    1) with then and else paths
 *    2) only a then path
 *    3) only an else path
 * #2 and #3 are similar: the existing path is appended and the proper condition causes branch around to the merge point
 * #1: then path exists "out of line", else path falls through from If and then jumps to merge, followed by then path which
 *     falls through to the merge point
 */
void
IlBuilder::IfThenElse(TR::IlBuilder **thenPath, TR::IlBuilder **elsePath, TR::IlValue *condition)
   {
   //ILB_REPLAY_BEGIN();
   TR_ASSERT(thenPath != NULL || elsePath != NULL, "IfThenElse needs at least one conditional path");

   TR::Block *thenEntry = NULL;
   TR::Block *elseEntry = NULL;

   if (thenPath)
      {
      *thenPath = createBuilderIfNeeded(*thenPath);
      thenEntry = (*thenPath)->getEntry();
      }

   if (elsePath)
      {
      *elsePath = createBuilderIfNeeded(*elsePath);
      elseEntry = (*elsePath)->getEntry();
      }

   TR::Block *mergeBlock = emptyBlock();

   TraceIL("IlBuilder[ %p ]::IfThenElse %d", this, condition->getCPIndex());
   if (thenEntry)
      TraceIL(" then B%d", thenEntry->getNumber());
   if (elseEntry)
      TraceIL(" else B%d", elseEntry->getNumber());
   TraceIL(" merge B%d\n", mergeBlock->getNumber());

   TR::IlValue *zero = zeroForValue(condition);
   if (thenPath == NULL) // case #3
      {
      ifCmpNotEqual(condition, zero, mergeBlock);
      if ((*elsePath)->_partOfSequence)
         gotoBlock(elseEntry);
      else
         AppendBuilder(*elsePath);
      }
   else if (elsePath == NULL) // case #2
      {
      ifCmpEqual(condition, zero, mergeBlock);
      if ((*thenPath)->_partOfSequence)
         gotoBlock(thenEntry);
      else
         AppendBuilder(*thenPath);
      }
   else // case #1
      {
      ifCmpNotEqual(condition, zero, thenEntry);
      if ((*elsePath)->_partOfSequence)
         {
         gotoBlock(elseEntry);
         }
      else
         {
         AppendBuilder(*elsePath);
         appendGoto(mergeBlock);
         }
      if (!(*thenPath)->_partOfSequence)
         AppendBuilder(*thenPath);
      // if then path exists elsewhere already,
      //  then IfCmpNotEqual above already brances to it
      }

   // all paths possibly merge back here
   appendBlock(mergeBlock);
   //ILB_REPLAY_END();
   //ILB_REPLAY("IfThenElse(%s, %s, %s);", REPLAY_PTRTOBUILDER(thenPath), REPLAY_PTRTOBUILDER(elsePath), REPLAY_VALUE(condition));
   }

void
IlBuilder::Switch(const char *selectionVar,
                  TR::IlBuilder **defaultBuilder,
                  uint32_t numCases,
                  int32_t *caseValues,
                  TR::IlBuilder **caseBuilders,
                  bool *caseFallsThrough)
   {
   //ILB_REPLAY_BEGIN();
   TR::IlValue *selectorValue = Load(selectionVar);
   TR_ASSERT(selectorValue->getSymbol()->getDataType() == TR::Int32, "Switch only supports selector having type Int32");
   *defaultBuilder = createBuilderIfNeeded(*defaultBuilder);

   appendBlock();

   TR::Node *defaultNode = TR::Node::createCase(0, (*defaultBuilder)->getEntry()->getEntry());
   TR::Node *lookupNode = TR::Node::create(TR::lookup, numCases + 2, loadValue(selectorValue), defaultNode);

   // get the lookup tree into this builder, even though we haven't completely filled it in yet
   genTreeTop(lookupNode);
   TR::Block *switchBlock = _currentBlock;

   // make sure no fall through edge created from the lookup
   appendNoFallThroughBlock();

   TR::IlBuilder *breakBuilder = OrphanBuilder();

   // each case handler is a sequence of two builder objects: first the one passed in via caseBuilder (or will be passed
   //   back via caseBuilders, and second a builder that branches to the breakBuilder (unless this case falls through)
   for (int32_t c=0;c < numCases;c++)
      {
      int32_t value = caseValues[c];
      TR::IlBuilder *handler = NULL;
      if (!caseFallsThrough[c])
         {
         handler = OrphanBuilder();

         caseBuilders[c] = createBuilderIfNeeded(caseBuilders[c]);
         handler->AppendBuilder(caseBuilders[c]);

         // handle "break" with a separate builder so user can add whatever they want into caseBuilders[c]
         TR::IlBuilder *branchToBreak = OrphanBuilder();
         branchToBreak->Goto(&breakBuilder);
         handler->AppendBuilder(branchToBreak);
         }
      else
         {
         caseBuilders[c] = createBuilderIfNeeded(caseBuilders[c]);
         handler = caseBuilders[c];
         }

      TR::Block *caseBlock = handler->getEntry();
      cfg()->addEdge(switchBlock, caseBlock);
      AppendBuilder(handler);

      TR::Node *caseNode = TR::Node::createCase(0, caseBlock->getEntry(), value);
      lookupNode->setAndIncChild(c+2, caseNode);
      }

   cfg()->addEdge(switchBlock, (*defaultBuilder)->getEntry());
   AppendBuilder(*defaultBuilder);

   AppendBuilder(breakBuilder);
   //ILB_REPLAY_END();
   }

void
IlBuilder::Switch(const char *selectionVar,
                  TR::IlBuilder **defaultBuilder,
                  uint32_t numCases,
                  ...)
   {
   //TODO figure out switch replay
   // ILB_REPLAY("%s->IfCmpGreaterThan(%s, %s);", REPLAY_BUILDER(this), REPLAY_BUILDER(target), REPLAY_VALUE(left), REPLAY_VALUE(right));
   int32_t *caseValues = (int32_t *) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(int32_t));
   TR_ASSERT(0 != caseValues, "out of memory");

   TR::IlBuilder **caseBuilders = (TR::IlBuilder **) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(TR::IlBuilder *));
   TR_ASSERT(0 != caseBuilders, "out of memory");

   bool *caseFallsThrough = (bool *) _comp->trMemory()->allocateHeapMemory(numCases * sizeof(bool));
   TR_ASSERT(0 != caseFallsThrough, "out of memory");

   va_list cases;
   va_start(cases, numCases);
   for (int32_t c=0;c < numCases;c++)
      {
      caseValues[c] = (int32_t) va_arg(cases, int);
      caseBuilders[c] = *(TR::IlBuilder **) va_arg(cases, TR::IlBuilder **);
      caseFallsThrough[c] = (bool) va_arg(cases, int);
      }
   va_end(cases);

   Switch(selectionVar, defaultBuilder, numCases, caseValues, caseBuilders, caseFallsThrough);

   // if Switch created any new builders, we need to put those back into the arguments passed into this Switch call
   va_start(cases, numCases);
   for (int32_t c=0;c < numCases;c++)
      {
      int throwawayValue = va_arg(cases, int);
      TR::IlBuilder **caseBuilder = va_arg(cases, TR::IlBuilder **);
      (*caseBuilder) = caseBuilders[c];
      int throwAwayFallsThrough = va_arg(cases, int);
      }
   va_end(cases);
   }


void
IlBuilder::ForLoop(bool countsUp,
                   const char *indVar,
                   TR::IlBuilder **loopCode,
                   TR::IlBuilder **breakBuilder,
                   TR::IlBuilder **continueBuilder,
                   TR::IlValue *initial,
                   TR::IlValue *end,
                   TR::IlValue *increment)
   {
   //ILB_REPLAY_BEGIN();

   TR_ASSERT(loopCode != NULL, "ForLoop needs to have loopCode builder");
   *loopCode = createBuilderIfNeeded(*loopCode);

   TraceIL("IlBuilder[ %p ]::ForLoop ind %s initial %d end %d increment %d loopCode %p countsUp %d\n", this, indVar, initial->getCPIndex(), end->getCPIndex(), increment->getCPIndex(), *loopCode, countsUp);

   Store(indVar, initial);

   TR::IlValue *loopCondition;
   TR::IlBuilder *loopBody = OrphanBuilder();
   loopCondition = countsUp ? LessThan(Load(indVar), end) : GreaterThan(Load(indVar), end);
   IfThen(&loopBody, loopCondition);
   loopBody->AppendBuilder(*loopCode);
   TR::IlBuilder *loopContinue = OrphanBuilder();
   loopBody->AppendBuilder(loopContinue);

   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "ForLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = OrphanBuilder();
      AppendBuilder(*breakBuilder);
      }

   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "ForLoop returns continueBuilder, cannot provide continueBuilder as input");
      *continueBuilder = loopContinue;
      }

   if (countsUp)
      {
      loopContinue->Store(indVar,
      loopContinue->   Add(
      loopContinue->      Load(indVar),
                          increment));
      loopContinue->IfCmpLessThan(&loopBody,
      loopContinue->   Load(indVar),
                       end);
      }
   else
      {
      loopContinue->Store(indVar,
      loopContinue->   Sub(
      loopContinue->      Load(indVar),
                          increment));
      loopContinue->IfCmpGreaterThan(&loopBody,
      loopContinue->   Load(indVar),
                       end);
      }
   //ILB_REPLAY_END();
#if 0
   ILB_REPLAY("%s->ForLoopUp(%s, \"%s\", %s, %s, %s, %s, %s, %s, %s);",
          REPLAY_BUILDER(this),
          REPLAY_BOOL(countsUp),
          indVar,
          REPLAY_PTRTOBUILDER(loopCode),
          REPLAY_PTRTOBUILDER(breakBuilder),
          REPLAY_PTRTOBUILDER(continueBuilder),
          REPLAY_VALUE(initial),
          REPLAY_VALUE(end),
          REPLAY_VALUE(increment));
#endif
   }

void
IlBuilder::DoWhileLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   //ILB_REPLAY_BEGIN();

   TR_ASSERT(body != NULL, "doWhileLoop needs to have a body");

   if (!_methodBuilder->symbolDefined(whileCondition))
      _methodBuilder->defineValue(whileCondition, Int32);

   *body = createBuilderIfNeeded(*body);
   TraceIL("IlBuilder[ %p ]::DoWhileLoop do body B%d while %s\n", this, (*body)->getEntry()->getNumber(), whileCondition);

   AppendBuilder(*body);
   TR::IlBuilder *loopContinue = NULL;

   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "DoWhileLoop returns continueBuilder, cannot provide continueBuilder as input");
      loopContinue = *continueBuilder = OrphanBuilder();
      }
   else
      loopContinue = OrphanBuilder();

   AppendBuilder(loopContinue);
   loopContinue->IfCmpNotEqualZero(body,
   loopContinue->   Load(whileCondition));

   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "DoWhileLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = OrphanBuilder();
      AppendBuilder(*breakBuilder);
      }

   //ILB_REPLAY_END();
   //ILB_REPLAY("%s->DoWhileLoop(%s, %s, %s, %s);",
          //REPLAY_BUILDER(this),
          //whileCondition,
          //REPLAY_PTRTOBUILDER(body),
          //REPLAY_PTRTOBUILDER(breakBuilder),
          //REPLAY_PTRTOBUILDER(continueBuilder));
   }

void
IlBuilder::WhileDoLoop(const char *whileCondition, TR::IlBuilder **body, TR::IlBuilder **breakBuilder, TR::IlBuilder **continueBuilder)
   {
   //ILB_REPLAY_BEGIN();

   TR_ASSERT(body != NULL, "WhileDo needs to have a body");
   TraceIL("IlBuilder[ %p ]::WhileDoLoop while %s do body %p\n", this, whileCondition, *body);

   TR::IlBuilder *done = OrphanBuilder();
   if (breakBuilder)
      {
      TR_ASSERT(*breakBuilder == NULL, "WhileDoLoop returns breakBuilder, cannot provide breakBuilder as input");
      *breakBuilder = done;
      }

   TR::IlBuilder *loopContinue = OrphanBuilder();
   if (continueBuilder)
      {
      TR_ASSERT(*continueBuilder == NULL, "WhileDoLoop returns continueBuilder, cannot provide continueBuilder as input");
      *continueBuilder = loopContinue;
      }

   AppendBuilder(loopContinue);
   loopContinue->IfCmpEqualZero(&done,
   loopContinue->   Load(whileCondition));

   *body = createBuilderIfNeeded(*body);
   AppendBuilder(*body);

   Goto(&loopContinue);
   AppendBuilder(done);

   //ILB_REPLAY_END();
   //ILB_REPLAY("%s->WhileDoLoop(%s, %s, %s, %s);",
          //REPLAY_BUILDER(this),
          //whileCondition,
          //REPLAY_PTRTOBUILDER(body),
          //REPLAY_PTRTOBUILDER(breakBuilder),
          //REPLAY_PTRTOBUILDER(continueBuilder));
   }

} // namespace OMR
