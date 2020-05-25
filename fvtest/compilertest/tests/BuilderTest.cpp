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

#include <limits.h>
#include <stdio.h>
#include "gtest/gtest.h"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/TypeDictionary.hpp"
#include "tests/BuilderTest.hpp"
#include "OpCodesTest.hpp"

namespace TestCompiler
{

RecursiveFibFunctionType *BuilderTest::_recursiveFibMethod = 0;
IterativeFibFunctionType *BuilderTest::_iterativeFibMethod = 0;
DoWhileFibFunctionType *BuilderTest::_doWhileFibMethod = 0;
DoWhileFunctionType *BuilderTest::_doWhileWithBreakMethod = 0;
DoWhileFunctionType *BuilderTest::_doWhileWithContinueMethod = 0;
DoWhileFunctionType *BuilderTest::_doWhileWithBreakAndContinueMethod = 0;
WhileDoFibFunctionType *BuilderTest::_whileDoFibMethod = 0;
WhileDoFunctionType *BuilderTest::_whileDoWithBreakMethod = 0;
WhileDoFunctionType *BuilderTest::_whileDoWithContinueMethod = 0;
WhileDoFunctionType *BuilderTest::_whileDoWithBreakAndContinueMethod = 0;
BasicForLoopFunctionType *BuilderTest::_basicForLoopUpMethod = 0;
BasicForLoopFunctionType *BuilderTest::_basicForLoopDownMethod = 0;
signatureCharI_I_testMethodType *BuilderTest::_shootoutNestedLoopMethod = 0;
IfFunctionType *BuilderTest::_absDiffIfThenElseMethod = 0;
IfFunctionType *BuilderTest::_maxIfThenMethod = 0;
IfLongFunctionType *BuilderTest::_subIfFalseThenMethod = 0;
IfFunctionType *BuilderTest::_ifThenElseLoopMethod = 0;
LoopIfThenElseFunctionType *BuilderTest::_forLoopUpIfThenElseMethod = 0;
LoopIfThenElseFunctionType *BuilderTest::_whileDoIfThenElseMethod = 0;
LoopIfThenElseFunctionType *BuilderTest::_doWhileIfThenElseMethod = 0;
ForLoopContinueFunctionType *BuilderTest::_forLoopContinueMethod = 0;
ForLoopBreakFunctionType *BuilderTest::_forLoopBreakMethod = 0;
ForLoopBreakAndContinueFunctionType *BuilderTest::_forLoopBreakAndContinueMethod = 0;
bool BuilderTest::_traceEnabled = false;

void
BuilderTest::compileTestMethods()
   {
   int32_t rc = 0;
   uint8_t *entry=0;

   TR::TypeDictionary types;

   IterativeFibonnaciMethod iterFibMethodBuilder(&types, this);
   rc = compileMethodBuilder(&iterFibMethodBuilder, &entry);
   _iterativeFibMethod = (IterativeFibFunctionType *) entry;

#if (defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)) || defined(TR_TARGET_S390)
   RecursiveFibonnaciMethod recFibMethodBuilder(&types, this);
   rc = compileMethodBuilder(&recFibMethodBuilder, &entry);
   _recursiveFibMethod = (RecursiveFibFunctionType *) entry;
#endif
   }

void
BuilderTest::compileControlFlowTestMethods()
   {
   int32_t rc = 0;
   uint8_t *entry=0;

   TR::TypeDictionary types;

   BasicForLoopUpMethod basicForLoopUpMethodBuilder(&types, this);
   rc = compileMethodBuilder(&basicForLoopUpMethodBuilder, &entry);
   _basicForLoopUpMethod = (BasicForLoopFunctionType *) entry;

   BasicForLoopDownMethod basicForLoopDownMethodBuilder(&types, this);
   rc = compileMethodBuilder(&basicForLoopDownMethodBuilder, &entry);
   _basicForLoopDownMethod = (BasicForLoopFunctionType *) entry;

   ShootoutNestedLoopMethod shootoutNestedLoopMethodBuilder(&types, this);
   rc = compileMethodBuilder(&shootoutNestedLoopMethodBuilder, &entry);
   _shootoutNestedLoopMethod = (signatureCharI_I_testMethodType *) entry;

   AbsDiffIfThenElseMethod absDiffIfThenElseMethodBuilder(&types, this);
   rc = compileMethodBuilder(&absDiffIfThenElseMethodBuilder, &entry);
   _absDiffIfThenElseMethod = (IfFunctionType *) entry;

   MaxIfThenMethod maxIfThenMethodBuilder(&types, this);
   rc = compileMethodBuilder(&maxIfThenMethodBuilder, &entry);
   _maxIfThenMethod = (IfFunctionType *) entry;

   DoWhileFibonnaciMethod doWhileFibonnaciMethodBuilder(&types, this);
   rc = compileMethodBuilder(&doWhileFibonnaciMethodBuilder, &entry);
   _doWhileFibMethod = (DoWhileFibFunctionType *) entry;

   DoWhileWithBreakMethod doWhileWithBreakMethodBuilder(&types, this);
   rc = compileMethodBuilder(&doWhileWithBreakMethodBuilder, &entry);
   _doWhileWithBreakMethod = (DoWhileFunctionType *) entry;

   DoWhileWithContinueMethod doWhileWithContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&doWhileWithContinueMethodBuilder, &entry);
   _doWhileWithContinueMethod = (DoWhileFunctionType *) entry;

   DoWhileWithBreakAndContinueMethod doWhileWithBreakAndContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&doWhileWithBreakAndContinueMethodBuilder, &entry);
   _doWhileWithBreakAndContinueMethod = (DoWhileFunctionType *) entry;

   WhileDoFibonnaciMethod whileDoFibonnaciMethodBuilder(&types, this);
   rc = compileMethodBuilder(&whileDoFibonnaciMethodBuilder, &entry);
   _whileDoFibMethod = (WhileDoFibFunctionType *) entry;

   WhileDoWithBreakMethod whileDoWithBreakMethodBuilder(&types, this);
   rc = compileMethodBuilder(&whileDoWithBreakMethodBuilder, &entry);
   _whileDoWithBreakMethod = (WhileDoFunctionType *) entry;

   WhileDoWithContinueMethod whileDoWithContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&whileDoWithContinueMethodBuilder, &entry);
   _whileDoWithContinueMethod = (WhileDoFunctionType *) entry;

   WhileDoWithBreakAndContinueMethod whileDoWithBreakAndContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&whileDoWithBreakAndContinueMethodBuilder, &entry);
   _whileDoWithBreakAndContinueMethod = (WhileDoFunctionType *) entry;

   ForLoopContinueMethod forLoopContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&forLoopContinueMethodBuilder, &entry);
   _forLoopContinueMethod = (ForLoopContinueFunctionType *) entry;

   ForLoopBreakMethod forLoopBreakMethodBuilder(&types, this);
   rc = compileMethodBuilder(&forLoopBreakMethodBuilder, &entry);
   _forLoopBreakMethod = (ForLoopBreakFunctionType *) entry;

   ForLoopBreakAndContinueMethod forLoopBreakAndContinueMethodBuilder(&types, this);
   rc = compileMethodBuilder(&forLoopBreakAndContinueMethodBuilder, &entry);
   _forLoopBreakAndContinueMethod = (ForLoopBreakAndContinueFunctionType *) entry;
   }

