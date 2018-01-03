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

#ifndef OMR_METHODBUILDER_IMPL_INCL
#define OMR_METHODBUILDER_IMPL_INCL


#ifndef TR_METHODBUILDER_IMPL_DEFINED
#define TR_METHODBUILDER_IMPL_DEFINED
#define PUT_OMR_METHODBUILDER_IMPL_INTO_TR
#endif


#include <map>
#include <set>
#include <fstream>
#include "ilgen/IlBuilderImpl.hpp"
#include "env/TypedAllocator.hpp"

// Maximum length of _definingLine string (including null terminator)
#define MAX_LINE_NUM_LEN 7

class TR_BitVector;
namespace TR { class BytecodeBuilder; }
namespace TR { class ResolvedMethod; }
namespace TR { class SymbolReference; }
namespace OMR { class VirtualMachineState; }

namespace TR { class SegmentProvider; }
namespace TR { class Region; }
class TR_Memory;

namespace TR { class BytecodeBuilderImpl; }
namespace TR { class IlType; }
namespace TR { class IlTypeImpl; }
namespace TR { class MethodBuilder; }
namespace TR { class MethodBuilderImpl; }
namespace TR { class TypeDictionaryImpl; }

namespace OMR
{

class MethodBuilderImpl : public TR::IlBuilderImpl
   {
   public:
   TR_ALLOC(TR_Memory::IlGenerator)

   MethodBuilderImpl(TR::MethodBuilder *clientBuilder, TR::TypeDictionaryImpl *types, OMR::VirtualMachineState *vmState = NULL);
   MethodBuilderImpl(TR::MethodBuilder *clientBuilder, const MethodBuilderImpl &src);
   virtual ~MethodBuilderImpl();

   void operator delete(void * ptr)
      { }

   virtual bool buildIL();

   virtual TR::MethodBuilderImpl * asMethodBuilder();

   /*
    * Client API support for MethodBuilder, in alphabetical order
    */
   void AllLocalsHaveBeenDefined()
      {
      _newSymbolsAreTemps = true;
      }
   void AppendBuilder(TR::BytecodeBuilderImpl *bb);
   void AppendBytecodeBuilder(TR::BytecodeBuilderImpl *builder);
   void DefineArrayParameter(const char *name, TR::IlTypeImpl *dt);
   void DefineFile(const char *file)
      {
      _definingFile = file;
      }
   void DefineFunction(const char* const name,
                               const char* const fileName,
                               const char* const lineNumber,
                               void           * entryPoint,
                               TR::IlTypeImpl * returnType,
                               int32_t          numParms,
                               ...);
   void DefineFunction(const char* const name,
                               const char* const fileName,
                               const char* const lineNumber,
                               void           * entryPoint,
                               TR::IlTypeImpl * returnType,
                               int32_t          numParms,
                               TR::IlTypeImpl ** parmTypes);
   void DefineLine(const char *line)
      {
      snprintf(_definingLine, MAX_LINE_NUM_LEN * sizeof(char), "%s", line);
      }
   void DefineLine(int line)
      {
      snprintf(_definingLine, MAX_LINE_NUM_LEN * sizeof(char), "%d", line);
      }
   void DefineLocal(const char *name, TR::IlTypeImpl *dt);
   void DefineMemory(const char *name, TR::IlTypeImpl *dt, void *location);
   void DefineName(const char *name);
   void DefineParameter(const char *name, TR::IlTypeImpl *type);
   void DefineReturnType(TR::IlTypeImpl *dt);
   int32_t GetNextBytecodeFromWorklist();
   TR::BytecodeBuilderImpl *OrphanBytecodeBuilderImpl(int32_t bcIndex=0, char *name=NULL);
   bool RequestFunction(const char *name);
   void SetVMState(OMR::VirtualMachineState *vmState)
      {
      _vmState = vmState;
      }



   /*
    * Client API support for IlBuilder, in alphabetical order
    */
   void AppendBuilder(TR::IlBuilderImpl *b)
      {
      this->OMR::IlBuilderImpl::AppendBuilder(b);
      }


   /*
    * Public API for *Impl classes
    */

   static TR::MethodBuilderImpl *allocate(TR::MethodBuilder *client, TR::TypeDictionaryImpl *types, OMR::VirtualMachineState *vmState);

   TR::BytecodeBuilderImpl *orphanBytecodeBuilderImpl(int32_t bcIndex=0, char *name=NULL);

   virtual void setupForBuildIL();
   virtual bool injectIL();

   int32_t getNextValueID()                                  { return _nextValueID++; }
   bool usesBytecodeBuilders()                               { return _useBytecodeBuilders; }
   void setUseBytecodeBuilders()                             { _useBytecodeBuilders = true; }

   void addToAllBytecodeBuildersList(TR::BytecodeBuilderImpl *bcBuilder);
   void addToTreeConnectingWorklist(TR::BytecodeBuilderImpl *builder);
   void addToBlockCountingWorklist(TR::BytecodeBuilderImpl *builder);

   virtual bool isMethodBuilder()                            { return true; }

   OMR::VirtualMachineState *vmState()
      {
      return _vmState;
      }

   const char *getDefiningFile()                             { return _definingFile; }
   const char *getDefiningLine()                             { return _definingLine; }

   const char *getMethodName()                               { return _methodName; }

