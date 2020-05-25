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

#ifndef OMR_REGISTER_PAIR_INCL
#define OMR_REGISTER_PAIR_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_PAIR_CONNECTOR
#define OMR_REGISTER_PAIR_CONNECTOR
namespace OMR { class RegisterPair; }
namespace OMR { typedef OMR::RegisterPair RegisterPairConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class RegisterPair; }

template<typename QueueKind> class TR_Queue;

namespace OMR
{

class OMR_EXTENSIBLE RegisterPair : public TR::Register
   {
   protected:

   RegisterPair() : _lowOrder(NULL), _highOrder(NULL) {}
   RegisterPair(TR_RegisterKinds rk) : TR::Register(rk), _lowOrder(NULL), _highOrder(NULL) {}
   RegisterPair(TR::Register *lo, TR::Register *ho) : _lowOrder(lo), _highOrder(ho) {}

   public:

   virtual void block();
   virtual void unblock();
   virtual bool usesRegister(TR::Register* reg);

   virtual TR::Register     *getLowOrder();
   virtual TR::Register     *getHighOrder();

   TR::Register     *setLowOrder(TR::Register *lo, TR::CodeGenerator *codeGen);
   TR::Register     *setHighOrder(TR::Register *ho, TR::CodeGenerator *codeGen);

   virtual TR::Register     *getRegister();
   virtual TR::RegisterPair *getRegisterPair();


   private:
   TR::Register *_lowOrder;
   TR::Register *_highOrder;

   TR::RegisterPair *self();
   };

}

#endif
