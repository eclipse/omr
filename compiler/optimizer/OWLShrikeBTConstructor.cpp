//
// Created by Cijie Xia on 2019-09-29.
//

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "optimizer/OWLShrikeBTConstructor.hpp"
#include "optimizer/OWLJNIConfig.hpp"

TR_OWLShrikeBTConstructor::TR_OWLShrikeBTConstructor(TR_OWLJNIClient* jniClient) {
    _jniClient = jniClient;
}

TR_OWLShrikeBTConstructor::~TR_OWLShrikeBTConstructor() {}

std::vector<jobject> TR_OWLShrikeBTConstructor::constructShrikeBTInstructions(std::vector<OWLInstruction> owlInstructions) {

    std::vector<jobject> shrikeBTInstructions;

    for (uint32_t i = 0 ; i < owlInstructions.size(); i ++ ) {
        
        OWLInstruction owlInstruction = owlInstructions[i];
        bool isShrikeBTInstruction = owlInstruction.isShrikeBTInstruction;
        if (isShrikeBTInstruction){

            ShrikeBTInstruction instruction = owlInstruction.instruction;
            ShrikeBTInstructionFieldsUnion instrUnion = owlInstruction.instructionFieldsUnion;

            jobject instructionObject;

            switch(instruction){
                case SHRIKE_BT_CONSTANT: {
                    ConstantInstructionFields constFields = instrUnion.constantInstructionFields;
                    jobject value;
                    char* type = constFields.type;
                    if (!strcmp(TYPE_int, type)){
                        value = Integer(constFields.value.i);
                    }
                    else if (!strcmp(TYPE_long, type)){
                        value = Long(constFields.value.l);
                    }
                    else if (!strcmp(TYPE_double, type)){
                        value = Double(constFields.value.d);
                    }
                    else if (!strcmp(TYPE_float, type)) {
                        value = Float(constFields.value.f);
                    }
                    else if (!strcmp(TYPE_null, type)) { // const null
                        value = nullptr; 
                    }
                    else{
                        perror("NO type match in constructing const instruction object!\n");
                        exit(1);
                    }

                    instructionObject = ConstantInstruction(type, value);
                    break;
                }
                case SHRIKE_BT_STORE: {
                    StoreInstructionFields storeFields = instrUnion.storeInstructionFields;
                    instructionObject = StoreInstruction(storeFields.type, storeFields.index);
                    break;
                }
                case SHRIKE_BT_LOAD: {
                    LoadInstructionFields loadFields = instrUnion.loadInstructionFields;
                    instructionObject = LoadInstruction(loadFields.type,loadFields.index);
                    break;
                }
                case SHRIKE_BT_BINARY_OP: {
                    BinaryOpInstructionFields binaryFields = instrUnion.binaryOpInstructionFields;
                    instructionObject = BinaryOpInstruction(binaryFields.type,binaryFields.op);
                    break;
                }
                case SHRIKE_BT_UNARY_OP: {
                    UnaryOpInstructionFields unaryFields = instrUnion.unaryOpInstructionFields;
                    instructionObject = UnaryOpInstruction(unaryFields.type);
                    break;
                }
                case SHRIKE_BT_RETURN: {
                    ReturnInstructionFields returnFields = instrUnion.returnInstructionFields;
                    instructionObject = ReturnInstruction(returnFields.type);
                    break;
                }
                case SHRIKE_BT_GOTO: {
                    GotoInstructionFields gotoFields = instrUnion.gotoInstructionFields;
                    instructionObject = GotoInstruction(gotoFields.label);
                    break;
                }
                case SHRIKE_BT_CONDITIONAL_BRANCH: {
                    ConditionalBranchInstructionFields condiFields = instrUnion.conditionalBranchInstructionFields;
                    instructionObject = ConditionalBranchInstruction(condiFields.type,condiFields.op,condiFields.label);
                    break;
                }
                case SHRIKE_BT_COMPARISON: {
                    ComparisonInstructionFields compaFields = instrUnion.comparisonInstructionFields;
                    instructionObject = ComparisonInstruction(compaFields.type,compaFields.op);
                    break;
                }
                case SHRIKE_BT_CONVERSION: {
                    ConversionInstructionFields conversionFields = instrUnion.conversionInstructionFields;
                    instructionObject = ConversionInstruction(conversionFields.fromType,conversionFields.toType);
                    break;
                }
                case SHRIKE_BT_INVOKE: {
                    InvokeInstructionFields invokeFields = instrUnion.invokeInstructionFields;
                    instructionObject = InvokeInstruction(invokeFields.type,invokeFields.className,invokeFields.methodName,invokeFields.disp);
                    break;
                }
                case SHRIKE_BT_SWAP:{
                    SwapInstructionFields swapFields = instrUnion.swapInstructionFields;
                    instructionObject = SwapInstruction();
                    break;
                }
                case SHRIKE_BT_POP: {
                    PopInstructionFields popFields = instrUnion.popInstructionFields;
                    instructionObject = PopInstruction(popFields.size);
                    break;
                }
                case SHRIKE_BT_DUP: {
                    DupInstructionFields dupFields = instrUnion.dupInstructionFields;
                    instructionObject = DupInstruction(dupFields.delta);
                    break;
                }
                case SHRIKE_BT_ARRAY_STORE: {
                    ArrayStoreInstructionFields arrayStoreFields = instrUnion.arrayStoreInstructionFields;
                    instructionObject = ArrayStoreInstruction(arrayStoreFields.type);
                    break;
                }
                case SHRIKE_BT_ARRAY_LOAD: {
                    ArrayLoadInstructionFields arrayLoadFields = instrUnion.arrayLoadInstructionFields;
                    instructionObject = ArrayLoadInstruction(arrayLoadFields.type);
                    break;
                }
                case SHRIKE_BT_ARRAY_NEW: {
                    NewInstructionFields newInstructionFields = instrUnion.newInstructionFields;
                    instructionObject = NewInstruction(newInstructionFields.type, newInstructionFields.arrayBoundsCount);
                    break;
                }
                case SHRIKE_BT_PUT:{
                    PutInstructionFields putFields = instrUnion.putInstructionFields;
                    instructionObject = PutInstruction(putFields.type,putFields.className, putFields.fieldName, putFields.isStatic);
                    break;
                }
                case SHRIKE_BT_GET:{
                    GetInstructionFields getFields = instrUnion.getInstructionFields;
                    instructionObject = GetInstruction(getFields.type, getFields.className, getFields.fieldName, getFields.isStatic);
                    break;
                }
                case SHRIKE_BT_INSTANCE_OF:{
                    InstanceofInstructionFields instanceofFields = instrUnion.instanceofInstructionFields;
                    instructionObject = InstanceofInstruction(instanceofFields.type);
                    break;
                }
                case SHRIKE_BT_ARRAY_LENGTH:{
                    instructionObject = ArrayLengthInstruction();
                    break;
                }
                case SHRIKE_BT_SHIFT:{
                    ShiftInstructionFields shiftFields = instrUnion.shiftInstructionFields;
                    instructionObject = ShiftInstruction(shiftFields.type, shiftFields.op);
                    break;
                }
                case SHRIKE_BT_SWITCH:{
                    SwitchInstructionFields switchFields = instrUnion.switchInstructionFields;
                    instructionObject = SwitchInstruction(switchFields.casesAndLabels, switchFields.length, switchFields.defaultLabel);
                    break;
                }
                case SHRIKE_BT_CHECK_CAST: {
                    CheckCastInstructionFields checkCastFields = instrUnion.checkCastInstructionFields;
                    instructionObject = CheckCastInstruction(checkCastFields.type);
                    break;
                }
                default:
                    perror("No instruction matched inside construct instruction object function!\n");
                    exit(1);
            }
            
            shrikeBTInstructions.push_back(instructionObject);
        }
    }
    return shrikeBTInstructions;
}

