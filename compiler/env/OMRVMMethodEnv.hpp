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

#ifndef OMR_VMMETHODENV_INCL
#define OMR_VMMETHODENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_VMMETHODENV_CONNECTOR
#define OMR_VMMETHODENV_CONNECTOR
namespace OMR { class VMMethodEnv; }
namespace OMR { typedef OMR::VMMethodEnv VMMethodEnvConnector; }
#endif

#include <stdint.h>
#include "infra/Annotations.hpp"
#include "env/jittypes.h"


namespace OMR
{

class OMR_EXTENSIBLE VMMethodEnv
   {
public:

   /**
    * \brief  Does this method contain any backward branches?
    * \return true if there are backward branches, false if there aren't or unknown
    */
   bool hasBackwardBranches(TR_OpaqueMethodBlock *method) { return false; }

   /**
    * \brief  Is this method compiled?
    * \return true if compiled, false if not or unknown
    */ 
   bool isCompiledMethod(TR_OpaqueMethodBlock *method) { return false; }

   /**
    * \brief  Ask for the start PC of the provided method
    * \return the start PC, or 0 if unknown or not compiled
    */
   uintptr_t startPC(TR_OpaqueMethodBlock *method) { return 0; }
   };

}

#endif
