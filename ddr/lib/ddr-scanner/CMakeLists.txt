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

add_library(omr_ddr_scanner
	Scanner.cpp
)

set_property(TARGET omr_ddr_scanner PROPERTY CXX_STANDARD 11)

target_link_libraries(omr_ddr_scanner
	PUBLIC
		omr_ddr_base
		omr_ddr_ir
)

if(OMR_HOST_OS STREQUAL "win")
	find_package(DiaSDK REQUIRED)
	target_sources(omr_ddr_scanner
		PRIVATE
			pdb/PdbScanner.cpp
	)
	target_link_libraries(omr_ddr_scanner
		PUBLIC
			${DIASDK_LIBRARIES}
	)
	target_include_directories(omr_ddr_scanner
		PUBLIC
			${DIASDK_INCLUDE_DIRS}
	)
elseif(OMR_HOST_OS STREQUAL "linux")
	
	find_package(LibDwarf REQUIRED)

	target_link_libraries(omr_ddr_scanner
		PUBLIC
			${LIBDWARF_LIBRARIES}
	)
	target_include_directories(omr_ddr_scanner
		PUBLIC
			${LIBDWARF_INCLUDE_DIRS}
	)
	target_compile_definitions(omr_ddr_scanner
		PUBLIC
			${LIBDWARF_DEFINITIONS}
	)

	target_sources(omr_ddr_scanner
		PRIVATE
			dwarf/DwarfFunctions.cpp
			dwarf/DwarfScanner.cpp
	)

elseif(OMR_HOST_OS STREQUAL "osx")
	target_sources(omr_ddr_scanner
		PRIVATE
			dwarf/DwarfFunctions.cpp
			dwarf/DwarfParser.cpp
			dwarf/DwarfScanner.cpp
	)
elseif(OMR_HOST_OS STREQUAL "aix")
	target_sources(omr_ddr_scanner
		PRIVATE
			dwarf/AixSymbolTableParser.cpp
			dwarf/DwarfFunctions.cpp
			dwarf/DwarfScanner.cpp
	)
	target_link_libraries(omr_ddr_scanner
		PUBLIC
			${LIBDWARF_LIBRARIES}
	)
endif()
