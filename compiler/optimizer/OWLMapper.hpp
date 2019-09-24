//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include "il/OMRNode_inlines.hpp"
#include "compile/Compilation.hpp"
#include "OWLJNIClient.hpp"

class TR_OWLMapper
{
private:
    TR_OWLJNIClient *_jniClient;
    int32_t _index;
    void _processTree(TR::Node *root,TR::NodeChecklist &visited );
    void _mapInstruction(TR::Node *node);
    char* _getInstructionString(jobject instructionObj, MethodConfig toStringMethodConfig);
    jobject _ConstantInstruction(char* type, int32_t value);
    jobject _StoreInstruction(char* type, int32_t index);
    jobject _LoadInstruction(char* type, int32_t index);
    jobject _BinaryOpInstruction(char* type, jobject op);
    jobject _ReturnInstruction(char* type);

public:
    TR_OWLMapper();
    ~TR_OWLMapper();
    void perform(TR::Compilation *compilation);

};
#endif //OMR_OWLWALAMAPPER_HPP
