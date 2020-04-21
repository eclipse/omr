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

#ifndef OMR_RV_ENVIRONMENT_INCL
#define OMR_RV_ENVIRONMENT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ENVIRONMENT_CONNECTOR
#define OMR_ENVIRONMENT_CONNECTOR
namespace OMR { namespace RV { class Environment; } }
namespace OMR { typedef OMR::RV::Environment EnvironmentConnector; }
#else
#error OMR::RV::Environment expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/env/OMREnvironment.hpp"


namespace OMR
{

namespace RV
{

class Environment : public OMR::Environment
   {
public:

   Environment() :
      OMR::Environment()
      {}

   Environment(TR::MajorOperatingSystem o, TR::Bitness b) :
      OMR::Environment(o, b)
      {}
      
   };

}

}
#endif //OMR_RV_ENVIRONMENT_INCL
