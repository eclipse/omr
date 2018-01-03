/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#include "ilgen/VirtualMachineRegister.hpp"
#include "ilgen/IlBuilder.hpp"
#include "ilgen/IlType.hpp"
#include "ilgen/IlTypeImpl.hpp"  // must move out

OMR::VirtualMachineRegister::VirtualMachineRegister(TR::IlBuilder *b,
                                                    const char * const localName,
                                                    TR::IlType * pointerToRegisterType,
                                                    uint32_t adjustByStep,
                                                    TR::IlValue * addressOfRegister)
   : OMR::VirtualMachineState(),
   _localName(localName),
   _addressOfRegister(addressOfRegister),
   _pointerToRegisterType(pointerToRegisterType),
   _elementType(pointerToRegisterType->impl()->baseType()->baseType()),
   _adjustByStep(adjustByStep)
   {
   Reload(b);
   }
