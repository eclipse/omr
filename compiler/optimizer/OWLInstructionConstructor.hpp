//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
#define OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP

#include "OWLJNIClient.hpp"
enum Op {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    AND,
    OR,
    XOR
};

enum Instruction{
    CONST,
    STORE,
    LOAD,
    BINARY_OP,
    RETURN,
    GOTO
};

class TR_OWLInstructionConstructor
{
private:
    TR_OWLJNIClient *_jniClient;
    int32_t _index;
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
    jobject Operator(Op op);

    /*** WALA Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type);
    jobject LoadInstruction(char* type);
    jobject BinaryOpInstruction(char* type, jobject op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(int label);

};
#endif //OMR_OWLINSTRUCTIONCONSTRUCTOR_HPP
