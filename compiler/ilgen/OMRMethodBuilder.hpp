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

#ifndef OMR_METHODBUILDER_INCL
#define OMR_METHODBUILDER_INCL

#include <map>
#include <set>
#include <fstream>
#include "env/TRMemory.hpp"
#include "ilgen/IlBuilder.hpp"
#include "env/TypedAllocator.hpp"

// Maximum length of _definingLine string (including null terminator)
#define MAX_LINE_NUM_LEN 7

class TR_BitVector;
namespace TR { class BytecodeBuilder; }
namespace TR { class ResolvedMethod; }
namespace TR { class SymbolReference; }
namespace TR { class VirtualMachineState; }

namespace TR { class SegmentProvider; }
namespace TR { class Region; }

extern "C"
{
typedef bool (*RequestFunctionCallback)(void *client, const char *name);
}

namespace OMR
{

class MethodBuilder : public TR::IlBuilder
   {
   public:
   TR_ALLOC(TR_Memory::IlGenerator)

   MethodBuilder(TR::TypeDictionary *types, TR::VirtualMachineState *vmState = NULL);
   MethodBuilder(TR::MethodBuilder *callerMB, TR::VirtualMachineState *vmState = NULL);
   virtual ~MethodBuilder();

   virtual void setupForBuildIL();

   /**
    * @brief returns the next index to be used for new values
    * @returns the next value index
    * If this method build is an inlined MethodBuilder, then the answer to
    * this query is delegated to the caller's MethodBuilder, which means
    * only the top-level MethodBuilder object assigns value IDs.
    */
   int32_t getNextValueID();

   bool usesBytecodeBuilders()                               { return _useBytecodeBuilders; }
   void setUseBytecodeBuilders()                             { _useBytecodeBuilders = true; }

   void addToAllBytecodeBuildersList(TR::BytecodeBuilder *bcBuilder);
   void addToTreeConnectingWorklist(TR::BytecodeBuilder *builder);
   void addToBlockCountingWorklist(TR::BytecodeBuilder *builder);

   virtual TR::VirtualMachineState *vmState()                { return _vmState; }
   virtual void setVMState(TR::VirtualMachineState *vmState) { _vmState = vmState; }

   virtual bool isMethodBuilder()                            { return true; }
   virtual TR::MethodBuilder *asMethodBuilder();

   TR::TypeDictionary *typeDictionary()                      { return _types; }

   const char *getDefiningFile()                             { return _definingFile; }
   const char *getDefiningLine()                             { return _definingLine; }

   const char *GetMethodName()                               { return _methodName; }
   void AllLocalsHaveBeenDefined()                           { _newSymbolsAreTemps = true; }

   TR::IlType *getReturnType()                               { return _returnType; }
   int32_t getNumParameters()                                { return _numParameters; }
   const char *getSymbolName(int32_t slot);

   TR::IlType **getParameterTypes();
   char *getSignature(int32_t numParams, TR::IlType **paramTypeArray);
   char *getSignature(TR::IlType **paramTypeArray)
      {
      return getSignature(_numParameters, paramTypeArray);
      }

   TR::SymbolReference *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::SymbolReference *v);
   bool symbolDefined(const char *name);
   bool isSymbolAnArray(const char * name);

   TR::ResolvedMethod *lookupFunction(const char *name);

   void AppendBuilder(TR::BytecodeBuilder *bb) { AppendBytecodeBuilder(bb); }
   void AppendBuilder(TR::IlBuilder *b)    { this->OMR::IlBuilder::AppendBuilder(b); }

   void DefineFile(const char *file)                         { _definingFile = file; }

   void DefineLine(const char *line);
   void DefineLine(int line);
   void DefineName(const char *name);
   void DefineParameter(const char *name, TR::IlType *type);
   void DefineArrayParameter(const char *name, TR::IlType *dt);
   void DefineReturnType(TR::IlType *dt);
   void DefineLocal(const char *name, TR::IlType *dt);
   void DefineMemory(const char *name, TR::IlType *dt, void *location);

   /**
    * @brief Define a global symbol
    * @param name the name by which the global symbol will be referred to
    * @param dt the data type of the global symbol
    * @param location the address of the value the global symbol refers to
    */
   void DefineGlobal(const char *name, TR::IlType *dt, void *location);
   void DefineFunction(const char* const name,
                       const char* const fileName,
                       const char* const lineNumber,
                       void           * entryPoint,
                       TR::IlType     * returnType,
                       int32_t          numParms,
                       ...);
   void DefineFunction(const char* const name,
                       const char* const fileName,
                       const char* const lineNumber,
                       void           * entryPoint,
                       TR::IlType     * returnType,
                       int32_t          numParms,
                       TR::IlType     ** parmTypes);

