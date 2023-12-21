/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"


/**
   @class RealRegisterManager
   @brief An utility to manage virtual registers that mapped with predefined real registers

   The main use of this class is to aid allocating virtual registers that mapped with predefined real registers,
   it is usually used when large number of real-virtual register pairs is needed, i.e. in linkages.
   Note that only register types TR_GPR and TR_FPR are currently supported.
*/
class RealRegisterManager
   {
   public:
   /**
      @brief Create an utility to manage virtual registers that mapped with predefined real registers.

      @param cg        : the code generator.
   */
   RealRegisterManager(TR::CodeGenerator* cg) :
      _cg(cg),
      _numberOfRegistersInUse(0)
      {
      memset(_registers, 0x0, sizeof(_registers));
      }
   /**
      @brief Stop all virtual registers that this Manager holds.
   */
   ~RealRegisterManager()
      {
      // RAII: stop using all registers
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_registers[i] != NULL)
            {
            _cg->stopUsingRegister(_registers[i]);
            }
         }
      }
   /**
      @brief Find or allocate a virtual register which is bind to the given real register.

      @param reg: the real register.
   */
   TR::Register* use(TR::RealRegister::RegNum reg)
      {
      if (_registers[reg] == NULL)
         {
         bool isGPR = reg >= TR::RealRegister::FirstGPR && reg <= TR::RealRegister::LastGPR;
         _registers[reg] = _cg->allocateRegister(isGPR ? TR_GPR : TR_FPR);
         _numberOfRegistersInUse++;
         }
      return _registers[reg];
      }
   /**
      @brief Build the dependencies for each virtual-real register pair.
   */
   TR::RegisterDependencyConditions* buildRegisterDependencyConditions()
      {
      TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions(numberOfRegistersInUse(),
                                                                                    numberOfRegistersInUse(),
                                                                                    _cg);
      for (uint8_t i = (uint8_t)TR::RealRegister::NoReg; i < (uint8_t)TR::RealRegister::NumRegisters; i++)
         {
         if (_registers[i] != NULL)
            {
            deps->addPreCondition(_registers[i], (TR::RealRegister::RegNum)i, _cg);
            deps->addPostCondition(_registers[i], (TR::RealRegister::RegNum)i, _cg);
            }
         }
      return deps;
      }
   /**
      @brief Report number of registers that this Manager uses.
   */
   inline uint8_t numberOfRegistersInUse() const
      {
      return _numberOfRegistersInUse;
      }

   private:
   uint8_t            _numberOfRegistersInUse;
   TR::Register*      _registers[TR::RealRegister::NumRegisters];
   TR::CodeGenerator* _cg;
   };
