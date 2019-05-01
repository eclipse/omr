/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#ifndef OMR_THUNKBUILDER_INCL
#define OMR_THUNKBUILDER_INCL


#include "ilgen/MethodBuilder.hpp"

namespace TR { class ThunkBuilder; }

namespace OMR
{

/**
 * @brief provide mechanism to call arbitrary C functions by signature passing array of arguments
 *
 * ThunkBuilder provides a mechanism like libffi to be able to construct a call to any C function
 * by passing arguments in an array. It is not an uncommon scenario when writing a languge runtime
 * that you have to call a native function, but you cannot write a direct call to it. For example,
 * a language may provide the name of a native call target along with its signature and arguments,
 * but the runtime still needs to pass those arguments to particular native function. The runtime
 * code needs to be able to handle any kind of native call signature: any number of arguments and
 * any combination of parameter types and any return type. ThunkBuilder provides a convenient way
 * to generalize the calling of native functions assuming you can describe the signature and have
 * a function address to call.
 *
 * When creating a ThunkBuilder instance, you provide the set of parameters and the return type. After
 * compiling the ThunkBuilder instance, the resulting thunk can be used to call any function with that
 * signature. You pass the address of the function as well as a properly sized array of Word sized
 * arguments. The thunk will convert each argument to the type of its corresponding parameter as it
 * calls the given function, and will return the return value as expected.
 */

class ThunkBuilder : public TR::MethodBuilder
   {
   public:
   TR_ALLOC(TR_Memory::IlGenerator)

   /**
    * @brief construct a ThunkBuilder for a particular signature
    * @param types TypeDictionary object that will be used by the ThunkBuilder object
    * @param name primarily used for debug purposes and will appear in the compilation log
    * @param returnType return type for the thunk's signature
    * @param numCalleeParams number of parameters in the thunk's signature
    * @param calleeParamTypes array of parameter types in the thunk's signature, must have numCalleeParams elements
    */
   ThunkBuilder(TR::TypeDictionary *types, const char *name, TR::IlType *returnType,
                uint32_t numCalleeParams, TR::IlType **calleeParamTypes);
   virtual ~ThunkBuilder() { }

   virtual bool buildIL();

   /**
    * @brief returns the client object associated with this object, allocating it if necessary
    */
   void *client();

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

   private:
   uint32_t      _numCalleeParams;
   TR::IlType ** _calleeParamTypes;

   // static void * allocateClientObject(TR::ThunkBuilder *);
   static ClientAllocator _clientAllocator;

   /**
    * @brief pointer to impl getter function
    */
   static ImplGetter _getImpl;
   };

} // namespace OMR

#endif // !defined(OMR_THUNKBUILDER_INCL)
