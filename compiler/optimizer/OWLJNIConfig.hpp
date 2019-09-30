//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "OWLJNIClient.hpp"

/* Types */
extern char* const TYPE_short;
extern char* const TYPE_long;
extern char* const TYPE_int;
extern char* const TYPE_float;
extern char* const TYPE_double;
extern char* const TYPE_char;
extern char* const TYPE_byte;
extern char* const TYPE_void;
extern char* const TYPE_boolean;

/***** Method Config ******/

extern const MethodConfig ConstantInstructionConfig;
extern const MethodConfig ConstantInstructionToStringConfig;
extern const MethodConfig StoreInstructionConfig;
extern const MethodConfig StoreInstructionToStringConfig;
extern const MethodConfig LoadInstructionConfig;
extern const MethodConfig LoadInstructionToStringConfig;
extern const MethodConfig BinaryOpInstructionConfig;
extern const MethodConfig BinaryOpInstructionToStringConfig;
extern const MethodConfig ReturnInstructionConfig;
extern const MethodConfig ReturnInstructionToStringConfig;
extern const MethodConfig GotoInstructionConfig;
extern const MethodConfig GotoInstructionToStringConfig;


/***** Field Config ******/

extern const FieldConfig AddOperatorConfig;
extern const FieldConfig SubOperatorConfig;
extern const FieldConfig MulOperatorConfig;
extern const FieldConfig DivOperatorConfig;
extern const FieldConfig RemOperatorConfig;
extern const FieldConfig AndOperatorConfig;
extern const FieldConfig OrOperatorConfig;
extern const FieldConfig XorOperatorConfig;

#endif //OMR_OWLMETHODCONFIG_HPP
