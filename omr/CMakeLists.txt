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

add_tracegen(omrti.tdf)
add_tracegen(omrvm.tdf)

add_library(omrcore
	OMR_Agent.cpp
	OMR_MethodDictionary.cpp
	OMR_MethodDictionary.hpp
	OMR_Profiler.cpp
	OMR_Runtime.cpp
	OMR_TI.cpp
	OMR_TIMemorySize.cpp
	OMR_VM.cpp
	OMR_VMThread.cpp
	omrrasinit.c
	omrti.tdf
	omrtrcinit.c
	startup/omrvmstartup.cpp
	ut_omrti.c
	ut_omrti.h
	ut_omrvm.c
	ut_omrvm.h
)
omr_set_target_flags(omrcore)

add_dependencies(omrcore omrgc_tracegen)
target_include_directories(omrcore
	PUBLIC
		.
)

target_link_libraries(omrcore
	PUBLIC
		omrgc # mminitcore.h
		omrport
		omrtrace
)
