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

compiler_library(il 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRSymbol.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRAutomaticSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRLabelSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRMethodSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRParameterSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRRegisterMappedSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRResolvedMethodSymbol.cpp 
	${CMAKE_CURRENT_SOURCE_DIR}/symbol/OMRStaticSymbol.cpp 

	${CMAKE_CURRENT_SOURCE_DIR}/Aliases.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRBlock.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRDataTypes.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRIL.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRILOps.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRNode.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSymbolReference.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRTreeTop.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NodePool.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NodeUtils.cpp
)
