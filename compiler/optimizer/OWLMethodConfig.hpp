//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "OWLJNIClient.hpp"

/* Types */
extern char * const TYPE_int = "I";
extern char * const TYPE_float = "F";

/***** Method Config ******/

extern const MethodConfig ConstantInstructionConfig = {
        "com/ibm/wala/shrikeBT/ConstantInstruction",
        "make",
        "(Ljava/lang/String;Ljava/lang/Object;)Lcom/ibm/wala/shrikeBT/ConstantInstruction;"
};

extern const MethodConfig ConstantInstructionToStringConfig = {
        "com/ibm/wala/shrikeBT/ConstantInstruction",
        "toString",
        "()Ljava/lang/String;"
};

extern const MethodConfig StoreInstructionConfig = {
        "com/ibm/wala/shrikeBT/StoreInstruction",
        "make",
        "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/StoreInstruction;"
};

extern const MethodConfig StoreInstructionToStringConfig = {
        "com/ibm/wala/shrikeBT/StoreInstruction",
        "toString",
        "()Ljava/lang/String;"
};

extern const MethodConfig LoadInstructionConfig = {
        "com/ibm/wala/shrikeBT/LoadInstruction",
        "make",
        "(Ljava/lang/String;I)Lcom/ibm/wala/shrikeBT/LoadInstruction;"
};

extern const MethodConfig LoadInstructionToStringConfig = {
        "com/ibm/wala/shrikeBT/LoadInstruction",
        "toString",
        "()Ljava/lang/String;"
};

extern const MethodConfig BinaryOpInstructionConfig = {
        "com/ibm/wala/shrikeBT/BinaryOpInstruction",
        "make",
        "(Ljava/lang/String;Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;)Lcom/ibm/wala/shrikeBT/BinaryOpInstruction;"
};

extern const MethodConfig BinaryOpInstructionToStringConfig = {
        "com/ibm/wala/shrikeBT/BinaryOpInstruction",
        "toString",
        "()Ljava/lang/String;"
};

extern const MethodConfig ReturnInstructionConfig = {
        "com/ibm/wala/shrikeBT/ReturnInstruction",
        "make",
        "(Ljava/lang/String;)Lcom/ibm/wala/shrikeBT/ReturnInstruction;"
};

extern const MethodConfig ReturnInstructionToStringConfig = {
        "com/ibm/wala/shrikeBT/ReturnInstruction",
        "toString",
        "()Ljava/lang/String;"
};


/***** Field Config ******/

extern const FieldConfig AddOperatorConfig = {
        true,
        "com/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator",
        "ADD",
        "Lcom/ibm/wala/shrikeBT/IBinaryOpInstruction$Operator;"
};
#endif //OMR_OWLMETHODCONFIG_HPP
