###############################################################################
# Copyright (c) 2016, 2017 IBM Corp. and others
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
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMBinaryEncoding.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMDebug.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMDisassem.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMGenerateInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMOperand2.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMOutOfLineCodeSection.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ARMSystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/BinaryCommutativeAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ConstantDataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OpProperties.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/StackCheckFailureSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/SubtractAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/codegen/UnaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/arm/env/OMRCompilerEnv.cpp

JIT_PRODUCT_SOURCE_FILES+=
