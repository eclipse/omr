//
// Created by Cijie Xia on 2019-09-23.
//

#include <string.h>
#include <stdlib.h>
#include "il/ILHelpers.hpp"
#include "il/MethodSymbol.hpp"
#include "il/ParameterSymbol.hpp"
#include "compile/Method.hpp"
#include "optimizer/OWLMapper.hpp"
#include "infra/List.hpp"

/***** Local Var table *******/
TR_OWLLocalVariableTable::TR_OWLLocalVariableTable() {
    _index = 0;
}

void TR_OWLLocalVariableTable::_increaseIndex(char* type) {
    if (strcmp(TYPE_double,type) == 0 || strcmp(TYPE_long,type) == 0){
        _index += 2;
    }
    else{
        _index += 1;
    }
}

uint32_t TR_OWLLocalVariableTable::store(TR::SymbolReference* symRef, char* type) {
    std::unordered_map<TR::SymbolReference*, uint32_t >::const_iterator it = _localVarTableBySymRef.find(symRef);

    uint32_t i = _index;

    // if the local var table has already stored this variable
    if (contain(symRef)) {
        i = _localVarTableBySymRef[symRef];
    }
    else {
        _localVarTableBySymRef[symRef] = _index;
        _increaseIndex(type);
    }

    return i;
}

uint32_t TR_OWLLocalVariableTable::load(TR::SymbolReference* symRef) {
    return _localVarTableBySymRef[symRef];
}

uint32_t TR_OWLLocalVariableTable::implicitStore(TR::Node* node, char* type) {
    uint32_t i = _index;

    if (contain(node)) {
        i = _localVarTableByNode[node];
    }
    else {
        _localVarTableByNode[node] = _index;
        _increaseIndex(type); 
    }

    return i;
}

uint32_t TR_OWLLocalVariableTable::implicitLoad(TR::Node* node) {
    return _localVarTableByNode[node];
}

uint32_t TR_OWLLocalVariableTable::storeParameter(TR::ParameterSymbol* parmSym, char* type) {
    u_int32_t i = _index;

    if (contain(parmSym)) {
        i = _localVarTableByParmSym[parmSym];
    }
    else {
        _localVarTableByParmSym[parmSym] = _index;
        _increaseIndex(type);
    }

    return i;
}

uint32_t TR_OWLLocalVariableTable::loadParameter(TR::ParameterSymbol* parmSym) {
    return _localVarTableByParmSym[parmSym];
}

bool TR_OWLLocalVariableTable::contain(TR::Node* node) {
    std::unordered_map<TR::Node* ,uint32_t >::const_iterator it = _localVarTableByNode.find(node);
    return it != _localVarTableByNode.end();
}

bool TR_OWLLocalVariableTable::contain(TR::SymbolReference* symRef) {
    std::unordered_map<TR::SymbolReference*, uint32_t >::const_iterator it = _localVarTableBySymRef.find(symRef);
    return it != _localVarTableBySymRef.end();
}

bool TR_OWLLocalVariableTable::contain(TR::ParameterSymbol* parmSym) {
    std::unordered_map<TR::ParameterSymbol*, uint32_t >::const_iterator it = _localVarTableByParmSym.find(parmSym);
    return it != _localVarTableByParmSym.end();
}

/****** Mapper ******/
TR_OWLMapper::TR_OWLMapper(TR::Compilation* compilation) {
    _compilation = compilation;
    _debug = compilation->getDebug();
    _localVarTable = new TR_OWLLocalVariableTable();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _localVarTable;
}

std::vector<TranslationUnit> TR_OWLMapper::map() {

    TR::NodeChecklist instructionMappingVisited(_compilation); // visited nodes for mapping the instruction

    _storeParameters();

    for (TR::TreeTop *tt = _compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _processTree(node, NULL, instructionMappingVisited);
    }

    _adjustOffset();

    return _translationUnits;
}

/****** private ******/
void TR_OWLMapper::_logOMRIL(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root)){
        return;
    }

    visited.add(root);

    for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
        _logOMRIL(root->getChild(i), visited);
    }
    printf("%d: %s | ref count: %u\n ", root->getGlobalIndex(),root->getOpCode().getName(),root->getReferenceCount());

    TR::ILOpCodes opCodeValue = root->getOpCodeValue();
    if ( opCodeValue == TR::treetop || opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd){
        return;
    }
 
}

void TR_OWLMapper::_storeParameters() {
    
    TR::ResolvedMethodSymbol* methodSymbol = _compilation->getMethodSymbol();
    ListIterator<TR::ParameterSymbol>   paramIterator(&(methodSymbol->getParameterList()));
    TR::ParameterSymbol *p;
   
    for (p = paramIterator.getFirst(); p != NULL; p = paramIterator.getNext()) {
        _localVarTable->storeParameter(p, _getType(p->getDataType()));
    }
}

/**
 * STEP 1: pre-processing the tree: such as create implicit load from local var table if this node has been evaluated. And/ or rearrange the evaluation order or skip certain children of each instruction
 * STEP 2: map the current node: translate the OMR node to the corresponding shrikeBT instruction
 * STEP 3: post-processing the tree: create implicit load or store if this node will be referred later
 * */
