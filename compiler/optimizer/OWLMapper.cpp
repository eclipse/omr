//
// Created by Cijie Xia on 2019-09-23.
//

#include <string.h>
#include <stdlib.h>
#include <tuple>
#include "il/ILHelpers.hpp"
#include "optimizer/OWLMapper.hpp"
#include "optimizer/OWLJNIConfig.hpp"



/****** public ******/

TR_OWLMapper::TR_OWLMapper() {

    _con = new TR_OWLShrikeBTConstructor();
    _logger = new TR_OWLLogger();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _con;
    delete _logger;
}

void TR_OWLMapper::map(TR::Compilation* compilation) {

    TR::NodeChecklist instructionMappingVisited(compilation); // visited nodes for mapping the instruction

    printf("======= OMR Instructions ========\n");

    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _processTree(node, instructionMappingVisited);
    }

    _adjustOffset();

    printf("======= ShrikeBT Instructions ========\n");

    _constructShrikeBTInstructionObjects();
}

/****** private ******/

void TR_OWLMapper::_processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root)){ // indicates this node has already been evaluated => load the value from local var table
        _createImplicitLoad(root);
        return;
    }
    
    visited.add(root);

    if (root->getOpCode().isTernary()){ // deal with ternary expression. Evaluate last two children first

        _processTree(root->getChild(1),visited);
        _processTree(root->getChild(2),visited);
        _processTree(root->getChild(0),visited);
        
    }
    else if (root->getOpCode().isStoreIndirect()) {
        
        // if (root->getChild(0)->getChild(0)->getChild(1)->getNumChildren() == 0) { //struct
        //     //_processTree(root->getChild(0)->getChild(0)->getChild(0),visited); // reference
        //     _processTree(root->getChild(0)->getChild(0)->getChild(1),visited);
        // }
        // else { // array
        //     //_processTree(root->getChild(0)->getChild(0)->getChild(0),visited); // reference
        //     _processTree(root->getChild(0)->getChild(0)->getChild(1)->getChild(0), visited);
        // }
        // //the value of indirect store
        // _processTree(root->getChild(1),visited);
    }
    else if (root->getOpCode().isLoadIndirect()) {
        
        if (root->getChild(0)->getChild(0)->getChild(1)->getNumChildren() == 0) { //struct
            _processTree(root->getChild(0)->getChild(0)->getChild(1),visited);
        }
        else { // array
            _processTree(root->getChild(0)->getChild(0)->getChild(1)->getChild(0), visited);
        }
    }
    else{
        for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
            _processTree(root->getChild(i), visited);
        }
    }

    _instructionRouter(root);
    
}

void TR_OWLMapper::_adjustOffset() {
    /**** build OMR index to shrikeBT offset table ****/
    std::unordered_map<uint32_t,uint32_t> offsetMappingTable;
    uint32_t offset = 0;
    for (uint32_t i = 0 ; i < _instructionMetaList.size(); i ++) {

        InstructionMeta instrMeta = _instructionMetaList[i];

        bool isShrikeBTInstruction = instrMeta.isShrikeBTInstruction;

        offsetMappingTable[instrMeta.omrGlobalIndex] = offset;
        if (isShrikeBTInstruction) {
            offset ++;
        }
    }
    
    offset = 0;
    /**** Adjust branch target label and instruction offsets ****/
    for (uint32_t i = 0 ; i < _instructionMetaList.size(); i ++) {
        InstructionMeta* instrMeta = &_instructionMetaList[i];
        bool isShrikeBTInstruction= instrMeta->isShrikeBTInstruction;
        ShrikeBTInstruction instruction = instrMeta->instruction;
        
        if (isShrikeBTInstruction) {
            BranchTargetLabelAdjustType adjustType = instrMeta->branchTargetLabelAdjustType;
            ShrikeBTInstructionFieldsUnion *fieldsUnion = &instrMeta->instructionFieldsUnion;
            uint32_t label;

            if (instruction == GOTO) {

                GotoInstructionFields *gotoFields = &fieldsUnion->gotoInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[gotoFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + instrMeta->branchTargetLabelAdjustAmount;
                }

                gotoFields->label = label;
               
            }
            else if (instruction == CONDITIONAL_BRANCH) {

                ConditionalBranchInstructionFields *condiFields = &fieldsUnion->conditionalBranchInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[condiFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + instrMeta->branchTargetLabelAdjustAmount;
                }

                condiFields->label = label;

            }
            
            instrMeta->shrikeBTOffset = offset;
            offset ++;
        }
    }

}

