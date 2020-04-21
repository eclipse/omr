/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/FrontEnd.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "infra/Cfg.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "ras/ILValidator.hpp"

#define OPT_DETAILS "O^O ILGEN: "

OMR::IlInjector::IlInjector(TR::TypeDictionary *types)
   : TR_IlGenerator(),
   _types(types),
   _comp(NULL),
   _fe(NULL),
   _symRefTab(NULL),
   _details(NULL),
   _methodSymbol(NULL),
   _currentBlock(0),
   _currentBlockNumber(-1),
   _numBlocks(0),
   _blocks(0),
   _blocksAllocatedUpFront(false)
   {
   initPrimitiveTypes();
   }

OMR::IlInjector::IlInjector(TR::IlInjector *source)
   : TR_IlGenerator(),
   _types(source->_types),
   _comp(source->_comp),
   _fe(source->_fe),
   _symRefTab(source->_symRefTab),
   _details(source->_details),
   _methodSymbol(source->_methodSymbol),
   _currentBlock(0),
   _currentBlockNumber(-1),
   _numBlocks(0),
   _blocks(0),
   _blocksAllocatedUpFront(false)
   {
   initPrimitiveTypes();
   }

void
OMR::IlInjector::initPrimitiveTypes()
   {
   NoType       = _types->PrimitiveType(TR::NoType);
   Int8         = _types->PrimitiveType(TR::Int8);
   Int16        = _types->PrimitiveType(TR::Int16);
   Int32        = _types->PrimitiveType(TR::Int32);
   Int64        = _types->PrimitiveType(TR::Int64);
   Float        = _types->PrimitiveType(TR::Float);
   Double       = _types->PrimitiveType(TR::Double);
   Address      = _types->PrimitiveType(TR::Address);
   VectorInt8   = _types->PrimitiveType(TR::VectorInt8);
   VectorInt16  = _types->PrimitiveType(TR::VectorInt16);
   VectorInt32  = _types->PrimitiveType(TR::VectorInt32);
   VectorInt64  = _types->PrimitiveType(TR::VectorInt64);
   VectorFloat  = _types->PrimitiveType(TR::VectorFloat);
   VectorDouble = _types->PrimitiveType(TR::VectorDouble);

   if (TR::Compiler->target.is64Bit())
      Word = Int64;
   else
      Word = Int32;
   }

void
OMR::IlInjector::initialize(TR::IlGeneratorMethodDetails * details,
                            TR::ResolvedMethodSymbol     * methodSymbol,
                            TR::FrontEnd                 * fe,
                            TR::SymbolReferenceTable     * symRefTab)
   {
   _details = details;
   _methodSymbol = methodSymbol;
   _fe = fe;
   _symRefTab = symRefTab;
   _comp = TR::comp();
   }

TR::CFG *
OMR::IlInjector::cfg()
   {
   return _methodSymbol->getFlowGraph();
   }

TR::Block *
OMR::IlInjector::getCurrentBlock()
   {
   return _currentBlock;
   }

bool
OMR::IlInjector::genIL()
   {
   _comp->reportILGeneratorPhase();

   TR::StackMemoryRegion stackMemoryRegion(*_comp->trMemory());

   _comp->setCurrentIlGenerator(this);
   bool success = injectIL();
   _comp->setCurrentIlGenerator(0);

#if !defined(DISABLE_CFG_CHECK)
   if (success && _comp->getOption(TR_UseILValidator))
      {
      /* Setup the ILValidator for the current Compilation Thread. */
      _comp->setILValidator(createILValidatorObject(_comp));
      }
#endif

   return success;
   }


// following are common helpers to inject IL

void
OMR::IlInjector::generateToBlock(int32_t b)
   {
   _currentBlockNumber = b;
   _currentBlock = _blocks[b];
   }

void
OMR::IlInjector::allocateBlocks(int32_t num)
   {
   _numBlocks = num;
   _blocks = (TR::Block **) _comp->trMemory()->allocateHeapMemory(num * sizeof(TR::Block *));
   _blocksAllocatedUpFront = true;
   }

TR::Block *
OMR::IlInjector::newBlock()
   {
   return TR::Block::createEmptyBlock(comp());
   }

