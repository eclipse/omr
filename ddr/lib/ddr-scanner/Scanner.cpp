/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "ddr/config.hpp"

#include "ddr/scanner/Scanner.hpp"

#include <fstream>
#include <iostream>
#include <stdio.h>

using std::ios;
using std::ifstream;

static bool
checkStringMatch(string checked, set<string> *patterns)
{
	bool match = patterns->find(checked) != patterns->end();

	if (!match) {
		for (set<string>::iterator it = patterns->begin(); it != patterns->end(); it ++) {
			size_t wildcardPos = 0;
			size_t lastWildcardPos = 0;
			size_t lastSequencePos = 0;
			bool matchFound = false;
			while ((string::npos != wildcardPos) && (it->length() > wildcardPos)) {
				wildcardPos = it->find("*", lastWildcardPos);
				if (string::npos == wildcardPos) {
					wildcardPos = it->length();
				}
				string sequence = it->substr(lastWildcardPos, wildcardPos - lastWildcardPos);
				size_t sequencePos = checked.find(sequence);
				matchFound = sequence.empty()
					|| (0 == lastWildcardPos && 0 == sequencePos)
					|| ((string::npos != sequencePos) && (sequencePos > lastSequencePos) && (0 != lastWildcardPos));
				matchFound = matchFound
					&& (sequencePos + sequence.length() == checked.length()
					|| wildcardPos < it->length()
					|| sequence.empty());
				if (!matchFound) {
					break;
				}
				lastWildcardPos = wildcardPos + 1;
				lastSequencePos = sequencePos;
			}
			if (matchFound) {
				match = true;
				break;
			}
		}
	}
	return match;
}

bool
Scanner::checkBlacklistedType(string name)
{
	return checkStringMatch(name, &_blacklistedTypes);
}

bool
Scanner::checkBlacklistedFile(string name)
{
	return checkStringMatch(name, &_blacklistedFiles);
}

DDR_RC
Scanner::loadBlacklist(string path)
{
	/* Load the blacklist file. The blacklist contains a list of types
	 * to ignore and not add to the IR, such as system types.
	 */
	DDR_RC rc = DDR_RC_OK;
	string line = "";
	ifstream blackListInput(path.c_str(), ios::in);

	if (blackListInput.is_open()) {
		while (getline(blackListInput, line)) {
			string pattern = line.substr(5, line.length() - (string::npos == line.find_last_of("\n") ? 5 : 6));
			if (0 == line.find("file:")) {
				_blacklistedFiles.insert(pattern);
			} else if (0 == line.find("type:")) {
				_blacklistedTypes.insert(pattern);
			}
		}
	} else {
		ERRMSG("Could not open blackList.txt");
		rc = DDR_RC_ERROR;
	}
	blackListInput.close();

	return rc;
}
