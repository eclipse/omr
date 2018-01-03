/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#include <iostream>
#include <fstream>

#include <stdint.h>
#include "compile/Method.hpp"
#include "env/FrontEnd.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/STLUtils.hpp"
#include "infra/List.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilderImpl.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/MethodBuilderImpl.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/BytecodeBuilderImpl.hpp"
#include "ilgen/TypeDictionaryImpl.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define OPT_DETAILS "O^O ILBLD: "

// Size of MethodBuilderImpl memory segments
#define MEM_SEGMENT_SIZE 1 << 16   // i.e. 65536 bytes (~64KB)

// MethodBuilderImpl is an IlBuilderImpl object representing an entire method /
// function, so it conceptually has an entry point (though multiple entry
// method builders are entirely possible). Typically there is a single
// MethodBuilderImpl created for each particular method compilation unit and
// multiple methods would require multiple MethodBuilderImpl objects.
//
// A MethodBuilderImpl is also an IlBuilderImpl whose control flow path starts when
// the method is called. Returning from the method must be explicitly built
// into the builder.
//

OMR::MethodBuilderImpl::MethodBuilderImpl(TR::MethodBuilder *clientBuilder, TR::TypeDictionaryImpl *types, OMR::VirtualMachineState *vmState)
   : TR::IlBuilderImpl(static_cast<TR::MethodBuilderImpl *>(this), types),
   // Note: _memoryRegion and the corresponding TR::SegmentProvider and TR::Memory instances are stored as pointers within MethodBuilderImpl
   // in order to avoid increasing the number of header files needed to compile against the JitBuilder library. Because we are storing
   // them as pointers, we cannot rely on the default C++ destruction semantic to destruct and deallocate the memory region, but rather
   // have to do it explicitly in the MethodBuilderImpl destructor. And since C++ destroys the other members *after* executing the user defined
   // destructor, we need to make sure that any members (and their contents) that are allocated in _memoryRegion are explicitly destroyed
   // and deallocated *before* _memoryRegion in the MethodBuilderImpl destructor.
   _segmentProvider(new(TR::Compiler->persistentAllocator()) TR::SystemSegmentProvider(MEM_SEGMENT_SIZE, TR::Compiler->rawAllocator)),
   _memoryRegion(new(TR::Compiler->persistentAllocator()) TR::Region(*_segmentProvider, TR::Compiler->rawAllocator)),
   _trMemory(new(TR::Compiler->persistentAllocator()) TR_Memory(*::trPersistentMemory, *_memoryRegion)),
   _methodName("NoName"),
   _returnType(NoType),
   _numParameters(0),
   _symbols(str_comparator, *_memoryRegion),
   _parameterSlot(str_comparator, *_memoryRegion),
   _symbolTypes(str_comparator, *_memoryRegion),
   _symbolNameFromSlot(std::less<int32_t>(), *_memoryRegion),
   _symbolIsArray(str_comparator, *_memoryRegion),
   _memoryLocations(str_comparator, *_memoryRegion),
   _functions(str_comparator, *_memoryRegion),
   _cachedParameterTypes(0),
   _definingFile(""),
   _newSymbolsAreTemps(false),
   _nextValueID(0),
   _useBytecodeBuilders(false),
   _countBlocksWorklist(0),
   _connectTreesWorklist(0),
   _allBytecodeBuilders(0),
   _vmState(vmState),
   _bytecodeWorklist(NULL),
   _bytecodeHasBeenInWorklist(NULL)
   {
   _definingLine[0] = '\0';
   setClient(clientBuilder);
   }

