/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#ifndef LINKEDLIST_INCL
#define LINKEDLIST_INCL

#include "ilgen/MethodBuilder.hpp"

namespace TR { class TypeDictionary; }

typedef struct Element
   {
   struct Element *next;
   int16_t key;
   int32_t val;
   } Element;

typedef int32_t (LinkedListFunctionType)(Element *, int16_t);

class LinkedListMethod : public TR::MethodBuilder
   {
   public:
   LinkedListMethod(TR::TypeDictionary *);
   virtual bool buildIL();
   };

#endif // !defined(LINKEDLIST_INCL)
