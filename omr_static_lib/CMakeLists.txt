###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

set(OMR_STATIC_LIBNAME omrstatic)

set(OMR_LIBS 
	j9avl 
	j9hashtable
	j9hookstatic
	j9pool
	omrcore
	omrtrace
	omrutil
)

if(OMR_PORT)
	set(OMR_LIBS ${OMR_LIBS} omrport)
endif()

if(OMR_THREAD)
	set(OMR_LIBS ${OMR_LIBS} j9thrstatic)
endif()

if(OMR_GC)
	set(OMR_LIBS ${OMR_LIBS} omrgc)
endif()

set(OMR_LIB_FILES "")
foreach(OMR_LIB ${OMR_LIBS})
	list(APPEND OMR_LIB_FILES $<TARGET_FILE:${OMR_LIB}>)
endforeach(OMR_LIB)

if(UNIX)
	set(OMR_STATIC_LIBNAME ${OMR_STATIC_LIBNAME}.a)
	set(OMR_AR_CMD ar -rcT ${OMR_STATIC_LIBNAME} ${OMR_LIB_FILES})
elseif(WIN32 AND MSVC) 
	set(OMR_STATIC_LIBNAME ${OMR_STATIC_LIBNAME}.lib)
	set(OMR_AR_CMD lib.exe /OUT:${OMR_STATIC_LIBNAME} ${OMR_LIB_FILES})
else()
	message(FATAL "${OMR_STATIC_LIBNAME}: A build config must be provided for ${CMAKE_SYSTEM_NAME}")
endif()

add_custom_target(omrstatic ALL
    COMMAND ${OMR_AR_CMD}
    DEPENDS ${OMR_LIBS}
    COMMENT "Generating ${OMR_STATIC_LIBNAME} ..."
)

