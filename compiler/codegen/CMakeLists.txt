###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

compiler_library(codegen
	${CMAKE_CURRENT_LIST_DIR}/OMRAheadOfTimeCompile.cpp
	${CMAKE_CURRENT_LIST_DIR}/Analyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/CodeGenPrep.cpp
	${CMAKE_CURRENT_LIST_DIR}/CodeGenGC.cpp
	${CMAKE_CURRENT_LIST_DIR}/CodeGenRA.cpp
	${CMAKE_CURRENT_LIST_DIR}/FrontEnd.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRGCRegisterMap.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRGCStackAtlas.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/LiveRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/OutOfLineCodeSection.cpp
	${CMAKE_CURRENT_LIST_DIR}/Relocation.cpp
	${CMAKE_CURRENT_LIST_DIR}/RegisterIterator.cpp
	${CMAKE_CURRENT_LIST_DIR}/ScratchRegisterManager.cpp
	${CMAKE_CURRENT_LIST_DIR}/StorageInfo.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/PreInstructionSelection.cpp
	${CMAKE_CURRENT_LIST_DIR}/NodeEvaluation.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRUnresolvedDataSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSnippetGCMap.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRCodeGenerator.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRCodeGenPhase.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRMemoryReference.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRMachine.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRealRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRegisterPair.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRInstruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/ELFGenerator.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRELFRelocationResolver.cpp
)
