/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef SELECT_OPT_INCL
#define SELECT_OPT_INCL

#pragma once

#include "control/OMROptions.hpp"
#include "optimizer/OptimizationManager.hpp" 

namespace TR 
{

/**
 * Implementation of an optimization selection facility.
 *
 * \tparam E Enum defined by TR_CompilationOptions
 * \tparam C1 Optimization to be created if getOption(E) is true.
 * \tparam C2 Optimization to be created if getOption(E) is false.
 * 
 * Selection is controlled by a TR_CompilationOptions enum.
 *
 * This API is intended to be used in the Optimizer.
 * When optimizations are being defined and placed in the `_opts` array,
 * one can use this API to switch between two optimizations.
 * A use of this API would look like:
 * 
 *    _opts[OMR::someOpt] = new (comp->allocator())
 *       TR::OptimizationManager(self(),
 *       TR::SelectOpt<SomeOption, Opt1, Opt2>::create, OMR::someOpt);
 *
 */
template<enum TR_CompilationOptions E, class C1, class C2>
class SelectOpt
   {
   public:

   static TR::Optimization *
   create(TR::OptimizationManager *m)
      {
      return TR::Options::getJITCmdLineOptions()->getOption(E) ? C1::create(m) : C2::create(m);
      }

   };

} // namespace TR

#endif
