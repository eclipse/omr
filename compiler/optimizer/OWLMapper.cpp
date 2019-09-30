//
// Created by Cijie Xia on 2019-09-23.
//

#include "OWLMapper.hpp"
#include "OWLJNIConfig.hpp"
#include "string.h"
#include <stdlib.h>

TR_OWLMapper::TR_OWLMapper() {
    _con = new TR_OWLInstructionConstructor();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _con;
}

void TR_OWLMapper::_processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;

    visited.add(root);

    for (int32_t i = 0; i < root->getNumChildren(); ++i) {
        _processTree(root->getChild(i), visited);
    }

    _instructionRouter(root);
}

void TR_OWLMapper::_logInstruction(char *instruction) {
    printf(" >>>> %s",instruction);
}

/****** instruction router, starting point ********/
void TR_OWLMapper::_instructionRouter(TR::Node *node) {

    if (node->getOpCodeValue() == TR::BBStart){
        printf("\n");
    }

    printf("%d: %s",node->getGlobalIndex(),node->getOpCode().getName());

    TR::ILOpCode opCode = node->getOpCode();

    jobject instructionObj;

    if (opCode.isLoadConst()) { //constant instruction
        instructionObj = _mapConstantInstruction(node);
        char *str = _con->getInstructionString(instructionObj, CONST);
        _logInstruction(str);
        free(str);

    }
    else if (opCode.isStore()) { // store instruction
        instructionObj = _mapStoreInstruction(node);
        char *str = _con->getInstructionString(instructionObj, STORE);
        _logInstruction(str);
        free(str);

    }
    else if (opCode.isLoad()) { // load instruction
        instructionObj = _mapLoadInstruction(node);
        char* str = _con->getInstructionString(instructionObj, LOAD);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isReturn()) { // return instruction
        instructionObj = _mapReturnInstruction(node);
        char *str = _con->getInstructionString(instructionObj, RETURN);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isAdd()) { // add instruction
        instructionObj = _mapBinaryOpInstruction(node,ADD);
        char *str = _con->getInstructionString(instructionObj, BINARY_OP);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isSub()) { // sub instruction
        instructionObj = _mapBinaryOpInstruction(node, SUB);
        char *str = _con->getInstructionString(instructionObj, BINARY_OP);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isMul()) { // multiply instruction
        instructionObj = _mapBinaryOpInstruction(node, MUL);
        char *str = _con->getInstructionString(instructionObj, BINARY_OP);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isDiv()) { // divide instruction
        instructionObj = _mapBinaryOpInstruction(node, DIV);
        char *str = _con->getInstructionString(instructionObj, BINARY_OP);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isRem()) { //remainder instruction
        instructionObj = _mapBinaryOpInstruction(node, REM);
        char *str = _con->getInstructionString(instructionObj, BINARY_OP);
        _logInstruction(str);
        free(str);
    }
    else if (opCode.isGoto()){ // goto instruction
        instructionObj = _mapGotoInstruction(node);
        char*str = _con->getInstructionString(instructionObj,GOTO);
        _logInstruction(str);
        free(str);
    }

    printf("\n");

}

char* TR_OWLMapper::_getType(TR::ILOpCode opCode) {
    if (opCode.isInt()){
        return TYPE_int;
    }
    else if (opCode.isShort()){
        return TYPE_short;
    }
    else if (opCode.isLong()){
        return TYPE_long;
    }
    else if (opCode.isFloat()){
        return TYPE_float;
    }
    else if (opCode.isDouble()){
        return TYPE_double;
    }

    perror("No type matched!\n");
    exit(1);
}

jobject TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {
    jobject constantInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    if (opCode.isInt()){
        constantInstruction = _con->ConstantInstruction(TYPE_int, _con->Integer(node->getInt()));
    }
    else if (opCode.isFloat()){
        constantInstruction = _con->ConstantInstruction(TYPE_float, _con->Integer(node->getFloat()));
    }
    else if (opCode.isDouble()){
        constantInstruction = _con->ConstantInstruction(TYPE_double, _con->Double(node->getDouble()));
    }
    else if (opCode.isShort()){
        constantInstruction = _con->ConstantInstruction(TYPE_short, _con->Short(node->getShortInt()));
    }
    else if (opCode.isLong()){
        constantInstruction = _con->ConstantInstruction(TYPE_long, _con->Long(node->getLongInt()));
    }
    return constantInstruction;

}

jobject TR_OWLMapper::_mapStoreInstruction(TR::Node *node) {
    jobject storeInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    storeInstruction = _con->StoreInstruction(type);
    return storeInstruction;
}

jobject TR_OWLMapper::_mapLoadInstruction(TR::Node *node) {
    jobject loadInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    loadInstruction = _con->LoadInstruction(type);
    return loadInstruction;
}

jobject TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    jobject returnInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    returnInstruction = _con->ReturnInstruction(type);
    return returnInstruction;
}

jobject TR_OWLMapper::_mapBinaryOpInstruction(TR::Node *node, Op op) {
    jobject binaryOpInstruction;
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    binaryOpInstruction = _con->BinaryOpInstruction(type, _con->Operator(op));
    return binaryOpInstruction;
}

jobject TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    jobject gotoInstruction;
    gotoInstruction = _con->GotoInstruction(node->getBranchDestination()->getNode()->getGlobalIndex());
    return gotoInstruction;
}



void TR_OWLMapper::map(TR::Compilation* compilation) {

    TR::NodeChecklist visited(compilation); // visited nodes

    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();

        _processTree(node, visited);

    }
}