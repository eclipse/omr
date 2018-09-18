###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################
#codegen files

JIT_PRODUCT_BACKEND_SOURCES+= \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ARM64BinaryEncoding.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ARM64Debug.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ARM64Instruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ARM64OutOfLineCodeSection.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ARM64SystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/GenerateInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/aarch64/codegen/UnaryEvaluator.cpp

#environement files

JIT_PRODUCT_BACKEND_SOURCES+= \
    $(JIT_OMR_DIRTY_DIR)/aarch64/env/OMRDebugEnv.cpp
