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
###############################################################################

add_executable(omrgctest
	GCConfigObjectTable.cpp
	GCConfigTest.cpp
	gcTestHelpers.cpp
	main.cpp
	StartupManagerTestExample.cpp
)

#TODO this is a real gross, tangled mess
target_link_libraries(omrgctest
	omrGtestGlue
	pugixml
	omrtestutil
	omrcore
	${OMR_HOOK_LIB}
	${OMR_TRACE_LIB}
	${OMR_GC_LIB}
	${OMR_THREAD_LIB}
	${OMR_PORT_LIB}
)

# TODO check linker flags from makefile are ported properly.
# Relevant section from makefile below
#ifeq (linux,$(OMR_HOST_OS))
	#MODULE_SHARED_LIBS += rt pthread
#endif
#ifeq (osx,$(OMR_HOST_OS))
	#MODULE_SHARED_LIBS += iconv pthread
#endif
#ifeq (aix,$(OMR_HOST_OS))
	#MODULE_SHARED_LIBS += iconv perfstat
#endif
#ifeq (win,$(OMR_HOST_OS))
	#MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
#endif

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrgctest j9a2e)
endif()

set_property(TARGET omrgctest PROPERTY FOLDER fvtest)

add_test(NAME gctest
	COMMAND omrgctest "--gtest_filter=gcFunctionalTest*"
	WORKING_DIRECTORY "${omr_SOURCE_DIR}"
)
