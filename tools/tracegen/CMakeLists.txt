###############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
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

omr_set_target_flags(trace)
omr_set_target_flags(tracegen)

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(trace j9a2e)
endif()
