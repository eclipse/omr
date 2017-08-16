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

#include <stdint.h>
#include <string>

#include "compile/Compilation.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/ThunkBuilder.hpp"
#include "ilgen/TypeDictionary.hpp"

#define OPT_DETAILS "O^O THUNKBUILDER: "

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

/**
 * ThunkBuilder is a MethodBuilder object representing a thunk for
 * calling native functions with a particular signature (kind of like
 * libffi). It is designed to take a target address and an array of
 * Word-sized arguments and to call that target address with the
 * provided arguments (after converting to the appropriate parameter
 * types). The types of the parameters and return type are provided at
 * construction time so that different ThunkBuilder instances can use
 * different method signatures.
 */

namespace OMR
{

ThunkBuilder::ThunkBuilder(TR::TypeDictionary *types, const char *name, TR::IlType *returnType,
                           uint32_t numCalleeParams, TR::IlType **calleeParamTypes)
   : TR::MethodBuilder(types),
   _numCalleeParams(numCalleeParams),
   _calleeParamTypes(calleeParamTypes)
   {
   DefineFile(__FILE__);
   DefineLine(LINETOSTR(__LINE__));
   DefineName(name);
   DefineReturnType(returnType);
   DefineParameter("target", Address); // target
   DefineParameter("args", types->PointerTo(Word));     // array of arguments

   DefineFunction("thunkCallee",
                  __FILE__,
                  LINETOSTR(__LINE__),
                  0, // entry will be a computed value
                  returnType,
                  numCalleeParams,
                  calleeParamTypes);
   }

bool
ThunkBuilder::buildIL()
   {
   TR::IlType *pWord = typeDictionary()->PointerTo(Word);

   uint32_t numArgs = _numCalleeParams+1;
   TR::IlValue **args = (TR::IlValue **) comp()->trMemory()->allocateHeapMemory(numArgs * sizeof(TR::IlValue *));

   // first argument is the target address
   args[0] = Load("target");

   // followed by the actual arguments
   for (uint32_t p=0; p < _numCalleeParams; p++)
      {
      uint32_t a=p+1;
      args[a] = ConvertTo(_calleeParamTypes[p],
                   LoadAt(pWord,
                      IndexAt(pWord,
                         Load("args"),
                         ConstInt32(p))));
      }

   TR::IlValue *retValue = ComputedCall("thunkCallee", numArgs, args);

   if (getReturnType() != NoType)
      Return(retValue);

   Return();

   return true;
   }

} // namespace OMR