OMR::MethodBuilderImpl::~MethodBuilderImpl()
   {
   // Cleanup allocations in _memoryRegion *before* its destroyed below (see note in constructor)
   _symbols.clear();
   _parameterSlot.clear();
   _symbolTypes.clear();
   _symbolNameFromSlot.clear();
   _symbolIsArray.clear();
   _memoryLocations.clear();
   _functions.clear();

   _trMemory->~TR_Memory();
   ::operator delete(_trMemory, TR::Compiler->persistentAllocator());
   _memoryRegion->~Region();
   ::operator delete(_memoryRegion, TR::Compiler->persistentAllocator());
   static_cast<TR::SystemSegmentProvider *>(_segmentProvider)->~SystemSegmentProvider();
   ::operator delete(_segmentProvider, TR::Compiler->persistentAllocator());
   }

TR::MethodBuilder *
OMR::MethodBuilderImpl::client()
   {
   return static_cast<TR::MethodBuilder *>(_clientBuilder);
   }

TR::MethodBuilderImpl *
OMR::MethodBuilderImpl::asMethodBuilder()
   {
   return static_cast<TR::MethodBuilderImpl *>(this);
   }

bool
OMR::MethodBuilderImpl::buildIL()
   {
   AllLocalsHaveBeenDefined();
   return client()->buildIL();
   }

void
OMR::MethodBuilderImpl::setupForBuildIL()
   {
   initSequence();

   _entryBlock = cfg()->getStart()->asBlock();
   _exitBlock = cfg()->getEnd()->asBlock();

   TraceIL("\tEntry = %p\n", _entryBlock);
   TraceIL("\tExit  = %p\n", _exitBlock);

   // initial "real" block 2 flowing from Entry
   appendBlock(NULL, false);

   // Method's first tree is from Entry block
   _methodSymbol->setFirstTreeTop(_currentBlock->getEntry());

   // set up initial CFG
   cfg()->addEdge(_entryBlock, _currentBlock);
   }

bool
OMR::MethodBuilderImpl::injectIL()
   {
   bool rc = IlBuilderImpl::injectIL();
   return rc;
   }


uint32_t
OMR::MethodBuilderImpl::countBlocks()
   {
   if (_count > -1)
      return _count;

   TraceIL("[ %p ] TR::MethodBuilderImpl::countBlocks 0 at entry\n", this);

   uint32_t numBlocks = this->TR::IlBuilderImpl::countBlocks();

   _numBlocksBeforeWorklist = numBlocks;

   TraceIL("[ %p ] TR::MethodBuilderImpl::countBlocks %d before worklist\n", this, numBlocks);

   // if not using bytecode builders, numBlocks is the real count
   if (!_useBytecodeBuilders)
      return numBlocks;

   TraceIL("[ %p ] TR::MethodBuilderImpl::countBlocks iterating over worklist\n", this);
   // also need to visit any bytecode builders that have been added to the count worklist
   while (!_countBlocksWorklist->isEmpty())
      {

      while (!_countBlocksWorklist->isEmpty())
         {
         TR::BytecodeBuilderImpl *builder = _countBlocksWorklist->popHead();
         TraceIL("[ %p ] TR::MethodBuilderImpl::countBlocks visiting [ %p ]\n", this, builder);
         numBlocks += builder->countBlocks();
         TraceIL("[ %p ] numBlocks is %d\n", this, numBlocks);
         }

      ListIterator<TR::BytecodeBuilderImpl> iter(_allBytecodeBuilders);
      for (TR::BytecodeBuilderImpl *builder=iter.getFirst();
           !iter.atEnd();
           builder = iter.getNext())
         {
         // any BytecodeBuilderImpls that have not yet been connected are actually unreachable
         // but unreachable code analysis will assert if their trees aren't in the method
         // we could iterate through the trees to remove this builder's blocks from the CFG
         // but it's probably easier to just connect the trees and let them be ripped out later
         // but here: need to count their blocks
         if (builder->_count < 0)
            {
            TraceIL("[ %p ] Adding unreachable BytecodeBuilderImpl %p to counting worklist\n", this, builder);
            _countBlocksWorklist->add(builder);
            }
         }
      }

   _count = numBlocks;
   return numBlocks;
   }

