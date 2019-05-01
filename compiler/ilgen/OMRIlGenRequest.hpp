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

#ifndef OMR_ILGENREQUEST_INCL
#define OMR_ILGENREQUEST_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ILGENREQUEST_CONNECTOR
#define OMR_ILGENREQUEST_CONNECTOR
namespace OMR { class IlGenRequest; }
namespace OMR { typedef OMR::IlGenRequest IlGenRequestConnector; }
#endif

#include "env/FilePointerDecl.hpp"
#include "infra/Annotations.hpp"

class TR_FrontEnd;
class TR_IlGenerator;
namespace TR { class Compilation; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

namespace OMR
{

/**
 * IlGenRequestBase defines the IlGenRequest API that common code can count on, although it's more documentation
 *   then enforcement.  A front end will typically extend this class (perhaps even with a hierarchy of classes)
 *   and then map its own concrete base class onto TR::IlGenRequest.
 *
 * Accessing ANY front-end specific API/data in common code is prohibited since other front end builds would fail.
 *
 */

class OMR_EXTENSIBLE IlGenRequest
   {

public:

   /// Create an ILGen object to execute this request
   ///
   virtual TR_IlGenerator *getIlGenerator(
         TR::ResolvedMethodSymbol *methodSymbol,
         TR_FrontEnd *fe,
         TR::Compilation *comp,
         TR::SymbolReferenceTable *symRefTab) = 0;

   virtual void print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix) = 0;

   TR::IlGeneratorMethodDetails & details() { return _methodDetails; }

protected:

   IlGenRequest(TR::IlGeneratorMethodDetails & methodDetails) :
      _methodDetails(methodDetails) { }

   TR::IlGeneratorMethodDetails & _methodDetails;
   };

}

#endif
