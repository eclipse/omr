###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

omr_add_library(omrtrace STATIC
	omrtraceapi.cpp
	omrtracecomponent.cpp
	omrtraceformatter.cpp
	omrtracelog.cpp
	omrtracemain.cpp
	omrtracemisc.cpp
	omrtraceoptions.cpp
	omrtracepublish.cpp
	omrtracewrappers.cpp
)

# Avoid a problem verifying strncpys inlined to operate on array fields used
# to implement variable length structs
if (OMR_STRNCPY_FORTIFY_OPTIONS)
	target_compile_options(omrtrace PRIVATE ${OMR_STRNCPY_FORTIFY_OPTIONS})
endif()

target_link_libraries(omrtrace
	PUBLIC
		omr_base
		${OMR_THREAD_LIB}
		omrutil
)

target_enable_ddr(omrtrace)
ddr_add_headers(omrtrace
	${omr_SOURCE_DIR}/include_core/ute_module.h
)
ddr_set_add_targets(omrddr omrtrace)

#TODO: check if following makefile fragment still required:
#ifeq (gcc,$(OMR_TOOLCHAIN))
## disable builtin strncpy buffer length check for optimized builds (_FORTIFY_SOURCE=1 requires -O3)
	#ifeq ($(OMR_OPTIMIZE),1)
		#MODULE_CXXFLAGS+=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
	#endif
#endif
