//
// Created by Cijie Xia on 2019-09-29.
//

#include "optimizer/OWLJNIConfig.hpp"

/*** Types ***/
char* const TYPE_short = "S";
char* const TYPE_long = "J";
char* const TYPE_int = "I";
char* const TYPE_float = "F";
char* const TYPE_double = "D";
char* const TYPE_char = "C";
char* const TYPE_byte = "B";
char* const TYPE_void = "V";
char* const TYPE_boolean = "Z";
char* const TYPE_Object = "Ljava/lang/Object;";


/***** Method Config ******/

const MethodConfig ConstantInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/ConstantInstruction",
    "make",
    "(Ljava/lang/String;Ljava/lang/Object;)Lcom/ibm/wala/shrikeBT/ConstantInstruction;"
};

const MethodConfig ConstantInstructionToStringConfig = {
    false,
    "com/ibm/wala/shrikeBT/ConstantInstruction",
    "toString",
    "()Ljava/lang/String;"
};

const MethodConfig StoreInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/StoreInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/StoreInstruction;"
};

const MethodConfig StoreInstructionToStringConfig = {
    false,
    "com/ibm/wala/shrikeBT/StoreInstruction",
    "toString",
    "()Ljava/lang/String;"
};

const MethodConfig LoadInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/LoadInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/LoadInstruction;"
};

const MethodConfig LoadInstructionToStringConfig = {
    false,
    "com/ibm/wala/shrikeBT/LoadInstruction",
    "toString",
    "()Ljava/lang/String;"
};

const MethodConfig BinaryOpInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/BinaryOpInstruction",
    "make",
    "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;)Lcom/ibm/wala/shrikeBT/BinaryOpInstruction;"
};

const MethodConfig BinaryOpInstructionToStringConfig = {
    false,
    "com/ibm/wala/shrikeBT/BinaryOpInstruction",
    "toString",
    "()Ljava/lang/String;"
};

const MethodConfig ReturnInstructionConfig = {
    true,
    "com/ibm/wala/shrikeBT/ReturnInstruction",
    "make",
    "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ReturnInstruction;"
};

const MethodConfig ReturnInstructionToStringConfig = {
    false,
    "com/ibm/wala/shrikeBT/ReturnInstruction",
    "toString",
    "()Ljava/lang/String;"
};

const MethodConfig GotoInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/GotoInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/GotoInstruction;"
};

const MethodConfig GotoInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/GotoInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig ConditionalBranchInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ConditionalBranchInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;I)Lcom/ibm/wala/shrikeBT/ConditionalBranchInstruction;"
};

const MethodConfig ConditionalBranchInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/ConditionalBranchInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig ComparisonInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ComparisonInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;)Lcom/ibm/wala/shrikeBT/ComparisonInstruction;"
};

const MethodConfig ComparisonInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/ComparisonInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig ConversionInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ConversionInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ConversionInstruction;"
};

const MethodConfig ConversionInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/ConversionInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig UnaryOpInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/UnaryOpInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/UnaryOpInstruction;"
};

const MethodConfig UnaryOpInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/UnaryOpInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig InvokeInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/InvokeInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;)Lcom/ibm/wala/shrikeBT/InvokeInstruction;"
};

const MethodConfig InvokeInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/InvokeInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig SwapInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/SwapInstruction",
        "make",
        "()Lcom/ibm/wala/shrikeBT/SwapInstruction;"
};

const MethodConfig SwapInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/SwapInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig PopInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/PopInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/PopInstruction;"
};

const MethodConfig PopInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/PopInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig ArrayStoreInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ArrayStoreInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayStoreInstruction;"
};

const MethodConfig ArrayStoreInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/ArrayStoreInstruction",
        "toString",
        "()Ljava/lang/String;"
};

const MethodConfig ArrayLoadInstructionConfig = {
        true,
        "com/ibm/wala/shrikeBT/ArrayLoadInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayLoadInstruction;"
};

const MethodConfig ArrayLoadInstructionToStringConfig = {
        false,
        "com/ibm/wala/shrikeBT/ArrayLoadInstruction",
        "toString",
        "()Ljava/lang/String;"
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