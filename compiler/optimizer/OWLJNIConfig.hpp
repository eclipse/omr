//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "optimizer/OWLJNIClient.hpp"

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
extern char* const TYPE_Object;

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
extern const MethodConfig ConditionalBranchInstructionConfig;
extern const MethodConfig ConditionalBranchInstructionToStringConfig;
extern const MethodConfig ComparisonInstructionConfig;
extern const MethodConfig ComparisonInstructionToStringConfig;
extern const MethodConfig ConversionInstructionConfig;
extern const MethodConfig ConversionInstructionToStringConfig;
extern const MethodConfig UnaryOpInstructionConfig;
extern const MethodConfig UnaryOpInstructionToStringConfig;
extern const MethodConfig InvokeInstructionConfig;
extern const MethodConfig InvokeInstructionToStringConfig;

/***** Field Config ******/

extern const FieldConfig ADD_OperatorConfig;
extern const FieldConfig SUB_OperatorConfig;
extern const FieldConfig MUL_OperatorConfig;
extern const FieldConfig DIV_OperatorConfig;
extern const FieldConfig REM_OperatorConfig;
extern const FieldConfig AND_OperatorConfig;
extern const FieldConfig OR_OperatorConfig;
extern const FieldConfig XOR_OperatorConfig;

extern const FieldConfig EQ_OperatorConfig;
extern const FieldConfig NE_OperatorConfig;
extern const FieldConfig LT_OperatorConfig;
extern const FieldConfig GE_OperatorConfig;
extern const FieldConfig GT_OperatorConfig;
extern const FieldConfig LE_OperatorConfig;

extern const FieldConfig CMP_OperatorConfig;
extern const FieldConfig CMPL_OperatorConfig;
extern const FieldConfig CMPG_OperatorConfig;

extern const FieldConfig VIRTUAL_DispatchConfig;
extern const FieldConfig SPECIAL_DispatchConfig;
extern const FieldConfig INTERFACE_DispatchConfig;
extern const FieldConfig STATIC_DispatchConfig;


#endif //OMR_OWLMETHODCONFIG_HPP