bool
OMR::MethodBuilderImpl::connectTrees()
   {
   TraceIL("[ %p ] TR::MethodBuilderImpl::connectTrees entry\n", this);
   if (_useBytecodeBuilders)
      {
      // allocate worklists up front

      _countBlocksWorklist = new (comp()->trHeapMemory()) List<TR::BytecodeBuilderImpl>(comp()->trMemory());
      _connectTreesWorklist = new (comp()->trHeapMemory()) List<TR::BytecodeBuilderImpl>(comp()->trMemory());

      // this will go count everything up front
      _count = countBlocks();
      }

   TraceIL("[ %p ] TR::MethodBuilderImpl::connectTrees total blocks %d\n", this, _count);

   bool rc = TR::IlBuilderImpl::connectTrees();

   if (!_useBytecodeBuilders || !rc)
      return rc;

   // call to IlBuilderImpl::connectTrees only filled in blocks for this method builder
   // and the first bytecode builder, but there could still be a worklist of
   // bytecode builders to process

   TR::Block **blocks = _blocks;

   uint32_t currentBlock = _numBlocksBeforeWorklist;

   TR::TreeTop *lastTree = blocks[currentBlock-1]->getExit();

   do
      {

      // iterate on the worklist pulling trees and blocks into this builder
      while (!_connectTreesWorklist->isEmpty())
         {
         TR::BytecodeBuilderImpl *builder = _connectTreesWorklist->popHead();
         if (!builder->_connectedTrees)
            {
            TraceIL("[ %p ] connectTrees visiting next builder from worklist [ %p ]\n", this, builder);

            TR::TreeTop *firstTree = NULL;
            TR::TreeTop *newLastTree = NULL;

            pullInBuilderTrees(builder, &currentBlock, &firstTree, &newLastTree);

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
         }

      ListIterator<TR::BytecodeBuilderImpl> iter(_allBytecodeBuilders);
      for (TR::BytecodeBuilderImpl *builder=iter.getFirst();
           !iter.atEnd();
           builder = iter.getNext())
         {
         // any BytecodeBuilderImpls that have not yet been connected are actually unreachable
         // but unreachable code analysis will assert if their trees aren't in the method
         // we could iterate through the trees to remove this builder's blocks from the CFG
         // but it's probably easier to just connect the trees and let them be ripped out later
         if (!builder->_connectedTrees)
            {
            TraceIL("[ %p ] Adding unreachable BytecodeBuilderImpl %p to connection worklist\n", this, builder);
            _connectTreesWorklist->add(builder);
            }
         }
      } while (!_connectTreesWorklist->isEmpty());

   return true;
   }

bool
OMR::MethodBuilderImpl::symbolDefined(const char *name)
   {
   // _symbols not good enough because symbol can be defined even if it has
   // never been stored to, but _symbolTypes will contain all symbols, even
   // if they have never been used. See ::DefineLocal, for example, which can
   // be called in a MethodBuilderImpl contructor. In contrast, ::DefineSymbol
   // which inserts into _symbols, can only be called from within a MethodBuilderImpl's
   // ::buildIL() method ).
   return _symbolTypes.find(name) != _symbolTypes.end();
   }

void
OMR::MethodBuilderImpl::defineSymbol(const char *name, TR::SymbolReference *symRef)
   {
   TR_ASSERT_FATAL(_symbols.find(name) == _symbols.end(), "Symbol '%s' already defined", name);

   _symbols.insert(std::make_pair(name, symRef));
   _symbolNameFromSlot.insert(std::make_pair(symRef->getCPIndex(), name));
   
   TR::IlTypeImpl *type = typeDictionary()->PrimitiveType(symRef->getSymbol()->getDataType());
   _symbolTypes.insert(std::make_pair(name, type));

   if (!_newSymbolsAreTemps)
      _methodSymbol->setFirstJitTempIndex(_methodSymbol->getTempIndex());
   }

TR::SymbolReference *
OMR::MethodBuilderImpl::lookupSymbol(const char *name)
   {
   TR::SymbolReference *symRef;

   SymbolMap::iterator symbolsIterator = _symbols.find(name);
   if (symbolsIterator != _symbols.end())  // Found
      {
      symRef = symbolsIterator->second;
      return symRef;
      }

   SymbolTypeMap::iterator symTypesIterator =  _symbolTypes.find(name);

   TR_ASSERT_FATAL(symTypesIterator != _symbolTypes.end(), "Symbol '%s' doesn't exist", name);

   TR::IlTypeImpl *symbolType = symTypesIterator->second;
   TR::DataType primitiveType = symbolType->getRealPrimitiveType();

   ParameterMap::iterator paramSlotsIterator = _parameterSlot.find(name);
   if (paramSlotsIterator != _parameterSlot.end())
      {
      int32_t slot = paramSlotsIterator->second;
      symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol,
                                                   slot,
                                                   primitiveType,
                                                   true, false, true);
      }
   else
      {
      symRef = symRefTab()->createTemporary(_methodSymbol, primitiveType);
      symRef->getSymbol()->getAutoSymbol()->setName(name);
      _symbolNameFromSlot.insert(std::make_pair(symRef->getCPIndex(), name));
      }
   symRef->getSymbol()->setNotCollected();

   _symbols.insert(std::make_pair(name, symRef));

   return symRef;
   }

