/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef OPCODETEST_HPP
#define OPCODETEST_HPP

#include "JitTest.hpp"

#include <string>
#include <vector>
#include <iterator>
#include <limits>

namespace TRTest
{

// C++11 upgrade (Issue #1916).
template <typename Ret, typename Left, typename Right>
struct BinaryOpParamStruct {
        Left lhs;
        Right rhs;
        std::string opcode;
        Ret (*oracle)(Left, Right);
};

// C++11 upgrade (Issue #1916).
template <typename Ret, typename T>
struct UnaryOpParamStruct {
        T value;
        std::string opcode;
        Ret (*oracle)(T);
};


/**
 * @brief Given an instance of UnaryOpParamType, returns an equivalent instance
 *    of UnaryOpParamStruct
 */
template <typename Ret, typename T>
UnaryOpParamStruct<Ret, T> to_struct(std::tuple<T , std::tuple<std::string, Ret (*)(T)>> param) {
    UnaryOpParamStruct<Ret, T> s;
    s.value  = std::get<0>(param);
    s.opcode = std::get<0>(std::get<1>(param));
    s.oracle = std::get<1>(std::get<1>(param));
    return s;
}
/**
 * @brief Given an instance of BinaryOpParamType, returns an equivalent instance
 *    of BinaryOpParamStruct
 */
template <typename Ret, typename Left, typename Right>
BinaryOpParamStruct<Ret, Left, Right> to_struct(std::tuple<std::tuple<Left,Right>, std::tuple<std::string, Ret (*)(Left,Right)>> param) {
    BinaryOpParamStruct<Ret, Left, Right> s;
    s.lhs = std::get<0>(std::get<0>(param));
    s.rhs = std::get<1>(std::get<0>(param));
    s.opcode = std::get<0>(std::get<1>(param));
    s.oracle = std::get<1>(std::get<1>(param));
    return s;
}

//~ Opcode test fixtures ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <typename Ret, typename... Args>
class OpCodeTest : public JitTest, public ::testing::WithParamInterface< std::tuple<std::tuple<Args...>, std::tuple<std::string, Ret (*)(Args...)>> > {};

template <typename T>
class BinaryOpTest : public JitTest, public ::testing::WithParamInterface< std::tuple< std::tuple<T,T>, std::tuple<std::string, T (*)(T,T)>> > {};

template <typename Ret, typename T = Ret>
class UnaryOpTest : public JitTest, public ::testing::WithParamInterface< std::tuple< T , std::tuple<std::string, Ret (*)(T)>> > {};


} // namespace CompTest

#endif // OPCODETEST_HPP
