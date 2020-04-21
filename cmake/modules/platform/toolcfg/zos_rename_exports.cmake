###############################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

# Inputs:
# LIBRARY_FILE_NAME - name of the generated dll file (no path)
# BINARY_DIR - CMake Binary Dir for the library target

# xlc creates the output file name by taking the name of the generated library
# and change the file exentsion to .x. (or appending if no extension exists)
# e.g.
# - foo.so => foo.x
# - foo.bar.baz => foo.bar.x
# - foo => foo.x

string(FIND "${LIBRARY_FILE_NAME}" "." dot_pos REVERSE)
string(SUBSTRING "${LIBRARY_FILE_NAME}" 0 ${dot_pos} base_name)

set(SRC_FILE "${CMAKE_BINARY_DIR}/${base_name}.x")
set(DEST_FILE "${LIBRARY_FOLDER}/${base_name}.x")

if(NOT "${SRC_FILE}" STREQUAL "${DEST_FILE}")
	file(RENAME "${SRC_FILE}" "${DEST_FILE}")
endif()