void TR_OWLMapper::_processTree(TR::Node *root, TR::Node *parent, TR::NodeChecklist &visited) {
        
    TR::ILOpCode opCode = root->getOpCode();
    TR::ILOpCodes opCodeValue = root->getOpCodeValue();
    TR::SymbolReference *symRef = root->getSymbolReference();

    /*** 1. Pre-processing ***/

    if (visited.contains(root)){ // indicates this node has already been evaluated => load the value from local var table

        if (_localVarTable->contain(root)){
            _createImplicitLoad(root);
        }
        
        return;
    }

    visited.add(root);

    if (opCode.isTernary()){ // deal with ternary expression. Evaluate last two children first

        _processTree(root->getChild(1), root, visited);
        _processTree(root->getChild(2), root, visited);
        _processTree(root->getChild(0), root, visited);
        
    }
    else if (opCode.isCheckCast()) {
        _processTree(root->getChild(0),root,visited); //only evaluate the first aload
    }
    else if (opCode.isBndCheck()) { // skip BNDcheck

    }
    else if (opCode.isCallIndirect()) {
        // skip the first aload
        _processTree(root->getChild(0)->getChild(0), root, visited);  // only evaluate this aload 

    }
    else if (opCodeValue == TR::newarray) { // skip the second child of new array (it indicates the type)
        _processTree(root->getChild(0), root, visited);
    }
    else if (opCodeValue == TR::multianewarray) {
        for (uint32_t i = 1; i < root->getNumChildren()-1; ++i) {
            _processTree(root->getChild(i), root, visited);
        }
    }
    else if ( !opCode.isWrtBar() && opCode.isStoreIndirect() && symRef->getSymbol()->isArrayShadowSymbol()) { // array store for primitive types
        _processTree(root->getChild(0)->getChild(0), root, visited); // ref
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited); //index
        _processTree(root->getChild(1), root, visited); //value
    }
    else if (opCode.isLoadIndirect() && symRef->getSymbol()->isArrayShadowSymbol()) { //array load 
        _processTree(root->getChild(0)->getChild(0), root, visited);
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited);
    }
    else if (opCode.isSwitch()){
        _processTree(root->getChild(0), root, visited);
    }
    else if (opCode.isWrtBar() && symRef->getSymbol()->isArrayShadowSymbol() ){ //array store for reference types
        _processTree(root->getChild(2),root, visited); // ref
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited); //index
        _processTree(root->getChild(1), root, visited); //value
    }
    else { 
        for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
            _processTree(root->getChild(i), root, visited);
        }
    }

    /*** 2. processing the current node ***/

    _instructionRouter(root); // routed to corresponding mapping

    /*** 3. post-processsing ***/

    if (opCodeValue == TR::New) {  // if it is new instruction, do not create implicit load or store, create dup for New Object. Implicit store will be created by constructor after it is invoked
        _createDupInstruction(root , 0);
        return;
    }   

    if (opCode.isWrtBar()) {
        return;
    }

    // for those node reference count > 1, create implicit load or store 
    if (root->getReferenceCount() > 1) {
        if (parent == NULL){
            _createImplicitStore(root);
        }
        else{
            // only need to create implicit store

            if (parent->getOpCodeValue() == TR::treetop || 
            parent->getOpCodeValue() == TR::BBStart || 
            (parent->getOpCode().isCheck() && !parent->getOpCode().isCheckCast()) || // except for check cast
            parent->getOpCodeValue() == TR::compressedRefs){
                _createImplicitStore(root);

            }
            else{
                _createImplicitStore(root);
                _createImplicitLoad(root);
            }
        }
    }

    // post-processing for call direct and indirect instructions
    if (opCode.isCall()) {

        // for constructors, create implicit store if the reference count of NEW is > 2, otherwise, create POP since the object ref won't be referred anymore
        if (opCode.isCallDirect() && root->getSymbolReference()->getSymbol()->getMethodSymbol()->getMethod()->isConstructor()){
            if (root->getChild(0)->getReferenceCount() > 2){
                _createImplicitStore(root->getChild(0)); // create implicit store for new after constructor has been invoked!
            }
            else{
                _createPopInstruction(root,1); //pop the ref from stack
            }
        }

        //for method calls, if the return type is not void and its referece count is 1. Create POP instruction. 
        if (strcmp(TYPE_void, _getType(opCode.getDataType())) != 0 && root->getReferenceCount() < 2) {
            _createPopInstruction(root, 1);
        }
        
    }

    
}

