//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "optimizer/OWLJNIClient.hpp"
/***** class Names *******/
extern const char* InstructionClassName;

/***** Method Config ******/

extern const MethodConfig ConstantInstructionConfig;

extern const MethodConfig StoreInstructionConfig;

extern const MethodConfig LoadInstructionConfig;

extern const MethodConfig BinaryOpInstructionConfig;

extern const MethodConfig ReturnInstructionConfig;

extern const MethodConfig GotoInstructionConfig;

extern const MethodConfig ConditionalBranchInstructionConfig;

extern const MethodConfig ComparisonInstructionConfig;

extern const MethodConfig ConversionInstructionConfig;

extern const MethodConfig UnaryOpInstructionConfig;

extern const MethodConfig InvokeInstructionConfig;

extern const MethodConfig SwapInstructionConfig;

extern const MethodConfig PopInstructionConfig;

extern const MethodConfig ArrayStoreInstructionConfig;

extern const MethodConfig ArrayLoadInstructionConfig;

extern const MethodConfig NewInstructionConfig;

extern const MethodConfig PrinterConfig;


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
