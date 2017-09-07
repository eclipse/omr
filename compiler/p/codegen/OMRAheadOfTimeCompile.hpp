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

#ifndef OMR_POWER_AHEADOFTIMECOMPILE_INCL
#define OMR_POWER_AHEADOFTIMECOMPILE_INCL

#ifndef OMR_AHEADOFTIMECOMPILE_CONNECTOR
#define OMR_AHEADOFTIMECOMPILE_CONNECTOR
namespace OMR { namespace Power { class AheadOfTimeCompile; } }
namespace OMR { typedef OMR::Power::AheadOfTimeCompile AheadOfTimeCompileConnector; }
#endif // OMR_AHEADOFTIMECOMPILE_CONNECTOR

#include "compiler/codegen/OMRAheadOfTimeCompile.hpp"
#include "codegen/PPCAOTRelocation.hpp"

namespace OMR
{

namespace Power
{

class OMR_EXTENSIBLE AheadOfTimeCompile : public OMR::AheadOfTimeCompile
   {
   public:
   AheadOfTimeCompile(uint32_t *headerSizeMap, TR::Compilation * c)
       : OMR::AheadOfTimeCompile(headerSizeMap, c),
        _relocationList(getTypedAllocator<TR::PPCRelocation*>(c->allocator()))
     {
     }

   TR::list<TR::PPCRelocation*>& getRelocationList() {return _relocationList;}

   private:
   TR::list<TR::PPCRelocation*> _relocationList;
   };

} // namespace Power

} // namespace OMR

#endif
