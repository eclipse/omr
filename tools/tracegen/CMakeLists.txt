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

add_executable(tracegen
	main.cpp
)

target_link_libraries(tracegen
	PRIVATE
		trace # static
)

# TODO: Move libtrace to it's own home
# NOTE: trace is static so we don't futz with RPATHs to run the tools.
add_library(trace STATIC
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

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(trace j9a2e)
endif()
