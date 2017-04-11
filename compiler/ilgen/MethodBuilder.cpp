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
 *******************************************************************************/

#include <iostream>
#include <fstream>

#include <stdint.h>
#include "compile/Method.hpp"
#include "env/FrontEnd.hpp"
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
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/MethodBuilder.hpp"
#include "ilgen/BytecodeBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "ilgen/VirtualMachineState.hpp"

#define OPT_DETAILS "O^O ILBLD: "

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

// replay always on for now
//#define REPLAY(x)            { x; }
#define REPLAY(x)            { }
#define MB_REPLAY(...)	     { REPLAY({sprintf(_rpLine, ##__VA_ARGS__); (*_rpCpp) << "\t" << _rpLine << std::endl;}) }
#define MB_REPLAY_NONL(...)       { REPLAY({sprintf(_rpLine, ##__VA_ARGS__); (*_rpCpp) << "\t" << _rpLine;}) }
#define REPLAY_TYPE(t)    ((t)->getName())

#define REPLAY_USE_NAMES_FOR_POINTERS
#if defined(REPLAY_USE_NAMES_FOR_POINTERS)
#define	REPLAY_POINTER_FMT   "&%s"
#define REPLAY_POINTER(p,n)  n
#else
#define	REPLAY_POINTER_FMT   "%p"
#define REPLAY_POINTER(p,n)  p
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

namespace OMR
{

MethodBuilder::MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState)
   : TR::IlBuilder(asMethodBuilder(), types),
   _methodName("NoName"),
   _returnType(NoType),
   _numParameters(0),
   _cachedParameterTypes(0),
   _cachedSignature(0),
   _definingFile(""),
   _definingLine(""),
   _symbols(0),
   _newSymbolsAreTemps(false),
   _useBytecodeBuilders(false),
   _countBlocksWorklist(0),
   _connectTreesWorklist(0),
   _allBytecodeBuilders(0),
   _vmState(vmState),
   _bytecodeWorklist(NULL),
   _bytecodeHasBeenInWorklist(NULL)
   {
   REPLAY({
      std::fstream rpHpp("ReplayMethod.hpp",std::fstream::out);
      rpHpp << "#include \"ilgen/MethodBuilder.hpp\"" << std::endl;
      rpHpp << "class ReplayMethod : public TR::MethodBuilder {" << std::endl;
      rpHpp << "\tReplayMethod(TR::TypeDictionary *types);" << std::endl;
      rpHpp << "\tvirtual bool buildIL();" << std::endl;
      rpHpp << "};" << std::endl;
      rpHpp.close();

      _rpCpp = new std::fstream("ReplayMethodConstructor.cpp",std::fstream::out);
      (*_rpCpp) << "#include \"ilgen/TypeDictionary.hpp\"" << std::endl << std::endl;
      (*_rpCpp) << "#include \"ReplayMethod.hpp\"" << std::endl << std::endl;
      (*_rpCpp) << "ReplayMethod::ReplayMethod(TR::TypeDictionary *types)" << std::endl;
      (*_rpCpp) << "\t: TR::MethodBuilder(types) {" << std::endl;
      // } to match open one in string in prev line so editors can match properly

      _rpILCpp = new std::fstream("ReplayMethodBuildIL.cpp",std::fstream::out);
      (*_rpILCpp) << "#include \"ilgen/TypeDictionary.hpp\"" << std::endl << std::endl;
      (*_rpILCpp) << "#include \"ReplayMethod.hpp\"" << std::endl << std::endl;
      (*_rpILCpp) << "bool ReplayMethod::buildIL() {" << std::endl;
      // } to match open one in string in prev line so editors can match properly

      strcpy(_replayName, "this");
      _haveReplayName = true;
   })

   initMaps();
   }

TR::MethodBuilder *
MethodBuilder::asMethodBuilder()
   {
   return static_cast<TR::MethodBuilder *>(this);
   }

void
MethodBuilder::initMaps()
   {
   _parameterSlot = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   _symbolTypes = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   _symbolNameFromSlot = new (PERSISTENT_NEW) TR_HashTabInt(typeDictionary()->trMemory());
   _symbolIsArray = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   _memoryLocations = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   _functions = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   _symbols = new (PERSISTENT_NEW) TR_HashTabString(typeDictionary()->trMemory());
   }

void
MethodBuilder::setupForBuildIL()
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
MethodBuilder::injectIL()
   {
   bool rc = IlBuilder::injectIL();
   REPLAY({
      (*_rpCpp) << "}" << std::endl;
      _rpCpp->close();

      (*_rpILCpp) << "}" << std::endl;
      _rpILCpp->close();
   })
   return rc;
   }


uint32_t
MethodBuilder::countBlocks()
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
MethodBuilder::connectTrees()
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
MethodBuilder::symbolDefined(const char *name)
   {
   TR_HashId id=0, typeID=0;
   // _symbols not good enough because symbol can be defined even if it has
   // never been stored to, but _symbolTypes will contain all symbols, even
   // if they have never been used. See ::DefineLocal, for example, which can
   // be called in a MethodBuilder contructor. In contrast, ::DefineSymbol
   // which inserts into _symbols, can only be called from within a MethodBuilder's
   // ::buildIL() method ).
   return (_symbolTypes->locate(name, typeID));
   }

