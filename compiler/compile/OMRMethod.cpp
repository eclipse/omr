/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "compile/Method.hpp"

#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Assert.hpp"


TR::Method *
OMR::Method::self()
   {
   return static_cast<TR::Method *>(this);
   }


uint32_t
OMR::Method::numberOfExplicitParameters()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR::DataType
OMR::Method::parmType(uint32_t)
   {
   TR_UNIMPLEMENTED();
   return TR::NoType;
   }


TR::ILOpCodes
OMR::Method::directCallOpCode()
   {
   TR_UNIMPLEMENTED();
   return TR::BadILOp;
   }


TR::ILOpCodes
OMR::Method::indirectCallOpCode()
   {
   TR_UNIMPLEMENTED();
   return TR::BadILOp;
   }


TR::DataType
OMR::Method::returnType()
   {
   TR_UNIMPLEMENTED();
   return TR::NoType;
   }


bool
OMR::Method::returnTypeIsUnsigned()
   {
   TR_UNIMPLEMENTED();
   return false;
   }


uint32_t
OMR::Method::returnTypeWidth()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


TR::ILOpCodes
OMR::Method::returnOpCode()
   {
   TR_UNIMPLEMENTED();
   return TR::BadILOp;
   }


uint16_t
OMR::Method::classNameLength()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


uint16_t
OMR::Method::nameLength()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


uint16_t
OMR::Method::signatureLength()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


char *
OMR::Method::classNameChars()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


char *
OMR::Method::nameChars()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


char *
OMR::Method::signatureChars()
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


bool
OMR::Method::isConstructor()
   {
   TR_UNIMPLEMENTED();
   return false;
   }


bool
OMR::Method::isFinalInObject()
   {
   TR_UNIMPLEMENTED();
   return false;
   }


const char *
OMR::Method::signature(TR_Memory *, TR_AllocationKind)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


void
OMR::Method::setArchetypeSpecimen(bool b)
   {
   TR_UNIMPLEMENTED();
   }


TR_MethodParameterIterator *
OMR::Method::getParameterIterator(TR::Compilation&, TR_ResolvedMethod *)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


bool
OMR::Method::isBigDecimalMethod(TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   return false;
   }


bool
OMR::Method::isUnsafeCAS(TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   return false;
   }


bool
OMR::Method::isUnsafeWithObjectArg(TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   return false;
   }


bool
OMR::Method::isBigDecimalConvertersMethod(TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

