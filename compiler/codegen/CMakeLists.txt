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

compiler_library(codegen
	${CMAKE_CURRENT_SOURCE_DIR}/OMRAheadOfTimeCompile.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Analyser.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CodeGenPrep.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CodeGenGC.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CodeGenRA.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/FrontEnd.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRGCRegisterMap.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRGCStackAtlas.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRLinkage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LiveRegister.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OutOfLineCodeSection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Relocation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ScratchRegisterManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/StorageInfo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRTreeEvaluator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PreInstructionSelection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/NodeEvaluation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSnippet.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRUnresolvedDataSnippet.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSnippetGCMap.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRCodeGenerator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRCodeGenPhase.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRMemoryReference.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRMachine.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRRegister.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRRealRegister.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRRegisterPair.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRInstruction.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ELFObjectFileGenerator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRELFRelocationResolver.cpp
)
