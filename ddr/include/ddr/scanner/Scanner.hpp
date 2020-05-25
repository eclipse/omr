/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#ifndef SCANNER_HPP
#define SCANNER_HPP

#include "ddr/error.hpp"
#include "ddr/std/string.hpp"

#include "omrport.h"

#include <set>
#include <vector>

class ClassUDT;
class EnumUDT;
class NamespaceUDT;
class Symbol_IR;
class Type;
class TypedefUDT;
class UnionUDT;

using std::set;
using std::string;
using std::vector;

class Scanner
{
public:
	virtual DDR_RC startScan(OMRPortLibrary *portLibrary, Symbol_IR *ir,
			vector<string> *debugFiles, const char *blacklistPath) = 0;

protected:
	set<string> _blacklistedFiles;
	set<string> _blacklistedTypes;

	bool checkBlacklistedType(const string &name) const;
	bool checkBlacklistedFile(const string &name) const;
	DDR_RC loadBlacklist(OMRPortLibrary *portLibrary, const char *file);
};

#endif /* SCANNER_HPP */
