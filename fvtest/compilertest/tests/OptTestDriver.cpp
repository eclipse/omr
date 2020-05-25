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

#include "OptTestDriver.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "optimizer/Optimizer.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "ilgen/IlInjector.hpp"
#include "ras/IlVerifierHelpers.hpp"
#include "ilgen/MethodInfo.hpp"

TestCompiler::OptTestDriver::OptTestDriver() : _compiledMethod(NULL), _methodInfo(NULL), _ilVer(NULL)
   {
   }

TestCompiler::OptTestDriver::~OptTestDriver()
   {
   TR::Optimizer::setMockStrategy(NULL);
   }

/**
 * Fill the array \p strategy with optimizations.
 *
 * @param[out] strategy An array with at least
 * `_optimization.size() + 1` elements.
 */
void TestCompiler::OptTestDriver::makeOptimizationStrategyArray(OptimizationStrategy *strategy)
   {
   for(unsigned int i = 0; i < _optimizations.size(); ++i)
      {
      strategy[i]._num = _optimizations[i]._num;
      strategy[i]._options = _optimizations[i]._options;
      }

   strategy[_optimizations.size()]._num = OMR::endOpts;
   strategy[_optimizations.size()]._options = 0;
   }

/**
 * Similar to VerifyAndInvoke(), however the IL is not compiled
 * after IL verification, and tests are not invoked.
 */
void TestCompiler::OptTestDriver::Verify()
   {
   OptimizationStrategy *strategy = new OptimizationStrategy[_optimizations.size() + 1];
   makeOptimizationStrategyArray(strategy);
   TR::Optimizer::setMockStrategy(strategy);

   // To stop before codegen, throw an exception at the end of IL verification.
   TR::IlVerifier *oldVerifier = _ilVer;
   TR::NoCodegenVerifier noCodegenVerifier(oldVerifier);
   setIlVerifier(&noCodegenVerifier);

   compileTestMethods();

   setIlVerifier(oldVerifier);
   TR::Optimizer::setMockStrategy(NULL);
   delete[] strategy;

   ASSERT_EQ(true, noCodegenVerifier.hasRun()) << "Did not run verifiers.";
   }

/**
 * This generates IL, runs the requested optimizations,
 * verifies the optimized IL, compiles it, then invokes tests.
 *
 * Must be called after setMethodInfo() and setIlVerifier().
 * Should be called after all requested optimizations are
 * added via addOptimization().
 */
void TestCompiler::OptTestDriver::VerifyAndInvoke()
   {
   OptimizationStrategy *strategy = new OptimizationStrategy[_optimizations.size() + 1];
   makeOptimizationStrategyArray(strategy);
   TR::Optimizer::setMockStrategy(strategy);

   RunTest();

   TR::Optimizer::setMockStrategy(NULL);
   delete[] strategy;
   }

/**
 * Append a single optimization to the list of optimizations to perform.
 * The optimization is marked as `MustBeDone`.
 *
 * @param opt The optimization to perform.
 */
void TestCompiler::OptTestDriver::addOptimization(OMR::Optimizations opt)
   {
   OptimizationStrategy strategy = {opt, OMR::MustBeDone};
   _optimizations.push_back(strategy);
   }

/**
 * Append an optimization strategy to the list of optimizations to perform.
 *
 * @param opts An array of optimizations to perform. The last item in this
 * array must be `endOpts` or `endGroup`.
 */
void TestCompiler::OptTestDriver::addOptimizations(const OptimizationStrategy *opts)
   {
   const OptimizationStrategy *end = opts;
   while(end->_num != OMR::endOpts && end->_num != OMR::endGroup)
      ++end;

   _optimizations.insert(_optimizations.end(), opts, end);
   }

/**
 * @brief Compiles the method.
 *
 * This is called by Verify() and VerifiyAndInvoke().
 * The default implementation gets a #ResolvedMethod from #MethodInfo,
 * adds the IlVerifier, then compiles the method.
 */
void TestCompiler::OptTestDriver::compileTestMethods()
   {
   TR::ResolvedMethod resolvedMethod = _methodInfo->ResolvedMethod();
   TR::IlGeneratorMethodDetails details(&resolvedMethod);
   details.setIlVerifier(_ilVer);

   int32_t rc = 0;
   _compiledMethod = compileMethod(details, warm, rc);
   }
