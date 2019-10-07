//
// Created by Cijie Xia on 2019-09-23.
//

#include <string.h>
#include <stdlib.h>
#include "il/ILHelpers.hpp"
#include "OWLMapper.hpp"
#include "OWLJNIConfig.hpp"


TR_OWLMapper::TR_OWLMapper() {
    _offset = 0; // only used for building index-offset map table
    _offsetAdjust = 0;
    _con = new TR_OWLInstructionConstructor();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _con;
}

void TR_OWLMapper::_buildOMR_IndexToWALA_MappingTable(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;

    visited.add(root);

    for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
        _buildOMR_IndexToWALA_MappingTable(root->getChild(i), visited);
    }

    _OMR_IndexToWALA_OffsetMap[root->getGlobalIndex()] = _offset;
    if (root->getOpCodeValue() != TR::BBStart && root->getOpCodeValue() != TR::BBEnd){
        _offset++;
    }

}

void TR_OWLMapper::_processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;

    visited.add(root);

    for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
        _processTree(root->getChild(i), visited);
    }

    _instructionRouter(root);
}

void TR_OWLMapper::_logInstruction(const char *instruction, uint32_t offset) {
    printf("%d: %s\n",offset, instruction);

}

uint32_t TR_OWLMapper::_getWALAOffset(TR::Node *node, int16_t adjustAmount) {
    uint32_t offset = _OMR_IndexToWALA_OffsetMap[node->getGlobalIndex()] + _offsetAdjust;
    _offsetAdjust += adjustAmount;
    return offset;
}

/****** instruction router, starting point ********/
void TR_OWLMapper::_instructionRouter(TR::Node *node) {

    if (node->getOpCodeValue() == TR::BBStart || node->getOpCodeValue() == TR::BBEnd){
        return;
    }

    //printf("%s ",node->getOpCode().getName());

    TR::ILOpCode opCode = node->getOpCode();

    if (opCode.isLoadConst()) { //constant instruction
        _mapConstantInstruction(node);
    }
    else if (opCode.isStore()) { // store instruction
        _mapStoreInstruction(node);
    }
    else if (opCode.isLoad()) { // load instruction
        _mapLoadInstruction(node);
    }
    else if (opCode.isReturn()) { // return instruction
        _mapReturnInstruction(node);
    }
    else if (opCode.isArithmetic()){ // arithmetic (binary op) instructions
        _mapBinaryOpInstruction(node);
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
    else if (opCode.isNeg()) { // unary (Neg) Instruction
        _mapUnaryOpInstruction(node);
    }
    else{ // for those have not been mapped
        printf("XXXXX");
        _logInstruction(node->getOpCode().getName(), _getWALAOffset(node,0));
    }



}

char* TR_OWLMapper::_getType(TR::ILOpCode opCode) {

    if (opCode.isInt()) return TYPE_int;
    else if (opCode.isShort()) return TYPE_int; // no short type for Java Bytecode
    else if (opCode.isLong()) return TYPE_long;
    else if (opCode.isFloat()) return TYPE_float;
    else if (opCode.isDouble()) return TYPE_double;
    else if (opCode.isByte()) return TYPE_int; // no byte for Java Bytecode

    perror("No type matched!\n");
    exit(1);
}

void TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {
    jobject constantInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    if (opCode.isInt()){
        constantInstruction = _con->ConstantInstruction(TYPE_int, _con->Integer(node->getInt()));
    }
    else if (opCode.isFloat()){
        constantInstruction = _con->ConstantInstruction(TYPE_float, _con->Float(node->getFloat()));
    }
    else if (opCode.isDouble()){
        constantInstruction = _con->ConstantInstruction(TYPE_double, _con->Double(node->getDouble()));
    }
    else if (opCode.isShort()){
        constantInstruction = _con->ConstantInstruction(TYPE_int, _con->Integer(node->getShortInt()));
    }
    else if (opCode.isLong()){
        constantInstruction = _con->ConstantInstruction(TYPE_long, _con->Long(node->getLongInt()));
    }
    else if (opCode.isByte()){
        constantInstruction = _con->ConstantInstruction(TYPE_int, _con->Integer(node->getByte()));
    }
    else{
        perror("No Constant type matched!\n");
        exit(0);
    }

    char * str = _con->getInstructionString(constantInstruction,CONSTANT);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);

}

