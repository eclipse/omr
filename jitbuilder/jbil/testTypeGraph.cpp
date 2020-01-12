/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include <stdio.h>
#include <sstream>
#include "Action.hpp"
#include "TypeDictionary.hpp"
#include "TypeGraph.hpp"

using namespace OMR::JitBuilder;

class Tester
   {
   public:
      Tester(bool verbose=false)
         : _total(0)
         , _pass(0)
         , _verbose(verbose)
         { }
      void report()
         {
         if (_pass == _total)
            std::cout << "Passed all tests!\n";
         else
            std::cout << "Failed some test\n";
         }

   protected:
      bool performTest(std::ostringstream & oss)
         {
         if (_verbose)
            std::cout << "[ " << _total << " ] Performing " << oss.str() << "\n";
         return true;
         }

      int _total;
      int _pass;
      bool _verbose;
   };

class TypeGraphTester : public Tester
   {
public :
   TypeGraphTester(bool verbose=false)
      : Tester(verbose)
      { }

   Type ** testAllUnaryTypeCombinations(TypeGraph & g, Action a, int numTypes, Type ** types)
      {
      Type **results = new Type *[numTypes];
      for (int t = 0;t < numTypes; t++)
         {
         results[t] = g.producedType(a, types[t]);
         }
      return results;
      }

   Type *** testAllBinaryTypeCombinations(TypeGraph & g, Action a, int numTypes, Type ** types)
      {
      Type ***results = new Type **[numTypes];
      for (int t1 = 0;t1 < numTypes;t1++)
         {
         results[t1] = new Type *[numTypes];
         for (int t2 = 0;t2 < numTypes; t2++)
            {
            results[t1][t2] = g.producedType(a, types[t1], types[t2]);
            }
         }
      return results;
      }

   int countProducedTypes(int numTypes, Type ** results)
      {
      int numNonNullResults = 0;
      for (int t = 0;t < numTypes;t++)
         {
         numNonNullResults += (results[t] != NULL) ? 1 : 0;
         }
      return numNonNullResults;
      }

   int countProducedTypes(int numTypes, Type *** results)
      {
      int numNonNullResults = 0;
      for (int t1 = 0;t1 < numTypes;t1++)
         {
         for (int t2 = 0;t2 < numTypes; t2++)
            {
            numNonNullResults += (results[t1][t2] != NULL) ? 1 : 0;
            }
         }
      return numNonNullResults;
      }

   void testSingleTypeProducedBinaryActions()
      {
      std::cout << "Binary operation tests:\n";

      int pass=0;
      int total=0;
      TypeDictionary types;

      Action actions[] = {
                         aAdd,
                         aSub,
                         aMul
                         };
      const int numActions = sizeof(actions) / sizeof (Action);
      const char *actionName[] = {
                                  "Add",
                                  "Sub",
                                  "Mul"
                                  };
      const int numActionNames = sizeof(actionName) / sizeof (const char *);
      static_assert(numActionNames == numActions, "wrong number of action names");

      Type * baseTypes[] = {
                           types.NoType,
                           types.Int8,
                           types.Int16,
                           types.Int32,
                           types.Int64,
                           types.Float,
                           types.Double
                           };
      const int numTypes = sizeof(baseTypes) / sizeof (Type *);
   
      for (int aIdx = 0;aIdx < numActions;aIdx++)
         {
         Action a = actions[aIdx];
         for (int t1Idx=0;t1Idx < numTypes;t1Idx++)
            {
            Type * t1 = baseTypes[t1Idx];
            for (int t2Idx=0;t2Idx < numTypes; t2Idx++)
               {
               Type * t2 = baseTypes[t2Idx];
               for (int t3Idx=0;t3Idx < numTypes; t3Idx++)
                  {
                  Type * t3 = baseTypes[t3Idx];

                  std::ostringstream oss;
                  oss << "Test " << t1->name() << " <- " << actionName[aIdx] << "( " << t2->name() << ", " << t3->name() << " )";
                  if (performTest(oss))
                     {
                     // initialize a graph with a set of types
                     TypeGraph g(&types);
                     for (int tt=0;tt < numTypes;tt++)
                        g.registerType(baseTypes[tt]);
   
                     // permit only one possible combination for this action and type
                     g.registerValidOperation(t1, a, t2, t3);
   
                     // test all combinations and capture results
                     Type *** r = testAllBinaryTypeCombinations(g, a, numTypes, baseTypes);
   
                     // validate only one result possible and that it's the one we expect
                     int c = countProducedTypes(numTypes, r);
                     if (c == 1 && r[t2Idx][t3Idx] == t1)
                        _pass++;
                     else
                        {
                        std::cout << "Fail: c is " << c << " (expecting 1) and produced type is ";
                        if (r[t2Idx][t3Idx])
                           std::cout << r[t2Idx][t3Idx]->name() << " (expecting " << t1->name() << ")\n";
                        else
                           std::cout << "nullptr (expecting " << t1->name() << ")\n";
                        }

                     _total++;

                     // release the results array
                     for (int tt=0;tt < numTypes;tt++)
                       delete[] r[tt];
                     delete[] r;
                     }
                  }
               }
            }
         }
      }

   void testSingleTypeProducedUnaryActions()
      {
      std::cout << "Unary operation tests:\n";

      int pass=0;
      int total=0;
      TypeDictionary types;

      Action actions[] = {
                         aStore,
                         aIfThenElse,
                         aSwitch,
                         };
      const int numActions = sizeof(actions) / sizeof (Action);
      const char *actionName[] = {
                                  "Store",
                                  "IfThenElse",
                                  "Switch"
                                  };
      const int numActionNames = sizeof(actionName) / sizeof (const char *);
      static_assert(numActionNames == numActions, "wrong number of action names");

      Type * baseTypes[] = {
                           types.NoType,
                           types.Int8,
                           types.Int16,
                           types.Int32,
                           types.Int64,
                           types.Float,
                           types.Double
                           };
      const int numTypes = sizeof(baseTypes) / sizeof (Type *);
   
      for (int aIdx = 0;aIdx < numActions;aIdx++)
         {
         Action a = actions[aIdx];
         for (int t1Idx=0;t1Idx < numTypes;t1Idx++)
            {
            Type * t1 = baseTypes[t1Idx];
            for (int t2Idx=0;t2Idx < numTypes; t2Idx++)
               {
               Type * t2 = baseTypes[t2Idx];

               std::ostringstream oss;
               oss << "Test " << t1->name() << " <- " << actionName[aIdx] << "( " << t2->name() << " )";
               if (performTest(oss))
                  {
                  // initialize a graph with a set of types
                  TypeGraph g(&types);
                  for (int tt=0;tt < numTypes;tt++)
                     g.registerType(baseTypes[tt]);

                  // permit only one possible combination for this action and type
                  g.registerValidOperation(t1, a, t2);

                  // test all combinations and capture results
                  Type ** r = testAllUnaryTypeCombinations(g, a, numTypes, baseTypes);

                  // validate only one result possible and that it's the one we expect
                  int c = countProducedTypes(numTypes, r);
                  if (c == 1 && r[t2Idx] == t1)
                     _pass++;
                  else
                     {
                     std::cout << "Fail: c is " << c << " (expecting 1) and produced type is ";
                     if (r[t2Idx])
                        std::cout << r[t2Idx]->name() << " (expecting " << t1->name() << ")\n";
                     else
                        std::cout << "nullptr (expecting " << t1->name() << ")\n";
                     }

                  _total++;

                  // release the results array
                  delete[] r;
                  }
               }
            }
         }
      }
   };

int
main(int argc, char *argv[])
   {
   bool verbose=false;
   for (int a=1; a < argc; a++)
      {
      if (strncmp(argv[a], "-verbose", strlen("-verbose")) == 0)
         verbose = true;
      else
         {
         std::cerr << "Error: unrecognized option " << argv[a] << ", aborting without running tests\n";
         exit(-1);
         }
      }

   TypeGraphTester t(verbose);

   t.testSingleTypeProducedBinaryActions();
   t.testSingleTypeProducedUnaryActions();

   t.report();
   }
