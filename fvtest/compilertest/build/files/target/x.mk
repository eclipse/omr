JIT_PRODUCT_BACKEND_SOURCES+=\
    $(JIT_OMR_DIRTY_DIR)/x/codegen/BinaryCommutativeAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/BinaryEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/CompareAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/ConstantDataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/ControlFlowEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/DataSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/DivideCheckSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPBinaryArithmeticAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPCompareAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/FPTreeEvaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/HelperCallSnippet.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/IA32LinkageUtils.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/IntegerMultiplyDecomposer.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRMemoryReference.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OpBinary.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OpNames.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OutlinedInstructions.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/RegisterRematerialization.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/SubtractAnalyser.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/codegen/Trampoline.cpp \
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
    $(JIT_OMR_DIRTY_DIR)/x/codegen/OMRRegisterIterator.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    $(JIT_PRODUCT_DIR)/x/codegen/Evaluator.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/env/OMRDebugEnv.cpp \
    $(JIT_OMR_DIRTY_DIR)/x/env/OMRCPU.cpp

include $(JIT_MAKE_DIR)/files/target/$(TARGET_SUBARCH).mk
