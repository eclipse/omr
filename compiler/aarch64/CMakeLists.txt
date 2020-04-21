#################################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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
###############################################################################

compiler_library(aarch64
# Target files
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64BinaryEncoding.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64Debug.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64HelperCallSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64Instruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64OutOfLineCodeSection.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ARM64SystemLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ConstantDataSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ControlFlowEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/FPTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/GenerateInstructions.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRCodeGenerator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstructionDelegate.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMachine.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMemoryReference.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRealRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRegisterDependency.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OpBinary.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/UnaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRCPU.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRDebugEnv.cpp
	${CMAKE_CURRENT_LIST_DIR}/runtime/CodeSync.cpp
)