   int32_t Compile(void **entry);

   /**
    * @brief will be called if a Call is issued to a function that has not yet been defined, provides a
    *        mechanism for MethodBuilder subclasses to provide method lookup on demand rather than all up
    *        front via the constructor.
    * @returns true if the function was found and DefineFunction has been called for it, otherwise false
    */
   virtual bool RequestFunction(const char *name)
      {
      if (_clientCallbackRequestFunction)
         return _clientCallbackRequestFunction(_client, name);

      return false;
      }

   /**
    * @brief append the first bytecode builder object to this method
    * @param builder the bytecode builder object to add, usually for bytecode index 0
    * A side effect of this call is that the builder is added to the worklist so that
    * all other bytecodes can be processed by asking for GetNextBytecodeFromWorklist()
    */
   void AppendBytecodeBuilder(TR::BytecodeBuilder *builder);

   /**
    * @brief add a bytecode builder to the worklist
    * @param bcBuilder is the bytecode builder whose bytecode index will be added to the worklist
    */
   void addBytecodeBuilderToWorklist(TR::BytecodeBuilder* bcBuilder);

   /**
    * @brief get lowest index bytecode from the worklist
    * @returns lowest bytecode index or -1 if worklist is empty
    * It is important to use the worklist because it guarantees no bytecode will be
    * processed before at least one predecessor bytecode has been processed, which
    * means there should be a non-NULL VirtualMachineState object on the associated
    * BytecodeBuilder object.
    */
   int32_t GetNextBytecodeFromWorklist();

   /**
    * @brief Override this MethodBuilder's inline site index
    * @param siteIndex the inline site index to use for this MethodBuilder
    */
   void setInlineSiteIndex(int32_t siteIndex)
      {
      _inlineSiteIndex = siteIndex;
      }

   /**
    * @brief returns this MethodBuilder's inline site index
    * @returns the inlined site index
    */
   int32_t inlineSiteIndex()
      {
      return _inlineSiteIndex;
      }

   /**
    * @brief returns the next inline site index to be used for inlined methods
    * @returns the next inlined site index
    * If this method build is an inlined MethodBuilder, then the answer to
    * this query is delegated to the caller's MethodBuilder, which means
    * only the top-level MethodBuilder object assigns inlined site IDs.
    */
   int32_t getNextInlineSiteIndex();

   /**
    * @brief associate a particular IlBuilder object as the return landing pad for this (inlined) MethodBuilder
    * @param returnBuilder the IlBuilder object to use as a return landing pad
    */
   void setReturnBuilder(TR::IlBuilder *returnBuilder)
      {
      _returnBuilder = returnBuilder;
      }

   /**
    * @brief get the return landing pad IlBuilder for this (inlined) MethodBuilder
    * @returns the return landing pad IlBUilder object
    */
   TR::IlBuilder *returnBuilder()
      {
      return _returnBuilder;
      }

   /**
    * @brief set the name of the symbol to use to store the return value from this (inined) MethodBuilder
    * @param symbolName the name of the symbol to store any return value
    */
   void setReturnSymbol(const char *symbolName)
      {
      _returnSymbolName = symbolName;
      }

   /**
    * @brief get the return symbol name to store any return value from this (inlined) MethodBuilder
    * @returns the return symbol name
    */
   const char *returnSymbol()
      {
      return _returnSymbolName;
      }

   /*
    * @brief If this is an inlined MethodBuilder, return the MethodBuilder that directly inlined it
    * @returns the directly inlining MethodBuilder or NULL if no MethodBuilder inlined this one
    */
   TR::MethodBuilder *callerMethodBuilder();
   
   /**
    * @brief returns the client object associated with this object, allocating it if necessary
    */
   void *client();

   /**
    * @brief Store callback function to be called on client when RequestFunction is called
    */
   void setClientCallback_RequestFunction(void *callback)
      {
      _clientCallbackRequestFunction = (RequestFunctionCallback) callback;
      }

   /**
    * @brief Set the Client Allocator function
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
   virtual uint32_t countBlocks();
   virtual bool connectTrees();
   TR_Memory *trMemory() { return memoryManager._trMemory; }

   /*
    * @brief adjusts a local variable name so that it will be unique to the current inlined site to prevent inlining-induced name aliasing
    * @param name the original local variable name
    * @returns an adjusted name that will be unique to the current inlined site
    */
   const char * adjustNameForInlinedSite(const char *name);

