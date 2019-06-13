/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "ddr/ir/TextFile.hpp"

#include "omrport.h"

/*
 * Open the named file for reading.
 * Returns true if the file was newly opened; false otherwise.
 */
bool
TextFile::openRead(const char *filename)
{
	if (_file < 0) {
		OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

		_file = omrfile_open(filename, EsOpenRead, 0);

		if (_file >= 0) {
			return true;
		}
	}

	return false;
}

/*
 * Read a line of text from this file into 'line'; excluding any line terminators.
 * Returns true if any text was read (even a blank line); false otherwise.
 */
bool
TextFile::readLine(std::string &line)
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	char buffer[200];

	line.clear();

	while (NULL != omrfile_read_text(_file, buffer, sizeof(buffer))) {
		for (size_t index = 0;; ++index) {
			char ch = buffer[index];

			if (('\0' == ch) || ('\n' == ch) || ('\r' == ch)) {
				if (0 != index) {
					line.append(buffer, index);
				}
				if ('\0' == ch) {
					break;
				} else {
					return true;
				}
			}
		}
	}

	return false;
}

/*
 * Close this file. This has no effect if the file was not opened
 * or has already been closed.
 */
void
TextFile::close()
{
	if (_file >= 0) {
		OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

		omrfile_close(_file);
		_file = -1;
	}
}
