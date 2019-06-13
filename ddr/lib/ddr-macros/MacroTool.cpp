/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

#include "ddr/macros/MacroTool.hpp"

#include "ddr/ir/NamespaceUDT.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/TextFile.hpp"

#define FileBeginEyeCatcher "@DDRFILE_BEGIN "
#define FileEndEyeCatcher "@DDRFILE_END "
#define MacroEyeCatcher "@MACRO_"
#define TypeEyeCatcher "@TYPE_"

#define LITERAL_STRLEN(s) (sizeof(s) - 1)

static bool
startsWith(const string &str, const char *prefix, size_t prefix_len)
{
	return 0 == str.compare(0, prefix_len, prefix, prefix_len);
}

/**
 * This function reads in a formatted file (fname) full of macro names, their
 * values and their associated types. This information is used to fill in a
 * vector of structures (MacroInfo) amalgamating macros with their associated
 * type name.
 *
 * @return DDR_RC_OK on failure, DDR_RC_ERROR if an error is encountered.
 */
DDR_RC
MacroTool::getMacros(OMRPortLibrary *portLibrary, const char *filename)
{
	if ('\0' == *filename) {
		ERRMSG("invalid macrolist file name");
		return DDR_RC_ERROR;
	}

	TextFile macrolist(portLibrary);

	if (!macrolist.openRead(filename)) {
		ERRMSG("cannot open macrolist file %s", filename);
		return DDR_RC_ERROR;
	}

	set<string> includeFileSet;
	string line;

	/* Parse the input file for macros with values. */
	while (macrolist.readLine(line)) {
		if (startsWith(line, FileBeginEyeCatcher, LITERAL_STRLEN(FileBeginEyeCatcher))) {
			const string fileName = line.substr(LITERAL_STRLEN(FileBeginEyeCatcher));
			if (includeFileSet.end() == includeFileSet.find(fileName)) {
				includeFileSet.insert(fileName);
			} else {
				/* This include file was already processed.
				 * Scan until the file end delimiter is found.
				 */
				while (macrolist.readLine(line)) {
					if (startsWith(line, FileEndEyeCatcher, LITERAL_STRLEN(FileEndEyeCatcher))) {
						break;
					}
				}
			}
		} else if (startsWith(line, TypeEyeCatcher, LITERAL_STRLEN(TypeEyeCatcher))) {
			const string typeName = line.substr(LITERAL_STRLEN(TypeEyeCatcher));
			MacroInfo info(typeName);
			macroList.push_back(info);
		} else if (startsWith(line, MacroEyeCatcher, LITERAL_STRLEN(MacroEyeCatcher))) {
			const char * const whitespace = "\t ";
			const size_t pos = line.find_first_of(whitespace);
			if ((string::npos == pos) || (pos <= LITERAL_STRLEN(MacroEyeCatcher))) {
				// input malformed: no space
			} else {
				/* omit leading whitespace */
				size_t start = line.find_first_not_of(whitespace, pos);

				if (string::npos != start) {
					/* omit trailing whitespace */
					size_t end = line.find_last_not_of(whitespace);

					const string name = line.substr(LITERAL_STRLEN(MacroEyeCatcher), pos - LITERAL_STRLEN(MacroEyeCatcher) );
					const string value = line.substr(start, end - start + 1);

					macroList.back().addMacro(name, value);
				}
			}
		}
	}

	macrolist.close();

	return DDR_RC_OK;
}

DDR_RC
MacroTool::addMacrosToIR(Symbol_IR *ir) const
{
	DDR_RC rc = DDR_RC_OK;

	/* Create a map of name to IR Type for all types in the IR which
	 * can contain macros. This is used to add macros to existing types.
	 */
	unordered_map<string, Type *> irMap;
	for (std::vector<Type *>::const_iterator it = ir->_types.begin(); it != ir->_types.end(); ++it) {
		Type *type = *it;

		if (type->isAnonymousType()) {
			continue;
		} else if (type->getSymbolKindName().empty()) {
			continue;
		} else {
			const string &typeName = type->_name;

			if (irMap.end() == irMap.find(typeName)) {
				irMap[typeName] = type;
			} else {
				ERRMSG("Duplicate type name: %s", typeName.c_str());
				return DDR_RC_ERROR;
			}
		}
	}

	for (std::vector<MacroInfo>::const_iterator it = macroList.begin(); it != macroList.end(); ++it) {
		/* For each found MacroInfo which contains macros, add the macros to the IR. */
		const MacroInfo &macroInfo = *it;
		if (macroInfo.getNumMacros() > 0) {
			Type *outerType = NULL;
			/* If there is a type of the correct name already, use it.
			 * Otherwise, create a new namespace to contain the macros.
			 */
			const string &typeName = macroInfo.getTypeName();
			unordered_map<string, Type *>::iterator mapIt = irMap.find(typeName);
			if (irMap.end() != mapIt) {
				outerType = mapIt->second;
			} else {
				outerType = new NamespaceUDT;
				outerType->_name = typeName;
				ir->_types.push_back(outerType);
				irMap[typeName] = outerType;
			}

			if (NULL == outerType) {
				ERRMSG("Cannot find or create UDT to add macro");
				rc = DDR_RC_ERROR;
				break;
			} else {
				for (set<pair<string, string> >::const_iterator it = macroInfo.getMacroStart(); it != macroInfo.getMacroEnd(); ++it) {
					outerType->addMacro(new Macro(it->first, it->second));
				}
			}
		}
	}

	return rc;
}
