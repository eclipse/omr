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

#ifndef OMR_METHODBUILDER_INCL
#define OMR_METHODBUILDER_INCL

#ifndef TR_METHODBUILDER_DEFINED
#define TR_METHODBUILDER_DEFINED
#define PUT_OMR_METHODBUILDER_INTO_TR
#endif

#include <vector>
#include "ilgen/IlBuilder.hpp"

namespace TR  { class BytecodeBuilder; }
namespace TR  { class TypeDictionary; }
namespace OMR { class BytecodeBuilder; }
namespace OMR { class IlBuilderImpl; }
namespace OMR { class IlValueImpl; }
namespace OMR { class VirtualMachineState; }

// TR::MethodBuilderImpl is an opaque class by design in this header
namespace TR  { class MethodBuilderImpl; }
namespace JitBuilder { class ResolvedMethod; }

namespace OMR
{

/*
 * MethodBuilder Client API
 * TODO: put class documentation here
 * Constructors/Destructors at the top, all other API in alphabetical order
 * All implementations, with the exception of constructors, are simply
 * delegations to the MethodBuilderImpl object stored in _impl (inherited
 * from IlBuilder) . There is a 1-1 relationship between MethodBuilder and
 * MethodBuilderImpl objects. When the MethodBuilder object is destroyed,
 * the associated MethodBuilderImpl object will also be destroyed.
 */
class MethodBuilder : public TR::IlBuilder
   {
   public:
   friend class OMR::IlBuilder;
   friend class OMR::IlValueImpl;
   friend class OMR::BytecodeBuilder;
   friend class JitBuilder::ResolvedMethod;

   /**
    *  @brief constructor for use by clients
    */
   MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState);

   /**
    * @brief constructor for use by other JitBuilder classes (e.g. ThunkBuilder)
    */
   MethodBuilder(TR::IlBuilderImpl *impl, TR::TypeDictionary *types, OMR::VirtualMachineState *vmState);

   /**
    * @brief destructor also takes care of destructing the underlying implementation object
    */
   virtual ~MethodBuilder();

   /**
    * @brief to be implemented by subclass to build IL for a method
    */
   virtual bool buildIL() { return false; }

   /**
    * @brief tell compiler that all user-visible local variables have been defined;
    *        any other locals are temporaries that do not need to be visible.
    *        Sometimes permits more optimization opportunities with temporary
    *        results, but currently only has an effect for floating point values.
    */
   void AllLocalsHaveBeenDefined();

   /**
    * @brief append a BytecodeBuilder object to the current MethodBuilder: control
    *        from earlier code will fall through to the given object
    * @param bb the BytecodeBuilder object to append
    */
   void AppendBuilder(TR::BytecodeBuilder *bb);

   /**
    * @brief append an IlBuilder object to the current MethodBuilder: control from
    *        earlier code will fall through to the given object
    * @param b the IlBuilder object to append
    */
   void AppendBuilder(TR::IlBuilder *b);

   /**
    * @brief append the first bytecode builder object to this method
    * @param builder the bytecode builder object to add, usually for bytecode index 0
    * A side effect of this call is that the builder is added to the worklist so that
    * all other bytecodes can be processed by asking for GetNextBytecodeFromWorklist()
    */
   void AppendBytecodeBuilder(TR::BytecodeBuilder *builder);

   /**
    * @brief adds a parameter that is an array of the given type; order of calls
    *        to this function and DefineParameter defines the expected order of
    *        parameters
    * @param name the name of the array parameter
    * @param dt the type of each element of the array parameter
    */
   void DefineArrayParameter(const char *name, TR::IlType *dt);

   /**
    * @brief define the source file name defining this method, useful for logging
    * @param fileName the file name
    */
   void DefineFile(const char *fileName);

   /**
    * @brief define a native function (variadic form) that can be later called by
    *        name via Call passing arguments
    * @param name the name that can be given to Call to later call this function
    * @param fileName the name of the file where this function is defined (used
    *        for logging)
    * @param lineNumber the line number in the file where this function is defined
    *        (used for logging)
    * @param entryPoint the target address of this function
    * @param returnType the type of the value returned by the function (NoType if
    *        void)
    * @param numParms the number of parameters taken by the function
    * @param ... list of types describing the parameter types in order, expected
    *        to be numParms types
    */
   void DefineFunction(const char* const name,
                       const char* const fileName,
                       const char* const lineNumber,
                       void            * entryPoint,
                       TR::IlType      * returnType,
                       int32_t           numParms,
                       ...);

