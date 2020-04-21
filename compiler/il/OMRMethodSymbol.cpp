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

#include "compile/Method.hpp"
#include "env/TRMemory.hpp"
#include "il/MethodSymbol.hpp"
#include "infra/Flags.hpp"

OMR::MethodSymbol::MethodSymbol(TR_LinkageConventions lc, TR::Method *m) :
   TR::Symbol(),
   _methodAddress(NULL),
   _method(m),
   _linkageConvention(lc)
   {
   _flags.setValue(KindMask, IsMethod);
   }

bool
OMR::MethodSymbol::firstArgumentIsReceiver()
   {
   if (self()->isSpecial() || self()->isVirtual() || self()->isInterface() || self()->isComputedVirtual())
      return true;

   return false;
   }


TR::MethodSymbol *
OMR::MethodSymbol::self()
   {
   return static_cast<TR::MethodSymbol *>(this);
   }

bool
OMR::MethodSymbol::isComputed()
   {
   return self()->isComputedStatic() || self()->isComputedVirtual();
   }

/**
 * Method Symbol Factory.
 */
template <typename AllocatorType>
TR::MethodSymbol * OMR::MethodSymbol::create(AllocatorType t, TR_LinkageConventions lc, TR::Method *m)
   {
   return new (t) TR::MethodSymbol(lc, m);
   }

//Explicit instantiations
template TR::MethodSymbol * OMR::MethodSymbol::create(TR_HeapMemory t,          TR_LinkageConventions lc, TR::Method *m);
template TR::MethodSymbol * OMR::MethodSymbol::create(TR_StackMemory t,         TR_LinkageConventions lc, TR::Method *m);
template TR::MethodSymbol * OMR::MethodSymbol::create(PERSISTENT_NEW_DECLARE t, TR_LinkageConventions lc, TR::Method *m);
