//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include <unordered_map>
#include <vector>
#include "il/OMRNode_inlines.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/OWLMapperTypes.hpp"
#include "optimizer/OWLLogger.hpp"
#include "optimizer/OWLShrikeBTConstructor.hpp"


class TR_OWLMapper
{
private:

    TR_OWLShrikeBTConstructor* _con;
    TR_OWLLogger* _logger;

    std::vector<InstructionMeta> _instructionMetaList; 

    void _processTree(TR::Node *root,TR::NodeChecklist &visited ); // traverse the tree
    void _adjustOffset();

    void _instructionRouter(TR::Node *node);

    char* _getType(TR::ILOpCode opCode);

    /*** create instruction meta and push it into instruction meta list ***/
    void _createInstructionMeta(
        bool isShrikeBTInstruction, 
        uint32_t omrGlobalIndex, 
        uint32_t shrikeBTOffset, 
        BranchTargetLabelAdjustType branchTargetLabelAdjustType, 
        int32_t branchTargetLabelAdjustAmount,
        ShrikeBTInstructionFieldsUnion instructionFieldsUnion, 
        ShrikeBTInstruction instruction );
    
    void _createImplicitStoreAndLoad(TR::Node *node);
    void _createImplicitLoad(TR::Node *node);

    void _mapConstantInstruction(TR::Node *node);
    void _mapStoreInstruction(TR::Node* node);
    void _mapLoadInstruction(TR::Node* node);
    void _mapReturnInstruction(TR::Node* node);
    void _mapArithmeticInstruction(TR::Node* node);
    void _mapGotoInstruction(TR::Node* node);
    void _mapConditionalBranchInstruction(TR::Node* node);
    void _mapComparisonInstruction(TR::Node* node);
    void _mapConversionInstruction(TR::Node* node);
    void _mapInvokeInstruction(TR::Node* node);
    void _mapTernaryInstruction(TR::Node* node);

    void _constructShrikeBTInstructionObjects();

public:
    TR_OWLMapper();
    ~TR_OWLMapper();
    void map(TR::Compilation *compilation);

};
#endif 
