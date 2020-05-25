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

#include "ilgen/IlInjector.hpp"

#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Recompilation.hpp"
#include "env/FrontEnd.hpp"
#include "env/StackMemoryRegion.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "infra/Cfg.hpp"

#define OPT_DETAILS "O^O ILGEN: "

void
TestCompiler::IlInjector::setMethodAndTest(TR::IlInjector *source)
   {
   setMethodAndTest(source->_method, source->_test);
   }

void
TestCompiler::IlInjector::initialize(TR::IlGeneratorMethodDetails * details,
                             TR::ResolvedMethodSymbol     * methodSymbol,
                             TR::FrontEnd                 * fe,
                             TR::SymbolReferenceTable     * symRefTab)
   {
   this->OMR::IlInjector::initialize(details, methodSymbol, fe, symRefTab);
   _method = reinterpret_cast<TR::ResolvedMethod *>(methodSymbol->getResolvedMethod());
   }

TR::Node *
TestCompiler::IlInjector::callFunction(TR::ResolvedMethod *resolvedMethod, TR::IlType *returnType, int32_t numArgs, TR::Node *firstArg)
   {
   TR_ASSERT(numArgs == 1, "Hack alert: currently only supports single argument function calls!");

   // arbitrarily treat as "Static" so no receiver expected and should match use of a direct call opcode
   TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateMethodSymbol(_methodSymbol->getResolvedMethodIndex(), 0, resolvedMethod, TR::MethodSymbol::Kinds::Static);
   TR::Node *callNode = TR::Node::createWithSymRef(TR::ILOpCode::getDirectCall(returnType->getPrimitiveType()), numArgs, methodSymRef);
   callNode->setAndIncChild(0, firstArg);
   return callNode;
   }
