//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMETHODCONFIG_HPP
#define OMR_OWLMETHODCONFIG_HPP

#include "optimizer/OWLJNIClient.hpp"
/***** class Names *******/
extern const char* InstructionClassName;
extern const char* ExceptionHandlerClassName;

/***** Method Config ******/

extern const JNIMethodConfig ConstantInstruction_make_Config;

extern const JNIMethodConfig StoreInstruction_make_Config;

extern const JNIMethodConfig LoadInstruction_make_Config;

extern const JNIMethodConfig BinaryOpInstruction_make_Config;

extern const JNIMethodConfig ReturnInstruction_make_Config;

extern const JNIMethodConfig GotoInstruction_make_Config;

extern const JNIMethodConfig ConditionalBranchInstruction_make_Config;

extern const JNIMethodConfig ComparisonInstruction_make_Config;

extern const JNIMethodConfig ConversionInstruction_make_Config;

extern const JNIMethodConfig UnaryOpInstruction_make_Config;

extern const JNIMethodConfig InvokeInstruction_make_Config;

extern const JNIMethodConfig SwapInstruction_make_Config;

extern const JNIMethodConfig PopInstruction_make_Config;

extern const JNIMethodConfig ArrayStoreInstruction_make_Config;

extern const JNIMethodConfig ArrayLoadInstruction_make_Config;

extern const JNIMethodConfig NewInstruction_make_Config;

extern const JNIMethodConfig PutInstruction_make_Config;

extern const JNIMethodConfig GetInstruction_make_Config;

extern const JNIMethodConfig DupInstruction_make_Config;

extern const JNIMethodConfig InstanceofInstruction_make_Config;

extern const JNIMethodConfig ArrayLengthInstruction_make_Config;

extern const JNIMethodConfig ShiftInstruction_make_Config;

extern const JNIMethodConfig SwitchInstruction_make_Config;

extern const JNIMethodConfig CheckCastInstruction_make_Config;

extern const JNIMethodConfig Printer_print_Config;

extern const JNIMethodConfig Verifier_verify_Config;

/*** Constructor Config ***/

extern const JNIConstructorConfig Verifier;

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