   /**
    * @brief define a native function that can be later called by name via Call
    *        passing arguments
    * @param name the name that can be given to Call to later call this function
    * @param fileName the name of the file where this function is defined (used
    *        for logging)
    * @param lineNumber the line number in the file where this function is defined
    *        (used for logging)
    * @param entryPoint the target address of this function
    * @param returnType the type of the value returned by the function (NoType if
    *        void)
    * @param numParms the number of parameters taken by the function
    * @param parms array of types describing the parameter types in order
    */
   void DefineFunction(const char* const name,
                       const char* const fileName,
                       const char* const lineNumber,
                       void            * entryPoint,
                       TR::IlType      * returnType,
                       int32_t           numParms,
                       TR::IlType     ** parmTypes);

   /**
    * @brief define the line number (as string) of the source file that defines
    *        this method, useful for logging
    * @param line the line number
    */
   void DefineLine(const char *line);

   /**
    * @brief define the line number (as integer) of the source file that defines
    *        this method, useful for logging
    * @param line the line number
    */
   void DefineLine(int line);

   /**
    * @brief define a new local variable (slot in the method's stack frame)
    * @param name the name of the local variable
    * @param dt the type of the local variable: only values of this type can be
    *        stored in the variable
    */
   void DefineLocal(const char *name, TR::IlType *dt);

   /**
    * @brief define a memory location by name and type
    * @param name the name of the memory location (could be used to build a
    *        relocation)
    * @param dt the type of the value stored at the memory location
    * @param location the actual location of the value
    */
   void DefineMemory(const char *name, TR::IlType *dt, void *location);

   /**
    * @brief define the name of this method; used primarily for logging
    * @param name the desired name
    */
   void DefineName(const char *name);

   /**
    * @brief adds another parameter of the given type; order of calls to this
    *        function and DefineArrayParameter defines expected order of parameters
    * @param name the name of the array parameter
    * @param dt the type of each element of the array parameter
    */
   void DefineParameter(const char *name, TR::IlType *type);

   /**
    * @brief define the return type of this method
    * @param dt the type that will be returned
    */
   void DefineReturnType(TR::IlType *dt);

   /**
    * @brief get lowest index bytecode from the worklist
    * @returns lowest bytecode index or -1 if worklist is empty
    * It is important to use the worklist when working with BytecodeBuilders because
    * the worklist guarantees no bytecode will be processed before at least one
    * predecessor bytecode has been processed, which means there should be a non-NULL
    * VirtualMachineState object on the associated BytecodeBuilder object.
    */
   int32_t GetNextBytecodeFromWorklist();

   /**
    * @brief allocate a new BytecodeBuilder object using memory tracked by the compiler
    *        if possible
    * @param bcIndex (optional) the bytecode index to use for the new object (otherwise
    *        will be zero). To use the worklist (recommended), meaningful bytecode indices
    *        must be assigned.
    * @param name (optional) a name to use for the new object (useful for logging,
    *        otherwise will be NULL)
    * @returns the newly allocated object
    */
   TR::BytecodeBuilder *OrphanBytecodeBuilder(int32_t bcIndex=0, char *name=NULL);

   /**
    * @brief callback to be optionally implemented by subclasses: will be called when
    *        a Call is issued to a function that has not yet been defined. Provides a
    *        mechanism for MethodBuilder subclasses to provide method lookup on demand
    *        rather than all up front via the constructor.
    @ @param name the name of the method requested to be defined
    * @returns true if the function was found and DefineFunction has been called for it,
    *          otherwise false
    */
   virtual bool RequestFunction(const char *name) { return false; }

   /**
    * @brief initialize the VirtualMachineState to use in this MethodBuilder. Once set,
    *        this VirtualMachineState will be automatically updated and appropriately
    *        merged on all flow transitions between BytecodeBuilders.
    * @param vmState the virtual machine state object to use
    * TODO capitalize to SetVMState for consistency
    */
   void setVMState(OMR::VirtualMachineState *vmState);

   TR::TypeDictionary *typeDictionary()
      { return _types; }

   protected:
   /**
    * @brief used for delegating work to the underlying implementation class for MethodBuilder
    * @returns TR::MethodBuilderImpl pointer corresponding to this MethodBuilder object
    */
   TR::MethodBuilderImpl *impl();

   private:
   void newBuilder(TR::IlBuilder *b);

   TR::TypeDictionary         * _types;

   std::vector<TR::IlBuilder *> _ownedBuilderObjects;
   };

} // namespace OMR

#if defined(PUT_OMR_METHODBUILDER_INTO_TR)

namespace TR
{
   class MethodBuilder : public OMR::MethodBuilder
      {
      public:
         MethodBuilder(TR::TypeDictionary *types, OMR::VirtualMachineState *vmState=NULL)
            : OMR::MethodBuilder(types, vmState)
            { }
         MethodBuilder(TR::IlBuilderImpl *impl, TR::TypeDictionary *types, OMR::VirtualMachineState *vmState=NULL)
            : OMR::MethodBuilder(impl, types, vmState)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_METHODBUILDER_INTO_TR)

#endif // !defined(OMR_METHODBUILDER_INCL)