/*** Helper methods ***/

jobject TR_OWLShrikeBTConstructor::Integer(int32_t i) {
    return _jniClient->newInteger(i);
}

jobject TR_OWLShrikeBTConstructor::Float(float f) {
    return _jniClient->newFloat(f);
}

jobject TR_OWLShrikeBTConstructor::Double(double d) {
    return _jniClient->newDouble(d);
}

jobject TR_OWLShrikeBTConstructor::Short(int16_t s) {
    return _jniClient->newShort(s);
}

jobject TR_OWLShrikeBTConstructor::Long(int64_t l) {
    return _jniClient->newLong(l);
}

jobject TR_OWLShrikeBTConstructor::BinaryOperator(ShrikeBTBinaryOperator op) {

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

        default:
            perror("Error: Binary Operator not found!\n");
            exit(1);
    }

    return opr;
}

jobject TR_OWLShrikeBTConstructor::ConditionalBranchOperator(ShrikeBTConditionalBranchOperator op) {

    jobject opr;
    switch (op){

        case EQ: _jniClient->getField(EQ_OperatorConfig,NULL,&opr); break;
        case NE: _jniClient->getField(NE_OperatorConfig,NULL,&opr); break;
        case LT: _jniClient->getField(LT_OperatorConfig,NULL,&opr); break;
        case GE: _jniClient->getField(GE_OperatorConfig,NULL,&opr); break;
        case GT: _jniClient->getField(GT_OperatorConfig,NULL,&opr); break;
        case LE: _jniClient->getField(LE_OperatorConfig,NULL,&opr); break;

        default:
            perror("Error: Conditional Branch Operator not found!\n");
            exit(1);
    }

    return opr;
}

