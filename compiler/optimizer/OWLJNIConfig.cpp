//
// Created by Cijie Xia on 2019-09-29.
//

#include "optimizer/OWLJNIConfig.hpp"


/**** Class Names *****/
const char* InstructionClassName = "com/ibm/wala/shrikeBT/Instruction";

/***** Method Config ******/

const MethodConfig ConstantInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/ConstantInstruction",
    "make",
    "(Ljava/lang/String;Ljava/lang/Object;)Lcom/ibm/wala/shrikeBT/ConstantInstruction;"
};

const MethodConfig StoreInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/StoreInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/StoreInstruction;"
};

const MethodConfig LoadInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/LoadInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/LoadInstruction;"
};

const MethodConfig BinaryOpInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/BinaryOpInstruction",
    "make",
    "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;)Lcom/ibm/wala/shrikeBT/BinaryOpInstruction;"
};

const MethodConfig ReturnInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/ReturnInstruction",
    "make",
    "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ReturnInstruction;"
};

const MethodConfig GotoInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/GotoInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/GotoInstruction;"
};

const MethodConfig ConditionalBranchInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ConditionalBranchInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;I)Lcom/ibm/wala/shrikeBT/ConditionalBranchInstruction;"
};

const MethodConfig ComparisonInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ComparisonInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;)Lcom/ibm/wala/shrikeBT/ComparisonInstruction;"
};

const MethodConfig ConversionInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ConversionInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ConversionInstruction;"
};

const MethodConfig UnaryOpInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/UnaryOpInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/UnaryOpInstruction;"
};

const MethodConfig InvokeInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/InvokeInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;)Lcom/ibm/wala/shrikeBT/InvokeInstruction;"
};

const MethodConfig SwapInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/SwapInstruction",
        "make",
        "()Lcom/ibm/wala/shrikeBT/SwapInstruction;"
};

const MethodConfig PopInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/PopInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/PopInstruction;"
};

const MethodConfig ArrayStoreInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ArrayStoreInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayStoreInstruction;"
};

const MethodConfig ArrayLoadInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ArrayLoadInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayLoadInstruction;"
};

const MethodConfig NewInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/NewInstruction",
        "make",
        "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/NewInstruction;"
};

const MethodConfig PrinterConfig = {
        true,
        "com/ibm/wala/shrikeBT/Printer",
        "print",
        "([Lcom/ibm/wala/shrikeBT/Instruction;)V"
};

/***** Field Config ******/

const FieldConfig ADD_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "ADD",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig SUB_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "SUB",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig MUL_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "MUL",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig DIV_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "DIV",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig REM_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "REM",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};


const FieldConfig AND_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "AND",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig OR_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "OR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig XOR_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "XOR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig EQ_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "EQ",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig NE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "NE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig LT_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "LT",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig GE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "GE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig GT_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "GT",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig LE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "LE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const FieldConfig CMP_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMP",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const FieldConfig CMPL_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMPL",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const FieldConfig CMPG_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMPG",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const FieldConfig VIRTUAL_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "VIRTUAL",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const FieldConfig SPECIAL_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "SPECIAL",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const FieldConfig INTERFACE_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "INTERFACE",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const FieldConfig STATIC_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "STATIC",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};