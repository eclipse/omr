/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
 *  * This program and the accompanying materials are made available under
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


#ifndef TYPEDICTIONARY_EXTRAS_OUTSIDE_CLASS_INCL
#define TYPEDICTIONARY_EXTRAS_OUTSIDE_CLASS_INCL

#include <OMR/TypeTraits.hpp>

/**
 * @brief Convenience API for defining JitBuilder structs from C/C++ structs (PODs)
 *
 * These macros simply allow the name of C/C++ structs and fields to be used to
 * define JitBuilder structs. This can help ensure a consistent API between a
 * VM struct and JitBuilder's representation of the same struct. Their definitions
 * expand to calls to `TR::TypeDictionary` methods that create a representation
 * corresponding to that specified type.
 *
 * ## Usage
 *
 * Given the following struct:
 *
 * ```c++
 * struct MyStruct {
 *    int32_t id;
 *    int32_t* val;
 * };
 *
 * a JitBuilder representation of this struct can be defined as follows:
 *
 * ```c++
 * TR::TypeDictionary d;
 * d.DEFINE_STRUCT(MyStruct);
 * d.DEFINE_FIELD(MyStruct, id, Int32);
 * d.DEFINE_FIELD(MyStruct, val, pInt32);
 * d.CLOSE_STRUCT(MyStruct);
 * ```
 *
 * This definition will expand to the following:
 *
 * ```c++
 * TR::TypeDictionary d;
 * d.DefineStruct("MyStruct");
 * d.DefineField("MyStruct", "id", Int32, offsetof(MyStruct, id));
 * d.DefineField("MyStruct", "val", pInt32, offsetof(MyStruct, val));
 * d.CloseStruct("MyStruct", sizeof(MyStruct));
 * ```
 */
#define DEFINE_STRUCT(structName) \
   DefineStruct(#structName)
#define DEFINE_FIELD(structName, fieldName, filedIlType) \
   DefineField(#structName, #fieldName, filedIlType, offsetof(structName, fieldName))
#define CLOSE_STRUCT(structName) \
   CloseStruct(#structName, sizeof(structName))

#endif // !defined(TYPEDICTIONARY_EXTRAS_OUTSIDE_CLASS_INCL)
