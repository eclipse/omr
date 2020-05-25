/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "JitTest.hpp"

namespace testing {
namespace internal {

template<>
AssertionResult CmpHelperEQ<float, float>(const char* lhs_expression,
                            const char* rhs_expression,
                            const float& lhs,
                            const float& rhs) {
  if ((std::isnan(lhs) && std::isnan(rhs)) || (lhs == rhs)) {
    return AssertionSuccess();
  }
  return CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
}

template<>
AssertionResult CmpHelperEQ<volatile float, volatile float>(const char* lhs_expression,
                            const char* rhs_expression,
                            const volatile float& lhs,
                            const volatile float& rhs) {
  if ((std::isnan(lhs) && std::isnan(rhs)) || (lhs == rhs)) {
    return AssertionSuccess();
  }
  return CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
}

template<>
AssertionResult CmpHelperEQ<double, double>(const char* lhs_expression,
                            const char* rhs_expression,
                            const double& lhs,
                            const double& rhs) {
  if ((std::isnan(lhs) && std::isnan(rhs)) || (lhs == rhs)) {
    return AssertionSuccess();
  }
  return CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
}

template<>
AssertionResult CmpHelperEQ<volatile double, volatile double>(const char* lhs_expression,
                            const char* rhs_expression,
                            const volatile double& lhs,
                            const volatile double& rhs) {
  if ((std::isnan(lhs) && std::isnan(rhs)) || (lhs == rhs)) {
    return AssertionSuccess();
  }
  return CmpHelperEQFailure(lhs_expression, rhs_expression, lhs, rhs);
}

} // namespace internal
} // namespace testing