void
BuilderTest::compileNestedControlFlowLoopTestMethods()
   {
   int32_t rc = 0;
   uint8_t *entry=0;

   TR::TypeDictionary types;

   IfThenElseLoopMethod ifThenElseLoopMethod(&types, this);
   rc = compileMethodBuilder(&ifThenElseLoopMethod, &entry);
   _ifThenElseLoopMethod = (IfFunctionType *) entry;

   ForLoopUPIfThenElseMethod forLoopIfThenElseMethod(&types, this);
   rc = compileMethodBuilder(&forLoopIfThenElseMethod, &entry);
   _forLoopUpIfThenElseMethod = (LoopIfThenElseFunctionType *) entry;

   DoWhileIfThenElseMethod doWhileIfThenElseMethod(&types, this);
   rc = compileMethodBuilder(&doWhileIfThenElseMethod, &entry);
   _doWhileIfThenElseMethod = (LoopIfThenElseFunctionType *) entry;

   WhileDoIfThenElseMethod whileDoIfThenElseMethod(&types, this);
   rc = compileMethodBuilder(&whileDoIfThenElseMethod, &entry);
   _whileDoIfThenElseMethod = (LoopIfThenElseFunctionType *) entry;
   }

int32_t
BuilderTest::recursiveFib(int32_t n)
   {
   if (n < 2)
      return n;
   return recursiveFib(n-1) + recursiveFib(n-2);
   }

int32_t
BuilderTest::iterativeFib(int32_t n)
   {
   if (n < 2)
      {
      return n;
      }

   int32_t last_sum = 1, lastlast_sum = 0;
   for (int32_t i=1; i < n;i++)
      {
      int32_t temp_sum = last_sum + lastlast_sum;
      lastlast_sum = last_sum;
      last_sum = temp_sum;
      }
   return last_sum;
   }

int32_t
BuilderTest::doWhileFib(int32_t n)
   {
   if (n < 2)
      {
      return n;
      }

   int32_t last_sum = 1, lastlast_sum = 0;

   int32_t i = 1;
   do
      {
      int32_t temp_sum = last_sum + lastlast_sum;
      lastlast_sum = last_sum;
      last_sum = temp_sum;
      i = i + 1;
      }
   while (i < n);

   return last_sum;
   }

int32_t
BuilderTest::doWhileWithBreak(int32_t n)
   {
   int32_t a = 0;
   do
      {
      a = a + 1;
      if ( a > 10)
         {
         a = 20;
         break;
         }
      a = a + 1;
      }
   while (a < n);

   return a;
   }

int32_t
BuilderTest::doWhileWithContinue(int32_t n)
   {
   int32_t a = 0;
   do
      {
      a = a + 1;
      if ( a == 5)
         {
         a = a - 2;
         continue;
         }
      a = a + 1;
      }
   while (a < n);

   return a;
   }

int32_t
BuilderTest::doWhileWithBreakAndContinue(int32_t n)
   {
   int32_t a = 0;
   do
      {
      a = a + 1;
      if ( a == 5)
         {
         a = a - 2;
         continue;
         }

      a = a + 1;
      if ( a > 10)
         {
         a = 20;
         break;
         }

      }
   while (a < n);

   return a;
   }

int32_t
BuilderTest::whileDoFib(int32_t n)
   {
   if (n < 2)
      {
      return n;
      }

   int32_t last_sum = 1, lastlast_sum = 0;

   int32_t i = 1;
   while (i < n)
      {
      int32_t temp_sum = last_sum + lastlast_sum;
      lastlast_sum = last_sum;
      last_sum = temp_sum;
      i = i + 1;
      }

   return last_sum;
   }

int32_t
BuilderTest::whileDoWithBreak(int32_t n)
   {
   int32_t a = 0;
   while (a < n)
      {
      a = a + 1;
      if ( a > 10)
         {
         a = 20;
         break;
         }
      a = a + 1;
      }

   return a;
   }

int32_t
BuilderTest::whileDoWithContinue(int32_t n)
   {
   int32_t a = 0;
   while (a < n)
      {
      a = a + 1;
      if ( a == 5)
         {
         a = a - 2;
         continue;
         }
      a = a + 1;
      }

   return a;
   }

int32_t
BuilderTest::whileDoWithBreakAndContinue(int32_t n)
   {
   int32_t a = 0;
   while (a < n)
      {
      a = a + 1;
      if ( a == 5)
         {
         a = a - 2;
         continue;
         }
      a = a + 1;
      if ( a > 10)
         {
         a = 20;
         break;
         }
      }

   return a;
   }

int32_t
BuilderTest::basicForLoop(int32_t initial, int32_t final, int32_t bump, bool countsUp)
   {
   int32_t sum = 0;

   if (countsUp)
      {
      for (int32_t i = initial; i < final; i = i + bump)
         {
         sum = sum + i;
         }
      }
   else
      {
      for (int32_t i = initial; i > final; i = i - bump)
         {
         sum = sum - i;
         }
      }
   return sum;
   }

int32_t
BuilderTest::shootoutNestedLoop(int32_t n)
   {
   int a, b, c, d, e, f, x=0;
   for (a = 0; a < n; a++)
      {
      for (b = 0; b < n; b++)
         {
         for (c = 0; c < n; c++)
            {
            for (d = 0; d < n; d++)
               {
               for (e = 0; e < n; e++)
                  {
                  for (f = 0; f < n; f++)
                     {
                     x++;
                     }
                  }
               }
            }
         }
      }
   return x;
   }

int32_t
BuilderTest::absDiff(int32_t valueA, int32_t valueB)
   {
   int32_t diff = 0;
   if (valueA > valueB)
      {
      diff = valueA - valueB;
      }
   else
      {
      diff = valueB - valueA;
      }
   return diff;
   }

int32_t
BuilderTest::maxIfThen(int32_t leftV, int32_t rightV)
   {
   if (leftV > rightV)
      {
      return leftV;
      }
   return rightV;
   }

int64_t
BuilderTest::subIfFalseThen(int64_t valueA, int64_t valueB)
   {
   if (0)
      {
      return valueA + valueB;
      }
   else
      {
      return valueA - valueB;
      }
   }

