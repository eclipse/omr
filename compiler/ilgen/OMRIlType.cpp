/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include <stdlib.h>

#include "il/DataTypes.hpp"
#include "il/SymbolReference.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/Compilation.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/STLUtils.hpp"

#include "ilgen/IlType.hpp"


const char *
OMR::IlType::signatureNameForType[TR::NumOMRTypes] =
   {
   "V",  // NoType
   "B",  // Int8
   "C",  // Int16
   "I",  // Int32
   "J",  // Int64
   "F",  // Float
   "D",  // Double
   "L",  // Address
   "V1", // VectorInt8
   "V2", // VectorInt16
   "V4", // VectorInt32
   "V8", // VectorInt64
   "VF", // VectorFloat
   "VD", // VectorDouble
   };

const uint8_t
OMR::IlType::primitiveTypeAlignment[TR::NumOMRTypes] =
   {
   1,  // NoType
   1,  // Int8
   2,  // Int16
   4,  // Int32
   8,  // Int64
   4,  // Float
   8,  // Double
#if TR_TARGET_64BIT // HOST?
   8,  // Address/Word
#else
   4,  // Address/Word
#endif
   16, // VectorInt8
   16, // VectorInt16
   16, // VectorInt32
   16, // VectorInt64
   16, // VectorFloat
   16  // VectorDouble
   };

char *
OMR::IlType::getSignatureName()
   {
   TR::DataType dt = getPrimitiveType();
   if (dt == TR::Address)
      return (char *)_name;
   return (char *) signatureNameForType[dt];
   }

TR::IlType *
OMR::IlType::primitiveType(TR::TypeDictionary *d)
   {
   return static_cast<TR::IlType *>(this);
   }

size_t
OMR::IlType::getSize()
   {
   TR_ASSERT_FATAL(0, "The input type does not have a defined size\n");
   return 0;
   }

void *
OMR::IlType::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::IlType *>(this));
   return _client;
   }

ClientAllocator OMR::IlType::_clientAllocator = NULL;
ClientAllocator OMR::IlType::_getImpl = NULL;
