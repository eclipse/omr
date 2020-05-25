###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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
#############################################################################

# Find libdwarf.
# Optional dependencies:
#   LibElf
#   LibZ
# Will set:
#   LIBDWARF_FOUND
#   LIBDWARF_INCLUDE_DIRS
#   LIBDWARF_LIBRARIES
#   LIBDWARF_DEFINITIONS

find_package(LibElf)
find_package(LibZ)

# Find dwarf.h
# Will set:
#   DWARF_H_DEFINITIONS
#   DWARF_H_FOUND
#   DWARF_H_INCLUDE_DIRS

set(DWARF_H_DEFINITIONS)
set(DWARF_H_FOUND false)
set(DWARF_H_INCLUDE_DIRS)

find_path(DWARF_H_INCLUDE_DIR "dwarf.h")

if(DWARF_H_INCLUDE_DIR)
	list(APPEND DWARF_H_INCLUDE_DIRS "${DWARF_H_INCLUDE_DIR}")
	list(APPEND DWARF_H_DEFINITIONS HAVE_DWARF_H)
	set(DWARF_H_FOUND true)
endif()

find_path(LIBDWARF_DWARF_H_INCLUDE_DIR "libdwarf/dwarf.h")

if(LIBDWARF_DWARF_H_INCLUDE_DIR)
	list(APPEND DWARF_H_INCLUDE_DIRS "${LIBDWARF_DWARF_H_INCLUDE_DIR}")
	list(APPEND DWARF_H_DEFINITIONS HAVE_LIBDWARF_DWARF_H)
	set(DWARF_H_FOUND true)
endif()

if(NOT DWARF_H_FOUND)
	set(DWARF_H_FOUND false)
	set(DWARF_H_INCLUDE_DIRS NOTFOUND)
	set(DWARF_H_DEFINITIONS false)
endif()

# Find libdwarf.h
# Will set:
#   LIBDWARF_H_DEFINITIONS
#   LIBDWARF_H_FOUND
#   LIBDWARF_H_INCLUDE_DIRS

set(LIBDWARF_H_DEFINITIONS)
set(LIBDWARF_H_FOUND false)
set(LIBDWARF_H_INCLUDE_DIRS)

find_path(LIBDWARF_H_INCLUDE_DIR "libdwarf.h")

if(LIBDWARF_H_INCLUDE_DIR)
	list(APPEND LIBDWARF_H_INCLUDE_DIRS "${LIBDWARF_H_INCLUDE_DIR}")
	list(APPEND LIBDWARF_H_DEFINITIONS HAVE_LIBDWARF_H)
	set(LIBDWARF_H_FOUND true)
endif()

find_path(LIBDWARF_LIBDWARF_H_INCLUDE_DIR "libdwarf/libdwarf.h")

if(LIBDWARF_LIBDWARF_H_INCLUDE_DIR)
	list(APPEND LIBDWARF_H_INCLUDE_DIRS "${LIBDWARF_LIBDWARF_H_INCLUDE_DIR}")
	list(APPEND LIBDWARF_H_DEFINITIONS HAVE_LIBDWARF_LIBDWARF_H)
	set(LIBDWARF_H_FOUND true)
endif()

if(NOT LIBDWARF_H_FOUND)
	set(LIBDWARF_H_FOUND false)
	set(LIBDWARF_H_INCLUDE_DIRS NOTFOUND)
	set(LIBDWARF_H_DEFINITIONS false)
endif()

# Find library

find_library(LIBDWARF_LIBRARY dwarf)

# Handle the arguments

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LibDwarf
	DEFAULT_MSG
	LIBDWARF_LIBRARY
	DWARF_H_FOUND
	LIBDWARF_H_FOUND
)

if(NOT LIBDWARF_FOUND)
	set(LIBDWARF_INCLUDE_DIRS NOTFOUND)
	set(LIBDWARF_LIBRARIES NOTFOUND)
	set(LIBDWARF_DEFINITIONS NOTFOUND)
	return()
endif()

# Everything below is only set if the library is found

set(LIBDWARF_INCLUDE_DIRS ${DWARF_H_INCLUDE_DIRS} ${LIBDWARF_H_INCLUDE_DIRS})
set(LIBDWARF_LIBRARIES ${LIBDWARF_LIBRARY})
set(LIBDWARF_DEFINITIONS ${DWARF_H_DEFINITIONS} ${LIBDWARF_H_DEFINITIONS})

if(LibZ_FOUND)
	list(APPEND LIBDWARF_INCLUDE_DIRS ${LIBZ_INCLUDE_DIRS})
	list(APPEND LIBDWARF_LIBRARIES ${LIBZ_LIBRARIES})
	list(APPEND LIBDWARF_DEFINITIONS ${LIBZ_DEFINITIONS})
endif(LibZ_FOUND)

if(LibElf_FOUND)
	list(APPEND LIBDWARF_INCLUDE_DIRS ${LIBELF_INCLUDE_DIRS})
	list(APPEND LIBDWARF_LIBRARIES ${LIBELF_LIBRARIES})
	list(APPEND LIBDWARF_DEFINITIONS ${LIBELF_DEFINITIONS})
endif(LibElf_FOUND)

if(NOT TARGET LibDwarf::dwarf)
	add_library(LibDwarf::dwarf UNKNOWN IMPORTED)

	set_target_properties(LibDwarf::dwarf
		PROPERTIES
			IMPORTED_LOCATION "${LIBDWARF_LIBRARY}"
			INTERFACE_INCLUDE_DIRECTORIES "${DWARF_H_INCLUDE_DIRS};${LIBDWARF_H_INCLUDE_DIRS}"
			INTERFACE_COMPILE_DEFINITIONS "${DWARF_H_DEFINITIONS};${LIBDWARF_H_DEFINITIONS}"
	)

	if(LibZ_FOUND)
		set_property(
			TARGET LibDwarf::dwarf APPEND
			PROPERTY INTERFACE_LINK_LIBRARIES LibZ::z
		)
	endif(LibZ_FOUND)

	if(LibElf_FOUND)
		set_property(
			TARGET LibDwarf::dwarf APPEND
			PROPERTY INTERFACE_LINK_LIBRARIES LibElf::elf
		)
	endif(LibElf_FOUND)
endif()