void TR_OWLMapper::_constructShrikeBTInstructionObjects() {

    std::vector<MappedInstructionObject> mappedInstructionObjectList;

    for (uint32_t i = 0 ; i < _instructionMetaList.size(); i ++ ) {
        
        InstructionMeta instrMeta = _instructionMetaList[i];
        bool isShrikeBTInstruction = instrMeta.isShrikeBTInstruction;
        if (isShrikeBTInstruction){

            ShrikeBTInstruction instruction = instrMeta.instruction;
            ShrikeBTInstructionFieldsUnion instrUnion = instrMeta.instructionFieldsUnion;
            uint32_t offset = instrMeta.shrikeBTOffset;

            jobject instructionObject;

            switch(instruction){
                case CONSTANT: {
                    ConstantInstructionFields constFields = instrUnion.constantInstructionFields;
                    instructionObject = _con->ConstantInstruction(constFields.type, constFields.value);
                    break;
                }
                case STORE: {
                    StoreInstructionFields storeFields = instrUnion.storeInstructionFields;
                    instructionObject = _con->StoreInstruction(storeFields.type, storeFields.symbolReference);
                    break;
                }
                case IMPLICIT_STORE: {
                    ImplicitStoreInstructionFields impStoreFields = instrUnion.implicitStoreInstructionFields;
                    instructionObject = _con->ImplicitStoreInstruction(impStoreFields.type, impStoreFields.omrGlobalIndex);
                    break;
                }
                case LOAD: {
                    LoadInstructionFields loadFields = instrUnion.loadInstructionFields;
                    instructionObject = _con->LoadInstruction(loadFields.type,loadFields.symbolReference);
                    break;
                }
                case IMPLICIT_LOAD: {
                    ImplicitLoadInstructionFields impLoadFields = instrUnion.implicitLoadInstructionFields;
                    instructionObject = _con->ImplicitLoadInstruction(impLoadFields.type, impLoadFields.omrGloablIndex);
                    break;
                }
                case BINARY_OP: {
                    BinaryOpInstructionFields binaryFields = instrUnion.binaryOpInstructionFields;
                    instructionObject = _con->BinaryOpInstruction(binaryFields.type,binaryFields.op);
                    break;
                }
                case UNARY_OP: {
                    UnaryOpInstructionFields unaryFields = instrUnion.unaryOpInstructionFields;
                    instructionObject = _con->UnaryOpInstruction(unaryFields.type);
                    break;
                }
                case RETURN: {
                    ReturnInstructionFields returnFields = instrUnion.returnInstructionFields;
                    instructionObject = _con->ReturnInstruction(returnFields.type);
                    break;
                }
                case GOTO: {
                    GotoInstructionFields gotoFields = instrUnion.gotoInstructionFields;
                    instructionObject = _con->GotoInstruction(gotoFields.label);
                    break;
                }
                case CONDITIONAL_BRANCH: {
                    ConditionalBranchInstructionFields condiFields = instrUnion.conditionalBranchInstructionFields;
                    instructionObject = _con->ConditionalBranchInstruction(condiFields.type,condiFields.op,condiFields.label);
                    break;
                }
                case COMPARISON: {
                    ComparisonInstructionFields compaFields = instrUnion.comparisonInstructionFields;
                    instructionObject = _con->ComparisonInstruction(compaFields.type,compaFields.op);
                    break;
                }
                case CONVERSION: {
                    ConversionInstructionFields conversionFields = instrUnion.conversionInstructionFields;
                    instructionObject = _con->ConversionInstruction(conversionFields.fromType,conversionFields.toType);
                    break;
                }
                case INVOKE: {
                    InvokeInstructionFields invokeFields = instrUnion.invokeInstructionFields;
                    instructionObject = _con->InvokeInstruction(invokeFields.type,invokeFields.className,invokeFields.methodName,invokeFields.disp);
                    break;
                }
                case SWAP:{
                    SwapInstructionFields swapFields = instrUnion.swapInstructionFields;
                    instructionObject = _con->SwapInstruction();
                    break;
                }
                case POP: {
                    PopInstructionFields popFields = instrUnion.popInstructionFields;
                    instructionObject = _con->PopInstruction(popFields.size);
                    break;
                }
                case ARRAY_STORE: {
                    ArrayStoreInstructionFields arrayStoreFields = instrUnion.arrayStoreInstructionFields;
                    instructionObject = _con->ArrayStoreInstruction(arrayStoreFields.type);
                    break;
                }
                case ARRAY_LOAD: {
                    ArrayLoadInstructionFields arrayLoadFields = instrUnion.arrayLoadInstructionFields;
                    instructionObject = _con->ArrayLoadInstruction(arrayLoadFields.type);
                    break;
                }
                default:
                    perror("No instruction matched inside construct instruction object function!\n");
                    exit(1);
                    
            }

            MappedInstructionObject mappedInstructionObject = {instruction, instructionObject, offset};
            mappedInstructionObjectList.push_back(mappedInstructionObject);
        }
        
    }

    /*** log all mapped instruction to STDOUT ***/
    _logger->logAllMappedInstructions(mappedInstructionObjectList);

}


