/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef OMR_ARITHENV_INCL
#define OMR_ARITHENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_ARITHENV_CONNECTOR
#define OMR_ARITHENV_CONNECTOR
namespace OMR { class ArithEnv; }
namespace OMR { typedef OMR::ArithEnv ArithEnvConnector; }
#endif

#include <stdint.h>
#include <cmath>
#include "infra/Annotations.hpp"
#include "env/jittypes.h"

namespace TR { class ArithEnv; }

struct OMR_VMThread;
namespace TR { class Compilation; }

namespace OMR
{

class OMR_EXTENSIBLE ArithEnv
   {
public:

   TR::ArithEnv * self();

   template <typename T> inline T fpSquareRoot(T a) { return std::sqrt(a); }

   float floatAddFloat(float a, float b);
   float floatSubtractFloat(float a, float b);
   float floatMultiplyFloat(float a, float b);
   float floatDivideFloat(float a, float b);
   float floatRemainderFloat(float a, float b);
   float floatNegate(float a);
   int32_t floatComparefloat(float a, float b);
   double doubleAddDouble(double a, double b);
   double doubleSubtractDouble(double a, double b);
   double doubleMultiplyDouble(double a, double b);
   double doubleDivideDouble(double a, double b);
   double doubleRemainderDouble(double a, double b);
   double doubleNegate(double a);
   int32_t doubleComparedouble(double a, double b);
   double floatToDouble(float a);
   float doubleToFloat(double a);
   int64_t longRemainderLong(int64_t a, int64_t b);
   int64_t longDivideLong(int64_t a, int64_t b);
   int64_t longMultiplyLong(int64_t a, int64_t b);
   };

}

#endif
