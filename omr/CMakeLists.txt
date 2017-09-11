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

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_tracegen(omrti.tdf)
add_tracegen(omrvm.tdf)

add_library(omrcore STATIC
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

add_dependencies(omrcore
	omrgc_tracegen
	omrgc_hookgen
)

target_link_libraries(omrcore
	PRIVATE
		${OMR_CORE_GLUE_TARGET}
		omrtrace
		omrgc
)

target_include_directories(omrcore
	PUBLIC
		.
	PRIVATE
		$<TARGET_PROPERTY:omrgc,INTERFACE_INCLUDE_DIRECTORIES>
		$<TARGET_PROPERTY:${OMR_GC_GLUE_TARGET},INTERFACE_INCLUDE_DIRECTORIES>
)