jobject TR_OWLShrikeBTConstructor::ComparisonOperator(ShrikeBTComparisonOperator op) {
    jobject opr;
    switch (op){

        case CMP: _jniClient->getField(CMP_OperatorConfig,NULL,&opr); break;
        case CMPL: _jniClient->getField(CMPL_OperatorConfig,NULL,&opr); break;
        case CMPG: _jniClient->getField(CMPG_OperatorConfig,NULL,&opr); break;

        default:
            perror("Error: Comparison Operator not found!\n");
            exit(1);
    }

    return opr;
}

jobject TR_OWLShrikeBTConstructor::ShiftOperator(ShrikeBTShiftOperator op) {
    jobject opr;
    switch (op){
   
        case SHL: _jniClient->getField(SHL_OperatorConfig,NULL,&opr); break;
        case SHR: _jniClient->getField(SHR_OperatorConfig,NULL,&opr); break;
        case USHR: _jniClient->getField(USHR_OperatorConfig,NULL,&opr); break;

        default:
            perror("Error: Shift Operator not found!\n");
            exit(1);
    }

    return opr;
}

jobject TR_OWLShrikeBTConstructor::Dispatch(ShrikeBTDispatch disp) {
    jobject dis;
    switch(disp) {
        case VIRTUAL: _jniClient->getField(VIRTUAL_DispatchConfig,NULL,&dis); break;
        case SPECIAL: _jniClient->getField(SPECIAL_DispatchConfig,NULL,&dis); break;
        case INTERFACE: _jniClient->getField(INTERFACE_DispatchConfig,NULL,&dis); break;
        case STATIC: _jniClient->getField(STATIC_DispatchConfig,NULL,&dis); break;
        default:
        perror("Error no dispatch found!\n");
        exit(1);
    }

    return dis;
}

/*** ShrikeBT Instruction Constructors ***/
jobject TR_OWLShrikeBTConstructor::ConstantInstruction(char *type, jobject value) {
    jobject constantInstructionObject;

    _jniClient->callMethod
    (
        ConstantInstruction_make_Config,
        NULL,
        &constantInstructionObject,
        2,
        _jniClient->newString(type),
        value
    );
    return constantInstructionObject;
}

jobject TR_OWLShrikeBTConstructor::StoreInstruction(char *type, uint32_t index) {
    jobject storeInstructionObject;

    _jniClient->callMethod
    (
        StoreInstruction_make_Config,
        NULL,
        &storeInstructionObject,
        2,
        _jniClient->newString(type),
        index
    );

    return storeInstructionObject;
}


