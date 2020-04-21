/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "env/ObjectModel.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"

namespace TR { class Node; }

OMR::ObjectModel::ObjectModel()
   {
   }

int32_t
OMR::ObjectModel::sizeofReferenceField()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

intptr_t
OMR::ObjectModel::sizeofReferenceAddress()
   {
   return TR::Compiler->target.is64Bit() ? 8 : 4;
   }

uintptr_t
OMR::ObjectModel::elementSizeOfBooleanArray()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


uint32_t
OMR::ObjectModel::getSizeOfArrayElement(TR::Node * node)
   {
   TR_UNIMPLEMENTED();
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
OMR::ObjectModel::isDiscontiguousArray(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

intptr_t
OMR::ObjectModel::getArrayLengthInElements(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uintptr_t
OMR::ObjectModel::getArrayLengthInBytes(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uintptr_t
OMR::ObjectModel::getArrayElementWidthInBytes(TR::DataType type)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uintptr_t
OMR::ObjectModel::getArrayElementWidthInBytes(TR::Compilation* comp, uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

uintptr_t
OMR::ObjectModel::decompressReference(TR::Compilation* comp, uintptr_t compressedReference)
   {
   TR_UNIMPLEMENTED();
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