/****** instruction router, starting point ********/
void TR_OWLMapper::_instructionRouter(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();

    /*** deal with the nodes that are tree top or BBStart, BBEnd ***/
    if ( opCodeValue == TR::treetop || opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd){
        ShrikeBTInstructionFieldsUnion instrUnion;
        // need to take a place in instruction meta list
        _createInstructionMeta(false, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, NOT_INSTRUCTION );
        return;
    }

    printf("%d: %s | ref count: %u\n", node->getGlobalIndex(),node->getOpCode().getName(),node->getReferenceCount());

    TR::ILOpCode opCode = node->getOpCode();

    if (opCode.isLoadConst()) { //constant instruction
         _mapConstantInstruction(node);
    }
    else if (opCode.isStoreDirect()) { // store instruction
        _mapDirectStoreInstruction(node);
    }
    else if (opCode.isStoreIndirect()) { //storei
        _mapIndirectStoreInstruction(node);
    }
    else if (opCode.isLoadDirect()) { // load instruction
        _mapDirectLoadInstruction(node);
    }
    else if (opCode.isLoadIndirect()) { //loadi
        _mapIndirectLoadInstruction(node);
    }
    else if (opCode.isReturn()) { // return instruction
        _mapReturnInstruction(node);
    }
    else if (opCode.isArrayRef()) {
        printf("&&& %s\n", opCode.getName());
    }
    else if (opCode.isArithmetic()){ // arithmetic instructions
        _mapArithmeticInstruction(node);
    }
    else if (opCode.isGoto()){ // goto instruction
        _mapGotoInstruction(node);
    }
    else if (opCode.isBooleanCompare() && opCode.isBranch()){ // conditional branch instruction
        _mapConditionalBranchInstruction(node);
    }
    else if (opCode.isConversion()) { // conversion Instruction
        _mapConversionInstruction(node);
    }
    else if (opCode.isBooleanCompare()){ // comparison instruction
        _mapComparisonInstruction(node);
    }
    else if (opCode.isCall()) { // function call
        _mapCallInstruction(node);
    }
    else if (opCode.isTernary()) { // ternary instruction
        _mapTernaryInstruction(node);
    }
    else if (opCode.isLoadAddr()) {} //loadaddr

    if (node->getReferenceCount() > 1) {
        _createImplicitStoreAndLoad(node);
    }

}

char* TR_OWLMapper::_getType(TR::ILOpCode opCode) {

    if (opCode.isInt()) return TYPE_int;
    else if (opCode.isShort()) return TYPE_int; // no short type for Java Bytecode
    else if (opCode.isLong()) return TYPE_long;
    else if (opCode.isFloat()) return TYPE_float;
    else if (opCode.isDouble()) return TYPE_double;
    else if (opCode.isByte()) return TYPE_int; // no byte for Java Bytecode
    else if (opCode.isIntegerOrAddress()) return TYPE_Object;

    perror("No type matched!\n");
    exit(1);
}

