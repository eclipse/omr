/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "env/ObjectModel.hpp"

#include <limits.h>                   // for LONG_MAX
#include <stddef.h>                   // for NULL
#include <stdint.h>                   // for int32_t, int64_t, uint32_t
#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "compile/Compilation.hpp"    // for Compilation
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"             // for uintptrj_t, intptrj_t
#include "infra/Assert.hpp"           // for TR_ASSERT

namespace TR { class Node; }

#define notImplemented(A) TR_ASSERT(0, "OMR::ObjectModel::%s is undefined", (A) )

OMR::ObjectModel::ObjectModel()
   {
   }

int32_t
OMR::ObjectModel::sizeofReferenceField()
   {
   notImplemented("sizeofReferenceField");
   return 0;
   }

intptrj_t
OMR::ObjectModel::sizeofReferenceAddress()
   {
   return TR::Compiler->target.is64Bit() ? 8 : 4;
   }

uintptrj_t
OMR::ObjectModel::elementSizeOfBooleanArray()
   {
   notImplemented("elementSizeOfBooleanArray");
   return 0;
   }


uint32_t
OMR::ObjectModel::getSizeOfArrayElement(TR::Node * node)
   {
   notImplemented("getSizeOfArrayElement");
   return 0;
   }

int64_t
OMR::ObjectModel::maxArraySizeInElementsForAllocation(TR::Node *newArray, TR::Compilation *comp)
   {
   return LONG_MAX;
   }

int64_t
OMR::ObjectModel::maxArraySizeInElements(int32_t knownMinElementSize, TR::Compilation *comp)
   {
   return LONG_MAX;
   }

bool
OMR::ObjectModel::isDiscontiguousArray(TR::Compilation* comp, uintptrj_t objectPointer)
   {
   notImplemented("isDiscontiguousArray");
   return false;
   }

intptrj_t
OMR::ObjectModel::getArrayLengthInElements(TR::Compilation* comp, uintptrj_t objectPointer)
   {
   notImplemented("getArrayLengthInElements");
   return 0;
   }

uintptrj_t
OMR::ObjectModel::getArrayLengthInBytes(TR::Compilation* comp, uintptrj_t objectPointer)
   {
   notImplemented("getArrayLengthInBytes");
   return 0;
   }

uintptrj_t
OMR::ObjectModel::getArrayElementWidthInBytes(TR::DataType type)
   {
   notImplemented("getArrayElementWidthInBytes");
   return 0;
   }

uintptrj_t
OMR::ObjectModel::getArrayElementWidthInBytes(TR::Compilation* comp, uintptrj_t objectPointer)
   {
   notImplemented("getArrayElementWidthInBytes");
   return 0;
   }

uintptrj_t
OMR::ObjectModel::decompressReference(TR::Compilation* comp, uintptrj_t compressedReference)
   {
   notImplemented("decompressReference");
   return 0;
   }

int32_t
OMR::ObjectModel::compressedReferenceShiftOffset()
   {
   return 0;
   }

int32_t
OMR::ObjectModel::compressedReferenceShift()
   {
   return 0;
   }
