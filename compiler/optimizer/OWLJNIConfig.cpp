//
// Created by Cijie Xia on 2019-09-29.
//

#include "optimizer/OWLJNIConfig.hpp"


/**** Class Names *****/
const char* InstructionClassName = "com/ibm/wala/shrikeBT/Instruction";

/***** Method Config ******/

const JNIMethodConfig ConstantInstruction_make_Config = {
    true,
    "com/ibm/wala/shrikeBT/ConstantInstruction",
    "make",
    "(Ljava/lang/String;Ljava/lang/Object;)Lcom/ibm/wala/shrikeBT/ConstantInstruction;"
};

const JNIMethodConfig StoreInstruction_make_Config = {
    true,
    "com/ibm/wala/shrikeBT/StoreInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/StoreInstruction;"
};

const JNIMethodConfig LoadInstruction_make_Config = {
    true,
    "com/ibm/wala/shrikeBT/LoadInstruction",
    "make",
    "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/LoadInstruction;"
};

const JNIMethodConfig BinaryOpInstruction_make_Config = {
    true,
    "com/ibm/wala/shrikeBT/BinaryOpInstruction",
    "make",
    "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;)Lcom/ibm/wala/shrikeBT/BinaryOpInstruction;"
};

const JNIMethodConfig ReturnInstruction_make_Config = {
    true,
    "com/ibm/wala/shrikeBT/ReturnInstruction",
    "make",
    "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ReturnInstruction;"
};

const JNIMethodConfig GotoInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/GotoInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/GotoInstruction;"
};

const JNIMethodConfig ConditionalBranchInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ConditionalBranchInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;I)Lcom/ibm/wala/shrikeBT/ConditionalBranchInstruction;"
};

const JNIMethodConfig ComparisonInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ComparisonInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;)Lcom/ibm/wala/shrikeBT/ComparisonInstruction;"
};

const JNIMethodConfig ConversionInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ConversionInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ConversionInstruction;"
};

const JNIMethodConfig UnaryOpInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/UnaryOpInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/UnaryOpInstruction;"
};

const JNIMethodConfig InvokeInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/InvokeInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;)Lcom/ibm/wala/shrikeBT/InvokeInstruction;"
};

const JNIMethodConfig SwapInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/SwapInstruction",
        "make",
        "()Lcom/ibm/wala/shrikeBT/SwapInstruction;"
};

const JNIMethodConfig PopInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/PopInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/PopInstruction;"
};

const JNIMethodConfig ArrayStoreInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ArrayStoreInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayStoreInstruction;"
};

const JNIMethodConfig ArrayLoadInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ArrayLoadInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ArrayLoadInstruction;"
};

const JNIMethodConfig NewInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/NewInstruction",
        "make",
        "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/NewInstruction;"
};

const JNIMethodConfig PutInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/PutInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Lcom/ibm/wala/shrikeBT/PutInstruction;"
};

const JNIMethodConfig GetInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/GetInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)Lcom/ibm/wala/shrikeBT/GetInstruction;"
};

const JNIMethodConfig DupInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/DupInstruction",
        "make",
        "(I)Lcom/ibm/wala/shrikeBT/DupInstruction;"
};

const JNIMethodConfig InstanceofInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/InstanceofInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/InstanceofInstruction;"
};

const JNIMethodConfig ArrayLengthInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ArrayLengthInstruction",
        "make",
        "()Lcom/ibm/wala/shrikeBT/ArrayLengthInstruction;"
};

const JNIMethodConfig ShiftInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/ShiftInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IShiftInstruction$Operator;)Lcom/ibm/wala/shrikeBT/ShiftInstruction;"
};

const JNIMethodConfig SwitchInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/SwitchInstruction",
        "make",
        "([II)Lcom/ibm/wala/shrikeBT/SwitchInstruction;"
};

const JNIMethodConfig CheckCastInstruction_make_Config = {
        true,
        "com/ibm/wala/shrikeBT/CheckCastInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/CheckCastInstruction;"
};

const JNIMethodConfig Verifier_init_Config = {

};

const JNIMethodConfig Printer_print_Config = {
        true,
        "com/ibm/wala/shrikeBT/Printer",
        "print",
        "([Lcom/ibm/wala/shrikeBT/Instruction;)V"
};

const JNIMethodConfig Verify_Config =  {
        true,
        "com/ibm/wala/shrikeBT/analysis/VerifierHelper",
        "verify",
        "([Lcom/ibm/wala/shrikeBT/Instruction;)V"
};


/***** Constructor Config *****/
const JNIConstructorConfig Printer = {
        "com/ibm/wala/shrikeBT/Printer",
        "()V"
};

const JNIConstructorConfig Verifier = {
        "com/ibm/wala/shrikeBT/analysis/Verifier",
        "(ZZLjava/lang/String;Ljava/lang/String;[Lcom/ibm/wala/shrikeBT/IInstruction;[[Lcom/ibm/wala/shrikeBT/ExceptionHandler;[I[[Ljava/lang/String;)V"
};

/***** Field Config ******/

const JNIFieldConfig ADD_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "ADD",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig SUB_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "SUB",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig MUL_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "MUL",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig DIV_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "DIV",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig REM_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "REM",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};


const JNIFieldConfig AND_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "AND",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig OR_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "OR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig XOR_OperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "XOR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const JNIFieldConfig EQ_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "EQ",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig NE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "NE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig LT_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "LT",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig GE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "GE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig GT_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "GT",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig LE_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator",
        "LE",
        "Lcom/ibm/wala/shrikeBT/IConditionalBranchInstruction$Operator;"
};

const JNIFieldConfig CMP_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMP",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const JNIFieldConfig CMPL_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMPL",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const JNIFieldConfig CMPG_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IComparisonInstruction$Operator",
        "CMPG",
        "Lcom/ibm/wala/shrikeBT/IComparisonInstruction$Operator;"
};

const JNIFieldConfig SHL_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IShiftInstruction$Operator",
        "SHL",
        "Lcom/ibm/wala/shrikeBT/IShiftInstruction$Operator;"
};

const JNIFieldConfig SHR_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IShiftInstruction$Operator",
        "SHR",
        "Lcom/ibm/wala/shrikeBT/IShiftInstruction$Operator;"
};

const JNIFieldConfig USHR_OperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IShiftInstruction$Operator",
        "USHR",
        "Lcom/ibm/wala/shrikeBT/IShiftInstruction$Operator;"
};

const JNIFieldConfig VIRTUAL_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "VIRTUAL",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const JNIFieldConfig SPECIAL_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "SPECIAL",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const JNIFieldConfig INTERFACE_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "INTERFACE",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};

const JNIFieldConfig STATIC_DispatchConfig = {
        true,
        "com/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch",
        "STATIC",
        "Lcom/ibm/wala/shrikeBT/IInvokeInstruction$Dispatch;"
};