void TR_OWLMapper::_createImplicitStoreAndLoad(TR::Node *node) {

    char* type = _getType(node->getOpCode());
    uint32_t omrGlobalIndex = node->getGlobalIndex();
    ImplicitStoreInstructionFields impStoreFields = {type,omrGlobalIndex};
    ImplicitLoadInstructionFields impLoadFields = {type,omrGlobalIndex};


    ShrikeBTInstructionFieldsUnion insUnion;
    insUnion.implicitStoreInstructionFields = impStoreFields;

    _createInstructionMeta(true, omrGlobalIndex,0, NO_ADJUST, 0, insUnion, IMPLICIT_STORE);

    insUnion.implicitLoadInstructionFields = impLoadFields;

    _createInstructionMeta(true, omrGlobalIndex,0, NO_ADJUST, 0, insUnion, IMPLICIT_LOAD);
    
}

void TR_OWLMapper::_createImplicitLoad(TR::Node* node) {
    char* type = _getType(node->getOpCode());
    uint32_t omrGlobalIndex = node->getGlobalIndex();
    ImplicitLoadInstructionFields impLoadFields = {type, omrGlobalIndex};

    ShrikeBTInstructionFieldsUnion insUnion;
    insUnion.implicitLoadInstructionFields = impLoadFields;
    
    _createInstructionMeta(true, omrGlobalIndex, 0, NO_ADJUST, 0, insUnion, IMPLICIT_LOAD);

}

void TR_OWLMapper::_createInstructionMeta(bool isShrikeBTInstruction, uint32_t omrGlobalIndex, uint32_t shrikeBTOffset, 
    BranchTargetLabelAdjustType branchTargetLabelAdjustType,int32_t branchTargetLabelAdjustAmount, 
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion, ShrikeBTInstruction instruction ) 
{

    InstructionMeta insMeta = {
        isShrikeBTInstruction,
        omrGlobalIndex,
        shrikeBTOffset,
        branchTargetLabelAdjustType,
        branchTargetLabelAdjustAmount,
        instructionFieldsUnion,
        instruction
    };

    _instructionMetaList.push_back(insMeta);
}

void TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {

    ConstantInstructionFields constFields;

    TR::ILOpCode opCode = node->getOpCode();
    if (opCode.isInt()){
        constFields = {TYPE_int, _con->Integer(node->getInt())};
    }
    else if (opCode.isFloat()){
        constFields = {TYPE_float, _con->Float(node->getFloat())};
    }
    else if (opCode.isDouble()){
        constFields = {TYPE_double, _con->Double(node->getDouble())};
    }
    else if (opCode.isShort()){
        constFields = {TYPE_int, _con->Integer(node->getShortInt())};
    }
    else if (opCode.isLong()){
        constFields = {TYPE_long, _con->Long(node->getLongInt())};
    }
    else if (opCode.isByte()){
        constFields = {TYPE_int, _con->Integer(node->getByte())};
    }
    else{
        perror("No Constant type matched!\n");
        exit(1);
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.constantInstructionFields = constFields;

    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, CONSTANT);
   
}

void TR_OWLMapper::_mapDirectStoreInstruction(TR::Node *node) {
  
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    StoreInstructionFields storeFields = {type,node->getSymbolReference()};
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.storeInstructionFields = storeFields;

    _createInstructionMeta(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion, STORE);

}

void TR_OWLMapper::_mapDirectLoadInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    LoadInstructionFields loadFields = {type, node->getSymbolReference()};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.loadInstructionFields = loadFields;

    _createInstructionMeta(true, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion, LOAD);

}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    
    ReturnInstructionFields returnFields = {type};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.returnInstructionFields = returnFields;

    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, RETURN);
   
}

