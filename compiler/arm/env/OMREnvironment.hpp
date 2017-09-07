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

#ifndef OMR_ARM_ENVIRONMENT_INCL
#define OMR_ARM_ENVIRONMENT_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ENVIRONMENT_CONNECTOR
#define OMR_ENVIRONMENT_CONNECTOR
namespace OMR { namespace ARM { class Environment; } }
namespace OMR { typedef OMR::ARM::Environment EnvironmentConnector; }
#else
#error OMR::ARM::Environment expected to be a primary connector, but an OMR connector is already defined
#endif

#include "compiler/env/OMREnvironment.hpp"


namespace OMR
{

namespace ARM
{

class Environment : public OMR::Environment
   {
public:

   Environment() :
      OMR::Environment(),
         _isEABI(false)
      {}

   Environment(TR::MajorOperatingSystem o, TR::Bitness b) :
      OMR::Environment(o, b),
         _isEABI(false)
      {}

   bool isEABI() { return _isEABI; }
   void setEABI(bool b) { _isEABI = b; }

private:

   bool _isEABI;
   };

}

}


#endif
