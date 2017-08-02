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

add_executable(omrutiltest
	main.cpp
)

target_link_libraries(omrutiltest PRIVATE
	#omrGtestGlue
	omrGtest
	omrutil
	omrglue
)

omr_set_target_flags(omrutiltest)

target_include_directories(omrutiltest
	PRIVATE
	$<TARGET_PROPERTY:omrGtestGlue,INTERFACE_INCLUDE_DIRECTORIES>
)

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrutiltest PRIVATE j9a2e)
endif()

add_test(NAME utiltest COMMAND omrutiltest)