jobject TR_OWLShrikeBTConstructor::LoadInstruction(char *type, uint32_t index) {
    jobject loadInstructionObject;
    _jniClient->callMethod
    (
        LoadInstruction_make_Config,
        NULL,
        &loadInstructionObject,
        2,
        _jniClient->newString(type),
        index
    );
    return loadInstructionObject;
}

jobject TR_OWLShrikeBTConstructor::BinaryOpInstruction(char* type, ShrikeBTBinaryOperator op) {
    jobject binaryOpInstruction;

    _jniClient->callMethod
    (
        BinaryOpInstruction_make_Config,
        NULL,
        &binaryOpInstruction,
        2,
        _jniClient->newString(type),
        BinaryOperator(op)
    );
    return binaryOpInstruction;
}

jobject TR_OWLShrikeBTConstructor::ReturnInstruction(char* type) {
    jobject returnInstruction;

    _jniClient->callMethod
    (
        ReturnInstruction_make_Config,
        NULL,
        &returnInstruction,
        1,
        _jniClient->newString(type)
    );

    return returnInstruction;
}

jobject TR_OWLShrikeBTConstructor::GotoInstruction(uint32_t label) {
    jobject gotoInstruction;

    _jniClient->callMethod
    (
        GotoInstruction_make_Config,
        NULL,
        &gotoInstruction,
        1,
        label
    );

    return gotoInstruction;
}

jobject TR_OWLShrikeBTConstructor::ConditionalBranchInstruction(char *type, ShrikeBTConditionalBranchOperator op, uint32_t label) {
    jobject conditionalBranchInstruction;

    _jniClient->callMethod
    (
        ConditionalBranchInstruction_make_Config,
        NULL,
        &conditionalBranchInstruction,
        3,
        _jniClient->newString(type),
        ConditionalBranchOperator(op),
        label
    );

    return conditionalBranchInstruction;
}

jobject TR_OWLShrikeBTConstructor::ComparisonInstruction(char *type, ShrikeBTComparisonOperator op) {
    jobject comparisonInstruction;

    _jniClient->callMethod
    (
        ComparisonInstruction_make_Config,
        NULL,
        &comparisonInstruction,
        2,
        _jniClient->newString(type),
        ComparisonOperator(op)

    );

    return comparisonInstruction;
}

jobject TR_OWLShrikeBTConstructor::ConversionInstruction(char *fromType, char *toType) {
    jobject conversionInstruction;

    _jniClient->callMethod
    (
        ConversionInstruction_make_Config,
        NULL,
        &conversionInstruction,
        2,
        _jniClient->newString(fromType),
        _jniClient->newString(toType)
    );

    return conversionInstruction;
}


jobject TR_OWLShrikeBTConstructor::UnaryOpInstruction(char *type) {
    jobject unaryOpInstruction;

    _jniClient->callMethod
    (
        UnaryOpInstruction_make_Config,
        NULL,
        &unaryOpInstruction,
        1,
        _jniClient->newString(type)
    );

    return unaryOpInstruction;
}

jobject TR_OWLShrikeBTConstructor::InvokeInstruction(char* type, char* className, char* methodName, ShrikeBTDispatch disp) {
    jobject invokeInstruction;

    _jniClient->callMethod
    (
        InvokeInstruction_make_Config,
        NULL,
        &invokeInstruction,
        4,
        _jniClient->newString(type),
        _jniClient->newString(className),
        _jniClient->newString(methodName),
        Dispatch(disp)
    );

    return invokeInstruction;
}

jobject TR_OWLShrikeBTConstructor::SwapInstruction() {
    jobject swapInstruction;

    _jniClient->callMethod
    (
        SwapInstruction_make_Config,
        NULL,
        &swapInstruction,
        0
    );

    return swapInstruction;
}

jobject TR_OWLShrikeBTConstructor::PopInstruction(uint16_t size) {
    jobject popInstruction;

    _jniClient->callMethod
    (
        PopInstruction_make_Config,
        NULL,
        &popInstruction,
        1,
        size
    );

    return popInstruction;
}

