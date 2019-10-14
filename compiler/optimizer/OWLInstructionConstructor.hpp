//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
#define OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP

#include <unordered_map>
#include "il/OMRSymbolReference.hpp"
#include "optimizer/OWLJNIClient.hpp"


enum WALA_Instruction {
    CONSTANT,
    STORE,
    LOAD,
    BINARY_OP,
    UNARY_OP,
    RETURN,
    GOTO,
    CONDITIONAL_BRANCH,
    COMPARISON,
    CONVERSION,
    INVOKE
};

enum WALA_Operator {
    //binary op
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    AND,
    OR,
    XOR,
    //unary op
    NEG,
    //conditional branch op
    EQ,
    NE,
    LT,
    GE,
    GT,
    LE,
    //comparison op
    CMP,
    CMPL,
    CMPG
};

enum WALA_Dispatch {
    VIRTUAL,
    SPECIAL,
    INTERFACE,
    STATIC 
};

struct ConstantInstructionFields {
    char* type;
    jobject value;
};

struct StoreInstructionFields {
    char* type;
    TR::SymbolReference * symbolReference;
};

struct LoadInstructionFields {
    char* type;
    TR::SymbolReference * symbolReference;
};

struct BinaryOpInstructionFields {
    char* type;
    WALA_Operator op;
};

struct ReturnInstructionFields {
    char* type;
};

struct GotoInstructionFields {
    uint32_t label;
};

struct ConditionalBranchInstructionFields {
    char* type;
    WALA_Operator op;
    uint32_t label;
};

struct ComparisonInstructionFields {
    char* type;
    WALA_Operator op;
};

struct ConversionInstructionFields {
    char* fromType;
    char* toType;
};

struct UnaryOpInstructionFields {
    char* type;
};

struct InvokeInstructionFields {
    char* type;
    char* className; char* methodName;
    WALA_Dispatch disp;
};

union WALA_InstructionFieldsUnion {
    ConstantInstructionFields constantInstructionFields;
    StoreInstructionFields storeInstructionFields;
    LoadInstructionFields loadInstructionFields;
    BinaryOpInstructionFields binaryOpInstructionFields;
    ReturnInstructionFields returnInstructionFields;
    GotoInstructionFields gotoInstructionFields;
    ConditionalBranchInstructionFields conditionalBranchInstructionFields;
    ComparisonInstructionFields comparisonInstructionFields;
    ConversionInstructionFields conversionInstructionFields;
    UnaryOpInstructionFields unaryOpInstructionFields;
    InvokeInstructionFields invokeInstructionFields;
};

class TR_OWLInstructionConstructor
{
private:
    TR_OWLJNIClient *_jniClient;
    uint32_t _index;
    std::unordered_map<TR::SymbolReference *, uint32_t> localVariableTable;
public:
    TR_OWLInstructionConstructor();
    ~TR_OWLInstructionConstructor();

    /*** helper methods ***/
    char* getInstructionString(jobject instructionObj, WALA_Instruction ins);
    jobject Integer(int32_t i);
    jobject Float(float f);
    jobject Double(double d);
    jobject Long(int64_t l);
    jobject Short(int16_t s);
    jobject Operator(WALA_Operator op);
    jobject Dispatch(WALA_Dispatch disp);

    /*** WALA Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject LoadInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject BinaryOpInstruction(char* type, WALA_Operator op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(uint32_t label);
    jobject ConditionalBranchInstruction(char* type, WALA_Operator op, uint32_t label);
    jobject ComparisonInstruction(char* type, WALA_Operator op);
    jobject ConversionInstruction(char* fromType, char* toType);
    jobject UnaryOpInstruction(char* type);
    jobject InvokeInstruction(char* type, char* className, char* methodName, WALA_Dispatch disp);

};
#endif //OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
