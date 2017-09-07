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

#ifndef TEST_ILBUILDER_INCL
#define TEST_ILBUILDER_INCL

#ifndef TR_ILBUILDER_DEFINED
#define TR_ILBUILDER_DEFINED
#define PUT_TEST_ILBUILDER_INTO_TR
#endif


#include "compiler/ilgen/IlBuilder.hpp"

namespace TestCompiler
{

class TestDriver;

class IlBuilder : public OMR::IlBuilder
   {
public:
   TR_ALLOC(TR_Memory::IlGenerator)

   IlBuilder(TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
      : OMR::IlBuilder(methodBuilder, types)
      { }

   IlBuilder(TR::IlBuilder *source)
      : OMR::IlBuilder(source)
      { }

   IlBuilder(TestDriver *test, TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
      : OMR::IlBuilder(methodBuilder, types)
      {
      // need to explicitly initialize TestCompiler::IlInjector layer because
      // it's hiding behind our OMR::IlBuilder base class
      setMethodAndTest((TR::ResolvedMethod *)NULL, test);
      }
   };

} // namespace TestCompiler


#ifdef PUT_TEST_ILBUILDER_INTO_TR

namespace TR
{
   class IlBuilder : public TestCompiler::IlBuilder
      {
      public:
         IlBuilder(TR::MethodBuilder *methodBuilder, TypeDictionary *types)
            : TestCompiler::IlBuilder(methodBuilder, types)
            { }

         IlBuilder(TR::IlBuilder *source)
            : TestCompiler::IlBuilder(source)
            { }

         IlBuilder(TestCompiler::TestDriver *test, TR::MethodBuilder *methodBuilder, TR::TypeDictionary *types)
            : TestCompiler::IlBuilder(test, methodBuilder, types)
            { }
      };

} // namespace TR

#endif // defined(PUT_TEST_ILBUILDER_INTO_TR)

#endif // !defined(TEST_ILBUILDER_INCL)
