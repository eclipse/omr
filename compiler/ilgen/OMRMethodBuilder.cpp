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

#include <iostream>
#include <fstream>

#include <stdint.h>
#include "compile/Method.hpp"
#include "env/FrontEnd.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/CompileMethod.hpp"
#include "control/Recompilation.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/STLUtils.hpp"
#include "infra/List.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define OPT_DETAILS "O^O ILBLD: "

// Size of MethodBuilder memory segments
#define MEM_SEGMENT_SIZE 1 << 16   // i.e. 65536 bytes (~64KB)

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

#if defined (_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

// MethodBuilder is an IlBuilder object representing an entire method /
// function, so it conceptually has an entry point (though multiple entry
// method builders are entirely possible). Typically there is a single
// MethodBuilder created for each particular method compilation unit and
// multiple methods would require multiple MethodBuilder objects.
//
// A MethodBuilder is also an IlBuilder whose control flow path starts when
// the method is called. Returning from the method must be explicitly built
// into the builder.
//

// Note: _memoryRegion and the corresponding TR::SegmentProvider and TR::Memory instances are stored as pointers within MethodBuilder
// in order to avoid increasing the number of header files needed to compile against the JitBuilder library. Because we are storing
// them as pointers, we cannot rely on the default C++ destruction semantic to destruct and deallocate the memory region, but rather
// have to do it explicitly in the MethodBuilder::MemoryManager destructor. And since C++ destroys the other members *after* executing
// the user defined destructor, we need to make sure that any members (and their contents) that are allocated in _memoryRegion are
// explicitly destroyed and deallocated *before* _memoryRegion in the MethodBuilder::MemoryManager destructor.
OMR::MethodBuilder::MemoryManager::MemoryManager() :
   _segmentProvider(new(TR::Compiler->persistentAllocator()) TR::SystemSegmentProvider(MEM_SEGMENT_SIZE, TR::Compiler->rawAllocator)),
   _memoryRegion(new(TR::Compiler->persistentAllocator()) TR::Region(*_segmentProvider, TR::Compiler->rawAllocator)),
   _trMemory(new(TR::Compiler->persistentAllocator()) TR_Memory(*::trPersistentMemory, *_memoryRegion))
   {}

OMR::MethodBuilder::MemoryManager::~MemoryManager()
   {
   _trMemory->~TR_Memory();
   ::operator delete(_trMemory, TR::Compiler->persistentAllocator());
   _memoryRegion->~Region();
   ::operator delete(_memoryRegion, TR::Compiler->persistentAllocator());
   static_cast<TR::SystemSegmentProvider *>(_segmentProvider)->~SystemSegmentProvider();
   ::operator delete(_segmentProvider, TR::Compiler->persistentAllocator());
   }

OMR::MethodBuilder::MethodBuilder(TR::TypeDictionary *types, TR::VirtualMachineState *vmState)
   : TR::IlBuilder(asMethodBuilder(), types),
   _clientCallbackRequestFunction(0),
   _methodName("NoName"),
   _returnType(NoType),
   _numParameters(0),
   _symbols(str_comparator, trMemory()->heapMemoryRegion()),
   _parameterSlot(str_comparator, trMemory()->heapMemoryRegion()),
   _symbolTypes(str_comparator, trMemory()->heapMemoryRegion()),
   _symbolNameFromSlot(std::less<int32_t>(), trMemory()->heapMemoryRegion()),
   _symbolIsArray(str_comparator, trMemory()->heapMemoryRegion()),
   _memoryLocations(str_comparator, trMemory()->heapMemoryRegion()),
   _globals(str_comparator,trMemory()->heapMemoryRegion()),
   _functions(str_comparator, trMemory()->heapMemoryRegion()),
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
   _bytecodeHasBeenInWorklist(NULL),
   _inlineSiteIndex(-1),
   _nextInlineSiteIndex(0),
   _returnBuilder(NULL),
   _returnSymbolName(NULL)
   {
   _definingLine[0] = '\0';
   }