int32_t
BuilderTest::ifThenElseLoop(int32_t leftV, int32_t rightV)
   {
   int32_t sum = 1, last_sum = 0;

   if (leftV > rightV)
      {
      if (leftV < 2)
         {
         return leftV;
         }
      int32_t i = 1;
      while (i < leftV)
         {
         int32_t temp_sum = sum + last_sum;
         last_sum = sum;
         sum = temp_sum;
         i = i + 1;
         }
      }
   else
      {
      if (rightV < 2)
         {
         return rightV;
         }
      for (int32_t i=1; i < rightV;i++)
         {
         int32_t temp_sum = sum + last_sum;
         last_sum = sum;
         sum = temp_sum;
         }
      }
   return sum;
   }

int32_t
BuilderTest::forLoopIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue, bool countsUp)
   {
   if (countsUp)
      {
      int32_t sum = 0;
      for (int32_t i = initial; i < final;i += bump)
         {
         if (i > compareValue)
            {
            if ( i > sum )
               {
               return i;
               }
            else
               {
               return sum ;
               }
            }
         sum += bump;
         }
      }
   else
      {
      TR_ASSERT(0, "for loop down is not supported right now");
      }

   TR_ASSERT(0, "input parameters are not properly defined");
   return -1;
   }

int32_t
BuilderTest::whileDoIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue)
   {
   int32_t sum = 0;
   int32_t i = initial;
   while (i < final)
      {
      if (i > compareValue)
         {
         if ( i > sum )
            {
            return i;
            }
         else
            {
            return sum ;
            }
         }
      sum += bump;
      i += bump;
      }

   TR_ASSERT(0, "input parameters are not properly defined");
   return -1;
   }

int32_t
BuilderTest::doWhileIfThenElse(int32_t initial, int32_t final, int32_t bump, int32_t compareValue)
   {
   int32_t sum = 0;
   int32_t i = initial;
   do
      {
      if (i > compareValue)
         {
         if( i == sum)
            {
            return sum;
            }
         if ( i < sum )
            {
            return sum;
            }
         else
            {
            return i ;
            }
         }
      sum += bump;
      i += bump;
      }
   while (i < final);
   TR_ASSERT(0, "input parameters are not properly defined");
   return -1;
   }

int32_t
BuilderTest::forLoopContinue(int32_t n)
   {
   int32_t sum = 0;
   for (int32_t i = 0; i < n; i++)
      {
      if (i == 4)
         {
         sum += i;
         continue;
         }
      int32_t temp = i * 2;
      sum += temp;
      }
   return sum;
   }

int32_t
BuilderTest::forLoopBreak(int32_t n)
   {
   int32_t sum = 0;
   for (int32_t i = 0; i < n; i++)
      {
      if (i == 8)
         {
         sum += i;
         break;
         }
      int32_t temp = i * 2;
      sum += temp;
      }
   return sum;
   }

int32_t
BuilderTest::forLoopBreakAndContinue(int32_t n)
   {
   int32_t sum = 0;
   for (int32_t i = 0; i < n; i++)
      {
      if (i == 8)
         {
         sum += i;
         break;
         }

      if (i == 4)
         {
         sum += i;
         continue;
         }

      int32_t temp = i * 2;
      sum += temp;
      }
   return sum;
   }

void
BuilderTest::invokeTests()
   {
   for(uint32_t i = 0; i <= 20 ; i++)
      {
   	OMR_CT_EXPECT_EQ(_iterativeFibMethod, iterativeFib(i), _iterativeFibMethod(i));
#if (defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)) || defined(TR_TARGET_S390)
   	OMR_CT_EXPECT_EQ(_recursiveFibMethod, recursiveFib(i), _recursiveFibMethod(i));
#endif
      }
   }

