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
    $(JIT_OMR_DIRTY_DIR)/p/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/GenerateInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OpProperties.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCAOTRelocation.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCBinaryEncoding.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCDebug.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCHelperCallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCSystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCOutOfLineCodeSection.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/PPCTableOfConstants.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/TreeEvaluatorVMX.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/UnaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/codegen/OMRConstantDataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/env/OMRCPU.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    $(JIT_PRODUCT_DIR)/p/codegen/Evaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/p/env/OMRDebugEnv.cpp
