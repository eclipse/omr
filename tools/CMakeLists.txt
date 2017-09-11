###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

include(OmrAssert)

omr_assert(TEST OMR_TOOLS)
omr_assert(TEST NOT OMR_TOOLS_IMPORTFILE)

if(CMAKE_CROSSCOMPILING)
	message(STATUS "OMR: Using cross compiled tools.")
	message(STATUS "OMR: You may need to import tools from a native project.")
	message(STATUS "OMR: See OMR_TOOLS_IMPORTFILE.")
endif(CMAKE_CROSSCOMPILING)

add_subdirectory(hookgen)
add_subdirectory(tracemerge)
add_subdirectory(tracegen)

export(TARGETS hookgen tracemerge tracegen FILE "ImportTools.cmake")
