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

#ifndef OMR_ILGENERATOR_METHOD_DETAILS_INCL
#define OMR_ILGENERATOR_METHOD_DETAILS_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ILGENERATORMETHODDETAILS_CONNECTOR
#define OMR_ILGENERATORMETHODDETAILS_CONNECTOR
namespace OMR { class IlGeneratorMethodDetails; }
namespace OMR { typedef OMR::IlGeneratorMethodDetails IlGeneratorMethodDetailsConnector; }
#endif

#include <stddef.h>
#include "env/FilePointerDecl.hpp"
#include "infra/Annotations.hpp"

class TR_FrontEnd;
class TR_ResolvedMethod;
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class IlVerifier; }

namespace OMR
{

/**
 * Accessing ANY language-specific API/data in common code is prohibited.
 *
 * IlGeneratorMethodDetails defines the IlGenRequest API.
 */
class OMR_EXTENSIBLE IlGeneratorMethodDetails
   {

public:

   inline TR::IlGeneratorMethodDetails *self();
   inline const TR::IlGeneratorMethodDetails *self() const;

   virtual bool isMethodInProgress() const { return false; }
   bool supportsInvalidation() { return false; }
   bool sameAs(TR::IlGeneratorMethodDetails & other) { return false; }
   void print(TR_FrontEnd *fe, TR::FILE *file) { }

   inline static TR::IlGeneratorMethodDetails & create(TR::IlGeneratorMethodDetails & target, TR_ResolvedMethod *method);

   TR::IlVerifier * getIlVerifier()                     { return _ilVerifier; }
   void setIlVerifier(TR::IlVerifier * ilVerifier)      { _ilVerifier = ilVerifier; }

protected:
   IlGeneratorMethodDetails() : _ilVerifier(NULL) { }
   virtual ~IlGeneratorMethodDetails() {}

   void *operator new(size_t size, TR::IlGeneratorMethodDetails *p){ return (void*) p; }
   void *operator new(size_t size, TR::IlGeneratorMethodDetails &p){ return (void*)&p; }
   void operator delete(void *pMem, TR::IlGeneratorMethodDetails *p) {};
   void operator delete(void *pMem, TR::IlGeneratorMethodDetails &p) {};
   void operator delete(void *pMem, size_t size) { ::operator delete(pMem); };

   TR::IlVerifier     * _ilVerifier;
   };

}

#endif
