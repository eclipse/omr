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


#ifndef OMR_THUNKBUILDER_INCL
#define OMR_THUNKBUILDER_INCL


#ifndef TR_THUNKBUILDER_DEFINED
#define TR_THUNKBUILDER_DEFINED
#define PUT_OMR_THUNKBUILDER_INTO_TR
#endif


#include "ilgen/MethodBuilder.hpp"


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

   private:
   uint32_t      _numCalleeParams;
   TR::IlType ** _calleeParamTypes;
   };

} // namespace OMR


#if defined(PUT_OMR_THUNKBUILDER_INTO_TR)

namespace TR
{
   class ThunkBuilder : public OMR::ThunkBuilder
      {
      public:
         ThunkBuilder(TR::TypeDictionary *types, const char *name, TR::IlType *returnType,
                      uint32_t numCalleeParams, TR::IlType **calleeParamTypes)
            : OMR::ThunkBuilder(types, name, returnType, numCalleeParams, calleeParamTypes)
            { }
      };

} // namespace TR

#endif // defined(PUT_OMR_THUNKBUILDER_INTO_TR)

#endif // !defined(OMR_THUNKBUILDER_INCL)
