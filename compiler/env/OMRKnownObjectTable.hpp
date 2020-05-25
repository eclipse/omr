/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_KNOWN_OBJECT_TABLE_INCL
#define OMR_KNOWN_OBJECT_TABLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_KNOWN_OBJECT_TABLE_CONNECTOR
#define OMR_KNOWN_OBJECT_TABLE_CONNECTOR
namespace OMR { class KnownObjectTable; }
namespace OMR { typedef OMR::KnownObjectTable KnownObjectTableConnector; }
#endif


#include <stdint.h>
#include "env/FilePointerDecl.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"
#include "infra/BitVector.hpp"

class TR_FrontEnd;
namespace TR { class Compilation; }
namespace TR { class KnownObjectTable; }

namespace OMR
{

 /**
  * Table of known objects.
  *
  * These are cases where language properties allows the compiler to know
  * statically not just the type of the object, but the identity of the object.
  */
class OMR_EXTENSIBLE KnownObjectTable
   {
   TR_FrontEnd *_fe;

protected:

   KnownObjectTable(TR::Compilation *comp);
   TR::Compilation *_comp;
   TR_BitVector* _arrayWithConstantElements;

public:

   TR::KnownObjectTable * self();

   typedef int32_t Index;
   static const Index UNKNOWN = -1;

   TR_FrontEnd *fe() const { return _fe; }
   void setFe(TR_FrontEnd *fe) { _fe = fe; }

   TR::Compilation *comp() const { return _comp; }
   void setComp(TR::Compilation *comp) { _comp = comp; }

   Index getEndIndex();                      // Highest index assigned so far + 1
   Index getOrCreateIndex(uintptr_t objectPointer); // Must hold vm access for this
   Index getOrCreateIndex(uintptr_t objectPointer, bool isArrayWithConstantElements); // Must hold vm access for this
   uintptr_t *getPointerLocation(Index index);
   bool isNull(Index index);

   void dumpTo(TR::FILE *file, TR::Compilation *comp);

   // Handy wrappers

   // API for checking if an known object is an array with immutable elements
   bool isArrayWithConstantElements(Index index);

   Index getOrCreateIndexAt(uintptr_t *objectReferenceLocation);
   Index getOrCreateIndexAt(uintptr_t *objectReferenceLocation, bool isArrayWithConstantElements);
   Index getExistingIndexAt(uintptr_t *objectReferenceLocation);

   uintptr_t getPointer(Index index);

   void updateKnownObjectTableAtServer(Index index, uintptr_t *objectReferenceLocation);

protected:
   void addArrayWithConstantElements(Index index);

   };
}

#endif
