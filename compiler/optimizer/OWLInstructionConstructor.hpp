//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
#define OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP

#include <unordered_map>
#include "il/OMRSymbolReference.hpp"
#include "OWLJNIClient.hpp"


enum Op {
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

enum Instruction {
    CONSTANT,
    STORE,
    LOAD,
    BINARY_OP,
    UNARY_OP,
    RETURN,
    GOTO,
    CONDITIONAL_BRANCH,
    COMPARISON,
    CONVERSION
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
    char* getInstructionString(jobject instructionObj, Instruction ins);
    jobject Integer(int32_t i);
    jobject Float(float f);
    jobject Double(double d);
    jobject Long(int64_t l);
    jobject Short(int16_t s);
    jobject Operator(Op  op);

    /*** WALA Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject LoadInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject BinaryOpInstruction(char* type, Op op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(uint32_t label);
    jobject ConditionalBranchInstruction(char* type, Op op, uint32_t label);
    jobject ComparisonInstruction(char* type, Op op);
    jobject ConversionInstruction(char* fromType, char* toType);
    jobject UnaryOpInstruction(char* type);

};
#endif //OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
