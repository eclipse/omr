/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "ddr/config.hpp"

#include <set>
#include "ddr/std/string.hpp"
#include <vector>

#include "omrport.h"

#include "ddr/error.hpp"

class Symbol_IR;
class Type;
class EnumUDT;
class NamespaceUDT;
class TypedefUDT;
class ClassUDT;
class UnionUDT;

using std::set;
using std::string;
using std::vector;

class Scanner
{
public:
	virtual DDR_RC startScan(OMRPortLibrary *portLibrary, Symbol_IR *const ir, vector<string> *debugFiles, string blacklistPath) = 0;
	
protected:
	set<string> _blacklistedFiles;
	set<string> _blacklistedTypes;

	bool checkBlacklistedType(string name);
	bool checkBlacklistedFile(string name);
	DDR_RC loadBlacklist(string file);
};

#endif /* SCANNER_HPP */