// used when inlining:
OMR::MethodBuilder::MethodBuilder(TR::MethodBuilder *callerMB, TR::VirtualMachineState *vmState)
   : TR::IlBuilder(asMethodBuilder(), callerMB->typeDictionary()),
   _methodName("NoName"),
   _returnType(NoType),
   _numParameters(0),
   _symbols(str_comparator, trMemory()->heapMemoryRegion()),
   _parameterSlot(str_comparator, trMemory()->heapMemoryRegion()),
   _symbolTypes(str_comparator, trMemory()->heapMemoryRegion()),
   _symbolNameFromSlot(std::less<int32_t>(), trMemory()->heapMemoryRegion()),
   _symbolIsArray(str_comparator, trMemory()->heapMemoryRegion()),
   _memoryLocations(str_comparator, trMemory()->heapMemoryRegion()),
   _globals(str_comparator,trMemory()->heapMemoryRegion()),
   _functions(str_comparator, trMemory()->heapMemoryRegion()),
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
   _bytecodeHasBeenInWorklist(NULL),
   _inlineSiteIndex(callerMB->getNextInlineSiteIndex()),
   _nextInlineSiteIndex(0),
   _returnBuilder(NULL),
   _returnSymbolName(NULL)
   {
   _definingLine[0] = '\0';
   initialize(callerMB->_details, callerMB->_methodSymbol, callerMB->_fe, callerMB->_symRefTab);
   }

OMR::MethodBuilder::~MethodBuilder()
   {
   // Cleanup allocations in _memoryRegion *before* its destroyed in
   // the MethodBuilder::MemoryManager destructor
   _symbols.clear();
   _parameterSlot.clear();
   _symbolTypes.clear();
   _symbolNameFromSlot.clear();
   _symbolIsArray.clear();
   _memoryLocations.clear();
   _functions.clear();
   }

TR::MethodBuilder *
OMR::MethodBuilder::asMethodBuilder()
   {
   return static_cast<TR::MethodBuilder *>(this);
   }

int32_t
OMR::MethodBuilder::getNextValueID()
   {
   TR::MethodBuilder *caller = callerMethodBuilder();
   if (caller)
      // let top most method assign value IDs
      return caller->getNextValueID();

   return _nextValueID++;
   }

int32_t
OMR::MethodBuilder::getNextInlineSiteIndex()
   {
   TR::MethodBuilder *caller = callerMethodBuilder();
   if (caller != NULL)
      // let top most method assign inlined site indices
      return caller->getNextInlineSiteIndex();

   return ++_nextInlineSiteIndex;
   }

TR::MethodBuilder *
OMR::MethodBuilder::callerMethodBuilder()
   {
   if (_returnBuilder == NULL)
      return NULL;
   return _returnBuilder->_methodBuilder;
   }

void
OMR::MethodBuilder::setupForBuildIL()
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

uint32_t
OMR::MethodBuilder::countBlocks()
   {
   if (_count > -1)
      return _count;

   TraceIL("[ %p ] TR::MethodBuilder::countBlocks 0 at entry\n", this);

   uint32_t numBlocks = this->TR::IlBuilder::countBlocks();

   _numBlocksBeforeWorklist = numBlocks;

   TraceIL("[ %p ] TR::MethodBuilder::countBlocks %d before worklist\n", this, numBlocks);

   // if not using bytecode builders, numBlocks is the real count
   if (!_useBytecodeBuilders)
      return numBlocks;

   TraceIL("[ %p ] TR::MethodBuilder::countBlocks iterating over worklist\n", this);
   // also need to visit any bytecode builders that have been added to the count worklist
   while (!_countBlocksWorklist->isEmpty())
      {

      while (!_countBlocksWorklist->isEmpty())
         {
         TR::BytecodeBuilder *builder = _countBlocksWorklist->popHead();
         TraceIL("[ %p ] TR::MethodBuilder::countBlocks visiting [ %p ]\n", this, builder);
         numBlocks += builder->countBlocks();
         TraceIL("[ %p ] numBlocks is %d\n", this, numBlocks);
         }

      ListIterator<TR::BytecodeBuilder> iter(_allBytecodeBuilders);
      for (TR::BytecodeBuilder *builder=iter.getFirst();
           !iter.atEnd();
           builder = iter.getNext())
         {
         // any BytecodeBuilders that have not yet been connected are actually unreachable
         // but unreachable code analysis will assert if their trees aren't in the method
         // we could iterate through the trees to remove this builder's blocks from the CFG
         // but it's probably easier to just connect the trees and let them be ripped out later
         // but here: need to count their blocks
         if (builder->_count < 0)
            {
            TraceIL("[ %p ] Adding unreachable BytecodeBuilder %p to counting worklist\n", this, builder);
            _countBlocksWorklist->add(builder);
            }
         }
      }

   _count = numBlocks;
   return numBlocks;
   }

