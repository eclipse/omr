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

#ifndef TR_Z_ZOSSYSTEMLINKAGE_BASE_INCL
#define TR_Z_ZOSSYSTEMLINKAGE_BASE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TR_ZOSSYSTEMLINKAGE_BASE_CONNECTOR
#define TR_ZOSSYSTEMLINKAGE_BASE_CONNECTOR
namespace TR { namespace Z {class ZOSBaseSystemLinkage; } }
namespace TR { typedef TR::Z::ZOSBaseSystemLinkage ZOSBaseSystemLinkageConnector; }
#endif

#include <stdint.h>
#include "codegen/Linkage.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/SystemLinkage.hpp"

class TR_zOSGlobalCompilationInfo;
namespace TR { class CodeGenerator; }

////////////////////////////////////////////////////////////////////////////////
//  TR::Z::ZOSBaseSystemLinkage Definition
//
//  Intention for common zOS behavior across multiple zOS system linkages
////////////////////////////////////////////////////////////////////////////////

namespace TR
{

namespace Z
{

class ZOSBaseSystemLinkage : public TR::SystemLinkage
   {
protected:
   TR::RealRegister::RegNum _CAAPointerRegister;
   TR::RealRegister::RegNum _parentDSAPointerRegister;

   TR_zOSGlobalCompilationInfo* _globalCompilationInfo;

public:
   enum
      {
      CEECAAPLILWS    = 640,
      CEECAAAESS      = 788,
      CEECAAOGETS     = 796,
      CEECAAAGTS      = 944,

      CEEDSAType1Size = 0x80,
      };

   ZOSBaseSystemLinkage(TR::CodeGenerator * cg, TR_S390LinkageConventions elc=TR_S390LinkageDefault, TR_LinkageConventions lc=TR_System)
      : TR::SystemLinkage(cg, elc,lc)
      {
      }

   virtual TR::RealRegister::RegNum setCAAPointerRegister (TR::RealRegister::RegNum r)         { return _CAAPointerRegister = r; }
   virtual TR::RealRegister::RegNum getCAAPointerRegister()         { return _CAAPointerRegister; }
   virtual TR::RealRegister *getCAAPointerRealRegister() {return getRealRegister(_CAAPointerRegister);}

   virtual TR::RealRegister::RegNum setParentDSAPointerRegister (TR::RealRegister::RegNum r)         { return _parentDSAPointerRegister = r; }
   virtual TR::RealRegister::RegNum getParentDSAPointerRegister()         { return _parentDSAPointerRegister; }
   virtual TR::RealRegister *getParentDSAPointerRealRegister() {return getRealRegister(_parentDSAPointerRegister);}

   // === Fastlink specific
   virtual bool isAggregateReturnedInIntRegisters(int32_t aggregateLenth)   { return isFastLinkLinkageType(); }
   virtual bool isAggregateReturnedInIntRegistersAndMemory(int32_t aggregateLenth);

   };

}
}
#endif