void
BuilderTest::invokeControlFlowTests()
   {
   int32_t basicForLoopParms[][3] =
         {
         -9, 1000, 1,
         9, 1000, -1,
         0, 10000, 11,
         10000, 0, 20
         };

   for(uint32_t i = 0; i < sizeof(basicForLoopParms) / sizeof(basicForLoopParms[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_basicForLoopUpMethod, basicForLoop(basicForLoopParms[i][0], basicForLoopParms[i][1], basicForLoopParms[i][2], true), _basicForLoopUpMethod(basicForLoopParms[i][0], basicForLoopParms[i][1], basicForLoopParms[i][2]));

      OMR_CT_EXPECT_EQ(_basicForLoopDownMethod, basicForLoop(basicForLoopParms[i][0], basicForLoopParms[i][1], basicForLoopParms[i][2], false), _basicForLoopDownMethod(basicForLoopParms[i][0], basicForLoopParms[i][1], basicForLoopParms[i][2]));
      }

   int32_t nestedLoopParms[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 16, 20};

   for(uint32_t i = 0; i < sizeof(nestedLoopParms) / sizeof(nestedLoopParms[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_shootoutNestedLoopMethod, shootoutNestedLoop(nestedLoopParms[i]), _shootoutNestedLoopMethod(nestedLoopParms[i]));
      }

   int32_t absDiffParms[][2] =
      {
      (int32_t)TR::getMaxSigned<TR::Int32>(), (int32_t)TR::getMaxSigned<TR::Int32>() - 1,
      (int32_t)TR::getMinSigned<TR::Int32>() + 1, (int32_t)TR::getMinSigned<TR::Int32>(),
      (int32_t)TR::getMinSigned<TR::Int32>() + 9, 8
      };

   for(uint32_t i = 0; i < sizeof(absDiffParms) / sizeof(absDiffParms[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_absDiffIfThenElseMethod, absDiff(absDiffParms[i][0], absDiffParms[i][1]), _absDiffIfThenElseMethod(absDiffParms[i][0], absDiffParms[i][1]));
      }

   int32_t maxIfThenParms[][2] =
      {
      (int32_t)TR::getMaxSigned<TR::Int32>(), (int32_t)TR::getMinSigned<TR::Int32>(),
      (int32_t)TR::getMinSigned<TR::Int32>(), (int32_t)TR::getMaxSigned<TR::Int32>(),
      (int32_t)TR::getMinSigned<TR::Int32>() + 1, (int32_t)TR::getMinSigned<TR::Int32>(),
      9, 9
      };

   for(uint32_t i = 0; i < sizeof(maxIfThenParms) / sizeof(maxIfThenParms[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_maxIfThenMethod, maxIfThen(maxIfThenParms[i][0], maxIfThenParms[i][1]), _maxIfThenMethod(maxIfThenParms[i][0], maxIfThenParms[i][1]));
      }

   const int64_t LONG_NEG = -9;
   const int64_t LONG_POS = 9;
   int64_t subIfFalseThenParms[][2] =
      {
      TR::getMaxSigned<TR::Int64>(), LONG_POS,
      TR::getMinSigned<TR::Int64>(), LONG_NEG,
      LONG_POS, TR::getMaxSigned<TR::Int64>()
      };

   for(uint32_t i = 0; i < sizeof(subIfFalseThenParms) / sizeof(subIfFalseThenParms[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_subIfFalseThenMethod, subIfFalseThen(subIfFalseThenParms[i][0], subIfFalseThenParms[i][1]), _subIfFalseThenMethod(subIfFalseThenParms[i][0], subIfFalseThenParms[i][1]));
      }

   int32_t loopN[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20};
   for(uint32_t i = 0; i < sizeof(loopN) / sizeof(loopN[0]); ++i)
      {
      OMR_CT_EXPECT_EQ(_doWhileFibMethod, doWhileFib(loopN[i]), _doWhileFibMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_whileDoFibMethod, whileDoFib(loopN[i]), _whileDoFibMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_forLoopContinueMethod, forLoopContinue(loopN[i]), _forLoopContinueMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_forLoopBreakMethod, forLoopBreak(loopN[i]), _forLoopBreakMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_forLoopBreakAndContinueMethod, forLoopBreakAndContinue(loopN[i]), _forLoopBreakAndContinueMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_doWhileWithBreakMethod, doWhileWithBreak(loopN[i]), _doWhileWithBreakMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_doWhileWithContinueMethod, doWhileWithContinue(loopN[i]), _doWhileWithContinueMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_doWhileWithBreakAndContinueMethod, doWhileWithBreakAndContinue(loopN[i]), _doWhileWithBreakAndContinueMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_whileDoWithBreakMethod, whileDoWithBreak(loopN[i]), _whileDoWithBreakMethod(loopN[i]));
      OMR_CT_EXPECT_EQ(_whileDoWithContinueMethod, whileDoWithContinue(loopN[i]), _whileDoWithContinueMethod(loopN[i])) << i;
      OMR_CT_EXPECT_EQ(_whileDoWithBreakAndContinueMethod, whileDoWithBreakAndContinue(loopN[i]), _whileDoWithBreakAndContinueMethod(loopN[i]));
      }
   }

void
BuilderTest::invokeNestedControlFlowLoopTests()
   {
   int32_t ifThenElseLoopArr[][2] =
      {
      1, 1,
      1, 0,
      0, 1,
      0, 20,
      1, 10,
      2, 9,
      3, 8,
      4, 7,
      5, 6,
      6, 5,
      7, 4,
      8, 3,
      9, 2,
      10, 1,
      20, 0
      };

   int32_t loopIfThenElseArr[][4] =
         {
         -9, 1000, 1, 500,
         9, 1000, 8, 500,
         0, 10000, 11, 500,
         -9, 1000, 1, 500,
         -20, 1000, 8, 500,
         18, 10000, 11, 500
         };

   int32_t arrSize = 0;

   arrSize = sizeof(loopIfThenElseArr) / sizeof(loopIfThenElseArr[0]);
   for(uint32_t i = 0; i < arrSize; ++i)
      {
      OMR_CT_EXPECT_EQ(_forLoopUpIfThenElseMethod, forLoopIfThenElse(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]), _forLoopUpIfThenElseMethod(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]));
      }

   arrSize = sizeof(loopIfThenElseArr) / sizeof(loopIfThenElseArr[0]);
   for(uint32_t i = 0; i < arrSize; ++i)
      {
      OMR_CT_EXPECT_EQ(_whileDoIfThenElseMethod, whileDoIfThenElse(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]), _whileDoIfThenElseMethod(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]));
      }

   arrSize = sizeof(loopIfThenElseArr) / sizeof(loopIfThenElseArr[0]);
   for(uint32_t i = 0; i < arrSize; ++i)
      {
      OMR_CT_EXPECT_EQ(_doWhileIfThenElseMethod, doWhileIfThenElse(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]), _doWhileIfThenElseMethod(loopIfThenElseArr[i][0], loopIfThenElseArr[i][1], loopIfThenElseArr[i][2], loopIfThenElseArr[i][3]));
      }

   arrSize = sizeof(ifThenElseLoopArr) / sizeof(ifThenElseLoopArr[0]);
   for(int32_t i = 0; i < arrSize; i++)
      {
      OMR_CT_EXPECT_EQ(_ifThenElseLoopMethod, ifThenElseLoop(ifThenElseLoopArr[i][0], ifThenElseLoopArr[i][1]), _ifThenElseLoopMethod(ifThenElseLoopArr[i][0], ifThenElseLoopArr[i][1]));
      }
   }

RecursiveFibonnaciMethod::RecursiveFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_recur");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);

   DefineMemory("traceEnabled", Int32, test->traceEnabledLocation());
   }

bool
RecursiveFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *baseCase=NULL, *recursiveCase=NULL;
   IfThenElse(&baseCase, &recursiveCase,
      LessThan(
         Load("n"),
         ConstInt32(2)));

   DefineLocal("result", Int32);

   baseCase->Store("result",
   baseCase->   Load("n"));
   recursiveCase->Store("result",
   recursiveCase->   Add(
   recursiveCase->      Call("fib_recur", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(1))),
   recursiveCase->      Call("fib_recur", 1,
   recursiveCase->         Sub(
   recursiveCase->            Load("n"),
   recursiveCase->            ConstInt32(2)))));

   Return(
      Load("result"));
   return true;
   }

IterativeFibonnaciMethod::IterativeFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_iter");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
IterativeFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *returnZero = NULL;
   IfThen(&returnZero,
      EqualTo(
         Load("N"),
         ConstInt32(0)));
   returnZero->Return(
   returnZero->   ConstInt32(0));

   TR::IlBuilder *returnOne = NULL;
   IfThen(&returnOne,
      EqualTo(
         Load("N"),
         ConstInt32(1)));
   returnOne->Return(
   returnOne->   ConstInt32(1));

   Store("LastSum",
      ConstInt32(0));

   Store("Sum",
      ConstInt32(1));

   TR::IlBuilder *body = NULL;
   ForLoopUp("I", &body,
           ConstInt32(1),
           Load("N"),
           ConstInt32(1));

   body->Store("tempSum",
   body->   Add(
   body->      Load("Sum"),
   body->      Load("LastSum")));
   body->Store("LastSum",
   body->   Load("Sum"));
   body->Store("Sum",
   body->   Load("tempSum"));

   Return(
      Load("Sum"));

   return true;
   }

DoWhileFibonnaciMethod::DoWhileFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_DoWhile");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
DoWhileFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *returnN = NULL;
   IfThen(&returnN,
      LessThan(
         Load("N"),
         ConstInt32(2)));
   returnN->Return(
   returnN->   Load("N"));

   Store("LastSum",
      ConstInt32(0));

   Store("Sum",
      ConstInt32(1));

   Store("i",
      ConstInt32(1));

   DefineLocal("exitLoop", Int32);

   TR::IlBuilder *body = NULL;
   DoWhileLoop("exitLoop", &body);

   body->Store("tempSum",
   body->   Add(
   body->      Load("Sum"),
   body->      Load("LastSum")));
   body->Store("LastSum",
   body->   Load("Sum"));
   body->Store("Sum",
   body->   Load("tempSum"));
   body->Store("i",
   body->   Add(
   body->      Load("i"),
   body->      ConstInt32(1)));
   body->Store("exitLoop",
   body->   LessThan(
   body->      Load("i"),
   body->      Load("N")));

   Return(
      Load("Sum"));

   return true;
   }