void TR_OWLMapper::_mapArithmeticInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    ShrikeBTOperator op;
    
    InstructionMeta instrMeta;

    /**** Unary op ****/
    if (opCode.isNeg()){

        UnaryOpInstructionFields unaryFields = {type};
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.unaryOpInstructionFields = unaryFields;
        _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, UNARY_OP);

    }
    /**** Binary op ****/
    else{
        if (opCode.isAdd()) op = ADD;
        else if (opCode.isSub()) op = SUB;
        else if (opCode.isMul()) op = MUL;
        else if (opCode.isDiv()) op = DIV;
        else if (opCode.isRem()) op = REM;
        else if (opCode.isAnd()) op = AND;
        else if (opCode.isOr()) op = OR;
        else if (opCode.isXor()) op = XOR;
        else{
            perror("No arithmetic operator matched!\n");
            exit(1);
        }
        BinaryOpInstructionFields binaryFields = {type,op};
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.binaryOpInstructionFields = binaryFields;

        _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, BINARY_OP);
    
    }
}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    GotoInstructionFields gotoFields = {index};
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.gotoInstructionFields = gotoFields;

    _createInstructionMeta(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, GOTO);

}

void TR_OWLMapper::_mapConditionalBranchInstruction(TR::Node *node) {

    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
    ShrikeBTOperator op;

    if (TR::ILOpCode::isStrictlyLessThanCmp(opCodeValue)) op = LT;
    else if (TR::ILOpCode::isStrictlyGreaterThanCmp(opCodeValue)) op = GT;
    else if (TR::ILOpCode::isLessCmp(opCodeValue)) op = LE;
    else if (TR::ILOpCode::isGreaterCmp(opCodeValue)) op = GE;
    else if (TR::ILOpCode::isEqualCmp(opCodeValue)) op = EQ;
    else op = NE;

    TR::ILOpCode childOpCode = node->getFirstChild()->getOpCode();
    char* type = _getType(childOpCode);
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    ConditionalBranchInstructionFields condiFields = {type,op,index};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.conditionalBranchInstructionFields = condiFields;

    _createInstructionMeta(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, CONDITIONAL_BRANCH);

}

/***
 * Since WALA comparison instruction does not have a full set of operators like OMR
 * Comparison instruction will be mapped as conditional branch instruction 
 */

void TR_OWLMapper::_mapComparisonInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
    char* type = _getType(opCode);

    TR_ComparisonTypes cmpType = TR::ILOpCode::getCompareType(opCodeValue);
    ShrikeBTOperator op;

    if (!opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfGreater() ){ //GT
        op = GT;
    }
    else if (!opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfLess()){ //LT
        op = LT;
    }
    else if (!opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfGreater()) { //GE
        op = GE;
    }
    else if (!opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfLess() ) { // LE
        op = LE;
    }
    else if (opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual()) { // EQ
        op = EQ;
    }
    else if (opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual()){ //NE
        op = NE;
    }
    else{
        perror("No cmp type matched!\n");
        exit(1);
    }

    ConditionalBranchInstructionFields condiFields = {type,op,0};
    ConstantInstructionFields const0Fields = {TYPE_int, _con->Integer(0)};
    GotoInstructionFields gotoFields = {0};
    ConstantInstructionFields const1Fields = {TYPE_int, _con->Integer(1)};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, BY_VALUE, 3, instrUnion, CONDITIONAL_BRANCH);

    instrUnion.constantInstructionFields = const0Fields;
    _createInstructionMeta(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONSTANT);
  
    instrUnion.gotoInstructionFields = gotoFields;
    _createInstructionMeta(true,node->getGlobalIndex(),0,BY_VALUE,2,instrUnion,GOTO);
   
    instrUnion.constantInstructionFields = const1Fields;
    _createInstructionMeta(true,node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONSTANT);
  
}

