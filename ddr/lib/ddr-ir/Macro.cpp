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

#include "ddr/config.hpp"

#include "ddr/error.hpp"
#include "ddr/ir/Macro.hpp"

#include "ddr/std/sstream.hpp"
#include <string.h>

using std::string;
using std::stringstream;

DDR_RC
Macro::getNumeric(long long *ret)
{
	DDR_RC rc = DDR_RC_OK;
	string value = getValue();

	if (value.length() > 0) {
		/* If the pre-processed macro contains no brackets, read it as a number. */
		if (string::npos == value.find('(')) {
			stringstream ss;
			ss << value;
			long long valueNumeric = 0;
			ss >> valueNumeric;
			if (ss.fail()) {
				rc = DDR_RC_ERROR;
			} else if (NULL != ret) {
				*ret = valueNumeric;
			}
		} else {
			/* For macros containing brackets, extract the number. This works for casts
			 * such as ((int)5).
			 */
			rc = DDR_RC_ERROR;
			while (string::npos != value.find('(')) {
				size_t lastOpenParen = value.find_last_of('(');
				size_t nextCloseParen = value.find(')', lastOpenParen);
				if (string::npos == nextCloseParen) {
					break;
				}
				string substr = value.substr(lastOpenParen + 1, nextCloseParen - lastOpenParen - 1);
				stringstream ss;
				ss << substr;
				long long valueNumeric = 0;
				ss >> valueNumeric;
				if (!ss.fail()) {
					rc = DDR_RC_OK;
					if (NULL != ret) {
						*ret = valueNumeric;
					}
					break;
				} else {
					value.replace(lastOpenParen, nextCloseParen, "");
				}
			}
		}
	}

	return rc;
}
