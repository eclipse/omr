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

#ifndef OMR_DEBUG_ENV_INCL
#define OMR_DEBUG_ENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_DEBUG_ENV_CONNECTOR
#define OMR_DEBUG_ENV_CONNECTOR
namespace OMR { class DebugEnv; }
namespace OMR { typedef OMR::DebugEnv DebugEnvConnector; }
#endif


#include "infra/Annotations.hpp"
#include "env/jittypes.h"

namespace OMR
{

class OMR_EXTENSIBLE DebugEnv
   {
public:
   DebugEnv() {}

   void breakPoint();

   const char *extraAssertMessage(TR::Compilation *comp) { return ""; }

   int32_t hexAddressWidthInChars() { return _hexAddressWidthInChars; }

   // Deprecate in favour of 'pointerPrintfMaxLenInChars' ?
   //
   int32_t hexAddressFieldWidthInChars() { return _hexAddressFieldWidthInChars; }

   int32_t codeByteColumnWidth() { return _codeByteColumnWidth; }

   int32_t pointerPrintfMaxLenInChars() { return _hexAddressFieldWidthInChars; }

protected:

   int32_t _hexAddressWidthInChars;

   int32_t _hexAddressFieldWidthInChars;

   int32_t _codeByteColumnWidth;
   };

}

#endif