void TR_OWLMapper::_adjustOffset() {
    /**** build OMR index to shrikeBT offset table ****/
    std::unordered_map<uint32_t,uint32_t> offsetMappingTable;
    uint32_t offset = 0;
    for (uint32_t i = 0 ; i < _translationUnits.size(); i ++) {

        TranslationUnit translationUnit = _translationUnits[i];

        bool isShrikeBTInstruction = translationUnit.instruction != NOT_SHRIKE_BT_INSTRUCTION;

        offsetMappingTable[translationUnit.omrGlobalIndex] = offset;
        if (isShrikeBTInstruction) {
            offset ++;
        }
    }
    
    offset = 0;
    /**** Adjust branch target label and instruction offsets ****/
    for (uint32_t i = 0 ; i < _translationUnits.size(); i ++) {
        TranslationUnit* translationUnit = &_translationUnits[i];
        bool isShrikeBTInstruction= translationUnit->instruction != NOT_SHRIKE_BT_INSTRUCTION;
        ShrikeBTInstruction instruction = translationUnit->instruction;
        
        if (isShrikeBTInstruction) {
            BranchTargetLabelAdjustType adjustType = translationUnit->branchTargetLabelAdjustType;
            ShrikeBTInstructionFieldsUnion *instrUnion = &translationUnit->instructionFieldsUnion;
            uint32_t label;

            if (instruction == SHRIKE_BT_GOTO) {

                GotoInstructionFields *gotoFields = &instrUnion->gotoInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[gotoFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + translationUnit->branchTargetLabelAdjustAmount;
                }

                gotoFields->label = label;
               
            }
            else if (instruction == SHRIKE_BT_CONDITIONAL_BRANCH) {

                ConditionalBranchInstructionFields *condiFields = &instrUnion->conditionalBranchInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[condiFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + translationUnit->branchTargetLabelAdjustAmount;
                }

                condiFields->label = label;
            }
            else if (instruction == SHRIKE_BT_SWITCH) {
                
                SwitchInstructionFields *switchFields = &instrUnion->switchInstructionFields;

                if (adjustType == TABLE_MAP) {
                    switchFields->defaultLabel = offsetMappingTable[switchFields->defaultLabel];

                    for (uint32_t i = 1; i < switchFields->length; i += 2) {
                        switchFields->casesAndLabels[i] = offsetMappingTable[switchFields->casesAndLabels[i]];
                    }

                }
            }
            
            translationUnit->shrikeBTOffset = offset;
            offset ++;
        }
    }

}

void TR_OWLMapper::_instructionRouter(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
  
    /*** deal with the nodes that are tree top or BBStart, BBEnd ***/
    if ( opCodeValue == TR::treetop || opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd){
        ShrikeBTInstructionFieldsUnion instrUnion;
        // need to take a place in instruction list
        _pushTranslationUnit(NOT_SHRIKE_BT_INSTRUCTION, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion );
        return;
    }

    TR::ILOpCode opCode = node->getOpCode();

    if (opCode.isLoadConst()) { //constant instruction
         _mapConstantInstruction(node);
    }
    else if (opCode.isStoreDirect()) { // store instruction
        _mapDirectStoreInstruction(node);
    }
    else if (opCode.isLoadDirect()) { // load instruction
        _mapDirectLoadInstruction(node);
    }
    else if (opCode.isStoreIndirect()) { //storei
        _mapIndirectStoreInstruction(node);
    }
    else if (opCode.isLoadIndirect()) { //loadi
        _mapIndirectLoadInstruction(node);
    }
    else if (opCode.isReturn()) { // return instruction
        _mapReturnInstruction(node);
    }
    else if (opCode.isArrayRef()) {} // aladd
    else if (opCode.isArithmetic()){ // arithmetic instructions
        _mapArithmeticInstruction(node);
    }
    else if (opCode.isGoto()){ // goto instruction
        _mapGotoInstruction(node);
    }
    else if (opCode.isBooleanCompare() && opCode.isBranch()){ // conditional branch instruction
        _mapConditionalBranchInstruction(node);
    }
    else if (opCode.isConversion()) { // conversion Instruction
        _mapConversionInstruction(node);
    }
    else if (opCode.isBooleanCompare()){ // comparison instruction
        _mapComparisonInstruction(node);
    }
    else if (opCode.isCallDirect()) { // direct call
        _mapDirectCallInstruction(node);
    }
    else if (opCode.isCallIndirect()) { // indirect call
        _mapIndirectCallInstruction(node);
    }
    else if (opCode.isTernary()) { // ternary instruction
        _mapTernaryInstruction(node);
    }
    else if (opCode.isLoadAddr()) {} //loadaddr
    else if (opCode.isNullCheck()) {} //NULL CHECK
    else if (opCode.isAnchor()) { // compressedRef, create one pop
        _createPopInstruction(node,1);
    }
    else if (opCode.isNew()){ // new object, new array or new multidimensional array
        _mapNewInstruction(node);
    }
    else if (opCodeValue == TR::instanceof){ // instance of
        _mapInstanceofInstruction(node);
    }
    else if (opCode.isArrayLength()) { //array length
        _mapArrayLengthInstruction(node);
    }
    else if (opCode.isSwitch()){
        _mapSwitchInstruction(node);
    }
    else if (opCode.isWrtBar()) {
        _mapWriteBarrierInstruction(node);
    }
    else if (opCode.isCheckCast()) {
        _mapCheckCastInstruction(node);
    }

}

/**
 * Currently signed and unsigned are both treated as signed. May need modification later
 */
char* TR_OWLMapper::_getType(TR::DataType dataType) {
    if (dataType.isInt8()) return TYPE_int; // no byte for Java Bytecode
    else if (dataType.isInt16()) return TYPE_int; // no short type for Java Bytecode
    else if (dataType.isInt32() ) return TYPE_int;
    else if (dataType.isInt64()) return TYPE_long;
    else if (dataType.isDouble()) return TYPE_double;
    else if (dataType.isFloatingPoint()) return TYPE_float;
    else if (dataType.isAddress()) return TYPE_Object;
    else return TYPE_void; 
}

