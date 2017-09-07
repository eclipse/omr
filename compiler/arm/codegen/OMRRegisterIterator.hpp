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

#ifndef OMR_ARM_REGISTER_ITERATOR_INCL
#define OMR_ARM_REGISTER_ITERATOR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_ITERATOR_CONNECTOR
#define OMR_REGISTER_ITERATOR_CONNECTOR
   namespace OMR { namespace ARM { class RegisterIterator; } }
   namespace OMR { typedef OMR::ARM::RegisterIterator RegisterIteratorConnector; }
#else
   #error OMR::ARM::RegisterIterator expected to be a primary connector, but a OMR connector is already defined
#endif

#include "compiler/codegen/OMRRegisterIterator.hpp"
namespace TR { class Machine; }

namespace OMR
{

namespace ARM
{

class OMR_EXTENSIBLE RegisterIterator : public OMR::RegisterIterator
   {
   public:
   RegisterIterator(TR::Machine *machine, TR_RegisterKinds kind);

   TR::Register *getFirst();
   TR::Register *getCurrent();
   TR::Register *getNext();

   private:
   TR::Machine *_machine;
   int32_t _firstRegIndex;
   int32_t _lastRegIndex;
   int32_t _cursor;
   };

}

}


#endif
