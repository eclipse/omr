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

# Find libelf
# Will set:
#  LIBELF_FOUND
#  LIBELF_INCLUDE_DIRS
#  LIBELF_LIBRARIES
#  LIBELF_DEFINITIONS

find_path(ELF_H_INCLUDE_DIR elf.h)

find_path(LIBELF_H_INCLUDE_DIR libelf.h)

find_library(LIBELF_LIBRARY elf)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LibElf
	DEFAULT_MSG
	LIBELF_LIBRARY
	ELF_H_INCLUDE_DIR
	LIBELF_H_INCLUDE_DIR
)

if(LIBELF_FOUND)
	set(LIBELF_INCLUDE_DIRS
		${ELF_H_INCLUDE_DIR}
		${LIBELF_H_INCLUDE_DIR}
	)
	set(LIBELF_LIBRARIES
		${LIBELF_LIBRARY}
	)
	set (LIBELF_DEFINITIONS "")
endif(LIBELF_FOUND)