bool
OMR::MethodBuilder::connectTrees()
   {
   TraceIL("[ %p ] TR::MethodBuilder::connectTrees entry\n", this);
   if (_useBytecodeBuilders)
      {
      // allocate worklists up front

      _countBlocksWorklist = new (comp()->trHeapMemory()) List<TR::BytecodeBuilder>(comp()->trMemory());
      _connectTreesWorklist = new (comp()->trHeapMemory()) List<TR::BytecodeBuilder>(comp()->trMemory());

      // this will go count everything up front
      _count = countBlocks();
      }

   TraceIL("[ %p ] TR::MethodBuilder::connectTrees total blocks %d\n", this, _count);

   bool rc = TR::IlBuilder::connectTrees();

   if (!_useBytecodeBuilders || !rc)
      return rc;

   // call to IlBuilder::connectTrees only filled in blocks for this method builder
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
         TR::BytecodeBuilder *builder = _connectTreesWorklist->popHead();
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

      ListIterator<TR::BytecodeBuilder> iter(_allBytecodeBuilders);
      for (TR::BytecodeBuilder *builder=iter.getFirst();
           !iter.atEnd();
           builder = iter.getNext())
         {
         // any BytecodeBuilders that have not yet been connected are actually unreachable
         // but unreachable code analysis will assert if their trees aren't in the method
         // we could iterate through the trees to remove this builder's blocks from the CFG
         // but it's probably easier to just connect the trees and let them be ripped out later
         if (!builder->_connectedTrees)
            {
            TraceIL("[ %p ] Adding unreachable BytecodeBuilder %p to connection worklist\n", this, builder);
            _connectTreesWorklist->add(builder);
            }
         }
      } while (!_connectTreesWorklist->isEmpty());

   return true;
   }

bool
OMR::MethodBuilder::symbolDefined(const char *name)
   {
   // _symbols not good enough because symbol can be defined even if it has
   // never been stored to, but _symbolTypes will contain all symbols, even
   // if they have never been used. See ::DefineLocal, for example, which can
   // be called in a MethodBuilder contructor. In contrast, ::DefineSymbol
   // which inserts into _symbols, can only be called from within a MethodBuilder's
   // ::buildIL() method ).
   return _symbolTypes.find(name) != _symbolTypes.end();
   }

void
OMR::MethodBuilder::defineSymbol(const char *name, TR::SymbolReference *symRef)
   {
   TR_ASSERT_FATAL(_symbols.find(name) == _symbols.end(), "Symbol '%s' already defined", name);

   _symbols.insert(std::make_pair(name, symRef));
   _symbolNameFromSlot.insert(std::make_pair(symRef->getCPIndex(), name));

   TR::IlType *type = typeDictionary()->PrimitiveType(symRef->getSymbol()->getDataType());
   _symbolTypes.insert(std::make_pair(name, type));

   if (!_newSymbolsAreTemps)
      _methodSymbol->setFirstJitTempIndex(_methodSymbol->getTempIndex());
   }

const char *
OMR::MethodBuilder::adjustNameForInlinedSite(const char *name)
   {
   if (_inlineSiteIndex == -1)
      return name;

   // prefix with _INL<index>_ and return
   char *newName = (char *) _comp->trMemory()->allocateHeapMemory((4+10+1+1+strlen(name)) * sizeof(char)); // 4 ("_INL") + max 10 digits + 1 ("_") + original name string + trailing zero
   sprintf(newName, "_INL%u_%s", _inlineSiteIndex, name);
   return (const char *)newName;
   }

TR::SymbolReference *
OMR::MethodBuilder::lookupSymbol(const char *name)
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

   TR::IlType *symbolType = symTypesIterator->second;
   TR::DataType primitiveType = symbolType->getPrimitiveType();

   ParameterMap::iterator paramSlotsIterator = _parameterSlot.find(name);
   // if this MethodBuilder is inlined, even parameters should just be temps
   if (_inlineSiteIndex == -1 && paramSlotsIterator != _parameterSlot.end())
      {
      int32_t slot = paramSlotsIterator->second;
      symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol,
                                                   slot,
                                                   primitiveType,
                                                   true, false, true);
      }
   else
      {
      GlobalMap::iterator globalsIterator = _globals.find(name);
      if(globalsIterator != _globals.end())
         {
         symRef = symRefTab()->createNamedStatic(_methodSymbol, primitiveType, name);
         symRef->getSymbol()->getStaticSymbol()->setStaticAddress((*globalsIterator).second);
         symRef->getSymbol()->setVolatile();
         }
      else
         {
         symRef = symRefTab()->createTemporary(_methodSymbol, primitiveType);
         const char *adjustedName = adjustNameForInlinedSite(name);
         symRef->getSymbol()->getAutoSymbol()->setName(adjustedName);
         }
      _symbolNameFromSlot.insert(std::make_pair(symRef->getCPIndex(), name));

      // also do the symbol name mapping in caller so references to parameters can be shown in logs
      TR::MethodBuilder *callerMB = callerMethodBuilder();
      if (callerMB)
         callerMB->_symbolNameFromSlot.insert(std::make_pair(symRef->getCPIndex(), name));
      }
   symRef->getSymbol()->setNotCollected();

   _symbols.insert(std::make_pair(name, symRef));

   return symRef;
   }

