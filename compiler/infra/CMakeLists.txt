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

compiler_library(infra 
	${CMAKE_CURRENT_SOURCE_DIR}/Assert.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/BitVector.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/Checklist.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/HashTab.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/IGBase.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/IGNode.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/ILTraverser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ILWalk.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/InterferenceGraph.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRMonitor.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/OMRMonitorTable.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/Random.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleRegex.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/STLUtils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Timer.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/TreeServices.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/TrilDumper.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRCfg.cpp 
)
