//
// Created by Cijie Xia on 2019-09-29.
//

#include "OWLInstructionConstructor.hpp"
#include "OWLJNIConfig.hpp"
#include <stdlib.h>

TR_OWLInstructionConstructor::TR_OWLInstructionConstructor() {
    _jniClient = TR_OWLJNIClient::getInstance();
    _index = 0;
}

TR_OWLInstructionConstructor::~TR_OWLInstructionConstructor() {
    delete _jniClient;
}

/*** Helper methods ***/

/*** WARNING: It is user's responsibility to free the memory whoever uses this method ***/
char * TR_OWLInstructionConstructor::getInstructionString(jobject instructionObj, Instruction ins) {
    MethodConfig methodConfig;
    switch(ins){
        case CONST:
            methodConfig = ConstantInstructionToStringConfig;
            break;
        case BINARY_OP:
            methodConfig = BinaryOpInstructionToStringConfig;
            break;
        case LOAD:
            methodConfig = LoadInstructionToStringConfig;
            break;
        case STORE:
            methodConfig = StoreInstructionToStringConfig;
            break;
        case RETURN:
            methodConfig = ReturnInstructionToStringConfig;
            break;
        case GOTO:
            methodConfig = GotoInstructionToStringConfig;
            break;
        default:
            perror("Error: Instruction enum not found!\n");
            break;
    }

    char *instructionString = (char*)malloc(1024);

    _jniClient->callMethod
            (
                    methodConfig,
                    instructionObj,
                    &instructionString,
                    0
            );
    return instructionString;
}

jobject TR_OWLInstructionConstructor::Integer(int i) {
    return _jniClient->constructIntegerObject(i);
}

jobject TR_OWLInstructionConstructor::Float(float f) {
    return _jniClient->constructFloatObject(f);
}

jobject TR_OWLInstructionConstructor::Double(double d) {
    return _jniClient->constructDoubleObject(d);
}

jobject TR_OWLInstructionConstructor::Short(short s) {
    return _jniClient->constructShortObject(s);
}

jobject TR_OWLInstructionConstructor::Long(long l) {
    return _jniClient->constructLongObject(l);
}

jobject TR_OWLInstructionConstructor::Operator(Op op) {

    jobject opr;
    switch (op){
        case ADD:
            _jniClient->getField(AddOperatorConfig,NULL,&opr);
            break;
        case SUB:
            _jniClient->getField(SubOperatorConfig,NULL,&opr);
            break;
        case MUL:
            _jniClient->getField(MulOperatorConfig,NULL,&opr);
            break;
        case DIV:
            _jniClient->getField(DivOperatorConfig,NULL,&opr);
            break;
        case REM:
            _jniClient->getField(RemOperatorConfig,NULL,&opr);
            break;
        case AND:
            _jniClient->getField(AndOperatorConfig,NULL,&opr);
            break;
        case OR:
            _jniClient->getField(OrOperatorConfig,NULL,&opr);
            break;
        case XOR:
            _jniClient->getField(XorOperatorConfig,NULL,&opr);
            break;
        default:
            perror("Error: Operator not found!\n");
            exit(1);
            break;
    }

    return opr;
}

/*** WALA Instruction Constructors ***/
jobject TR_OWLInstructionConstructor::ConstantInstruction(char *type, jobject value) {
    jobject constantInstructionObject;

    _jniClient->callMethod
            (
                    ConstantInstructionConfig,
                    NULL,
                    &constantInstructionObject,
                    2,
                    _jniClient->constructString(type),
                    value
            );
    return constantInstructionObject;
}

jobject TR_OWLInstructionConstructor::StoreInstruction(char *type) {
    jobject storeInstructionObject;

    _jniClient->callMethod
            (
                    StoreInstructionConfig,
                    NULL,
                    &storeInstructionObject,
                    2,
                    _jniClient->constructString(type),
                    _index
            );

    return storeInstructionObject;
}

jobject TR_OWLInstructionConstructor::LoadInstruction(char *type) {
    jobject loadInstructionObject;

    _jniClient->callMethod
            (
                    LoadInstructionConfig,
                    NULL,
                    &loadInstructionObject,
                    2,
                    _jniClient->constructString(type),
                    _index
            );
    return loadInstructionObject;
}

jobject TR_OWLInstructionConstructor::BinaryOpInstruction(char* type, jobject op) {
    jobject binaryOpInstruction;

    _jniClient->callMethod
            (
                    BinaryOpInstructionConfig,
                    NULL,
                    &binaryOpInstruction,
                    2,
                    _jniClient->constructString(type),
                    op
            );
    return binaryOpInstruction;
}

jobject TR_OWLInstructionConstructor::ReturnInstruction(char* type) {
    jobject returnInstruction;

    _jniClient->callMethod
            (
                    ReturnInstructionConfig,
                    NULL,
                    &returnInstruction,
                    1,
                    _jniClient->constructString(type)
            );

    return returnInstruction;
}

jobject TR_OWLInstructionConstructor::GotoInstruction(int label) {
    jobject gotoInstruction;

    _jniClient->callMethod
    (
        GotoInstructionConfig,
        NULL,
        &gotoInstruction,
        1,
        label
    );

    return gotoInstruction;
}