   TR::IlTypeImpl *getReturnType()                           { return _returnType; }
   int32_t getNumParameters()                                { return _numParameters; }
   const char *getSymbolName(int32_t slot);

   TR::IlTypeImpl **getParameterTypes();
   char *getSignature(int32_t numParams, TR::IlTypeImpl **paramTypeArray);
   char *getSignature(TR::IlTypeImpl **paramTypeArray)
      {
      return getSignature(_numParameters, paramTypeArray);
      }

   TR::SymbolReference *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::SymbolReference *v);
   bool symbolDefined(const char *name);
   bool isSymbolAnArray(const char * name);

   TR::ResolvedMethod *lookupFunction(const char *name);

   /**
    * @brief add a bytecode builder to the worklist
    * @param bcBuilder is the bytecode builder whose bytecode index will be added to the worklist
    */
   void addBytecodeBuilderToWorklist(TR::BytecodeBuilderImpl *bcBuilder);

   /**
    * @brief return the client object associated with this MethodBuilderImpl object
    */
   TR::MethodBuilder *client();

   protected:

   virtual uint32_t countBlocks();
   virtual bool connectTrees();

   private:
   TR::SegmentProvider *_segmentProvider;
   TR::Region *_memoryRegion;
   TR_Memory *_trMemory;

   // These values are typically defined outside of a compilation
   const char                    * _methodName;
   TR::IlTypeImpl                * _returnType;
   int32_t                         _numParameters;

   typedef bool (*StrComparator)(const char *, const char*);

   typedef TR::typed_allocator<std::pair<const char * const, TR::SymbolReference *>, TR::Region &> SymbolMapAllocator;
   typedef std::map<const char *, TR::SymbolReference *, StrComparator, SymbolMapAllocator> SymbolMap;

   // This map should only be accessed inside a compilation via lookupSymbol
   SymbolMap                       _symbols;

   typedef TR::typed_allocator<std::pair<const char * const, int32_t>, TR::Region &> ParameterMapAllocator;
   typedef std::map<const char *, int32_t, StrComparator, ParameterMapAllocator> ParameterMap;
   ParameterMap                    _parameterSlot;

   typedef TR::typed_allocator<std::pair<const char * const, TR::IlTypeImpl *>, TR::Region &> SymbolTypeMapAllocator;
   typedef std::map<const char *, TR::IlTypeImpl *, StrComparator, SymbolTypeMapAllocator> SymbolTypeMap;
   SymbolTypeMap                   _symbolTypes;

   typedef TR::typed_allocator<std::pair<int32_t const, const char *>, TR::Region &> SlotToSymNameMapAllocator;
   typedef std::map<int32_t, const char *, std::less<int32_t>, SlotToSymNameMapAllocator> SlotToSymNameMap;
   SlotToSymNameMap                _symbolNameFromSlot;
   
   typedef TR::typed_allocator<const char *, TR::Region &> StringSetAllocator;
   typedef std::set<const char *, StrComparator, StringSetAllocator> ArrayIdentifierSet;

   // This set acts as an identifier for symbols which correspond to arrays
   ArrayIdentifierSet              _symbolIsArray;

   typedef TR::typed_allocator<std::pair<const char * const, void *>, TR::Region &> MemoryLocationMapAllocator;
   typedef std::map<const char *, void *, StrComparator, MemoryLocationMapAllocator> MemoryLocationMap;
   MemoryLocationMap               _memoryLocations;

   typedef TR::typed_allocator<std::pair<const char * const, TR::ResolvedMethod *>, TR::Region &> FunctionMapAllocator;
   typedef std::map<const char *, TR::ResolvedMethod *, StrComparator, FunctionMapAllocator> FunctionMap;
   FunctionMap                     _functions;

   TR::IlTypeImpl               ** _cachedParameterTypes;
   const char                    * _definingFile;
   char                            _definingLine[MAX_LINE_NUM_LEN];
   TR::IlTypeImpl                * _cachedParameterTypesArray[10];

   bool                            _newSymbolsAreTemps;
   int32_t                         _nextValueID;

   bool                            _useBytecodeBuilders;
   uint32_t                        _numBlocksBeforeWorklist;
   List<TR::BytecodeBuilderImpl> * _countBlocksWorklist;
   List<TR::BytecodeBuilderImpl> * _connectTreesWorklist;
   List<TR::BytecodeBuilderImpl> * _allBytecodeBuilders;
   OMR::VirtualMachineState      * _vmState;

   TR_BitVector                  * _bytecodeWorklist;
   TR_BitVector                  * _bytecodeHasBeenInWorklist;
   };

} // namespace OMR


#if defined(PUT_OMR_METHODBUILDER_IMPL_INTO_TR)

namespace TR
{
   class MethodBuilderImpl : public OMR::MethodBuilderImpl
      {
      public:
         MethodBuilderImpl(TR::MethodBuilder *clientBuilder, TR::TypeDictionaryImpl *types)
            : OMR::MethodBuilderImpl(clientBuilder, types)
            { }
         MethodBuilderImpl(TR::MethodBuilder *clientBuilder, TR::TypeDictionaryImpl *types, OMR::VirtualMachineState *vmState)
            : OMR::MethodBuilderImpl(clientBuilder, types, vmState)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_METHODBUILDER_IMPL_INTO_TR)

#endif // !defined(OMR_METHODBUILDER_IMPL_INCL)