TR::ResolvedMethod *
OMR::MethodBuilder::lookupFunction(const char *name)
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
OMR::MethodBuilder::isSymbolAnArray(const char *name)
   {
   return _symbolIsArray.find(name) != _symbolIsArray.end();
   }

void
OMR::MethodBuilder::DefineLine(const char *line)
   {
   snprintf(_definingLine, MAX_LINE_NUM_LEN * sizeof(char), "%s", line);
   }

void
OMR::MethodBuilder::DefineLine(int line)
   {
   snprintf(_definingLine, MAX_LINE_NUM_LEN * sizeof(char), "%d", line);
   }

void
OMR::MethodBuilder::DefineName(const char *name)
   {
   _methodName = name;
   }

void
OMR::MethodBuilder::DefineLocal(const char *name, TR::IlType *dt)
   {
   TR_ASSERT_FATAL(_symbolTypes.find(name) == _symbolTypes.end(), "Symbol '%s' already defined", name);
   _symbolTypes.insert(std::make_pair(name, dt));
   }

void
OMR::MethodBuilder::DefineMemory(const char *name, TR::IlType *dt, void *location)
   {
   TR_ASSERT_FATAL(_memoryLocations.find(name) == _memoryLocations.end(), "Memory '%s' already defined", name);

   _symbolTypes.insert(std::make_pair(name, dt));
   _memoryLocations.insert(std::make_pair(name, location));
   }

void
OMR::MethodBuilder::DefineGlobal(const char *name, TR::IlType *dt, void *location)
   {
   TR_ASSERT_FATAL(_globals.find(name) == _globals.end(), "Global '%s' already defined", name);

   _globals.insert(std::make_pair(name, location));
   _symbolTypes.insert(std::make_pair(name, dt));
   }

void
OMR::MethodBuilder::DefineParameter(const char *name, TR::IlType *dt)
   {
   TR_ASSERT_FATAL(_parameterSlot.find(name) == _parameterSlot.end(), "Parameter '%s' already defined", name);

   _parameterSlot.insert(std::make_pair(name, _numParameters));
   _symbolNameFromSlot.insert(std::make_pair(_numParameters, name));
   _symbolTypes.insert(std::make_pair(name, dt));

   _numParameters++;
   }

void
OMR::MethodBuilder::DefineArrayParameter(const char *name, TR::IlType *elementType)
   {
   DefineParameter(name, elementType);

   _symbolIsArray.insert(name);
   }

void
OMR::MethodBuilder::DefineReturnType(TR::IlType *dt)
   {
   _returnType = dt;
   }

void
OMR::MethodBuilder::DefineFunction(const char* const name,
                              const char* const fileName,
                              const char* const lineNumber,
                              void           * entryPoint,
                              TR::IlType     * returnType,
                              int32_t          numParms,
                              ...)
   {
   TR::IlType **parmTypes = (TR::IlType **) trMemory()->trPersistentMemory()->allocatePersistentMemory(numParms * sizeof(TR::IlType *));
   va_list parms;
   va_start(parms, numParms);
   for (int32_t p=0;p < numParms;p++)
      parmTypes[p] = (TR::IlType *) va_arg(parms, TR::IlType *);
   va_end(parms);

   DefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, parmTypes);
   }

void
OMR::MethodBuilder::DefineFunction(const char* const name,
                              const char* const fileName,
                              const char* const lineNumber,
                              void           * entryPoint,
                              TR::IlType     * returnType,
                              int32_t          numParms,
                              TR::IlType     ** parmTypes)
   {
   TR_ASSERT_FATAL(_functions.find(name) == _functions.end(), "Function '%s' already defined", name);

   // copy parameter types so don't have to force caller to keep the parmTypes array alive
   TR::IlType **copiedParmTypes = (TR::IlType **) trMemory()->trPersistentMemory()->allocatePersistentMemory(numParms * sizeof(TR::IlType *));
   for (int32_t p=0;p < numParms;p++)
      copiedParmTypes[p] = parmTypes[p];

   TR::ResolvedMethod *method = new (trMemory()->heapMemoryRegion()) TR::ResolvedMethod(
                                                                        (char*)fileName,
                                                                        (char*)lineNumber,
                                                                        (char*)name,
                                                                        numParms,
                                                                        copiedParmTypes,
                                                                        returnType,
                                                                        entryPoint,
                                                                        0);

   _functions.insert(std::make_pair(name, method));
   }

