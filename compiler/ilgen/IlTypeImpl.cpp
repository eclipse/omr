/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#include <stdlib.h>

#include "env/Region.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "ilgen/IlTypeImpl.hpp"



const char *OMR::IlTypeImpl::_signatureNameForType[] =
   {
   "V",  // NoType
   "B",  // Int8
   "C",  // Int16
   "I",  // Int32
   "J",  // Int64
   "F",  // Float
   "D",  // Double
   "L",  // Address
   // TODO: vector types!
   };

char *
OMR::IlTypeImpl::getSignatureName()
   {
   TR::DataType dt = getRealPrimitiveType();
   if (dt == TR::Address)
      return (char *)_name;
   return (char *) _signatureNameForType[dt];
   }

size_t
OMR::IlTypeImpl::getSize()
   {
   TR_ASSERT(0, "The input type does not have a defined size\n");
   return 0;
   }
