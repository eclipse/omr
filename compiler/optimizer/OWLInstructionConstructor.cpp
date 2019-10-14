//
// Created by Cijie Xia on 2019-09-29.
//

#include <stdlib.h>
#include <string.h>
#include "optimizer/OWLInstructionConstructor.hpp"
#include "optimizer/OWLJNIConfig.hpp"

TR_OWLInstructionConstructor::TR_OWLInstructionConstructor() {
    _jniClient = TR_OWLJNIClient::getInstance();
    _index = 0;
}

TR_OWLInstructionConstructor::~TR_OWLInstructionConstructor() {\
    /*** TODO: destructor of JNIClient has not been implemented yet ***/
    TR_OWLJNIClient::destroyInstance();
}

/*** Helper methods ***/

/*** WARNING: It is user's responsibility to free the memory whoever uses this method ***/
char * TR_OWLInstructionConstructor::getInstructionString(jobject instructionObj, WALA_Instruction ins) {
    MethodConfig methodConfig;
    switch(ins){
        case CONSTANT: methodConfig = ConstantInstructionToStringConfig; break;
        case BINARY_OP: methodConfig = BinaryOpInstructionToStringConfig; break;
        case UNARY_OP: methodConfig = UnaryOpInstructionToStringConfig; break;
        case LOAD: methodConfig = LoadInstructionToStringConfig; break;
        case STORE: methodConfig = StoreInstructionToStringConfig; break;
        case RETURN: methodConfig = ReturnInstructionToStringConfig; break;
        case GOTO: methodConfig = GotoInstructionToStringConfig; break;
        case CONDITIONAL_BRANCH: methodConfig = ConstantInstructionToStringConfig; break;
        case COMPARISON: methodConfig = ComparisonInstructionToStringConfig; break;
        case CONVERSION: methodConfig = ConversionInstructionToStringConfig; break;
        case INVOKE: methodConfig = InvokeInstructionToStringConfig; break;

        default: perror("Error: Instruction enum not found!\n"); exit(1); break;
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

jobject TR_OWLInstructionConstructor::Integer(int32_t i) {
    return _jniClient->constructObject(i);
}

jobject TR_OWLInstructionConstructor::Float(float f) {
    return _jniClient->constructObject(f);
}

jobject TR_OWLInstructionConstructor::Double(double d) {
    return _jniClient->constructObject(d);
}

jobject TR_OWLInstructionConstructor::Short(int16_t s) {
    return _jniClient->constructObject(s);
}

jobject TR_OWLInstructionConstructor::Long(int64_t l) {
    return _jniClient->constructObject(l);
}

jobject TR_OWLInstructionConstructor::Operator(WALA_Operator op) {

    jobject opr;
    switch (op){
        case ADD: _jniClient->getField(ADD_OperatorConfig,NULL,&opr); break;
        case SUB: _jniClient->getField(SUB_OperatorConfig,NULL,&opr); break;
        case MUL: _jniClient->getField(MUL_OperatorConfig,NULL,&opr); break;
        case DIV: _jniClient->getField(DIV_OperatorConfig,NULL,&opr); break;
        case REM: _jniClient->getField(REM_OperatorConfig,NULL,&opr); break;
        case AND: _jniClient->getField(AND_OperatorConfig,NULL,&opr); break;
        case OR: _jniClient->getField(OR_OperatorConfig,NULL,&opr); break;
        case XOR: _jniClient->getField(XOR_OperatorConfig,NULL,&opr); break;

        case EQ: _jniClient->getField(EQ_OperatorConfig,NULL,&opr); break;
        case NE: _jniClient->getField(NE_OperatorConfig,NULL,&opr); break;
        case LT: _jniClient->getField(LT_OperatorConfig,NULL,&opr); break;
        case GE: _jniClient->getField(GE_OperatorConfig,NULL,&opr); break;
        case GT: _jniClient->getField(GT_OperatorConfig,NULL,&opr); break;
        case LE: _jniClient->getField(LE_OperatorConfig,NULL,&opr); break;

        case CMP: _jniClient->getField(CMP_OperatorConfig,NULL,&opr); break;
        case CMPL: _jniClient->getField(CMPL_OperatorConfig,NULL,&opr); break;
        case CMPG: _jniClient->getField(CMPG_OperatorConfig,NULL,&opr); break;

        default:
            perror("Error: Operator not found!\n");
            exit(1);
    }

    return opr;
}

jobject TR_OWLInstructionConstructor::Dispatch(WALA_Dispatch disp) {
    jobject dis;
    switch(disp) {
        case VIRTUAL: _jniClient->getField(VIRTUAL_DispatchConfig,NULL,&dis); break;
        case SPECIAL: _jniClient->getField(SPECIAL_DispatchConfig,NULL,&dis); break;
        case INTERFACE: _jniClient->getField(INTERFACE_DispatchConfig,NULL,&dis); break;
        case STATIC: _jniClient->getField(STATIC_DispatchConfig,NULL,&dis); break;
        default:
        perror("Error not dispatch found!\n");
        exit(1);
    }

    return dis;
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

jobject TR_OWLInstructionConstructor::StoreInstruction(char *type, TR::SymbolReference * symbolReference) {
    jobject storeInstructionObject;

    std::unordered_map<TR::SymbolReference * ,uint32_t >::const_iterator it = localVariableTable.find(symbolReference);

    int i = _index;

    if (it != localVariableTable.end()) {
        i = localVariableTable[symbolReference];
    }

    _jniClient->callMethod
    (
            StoreInstructionConfig,
            NULL,
            &storeInstructionObject,
            2,
            _jniClient->constructString(type),
            i
    );

    if (it == localVariableTable.end()) {

        localVariableTable[symbolReference] = _index;

        if (strcmp(TYPE_double,type) == 0 || strcmp(TYPE_long,type) == 0){
            _index += 2;
        }
        else{
            _index += 1;
        }
    }

    return storeInstructionObject;
}

jobject TR_OWLInstructionConstructor::LoadInstruction(char *type, TR::SymbolReference * symbolReference) {
    jobject loadInstructionObject;
    _jniClient->callMethod
    (
            LoadInstructionConfig,
            NULL,
            &loadInstructionObject,
            2,
            _jniClient->constructString(type),
            localVariableTable[symbolReference]
    );
    return loadInstructionObject;
}

jobject TR_OWLInstructionConstructor::BinaryOpInstruction(char* type, WALA_Operator op) {
    jobject binaryOpInstruction;

    _jniClient->callMethod
    (
            BinaryOpInstructionConfig,
            NULL,
            &binaryOpInstruction,
            2,
            _jniClient->constructString(type),
            Operator(op)
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

jobject TR_OWLInstructionConstructor::GotoInstruction(uint32_t label) {
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

jobject TR_OWLInstructionConstructor::ConditionalBranchInstruction(char *type, WALA_Operator op, uint32_t label) {
    jobject conditionalBranchInstruction;

    _jniClient->callMethod
    (
        ConditionalBranchInstructionConfig,
        NULL,
        &conditionalBranchInstruction,
        3,
        _jniClient->constructString(type),
        Operator(op),
        label
    );

    return conditionalBranchInstruction;
}

jobject TR_OWLInstructionConstructor::ComparisonInstruction(char *type, WALA_Operator op) {
    jobject comparisonInstruction;

    _jniClient->callMethod
    (
        ComparisonInstructionConfig,
        NULL,
        &comparisonInstruction,
        2,
        _jniClient->constructString(type),
        Operator(op)

    );

    return comparisonInstruction;
}


jobject TR_OWLInstructionConstructor::ConversionInstruction(char *fromType, char *toType) {
    jobject conversionInstruction;

    _jniClient->callMethod
    (
        ConversionInstructionConfig,
        NULL,
        &conversionInstruction,
        2,
        _jniClient->constructString(fromType),
        _jniClient->constructString(toType)
    );

    return conversionInstruction;
}


jobject TR_OWLInstructionConstructor::UnaryOpInstruction(char *type) {
    jobject unaryOpInstruction;

    _jniClient->callMethod
    (
        UnaryOpInstructionConfig,
        NULL,
        &unaryOpInstruction,
        1,
        _jniClient->constructString(type)
    );

    return unaryOpInstruction;
}

jobject TR_OWLInstructionConstructor::InvokeInstruction(char* type, char* className, char* methodName, WALA_Dispatch disp) {
    jobject invokeInstruction;

    _jniClient->callMethod
    (
        InvokeInstructionConfig,
        NULL,
        &invokeInstruction,
        4,
        _jniClient->constructString(type),
        _jniClient->constructString(className),
        _jniClient->constructString(methodName),
        Dispatch(disp)
    );

    return invokeInstruction;
}