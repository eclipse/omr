//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include <unordered_map>
#include <vector>
#include "il/OMRNode_inlines.hpp"
#include "compile/Compilation.hpp"
#include "optimizer/OWLInstructionConstructor.hpp"

/*** indicates how the offset of branch instruction should be adjusted ***/
enum BranchTargetLabelAdjustType {
    NO_ADJUST, // no need to adjust the branch target
    TABLE_MAP, // get the target label from the omr index to wala offset mapping table
    ADD_2,// label target = current offset added by 2
    ADD_3,
};

struct InstructionInfo {
    /*** if false, this indicates it can be an OMR treetop, BBStart, BBEnd or an eliminated OMR instruction ***/
    bool is_WALA_instruction;
    uint32_t OMR_GlobalIndex;
    uint32_t WALA_offset; // set to 0 initially and let the adjust function assign the correct offset
    BranchTargetLabelAdjustType branchTargetLabelAdjustType;
    WALA_InstructionFieldsUnion instructionFieldsUnion;
    WALA_Instruction instruction;
};


class TR_OWLMapper
{
private:

    TR_OWLInstructionConstructor* _con;

    std::vector<InstructionInfo> _instructionInfoList; 

    void _processTree(TR::Node *root,TR::NodeChecklist &visited ); // traverse the tree
    void _adjustOffset();
    void _logAllMappedInstructions(); 

    void _logSingleInstruction(jobject instructionObj, WALA_Instruction instruction, uint32_t offset);
    
    void _instructionRouter(TR::Node *node);

    char* _getType(TR::ILOpCode opCode);

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

public:
    TR_OWLMapper();
    ~TR_OWLMapper();
    void map(TR::Compilation *compilation);

};
#endif //OMR_OWLWALAMAPPER_HPP
