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

compiler_library(env 
	${CMAKE_CURRENT_SOURCE_DIR}/ExceptionTable.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRCPU.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRObjectModel.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRArithEnv.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRClassEnv.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRDebugEnv.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRVMEnv.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/SegmentAllocator.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/SegmentProvider.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/SystemSegmentProvider.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/DebugSegmentProvider.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/Region.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/StackMemoryRegion.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRPersistentInfo.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/TRMemory.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/TRPersistentMemory.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/VerboseLog.cpp 


	# The following are listed under JIT_PRODUCT_SOURCE_FILES 
	# (for compiler test, but not jitbuilder). 
	# TODO: figure out why and move if required
	${CMAKE_CURRENT_SOURCE_DIR}/FEBase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/JitConfig.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRIO.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRKnownObjectTable.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Globals.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRCompilerEnv.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PersistentAllocator.cpp

)
