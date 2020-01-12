/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#ifndef ACTION_INCL
#define ACTION_INCL

#include <string>

namespace OMR
{
namespace JitBuilder
{

enum Action
   {
   aNone=0,
   aConstInt8,             // create an 8 bit integer constant
   aConstInt16,            // create a 16 bit integer constant
   aConstInt32,            // create a 32 bit integer constant
   aConstInt64,            // create a 64 bit integer constant
   aConstFloat,            // create a 32 bit floating point constant
   aConstDouble,           // create a 64 bit floating point constant
   aConstAddress,          // create an address (void *) constant
   aCoercePointer,         // coerce one pointer address to a new pointer type
   aAdd,                   // create a value that is the sum of two values
   aSub,                   // create a value that is the first value minus the second value
   aMul,                   // create a value that is the product of two values
   aLoad,                  // create value by loading a named local variable
   aLoadAt,                // create a value by loading through a pointer value
   aLoadField,             // create a value by loading from a field of a struct/union
   aLoadIndirect,          // create a value by loading from a field of a pointer to struct/union
   aStore,                 // store a value to a named local variable
   aStoreAt,               // store a value through a pointer value with the same base type
   aStoreField,            // store a value into a field of a struct/union
   aStoreIndirect,         // store a value into a field through a pointer value to a struct/union
   aIndexAt,               // create a pointer value by indexing a pointer value (using base type for element size)
   aAppendBuilder,         // append the given builder into this builder
   aCall,                  // call a function, passing arguments
   aGoto,                  // branch unconditionally to the beginning of another builder object
   aReturn,                // return a value or just return if function returns NoType
   aIfCmpGreaterThan,      // if the first value is greater than the second value, branch to the builder
   aIfCmpLessThan,         // if the first value is less than the second value, branch to the builder
   aIfCmpGreaterOrEqual,   // if the first value is greater than or equal to the second value, branch to the builder
   aIfCmpLessOrEqual,      // if the first value is less than or equal to the second value, branch to the builder
   aIfThenElse,            // if the value is non-zero, branch to the first builder, otherwise branch to the second builder
   aSwitch,                // the first value must be an integer, used to select from the cases provided to branch to a particular builder
   aForLoop,               // build a for loop around the body loop that iterates from initial to end by bump, possibly providing a break builder and a continue builder to use as destinations inside the loop
   aCreateLocalArray,      // allocate stack space for an array of elements, evaluates to the address of the stack space
   aCreateLocalStruct,     // allocate stack space for a struct, evalualtes to the address of the stack space

   // New actions
   // BEGIN {
   //

   //
   // } END
   // New actions

   LastActionSentinel
   };

const int32_t NumActions = LastActionSentinel;

extern std::string actionName[];

} // namespace JitBuilder
} // namespace OMR

#endif // defined(ACTION_INCL)
