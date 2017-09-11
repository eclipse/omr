/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

#ifndef TEST_OPTIMIZER_INCL
#define TEST_OPTIMIZER_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef TEST_OPTIMIZER_CONNECTOR
#define TEST_OPTIMIZER_CONNECTOR
namespace TestCompiler { class Optimizer; }
namespace TestCompiler { typedef TestCompiler::Optimizer OptimizerConnector; }
#endif

#include "optimizer/OMROptimizer.hpp"

#include <stddef.h>                    // for NULL
#include <stdint.h>                    // for uint16_t

namespace TR { class Compilation; }
namespace TR { class Optimizer; }
namespace TR { class ResolvedMethodSymbol; }
struct OptimizationStrategy;

namespace TestCompiler {

class Optimizer : public OMR::OptimizerConnector
   {
   public:

   Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
         const OptimizationStrategy *strategy = NULL, uint16_t VNType = 0) :
      OMR::OptimizerConnector(comp, methodSymbol, isIlGen, strategy, VNType) {}

   static const OptimizationStrategy *optimizationStrategy(TR::Compilation *c);

   /**
    * Override what #OptimizationStrategy to use. While this has the same
    * functionality as the `optTest=` parameter, it does not require
    * reinitializing the JIT, nor manually changing the optFile.
    *
    * @param strategy The #OptimizationStrategy to return when requested.
    */
   static void setMockStrategy(const OptimizationStrategy *strategy) { _mockStrategy = strategy; };

   private:
   static const OptimizationStrategy *_mockStrategy;
   };

}

#endif
