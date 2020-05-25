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

#include "ilgen/IlGenRequest.hpp"

#include <stddef.h>
#include "env/FrontEnd.hpp"
#include "ilgen/IlGen.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "env/IO.hpp"

namespace TR { class Compilation; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

namespace TR
{

TR_IlGenerator *
CompileIlGenRequest::getIlGenerator(
      TR::ResolvedMethodSymbol *methodSymbol,
      TR_FrontEnd *fe,
      TR::Compilation *comp,
      TR::SymbolReferenceTable *symRefTab)
   {
   return details().getIlGenerator(methodSymbol, fe, comp, symRefTab, false, NULL);
   }


void
CompileIlGenRequest::print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix)
   {
   trfprintf(file, "{");
   details().print(fe, file);
   trfprintf(file, "}%s", suffix);
   }


TR_IlGenerator *
InliningIlGenRequest::getIlGenerator(
      TR::ResolvedMethodSymbol *methodSymbol,
      TR_FrontEnd *fe,
      TR::Compilation *comp,
      TR::SymbolReferenceTable *symRefTab)
   {
   TR_IlGenerator *ilgen = details().getIlGenerator(methodSymbol, fe, comp, symRefTab, false, 0);
   ilgen->setCallerMethod(_callerSymbol);
   return ilgen;
   }


void
InliningIlGenRequest::print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix)
   {
   trfprintf(file, "{Inlining ");
   details().print(fe, file);
   trfprintf(file, "}%s", suffix);
   }


TR_IlGenerator *
PartialInliningIlGenRequest::getIlGenerator(
      TR::ResolvedMethodSymbol *methodSymbol,
      TR_FrontEnd *fe,
      TR::Compilation *comp,
      TR::SymbolReferenceTable *symRefTab)
   {
   TR_IlGenerator *ilgen = details().getIlGenerator(methodSymbol, fe, comp, symRefTab, false, _inlineBlocks);
   ilgen->setCallerMethod(_callerSymbol);
   return ilgen;
   }


void
PartialInliningIlGenRequest::print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix)
   {
   trfprintf(file, "{Partial inlining ");
   details().print(fe, file);
   trfprintf(file, "}%s", suffix);
   }


}
