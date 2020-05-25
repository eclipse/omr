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

#ifndef TEST_METHOD_INCL
#define TEST_METHOD_INCL

#include <string.h>

// Choose the OMR base version directly.  This is only temporary
// while the Method class is being made extensible.
//
#include "compiler/compile/Method.hpp"


// quick and dirty implementation to get up and running
// needs major overhaul

namespace TestCompiler
{

class Method : public TR::Method
   {
   public:
   TR_ALLOC(TR_Memory::Method);

   Method() : TR::Method(TR::Method::Test) {}

   // FIXME: need to provide real code for this group
   virtual uint16_t              classNameLength() { return strlen(classNameChars()); }
   virtual uint16_t              nameLength()      { return strlen(nameChars()); }
   virtual uint16_t              signatureLength() { return strlen(signatureChars()); }
   virtual char                * nameChars()       { return "Method"; }
   virtual char                * classNameChars()  { return ""; }
   virtual char                * signatureChars()  { return "()V"; }

   virtual bool                  isConstructor()   { return false; }
   virtual bool                  isFinalInObject() { return false; }
   };

} // namespace TestCompiler

#endif // !defined(TEST_METHOD_INCL)