DoWhileWithBreakMethod::DoWhileWithBreakMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("DoWhileWithBreak");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
DoWhileWithBreakMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   TR::IlBuilder *doWhileBody = NULL;
   TR::IlBuilder *breakBody = NULL;
   DefineLocal("exitLoop", Int32);
   DoWhileLoopWithBreak("exitLoop", &doWhileBody, &breakBody);

   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));

   TR::IlBuilder *thenPath = NULL;
   TR::IlValue *breakCondition = doWhileBody->GreaterThan(
                                 doWhileBody->   Load("a"),
                                 doWhileBody->   ConstInt32(10));

   doWhileBody->IfThen(&thenPath, breakCondition);
   thenPath->Store("a",
   thenPath->   ConstInt32(20));
   thenPath->Goto(&breakBody);

   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));
   doWhileBody->Store("exitLoop",
   doWhileBody->   LessThan(
   doWhileBody->      Load("a"),
   doWhileBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

DoWhileWithContinueMethod::DoWhileWithContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("DoWhileWithContinue");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
DoWhileWithContinueMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   TR::IlBuilder *doWhileBody = NULL;
   TR::IlBuilder *continueBody = NULL;
   DefineLocal("exitLoop", Int32);
   DoWhileLoopWithContinue("exitLoop", &doWhileBody, &continueBody);

   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));

   TR::IlBuilder *thenPath = NULL;
   TR::IlValue *continueCondition = doWhileBody->EqualTo(
                                    doWhileBody->   Load("a"),
                                    doWhileBody->   ConstInt32(5));

   doWhileBody->IfThen(&thenPath, continueCondition);
   thenPath->Store("a",
   thenPath->   Sub(
   thenPath->      Load("a"),
   thenPath->      ConstInt32(2)));
   thenPath->Goto(&continueBody);

   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));

   doWhileBody->Store("exitLoop",
   doWhileBody->   LessThan(
   doWhileBody->      Load("a"),
   doWhileBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

DoWhileWithBreakAndContinueMethod::DoWhileWithBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("DoWhileWithBreakAndContinue");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
DoWhileWithBreakAndContinueMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   TR::IlBuilder *doWhileBody = NULL;
   TR::IlBuilder *continueBody = NULL;
   TR::IlBuilder *breakBody = NULL;
   DefineLocal("exitLoop", Int32);
   DoWhileLoop("exitLoop", &doWhileBody, &breakBody, &continueBody);

   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));

   TR::IlBuilder *continueThenPath = NULL;
   TR::IlValue *continueCondition = doWhileBody->EqualTo(
                                    doWhileBody->   Load("a"),
                                    doWhileBody->   ConstInt32(5));

   doWhileBody->IfThen(&continueThenPath, continueCondition);
   continueThenPath->Store("a",
   continueThenPath->   Sub(
   continueThenPath->      Load("a"),
   continueThenPath->      ConstInt32(2)));
   continueThenPath->Goto(&continueBody);


   doWhileBody->Store("a",
   doWhileBody->   Add(
   doWhileBody->      Load("a"),
   doWhileBody->      ConstInt32(1)));

   TR::IlBuilder *breakThenPath = NULL;
   TR::IlValue *breakCondition = doWhileBody->GreaterThan(
                                    doWhileBody->   Load("a"),
                                    doWhileBody->   ConstInt32(10));
   doWhileBody->IfThen(&breakThenPath, breakCondition);
   breakThenPath->Store("a",
   breakThenPath->   ConstInt32(20));
   breakThenPath->Goto(&breakBody);

   doWhileBody->Store("exitLoop",
   doWhileBody->   LessThan(
   doWhileBody->      Load("a"),
   doWhileBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

WhileDoFibonnaciMethod::WhileDoFibonnaciMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("fib_WhileDo");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
WhileDoFibonnaciMethod::buildIL()
   {
   TR::IlBuilder *returnN = NULL;
   IfThen(&returnN,
      LessThan(
         Load("N"),
         ConstInt32(2)));
   returnN->Return(
   returnN->   Load("N"));

   Store("LastSum",
      ConstInt32(0));

   Store("Sum",
      ConstInt32(1));

   Store("i",
      ConstInt32(1));

   Store("exitLoop",
      LessThan(
         Load("i"),
         Load("N")));

   TR::IlBuilder *body = NULL;
   WhileDoLoop("exitLoop", &body);

   body->Store("tempSum",
   body->   Add(
   body->      Load("Sum"),
   body->      Load("LastSum")));
   body->Store("LastSum",
   body->   Load("Sum"));
   body->Store("Sum",
   body->   Load("tempSum"));
   body->Store("i",
   body->   Add(
   body->      Load("i"),
   body->      ConstInt32(1)));
   body->Store("exitLoop",
   body->   LessThan(
   body->      Load("i"),
   body->      Load("N")));

   Return(
      Load("Sum"));

   return true;
   }

WhileDoWithBreakMethod::WhileDoWithBreakMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("WhileDoWithBreak");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
WhileDoWithBreakMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   Store("exitLoop",
      LessThan(
         Load("a"),
         Load("N")));

   TR::IlBuilder *whileDoBody = NULL;
   TR::IlBuilder *breakBody = NULL;
   WhileDoLoopWithBreak("exitLoop", &whileDoBody, &breakBody);

   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));

   TR::IlBuilder *thenPath = NULL;
   TR::IlValue *breakCondition = whileDoBody->GreaterThan(
                                 whileDoBody->   Load("a"),
                                 whileDoBody->   ConstInt32(10));

   whileDoBody->IfThen(&thenPath, breakCondition);
   thenPath->Store("a",
   thenPath->   ConstInt32(20));
   thenPath->Goto(&breakBody);

   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));
   whileDoBody->Store("exitLoop",
   whileDoBody->   LessThan(
   whileDoBody->      Load("a"),
   whileDoBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

WhileDoWithContinueMethod::WhileDoWithContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("WhileDoWithContinue");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
WhileDoWithContinueMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   Store("exitLoop",
      LessThan(
         Load("a"),
         Load("N")));

   TR::IlBuilder *whileDoBody = NULL;
   TR::IlBuilder *continueBody = NULL;
   WhileDoLoopWithContinue("exitLoop", &whileDoBody, &continueBody);

   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));

   TR::IlBuilder *thenPath = NULL;
   TR::IlValue *continueCondition = whileDoBody->EqualTo(
                                    whileDoBody->   Load("a"),
                                    whileDoBody->   ConstInt32(5));

   whileDoBody->IfThen(&thenPath, continueCondition);
   thenPath->Store("a",
   thenPath->   Sub(
   thenPath->      Load("a"),
   thenPath->      ConstInt32(2)));
   thenPath->Goto(&continueBody);

   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));

   whileDoBody->Store("exitLoop",
   whileDoBody->   LessThan(
   whileDoBody->      Load("a"),
   whileDoBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

WhileDoWithBreakAndContinueMethod::WhileDoWithBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("WhileDoWithBreakAndContinue");
   DefineParameter("N", Int32);
   DefineReturnType(Int32);
   }

bool
WhileDoWithBreakAndContinueMethod::buildIL()
   {
   Store("a",
      ConstInt32(0));

   Store("exitLoop",
      LessThan(
         Load("a"),
         Load("N")));

   TR::IlBuilder *whileDoBody = NULL;
   TR::IlBuilder *continueBody = NULL;
   TR::IlBuilder *breakBody = NULL;
   WhileDoLoop("exitLoop", &whileDoBody, &breakBody, &continueBody);

   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));

   TR::IlBuilder *continueThenPath = NULL;
   TR::IlValue *continueCondition = whileDoBody->EqualTo(
                                    whileDoBody->   Load("a"),
                                    whileDoBody->   ConstInt32(5));

   whileDoBody->IfThen(&continueThenPath, continueCondition);
   continueThenPath->Store("a",
   continueThenPath->   Sub(
   continueThenPath->      Load("a"),
   continueThenPath->      ConstInt32(2)));
   continueThenPath->Goto(&continueBody);


   whileDoBody->Store("a",
   whileDoBody->   Add(
   whileDoBody->      Load("a"),
   whileDoBody->      ConstInt32(1)));

   TR::IlBuilder *breakThenPath = NULL;
   TR::IlValue *breakCondition = whileDoBody->GreaterThan(
                                    whileDoBody->   Load("a"),
                                    whileDoBody->   ConstInt32(10));
   whileDoBody->IfThen(&breakThenPath, breakCondition);
   breakThenPath->Store("a",
   breakThenPath->   ConstInt32(20));
   breakThenPath->Goto(&breakBody);

   whileDoBody->Store("exitLoop",
   whileDoBody->   LessThan(
   whileDoBody->      Load("a"),
   whileDoBody->      Load("N")));

   Return(
      Load("a"));

   return true;
   }

