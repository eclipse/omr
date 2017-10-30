/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"


// An utility to manage real registers and their corresponding virtual registers
class RealRegisterManager
   {
   public:
   RealRegisterManager(TR::CodeGenerator* cg) :
      _cg(cg),
      _NumberOfRegistersInUse(0)
      {
      memset(_Registers, 0x0, sizeof(_Registers));
      }
   ~RealRegisterManager()
      {
      // RAII: stop using all registers
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_Registers[i] != NULL)
            {
            _cg->stopUsingRegister(_Registers[i]);
            }
         }
      }
   // Find or allocate a virtual register which is bind to the given real register
   TR::Register* Use(TR::RealRegister::RegNum RealRegister)
      {
      if (_Registers[RealRegister] == NULL)
         {
         bool isGPR = RealRegister >= TR::RealRegister::FirstGPR && RealRegister <= TR::RealRegister::LastGPR;
         _Registers[RealRegister] = _cg->allocateRegister(isGPR ? TR_GPR : TR_FPR);
         _NumberOfRegistersInUse++;
         }
      return _Registers[RealRegister];
      }
   // Build the dependencies for each virtual-real register pair
   TR::RegisterDependencyConditions* BuildRegisterDependencyConditions()
      {
      TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions(NumberOfRegistersInUse() + 1, // For VMThread
                                                                                    NumberOfRegistersInUse() + 1, // For VMThread
                                                                                    _cg);
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_Registers[i] != NULL)
            {
            deps->addPreCondition(_Registers[i], (TR::RealRegister::RegNum)i, _cg);
            deps->addPostCondition(_Registers[i], (TR::RealRegister::RegNum)i, _cg);
            }
         }
      TR::Register* vmThread = _cg->getVMThreadRegister();
      deps->addPreCondition(vmThread, (TR::RealRegister::RegNum)vmThread->getAssociation(), _cg);
      deps->addPostCondition(vmThread, (TR::RealRegister::RegNum)vmThread->getAssociation(), _cg);
      return deps;
      }
   inline uint8_t NumberOfRegistersInUse() const
      {
      return _NumberOfRegistersInUse;
      }

   private:
   uint8_t            _NumberOfRegistersInUse;
   TR::Register*      _Registers[TR::RealRegister::NumRegisters];
   TR::CodeGenerator* _cg;
   };
