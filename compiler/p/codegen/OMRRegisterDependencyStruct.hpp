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

#ifndef OMR_Power_REGISTER_DEPENDENCY_STRUCT_INCL
#define OMR_Power_REGISTER_DEPENDENCY_STRUCT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
#define OMR_REGISTER_DEPENDENCY_STRUCT_CONNECTOR
namespace OMR { namespace Power { struct RegisterDependencyExt; } }
namespace OMR { typedef OMR::Power::RegisterDependencyExt RegisterDependency; }
#else
#error OMR::Power::RegisterDependencyExt expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterDependencyStruct.hpp"

#include "codegen/RealRegister.hpp"

#define DefinesDependentRegister    0x01
#define ReferencesDependentRegister 0x02
#define UsesDependentRegister       (ReferencesDependentRegister | DefinesDependentRegister)
#define ExcludeGPR0InAssigner       0x80

namespace OMR
{

namespace Power
{

struct RegisterDependencyExt: OMR::RegisterDependencyExt
   {
   TR::RealRegister::RegNum  _realRegister;

   TR::RealRegister::RegNum getRealRegister() {return _realRegister;}
   TR::RealRegister::RegNum setRealRegister(TR::RealRegister::RegNum r) { return (_realRegister = r); }

   uint32_t getExcludeGPR0()    {return _flags & ExcludeGPR0InAssigner;}
   uint32_t setExcludeGPR0()    {return (_flags |= ExcludeGPR0InAssigner);}
   uint32_t resetExcludeGPR0()  {return (_flags &= ~ExcludeGPR0InAssigner);}


   };

}
}

#endif
