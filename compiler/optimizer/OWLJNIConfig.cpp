//
// Created by Cijie Xia on 2019-09-29.
//

#include "OWLJNIConfig.hpp"

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

/***** Field Config ******/

const FieldConfig AddOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "ADD",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig SubOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "SUB",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig MulOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "MUL",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig DivOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "DIV",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig RemOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "REM",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};


const FieldConfig AndOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "AND",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig OrOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "OR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

const FieldConfig XorOperatorConfig = {
    true,
    "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
    "XOR",
    "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};

