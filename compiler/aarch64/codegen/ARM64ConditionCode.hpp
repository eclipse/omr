/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifndef ARM64CONDITIONCODE_INCL
#define ARM64CONDITIONCODE_INCL

namespace TR {

/*
 * Condition codes used in B.cond and other instructions
 */
typedef enum {
   CC_EQ = 0,
   CC_NE,
   CC_CS,
   CC_CC,
   CC_MI,
   CC_PL,
   CC_VS,
   CC_VC,
   CC_HI,
   CC_LS,
   CC_GE,
   CC_LT,
   CC_GT,
   CC_LE,
   CC_AL,
   CC_Illegal
} ARM64ConditionCode;

} // TR

/**
 * @brief Returns inverted condition code
 * @param[in] cc : condition code
 * @return inverted condition code
 */
inline TR::ARM64ConditionCode cc_invert(TR::ARM64ConditionCode cc) { return (TR::ARM64ConditionCode)(cc ^ 1); }

#endif
