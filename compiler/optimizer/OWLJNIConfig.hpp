//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "optimizer/OWLJNIClient.hpp"
/***** class Names *******/
extern const char* InstructionClassName;

/***** Method Config ******/

extern const JNIMethodConfig ConstantInstructionConfig;

extern const JNIMethodConfig StoreInstructionConfig;

extern const JNIMethodConfig LoadInstructionConfig;

extern const JNIMethodConfig BinaryOpInstructionConfig;

extern const JNIMethodConfig ReturnInstructionConfig;

extern const JNIMethodConfig GotoInstructionConfig;

extern const JNIMethodConfig ConditionalBranchInstructionConfig;

extern const JNIMethodConfig ComparisonInstructionConfig;

extern const JNIMethodConfig ConversionInstructionConfig;

extern const JNIMethodConfig UnaryOpInstructionConfig;

extern const JNIMethodConfig InvokeInstructionConfig;

extern const JNIMethodConfig SwapInstructionConfig;

extern const JNIMethodConfig PopInstructionConfig;

extern const JNIMethodConfig ArrayStoreInstructionConfig;

extern const JNIMethodConfig ArrayLoadInstructionConfig;

extern const JNIMethodConfig NewInstructionConfig;

extern const JNIMethodConfig PutInstructionConfig;

extern const JNIMethodConfig GetInstructionConfig;

extern const JNIMethodConfig DupInstructionConfig;

extern const JNIMethodConfig InstanceofInstructionConfig;

extern const JNIMethodConfig ArrayLengthInstructionConfig;

extern const JNIMethodConfig ShiftInstructionConfig;

extern const JNIMethodConfig PrinterConfig;




/***** Field Config ******/

extern const JNIFieldConfig ADD_OperatorConfig;
extern const JNIFieldConfig SUB_OperatorConfig;
extern const JNIFieldConfig MUL_OperatorConfig;
extern const JNIFieldConfig DIV_OperatorConfig;
extern const JNIFieldConfig REM_OperatorConfig;
extern const JNIFieldConfig AND_OperatorConfig;
extern const JNIFieldConfig OR_OperatorConfig;
extern const JNIFieldConfig XOR_OperatorConfig;

extern const JNIFieldConfig EQ_OperatorConfig;
extern const JNIFieldConfig NE_OperatorConfig;
extern const JNIFieldConfig LT_OperatorConfig;
extern const JNIFieldConfig GE_OperatorConfig;
extern const JNIFieldConfig GT_OperatorConfig;
extern const JNIFieldConfig LE_OperatorConfig;

extern const JNIFieldConfig CMP_OperatorConfig;
extern const JNIFieldConfig CMPL_OperatorConfig;
extern const JNIFieldConfig CMPG_OperatorConfig;

extern const JNIFieldConfig SHL_OperatorConfig;
extern const JNIFieldConfig SHR_OperatorConfig;
extern const JNIFieldConfig USHR_OperatorConfig;

extern const JNIFieldConfig VIRTUAL_DispatchConfig;
extern const JNIFieldConfig SPECIAL_DispatchConfig;
extern const JNIFieldConfig INTERFACE_DispatchConfig;
extern const JNIFieldConfig STATIC_DispatchConfig;




#endif //OMR_OWLMETHODCONFIG_HPP
