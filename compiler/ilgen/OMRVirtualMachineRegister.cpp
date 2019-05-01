/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#include "infra/Assert.hpp"
#include "ilgen/VirtualMachineRegister.hpp"
#include "compile/Compilation.hpp"

OMR::VirtualMachineRegister::VirtualMachineRegister(
      TR::IlBuilder *b,
      const char * const localName,
      TR::IlType * pointerToRegisterType,
      uint32_t adjustByStep,
      TR::IlValue * addressOfRegister)
   : TR::VirtualMachineState(),
   _localName(localName),
   _addressOfRegister(addressOfRegister),
   _pointerToRegisterType(pointerToRegisterType),
   _adjustByStep(adjustByStep)
   {
   TR_ASSERT(pointerToRegisterType->isPointer(),
         "VirtualMachineRegister for %s requires pointerToRegisterType (%s) to be a pointer",
         localName, pointerToRegisterType->getName());

   _integerTypeForAdjustments = pointerToRegisterType->baseType();
   if (_integerTypeForAdjustments->isPointer())
      {
      _integerTypeForAdjustments = _integerTypeForAdjustments->baseType();
      if (_integerTypeForAdjustments->isPointer())
         {
         _integerTypeForAdjustments = b->typeDictionary()->getWord();
         }
      TR_ASSERT(adjustByStep == _integerTypeForAdjustments->getSize(),
            "VirtualMachineRegister for %s adjustByStep (%u) != size represented by pointerToRegisterType (%u)",
            localName, _adjustByStep, _integerTypeForAdjustments->getSize());
      _isAdjustable = true;
      }
   else
      {
      TR_ASSERT(adjustByStep == 0, "VirtualMachineRegister for %s is representing a primitive type but adjustByStep (%u) != 0", localName, _adjustByStep);
      _isAdjustable = false;
      }
   Reload(b);
   }

void
OMR::VirtualMachineRegister::adjust(TR::IlBuilder *b, TR::IlValue *rawAmount)
   {
   TR_ASSERT(_isAdjustable, "VirtualMachineRegister can not be adjusted as it is simulating a primitive type");
   b->Store(_localName,
   b->   Add(
   b->      Load(_localName),
            rawAmount));
   }

TR::VirtualMachineState *
OMR::VirtualMachineRegister::MakeCopy()
   {
   return new (TR::comp()->trMemory()->trHeapMemory()) TR::VirtualMachineRegister(_localName,
                                                                                  _pointerToRegisterType,
                                                                                  _adjustByStep,
                                                                                  _addressOfRegister);
   }

void *
OMR::VirtualMachineRegister::client()
   {
   if (_client == NULL && _clientAllocator != NULL)
      _client = _clientAllocator(static_cast<TR::VirtualMachineRegister *>(this));
   return _client;
   }

ClientAllocator OMR::VirtualMachineRegister::_clientAllocator = NULL;
ClientAllocator OMR::VirtualMachineRegister::_getImpl = NULL;
