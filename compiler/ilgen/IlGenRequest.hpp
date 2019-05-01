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

#ifndef TR_ILGENREQUEST_INCL
#define TR_ILGENREQUEST_INCL

#include "env/FilePointerDecl.hpp"
#include "ilgen/OMRIlGenRequest.hpp"
#include "infra/Annotations.hpp"

class TR_FrontEnd;
class TR_IlGenerator;
class TR_InlineBlocks;
namespace TR { class Compilation; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

namespace TR
{

class OMR_EXTENSIBLE IlGenRequest : public OMR::IlGenRequestConnector
   {

   public:

   IlGenRequest(TR::IlGeneratorMethodDetails & methodDetails) :
      OMR::IlGenRequestConnector(methodDetails) {}

   virtual bool allowIlGenOptimizations() { return true; }

   };
}

// -----------------------------------------------------------------------------
// Specialized IL gen requests.  These could be made extensible classes if
// necessary.
// -----------------------------------------------------------------------------

namespace TR
{

class CompileIlGenRequest : public IlGenRequest
   {

public:
   CompileIlGenRequest(TR::IlGeneratorMethodDetails & methodDetails) :
      IlGenRequest(methodDetails) { }

   virtual TR_IlGenerator *getIlGenerator(
         TR::ResolvedMethodSymbol *methodSymbol,
         TR_FrontEnd *fe,
         TR::Compilation *comp,
         TR::SymbolReferenceTable *symRefTab);

   virtual void print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix);
   };


class InliningIlGenRequest : public IlGenRequest
   {
protected:
   TR::ResolvedMethodSymbol *_callerSymbol;

public:
   InliningIlGenRequest(TR::IlGeneratorMethodDetails & methodDetails, TR::ResolvedMethodSymbol *callerSymbol) :
      IlGenRequest(methodDetails), _callerSymbol(callerSymbol) { }

   virtual bool allowIlGenOptimizations() { return false; }

   virtual TR_IlGenerator *getIlGenerator(
         TR::ResolvedMethodSymbol *methodSymbol,
         TR_FrontEnd *fe,
         TR::Compilation *comp,
         TR::SymbolReferenceTable *symRefTab);

   virtual void print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix);
   };


class PartialInliningIlGenRequest : public InliningIlGenRequest
   {
protected:
   TR_InlineBlocks *_inlineBlocks;

public:
   PartialInliningIlGenRequest(
         TR::IlGeneratorMethodDetails & methodDetails,
         TR::ResolvedMethodSymbol *callerSymbol,
         TR_InlineBlocks *inlineBlocks) :
      InliningIlGenRequest(methodDetails, callerSymbol), _inlineBlocks(inlineBlocks) { }

   virtual TR_IlGenerator *getIlGenerator(
         TR::ResolvedMethodSymbol *methodSymbol,
         TR_FrontEnd *fe,
         TR::Compilation *comp,
         TR::SymbolReferenceTable *symRefTab);

   virtual void print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix);
   };


}

#endif
