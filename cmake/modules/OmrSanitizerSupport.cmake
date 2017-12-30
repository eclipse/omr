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

include(OmrUtility)

set(OMR_SANITIZER OFF CACHE STRING "-fsanitize parameter")
set_property(
	CACHE OMR_SANITIZER
	PROPERTY STRINGS OFF undefined address thread
	)

# GNU style sanitizers 
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU") 
	if (OMR_SANITIZER) 
		message(STATUS "${OMR_SANITIZER} Sanitizer is enabled")
		# Extra options for the sanitizers, often documented to improve output
		set(ADDITIONAL_SANITIZER_FLAGS "-fno-omit-frame-pointer")
		omr_append_flags(CMAKE_CXX_FLAGS ${ADDITIONAL_SANITIZER_FLAGS} "-fsanitize=${OMR_SANITIZER}")
		omr_append_flags(CMAKE_C_FLAGS   ${ADDITIONAL_SANITIZER_FLAGS} "-fsanitize=${OMR_SANITIZER}")
	endif() 
endif()