TR::ResolvedMethod *
OMR::MethodBuilderImpl::lookupFunction(const char *name)
   {
   FunctionMap::iterator it = _functions.find(name);

   if (it == _functions.end())  // Not found
      {
      size_t len = strlen(name);
      if (len == strlen(_methodName) && strncmp(_methodName, name, len) == 0)
         return static_cast<TR::ResolvedMethod *>(_methodSymbol->getResolvedMethod());
      return NULL;
      }

   TR::ResolvedMethod *method = it->second;
   return method;
   }

bool
OMR::MethodBuilderImpl::isSymbolAnArray(const char *name)
   {
   return _symbolIsArray.find(name) != _symbolIsArray.end();
   }

TR::BytecodeBuilderImpl *
OMR::MethodBuilderImpl::OrphanBytecodeBuilderImpl(int32_t bcIndex, char *name)
   {
   TR::BytecodeBuilderImpl *orphan = new (comp()->trHeapMemory()) TR::BytecodeBuilderImpl(_methodBuilder, bcIndex, name);
   orphan->initialize(_details, _methodSymbol, _fe, _symRefTab);
   orphan->setupForBuildIL();
   return orphan;
   }

TR::BytecodeBuilderImpl *
OMR::MethodBuilderImpl::orphanBytecodeBuilderImpl(int32_t bcIndex, char *name)
   {
   return client()->OrphanBytecodeBuilder(bcIndex, name)->impl();
   }

void
OMR::MethodBuilderImpl::AppendBuilder(TR::BytecodeBuilderImpl *bb)
   {
   this->OMR::IlBuilderImpl::AppendBuilder(bb);
   if (_vmState)
      bb->propagateVMState(_vmState);
   addBytecodeBuilderToWorklist(bb);
   }

void
OMR::MethodBuilderImpl::DefineName(const char *name)
   {
   _methodName = name;
   }

void
OMR::MethodBuilderImpl::DefineLocal(const char *name, TR::IlTypeImpl *dt)
   {
   TR_ASSERT_FATAL(_symbolTypes.find(name) == _symbolTypes.end(), "Symbol '%s' already defined", name);
   _symbolTypes.insert(std::make_pair(name, dt));
   }

void
OMR::MethodBuilderImpl::DefineMemory(const char *name, TR::IlTypeImpl *dt, void *location)
   {
   TR_ASSERT_FATAL(_memoryLocations.find(name) == _memoryLocations.end(), "Memory '%s' already defined", name);

   _symbolTypes.insert(std::make_pair(name, dt));
   _memoryLocations.insert(std::make_pair(name, location));
   }

