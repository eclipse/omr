/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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

#ifndef OPTTESTDRIVER_HPP
#define OPTTESTDRIVER_HPP

#include <stdint.h>
#include <vector>
#include "gtest/gtest.h"
#include "optimizer/Optimizations.hpp"
#include "optimizer/OptimizationStrategies.hpp"
#include "TestDriver.hpp"

namespace TestCompiler { class MethodInfo; }
namespace TR { class IlVerifier; }

namespace TestCompiler
{

class OptTestDriver : public TestDriver, public ::testing::Test
   {
   public:
   OptTestDriver();
   ~OptTestDriver();

   void setMethodInfo(TestCompiler::MethodInfo *methodInfo) { _methodInfo = methodInfo; }
   void setIlVerifier(TR::IlVerifier *ilVer) { _ilVer = ilVer; }

   typedef std::vector<OptimizationStrategy> OptVector;

   void addOptimization(OMR::Optimizations opt);
   void addOptimizations(const OptimizationStrategy *opts);

   void Verify();
   void VerifyAndInvoke();

   virtual void compileTestMethods();

   /**
    * Returns the method most recently compiled, cast to type `T`.
    */
   template<typename T> T getCompiledMethod() const { return reinterpret_cast<T>(_compiledMethod); }

   private:
   void makeOptimizationStrategyArray(OptimizationStrategy *strategy);
   void SetupStrategy();

   uint8_t *_compiledMethod;
   TestCompiler::MethodInfo *_methodInfo;
   TR::IlVerifier  *_ilVer;
   std::vector<OptimizationStrategy> _optimizations;
   };

}// namespace TestCompiler

#endif
