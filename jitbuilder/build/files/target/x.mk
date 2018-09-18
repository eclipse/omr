###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
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

JIT_PRODUCT_BACKEND_SOURCES+=\
    $(JIT_OMR_DIRTY_DIR)/x/codegen/BinaryCommutativeAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/CompareAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/DataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/DivideCheckSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPBinaryArithmeticAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPCompareAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/SIMDTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/HelperCallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/IA32LinkageUtils.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/IntegerMultiplyDecomposer.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OpNames.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OutlinedInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/RegisterRematerialization.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/SubtractAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/UnaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/X86BinaryEncoding.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/X86Debug.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/X86FPConversionSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRInstruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRX86Instruction.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRMachine.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRRealRegister.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRRegisterDependency.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/X86SystemLinkage.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/XMMBinaryArithmeticAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRCodeGenerator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/env/OMRDebugEnv.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/env/OMRCPU.cpp \
    $(JIT_PRODUCT_DIR)/x/codegen/Evaluator.cpp

include $(JIT_MAKE_DIR)/files/target/$(TARGET_SUBARCH).mk
