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
###############################################################################

compiler_library(ras
	${CMAKE_CURRENT_SOURCE_DIR}/CallStack.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CFGChecker.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Debug.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DebugCounter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ILValidationRules.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ILValidationUtils.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ILValidator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/IgnoreLocale.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LimitFile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LogTracer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OptionsDebug.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PPCOpNames.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Tree.cpp
)