void TR_OWLMapper::_mapConversionInstruction(TR::Node *node) {

    char* fromType = NULL;
    char* toType = NULL;

    TR::ILOpCodes opCodeValue = node->getOpCode().getOpCodeValue();
    switch(opCodeValue) {
        case TR::i2l:
        case TR::iu2l:
        case TR::b2l:
        case TR::bu2l:
        case TR::s2l:
        case TR::su2l:
            fromType = TYPE_int;
            toType = TYPE_long;
            break;
        case TR::i2f:
        case TR::iu2f:
        case TR::b2f:
        case TR::bu2f:
        case TR::s2f:
        case TR::su2f:
            fromType = TYPE_int;
            toType = TYPE_float;
            break;
        case TR::i2d:
        case TR::iu2d:
        case TR::b2d:
        case TR::bu2d:
        case TR::s2d:
        case TR::su2d:
            fromType = TYPE_int;
            toType = TYPE_double;
            break;
        case TR::l2i:
        case TR::l2b:
        case TR::l2s:
            fromType = TYPE_long;
            toType = TYPE_int;
            break;
        case TR::l2f:
        case TR::lu2f:
            fromType = TYPE_long;
            toType = TYPE_float;
            break;
        case TR::l2d:
        case TR::lu2d:
            fromType = TYPE_long;
            toType = TYPE_double;
            break;
        case TR::f2i:
        case TR::f2b:
        case TR::f2s:
            fromType = TYPE_float;
            toType = TYPE_int;
            break;
        case TR::f2l:
            fromType = TYPE_float;
            toType = TYPE_long;
            break;
        case TR::f2d:
            fromType = TYPE_float;
            toType = TYPE_double;
            break;
        case TR::d2i:
        case TR::d2s:
        case TR::d2b:
            fromType = TYPE_double;
            toType = TYPE_int;
            break;
        case TR::d2l:
            fromType = TYPE_double;
            toType = TYPE_long;
            break;
        case TR::d2f:
            fromType = TYPE_double;
            toType = TYPE_float;
            break;
        case TR::b2i:
        case TR::bu2i:
        case TR::s2i:
        case TR::su2i:
        case TR::i2b:
        case TR::i2s:
        case TR::s2b:
        case TR::b2s:
            // no conversion needed. Since all byte and short have already been converted into int by the OWLMapper
            break;
        default:
            perror("No type conversion matched!\n");
            exit(1);
    }

    if (fromType && toType){
        ConversionInstructionFields conversionFields = {fromType,toType};
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.conversionInstructionFields = conversionFields;
        _createInstructionMeta(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONVERSION);
        
    }
    else{ //for those unecessary conversion instructions. They still need to occupy a slot in the instruciton list for the offset adjustment
        ShrikeBTInstructionFieldsUnion instrUnion;
        _createInstructionMeta(false, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion, NOT_INSTRUCTION);
    }

}

void TR_OWLMapper::_mapCallInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    TR::Method * method = node->getSymbolReference()->getSymbol()->getMethodSymbol()->getMethod();
    char * className = method->classNameChars();
    char * methodName = method->nameChars();

    InvokeInstructionFields invokeFields = {type, className, methodName, STATIC};
    
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, INVOKE);

}

/**
 * A ternary expression will be expanded into 
 * const 0
 * conditional branch (Not equal)
 * swap, 
 * pop, 
 * goto, 
 * pop
 * */
void TR_OWLMapper::_mapTernaryInstruction(TR::Node *node) {
    ConstantInstructionFields const0Fields = {TYPE_int, _con->Integer(0)};
    ConditionalBranchInstructionFields condiFields = {TYPE_int, NE, 0};
    SwapInstructionFields swapFields = {};
    PopInstructionFields popFields = {1};
    GotoInstructionFields gotoFields = {0};
    // pop fields, use previous one

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.constantInstructionFields = const0Fields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST,0,instrUnion, CONSTANT);

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, BY_VALUE, 4, instrUnion, CONDITIONAL_BRANCH);

    instrUnion.swapInstructionFields = swapFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SWAP);

    instrUnion.popInstructionFields = popFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, POP);

    instrUnion.gotoInstructionFields = gotoFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, BY_VALUE, 2, instrUnion, GOTO );

    instrUnion.popInstructionFields = popFields;
    _createInstructionMeta(true, node->getGlobalIndex(), 0, NO_ADJUST,0, instrUnion, POP);

}

void TR_OWLMapper::_mapIndirectStoreInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode());
    ArrayStoreInstructionFields arrayStoreFields = {type};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayStoreInstructionFields = arrayStoreFields;

    _createInstructionMeta(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,ARRAY_STORE);

}

void TR_OWLMapper::_mapIndirectLoadInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode());
    ArrayLoadInstructionFields arrayLoadFields = {type};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayLoadInstructionFields = arrayLoadFields;

    _createInstructionMeta(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,ARRAY_LOAD);

}