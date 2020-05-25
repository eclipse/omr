###############################################################################
# Copyright (c) 2016, 2020 IBM Corp. and others
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

JIT_PRODUCT_BACKEND_SOURCES+=\
    $(JIT_OMR_DIRTY_DIR)/z/codegen/BinaryAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/BinaryCommutativeAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/CallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/CompareAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/ConstantDataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OpMemToMem.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390BranchCondNames.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390Debug.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390GenerateInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390HelperCallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390Instruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390Peephole.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/SystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/SystemLinkageLinux.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/SystemLinkagezOS.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390OutOfLineCodeSection.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/S390Snippets.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/TranslateEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/UnaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRInstructionDelegate.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/InstOpCode.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRCodeGenPhase.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/snippet/PPA1Snippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/snippet/PPA2Snippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/codegen/snippet/PPA2Snippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/env/OMRCPU.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    $(JIT_PRODUCT_DIR)/z/codegen/TestCodeGenerator.cpp \
    $(JIT_PRODUCT_DIR)/z/codegen/Evaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/z/env/OMRDebugEnv.cpp
