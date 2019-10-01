//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
#define OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP

#include <unordered_map>
#include "OWLJNIClient.hpp"

enum Op {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    AND,
    OR,
    XOR,

    EQ,
    NE,
    LT,
    GE,
    GT,
    LE,

    CMP,
    CMPL,
    CMPG
};

enum Instruction{
    CONST,
    STORE,
    LOAD,
    BINARY_OP,
    RETURN,
    GOTO,
    CONDITIONAL_BRANCH,
    COMPARISON
};

class TR_OWLInstructionConstructor
{
private:
    TR_OWLJNIClient *_jniClient;
    int32_t _index;
    std::unordered_map<int,int> localVariableTable;

public:
    TR_OWLInstructionConstructor();
    ~TR_OWLInstructionConstructor();

    /*** helper methods ***/
    char* getInstructionString(jobject instructionObj, Instruction ins);
    jobject Integer(int i);
    jobject Float(float f);
    jobject Double(double d);
    jobject Long(long l);
    jobject Short(short s);
    jobject Operator(Op  op);

    /*** WALA Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type, int referenceNumber);
    jobject LoadInstruction(char* type, int referenceNumber);
    jobject BinaryOpInstruction(char* type, Op op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(int label);
    jobject ConditionalBranchInstruction(char* type, Op op, int label);
    jobject ComparisonInstruction(char* type, Op op);

};
#endif //OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
