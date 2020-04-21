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

#include "env/KnownObjectTable.hpp"
#include "env/FrontEnd.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/VMAccessCriticalSection.hpp"
#endif
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"


OMR::KnownObjectTable::KnownObjectTable(TR::Compilation *comp) :
      _comp(comp),
      _fe(comp->fe()),
      _arrayWithConstantElements(NULL)
   {}


TR::KnownObjectTable *
OMR::KnownObjectTable::self()
   {
   return static_cast<TR::KnownObjectTable *>(this);
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getEndIndex()
   {
   TR_UNIMPLEMENTED();
   return -1;
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getOrCreateIndex(uintptr_t objectPointer)
   {
   TR_UNIMPLEMENTED();
   return -1;
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getOrCreateIndex(uintptr_t objectPointer, bool isArrayWithConstantElements)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(self()->comp()), "Getting KnownObjectTable index requires VM access");
   TR::KnownObjectTable::Index index = self()->getOrCreateIndex(objectPointer);
   if (isArrayWithConstantElements)
      {
      self()->addArrayWithConstantElements(index);
      }
   return index;
   }

void
OMR::KnownObjectTable::addArrayWithConstantElements(Index index)
   {
   TR_ASSERT(index != UNKNOWN && !self()->isNull(index), "Calling addArrayWithConstantElements for unknown object or null!");
   if (!_arrayWithConstantElements)
      _arrayWithConstantElements = new (self()->comp()->trHeapMemory()) TR_BitVector(self()->getEndIndex(), self()->comp()->trMemory(), heapAlloc, growable);
   _arrayWithConstantElements->set(index);
   }

bool
OMR::KnownObjectTable::isArrayWithConstantElements(Index index)
   {
   TR_ASSERT(index != UNKNOWN && 0 <= index && index < self()->getEndIndex(), "isArrayWithConstantElements(%d): index must be in range 0..%d", index, self()->getEndIndex());
   if (_arrayWithConstantElements
       && _arrayWithConstantElements->isSet(index))
      return true;
   return false;
   }

uintptr_t *
OMR::KnownObjectTable::getPointerLocation(Index index)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

bool
OMR::KnownObjectTable::isNull(Index index)
   {
   TR_UNIMPLEMENTED();
   return false;
   }

void
OMR::KnownObjectTable::dumpTo(TR::FILE *file, TR::Compilation *comp)
   {
   TR_UNIMPLEMENTED();
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getOrCreateIndexAt(uintptr_t *objectReferenceLocation)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::VMAccessCriticalSection getIndexCriticalSection(self()->comp());
#endif
   uintptr_t objectPointer = *objectReferenceLocation; // Note: object references held as uintptr_t must never be compressed refs
   Index result = self()->getOrCreateIndex(objectPointer);
   return result;
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getOrCreateIndexAt(uintptr_t *objectReferenceLocation, bool isArrayWithConstantElements)
   {
   Index result = self()->getOrCreateIndexAt(objectReferenceLocation);
   if (isArrayWithConstantElements)
      self()->addArrayWithConstantElements(result);
   return result;
   }

TR::KnownObjectTable::Index
OMR::KnownObjectTable::getExistingIndexAt(uintptr_t *objectReferenceLocation)
   {
   Index result = UNKNOWN;
#ifdef J9_PROJECT_SPECIFIC
   TR::VMAccessCriticalSection getIndexCriticalSection(self()->comp());
#endif
   uintptr_t objectPointer = *objectReferenceLocation;
   for (Index i = 0; i < self()->getEndIndex() && (result == UNKNOWN); i++)
      {
      if (self()->getPointer(i) == objectPointer)
         {
         result = i;
         break;
         }
      }
   return result;
   }

uintptr_t
OMR::KnownObjectTable::getPointer(Index index)
   {
   TR_ASSERT(TR::Compiler->vm.hasAccess(self()->comp()), "Getting a heap object pointer requires VM access");
   if (self()->isNull(index))
      return 0; // Assumes host and target representations of null match each other
   else
      return *self()->getPointerLocation(index);
   }

void
OMR::KnownObjectTable::updateKnownObjectTableAtServer(Index index, uintptr_t *objectReferenceLocation)
   {
   TR_UNIMPLEMENTED();
   }
