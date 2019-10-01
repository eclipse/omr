//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include "il/OMRNode_inlines.hpp"
#include "compile/Compilation.hpp"
#include "OWLInstructionConstructor.hpp"


class TR_OWLMapper
{
private:
    TR_OWLInstructionConstructor* _con;
    void _processTree(TR::Node *root,TR::NodeChecklist &visited ); // traverse the tree
    void _logInstruction(char* instruction);
    void _instructionRouter(TR::Node *node);
    char* _getType(TR::ILOpCode opCode);
    jobject _mapConstantInstruction(TR::Node *node);
    jobject _mapStoreInstruction(TR::Node* node);
    jobject _mapLoadInstruction(TR::Node* node);
    jobject _mapReturnInstruction(TR::Node* node);
    jobject _mapBinaryOpInstruction(TR::Node* node, Op op);
    jobject _mapGotoInstruction(TR::Node* node);
    jobject _mapConditionalBranchInstruction(TR::Node* node);
    jobject _mapComparisonInstruction(TR::Node* node);

public:
    TR_OWLMapper();
    ~TR_OWLMapper();
    void map(TR::Compilation *compilation);

};
#endif //OMR_OWLWALAMAPPER_HPP
