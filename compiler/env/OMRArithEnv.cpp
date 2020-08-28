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

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "env/ArithEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/jittypes.h"
#include "infra/Assert.hpp"


TR::ArithEnv *
OMR::ArithEnv::self()
   {
   return static_cast<TR::ArithEnv *>(this);
   }

float
OMR::ArithEnv::floatAddFloat(float a, float b)
   {
   return a+b;
   }

float
OMR::ArithEnv::floatSubtractFloat(float a, float b)
   {
   return a-b;
   }

float
OMR::ArithEnv::floatMultiplyFloat(float a, float b)
   {
   return a * b;
   }

float
OMR::ArithEnv::floatDivideFloat(float a, float b)
   {
   return a / b;
   }

float
OMR::ArithEnv::floatRemainderFloat(float a, float b)
   {

   // C99 IEEE remainder is not available in older MSVC versions

#if defined (_MSC_VER)
   return 0;
#else
   return remainderf(a, b);
#endif

   }

float
OMR::ArithEnv::floatNegate(float a)
   {
   float c;
   c = -a;
   return c;
   }

int32_t
OMR::ArithEnv::floatComparefloat(float a, float b)
   {
   if (a > b)
      {
      return 1; 
      }
   else if (a < b)
      {
      return -1; 
      }
   else
      {
      return 0;
      }
   }

double
OMR::ArithEnv::doubleAddDouble(double a, double b)
   {
   return a + b;
   }

double
OMR::ArithEnv::doubleSubtractDouble(double a, double b)
   {
   return a - b;
   }

double
OMR::ArithEnv::doubleMultiplyDouble(double a, double b)
   {
   return a * b;
   }

double
OMR::ArithEnv::doubleDivideDouble(double a, double b)
   {
   return a / b;
   }

double
OMR::ArithEnv::doubleRemainderDouble(double a, double b)
   {
#if defined (_MSC_VER)
   return 0;
#else
   return remainder(a, b);
#endif
   }

double
OMR::ArithEnv::doubleNegate(double a)
   {
   double c;
   c = -a;
   return c;
   }

int32_t
OMR::ArithEnv::doubleComparedouble(double a, double b)
   {
   if (a > b)
      {
      return 1; 
      }
   else if (a < b)
      {
      return -1; 
      }
   else
      {
      return 0;
      }
   }

double
OMR::ArithEnv::floatToDouble(float a)
   {
   return (double) a;
   }

float
OMR::ArithEnv::doubleToFloat(double a)
   {
   return (float) a;
   }

int64_t
OMR::ArithEnv::longRemainderLong(int64_t a, int64_t b)
   {
   return a % b;
   }

int64_t
OMR::ArithEnv::longDivideLong(int64_t a, int64_t b)
   {
   return a / b;
   }

int64_t
OMR::ArithEnv::longMultiplyLong(int64_t a, int64_t b)
   {
   return a * b;
   }