void TR_OWLMapper::_mapStoreInstruction(TR::Node *node) {
    jobject storeInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    storeInstruction = _con->StoreInstruction(type,node->getSymbolReference());
    char * str = _con->getInstructionString(storeInstruction,STORE);
    _logInstruction(str, _getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapLoadInstruction(TR::Node *node) {
    jobject loadInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    loadInstruction = _con->LoadInstruction(type, node->getSymbolReference());
    char *str = _con->getInstructionString(loadInstruction,LOAD);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    jobject returnInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    returnInstruction = _con->ReturnInstruction(type);
    char* str = _con->getInstructionString(returnInstruction,RETURN);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapBinaryOpInstruction(TR::Node *node) {
    jobject binaryOpInstruction;

    TR::ILOpCode opCode = node->getOpCode();

    Op op;
    if (opCode.isAdd()) op = ADD;
    else if (opCode.isSub()) op = SUB;
    else if (opCode.isMul()) op = MUL;
    else if (opCode.isDiv()) op = DIV;
    else if (opCode.isRem()) op = REM;
    else {
        perror("No binary operator matched!\n");
        exit(1);
    }

    char* type = _getType(opCode);

    binaryOpInstruction = _con->BinaryOpInstruction(type, op);
    char* str = _con->getInstructionString(binaryOpInstruction,BINARY_OP);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapUnaryOpInstruction(TR::Node *node) {
    jobject unaryOpInstruction;

    TR::ILOpCode opCode = node->getOpCode();

    char *type = _getType(opCode);

    unaryOpInstruction = _con->UnaryOpInstruction(type);
    char *str = _con->getInstructionString(unaryOpInstruction,UNARY_OP);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    jobject gotoInstruction;
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();
    gotoInstruction = _con->GotoInstruction(_OMR_IndexToWALA_OffsetMap[index] + _offsetAdjust);
    char* str = _con->getInstructionString(gotoInstruction,GOTO);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapConditionalBranchInstruction(TR::Node *node) {
    jobject conditionalBranchInstruction;

    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
    Op op;

    if (TR::ILOpCode::isStrictlyLessThanCmp(opCodeValue)) op = LT;
    else if (TR::ILOpCode::isStrictlyGreaterThanCmp(opCodeValue)) op = GT;
    else if (TR::ILOpCode::isLessCmp(opCodeValue)) op = LE;
    else if (TR::ILOpCode::isGreaterCmp(opCodeValue)) op = GE;
    else if (TR::ILOpCode::isEqualCmp(opCodeValue)) op = EQ;
    else op = NE;

    TR::ILOpCode childOpCode = node->getFirstChild()->getOpCode();
    char* type = _getType(childOpCode);
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();
    conditionalBranchInstruction = _con->ConditionalBranchInstruction(type, op, _OMR_IndexToWALA_OffsetMap[index] + _offsetAdjust);

    char* str = _con->getInstructionString(conditionalBranchInstruction,CONDITIONAL_BRANCH);
    _logInstruction(str,_getWALAOffset(node,0));
    free(str);
}

/***
 * Since WALA comparison instruction does not have a set of operators like OMR
 * We use conditional branch and create two const int 1 and const int 2
 * goto constant 1 if the condition is true
 */

void TR_OWLMapper::_mapComparisonInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
    char* type = _getType(opCode);

    TR_ComparisonTypes cmpType = TR::ILOpCode::getCompareType(opCodeValue);
    Op op;


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

    uint32_t index = node->getGlobalIndex();
    char* str = NULL;
    jobject conditionalBranchInstruction = _con->ConditionalBranchInstruction(type,op,_OMR_IndexToWALA_OffsetMap[index] + _offsetAdjust + 3);
    jobject constant0Instruction = _con->ConstantInstruction(TYPE_int, _con->Integer(0));
    jobject constant1Instruction = _con->ConstantInstruction(TYPE_int, _con->Integer(1));
    jobject gotoInstruction = _con->GotoInstruction(_OMR_IndexToWALA_OffsetMap[index] + _offsetAdjust + 4);

    str = _con->getInstructionString(conditionalBranchInstruction,CONDITIONAL_BRANCH);

    _logInstruction(str,_getWALAOffset(node,1));
    free(str);

    str = _con->getInstructionString(constant0Instruction,CONSTANT);

    _logInstruction(str,_getWALAOffset(node,1));
    free(str);

    str = _con->getInstructionString(gotoInstruction,GOTO);

    _logInstruction(str, _getWALAOffset(node,1));
    free(str);

    str = _con->getInstructionString(constant1Instruction,CONSTANT);

    _logInstruction(str, _getWALAOffset(node,0));
    free(str);
}

void TR_OWLMapper::_mapConversionInstruction(TR::Node *node) {
    jobject conversionInstruction;

    char* fromType = NULL;
    char* toType = NULL;

    TR::ILOpCodes opCodeValue = node->getOpCode().getOpCodeValue();
    switch(opCodeValue) {
        case TR::i2l:
        case TR::b2l:
        case TR::s2l:
            fromType = TYPE_int;
            toType = TYPE_long;
            break;
        case TR::i2f:
        case TR::b2f:
        case TR::s2f:
            fromType = TYPE_int;
            toType = TYPE_float;
            break;
        case TR::i2d:
        case TR::b2d:
        case TR::s2d:
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
            fromType = TYPE_long;
            toType = TYPE_float;
            break;
        case TR::l2d:
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
        case TR::s2i:
        case TR::i2b:
        case TR::i2s:
        case TR::s2b:
        case TR::b2s:
            // no conversion needed. Since all byte and short have already been converted into int by the OWLMapper
            break;
        default:
            perror("No type conversion matched!");
            exit(1);
    }

    if (fromType && toType){
        conversionInstruction = _con->ConversionInstruction(fromType, toType);
        char *str = _con->getInstructionString(conversionInstruction,CONVERSION);
        _logInstruction(str,_getWALAOffset(node,0));
        free(str);
    }
//    else{
//        _getWALAOffset(node, -1);
//    }

}

/****** public ******/

void TR_OWLMapper::map(TR::Compilation* compilation) {

    TR::NodeChecklist OMR_IndexToWALA_OffsetMappingVisited(compilation); //visited nodes for building OMR global index to WALA offset table

    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();

        _buildOMR_IndexToWALA_MappingTable(node, OMR_IndexToWALA_OffsetMappingVisited);
    }

    TR::NodeChecklist instructionMappingVisited(compilation); // visited nodes for mapping the instruction

    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();

        _processTree(node, instructionMappingVisited);
    }
}