BasicForLoopUpMethod::BasicForLoopUpMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("basicForLoopUp");
   DefineParameter("initial", Int32);
   DefineParameter("final", Int32);
   DefineParameter("bump", Int32);
   DefineReturnType(Int32);
   }

bool
BasicForLoopUpMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *body = NULL;
   ForLoopUp("I", &body,
           Load("initial"),
           Load("final"),
           Load("bump"));

   body->Store("Sum",
   body->   Add(
   body->      Load("Sum"),
   body->      Load("I")));

   Return(
      Load("Sum"));

   return true;
   }

BasicForLoopDownMethod::BasicForLoopDownMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("basicForLoopDown");
   DefineParameter("initial", Int32);
   DefineParameter("final", Int32);
   DefineParameter("bump", Int32);
   DefineReturnType(Int32);
   }

bool
BasicForLoopDownMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *body = NULL;
   ForLoopDown("I", &body,
           Load("initial"),
           Load("final"),
           Load("bump"));

   body->Store("Sum",
   body->   Sub(
   body->      Load("Sum"),
   body->      Load("I")));

   Return(
      Load("Sum"));

   return true;
   }

ShootoutNestedLoopMethod::ShootoutNestedLoopMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("ShootoutNestedLoop");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
ShootoutNestedLoopMethod::buildIL()
   {
   Store("x", ConstInt32(0));

   TR::IlBuilder *body_a = NULL;
   ForLoopUp("a", &body_a,
             ConstInt32(0),
             Load("n"),
             ConstInt32(1));

   TR::IlBuilder *body_b = NULL;
   body_a->ForLoopUp("b", &body_b,
   body_a->          ConstInt32(0),
   body_a->          Load("n"),
   body_a->          ConstInt32(1));

   TR::IlBuilder *body_c = NULL;
   body_b->ForLoopUp("c", &body_c,
   body_b->          ConstInt32(0),
   body_b->          Load("n"),
   body_b->          ConstInt32(1));

   TR::IlBuilder *body_d = NULL;
   body_c->ForLoopUp("d", &body_d,
   body_c->          ConstInt32(0),
   body_c->          Load("n"),
   body_c->          ConstInt32(1));

   TR::IlBuilder *body_e = NULL;
   body_d->ForLoopUp("e", &body_e,
   body_d->          ConstInt32(0),
   body_d->          Load("n"),
   body_d->          ConstInt32(1));

   TR::IlBuilder *body_f = NULL;
   body_e->ForLoopUp("f", &body_f,
   body_e->          ConstInt32(0),
   body_e->          Load("n"),
   body_e->          ConstInt32(1));

   body_f->Store("x",
   body_f->   Add(
   body_f->      Load("x"),
   body_f->      ConstInt32(1)));

   Return(
      Load("x"));

   return true;
   }


AbsDiffIfThenElseMethod::AbsDiffIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("AbsDiffIfThenElseMethod");
   DefineParameter("valueA", Int32);
   DefineParameter("valueB", Int32);
   DefineReturnType(Int32);
   }

bool
AbsDiffIfThenElseMethod::buildIL()
   {
   Store("diff", ConstInt32(0));
   TR::IlBuilder *thenPath = NULL;
   TR::IlBuilder *elsePath = NULL;
   IfThenElse(&thenPath, &elsePath,
      GreaterThan(
         Load("valueA"),
         Load("valueB")));

   thenPath->Store("diff",
   thenPath->   Sub(
   thenPath->      Load("valueA"),
   thenPath->      Load("valueB")));

   elsePath->Store("diff",
   elsePath->   Sub(
   elsePath->      Load("valueB"),
   elsePath->      Load("valueA")));

   Return(
      Load("diff"));

   return true;
   }

MaxIfThenMethod::MaxIfThenMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("MaxIfThenMethod");
   DefineParameter("leftV", Int32);
   DefineParameter("rightV", Int32);
   DefineReturnType(Int32);
   }

bool
MaxIfThenMethod::buildIL()
   {
   TR::IlBuilder *thenPath = NULL;
   TR::IlValue *condition;
   IfThen(&thenPath,
      GreaterThan(
         Load("leftV"),
         Load("rightV")));

   thenPath->Return(
   thenPath->   Load("leftV"));

   Return(
      Load("rightV"));

   return true;
   }

IfThenElseLoopMethod::IfThenElseLoopMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("IfThenElseLoopMethod");
   DefineParameter("leftV", Int32);
   DefineParameter("rightV", Int32);
   DefineReturnType(Int32);
   }

