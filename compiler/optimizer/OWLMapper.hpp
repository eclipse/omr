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

/**
 * Simulating java byte code local variable table 
 */
class TR_OWLLocalVariableTable
{
private:
    uint32_t _index; 
    std::unordered_map<TR::SymbolReference*, uint32_t> _localVarTableBySymRef;
    std::unordered_map<TR::Node*, uint32_t> _localVarTableByNode;
    std::unordered_map<TR::ParameterSymbol*, uint32_t> _localVarTableByParmSym;
    void _increaseIndex(char* type);
public:
    TR_OWLLocalVariableTable();
    uint32_t store(TR::SymbolReference* symRef,char* type);
    uint32_t implicitStore(TR::Node* node, char* type);
    uint32_t load(TR::SymbolReference* symRef);
    uint32_t implicitLoad(TR::Node* node);
    uint32_t storeParameter(TR::ParameterSymbol* parmSym, char* type);
    uint32_t loadParameter(TR::ParameterSymbol* parmSym);
    bool contain(TR::Node* node);
    bool contain(TR::SymbolReference* symRef);
    bool contain(TR::ParameterSymbol* parmSym);

};

/**
 * This Class performs the OMR IL to ShirkeBT instruction translation
 * The product of this class: a vector of Translation Unit objects that contain all the necessary data in order to construct shrikeBT instruction objects
 * OWLShrikeBTConstructor class takes the output of the map() method. 
 */
class TR_OWLMapper
{
private:

    TR_Debug* _debug;
    TR::Compilation* _compilation;
    TR_OWLLocalVariableTable* _localVarTable;

    std::vector<TranslationUnit> _translationUnits; 

    void _processTree(TR::Node *root, TR::Node *parent, TR::NodeChecklist &visited ); // traverse the IL tree
    void _logOMRIL(TR::Node *root, TR::NodeChecklist &visited); // log the OMR IL
    void _storeParameters(); //store the method parameters into local variable table
    void _adjustOffset(); // adjust the branch target of branch instructions

    void _instructionRouter(TR::Node *node); // route the Tree node to the corresponding mapping function

    char* _getType(TR::DataType dataType); // get the Java Byte code type 

    /*** create OWL instruction and push it into OWL instruction list ***/
    void _pushTranslationUnit
    (
        ShrikeBTInstruction instruction,
        uint32_t omrGlobalIndex, 
        uint32_t shrikeBTOffset, 
        BranchTargetLabelAdjustType branchTargetLabelAdjustType, 
        int32_t branchTargetLabelAdjustAmount,
        ShrikeBTInstructionFieldsUnion instructionFieldsUnion
    );
    
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
    void _mapCheckCastInstruction(TR::Node* node);


public:
    TR_OWLMapper(TR::Compilation* compilation);
    ~TR_OWLMapper();
    std::vector<TranslationUnit> map();
};
#endif 
