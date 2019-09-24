//
// Created by Cijie Xia on 2019-09-23.
//

#include "OWLMapper.hpp"
#include "OWLMethodConfig.hpp"

#include <iostream>

TR_OWLMapper::TR_OWLMapper() {
    TR_OWLJNIClient *_jniClient = TR_OWLJNIClient::getInstance();
    _index = -1;
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _jniClient;
}

void TR_OWLMapper::_processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;

    visited.add(root);

    for (int32_t i = 0; i < root->getNumChildren(); ++i) {
        _processTree(root->getChild(i), visited);
    }

    _mapInstruction(root);
}

void TR_OWLMapper::_mapInstruction(TR::Node *node) {
    std::cout << node->getOpCode().getName() << std::endl;

    char* arrow = ">>>>WALA: ";

    TR::ILOpCode opCode = node ->getOpCode();
    if (opCode.isInt()) { //Int instruction series
        if (opCode.isLoadConst()) { //iconst
            char* instruction = _getInstructionString(_ConstantInstruction(TYPE_int,node->getInt()), ConstantInstructionToStringConfig);
            std::cout << arrow << instruction << std::endl;
        }
        else if (opCode.isStore()) { //istore
            char* instruction = _getInstructionString(_StoreInstruction(TYPE_int, ++_index), StoreInstructionToStringConfig);
            std::cout << arrow << instruction << std::endl;
        }
        else if (opCode.isLoadVar()) { // iload
            char* instruction = _getInstructionString(_LoadInstruction(TYPE_int, _index--), LoadInstructionToStringConfig);
            std::cout << arrow << instruction << std::endl;
        }
        else if (opCode.isAdd()) { //iadd
            jobject op;
            _jniClient->getField(AddOperatorConfig,NULL, &op);
            char *instruction = _getInstructionString(_BinaryOpInstruction(TYPE_int,op), BinaryOpInstructionToStringConfig);
            std::cout << arrow << instruction << std::endl;
        }
        else if (opCode.isReturn()) { //ireturn
            char* instruction = _getInstructionString(_ReturnInstruction(TYPE_int), ReturnInstructionToStringConfig);
            std::cout << arrow << instruction << std::endl;
        }
    }

}

char* TR_OWLMapper::_getInstructionString(jobject instructionObj, MethodConfig toStringMethodConfig) {

    char* instructionString;

    _jniClient->callMethod
    (
            toStringMethodConfig,
            instructionObj,
            &instructionString,
            0
    );

    return instructionString;
}

jobject TR_OWLMapper::_ConstantInstruction(char* type, int32_t value) {

    jobject constantInstructionObject;

    _jniClient->callStaticMethod
    (
            ConstantInstructionConfig,
            &constantInstructionObject,
            2,
            _jniClient->constructString(type),
            _jniClient->constructIntObject(value)
    );

    return constantInstructionObject;
}

jobject TR_OWLMapper::_StoreInstruction(char* type, int32_t index) {

    jobject storeInstructionObject;

    _jniClient->callStaticMethod
    (
            StoreInstructionConfig,
            &storeInstructionObject,
            2,
            _jniClient->constructString(type),
            index
    );

    return storeInstructionObject;
}

jobject TR_OWLMapper::_LoadInstruction(char* type, int32_t index) {

    jobject loadInstructionObject;

    _jniClient->callStaticMethod
    (
            LoadInstructionConfig,
            &loadInstructionObject,
            2,
            _jniClient->constructString(type),
            index
    );

    return loadInstructionObject;
}

jobject TR_OWLMapper::_BinaryOpInstruction(char* type, jobject op) {
    jobject binaryOpInstruction;

    _jniClient->callStaticMethod
    (
        BinaryOpInstructionConfig,
        &binaryOpInstruction,
        2,
        _jniClient->constructString(type),
        op
    );
    return binaryOpInstruction;
}

jobject TR_OWLMapper::_ReturnInstruction(char *type) {
    jobject returnInstruction;

    _jniClient->callStaticMethod
    (
            ReturnInstructionConfig,
            &returnInstruction,
            1,
            _jniClient->constructString(type)
    );

    return returnInstruction;

}

void TR_OWLMapper::perform(TR::Compilation* compilation) {

    TR::NodeChecklist visited(compilation); // visited nodes

    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _processTree(node, visited);
    }
}