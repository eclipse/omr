//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include <unordered_map>
#include <vector>
#include "il/Node.hpp"
#include "compile/Compilation.hpp"
#include "il/Node_inlines.hpp"
#include "optimizer/OWLTypes.hpp"
#include "optimizer/OWLShrikeBTConstructor.hpp"


class TR_OWLMapper
{
private:

    std::vector<OWLInstruction> _owlInstructionList; 

    void _processTree(TR::Node *root, TR::Node *parent, TR::NodeChecklist &visited ); // traverse the tree
    void _logOMRIL(TR::Node *root, TR::NodeChecklist &visited); // log the OMR IL
    void _adjustOffset();

    void _instructionRouter(TR::Node *node);

    char* _getType(TR::ILOpCode opCode);

    /*** create OWL instruction and push it into OWL instruction list ***/
    void _createOWLInstruction(
        bool isShrikeBTInstruction, 
        uint32_t omrGlobalIndex, 
        uint32_t shrikeBTOffset, 
        BranchTargetLabelAdjustType branchTargetLabelAdjustType, 
        int32_t branchTargetLabelAdjustAmount,
        ShrikeBTInstructionFieldsUnion instructionFieldsUnion, 
        ShrikeBTInstruction instruction );
    
    void _createImplicitStore(TR::Node *node);
    void _createImplicitLoad(TR::Node *node);
    void _createDupInstruction(TR::Node *node, uint16_t delta);

    void _mapConstantInstruction(TR::Node *node);
    void _mapDirectStoreInstruction(TR::Node* node);
    void _mapDirectLoadInstruction(TR::Node* node);
    void _mapReturnInstruction(TR::Node* node);
    void _mapArithmeticInstruction(TR::Node* node);
    void _mapGotoInstruction(TR::Node* node);
    void _mapConditionalBranchInstruction(TR::Node* node);
    void _mapComparisonInstruction(TR::Node* node);
    void _mapConversionInstruction(TR::Node* node);
    void _mapCallInstruction(TR::Node* node);
    void _mapTernaryInstruction(TR::Node* node);
    void _mapIndirectStoreInstruction(TR::Node *node);
    void _mapIndirectLoadInstruction(TR::Node *node);
    void _mapNewInstruction(TR::Node *node);
    void _mapPutInstruction(TR::Node *node);

public:
    std::vector<OWLInstruction> map(TR::Compilation *compilation);
};
#endif 
