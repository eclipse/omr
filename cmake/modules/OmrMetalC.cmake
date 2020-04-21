###############################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
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

# Include once.
if(OMRMETALC_)
	return()
endif()
set(OMRMETALC_ 1)

include(OmrAssert)

find_program(AS_EXECUTABLE
	NAMES as
	DOC "The XLC assembler"
)

find_program(XLC_EXECUTABLE
	NAMES xlc
	DOC "The XLC compiler"
)

set(OMR_METALC_XLC_FLAGS "-qlongname" CACHE STRING "Options added to XLC when compiler METAL-C to HLASM")
set(OMR_METALC_ASM_FLAGS "-mgoff" "-I" "CBC.SCCNSAM" CACHE STRING "Options added when compiling METAL-C HLASM files")

if(OMR_ENV_DATA64)
	list(APPEND OMR_METALC_XLC_FLAGS "-q64")
endif()

# omr_compile_metalc(<mfile> <ofile>)
#
# Define a METAL-C generated file. Metal-C is a z/OS meta-assembly framework.
# METAL-C sources are translated to assembly via the XLC compiler. Once a .mc
# source has been defined, the resulting .s file can be added to any library as
# a regular source. This function only works with z/OS xlc.
#
# <mfile> is a METAL-C sourcefile. <ofile> is the resulting object file.
# By convention, the OMR project names METAL-C sources with a .mc extension.
#
# For more information: http://publibz.boulder.ibm.com/epubs/pdf/ccrug100.pdf
function(omr_compile_metalc mfile ofile)
	omr_assert(TEST XLC_EXECUTABLE)
	omr_assert(TEST AS_EXECUTABLE)

	if(NOT IS_ABSOLUTE "${mfile}")
		set(mfile "${CMAKE_CURRENT_SOURCE_DIR}/${mfile}")
	endif()

	if(NOT IS_ABSOLUTE "${ofile}")
		set(ofile "${CMAKE_CURRENT_BINARY_DIR}/${ofile}")
	endif()

	set(cfile "${ofile}.c")
	set(sfile "${ofile}.s")

	add_custom_command(
		OUTPUT "${cfile}"
		MAIN_DEPENDENCY "${mfile}"
		COMMAND "${CMAKE_COMMAND}" -E copy "${mfile}" "${cfile}"
		VERBATIM
	)
	add_custom_command(
		OUTPUT "${ofile}"
		MAIN_DEPENDENCY "${cfile}"
		COMMAND "${XLC_EXECUTABLE}" -qmetal -S ${OMR_METALC_XLC_FLAGS} -o "${sfile}" "${cfile}"
		COMMAND "${AS_EXECUTABLE}" ${OMR_METALC_ASM_FLAGS} -o "${ofile}" "${sfile}"
		VERBATIM
	)
endfunction(omr_compile_metalc)
