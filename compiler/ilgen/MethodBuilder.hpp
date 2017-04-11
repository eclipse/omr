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


#ifndef OMR_METHODBUILDER_INCL
#define OMR_METHODBUILDER_INCL


#ifndef TR_METHODBUILDER_DEFINED
#define TR_METHODBUILDER_DEFINED
#define PUT_OMR_METHODBUILDER_INTO_TR
#endif


#include <fstream>
#include "ilgen/IlBuilder.hpp"

class TR_HashTabInt;
class TR_HashTabString;
class TR_BitVector;
namespace TR { class BytecodeBuilder; }
namespace TR { class ResolvedMethod; }
namespace OMR { class VirtualMachineState; }

namespace OMR
{

class MethodBuilder : public TR::IlBuilder
   {
   public:
   TR_ALLOC(TR_Memory::IlGenerator)

   MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState = NULL);
   virtual ~MethodBuilder() { }

   virtual void setupForBuildIL();

   virtual bool injectIL();

   bool usesBytecodeBuilders()                               { return _useBytecodeBuilders; }
   void setUseBytecodeBuilders()                             { _useBytecodeBuilders = true; }

   void addToAllBytecodeBuildersList(TR::BytecodeBuilder *bcBuilder);
   void addToTreeConnectingWorklist(TR::BytecodeBuilder *builder);
   void addToBlockCountingWorklist(TR::BytecodeBuilder *builder);

   OMR::VirtualMachineState *vmState()                       { return _vmState; }
   void setVMState(OMR::VirtualMachineState *vmState)        { _vmState = vmState; }

   virtual bool isMethodBuilder()                            { return true; }
   virtual TR::MethodBuilder *asMethodBuilder();

   TR::TypeDictionary *typeDictionary()                      { return _types; }

   const char *getDefiningFile()                             { return _definingFile; }
   const char *getDefiningLine()                             { return _definingLine; }

   const char *getMethodName()                               { return _methodName; }
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

   TR::IlValue *lookupSymbol(const char *name);
   void defineSymbol(const char *name, TR::IlValue *v);
   bool symbolDefined(const char *name);
   bool isSymbolAnArray(const char * name);

   TR::ResolvedMethod *lookupFunction(const char *name);

   TR::BytecodeBuilder *OrphanBytecodeBuilder(int32_t bcIndex=0, char *name=NULL);

   void AppendBuilder(TR::BytecodeBuilder *bb);
   void AppendBuilder(TR::IlBuilder *b)    { this->OMR::IlBuilder::AppendBuilder(b); }

   void DefineFile(const char *file)                         { _definingFile = file; }
   void DefineLine(const char *line)                         { _definingLine = line; }

   void DefineName(const char *name);
   void DefineParameter(const char *name, TR::IlType *type);
   void DefineArrayParameter(const char *name, TR::IlType *dt);
   void DefineReturnType(TR::IlType *dt);
   void DefineLocal(const char *name, TR::IlType *dt);
   void DefineMemory(const char *name, TR::IlType *dt, void *location);
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
   
   protected:
   void initMaps();
   virtual uint32_t countBlocks();
   virtual bool connectTrees();

   private:

   // These values are typically defined outside of a compilation
   const char                * _methodName;
   TR::IlType                * _returnType;
   int32_t                     _numParameters;
   TR_HashTabString          * _parameterSlot;
   TR_HashTabString          * _symbolTypes;
   TR_HashTabInt             * _symbolNameFromSlot;
   TR_HashTabString          * _symbolIsArray;
   TR_HashTabString          * _memoryLocations;
   TR_HashTabString          * _functions;

   TR::IlType               ** _cachedParameterTypes;
   char                      * _cachedSignature;
   const char                * _definingFile;
   const char                * _definingLine;
   TR::IlType                * _cachedParameterTypesArray[10];
   char                        _cachedSignatureArray[100];

   // This map should only be accessed inside a compilation via lookupSymbol
   TR_HashTabString          * _symbols;
   bool                        _newSymbolsAreTemps;

   bool                        _useBytecodeBuilders;
   uint32_t                    _numBlocksBeforeWorklist;
   List<TR::BytecodeBuilder> * _countBlocksWorklist;
   List<TR::BytecodeBuilder> * _connectTreesWorklist;
   List<TR::BytecodeBuilder> * _allBytecodeBuilders;
   OMR::VirtualMachineState  * _vmState;

   TR_BitVector              * _bytecodeWorklist;
   TR_BitVector              * _bytecodeHasBeenInWorklist;

   std::fstream              * _rpCpp;
   };

} // namespace OMR


#if defined(PUT_OMR_METHODBUILDER_INTO_TR)

namespace TR
{
   class MethodBuilder : public OMR::MethodBuilder
      {
      public:
         MethodBuilder(TR::TypeDictionary *types)
            : OMR::MethodBuilder(types)
            { }
         MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState)
            : OMR::MethodBuilder(types, vmState)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_METHODBUILDER_INTO_TR)

#endif // !defined(OMR_METHODBUILDER_INCL)
