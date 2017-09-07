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

#ifndef OMR_Z_REGISTER_DEPENDENCY_STRUCT_INCL
#define OMR_Z_REGISTER_DEPENDENCY_STRUCT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
namespace OMR { namespace Z { struct RegisterDependencyExt; } }
namespace OMR { typedef OMR::Z::RegisterDependencyExt RegisterDependency; }
#else
#error OMR::Z::RegisterDependencyExt expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterDependencyStruct.hpp"

#include <stddef.h>                   // for NULL
#include "codegen/CodeGenerator.hpp"  // for CodeGenerator
#include "codegen/RealRegister.hpp"   // for RealRegister, etc
#include "codegen/Register.hpp"       // for Register
#include "infra/Array.hpp"            // for TR_Array
#include "infra/Assert.hpp"           // for TR_ASSERT

namespace TR { struct RegisterDependency; }

namespace OMR
{
namespace Z
{
struct RegisterDependencyExt
   {
   uint32_t                   _realRegister:7;
   uint32_t                   _flags:2;
   uint32_t                   _virtualRegIndex:23;

   #define   BAD_VIRTUAL_IDX  0x7FFFFF
   void operator=(const TR::RegisterDependency &other);

   TR::RealRegister::RegNum getRealRegister() {return (TR::RealRegister::RegNum) _realRegister;}
   void setRealRegister(TR::RealRegister::RegNum r)
      {
      _realRegister = (uint32_t) r;
      }

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

   TR::Register * getRegister(TR::CodeGenerator *cg)
      {
       if (_virtualRegIndex==BAD_VIRTUAL_IDX)
          return NULL;
       else
          return (cg->getRegisterArray())[_virtualRegIndex];
      }

   TR::Register * setRegister(TR::Register *r)
      {
      if (r==NULL)
         _virtualRegIndex=BAD_VIRTUAL_IDX;
      else
          {
          TR_ASSERT(r->getRealRegister()==NULL, "Saw a real register where a virtual is expected");
          _virtualRegIndex=r->getIndex();
          }
      return r;
      }
   };
}
}

#endif
