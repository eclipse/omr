/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

#ifndef ILVALIDATIONSTRATEGIES_HPP
#define ILVALIDATIONSTRATEGIES_HPP

#include <stdint.h>

namespace OMR {

enum ILValidationRule
   {
   soundnessRule,
   validateChildCount,
   validateChildTypes,
   validateLivenessBoundaries,
   validateNodeRefCountWithinBlock,
   validate_ireturnReturnType,
   validate_axaddEnvironment,
   /**
    * NOTE: Please add `id`s for any new ILValidationRule here!
    *       This needs to match the implementation of said *ILValidationRule.
    *       See ILValidationRules.cpp for relevant examples.
    */

   /* Used to mark the end of an ILValidationStrategy Array. */
   endRules
   };

struct ILValidationStrategy
   {
   OMR::ILValidationRule        id;
   };

extern const ILValidationStrategy emptyStrategy[];

extern const ILValidationStrategy postILgenValidatonStrategy[];

extern const ILValidationStrategy preCodegenValidationStrategy[];

} //namespace OMR

namespace TR {

enum ILValidationContext
   {
   noValidation,
   preCodegenValidation,
   postILgenValidation
   /* NOTE: Please add any new ILValidationContext here! */
   };

extern const OMR::ILValidationStrategy *omrValidationStrategies[];

} // namespace TR

#endif
