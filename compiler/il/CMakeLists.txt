###############################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

compiler_library(il
	${CMAKE_CURRENT_LIST_DIR}/OMRSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRAutomaticSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRLabelSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRMethodSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRParameterSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRegisterMappedSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRResolvedMethodSymbol.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRStaticSymbol.cpp

	${CMAKE_CURRENT_LIST_DIR}/Aliases.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRBlock.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRDataTypes.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRIL.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRILOps.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRNode.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSymbolReference.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRTreeTop.cpp
	${CMAKE_CURRENT_LIST_DIR}/NodePool.cpp
	${CMAKE_CURRENT_LIST_DIR}/NodeUtils.cpp
)
