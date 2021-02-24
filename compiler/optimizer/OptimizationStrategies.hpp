/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef OPTIMIZATIONSTRATEGIES_INCL
#define OPTIMIZATIONSTRATEGIES_INCL

#include <stdint.h>
#include "optimizer/Optimizations.hpp"

struct OptimizationStrategy
   {
   OMR::Optimizations _num;
   uint16_t      _options;
   };
//optimization groups

extern const OptimizationStrategy loopAliasRefinerOpts[];
extern const OptimizationStrategy arrayPrivatizationOpts[];
extern const OptimizationStrategy reorderArrayIndexOpts[];
extern const OptimizationStrategy localValuePropagationOpts[];
extern const OptimizationStrategy cheapObjectAllocationOpts[];
extern const OptimizationStrategy expensiveObjectAllocationOpts[];
extern const OptimizationStrategy eachEscapeAnalysisPassOpts[];
extern const OptimizationStrategy veryCheapGlobalValuePropagationOpts[];
extern const OptimizationStrategy cheapGlobalValuePropagationOpts[];
extern const OptimizationStrategy expensiveGlobalValuePropagationOpts[];
extern const OptimizationStrategy eachExpensiveGlobalValuePropagationOpts[];
extern const OptimizationStrategy veryExpensiveGlobalValuePropagationOpts[];
extern const OptimizationStrategy partialRedundancyEliminationOpts[];
extern const OptimizationStrategy isolatedStoreOpts[];
extern const OptimizationStrategy globalDeadStoreOpts[];
extern const OptimizationStrategy finalGlobalOpts[];
extern const OptimizationStrategy loopSpecializerOpts[];
extern const OptimizationStrategy loopVersionerOpts[];
extern const OptimizationStrategy lastLoopVersionerOpts[];
extern const OptimizationStrategy loopCanonicalizationOpts[];
extern const OptimizationStrategy blockManipulationOpts[];
extern const OptimizationStrategy eachLocalAnalysisPassOpts[];
extern const OptimizationStrategy stripMiningOpts[];
extern const OptimizationStrategy methodHandleInvokeInliningOpts[];

//arrays of optimizations

extern const OptimizationStrategy * omrCompilationStrategies[];

#endif