void
OMR::IlInjector::createBlocks(int32_t num)
   {
   allocateBlocks(num);
   for (int32_t b = 0;b < num;b ++)
      {
      _blocks[b] = newBlock();
      cfg()->addNode(_blocks[b]);
      }
   cfg()->addEdge(cfg()->getStart(), block(0));
   for (int32_t b = 0; b < num-1;b++)
      _blocks[b]->getExit()->join(_blocks[b+1]->getEntry());

   _methodSymbol->setFirstTreeTop(_blocks[0]->getEntry());

   generateToBlock(0);
   }

TR::TreeTop *
OMR::IlInjector::genTreeTop(TR::Node *n)
   {
   if (!n->getOpCode().isTreeTop())
      n = TR::Node::create(TR::treetop, 1, n);
   return _currentBlock->append(TR::TreeTop::create(comp(), n));
   }


TR::SymbolReference *
OMR::IlInjector::newTemp(TR::IlType *dt)
   {
   return symRefTab()->createTemporary(_methodSymbol, dt->getPrimitiveType());
   }

TR::Node *
OMR::IlInjector::parameter(int32_t slot, TR::IlType *dt)
   {
   return TR::Node::createLoad(symRefTab()->findOrCreateAutoSymbol(_methodSymbol, slot, dt->getPrimitiveType(), true, false, true));
   }

TR::Node *
OMR::IlInjector::iconst(int32_t value)
   {
   return TR::Node::iconst(value);
   }

TR::Node *
OMR::IlInjector::lconst(int64_t value)
   {
   return TR::Node::lconst(value);
   }

TR::Node *
OMR::IlInjector::bconst(int8_t value)
   {
   return TR::Node::bconst(value);
   }

TR::Node *
OMR::IlInjector::sconst(int16_t value)
   {
   return TR::Node::sconst(value);
   }

TR::Node *
OMR::IlInjector::aconst(uintptr_t value)
   {
   return TR::Node::aconst(value);
   }

TR::Node *
OMR::IlInjector::dconst(double value)
   {
   TR::Node *r = TR::Node::create(0, TR::dconst);
   r->setDouble(value);
   return r;
   }

TR::Node *
OMR::IlInjector::fconst(float value)
   {
   TR::Node *r = TR::Node::create(0, TR::fconst);
   r->setFloat(value);
   return r;
   }

TR::Node *
OMR::IlInjector::staticAddress(void *address)
   {
   return TR::Node::aconst((uintptr_t)address);
   }

void
OMR::IlInjector::storeToTemp(TR::SymbolReference *tempSymRef, TR::Node *value)
   {
   genTreeTop(TR::Node::createStore(tempSymRef, value));
   }


TR::Node *
OMR::IlInjector::loadTemp(TR::SymbolReference *tempSymRef)
   {
   return TR::Node::createLoad(tempSymRef);
   }

TR::Node *
OMR::IlInjector::i2l(TR::Node *n)
   {
   return TR::Node::create(TR::i2l, 1, n);
   }

TR::Node *
OMR::IlInjector::iu2l(TR::Node *n)
   {
   return TR::Node::create(TR::iu2l, 1, n);
   }

void
OMR::IlInjector::ifjump(TR::ILOpCodes op,
                        TR::Node *first,
                        TR::Node *second,
                        TR::Block *targetBlock)
   {
   TR::Node *ifNode = TR::Node::createif(op, first, second, targetBlock->getEntry());
   genTreeTop(ifNode);
   cfg()->addEdge(_currentBlock, targetBlock);
   if (_blocksAllocatedUpFront)
      {
      cfg()->addEdge(_currentBlock, _blocks[_currentBlockNumber+1]);
      generateToBlock(_currentBlockNumber + 1);
      }
   }

void
OMR::IlInjector::ifjump(TR::ILOpCodes op,
                        TR::Node *first,
                        TR::Node *second,
                        int32_t targetBlockNumber)
   {
   ifjump(op, first, second, _blocks[targetBlockNumber]);
   }

