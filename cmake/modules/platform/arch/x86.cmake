###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
#############################################################################

if(OMR_ENV_DATA64)
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DJ9HAMMER
	)
else()
	list(APPEND OMR_PLATFORM_DEFINITIONS
		-DJ9X86
	)
endif()

# Testarossa build variables. Longer term the distinction between TR and the rest 
# of the OMR code should be heavily reduced. In the mean time, we keep
# the distinction
list(APPEND TR_COMPILE_DEFINITIONS -DTR_HOST_X86 -DTR_TARGET_X86)

set(TR_HOST_ARCH    x)

if(OMR_ENV_DATA64)
	list(APPEND TR_COMPILE_DEFINITIONS -DTR_HOST_64BIT -DTR_TARGET_64BIT)


	set(TR_HOST_SUBARCH amd64)
	set(TR_HOST_BITS    64)
	set(CMAKE_ASM-ATT_FLAGS "--64 --defsym TR_HOST_X86=1 --defsym TR_HOST_64BIT=1 --defsym BITVECTOR_64BIT=1 --defsym LINUX=1 --defsym TR_TARGET_X86=1 --defsym TR_TARGET_64BIT=1")
else()
		message(FATAL_ERROR "JIT isn't ready to build with CMake on this platform: ")
endif()