const char *
OMR::MethodBuilder::getSymbolName(int32_t slot)
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

TR::IlType **
OMR::MethodBuilder::getParameterTypes()
   {
   if (_cachedParameterTypes)
      return _cachedParameterTypes;

   TR_ASSERT_FATAL(_numParameters < 18, "Too many parameters (%d) for parameter types array", _numParameters);
   TR::IlType **paramTypesArray = _cachedParameterTypesArray;
   for (int32_t p=0;p < _numParameters;p++)
      {
      SlotToSymNameMap::iterator symNamesIterator = _symbolNameFromSlot.find(p);
      TR_ASSERT_FATAL(symNamesIterator != _symbolNameFromSlot.end(), "No symbol found in slot %d", p);
      const char *name = symNamesIterator->second;

      auto symTypesIterator = _symbolTypes.find(name);
      TR_ASSERT_FATAL(symTypesIterator != _symbolTypes.end(), "No matching symbol type for parameter '%s'", name);
      paramTypesArray[p] = symTypesIterator->second;
      }

   _cachedParameterTypes = paramTypesArray;
   return paramTypesArray;
   }

void
OMR::MethodBuilder::addToBlockCountingWorklist(TR::BytecodeBuilder *builder)
   {
   TraceIL("[ %p ] TR::MethodBuilder::addToBlockCountingWorklist %p\n", this, builder);
   _countBlocksWorklist->add(builder);
   }

void
OMR::MethodBuilder::addToTreeConnectingWorklist(TR::BytecodeBuilder *builder)
   {
   if (!builder->_connectedTrees)
      {
      TraceIL("[ %p ] TR::MethodBuilder::addToTreeConnectingWorklist %p\n", this, builder);
      _connectTreesWorklist->add(builder);
      }
   }

void
OMR::MethodBuilder::addToAllBytecodeBuildersList(TR::BytecodeBuilder* bcBuilder)
   {
   if (NULL == _allBytecodeBuilders)
      {
      _allBytecodeBuilders = new (comp()->trHeapMemory()) List<TR::BytecodeBuilder>(comp()->trMemory());
      //if we're allocating this list, then this method builder uses bytecode builders
      setUseBytecodeBuilders();
      }
   _allBytecodeBuilders->add(bcBuilder);
   }

void
OMR::MethodBuilder::AppendBytecodeBuilder(TR::BytecodeBuilder *builder)
   {
   this->OMR::IlBuilder::AppendBuilder(builder);
   if (_vmState)
      builder->propagateVMState(_vmState);
   addBytecodeBuilderToWorklist(builder);
   }

void
OMR::MethodBuilder::addBytecodeBuilderToWorklist(TR::BytecodeBuilder *builder)
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
OMR::MethodBuilder::GetNextBytecodeFromWorklist()
   {
   if (_bytecodeWorklist == NULL || _bytecodeWorklist->isEmpty())
      return -1;

   TR_BitVectorIterator it(*_bytecodeWorklist);
   int32_t bci=it.getFirstElement();
   if (bci > -1)
      _bytecodeWorklist->reset(bci);
   return bci;
   }

int32_t
OMR::MethodBuilder::Compile(void **entry)
   {
   TR::ResolvedMethod resolvedMethod(static_cast<TR::MethodBuilder *>(this));
   TR::IlGeneratorMethodDetails details(&resolvedMethod);

   int32_t rc=0;
   *entry = (void *) compileMethodFromDetails(NULL, details, warm, rc);

   // let TypeDictionary know to clear out sym refs used in this compilation so
   // no dangling pointers
   typeDictionary()->NotifyCompilationDone();

   // in case this MethodBuilder object is used in another Call()
   // clear out symrefs allocated in this compilation (no dangling pointers)
   // and reset _connectedTrees so MethodBuilder can be inlined if needed
   _symbols.clear();
   _connectedTrees = false;

   return rc;
   }

void *
OMR::MethodBuilder::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::MethodBuilder *>(this));
   return _client;
   }

ClientAllocator OMR::MethodBuilder::_clientAllocator = NULL;
ClientAllocator OMR::MethodBuilder::_getImpl = NULL;