TR::Node *
OMR::IlInjector::shiftLeftBy(TR::Node *value, int32_t shift)
   {
   TR::Node *result;
   if (value->getDataType() == TR::Int32)
      result = TR::Node::create(TR::ishl, 2, value, TR::Node::iconst(shift));
   else
      {
      TR_ASSERT(value->getDataType() == TR::Int64, "expecting Int32 or Int64 for shiftLeftBy value expression");
      result = TR::Node::create(TR::lshl, 2, value, TR::Node::lconst(shift));
      }
   return result;
   }

TR::Node *
OMR::IlInjector::multiplyBy(TR::Node *value, int64_t factor)
   {
   TR::Node *result;
   if (value->getDataType() == TR::Int32)
      result = TR::Node::create(TR::imul, 2, value, TR::Node::iconst(factor));
   else
      {
      TR_ASSERT(value->getDataType() == TR::Int64, "expecting Int32 or Int64 for multiplyBy value expression");
      result = TR::Node::create(TR::lmul, 2, value, TR::Node::lconst(factor));
      }
   return result;
   }

TR::Node *
OMR::IlInjector::arrayLoad(TR::Node *base, TR::Node *index, TR::IlType *dt)
   {
   TR::Node *scaledIndex;
   TR::Node *element;
   if (index->getDataType() == TR::Int32)
      {
      scaledIndex = createWithoutSymRef(TR::imul, 2, index, iconst((int64_t) TR::DataType::getSize(dt->getPrimitiveType())));
      element = TR::Node::create(TR::aiadd, 2, base, scaledIndex);
      }
   else
      {
      TR_ASSERT(index->getDataType() == TR::Int64, "expecting Int32 or Int64 for arrayLoad index expression");
      scaledIndex = createWithoutSymRef(TR::lmul, 2, index, lconst((int64_t) TR::DataType::getSize(dt->getPrimitiveType())));
      element = TR::Node::create(TR::aladd, 2, base, scaledIndex);
      }
   TR::SymbolReference * symRef = symRefTab()->findOrCreateArrayShadowSymbolRef(dt->getPrimitiveType(), base);
   TR::Node *load = TR::Node::createWithSymRef(TR::ILOpCode::indirectLoadOpCode(dt->getPrimitiveType()), 1, element, 0, symRef);
   return load;
   }

void
OMR::IlInjector::returnValue(TR::Node *value)
   {
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(value->getDataType()), 1, value);
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   }

void
OMR::IlInjector::returnNoValue()
   {
   TR::Node *returnNode = TR::Node::create(TR::ILOpCode::returnOpCode(TR::NoType));
   genTreeTop(returnNode);
   cfg()->addEdge(_currentBlock, cfg()->getEnd());
   }

void
OMR::IlInjector::validateTargetBlock()
   {
   TR_ASSERT(_currentBlock != NULL, "Current Block is NULL! Probably need to call generateToBlock() beforehand");
   TR_ASSERT(_currentBlockNumber >= 0, "Current Block Number is invalid! Probably need to call generateToBlock() beforehand");
   }

void
OMR::IlInjector::gotoBlock(TR::Block *destBlock)
   {
   TR::Node *gotoNode = TR::Node::create(NULL, TR::Goto);
   gotoNode->setBranchDestination(destBlock->getEntry());
   genTreeTop(gotoNode);
   cfg()->addEdge(_currentBlock, destBlock);
   _currentBlock = NULL;
   _currentBlockNumber = -1;
   }

void
OMR::IlInjector::generateFallThrough()
   {
   cfg()->addEdge(_currentBlock, _blocks[_currentBlockNumber+1]);
   generateToBlock(_currentBlockNumber + 1);
   }

/*
 * Note:
 * This function should be removed and tests depending on this should be using
 * OMR::Node::createWithoutSymRef() instead once it is available publicly.
 * */
TR::Node *
OMR::IlInjector::createWithoutSymRef(TR::ILOpCodes opCode, uint32_t numArgs, ...)
   {
   TR_ASSERT(numArgs > 0, "Must be called with at least one child, but numChildArgs = %d", numArgs);
   va_list args;
   va_start(args, numArgs);
   TR::Node * result = TR::Node::create(opCode, numArgs);
   for (int i = 0; i < numArgs; ++i)
      {
      TR::Node * child = va_arg(args, TR::Node *);
      TR_ASSERT(child != NULL, "Child %d must be non NULL", i);
      result->setAndIncChild(i,child);
      }
   return result;
   }