void
OMR::MethodBuilderImpl::DefineParameter(const char *name, TR::IlTypeImpl *dt)
   {
   TR_ASSERT_FATAL(_parameterSlot.find(name) == _parameterSlot.end(), "Parameter '%s' already defined", name);

   _parameterSlot.insert(std::make_pair(name, _numParameters));
   _symbolNameFromSlot.insert(std::make_pair(_numParameters, name));
   _symbolTypes.insert(std::make_pair(name, dt));

   _numParameters++;
   }

void
OMR::MethodBuilderImpl::DefineArrayParameter(const char *name, TR::IlTypeImpl *elementType)
   {
   DefineParameter(name, elementType);

   _symbolIsArray.insert(name);
   }

void
OMR::MethodBuilderImpl::DefineReturnType(TR::IlTypeImpl *dt)
   {
   _returnType = dt;
   }

void
OMR::MethodBuilderImpl::DefineFunction(const char * const    name,
                                       const char * const    fileName,
                                       const char * const    lineNumber,
                                       void                * entryPoint,
                                       TR::IlTypeImpl      * returnType,
                                       int32_t               numParms,
                                       ...)
   {
   TR::IlTypeImpl **parmTypes = (TR::IlTypeImpl **) TR::comp()->trMemory()->allocateHeapMemory(numParms * sizeof(TR::IlTypeImpl *));
   va_list parms;
   va_start(parms, numParms);
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypes[p] = (TR::IlTypeImpl *) va_arg(parms, TR::IlTypeImpl *);
      }
   va_end(parms);

   DefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, parmTypes);
   }

void
OMR::MethodBuilderImpl::DefineFunction(const char * const    name,
                                       const char * const    fileName,
                                       const char * const    lineNumber,
                                       void                * entryPoint,
                                       TR::IlTypeImpl      * returnType,
                                       int32_t               numParms,
                                       TR::IlTypeImpl     ** parmTypes)
   {   
   TR_ASSERT_FATAL(_functions.find(name) == _functions.end(), "Function '%s' already defined", name);
   TR::ResolvedMethod *method = new (*_memoryRegion) TR::ResolvedMethod((char*)fileName,
                                                                        (char*)lineNumber,
                                                                        (char*)name,
                                                                        numParms,
                                                                        parmTypes,
                                                                        returnType,
                                                                        entryPoint,
                                                                        0);

   _functions.insert(std::make_pair(name, method));
   }

const char *
OMR::MethodBuilderImpl::getSymbolName(int32_t slot)
   {
   // Sometimes the code generators will manufacture a symbol reference themselves with no way
   // to properly assign a cpIndex, that these symbol references show up here with slot == -1
   // when JIT logging. One specific case is when the code generate converts an indirect store to
   // a known stack allocated object using a constant offset to a store with a different offset
   // based of the stack pointer (because it knows exactly which stack slot is being referenced),
   // but there could be other cases. This escape clause doesn't feel like a great solution to this
   // problem, but since the assertions only really catch while create JIT logs (names are only
   // needed when generating logs), it's actually more useful to allow the compilation to continue
   // so that the full log can be generated rather than aborting.
   if (slot == -1)
      return "Unknown";

   SlotToSymNameMap::iterator it = _symbolNameFromSlot.find(slot);
   TR_ASSERT_FATAL(it != _symbolNameFromSlot.end(), "No symbol found in slot %d", slot);

   const char *symbolName = it->second;
   return symbolName;
   }

