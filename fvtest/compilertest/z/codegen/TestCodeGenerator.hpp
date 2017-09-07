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

#ifndef TEST_Z_CODEGENERATORBASE_INCL
#define TEST_Z_CODEGENERATORBASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TEST_CODEGENERATORBASE_CONNECTOR
#define TEST_CODEGENERATORBASE_CONNECTOR

namespace TestCompiler { namespace Z { class CodeGenerator; } }
namespace TestCompiler { typedef Z::CodeGenerator CodeGeneratorConnector; }

#else
#error TestCompiler::Z::CodeGenerator expected to be a primary connector, but a TestCompiler connector is already defined
#endif


#include "compilertest/codegen/TestCodeGenerator.hpp"
#include "codegen/LinkageConventionsEnum.hpp"


namespace TestCompiler
{
namespace Z
{

class OMR_EXTENSIBLE CodeGenerator : public TestCompiler::CodeGenerator
   {
   public:

   CodeGenerator();

   };

} // namespace Z
} // namespace TestCompiler

#endif // !defined(TEST_Z_CODEGENERATORBASE_INCL)
