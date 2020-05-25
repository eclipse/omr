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

omr_add_executable(tracegen
	main.cpp
)

target_link_libraries(tracegen
	PRIVATE
		trace # static
)

# TODO: Move libtrace to it's own home
# NOTE: trace is static so we don't futz with RPATHs to run the tools.
omr_add_library(trace STATIC
	ArgParser.cpp
	CFileWriter.cpp
	DATFileWriter.cpp
	FileReader.cpp
	FileUtils.cpp
	Port.cpp
	StringUtils.cpp
	TDFParser.cpp
	TraceGen.cpp
	TraceHeaderWriter.cpp
)

target_include_directories(trace
	INTERFACE
		./
)

if(OMR_OS_ZOS)
	if(OMR_TOOLS_USE_NATIVE_ENCODING)
		target_link_libraries(trace PUBLIC omr_ebcdic)
	else()
		target_link_libraries(trace PUBLIC omr_ascii)
	endif()
endif()

set_target_properties(trace tracegen PROPERTIES FOLDER tools)

install(TARGETS tracegen
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	COMPONENT tooling
)