bool
IfThenElseLoopMethod::buildIL()
   {
   Store("LastSum",ConstInt32(0));
   Store("Sum",ConstInt32(1));

   TR::IlBuilder *thenPath = NULL;
   TR::IlBuilder *thenReturnN = NULL;
   TR::IlBuilder *whileDoBody = NULL;
   TR::IlBuilder *elsePath = NULL;
   TR::IlBuilder *elseReturnN = NULL;
   TR::IlBuilder *forBody = NULL;
   IfThenElse(&thenPath, &elsePath,
      GreaterThan(
         Load("leftV"),
         Load("rightV")));

   //if
   thenPath->IfThen(&thenReturnN,
   thenPath->   LessThan(
   thenPath->      Load("leftV"),
   thenPath->      ConstInt32(2)));

   thenReturnN->Return(
   thenReturnN->   Load("leftV"));

   thenPath->Store("i",
   thenPath->   ConstInt32(1));

   thenPath->Store("exitCondition",
   thenPath->   LessThan(
   thenPath->      Load("i"),
   thenPath->      Load("leftV")));

   thenPath->WhileDoLoop("exitCondition", &whileDoBody);

   whileDoBody->Store("tempSum",
   whileDoBody->   Add(
   whileDoBody->      Load("Sum"),
   whileDoBody->      Load("LastSum")));
   whileDoBody->Store("LastSum",
   whileDoBody->   Load("Sum"));
   whileDoBody->Store("Sum",
   whileDoBody->   Load("tempSum"));
   whileDoBody->Store("i",
   whileDoBody->      Add(
   whileDoBody->         Load("i"),
   whileDoBody->         ConstInt32(1)));
   whileDoBody->Store("exitCondition",
   whileDoBody->   LessThan(
   whileDoBody->      Load("i"),
   whileDoBody->      Load("leftV")));

   thenPath->Return(
   thenPath->   Load("Sum"));

   //else
   elsePath->IfThen(&elseReturnN,
   elsePath->   LessThan(
   elsePath->      Load("rightV"),
   elsePath->      ConstInt32(2)));

   elseReturnN->Return(
   elseReturnN->   Load("rightV"));

   elsePath->ForLoopUp("I", &forBody,
   elsePath->         ConstInt32(1),
   elsePath->         Load("rightV"),
   elsePath->         ConstInt32(1));

   forBody->Store("tempSum",
   forBody->   Add(
   forBody->      Load("Sum"),
   forBody->      Load("LastSum")));
   forBody->Store("LastSum",
   forBody->   Load("Sum"));
   forBody->Store("Sum",
   forBody->   Load("tempSum"));

   elsePath->Return(
   elsePath->   Load("Sum"));

   return true;
   }

ForLoopUPIfThenElseMethod::ForLoopUPIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("forLoopUpIfThenElse");
   DefineParameter("initial", Int32);
   DefineParameter("final", Int32);
   DefineParameter("bump", Int32);
   DefineParameter("compareValue", Int32);
   DefineReturnType(Int32);
   }

bool
ForLoopUPIfThenElseMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *ifThenBuilder = NULL;
   TR::IlBuilder *thenPath = NULL;
   TR::IlBuilder *elsePath = NULL;
   TR::IlBuilder *forLoopBody = NULL;

   ForLoopUp("I", &forLoopBody,
           Load("initial"),
           Load("final"),
           Load("bump"));

   TR::IlValue *condition1 =
   forLoopBody->GreaterThan(
   forLoopBody->   Load("I"),
   forLoopBody->   Load("compareValue"));
   forLoopBody->IfThen(&ifThenBuilder, condition1);


   TR::IlValue *condition2 =
   ifThenBuilder->GreaterThan(
   ifThenBuilder->   Load("I"),
   ifThenBuilder->   Load("Sum"));
   ifThenBuilder->IfThenElse(&thenPath, &elsePath, condition2);

   thenPath->Return(
   thenPath->   Load("I"));

   elsePath->Return(
   elsePath->   Load("Sum"));

   forLoopBody->Store("Sum",
   forLoopBody->   Add(
   forLoopBody->      Load("bump"),
   forLoopBody->      Load("Sum")));

   Return(ConstInt32(-1));

   return true;
   }

WhileDoIfThenElseMethod::WhileDoIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("whileDoIfThenElse");
   DefineParameter("initial", Int32);
   DefineParameter("final", Int32);
   DefineParameter("bump", Int32);
   DefineParameter("compareValue", Int32);
   DefineReturnType(Int32);
   }

bool
WhileDoIfThenElseMethod::buildIL()
   {
   Store("sum",
      ConstInt32(0));

   Store("i",
      Load("initial"));

   TR::IlBuilder *whileDoBody = NULL;
   TR::IlBuilder *ifThenBuilder = NULL;
   TR::IlBuilder *thenPath = NULL;
   TR::IlBuilder *elsePath = NULL;

   Store("exitLoop",
      LessThan(
         Load("i"),
         Load("final")));
   WhileDoLoop("exitLoop", &whileDoBody);

   TR::IlValue *condition1 =
   whileDoBody->GreaterThan(
   whileDoBody->   Load("i"),
   whileDoBody->   Load("compareValue"));
   whileDoBody->IfThen(&ifThenBuilder, condition1);

   TR::IlValue *condition2 =
   ifThenBuilder->GreaterThan(
   ifThenBuilder->   Load("i"),
   ifThenBuilder->   Load("sum"));
   ifThenBuilder->IfThenElse(&thenPath, &elsePath, condition2);

   thenPath->Return(
   thenPath->   Load("i"));

   elsePath->Return(
   elsePath->   Load("sum"));

   whileDoBody->Store("sum",
   whileDoBody->   Add(
   whileDoBody->      Load("bump"),
   whileDoBody->      Load("sum")));

   whileDoBody->Store("i",
   whileDoBody->   Add(
   whileDoBody->      Load("bump"),
   whileDoBody->      Load("i")));


   Return(ConstInt32(-1));

   return true;
   }

DoWhileIfThenElseMethod::DoWhileIfThenElseMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("doWhileIfThenElse");
   DefineParameter("initial", Int32);
   DefineParameter("final", Int32);
   DefineParameter("bump", Int32);
   DefineParameter("compareValue", Int32);
   DefineReturnType(Int32);
   }

