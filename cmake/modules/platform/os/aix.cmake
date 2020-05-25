###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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
###############################################################################

include(OmrAssert)

omr_assert(TEST DEFINED OMR_ARCH_POWER MESSAGE "OMR_HOST_OS configured as AIX but OMR_ARCH_POWER is not defined")

set(OMR_PLATFORM_DEFINITIONS
	-DRS6000
	-DAIXPPC
	-D_LARGE_FILES
	-D_ALL_SOURCE
)

# Testarossa build variables. Longer term the distinction between TR and the rest
# of the OMR code should be heavily reduced. In the mean time, we keep
# the distinction
list(APPEND TR_COMPILE_DEFINITIONS -DSUPPORTS_THREAD_LOCAL -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE -DAIX)


# On newer versions of CMake this seems to be requires (CMAKE_SHARED_LIBRARY_SUFFIX is not enough)
set(CMAKE_SHARED_LIBRARY_SUFFIX_C ".so")
set(CMAKE_SHARED_LIBRARY_SUFFIX_CXX ".so")