void
MethodBuilder::defineSymbol(const char *name, TR::IlValue *v)
   {
   TR_HashId id1=0, id2=0, id3;

   _symbols->add(name, id1, (void *)v);
   _symbolNameFromSlot->add(v->getCPIndex(), id2, (void *)name);
   _symbolTypes->add(name, id3, (void *)(uintptr_t) v->getSymbol()->getDataType());
   if (!_newSymbolsAreTemps)
      _methodSymbol->setFirstJitTempIndex(_methodSymbol->getTempIndex());
   }

TR::IlValue *
MethodBuilder::lookupSymbol(const char *name)
   {
   TR_HashId symbolsID=0;
   bool present = _symbols->locate(name, symbolsID);
   if (present)
      return (TR::IlValue *)_symbols->getData(symbolsID);

   TR::SymbolReference *symRef;
   TR_HashId typesID;
   _symbolTypes->locate(name, typesID);

   TR::DataType type = ((TR::IlType *)(_symbolTypes->getData(typesID)))->getPrimitiveType();

   TR_HashId slotID;
   if (_parameterSlot->locate(name, slotID))
      {
      symRef = symRefTab()->findOrCreateAutoSymbol(_methodSymbol,
                                                   (int32_t)(uintptr_t)_parameterSlot->getData(slotID),
                                                   type,
                                                   true, false, true);
      }
   else
      {
      symRef = symRefTab()->createTemporary(_methodSymbol, type);
      symRef->getSymbol()->getAutoSymbol()->setName(name);
      TR_HashId nameFromSlotID;
      _symbolNameFromSlot->add(symRef->getCPIndex(), nameFromSlotID, (void *)name);
      }
   symRef->getSymbol()->setNotCollected();
   _symbols->add(name, symbolsID, (void *)symRef);
   return symRef;
   }

TR::ResolvedMethod *
MethodBuilder::lookupFunction(const char *name)
   {
   TR_HashId functionsID;
   if (! _functions->locate(name, functionsID))
      {
      size_t len = strlen(name);
      if (len == strlen(_methodName) && strncmp(_methodName, name, len) == 0)
         return static_cast<TR::ResolvedMethod *>(_methodSymbol->getResolvedMethod());
      return NULL;
      }

   return (TR::ResolvedMethod *)(_functions->getData(functionsID));
   }

bool
MethodBuilder::isSymbolAnArray(const char *name)
   {
   TR_HashId isArrayID;
   return _symbolIsArray->locate(name, isArrayID);
   }

TR::BytecodeBuilder *
MethodBuilder::OrphanBytecodeBuilder(int32_t bcIndex, char *name)
   {
   MB_REPLAY("OrphanBytecodeBuilder(%d, \"%s\");", bcIndex, name);

   TR::BytecodeBuilder *orphan = new (comp()->trHeapMemory()) TR::BytecodeBuilder(_methodBuilder, bcIndex, name);
   orphan->initialize(_details, _methodSymbol, _fe, _symRefTab);
   orphan->setupForBuildIL();
   return orphan;
   }

void
MethodBuilder::AppendBuilder(TR::BytecodeBuilder *bb)
   {
   this->OMR::IlBuilder::AppendBuilder(bb);
   if (_vmState)
      bb->propagateVMState(_vmState);
   addBytecodeBuilderToWorklist(bb);
   }

void
MethodBuilder::DefineName(const char *name)
   {
   MB_REPLAY("DefineName(\"%s\");", name);
   _methodName = name;
   }

void
MethodBuilder::DefineLocal(const char *name, TR::IlType *dt)
   {
   MB_REPLAY("DefineLocal(\"%s\", %s);", name, REPLAY_TYPE(dt));
   TR_HashId typesID;
   _symbolTypes->add(name, typesID, (void *)dt);
   }

void
MethodBuilder::DefineMemory(const char *name, TR::IlType *dt, void *location)
   {
   MB_REPLAY("DefineMemory(\"%s\", %s, " REPLAY_POINTER_FMT ");", name, REPLAY_TYPE(dt), REPLAY_POINTER(location, name));
   TR_HashId typesID;
   _symbolTypes->add(name, typesID, (void *) dt);

   TR_HashId locationsID;
   _memoryLocations->add(name, locationsID, location);
   }

void
MethodBuilder::DefineParameter(const char *name, TR::IlType *dt)
   {
   MB_REPLAY("DefineParameter(\"%s\", %s);", name, REPLAY_TYPE(dt));
   TR_HashId slotID;
   _parameterSlot->add(name, slotID, (void *)(uintptr_t) _numParameters);

   TR_HashId nameFromSlotID;
   _symbolNameFromSlot->add(_numParameters, nameFromSlotID, (void *) name);

   TR_HashId typesID;
   _symbolTypes->add(name, typesID, (void *) dt);

   _numParameters++;
   }

