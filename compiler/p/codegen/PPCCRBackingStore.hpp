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

#ifndef PPCCRBACKINGSTORE_INCL
#define PPCCRBACKINGSTORE_INCL

#include <stdint.h>
#include "codegen/BackingStore.hpp"
#include "compile/Compilation.hpp"
#include "il/SymbolReference.hpp"

class TR_PPCCRBackingStore;

// Pseudo-safe downcast function, used exclusively for cr spill/restore
//
inline TR_PPCCRBackingStore *toPPCCRBackingStore(TR_BackingStore *r)
   {
   return (TR_PPCCRBackingStore *)r;
   }

class TR_PPCCRBackingStore: public TR_BackingStore
   {
   private:

   TR_BackingStore *original;
   uint8_t ccrFieldIndex;

   public:

   TR_PPCCRBackingStore();

   TR_PPCCRBackingStore(TR::Compilation * comp, TR_BackingStore *orig)
     : TR_BackingStore(comp->getSymRefTab(), orig->getSymbolReference()->getSymbol(), orig->getSymbolReference()->getOffset()), original(orig)
      {}

   uint8_t getCcrFieldIndex() {return ccrFieldIndex;}
   uint8_t setCcrFieldIndex(uint8_t cfi) {return (ccrFieldIndex=cfi);}
   TR_BackingStore *getOriginal() {return original;}
   TR_BackingStore *setOriginal(TR_BackingStore *o) {return (original = o);}
   };

#endif