bool
DoWhileIfThenElseMethod::buildIL()
   {
   Store("sum",
      ConstInt32(0));

   Store("i",
      Load("initial"));

   TR::IlBuilder *ifLessThanBody = NULL;
   TR::IlBuilder *ifGreaterThanBody = NULL;
   TR::IlBuilder *ifEqualToBody = NULL;
   TR::IlBuilder *thenPath = NULL;
   TR::IlBuilder *elsePath = NULL;
   TR::IlBuilder *doWhileBody = NULL;

   DefineLocal("exitCondition", Int32);
   DoWhileLoop("exitCondition", &doWhileBody);

   TR::IlValue *conditionGreaterThan =
   doWhileBody->GreaterThan(
   doWhileBody->   Load("i"),
   doWhileBody->   Load("compareValue"));
   doWhileBody->IfThen(&ifGreaterThanBody, conditionGreaterThan);

   TR::IlValue *conditionEqualTo =
   ifGreaterThanBody->EqualTo(
   ifGreaterThanBody->   Load("i"),
   ifGreaterThanBody->   Load("sum"));
   ifGreaterThanBody->IfThen(&ifEqualToBody, conditionEqualTo);

   ifEqualToBody->Return(
   ifEqualToBody->   Load("sum"));

   TR::IlValue *conditionLessThan =
   ifGreaterThanBody->LessThan(
   ifGreaterThanBody->   Load("i"),
   ifGreaterThanBody->   Load("sum"));
   ifGreaterThanBody->IfThenElse(&thenPath, &elsePath, conditionLessThan);

   thenPath->Return(
   thenPath->   Load("sum"));

   elsePath->Return(
   elsePath->   Load("i"));

   doWhileBody->Store("sum",
   doWhileBody->   Add(
   doWhileBody->      Load("sum"),
   doWhileBody->      Load("bump")));
   doWhileBody->Store("i",
   doWhileBody->   Add(
   doWhileBody->      Load("i"),
   doWhileBody->      Load("bump")));
   doWhileBody->Store("exitCondition",
   doWhileBody->   LessThan(
   doWhileBody->      Load("i"),
   doWhileBody->      Load("final")));

   Return(ConstInt32(-1));

   return true;
   }

ForLoopContinueMethod::ForLoopContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("ForLoopContinue");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
ForLoopContinueMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *forBody = NULL;
   TR::IlBuilder *continueBuilder = NULL;
   TR::IlBuilder *thenPath = NULL;
   ForLoopWithContinue(true, "I", &forBody,
                       &continueBuilder,
                       ConstInt32(0),
                       Load("n"),
                       ConstInt32(1));

   TR::IlValue *continueCondition = forBody->EqualTo(
                                    forBody->   Load("I"),
                                    forBody->   ConstInt32(4));

   forBody->IfThen(&thenPath, continueCondition);

   thenPath->Store("Sum",
   thenPath->   Add(
   thenPath->      Load("Sum"),
   thenPath->      Load("I")));
   thenPath->Goto(&continueBuilder);

   forBody->Store("temp",
   forBody->   Add(
   forBody->      Load("I"),
   forBody->      Load("I")));
   forBody->Store("Sum",
   forBody->   Add(
   forBody->      Load("Sum"),
   forBody->      Load("temp")));

   Return(
      Load("Sum"));

   return true;
   }

ForLoopBreakMethod::ForLoopBreakMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("ForLoopBreak");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
ForLoopBreakMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *forBody = NULL;
   TR::IlBuilder *breakBody = NULL;
   TR::IlBuilder *thenPath = NULL;
   ForLoopWithBreak(true, "I", &forBody,
                    &breakBody,
                    ConstInt32(0),
                    Load("n"),
                    ConstInt32(1));

   TR::IlValue *breakCondition = forBody->EqualTo(
                                 forBody->   Load("I"),
                                 forBody->   ConstInt32(8));

   forBody->IfThen(&thenPath, breakCondition);

   thenPath->Store("Sum",
   thenPath->   Add(
   thenPath->      Load("Sum"),
   thenPath->      Load("I")));
   thenPath->Goto(&breakBody);

   forBody->Store("temp",
   forBody->   Add(
   forBody->      Load("I"),
   forBody->      Load("I")));
   forBody->Store("Sum",
   forBody->   Add(
   forBody->      Load("Sum"),
   forBody->      Load("temp")));

   Return(
      Load("Sum"));

   return true;
   }

ForLoopBreakAndContinueMethod::ForLoopBreakAndContinueMethod(TR::TypeDictionary *types, BuilderTest *test)
   : TR::MethodBuilder(types, test)
   {
   DefineLine(LINETOSTR(__LINE__));
   DefineFile(__FILE__);

   DefineName("ForLoopBreakAndContinue");
   DefineParameter("n", Int32);
   DefineReturnType(Int32);
   }

bool
ForLoopBreakAndContinueMethod::buildIL()
   {
   Store("Sum",
      ConstInt32(0));

   TR::IlBuilder *forBody = NULL;
   TR::IlBuilder *breakBuilder = NULL;
   TR::IlBuilder *continueBuilder = NULL;
   TR::IlBuilder *breakThenPath = NULL;
   TR::IlBuilder *continueThenPath = NULL;
   ForLoop(true, "I", &forBody, &breakBuilder, &continueBuilder,
           ConstInt32(0),
           Load("n"),
           ConstInt32(1));

   TR::IlValue *breakCondition = forBody->EqualTo(
                                 forBody->   Load("I"),
                                 forBody->   ConstInt32(8));

   forBody->IfThen(&breakThenPath, breakCondition);

   breakThenPath->Store("Sum",
   breakThenPath->   Add(
   breakThenPath->      Load("Sum"),
   breakThenPath->      Load("I")));
   breakThenPath->Goto(&breakBuilder);

   TR::IlValue *continueCondition = forBody->EqualTo(
                                    forBody->   Load("I"),
                                    forBody->   ConstInt32(4));

   forBody->IfThen(&continueThenPath, continueCondition);

   continueThenPath->Store("Sum",
   continueThenPath->   Add(
   continueThenPath->      Load("Sum"),
   continueThenPath->      Load("I")));
   continueThenPath->Goto(&continueBuilder);

   forBody->Store("temp",
   forBody->   Add(
   forBody->      Load("I"),
   forBody->      Load("I")));
   forBody->Store("Sum",
   forBody->   Add(
   forBody->      Load("Sum"),
   forBody->      Load("temp")));

   Return(
      Load("Sum"));

   return true;
   }
} // namespace TestCompiler

TEST(JITTest, BuilderTest)
   {
   ::TestCompiler::BuilderTest _builderTest;
   _builderTest.RunTest();
   }

TEST(JITILBuilderTest, ControlFlowTest)
   {
   ::TestCompiler::BuilderTest _controlFlowTest;
   _controlFlowTest.compileControlFlowTestMethods();
   _controlFlowTest.invokeControlFlowTests();
   }

TEST(JITILBuilderTest, NestedControlFlowLoopTest)
   {
   ::TestCompiler::BuilderTest _nestedControlFlowLoopTest;
   _nestedControlFlowLoopTest.compileNestedControlFlowLoopTestMethods();
   _nestedControlFlowLoopTest.invokeNestedControlFlowLoopTests();
   }
