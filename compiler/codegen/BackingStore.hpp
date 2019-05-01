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

#ifndef BACKINGSTORE_INCL
#define BACKINGSTORE_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Flags.hpp"

namespace TR { class Symbol; }
namespace TR { class SymbolReferenceTable; }

class TR_BackingStore
   {

public:

   TR_ALLOC(TR_Memory::BackingStore)

   TR_BackingStore(TR::SymbolReferenceTable *symRefTab) :
      _flags(0),
      _maxSpillDepth(-1)
       {
       _symRef = new (TR::comp()->trHeapMemory()) TR::SymbolReference(symRefTab);
       }
   TR_BackingStore(TR::SymbolReferenceTable *symRefTab, TR::Symbol *s, int32_t o) :
      _flags(0),
      _maxSpillDepth(-1)
       {
       _symRef = new (TR::comp()->trHeapMemory()) TR::SymbolReference(symRefTab,s,o);
       }
   TR_BackingStore(TR::SymbolReference *symRef) :
      _symRef(symRef),
      _flags(0),
      _maxSpillDepth(-1) {}

   TR::SymbolReference *getSymbolReference() { return _symRef; }

   bool isEmpty()       { return _flags.testValue(OccupiedMask, 0); }
   void setIsEmpty()    { _flags.setValue(OccupiedMask, 0); }
   bool isOccupied()    { return _flags.testAny(OccupiedMask); }
   void setIsOccupied() { _flags.set(OccupiedMask); }

   bool firstHalfIsOccupied()     { return _flags.testAny(FirstHalfIsOccupied); }
   void setFirstHalfIsOccupied()  { _flags.set(FirstHalfIsOccupied); }
   void setFirstHalfIsEmpty()     { _flags.reset(FirstHalfIsOccupied); }
   bool secondHalfIsOccupied()    { return _flags.testAny(SecondHalfIsOccupied); }
   void setSecondHalfIsOccupied() { _flags.set(SecondHalfIsOccupied); }
   void setSecondHalfIsEmpty()    { _flags.reset(SecondHalfIsOccupied); }

   bool containsCollectedReference()          { return _flags.testAny(ContainsCollectedReference); }
   void setContainsCollectedReference(bool b) { _flags.set(ContainsCollectedReference, b); }

   int32_t setMaxSpillDepth(int32_t d)    { return _maxSpillDepth = d; }
   int32_t getMaxSpillDepth()             { return _maxSpillDepth; }

private:

   enum
      {
      OccupiedMask               = 0x03,
      FirstHalfIsOccupied        = 0x01,
      SecondHalfIsOccupied       = 0x02,

      ContainsCollectedReference = 0x04,
      };

   TR::SymbolReference *_symRef;

   int32_t  _maxSpillDepth;
   flags8_t _flags;

   };

#endif