void
MethodBuilder::DefineArrayParameter(const char *name, TR::IlType *elementType)
   {
   MB_REPLAY("DefineArrayParameter(\"%s\", %s);", name, REPLAY_TYPE(elementType));
   DefineParameter(name, elementType);

   TR_HashId isArrayID;
   // doesn't actually matter what we put there; its presence says isArray
   _symbolIsArray->add(name, isArrayID, (void *)(uintptr_t) 1);
   }

void
MethodBuilder::DefineReturnType(TR::IlType *dt)
   {
   MB_REPLAY("DefineReturnType(%s);", REPLAY_TYPE(dt));
   _returnType = dt;
   }

void
MethodBuilder::DefineFunction(const char* const name,
                              const char* const fileName,
                              const char* const lineNumber,
                              void           * entryPoint,
                              TR::IlType     * returnType,
                              int32_t          numParms,
                              ...)
   {
   TR::IlType **parmTypes = (TR::IlType **) malloc(numParms * sizeof(TR::IlType *));
   va_list parms;
   va_start(parms, numParms);
   for (int32_t p=0;p < numParms;p++)
      {
      parmTypes[p] = (TR::IlType *) va_arg(parms, TR::IlType *);
      }
   va_end(parms);

   DefineFunction(name, fileName, lineNumber, entryPoint, returnType, numParms, parmTypes);
   }

void
MethodBuilder::DefineFunction(const char* const name,
                              const char* const fileName,
                              const char* const lineNumber,
                              void           * entryPoint,
                              TR::IlType     * returnType,
                              int32_t          numParms,
                              TR::IlType     ** parmTypes)
   {   
   MB_REPLAY("DefineFunction((const char* const)\"%s\",", name);
   MB_REPLAY("               (const char* const)\"%s\",", fileName);
   MB_REPLAY("               (const char* const)\"%s\",", lineNumber);
   MB_REPLAY("               " REPLAY_POINTER_FMT ",", REPLAY_POINTER(entryPoint, name));
   MB_REPLAY("               %s,", REPLAY_TYPE(returnType));
   MB_REPLAY_NONL("               %d", numParms);

   for (int32_t p=0;p < numParms;p++)
      {   
      MB_REPLAY_NONL(",\n               %s", REPLAY_TYPE(parmTypes[p]));
      }   
   MB_REPLAY(");");

   TR::ResolvedMethod *method = new (PERSISTENT_NEW) TR::ResolvedMethod((char*)fileName,
                                                                        (char*)lineNumber,
                                                                        (char*)name,
                                                                        numParms,
                                                                        parmTypes,
                                                                        returnType,
                                                                        entryPoint,
                                                                        0);

   TR_HashId functionsID;
   _functions->add(name, functionsID, (void *)method);
   }

const char *
MethodBuilder::getSymbolName(int32_t slot)
   {
   TR_HashId nameFromSlotID;
   if (_symbolNameFromSlot->locate(slot, nameFromSlotID))
      return (const char *)_symbolNameFromSlot->getData(nameFromSlotID);
   return NULL;
   }

TR::IlType **
MethodBuilder::getParameterTypes()
   {
   if (_cachedParameterTypes)
      return _cachedParameterTypes;

   TR_ASSERT(_numParameters < 10, "too many parameters for parameter types array");
   TR::IlType **paramTypesArray = _cachedParameterTypesArray;
   for (int32_t p=0;p < _numParameters;p++)
      {
      TR_HashId nameFromSlotID;
      _symbolNameFromSlot->locate(p, nameFromSlotID);
      const char *name = (const char *) _symbolNameFromSlot->getData(nameFromSlotID);

      TR_HashId typesID;
      _symbolTypes->locate(name, typesID);
      paramTypesArray[p] = (TR::IlType *) _symbolTypes->getData(typesID);
      }

   _cachedParameterTypes = paramTypesArray;
   return paramTypesArray;
   }

void
MethodBuilder::addToBlockCountingWorklist(TR::BytecodeBuilder *builder)
   {
   TraceIL("[ %p ] TR::MethodBuilder::addToBlockCountingWorklist %p\n", this, builder);
   _countBlocksWorklist->add(builder);
   }

void
MethodBuilder::addToTreeConnectingWorklist(TR::BytecodeBuilder *builder)
   {
   if (!builder->_connectedTrees)
      {
      TraceIL("[ %p ] TR::MethodBuilder::addToTreeConnectingWorklist %p\n", this, builder);
      _connectTreesWorklist->add(builder);
      }
   }

void
MethodBuilder::addToAllBytecodeBuildersList(TR::BytecodeBuilder* bcBuilder)
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
MethodBuilder::AppendBytecodeBuilder(TR::BytecodeBuilder *builder)
   {
   IlBuilder::AppendBuilder(builder);
   
   }

void
MethodBuilder::addBytecodeBuilderToWorklist(TR::BytecodeBuilder *builder)
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
MethodBuilder::GetNextBytecodeFromWorklist()
   {
   if (_bytecodeWorklist == NULL || _bytecodeWorklist->isEmpty())
      return -1;

   TR_BitVectorIterator it(*_bytecodeWorklist);
   int32_t bci=it.getFirstElement();
   if (bci > -1)
      _bytecodeWorklist->reset(bci);
   return bci;
   }

} // namespace OMR

