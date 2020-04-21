/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef OMR_RV_REGISTER_INCL
#define OMR_RV_REGISTER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_REGISTER_CONNECTOR
#define OMR_REGISTER_CONNECTOR
   namespace OMR { namespace RV { class Register; } }
   namespace OMR { typedef OMR::RV::Register RegisterConnector; }
#else
   #error OMR::RV::Register expected to be a primary connector, but a OMR connector is already defined
#endif


#include "compiler/codegen/OMRRegister.hpp"

class TR_LiveRegisterInfo;

namespace OMR
{

namespace RV
{

class OMR_EXTENSIBLE Register: public OMR::Register
   {
   protected:

   Register(uint32_t f=0): OMR::Register(f) {_liveRegisterInfo._liveRegister = NULL;}
   Register(TR_RegisterKinds rk): OMR::Register(rk)  {_liveRegisterInfo._liveRegister = NULL;}
   Register(TR_RegisterKinds rk, uint16_t ar): OMR::Register(rk, ar) {_liveRegisterInfo._liveRegister = NULL;}


   public:
   /*
    * Getter/setters
    */
   TR_LiveRegisterInfo *getLiveRegisterInfo()                       {return _liveRegisterInfo._liveRegister;}
   TR_LiveRegisterInfo *setLiveRegisterInfo(TR_LiveRegisterInfo *p) {return (_liveRegisterInfo._liveRegister = p);}

   uint64_t getInterference()           {return _liveRegisterInfo._interference;}
   uint64_t setInterference(uint64_t i) {return (_liveRegisterInfo._interference = i);}

   private:

   union
      {
      TR_LiveRegisterInfo *_liveRegister; // Live register entry representing this register
      uint32_t             _interference; // Real registers that interfere with this register
      } _liveRegisterInfo;


   };

}

}


#endif /* OMR_RV_REGISTER_INCL */