TR::IlTypeImpl **
OMR::MethodBuilderImpl::getParameterTypes()
   {
   if (_cachedParameterTypes)
      return _cachedParameterTypes;

   TR_ASSERT(_numParameters < 10, "too many parameters for parameter types array");
   TR::IlTypeImpl **paramTypesArray = _cachedParameterTypesArray;
   for (int32_t p=0;p < _numParameters;p++)
      {
      SlotToSymNameMap::iterator symNamesIterator = _symbolNameFromSlot.find(p);
      TR_ASSERT_FATAL(symNamesIterator != _symbolNameFromSlot.end(), "No symbol found in slot %d", p);
      const char *name = symNamesIterator->second;

      std::map<const char *, TR::IlTypeImpl *, StrComparator>::iterator symTypesIterator = _symbolTypes.find(name);
      TR_ASSERT_FATAL(symTypesIterator != _symbolTypes.end(), "No matching symbol type for parameter '%s'", name);
      paramTypesArray[p] = symTypesIterator->second;
      }

   _cachedParameterTypes = paramTypesArray;
   return paramTypesArray;
   }

void
OMR::MethodBuilderImpl::addToBlockCountingWorklist(TR::BytecodeBuilderImpl *builder)
   {
   TraceIL("[ %p ] TR::MethodBuilderImpl::addToBlockCountingWorklist %p\n", this, builder);
   _countBlocksWorklist->add(builder);
   }

void
OMR::MethodBuilderImpl::addToTreeConnectingWorklist(TR::BytecodeBuilderImpl *builder)
   {
   if (!builder->_connectedTrees)
      {
      TraceIL("[ %p ] TR::MethodBuilderImpl::addToTreeConnectingWorklist %p\n", this, builder);
      _connectTreesWorklist->add(builder);
      }
   }

void
OMR::MethodBuilderImpl::addToAllBytecodeBuildersList(TR::BytecodeBuilderImpl* bcBuilder)
   {
   if (NULL == _allBytecodeBuilders)
      {
      _allBytecodeBuilders = new (comp()->trHeapMemory()) List<TR::BytecodeBuilderImpl>(comp()->trMemory());
      //if we're allocating this list, then this method builder uses bytecode builders
      setUseBytecodeBuilders();
      }
   _allBytecodeBuilders->add(bcBuilder);
   }

void
OMR::MethodBuilderImpl::AppendBytecodeBuilder(TR::BytecodeBuilderImpl *builder)
   {
   this->IlBuilderImpl::AppendBuilder(builder);
   }

void
OMR::MethodBuilderImpl::addBytecodeBuilderToWorklist(TR::BytecodeBuilderImpl *builder)
   {
   if (_bytecodeWorklist == NULL)
      {
      _bytecodeWorklist = new (comp()->trHeapMemory()) TR_BitVector(32, comp()->trMemory());
      _bytecodeHasBeenInWorklist = new (comp()->trHeapMemory()) TR_BitVector(32, comp()->trMemory());
      }

   int32_t b_bci = builder->bcIndex();
   if (!_bytecodeHasBeenInWorklist->get(b_bci))
      {
      _bytecodeWorklist->set(b_bci);
      _bytecodeHasBeenInWorklist->set(b_bci);
      }
   }

int32_t
OMR::MethodBuilderImpl::GetNextBytecodeFromWorklist()
   {
   if (_bytecodeWorklist == NULL || _bytecodeWorklist->isEmpty())
      return -1;

   TR_BitVectorIterator it(*_bytecodeWorklist);
   int32_t bci=it.getFirstElement();
   if (bci > -1)
      _bytecodeWorklist->reset(bci);
   return bci;
   }

TR::MethodBuilderImpl *
OMR::MethodBuilderImpl::allocate(TR::MethodBuilder *client, TR::TypeDictionaryImpl *types, OMR::VirtualMachineState *vmState)
   {
   //LEAK: unfortunately must allocate in persistent memory because compilation hasn't begun when MethodBuilder object is created
   return new (PERSISTENT_NEW) TR::MethodBuilderImpl(client, types, vmState);
   }

bool
OMR::MethodBuilderImpl::RequestFunction(const char *name)
   {
   return client()->RequestFunction(name);
   }
