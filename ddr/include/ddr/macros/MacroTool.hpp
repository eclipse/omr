/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#ifndef MACROTOOL_HPP
#define MACROTOOL_HPP

#include "ddr/std/string.hpp"

#include "ddr/config.hpp"
#include "ddr/macros/MacroInfo.hpp"
#include "ddr/ir/Symbol_IR.hpp"

using std::string;
using std::vector;

class MacroTool
{
private:
	vector<MacroInfo> macroList;

	string getTypeName(string s);
	pair<string, string> getMacroInfo(string s);
	string getFileName(string s);

public:
	DDR_RC getMacros(string fname);
	DDR_RC addMacrosToIR(Symbol_IR *ir);
};

#endif /* MACROTOOL_HPP */
