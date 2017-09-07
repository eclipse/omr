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

#ifndef TEST_ILGENERATOR_METHOD_DETAILS_INCL
#define TEST_ILGENERATOR_METHOD_DETAILS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TEST_ILGENERATOR_METHOD_DETAILS_CONNECTOR
#define TEST_ILGENERATOR_METHOD_DETAILS_CONNECTOR
namespace TestCompiler { class IlGeneratorMethodDetails; }
namespace TestCompiler { typedef TestCompiler::IlGeneratorMethodDetails IlGeneratorMethodDetailsConnector; }
#endif

#include "ilgen/OMRIlGeneratorMethodDetails.hpp"

#include "infra/Annotations.hpp"
#include "env/IO.hpp"

class TR_InlineBlocks;
class TR_ResolvedMethod;
class TR_IlGenerator;
namespace TR { class Compilation; }
namespace TR { class IlVerifier; }
namespace TR { class ResolvedMethod; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

namespace TestCompiler
{

class ResolvedMethod;

class OMR_EXTENSIBLE IlGeneratorMethodDetails : public OMR::IlGeneratorMethodDetailsConnector
   {

public:

   IlGeneratorMethodDetails() :
      _method(NULL),
      _ilVerifier(NULL)
      { }

   IlGeneratorMethodDetails(TR::ResolvedMethod *method) :
      _method(method),
      _ilVerifier(NULL)
      { }

   IlGeneratorMethodDetails(TR_ResolvedMethod *method);

   TR::ResolvedMethod * getMethod() { return _method; }
   TR_ResolvedMethod * getResolvedMethod() { return (TR_ResolvedMethod *)_method; }

   bool sameAs(TR::IlGeneratorMethodDetails & other, TR_FrontEnd *fe);

   void print(TR_FrontEnd *fe, TR::FILE *file);

   virtual TR_IlGenerator *getIlGenerator(TR::ResolvedMethodSymbol *methodSymbol,
                                          TR_FrontEnd * fe,
                                          TR::Compilation *comp,
                                          TR::SymbolReferenceTable *symRefTab,
                                          bool forceClassLookahead,
                                          TR_InlineBlocks *blocksToInline);

   TR::IlVerifier * getIlVerifier() { return _ilVerifier; }
   void setIlVerifier(TR::IlVerifier * ilVerifier) { _ilVerifier = ilVerifier; }

protected:

   TR::ResolvedMethod * _method;
   TR::IlVerifier     * _ilVerifier;
   };

}

#endif
