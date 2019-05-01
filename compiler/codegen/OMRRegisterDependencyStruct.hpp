/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef OMR_REGISTER_DEPENDENCY_STRUCT_INCL
#define OMR_REGISTER_DEPENDENCY_STRUCT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
namespace OMR { struct RegisterDependencyExt; }
namespace OMR { typedef OMR::RegisterDependencyExt RegisterDependency; }
#endif

#include <stdint.h>
#include "codegen/Register.hpp"

#define DefinesDependentRegister    0x01
#define ReferencesDependentRegister 0x02
#define UsesDependentRegister      (ReferencesDependentRegister | DefinesDependentRegister)

namespace OMR
{

struct RegisterDependencyExt
   {
   uint8_t                                 _flags;
   TR::Register                            *_virtualRegister;

   uint32_t getFlags()             {return _flags;}
   uint32_t assignFlags(uint8_t f) {return _flags = f;}
   uint32_t setFlags(uint8_t f)    {return _flags |= f;}
   uint32_t resetFlags(uint8_t f)  {return _flags &= ~f;}

   uint32_t getDefsRegister()   {return _flags & DefinesDependentRegister;}
   uint32_t setDefsRegister()   {return _flags |= DefinesDependentRegister;}
   uint32_t resetDefsRegister() {return _flags &= ~DefinesDependentRegister;}

   uint32_t getRefsRegister()   {return _flags & ReferencesDependentRegister;}
   uint32_t setRefsRegister()   {return _flags |= ReferencesDependentRegister;}
   uint32_t resetRefsRegister() {return _flags &= ~ReferencesDependentRegister;}

   uint32_t getUsesRegister()   {return _flags & UsesDependentRegister;}

   TR::Register *getRegister()               {return _virtualRegister;}
   TR::Register *setRegister(TR::Register *r) {return (_virtualRegister = r);}

   };
}

#endif
