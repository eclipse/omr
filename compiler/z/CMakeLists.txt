###############################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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

compiler_library(z
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryCommutativeAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/BinaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/CallSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/CompareAnalyser.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ConstantDataSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/ControlFlowEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/FPTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMemoryReference.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OpMemToMem.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRMachine.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390BranchCondNames.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390Debug.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390GenerateInstructions.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390HelperCallSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390Instruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390Peephole.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/SystemLinkage.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/SystemLinkageLinux.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/SystemLinkagezOS.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390OutOfLineCodeSection.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRegisterDependency.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRSnippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/S390Snippets.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/TranslateEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRTreeEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/UnaryEvaluator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstruction.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRInstructionDelegate.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/InstOpCode.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRRealRegister.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRCodeGenPhase.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/OMRCodeGenerator.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/snippet/PPA1Snippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/codegen/snippet/PPA2Snippet.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRCPU.cpp
	${CMAKE_CURRENT_LIST_DIR}/env/OMRDebugEnv.cpp
	${CMAKE_CURRENT_LIST_DIR}/runtime/VirtualGuardRuntime.cpp
)
