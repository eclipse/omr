/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_REGISTER_ITERATOR_INCL
#define OMR_REGISTER_ITERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_ITERATOR_CONNECTOR
#define OMR_REGISTER_ITERATOR_CONNECTOR
namespace OMR { class RegisterIterator; }
namespace OMR { typedef OMR::RegisterIterator RegisterIteratorConnector; }
#endif

#include "env/TRMemory.hpp"  // for TR_Memory, etc
namespace TR { class Register; }

namespace OMR
{

/*
 * This interface must be implemented by platform-specific descendents,
 * using either List and ListIterator, or some cheaper alternatives.
 */
class OMR_EXTENSIBLE RegisterIterator
   {
   public:
   TR_ALLOC(TR_Memory::RegisterIterator)

   virtual TR::Register *getFirst() = 0;

   virtual TR::Register *getCurrent() = 0;

   virtual TR::Register *getNext() = 0;

   };

}

#endif
