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

include(OmrMsvcRuntime)

omr_add_library(omrsig SHARED
	omrsig.cpp
)

if(OMR_HOST_OS STREQUAL "win")
	target_compile_definitions(omrsig PRIVATE -DMSVC_RUNTIME_DLL="${OMR_MSVC_CRT}")
endif()

omr_add_exports(omrsig
	omrsig_primary_signal
	omrsig_handler
	signal
)
if(NOT OMR_OS_WINDOWS)
	omr_add_exports(omrsig
		omrsig_primary_sigaction
		sigaction
		sigset
		sigignore
		bsd_signal
		sysv_signal
	)
endif()
if(OMR_OS_LINUX OR OMR_OS_OSX)
	omr_add_exports(omrsig
		__sysv_signal
		ssignal
	)
endif()
if(OMR_OS_ZOS)
	omr_add_exports(omrsig
		__sigactionset
	)
endif()
if((NOT OMR_OS_WINDOWS) AND (NOT OMR_OS_ZOS))
	omr_add_exports(omrsig
		sigvec
	)
endif()

target_link_libraries(omrsig
	PRIVATE
		omr_base
		omrutil
)

target_compile_options(omrsig
	PRIVATE
		${OMR_PLATFORM_SHARED_COMPILE_OPTIONS}
)

if(OMR_HOST_OS MATCHES "linux|osx")
	target_link_libraries(omrsig PRIVATE dl)
endif()
if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrsig PRIVATE j9a2e)
endif()
#TODO ensure we are linking in the same way as source makefile:
#MODULE_STATIC_LIBS += omrutil
#ifneq (,$(findstring linux,$(OMR_HOST_OS)))
	#MODULE_SHARED_LIBS += pthread
#endif
#ifneq (,$(findstring osx,$(OMR_HOST_OS)))
	#MODULE_SHARED_LIBS += pthread
#endif

install(
	TARGETS omrsig
	EXPORT OmrTargets
	PUBLIC_HEADER DESTINATION ${OMR_INSTALL_INC_DIR}/
	ARCHIVE DESTINATION ${OMR_INSTALL_ARCHIVE_DIR}/
	LIBRARY DESTINATION ${OMR_INSTALL_LIB_DIR}/
)

target_enable_ddr(omrsig)
ddr_set_add_targets(omrddr omrsig)
