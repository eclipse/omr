###############################################################################
#
# (c) Copyright IBM Corp. 2017, 2017
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