void TR_OWLMapper::_createImplicitStore(TR::Node *node) {
    char* type = _getType(node->getOpCode().getDataType());

    uint32_t omrGlobalIndex = node->getGlobalIndex();

    uint32_t index = _localVarTable->implicitStore(node,type);

    StoreInstructionFields storeFields;
    strcpy(storeFields.type, type);

    storeFields.index = index;

    ShrikeBTInstructionFieldsUnion insUnion;

    insUnion.storeInstructionFields = storeFields;

    _pushTranslationUnit(SHRIKE_BT_STORE, omrGlobalIndex,0, NO_ADJUST, 0, insUnion);
}

void TR_OWLMapper::_createImplicitLoad(TR::Node* node) {
    char* type = _getType(node->getOpCode().getDataType());
    uint32_t omrGlobalIndex = node->getGlobalIndex();
    
    uint32_t index = _localVarTable->implicitLoad(node);

    LoadInstructionFields loadFields;
    strcpy(loadFields.type, type);
    loadFields.index = index;

    ShrikeBTInstructionFieldsUnion insUnion;
    insUnion.loadInstructionFields = loadFields;
    
    _pushTranslationUnit(SHRIKE_BT_LOAD, omrGlobalIndex, 0, NO_ADJUST, 0, insUnion);
}