   private:
   // We have MemoryManager as the first member of TypeDictionary, so that
   // it is the last one to get destroyed and all objects allocated using
   // MemoryManager->_memoryRegion may be safely destroyed in the destructor.
   typedef struct MemoryManager
      {
      MemoryManager();
      ~MemoryManager();

      TR::SegmentProvider *_segmentProvider;
      TR::Region *_memoryRegion;
      TR_Memory *_trMemory;
      } MemoryManager;

   MemoryManager memoryManager;

   /**
    * @brief client callback function to call when RequestFunction is called
    */
   RequestFunctionCallback     _clientCallbackRequestFunction;

   // These values are typically defined outside of a compilation
   const char                * _methodName;
   TR::IlType                * _returnType;
   int32_t                     _numParameters;

   typedef bool (*StrComparator)(const char *, const char*);

   typedef TR::typed_allocator<std::pair<const char * const, TR::SymbolReference *>, TR::Region &> SymbolMapAllocator;
   typedef std::map<const char *, TR::SymbolReference *, StrComparator, SymbolMapAllocator> SymbolMap;

   // This map should only be accessed inside a compilation via lookupSymbol
   SymbolMap                   _symbols;

   typedef TR::typed_allocator<std::pair<const char * const, int32_t>, TR::Region &> ParameterMapAllocator;
   typedef std::map<const char *, int32_t, StrComparator, ParameterMapAllocator> ParameterMap;
   ParameterMap                _parameterSlot;

   typedef TR::typed_allocator<std::pair<const char * const, TR::IlType *>, TR::Region &> SymbolTypeMapAllocator;
   typedef std::map<const char *, TR::IlType *, StrComparator, SymbolTypeMapAllocator> SymbolTypeMap;
   SymbolTypeMap               _symbolTypes;

   typedef TR::typed_allocator<std::pair<int32_t const, const char *>, TR::Region &> SlotToSymNameMapAllocator;
   typedef std::map<int32_t, const char *, std::less<int32_t>, SlotToSymNameMapAllocator> SlotToSymNameMap;
   SlotToSymNameMap            _symbolNameFromSlot;
   
   typedef TR::typed_allocator<const char *, TR::Region &> StringSetAllocator;
   typedef std::set<const char *, StrComparator, StringSetAllocator> ArrayIdentifierSet;

   // This set acts as an identifier for symbols which correspond to arrays
   ArrayIdentifierSet          _symbolIsArray;

   typedef TR::typed_allocator<std::pair<const char * const, void *>, TR::Region &> MemoryLocationMapAllocator;
   typedef std::map<const char *, void *, StrComparator, MemoryLocationMapAllocator> MemoryLocationMap;
   MemoryLocationMap           _memoryLocations;

   typedef TR::typed_allocator<std::pair<const char * const, void *>, TR::Region &> GlobalMapAllocator;
   typedef std::map<const char *, void *, StrComparator, GlobalMapAllocator> GlobalMap;

   /**
    * @brief map of global symbol names and locations of values global symbols refer to.
    *        This map should only be accessed inside a compilation via lookupSymbol.
    */
   GlobalMap                   _globals;

   typedef TR::typed_allocator<std::pair<const char * const, TR::ResolvedMethod *>, TR::Region &> FunctionMapAllocator;
   typedef std::map<const char *, TR::ResolvedMethod *, StrComparator, FunctionMapAllocator> FunctionMap;
   FunctionMap                 _functions;

   TR::IlType                ** _cachedParameterTypes;
   const char                * _definingFile;
   char                        _definingLine[MAX_LINE_NUM_LEN];
   TR::IlType                * _cachedParameterTypesArray[18];

   bool                        _newSymbolsAreTemps;
   int32_t                     _nextValueID;

   bool                        _useBytecodeBuilders;
   uint32_t                    _numBlocksBeforeWorklist;
   List<TR::BytecodeBuilder> * _countBlocksWorklist;
   List<TR::BytecodeBuilder> * _connectTreesWorklist;
   List<TR::BytecodeBuilder> * _allBytecodeBuilders;
   TR::VirtualMachineState   * _vmState;

   TR_BitVector              * _bytecodeWorklist;
   TR_BitVector              * _bytecodeHasBeenInWorklist;

   int32_t                     _inlineSiteIndex;
   int32_t                     _nextInlineSiteIndex;
   TR::IlBuilder             * _returnBuilder;
   const char                * _returnSymbolName;

private:
   static ClientAllocator      _clientAllocator;
   static ImplGetter _getImpl;
   };

} // namespace OMR

#endif // !defined(OMR_METHODBUILDER_INCL)
