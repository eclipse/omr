/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "env/FrontEnd.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/InlineBlock.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/IO.hpp"

namespace JitBuilder
{

IlGeneratorMethodDetails::IlGeneratorMethodDetails(TR_ResolvedMethod *method) :
   OMR::IlGeneratorMethodDetailsConnector(),
   _method(static_cast<TR::ResolvedMethod *>(method))
   {
   }


bool
IlGeneratorMethodDetails::sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe)
   {
   return self()->getMethod() == other.getMethod();
   }


TR_IlGenerator *
IlGeneratorMethodDetails::getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                         TR_FrontEnd * fe,
                                         TR::Compilation *comp,
                                         TR::SymbolReferenceTable *symRefTab,
                                         bool forceClassLookahead,
                                         TR_InlineBlocks *blocksToInline)
   {
   TR_ASSERT(forceClassLookahead == false, "IlGenerator does not support class lookahead");
   TR_ASSERT(blocksToInline == 0, "IlGenerator does not yet support partial inlining");
   TR::ResolvedMethod *method = static_cast<TR::ResolvedMethod *>(methodSymbol->getResolvedMethod());
   return (TR_IlGenerator *) method->getInjector(self(), methodSymbol, static_cast<TR::FrontEnd *>(fe), symRefTab);
   }


void
IlGeneratorMethodDetails::print(TR_FrontEnd *fe, TR::FILE *file)
   {
   if (file == NULL)
      return;

   trfprintf(file, "( %p )", self()->getMethod());
   }

} // namespace JitBuilder