jobject TR_OWLShrikeBTConstructor::ArrayStoreInstruction(char* type) {
    jobject arrayStoreInstruction;

    _jniClient->callMethod
    (
        ArrayStoreInstruction_make_Config,
        NULL,
        &arrayStoreInstruction,
        1,
        _jniClient->newString(type)
    );

    return arrayStoreInstruction;
}

jobject TR_OWLShrikeBTConstructor::ArrayLoadInstruction(char* type) {
    jobject arrayLoadInstruction;

    _jniClient->callMethod
    (
        ArrayLoadInstruction_make_Config,
        NULL,
        &arrayLoadInstruction,
        1,
        _jniClient->newString(type)
    );

    return arrayLoadInstruction;
}

jobject TR_OWLShrikeBTConstructor::NewInstruction(char* type, int32_t arrayBoundsCount){
    jobject newInstruction;

    _jniClient->callMethod
    (
        NewInstruction_make_Config,
        NULL,
        &newInstruction,
        2,
        _jniClient->newString(type),
        arrayBoundsCount
    );

    return newInstruction;
}

jobject TR_OWLShrikeBTConstructor::PutInstruction(char* type, char* className, char* fieldName, bool isStatic){
    jobject putInstruction;

    _jniClient->callMethod
    (
        PutInstruction_make_Config,
        NULL,
        &putInstruction,
        4,
        _jniClient->newString(type),
        _jniClient->newString(className),
        _jniClient->newString(fieldName),
        isStatic ? JNI_TRUE : JNI_FALSE
    );
    
    return putInstruction;
}

jobject TR_OWLShrikeBTConstructor::GetInstruction(char* type, char* className, char* fieldName, bool isStatic) {
    jobject getInstruction;

    _jniClient->callMethod
    (
        GetInstruction_make_Config,
        NULL,
        &getInstruction,
        4,
        _jniClient->newString(type),
        _jniClient->newString(className),
        _jniClient->newString(fieldName),
        isStatic ? JNI_TRUE : JNI_FALSE
    );

    return getInstruction;
}

jobject TR_OWLShrikeBTConstructor::DupInstruction(uint16_t delta) {
    jobject dupInstruction;

    _jniClient->callMethod
    (
        DupInstruction_make_Config,
        NULL,
        &dupInstruction,
        1,
        delta
    );

    return dupInstruction;
}

jobject TR_OWLShrikeBTConstructor::InstanceofInstruction(char *type) {
    jobject instanceofInstruction;
    
    _jniClient->callMethod
    (
        InstanceofInstruction_make_Config,
        NULL,
        &instanceofInstruction,
        1,
        _jniClient->newString(type)

    );

    return instanceofInstruction;
}

jobject TR_OWLShrikeBTConstructor::ArrayLengthInstruction() {
    jobject arrayLengthInstruction;

    _jniClient->callMethod
    (
        ArrayLengthInstruction_make_Config,
        NULL,
        &arrayLengthInstruction,
        0
    );

    return arrayLengthInstruction;
}

jobject TR_OWLShrikeBTConstructor::ShiftInstruction(char* type, ShrikeBTShiftOperator op) {
    jobject shiftOperator;

    _jniClient->callMethod
    (
        ShiftInstruction_make_Config,
        NULL,
        &shiftOperator,
        2,
        _jniClient->newString(type), 
        ShiftOperator(op)
    );

    return shiftOperator;
}

jobject TR_OWLShrikeBTConstructor::SwitchInstruction(int* casesAndLabels, int length, int defaultLabel) {
    jobject switchInstruction;

    _jniClient->callMethod
    (
        SwitchInstruction_make_Config,
        NULL,
        &switchInstruction,
        2,
        _jniClient->newIntegerArray(casesAndLabels, length),
        defaultLabel
    );

    return switchInstruction;
}

jobject TR_OWLShrikeBTConstructor::CheckCastInstruction(char* type) {
    jobject checkCastInstruction;

    _jniClient->callMethod
    (
        CheckCastInstruction_make_Config,
        NULL,
        &checkCastInstruction,
        1,
        _jniClient->newString(type)
    );

    return checkCastInstruction;
}