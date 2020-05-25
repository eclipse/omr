/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#ifndef TEST_METHODINFO_INCL
#define TEST_METHODINFO_INCL

#include "compile/ResolvedMethod.hpp"
#include "infra/Assert.hpp"

namespace TR { class IlInjector; }
namespace TR { class IlType; }

namespace TestCompiler
{

/**
 * Describes a function, such as its signature and IlInjector.
 */
class MethodInfo
   {
   public:
   MethodInfo()
   :
      _file(NULL), _line(NULL), _name(NULL),
      _numArgs(0), _argTypes(NULL), _returnType(NULL)
      {
      }

   /**
    * Define the signature of the method this class is describing.
    */
   void DefineFunction(char *file,
                       char *line,
                       char *name,
                       int32_t numArgs,
                       TR::IlType **argTypes,
                       TR::IlType *returnType)
      {
      _file = file;
      _line = line;
      _name = name;
      _numArgs = numArgs;
      _argTypes = argTypes;
      _returnType = returnType;
      }

   /**
    * Define the IlInjector this method is based on.
    */
   void DefineILInjector(TR::IlInjector *ilInjector)
      {
      _ilInjector = ilInjector;
      }

   /**
    * Return a TR::ResolvedMethod based on the defined function and
    * IlInjector.
    *
    * Before calling this, the function signature and IlInjector must
    * be defined.
    */
   TR::ResolvedMethod ResolvedMethod() const
      {
      TR_ASSERT(
         _file != NULL
         && _line != NULL
         && _name != NULL
         && (_numArgs == 0 || _argTypes != NULL),
         "Cannot create ResolvedMethod without signature");
      TR_ASSERT(_ilInjector != NULL,
                "Cannot create ResolvedMethod without IlInjector");
      return TR::ResolvedMethod(_file, _line, _name, _numArgs, _argTypes, _returnType, 0, _ilInjector);
      }

   private:
   char *_file;
   char *_line;
   char *_name;
   int32_t _numArgs;
   TR::IlType **_argTypes;
   TR::IlType *_returnType;
   TR::IlInjector *_ilInjector;
   };

} // namespace TestCompiler

#endif // !defined(TEST_METHODINFO_INCL)
