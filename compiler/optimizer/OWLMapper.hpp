//
// Created by Cijie Xia on 2019-09-23.
//

#ifndef OMR_OWLMAPPER_HPP
#define OMR_OWLMAPPER_HPP

#include <unordered_map>
#include <vector>
#include <stack>
#include "il/Node.hpp"
#include "compile/Compilation.hpp"
#include "il/Node_inlines.hpp"
#include "ras/Debug.hpp"
#include "optimizer/OWLTypes.hpp"
#include "optimizer/OWLShrikeBTConstructor.hpp"

/**
 * Simulating java byte code local variable table 
 */
class TR_OWLLocalVariableTable
{
private:
    uint32_t _index; 
    std::unordered_map<TR::SymbolReference*, uint32_t> _localVarTableBySymRef;
    std::unordered_map<TR::Node*, uint32_t> _localVarTableByNode;
    void _increaseIndex(char* type);
public:
    TR_OWLLocalVariableTable();
    uint32_t store(TR::SymbolReference* symRef, char* type);
    uint32_t implicitStore(TR::Node* node, char* type);
    uint32_t load(TR::SymbolReference* symRef);
    uint32_t implicitLoad(TR::Node* node);
    bool contain(TR::Node* node);
    bool contain(TR::SymbolReference* symRef);

};

// /**
//  * Simulating java byte code operand stack
//  */
class TR_OWLOperandStack
{
private:
    std::stack<TR::Node*> _stack;
    std::unordered_map<TR::Node*, uint32_t> _numOnStackTable; //keep track of the number of operands created by each node on stack <Node> : <# on stack>
public:
    void push(TR::Node* node); // push an operand to stack
    void pop(); // pop the top of the stack
    void dup(); // dup the top operand on stack
    void swap(); // swap the first two operands on stack
    TR::Node* top(); // peek the top operand on stack
    bool isEmpty(); // if the stack is empty
    uint32_t size(); // size of the stack
    uint32_t getOperandNum(TR::Node* node); // get the number of operands created by the given node on stack
    bool contain(TR::Node* node); // if the stack contains the operand created by a given node
};

/**
 * Track the reference count of each node
 */
// class TR_OWLReferenceCountTable
// {
// private:
//     std::unordered_map<TR::Node*, uint32_t> _table;
// public:
//     void add(TR::Node* node, uint32_t remainingReferenceCount); // add an entry to the table: <Node>:<Remaining Reference Count>
//     void refer(TR::Node* node); // decrease the corresponding of a given node by 1. This method should be called every time we visit the node
//     bool noMoreReference(TR::Node* node); // whether the node won't be refered any more in the future. 
// };

class TR_OWLMapper
{
private:

    TR_Debug* _debug;
    TR::Compilation* _compilation;
    TR_OWLLocalVariableTable* _localVarTable;
    TR_OWLOperandStack* _operandStack;
    // TR_OWLReferenceCountTable* _referenceCountTable;

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
    
    void _createImplicitStore(TR::Node* node);
    void _createImplicitLoad(TR::Node* node);
    void _createDupInstruction(TR::Node* node, uint16_t delta);
    void _createPopInstruction(TR::Node* node, uint16_t size);
    void _createSwapInstruction(TR::Node* node);

    void _mapConstantInstruction(TR::Node *node);
    void _mapDirectStoreInstruction(TR::Node* node);
    void _mapDirectLoadInstruction(TR::Node* node);
    void _mapReturnInstruction(TR::Node* node);
    void _mapArithmeticInstruction(TR::Node* node);
    void _mapGotoInstruction(TR::Node* node);
    void _mapConditionalBranchInstruction(TR::Node* node);
    void _mapComparisonInstruction(TR::Node* node);
    void _mapConversionInstruction(TR::Node* node);
    void _mapDirectCallInstruction(TR::Node* node);
    void _mapIndirectCallInstruction(TR::Node* node);
    void _mapTernaryInstruction(TR::Node* node);
    void _mapIndirectStoreInstruction(TR::Node *node);
    void _mapIndirectLoadInstruction(TR::Node *node);
    void _mapNewInstruction(TR::Node *node);
    void _mapInstanceofInstruction(TR::Node *node);
    void _mapArrayLengthInstruction(TR::Node *node);
    void _mapSwitchInstruction(TR::Node *node);
    void _mapWriteBarrierInstruction(TR::Node *node);


public:
    TR_OWLMapper(TR::Compilation* compilation);
    ~TR_OWLMapper();
    std::vector<OWLInstruction> map();
};
#endif 
