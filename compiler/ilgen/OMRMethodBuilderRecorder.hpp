/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef OMR_METHODBUILDERRECORDER_INCL
#define OMR_METHODBUILDERRECORDER_INCL

#include "ilgen/IlBuilder.hpp"

// Maximum length of _definingLine string (including null terminator)
#define MAX_LINE_NUM_LEN 7

class TR_BitVector;
namespace TR { class BytecodeBuilder; }
namespace OMR { class VirtualMachineState; }

namespace OMR
{

class MethodBuilderRecorder : public TR::IlBuilder
   {
   public:
   TR_ALLOC(TR_Memory::IlGenerator)

   MethodBuilderRecorder(TR::TypeDictionary *types, TR::VirtualMachineState *vmState, TR::JitBuilderRecorder *recorder);
   virtual ~MethodBuilderRecorder()                          { }

   int32_t getNextValueID()                                  { return _nextValueID++; }

   bool usesBytecodeBuilders()                               { return _useBytecodeBuilders; }
   void setUseBytecodeBuilders()                             { _useBytecodeBuilders = true; }

   TR::VirtualMachineState *vmState()                       { return _vmState; }
   void setVMState(TR::VirtualMachineState *vmState)        { _vmState = vmState; }

   void AllLocalsHaveBeenDefined();

   void AppendBuilder(TR::IlBuilder *b)                     { TR::IlBuilder::AppendBuilder(b); }

   /**
    * @brief append a bytecode builder object to this method
    * @param builder the bytecode builder object to add, usually for bytecode index 0
    * A side effect of this call is that the builder is added to the worklist so that
    * all other bytecodes can be processed by asking for GetNextBytecodeFromWorklist()
    */
   void AppendBuilder(TR::BytecodeBuilder *bb);

   void DefineFile(const char *file);
   void DefineLine(const char *line);
   void DefineLine(int32_t line);
   void DefineName(const char *name);
   void DefineLocal(const char *name, TR::IlType *dt);
   void DefineMemory(const char *name, TR::IlType *dt, void *location);
   void DefineParameter(const char *name, TR::IlType *type);
   void DefineArrayParameter(const char *name, TR::IlType *dt);
   void DefineReturnType(TR::IlType *dt);
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
    * @brief will be called if a Call is issued to a function that has not yet been defined, provides a
    *        mechanism for MethodBuilder subclasses to provide method lookup on demand rather than all up
    *        front via the constructor.
    * @returns true if the function was found and DefineFunction has been called for it, otherwise false
    */
   virtual bool RequestFunction(const char *name) { return false; }

   /**
    * @brief add a bytecode builder to the worklist
    * @param bcBuilder is the bytecode builder whose bytecode index will be added to the worklist
    */
   void addBytecodeBuilderToWorklist(TR::BytecodeBuilder *bcBuilder);

   /**
    * @brief get lowest index bytecode from the worklist
    * @returns lowest bytecode index or -1 if worklist is empty
    * It is important to use the worklist because it guarantees no bytecode will be
    * processed before at least one predecessor bytecode has been processed, which
    * means there should be a non-NULL VirtualMachineState object on the associated
    * BytecodeBuilder object.
    */
   int32_t GetNextBytecodeFromWorklist();

   TR::JitBuilderRecorder *recorder() const { return _recorder; }
   void setRecorder(TR::JitBuilderRecorder *recorder) { _recorder = recorder; }

   protected:
   int32_t                          _nextValueID;

   bool                             _useBytecodeBuilders;
   TR::VirtualMachineState       * _vmState;

   TR_BitVector                   * _bytecodeWorklist;
   TR_BitVector                   * _bytecodeHasBeenInWorklist;

   TR::JitBuilderRecorder         * _recorder;

   TR::MethodBuilder *asMethodBuilder();
   };

} // namespace OMR

#endif // !defined(OMR_METHODBUILDERRECORDER_INCL)
