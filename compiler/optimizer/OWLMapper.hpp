//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include <unordered_map>
#include "il/OMRNode_inlines.hpp"
#include "compile/Compilation.hpp"
#include "OWLInstructionConstructor.hpp"


class TR_OWLMapper
{
private:
    uint32_t _offset; // only used for building index-offset map table
    int32_t _offsetAdjust;
    TR_OWLInstructionConstructor* _con;

    std::unordered_map<uint32_t, uint32_t> _OMR_IndexToWALA_OffsetMap;

    void _buildOMR_IndexToWALA_MappingTable(TR::Node *root,TR::NodeChecklist &visited); // build the index to offset table
    void _processTree(TR::Node *root,TR::NodeChecklist &visited ); // traverse the tree
    void _logInstruction(const char* instruction, uint32_t offset);

    uint32_t _getWALAOffset(TR::Node *node, int16_t adjustAmount );

    void _instructionRouter(TR::Node *node);

    char* _getType(TR::ILOpCode opCode);

    void _mapConstantInstruction(TR::Node *node);
    void _mapStoreInstruction(TR::Node* node);
    void _mapLoadInstruction(TR::Node* node);
    void _mapReturnInstruction(TR::Node* node);
    void _mapBinaryOpInstruction(TR::Node* node);
    void _mapUnaryOpInstruction(TR::Node* node);
    void _mapGotoInstruction(TR::Node* node);
    void _mapConditionalBranchInstruction(TR::Node* node);
    void _mapComparisonInstruction(TR::Node* node);
    void _mapConversionInstruction(TR::Node* node);

public:
    TR_OWLMapper();
    ~TR_OWLMapper();
    void map(TR::Compilation *compilation);

};
#endif //OMR_OWLWALAMAPPER_HPP