void TR_OWLMapper::_createDupInstruction(TR::Node *node, uint16_t delta) {
    DupInstructionFields dupFields = {delta};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.dupInstructionFields = dupFields;

    _pushTranslationUnit(SHRIKE_BT_DUP, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
}

void TR_OWLMapper::_createPopInstruction(TR::Node* node, uint16_t size) {
    PopInstructionFields popFields = {size};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.popInstructionFields = popFields;
    _pushTranslationUnit(SHRIKE_BT_POP, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
}

void TR_OWLMapper::_createSwapInstruction(TR::Node* node) {
    SwapInstructionFields swapFields = {};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.swapInstructionFields = swapFields;
    _pushTranslationUnit(SHRIKE_BT_SWAP, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
}

/**
 * Push a translation unit into translation unit vector
 * */
void TR_OWLMapper::_pushTranslationUnit(ShrikeBTInstruction instruction , uint32_t omrGlobalIndex, uint32_t shrikeBTOffset, 
    BranchTargetLabelAdjustType branchTargetLabelAdjustType,int32_t branchTargetLabelAdjustAmount, 
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion) 
{

    TranslationUnit translationUnit = {
        instruction,
        omrGlobalIndex,
        shrikeBTOffset,
        branchTargetLabelAdjustType,
        branchTargetLabelAdjustAmount,
        instructionFieldsUnion,
    };

    _translationUnits.push_back(translationUnit);
}

void TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {
    ConstantInstructionFields constFields;

    TR::ILOpCode opCode = node->getOpCode();

    char* type = _getType(opCode.getDataType());
    strcpy(constFields.type, type);

    if (opCode.isInt()){
        constFields.value.i = node->getInt();
    }
    else if (opCode.isFloat()){
        constFields.value.f = node->getFloat();
    }
    else if (opCode.isDouble()){
        constFields.value.d = node->getDouble();
    }
    else if (opCode.isShort()){
        constFields.value.i = node->getShortInt();
    }
    else if (opCode.isLong()){
        constFields.value.l = node->getLongInt();
    }
    else if (opCode.isByte()){
        constFields.value.i = node->getByte();
    }
    else if (node->getSymbolReference() == NULL){ // aconst null
        strcpy(constFields.type, TYPE_null); 
    }
    else{
        printf("No Constant type matched!\n");
        exit(1);
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.constantInstructionFields = constFields;

    _pushTranslationUnit(SHRIKE_BT_CONSTANT, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
}

void TR_OWLMapper::_mapDirectStoreInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode.getDataType());

    TR::SymbolReference* symRef = node->getSymbolReference();
    TR::Symbol* symbol = symRef->getSymbol();

    if (symbol->isAuto()) { //local var
        StoreInstructionFields storeFields;

        uint32_t index = _localVarTable->store(symRef, type);

        strcpy(storeFields.type, type);
        storeFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.storeInstructionFields = storeFields;

        _pushTranslationUnit(SHRIKE_BT_STORE, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);

    }
    else if (symbol->isConstString()) { // skip string for now

    }
    else if (symbol->isParm()) { //parameter
        StoreInstructionFields storeFields;
        strcpy(storeFields.type, type);
        
        uint32_t index = _localVarTable->storeParameter(symbol->getParmSymbol(), type);
        storeFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.storeInstructionFields = storeFields;
        _pushTranslationUnit(SHRIKE_BT_STORE, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion);
    }
    else if (symbol->isStatic()) { // static (SHRIKE_BT_PUT)

        PutInstructionFields putFields;

        char staticName[LARGE_BUFFER_SIZE];
        strcpy(staticName, _debug->getStaticName(symRef));

        char* clsNameToken = strtok(staticName,".");
        strcpy(putFields.className,"L");
        strcat(putFields.className, clsNameToken);
        strcat(putFields.className,";");

        char* restToken = strtok(NULL,".");
        char* fldNameToken = strtok(restToken," ");

        strcpy(putFields.fieldName,fldNameToken);
        strcpy(putFields.type, type);

        putFields.isStatic = true;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.putInstructionFields = putFields;

        _pushTranslationUnit(SHRIKE_BT_PUT, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }
   
}

void TR_OWLMapper::_mapDirectLoadInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode.getDataType());

    TR::SymbolReference* symRef = node->getSymbolReference();
    TR::Symbol* symbol = symRef->getSymbol();

    if (symbol->isAuto()) { //local var

        LoadInstructionFields loadFields;
        strcpy(loadFields.type, type);

        uint32_t index = _localVarTable->load(symRef);
        loadFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.loadInstructionFields = loadFields;

        _pushTranslationUnit(SHRIKE_BT_LOAD, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion);
    }
    else if (symbol->isConstString() ) { //skip string for now
        
    }
    else if (symbol->isParm()) { // parameter
        LoadInstructionFields loadFields;
        strcpy(loadFields.type, type);
        
        uint32_t index = _localVarTable->loadParameter(symbol->getParmSymbol());
        loadFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.loadInstructionFields = loadFields;
        _pushTranslationUnit(SHRIKE_BT_LOAD, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion);
    }
    else if (symbol->isStatic()) { // static (SHRIKE_BT_GET)

        GetInstructionFields getFields;

        char staticName[LARGE_BUFFER_SIZE];
        strcpy(staticName, _debug->getStaticName(symRef));
        char* clsNameToken = strtok(staticName,".");
        strcpy(getFields.className,"L");
        strcat(getFields.className, clsNameToken);
        strcat(getFields.className,";");

        char* restToken = strtok(NULL,".");
        char* fldNameToken = strtok(restToken," ");

        strcpy(getFields.fieldName,fldNameToken);
        strcpy(getFields.type, type);

        getFields.isStatic = true;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.getInstructionFields = getFields;

        _pushTranslationUnit(SHRIKE_BT_GET, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
    }

}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode.getDataType());
    
    ReturnInstructionFields returnFields;
    strcpy(returnFields.type, type);

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.returnInstructionFields = returnFields;

    _pushTranslationUnit(SHRIKE_BT_RETURN, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
}

void TR_OWLMapper::_mapArithmeticInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode.getDataType());

    /**** Unary op ****/
    if (opCode.isNeg()){
        UnaryOpInstructionFields unaryFields;
        strcpy(unaryFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.unaryOpInstructionFields = unaryFields;
        _pushTranslationUnit(SHRIKE_BT_UNARY_OP, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
    }
    /*** Shift ***/
    else if (opCode.isShift()) {
        ShiftInstructionFields shiftFields;
        ShrikeBTShiftOperator op;
        strcpy(shiftFields.type, type);

        if (opCode.isLeftShift()) op = SHL;
        else if (opCode.isRightShift() && !opCode.isShiftLogical()) op = SHR;
        else op = USHR;
        
        shiftFields.op = op;
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.shiftInstructionFields = shiftFields;
        _pushTranslationUnit(SHRIKE_BT_SHIFT, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }
    /**** Binary op ****/
    else{
        ShrikeBTBinaryOperator op;

        if (opCode.isAdd()) op = ADD;
        else if (opCode.isSub()) op = SUB;
        else if (opCode.isMul()) op = MUL;
        else if (opCode.isDiv()) op = DIV;
        else if (opCode.isRem()) op = REM;
        else if (opCode.isAnd()) op = AND;
        else if (opCode.isOr()) op = OR;
        else if (opCode.isXor()) op = XOR;
        else{
            printf("No arithmetic operator matched!\n");
            exit(1);
        }
        BinaryOpInstructionFields binaryFields;
        strcpy(binaryFields.type, type);
        binaryFields.op = op;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.binaryOpInstructionFields = binaryFields;

        _pushTranslationUnit(SHRIKE_BT_BINARY_OP, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion);
    }
}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    GotoInstructionFields gotoFields = {index};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.gotoInstructionFields = gotoFields;

    _pushTranslationUnit(SHRIKE_BT_GOTO, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion);
}

void TR_OWLMapper::_mapConditionalBranchInstruction(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
    ShrikeBTConditionalBranchOperator op;

    if (TR::ILOpCode::isStrictlyLessThanCmp(opCodeValue)) op = LT;
    else if (TR::ILOpCode::isStrictlyGreaterThanCmp(opCodeValue)) op = GT;
    else if (TR::ILOpCode::isLessCmp(opCodeValue)) op = LE;
    else if (TR::ILOpCode::isGreaterCmp(opCodeValue)) op = GE;
    else if (TR::ILOpCode::isEqualCmp(opCodeValue)) op = EQ;
    else op = NE;

    TR::ILOpCode childOpCode = node->getFirstChild()->getOpCode();
    char* type = _getType(childOpCode.getDataType());
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    ConditionalBranchInstructionFields condiFields;
    strcpy(condiFields.type, type);
    condiFields.op = op;
    condiFields.label = index;

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.conditionalBranchInstructionFields = condiFields;

    _pushTranslationUnit(SHRIKE_BT_CONDITIONAL_BRANCH, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion);
}

/**
 * Since Java Byte Code does not have a full set of operators like OMR
 * Comparison instruction will be mapped as conditional branch instruction 
 */
void TR_OWLMapper::_mapComparisonInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
    char* type = _getType(opCode.getDataType());

    TR_ComparisonTypes cmpType = TR::ILOpCode::getCompareType(opCodeValue);
    ShrikeBTConditionalBranchOperator op;

    if (!opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfGreater() ){ //GT
        op = GT;
    }
    else if (!opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfLess()){ //LT
        op = LT;
    }
    else if (!opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfGreater()) { //GE
        op = GE;
    }
    else if (!opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual() && opCode.isCompareTrueIfLess() ) { // LE
        op = LE;
    }
    else if (opCode.isCompareForEquality() && opCode.isCompareTrueIfEqual()) { // EQ
        op = EQ;
    }
    else if (opCode.isCompareForEquality() && !opCode.isCompareTrueIfEqual()){ //NE
        op = NE;
    }
    else{
        printf("No cmp type matched!\n");
        exit(1);
    }

    ConditionalBranchInstructionFields condiFields;
    strcpy(condiFields.type, type);
    condiFields.op = op;
    condiFields.label = 0;

    ConstantInstructionFields const0Fields;
    strcpy(const0Fields.type, TYPE_int);
    const0Fields.value.i = 0;

    GotoInstructionFields gotoFields = {0};

    ConstantInstructionFields const1Fields;
    strcpy(const1Fields.type, TYPE_int);
    const1Fields.value.i = 1;

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _pushTranslationUnit(SHRIKE_BT_CONDITIONAL_BRANCH, node->getGlobalIndex(), 0, BY_VALUE, 3, instrUnion);

    instrUnion.constantInstructionFields = const0Fields;
    _pushTranslationUnit(SHRIKE_BT_CONSTANT, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
  
    instrUnion.gotoInstructionFields = gotoFields;
    _pushTranslationUnit(SHRIKE_BT_GOTO,node->getGlobalIndex(),0,BY_VALUE,2,instrUnion);
   
    instrUnion.constantInstructionFields = const1Fields;
    _pushTranslationUnit(SHRIKE_BT_CONSTANT,node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
}

void TR_OWLMapper::_mapConversionInstruction(TR::Node *node) {

    char* fromType = NULL;
    char* toType = NULL;

    TR::ILOpCodes opCodeValue = node->getOpCode().getOpCodeValue();
    switch(opCodeValue) {
        case TR::i2l:
        case TR::iu2l:
        case TR::b2l:
        case TR::bu2l:
        case TR::s2l:
        case TR::su2l:
            fromType = TYPE_int;
            toType = TYPE_long;
            break;
        case TR::i2f:
        case TR::iu2f:
        case TR::b2f:
        case TR::bu2f:
        case TR::s2f:
        case TR::su2f:
            fromType = TYPE_int;
            toType = TYPE_float;
            break;
        case TR::i2d:
        case TR::iu2d:
        case TR::b2d:
        case TR::bu2d:
        case TR::s2d:
        case TR::su2d:
            fromType = TYPE_int;
            toType = TYPE_double;
            break;
        case TR::l2i:
        case TR::l2b:
        case TR::l2s:
            fromType = TYPE_long;
            toType = TYPE_int;
            break;
        case TR::l2f:
        case TR::lu2f:
            fromType = TYPE_long;
            toType = TYPE_float;
            break;
        case TR::l2d:
        case TR::lu2d:
            fromType = TYPE_long;
            toType = TYPE_double;
            break;
        case TR::f2i:
        case TR::f2b:
        case TR::f2s:
            fromType = TYPE_float;
            toType = TYPE_int;
            break;
        case TR::f2l:
            fromType = TYPE_float;
            toType = TYPE_long;
            break;
        case TR::f2d:
            fromType = TYPE_float;
            toType = TYPE_double;
            break;
        case TR::d2i:
        case TR::d2s:
        case TR::d2b:
            fromType = TYPE_double;
            toType = TYPE_int;
            break;
        case TR::d2l:
            fromType = TYPE_double;
            toType = TYPE_long;
            break;
        case TR::d2f:
            fromType = TYPE_double;
            toType = TYPE_float;
            break;
        case TR::b2i:
        case TR::bu2i:
        case TR::s2i:
        case TR::su2i:
        case TR::i2b:
        case TR::i2s:
        case TR::s2b:
        case TR::b2s:
            // no conversion needed. Since all byte and short have already been converted into int by the OWLMapper
            break;
        default:
            printf("No type conversion matched!\n");
            exit(1);
    }

    if (fromType && toType){
        ConversionInstructionFields conversionFields;
        strcpy(conversionFields.fromType, fromType);
        strcpy(conversionFields.toType, toType);
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.conversionInstructionFields = conversionFields;
        _pushTranslationUnit(SHRIKE_BT_CONVERSION, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
        
    }

}

void TR_OWLMapper::_mapDirectCallInstruction(TR::Node *node) {

    TR::MethodSymbol *methodSymbol = node->getSymbolReference()->getSymbol()->getMethodSymbol();
    TR::Method * method = methodSymbol->getMethod();

    TR::MethodSymbol::Kinds methodKind = methodSymbol->getMethodKind();

    char* className = method->classNameChars();
    uint16_t classNameLength = method->classNameLength();

    char* methodName = method->nameChars();
    uint16_t methodNameLength = method->nameLength();

    char* signature = method->signatureChars();
    uint16_t signatureLength = method->signatureLength();

    InvokeInstructionFields invokeFields;

    if (method->isConstructor()){ // if the call is constructor
        TR::Node *newNode = node->getChild(0);
        TR::Node *loadAddr = newNode->getChild(0);
 
        int32_t len;
        strcpy(invokeFields.className, loadAddr->getSymbolReference()->getTypeSignature(len));
        invokeFields.className[len] = '\0';

        invokeFields.disp = SPECIAL;
    }
    else if (methodKind == methodSymbol->Static) { // static method call
        strncpy(invokeFields.className, className, classNameLength);
        invokeFields.className[classNameLength] = '\0';

        invokeFields.disp = STATIC;
    }
    else if (methodKind == methodSymbol->Virtual) {
        strncpy(invokeFields.className, className, classNameLength);
        invokeFields.className[classNameLength] = '\0';

        invokeFields.disp = VIRTUAL;
    }

    strncpy(invokeFields.methodName, methodName, methodNameLength);
    invokeFields.methodName[methodNameLength] = '\0';

    strncpy(invokeFields.type, signature,signatureLength );
    invokeFields.type[signatureLength] = '\0';



    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _pushTranslationUnit(SHRIKE_BT_INVOKE, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion); 

}

void TR_OWLMapper::_mapIndirectCallInstruction(TR::Node* node) {

    TR::MethodSymbol *methodSymbol = node->getSymbolReference()->getSymbol()->getMethodSymbol();
    TR::Method * method = methodSymbol->getMethod();

    TR::MethodSymbol::Kinds methodKind = methodSymbol->getMethodKind();

    char* className = method->classNameChars();
    uint16_t classNameLength = method->classNameLength();

    char* methodName = method->nameChars();
    uint16_t methodNameLength = method->nameLength();

    char* signature = method->signatureChars();
    uint16_t signatureLength = method->signatureLength();

    InvokeInstructionFields invokeFields;
    strncpy(invokeFields.type, signature, signatureLength);
    invokeFields.type[signatureLength] = '\0';
    
    strncpy(invokeFields.className, className, classNameLength);
    invokeFields.className[classNameLength] = '\0';

    strncpy(invokeFields.methodName, methodName, methodNameLength);
    invokeFields.methodName[methodNameLength] = '\0';

    if (methodKind == methodSymbol->Virtual) {

        invokeFields.disp = VIRTUAL;

    }
    else if (methodKind == methodSymbol->Static) {
        invokeFields.disp = STATIC;

    }
    else if (methodKind == methodSymbol->Interface) {
        invokeFields.disp = INTERFACE;
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _pushTranslationUnit(SHRIKE_BT_INVOKE, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion); 

}

/**
 * A ternary expression will be expanded into 
 * const 0
 * conditional branch (Not equal)
 * swap, 
 * pop, 
 * goto, 
 * pop
 */
void TR_OWLMapper::_mapTernaryInstruction(TR::Node *node) {
    ConstantInstructionFields const0Fields;
    strcpy(const0Fields.type, TYPE_int);
    const0Fields.value.i = 0;

    ConditionalBranchInstructionFields condiFields;
    strcpy(condiFields.type, TYPE_int);
    condiFields.op = NE;
    condiFields.label = 0;

    GotoInstructionFields gotoFields = {0};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.constantInstructionFields = const0Fields;
    _pushTranslationUnit(SHRIKE_BT_CONSTANT, node->getGlobalIndex(), 0, NO_ADJUST,0,instrUnion);

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _pushTranslationUnit(SHRIKE_BT_CONDITIONAL_BRANCH, node->getGlobalIndex(), 0, BY_VALUE, 4, instrUnion);

    _createSwapInstruction(node);

    _createPopInstruction(node,1);

    instrUnion.gotoInstructionFields = gotoFields;
    _pushTranslationUnit(SHRIKE_BT_GOTO, node->getGlobalIndex(), 0, BY_VALUE, 2, instrUnion );

   _createPopInstruction(node,1);
}

void TR_OWLMapper::_mapIndirectStoreInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode().getDataType());
    TR::SymbolReference* symRef = node->getSymbolReference();

    if (symRef->getSymbol()->isRegularShadow() && !symRef->getSymbol()->isArrayShadowSymbol()){ // object fields

        char shadowName[LARGE_BUFFER_SIZE];
        strcpy(shadowName, _debug->getShadowName(symRef));
        
        PutInstructionFields putFields;

        //tokenize the shadow name for class type and field name
        char* clsNameToken = strtok(shadowName,".");
        strcpy(putFields.className,"L");
        strcat(putFields.className, clsNameToken);
        strcat(putFields.className,";");

        char* restToken = strtok(NULL,".");
        char* fldNameToken = strtok(restToken," ");

        strcpy(putFields.fieldName,fldNameToken);

        strcpy(putFields.type, type);
        putFields.isStatic = false;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.putInstructionFields = putFields;

        _pushTranslationUnit(SHRIKE_BT_PUT, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }
    else { // array
        ArrayStoreInstructionFields arrayStoreFields;
        strcpy(arrayStoreFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayStoreInstructionFields = arrayStoreFields;

        _pushTranslationUnit(SHRIKE_BT_ARRAY_STORE, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }
    
}

void TR_OWLMapper::_mapIndirectLoadInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode().getDataType());
    TR::SymbolReference* symRef = node->getSymbolReference();
    
    if (symRef->getSymbol()->isRegularShadow() && !symRef->getSymbol()->isArrayShadowSymbol()){ // object fields
        char shadowName[LARGE_BUFFER_SIZE];
        strcpy(shadowName, _debug->getShadowName(symRef));
        
        GetInstructionFields getFields;

        //tokenize the shadow name for class type and field name
        char* clsNameToken = strtok(shadowName,".");
        strcpy(getFields.className,"L");
        strcat(getFields.className, clsNameToken);
        strcat(getFields.className,";");

        char* restToken = strtok(NULL,".");
        char* fldNameToken = strtok(restToken," ");

        strcpy(getFields.fieldName,fldNameToken);

        strcpy(getFields.type, type);
        getFields.isStatic = false;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.getInstructionFields = getFields;

        _pushTranslationUnit(SHRIKE_BT_GET, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }
    else{ //array
        ArrayLoadInstructionFields arrayLoadFields;
        strcpy(arrayLoadFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayLoadInstructionFields = arrayLoadFields;

        _pushTranslationUnit(SHRIKE_BT_ARRAY_LOAD, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
    }

}

void TR_OWLMapper::_mapNewInstruction(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();

    NewInstructionFields newFields;

    if (opCodeValue == TR::New) { //new class
        TR::Node *loadAddr = node->getChild(0);
    
        int32_t len;
        const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);

        strcpy(newFields.type, type);
        newFields.arrayBoundsCount = 0;
    }
    else if (opCodeValue == TR::newarray) { //new array
        char* type = _getType(node->getChild(1)->getOpCode().getDataType());

        strcpy(newFields.type, "[");
        strcat(newFields.type, type);
        newFields.arrayBoundsCount = 1;
    }
    else if (opCodeValue == TR::anewarray ) { // new reference array
        int32_t len;
        TR::Node *loadAddr = node->getChild(1);
        const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);
        
        strcpy(newFields.type, "[");
        strcat(newFields.type, type);
        newFields.arrayBoundsCount = 1;
    }
    else if (opCodeValue == TR::multianewarray) { // new multidimentianal array
        TR::Node* loadAddr = node->getLastChild();
        int32_t len;
        const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);
        strcpy(newFields.type, type);
        newFields.arrayBoundsCount = node->getFirstChild()->getInt();

    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.newInstructionFields = newFields;
    _pushTranslationUnit(SHRIKE_BT_ARRAY_NEW, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion); 

}

void TR_OWLMapper::_mapInstanceofInstruction(TR::Node *node) {
    InstanceofInstructionFields instanceofFields;

    TR::Node *loadAddr = node->getChild(1);

    int32_t len;
    const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);

    strcpy(instanceofFields.type, type);
    
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.instanceofInstructionFields = instanceofFields;
    _pushTranslationUnit(SHRIKE_BT_INSTANCE_OF, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
}

void TR_OWLMapper::_mapArrayLengthInstruction(TR::Node *node) {
    ArrayLengthInstructionFields arrayLenFields;

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayLengthInstructionFields = arrayLenFields;
    _pushTranslationUnit(SHRIKE_BT_ARRAY_LENGTH, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
}

void TR_OWLMapper::_mapSwitchInstruction(TR::Node *node) {
    SwitchInstructionFields switchFields;

    uint16_t childrenNum = node->getNumChildren();
    uint32_t j = 0;

    for (uint16_t i = 2; i < childrenNum; i ++) {
        TR::Node *caseNode = node->getChild(i);
        switchFields.casesAndLabels[j++] = caseNode->getCaseConstant();
        switchFields.casesAndLabels[j++] = caseNode->getBranchDestination()->getNode()->getGlobalIndex();
    }

    switchFields.length = (childrenNum - 2)*2;
    switchFields.defaultLabel = node->getChild(1)->getBranchDestination()->getNode()->getGlobalIndex();
    
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.switchInstructionFields = switchFields;

    _pushTranslationUnit(SHRIKE_BT_SWITCH,node->getGlobalIndex(),0,TABLE_MAP,0,instrUnion);
}

void TR_OWLMapper::_mapWriteBarrierInstruction(TR::Node* node) {
    char *type = _getType(node->getOpCode().getDataType());
    ArrayStoreInstructionFields arrayStoreFields;
    strcpy(arrayStoreFields.type, type);

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayStoreInstructionFields = arrayStoreFields;

    _pushTranslationUnit(SHRIKE_BT_ARRAY_STORE, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
}

void TR_OWLMapper::_mapCheckCastInstruction(TR::Node* node) {
    TR::Node *loadAddr = node->getChild(1);
    int len;
    
    CheckCastInstructionFields checkCastFields;
    strcpy(checkCastFields.type, loadAddr->getSymbolReference()->getTypeSignature(len));

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.checkCastInstructionFields = checkCastFields;

    _pushTranslationUnit(SHRIKE_BT_CHECK_CAST, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion);
}