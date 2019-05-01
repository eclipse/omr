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

#ifndef STACK_INCL
#define STACK_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Array.hpp"

// TR_Stack use TR_Array as a stack
//
template<class T> class TR_Stack : public TR_Array<T>
   {
public:
   TR_Stack(TR_Memory * m, uint32_t initialSize = 8, bool zeroInit = false, TR_AllocationKind allocKind = heapAlloc)
      : TR_Array<T>(m, initialSize, zeroInit, allocKind) { }
   TR_Stack(const TR_Stack<T>& other) : TR_Array<T>(other) { }
   TR_Stack<T> & operator=(const TR_Stack<T>& other) { (TR_Array<T>&)*this = (TR_Array<T>&)other; return *this; }

   int32_t  topIndex() { return TR_Array<T>::lastIndex(); }
   T &      top()      { return TR_Array<T>::element(topIndex()); }
   void     push(T t)  { TR_Array<T>::add(t); }
   T        pop()      { T t = top(); TR_Array<T>::remove(topIndex()); return t; }
   void     swap()     { T t = top(); top() = TR_Array<T>::element(topIndex() - 1); TR_Array<T>::element(topIndex() - 1) = t; }
   bool     isEmpty()  { return TR_Array<T>::size() == 0; }
   void     clear()    { TR_Array<T>::setSize(0); }

   // FIFO behaviour
   T &      bottom()   { return TR_Array<T>::element(0); }
   T        drop()     { T t = bottom(); TR_Array<T>::remove(0); return t; }

   };

#endif
