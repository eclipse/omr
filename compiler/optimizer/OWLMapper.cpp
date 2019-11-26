//
// Created by Cijie Xia on 2019-09-23.
//

#include <string.h>
#include <stdlib.h>
#include "il/ILHelpers.hpp"
#include "il/MethodSymbol.hpp"
#include "compile/Method.hpp"
#include "optimizer/OWLMapper.hpp"
#include "optimizer/OWLJNIConfig.hpp"

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

bool TR_OWLLocalVariableTable::contain(TR::Node* node) {
    std::unordered_map<TR::Node* ,uint32_t >::const_iterator it = _localVarTableByNode.find(node);
    return it != _localVarTableByNode.end();
}

bool TR_OWLLocalVariableTable::contain(TR::SymbolReference* symRef) {
    std::unordered_map<TR::SymbolReference*, uint32_t >::const_iterator it = _localVarTableBySymRef.find(symRef);
    return it != _localVarTableBySymRef.end();
}

/**********  Operand stack *********************/
bool TR_OWLOperandStack::contain(TR::Node* node) {
    std::unordered_map<TR::Node*, uint32_t >::const_iterator it = _numOnStackTable.find(node);
    return it != _numOnStackTable.end();
}

void TR_OWLOperandStack::push(TR::Node* node) {
    printf("PUSH %s: %d\n",node->getOpCode().getName(), node->getGlobalIndex());

    if (!contain(node)) {
        _numOnStackTable[node] = 1;
    }
    else{
        _numOnStackTable[node] += 1;
    }

    _stack.push(node);

}

void TR_OWLOperandStack::pop() {
    if (isEmpty()){
        printf("Error: pop when operand stack is empty!\n");
        exit(1);
    }
    printf("POP %s: %d\n",_stack.top()->getOpCode().getName(), _stack.top()->getGlobalIndex());
    _numOnStackTable[_stack.top()] -= 1;
    _stack.pop();
}

TR::Node* TR_OWLOperandStack::top() {
    if (isEmpty()){
        printf("Error: top when operand stack is empty!\n");
        exit(1);
    }
    return _stack.top();
}

void TR_OWLOperandStack::dup(){
    if (isEmpty()){
        printf("Error: dup when operand stack is empty!\n");
        exit(1);
    }
    printf("DUP %s: %d\n",_stack.top()->getOpCode().getName(),_stack.top()->getGlobalIndex());
    _numOnStackTable[_stack.top()] += 1;
    _stack.push(_stack.top());
}

void TR_OWLOperandStack::swap() {
    if (_stack.size() < 2){
        printf("Error: cannot perform swap on operand stack! elements number < 2\n");
        exit(1);
    }
    TR::Node* node1 = _stack.top();
    _stack.pop();
    TR::Node* node2 = _stack.top();
    _stack.pop();
    _stack.push(node1);
    _stack.push(node2);
}

bool TR_OWLOperandStack::isEmpty(){
    return _stack.size() == 0;
}

uint32_t TR_OWLOperandStack::size(){
    return _stack.size();
}

uint32_t TR_OWLOperandStack::getOperandNum(TR::Node* node) {
    if (!contain(node)) {
        printf("Error: node not on operand stack!\n");
        exit(1);
    }
    return _numOnStackTable[node];
}


/***** Reference Count table *****/
void TR_OWLReferenceCountTable::add(TR::Node* node, uint32_t remainingReferenceCount) {
    std::unordered_map<TR::Node*, uint32_t>::const_iterator it = _table.find(node);
    if (it != _table.end()){
        printf("Error: node already exits in reference count table!\n");
        exit(1);
    }

    _table[node] = remainingReferenceCount;
}

void TR_OWLReferenceCountTable::refer(TR::Node* node) {
    std::unordered_map<TR::Node*, uint32_t>::const_iterator it = _table.find(node);
    if (it == _table.end()){
        printf("Error: node does not exit in reference count table!\n");
        exit(1);
    }

    if (_table[node] == 0) {
        printf("Error: reference count is 0 in reference count table. Cannot be referred!\n");
    }

    _table[node] = _table[node] - 1;
}

bool TR_OWLReferenceCountTable::noMoreReference(TR::Node* node) {
    std::unordered_map<TR::Node*, uint32_t>::const_iterator it = _table.find(node);
    if (it == _table.end()){
        printf("Error: node does not exit in reference count table!\n");
        exit(1);
    }

    return _table[node] == 0;
}

/****** Mapper ******/
TR_OWLMapper::TR_OWLMapper(TR::Compilation* compilation) {
    _compilation = compilation;
    _debug = compilation->getDebug();

    _localVarTable = new TR_OWLLocalVariableTable();
    _operandStack = new TR_OWLOperandStack();
    _referenceCountTable = new TR_OWLReferenceCountTable();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _localVarTable;
    delete _operandStack;
    delete _referenceCountTable;
}

std::vector<OWLInstruction> TR_OWLMapper::map() {

    TR::NodeChecklist logVisited(_compilation); // visisted nodes for logging OMR IL
    TR::NodeChecklist instructionMappingVisited(_compilation); // visited nodes for mapping the instruction

    // printf("======= OMR Instructions ========\n");
    for (TR::TreeTop *tt = _compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _logOMRIL(node, logVisited);
    }

    for (TR::TreeTop *tt = _compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _processTree(node, NULL, instructionMappingVisited);
    }

    _adjustOffset();

    //check is there is any operand remains on operand stack
    if (!_operandStack->isEmpty()){
        printf("Warning: Operand stack is not empty! %d left on stack\n", _operandStack->size());
    }

    return _owlInstructionList;
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

/**
 * STEP 1: pre-processing the tree
 * STEP 2: map the current node
 * STEP 3: post-processing the tree
 * */
void TR_OWLMapper::_processTree(TR::Node *root, TR::Node *parent, TR::NodeChecklist &visited) {
        
    TR::ILOpCode opCode = root->getOpCode();
    TR::ILOpCodes opCodeValue = root->getOpCodeValue();
    TR::SymbolReference *symRef = root->getSymbolReference();

    /*** 1. Pre-processing ***/

    if (visited.contains(root)){ // indicates this node has already been evaluated => load the value from local var table

        //refer to the node
        _referenceCountTable->refer(root);

        if (opCode.isWrtBar()){
            return;
        }

        if (!_localVarTable->contain(root) && !_referenceCountTable->noMoreReference(root) && _operandStack->getOperandNum(root) == 1) { // if we cannot find it in local var table, only one is left on stack and this node still needs to be referenced later, create DUP
            _createDupInstruction(root, 0);
        }

        if (_localVarTable->contain(root)){
            _createImplicitLoad(root);
        }
        return;

    }

    //add an entry to reference count table
    if (root->getReferenceCount() >= 1){
        _referenceCountTable->add(root, root->getReferenceCount()-1);
    }

    visited.add(root);

    if (opCode.isTernary()){ // deal with ternary expression. Evaluate last two children first

        _processTree(root->getChild(1), root, visited);
        _processTree(root->getChild(2), root, visited);
        _processTree(root->getChild(0), root, visited);
        
    }
    // else if (opCode.isCallIndirect()) {
    //     root->getChild(0)->getChild(0)->setReferenceCount(root->getChild(0)->getChild(0)->getReferenceCount() -1 ); // skip the first aload
    //     _processTree(root->getChild(0)->getChild(0), root, visited);  // only evaluate second aload 

    // }
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
    else if (opCode.isLoadIndirect() && symRef->getSymbol()->isArrayShadowSymbol()) { //array load for primitive types
        _processTree(root->getChild(0)->getChild(0), root, visited);
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited);
    }
    else if (opCode.isSwitch()){
        _processTree(root->getChild(0), root, visited);
    }
    else if (opCode.isWrtBar() && symRef->getSymbol()->isArrayShadowSymbol() ){ //array store for reference types
        _referenceCountTable->refer(root->getChild(0)->getChild(0));
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

    //check the integrity of operand stack
    if (parent == NULL) {
        while (!_operandStack->isEmpty()){
            
            TR::Node *top = _operandStack->top();
            printf("Check stack %s: %d\n",top->getOpCode().getName(), top->getGlobalIndex());
            if (_referenceCountTable->noMoreReference(top)){ // the top element of the stack will not be referenced anymore, pop it.
                _createPopInstruction(root,1);
            }
            else{
                break;
            }
        }

    }

    if (opCode.isNew()) {  // if it is new instruction, do not create implicit load or store, create dup for New Object
        if (opCodeValue == TR::New) {
            _createDupInstruction(root , 0);
        }
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
            if (parent->getOpCodeValue() == TR::treetop || parent->getOpCodeValue() == TR::BBStart || parent->getOpCode().isNullCheck()){
                _createImplicitStore(root);
            }
            else{
                _createImplicitStore(root);
                _createImplicitLoad(root);
            }
        }
    }
}

void TR_OWLMapper::_adjustOffset() {
    /**** build OMR index to shrikeBT offset table ****/
    std::unordered_map<uint32_t,uint32_t> offsetMappingTable;
    uint32_t offset = 0;
    for (uint32_t i = 0 ; i < _owlInstructionList.size(); i ++) {

        OWLInstruction owlInstr = _owlInstructionList[i];

        bool isShrikeBTInstruction = owlInstr.isShrikeBTInstruction;

        offsetMappingTable[owlInstr.omrGlobalIndex] = offset;
        if (isShrikeBTInstruction) {
            offset ++;
        }
    }
    
    offset = 0;
    /**** Adjust branch target label and instruction offsets ****/
    for (uint32_t i = 0 ; i < _owlInstructionList.size(); i ++) {
        OWLInstruction* owlInstr = &_owlInstructionList[i];
        bool isShrikeBTInstruction= owlInstr->isShrikeBTInstruction;
        ShrikeBTInstruction instruction = owlInstr->instruction;
        
        if (isShrikeBTInstruction) {
            BranchTargetLabelAdjustType adjustType = owlInstr->branchTargetLabelAdjustType;
            ShrikeBTInstructionFieldsUnion *instrUnion = &owlInstr->instructionFieldsUnion;
            uint32_t label;

            if (instruction == SHRIKE_BT_GOTO) {

                GotoInstructionFields *gotoFields = &instrUnion->gotoInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[gotoFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + owlInstr->branchTargetLabelAdjustAmount;
                }

                gotoFields->label = label;
               
            }
            else if (instruction == SHRIKE_BT_CONDITIONAL_BRANCH) {

                ConditionalBranchInstructionFields *condiFields = &instrUnion->conditionalBranchInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[condiFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + owlInstr->branchTargetLabelAdjustAmount;
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
            
            owlInstr->shrikeBTOffset = offset;
            offset ++;
        }
    }

}

void TR_OWLMapper::_instructionRouter(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
  
    /*** deal with the nodes that are tree top or BBStart, BBEnd ***/
    if ( opCodeValue == TR::treetop || opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd){
        ShrikeBTInstructionFieldsUnion instrUnion;
        // need to take a place in instruction meta list
        _createOWLInstruction(false, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, NOT_INSTRUCTION );
        return;
    }

    printf("--- %d: %s | ref count: %u\n", node->getGlobalIndex(),node->getOpCode().getName(),node->getReferenceCount());

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
    else if (opCode.isLoadIndirect() && (node->getSymbolReference()->getReferenceNumber() - _compilation->getSymRefTab()->getNumHelperSymbols() == TR::SymbolReferenceTable::vftSymbol)) { // loadi <vtf-symbol>
        _createPopInstruction(node, 1);
    }
    else if (opCode.isLoadIndirect()) { //loadi
        _mapIndirectLoadInstruction(node);
    }
    else if (opCode.isReturn()) { // return instruction
        _mapReturnInstruction(node);
    }
    else if (opCode.isArrayRef()) {
        printf("&&& %s\n", opCode.getName());
    }
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
    else if (opCode.isBndCheck()) { // BNDCHK, create two pop
        _createPopInstruction(node,1);
        _createPopInstruction(node,1);
    }
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

}

char* TR_OWLMapper::_getType(TR::ILOpCode opCode) {

    if (opCode.isInt()) return TYPE_int;
    else if (opCode.isShort()) return TYPE_int; // no short type for Java Bytecode
    else if (opCode.isLong()) return TYPE_long;
    else if (opCode.isFloat()) return TYPE_float;
    else if (opCode.isDouble()) return TYPE_double;
    else if (opCode.isByte()) return TYPE_int; // no byte for Java Bytecode
    else if (opCode.isIntegerOrAddress()) return TYPE_Object;
    else if (opCode.isVoid()) return TYPE_void;
    printf("No type matched!\n");
    exit(1);
}

void TR_OWLMapper::_createImplicitStore(TR::Node *node) {

    char* type = _getType(node->getOpCode());

    uint32_t omrGlobalIndex = node->getGlobalIndex();
    
    uint32_t index = _localVarTable->implicitStore(node,type);

    StoreInstructionFields storeFields;
    strcpy(storeFields.type, type);
    storeFields.index = index;

    ShrikeBTInstructionFieldsUnion insUnion;

    insUnion.storeInstructionFields = storeFields;
    _createOWLInstruction(true, omrGlobalIndex,0, NO_ADJUST, 0, insUnion, SHRIKE_BT_STORE);
    _operandStack->pop();
}

void TR_OWLMapper::_createImplicitLoad(TR::Node* node) {
    char* type = _getType(node->getOpCode());
    uint32_t omrGlobalIndex = node->getGlobalIndex();
    
    uint32_t index = _localVarTable->implicitLoad(node);

    LoadInstructionFields loadFields;
    strcpy(loadFields.type, type);
    loadFields.index = index;

    ShrikeBTInstructionFieldsUnion insUnion;
    insUnion.loadInstructionFields = loadFields;
    
    _createOWLInstruction(true, omrGlobalIndex, 0, NO_ADJUST, 0, insUnion, SHRIKE_BT_LOAD);
    _operandStack->push(node);
}

void TR_OWLMapper::_createDupInstruction(TR::Node *node, uint16_t delta) {
    DupInstructionFields dupFields = {delta};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.dupInstructionFields = dupFields;

    _createOWLInstruction(true,node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_DUP);
    _operandStack->dup();
}

void TR_OWLMapper::_createPopInstruction(TR::Node* node, uint16_t size) {
    PopInstructionFields popFields = {size};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.popInstructionFields = popFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_POP);

    _operandStack->pop();
}

void TR_OWLMapper::_createSwapInstruction(TR::Node* node) {
    SwapInstructionFields swapFields = {};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.swapInstructionFields = swapFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_SWAP);
    _operandStack->swap();
}

/**
 * Push an owl instruction into owl instruction list
 * */
void TR_OWLMapper::_createOWLInstruction(bool isShrikeBTInstruction, uint32_t omrGlobalIndex, uint32_t shrikeBTOffset, 
    BranchTargetLabelAdjustType branchTargetLabelAdjustType,int32_t branchTargetLabelAdjustAmount, 
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion, ShrikeBTInstruction instruction ) 
{

    OWLInstruction owlInstr = {
        isShrikeBTInstruction,
        omrGlobalIndex,
        shrikeBTOffset,
        branchTargetLabelAdjustType,
        branchTargetLabelAdjustAmount,
        instructionFieldsUnion,
        instruction
    };

    _owlInstructionList.push_back(owlInstr);
}

void TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {
    ConstantInstructionFields constFields;

    TR::ILOpCode opCode = node->getOpCode();

    char* type = _getType(opCode);
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

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_CONSTANT);
    _operandStack->push(node);
}

void TR_OWLMapper::_mapDirectStoreInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    TR::SymbolReference* symRef = node->getSymbolReference();
    TR::Symbol* symbol = symRef->getSymbol();

    if (symbol->isAuto()) { //local var
        StoreInstructionFields storeFields;

        uint32_t index = _localVarTable->store(symRef, type);

        strcpy(storeFields.type, type);
        storeFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.storeInstructionFields = storeFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion, SHRIKE_BT_STORE);

        _operandStack->pop();
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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_PUT);
        _operandStack->pop();
    }
   
}

void TR_OWLMapper::_mapDirectLoadInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    TR::SymbolReference* symRef = node->getSymbolReference();
    TR::Symbol* symbol = symRef->getSymbol();

    if (symbol->isAuto()) { //local var

        LoadInstructionFields loadFields;
        strcpy(loadFields.type, type);

        uint32_t index = _localVarTable->load(symRef);
        loadFields.index = index;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.loadInstructionFields = loadFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion, SHRIKE_BT_LOAD);
        _operandStack->push(node);
    }
    else if (symbol->isConstString() ) { //skip string for now
        
        _operandStack->push(node);
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

        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_GET);
    }

}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    
    ReturnInstructionFields returnFields;
    strcpy(returnFields.type, type);

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.returnInstructionFields = returnFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_RETURN);
    _operandStack->pop();
}

void TR_OWLMapper::_mapArithmeticInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    /**** Unary op ****/
    if (opCode.isNeg()){
        UnaryOpInstructionFields unaryFields;
        strcpy(unaryFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.unaryOpInstructionFields = unaryFields;
        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_UNARY_OP);
        _operandStack->pop();
        _operandStack->push(node);
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
        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_SHIFT);
        _operandStack->pop();
        _operandStack->pop();
        _operandStack->push(node);
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

        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_BINARY_OP);
        _operandStack->pop();
        _operandStack->pop();
        _operandStack->push(node);
    }
}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    GotoInstructionFields gotoFields = {index};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.gotoInstructionFields = gotoFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, SHRIKE_BT_GOTO);
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
    char* type = _getType(childOpCode);
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    ConditionalBranchInstructionFields condiFields;
    strcpy(condiFields.type, type);
    condiFields.op = op;
    condiFields.label = index;

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.conditionalBranchInstructionFields = condiFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, SHRIKE_BT_CONDITIONAL_BRANCH);
    _operandStack->pop();
    _operandStack->pop();
}

/**
 * Since Java Byte Code does not have a full set of operators like OMR
 * Comparison instruction will be mapped as conditional branch instruction 
 */
void TR_OWLMapper::_mapComparisonInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
    char* type = _getType(opCode);

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
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 3, instrUnion, SHRIKE_BT_CONDITIONAL_BRANCH);
    _operandStack->pop();
    _operandStack->pop();

    instrUnion.constantInstructionFields = const0Fields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_CONSTANT);
    _operandStack->push(node);
  
    instrUnion.gotoInstructionFields = gotoFields;
    _createOWLInstruction(true,node->getGlobalIndex(),0,BY_VALUE,2,instrUnion,SHRIKE_BT_GOTO);
   
    instrUnion.constantInstructionFields = const1Fields;
    _createOWLInstruction(true,node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_CONSTANT);
    _operandStack->push(node);
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
        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_CONVERSION);
        _operandStack->pop();
        _operandStack->push(node);
        
    }
    else{ //for those unecessary conversion instructions. They still need to occupy a slot in the instruciton list for the offset adjustment
        ShrikeBTInstructionFieldsUnion instrUnion;
        _createOWLInstruction(false, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion, NOT_INSTRUCTION);
    }

}

void TR_OWLMapper::_mapDirectCallInstruction(TR::Node *node) {
    
    TR::MethodSymbol *methodSymbol = node->getSymbolReference()->getSymbol()->getMethodSymbol();
    TR::Method * method = methodSymbol->getMethod();

    TR::MethodSymbol::Kinds methodKind = methodSymbol->getMethodKind();

    char* className = method->classNameChars();
    char* methodName = method->nameChars();
    char* type = method->signatureChars();

    InvokeInstructionFields invokeFields;

    if (method->isConstructor()){ // if the call is constructor
        TR::Node *newNode = node->getChild(0);
        TR::Node *loadAddr = newNode->getChild(0);
 
        int32_t len;
        strcpy(invokeFields.type, type);
        strcpy(invokeFields.className, loadAddr->getSymbolReference()->getTypeSignature(len));
        strcpy(invokeFields.methodName, methodName);
        invokeFields.disp = SPECIAL;

        for (int32_t i = 0 ; i < node->getNumArguments() ; i ++){
            _operandStack->pop();
        }
    }
    else if (methodKind == methodSymbol->Static) { // static method call
        strcpy(invokeFields.type, type);
        strcpy(invokeFields.className, className);
        strcpy(invokeFields.methodName, methodName);
        invokeFields.disp = STATIC;

        for (int32_t i = 0 ; i < node->getNumArguments() ; i ++){
            _operandStack->pop();
        }
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_INVOKE); 
    if (strcmp(TYPE_void, _getType(node->getOpCode())) != 0) {
        _operandStack->push(node);
    }
}

void TR_OWLMapper::_mapIndirectCallInstruction(TR::Node* node) {

    TR::MethodSymbol *methodSymbol = node->getSymbolReference()->getSymbol()->getMethodSymbol();
    TR::Method * method = methodSymbol->getMethod();

    TR::MethodSymbol::Kinds methodKind = methodSymbol->getMethodKind();

    char* className = method->classNameChars();
    char* methodName = method->nameChars();
    char* type = method->signatureChars();

    InvokeInstructionFields invokeFields;
    strcpy(invokeFields.type, type);
    strcpy(invokeFields.className, className);
    strcpy(invokeFields.methodName, methodName);

    if (methodKind == methodSymbol->Virtual) {

        invokeFields.disp = VIRTUAL;

        for (int32_t i = 0 ; i < node->getNumArguments() ; i ++){
            _operandStack->pop();
        }
    }
    else if (methodKind == methodSymbol->Static) {
        invokeFields.disp = STATIC;

        for (int32_t i = 0 ; i < node->getNumArguments() ; i ++){
            _operandStack->pop();
        }
    }
    else if (methodKind == methodSymbol->Interface) {
        invokeFields.disp = INTERFACE;

        for (int32_t i = 0 ; i < node->getNumArguments() ; i ++){
            _operandStack->pop();
        }
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SHRIKE_BT_INVOKE); 

    if (strcmp(TYPE_void, _getType(node->getOpCode())) != 0) {
        _operandStack->push(node);
    }
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
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST,0,instrUnion, SHRIKE_BT_CONSTANT);
    _operandStack->push(node);

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 4, instrUnion, SHRIKE_BT_CONDITIONAL_BRANCH);
    _operandStack->pop();

    _createSwapInstruction(node);
    _operandStack->swap();

    _createPopInstruction(node,1);
    _operandStack->pop();

    instrUnion.gotoInstructionFields = gotoFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 2, instrUnion, SHRIKE_BT_GOTO );

   _createPopInstruction(node,1);
   _operandStack->pop();
}

void TR_OWLMapper::_mapIndirectStoreInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode());
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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_PUT);
        _operandStack->pop();
        _operandStack->pop();
    }
    else { // array
        ArrayStoreInstructionFields arrayStoreFields;
        strcpy(arrayStoreFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayStoreInstructionFields = arrayStoreFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_ARRAY_STORE);
        _operandStack->pop();
        _operandStack->pop();
        _operandStack->pop();
    }
    
}

void TR_OWLMapper::_mapIndirectLoadInstruction(TR::Node *node) {
    char* type = _getType(node->getOpCode());
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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_GET);
        _operandStack->pop();
        _operandStack->push(node);
    }
    else{ //array
        ArrayLoadInstructionFields arrayLoadFields;
        strcpy(arrayLoadFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayLoadInstructionFields = arrayLoadFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_ARRAY_LOAD);
        _operandStack->pop();
        _operandStack->pop();
        _operandStack->push(node);
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
        char* type = _getType(node->getChild(1)->getOpCode());

        strcpy(newFields.type, "[");
        strcat(newFields.type, type);
        newFields.arrayBoundsCount = 1;
        _operandStack->pop();
    }
    else if (opCodeValue == TR::anewarray ) { // new reference array
        int32_t len;
        TR::Node *loadAddr = node->getChild(1);
        const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);
        
        strcpy(newFields.type, "[");
        strcat(newFields.type, type);
        newFields.arrayBoundsCount = 1;
        _operandStack->pop();
    }
    else if (opCodeValue == TR::multianewarray) { // new multidimentianal array
        TR::Node* loadAddr = node->getLastChild();
        int32_t len;
        const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);
        strcpy(newFields.type, type);
        newFields.arrayBoundsCount = node->getFirstChild()->getInt();
        for (int32_t i = 0 ; i < node->getNumArguments()-2; i ++){
            _operandStack->pop();
        }

    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.newInstructionFields = newFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_ARRAY_NEW); 

    _operandStack->push(node);
}

void TR_OWLMapper::_mapInstanceofInstruction(TR::Node *node) {
    InstanceofInstructionFields instanceofFields;

    TR::Node *loadAddr = node->getChild(1);

    int32_t len;
    const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);

    strcpy(instanceofFields.type, type);
    
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.instanceofInstructionFields = instanceofFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_INSTANCE_OF);
    _operandStack->pop();
}

void TR_OWLMapper::_mapArrayLengthInstruction(TR::Node *node) {
    ArrayLengthInstructionFields arrayLenFields;

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayLengthInstructionFields = arrayLenFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_ARRAY_LENGTH);
    _operandStack->pop();
    _operandStack->push(node);
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

    _createOWLInstruction(true,node->getGlobalIndex(),0,TABLE_MAP,0,instrUnion,SHRIKE_BT_SWITCH);
    _operandStack->pop();
}

void TR_OWLMapper::_mapWriteBarrierInstruction(TR::Node* node) {
    char *type = _getType(node->getOpCode());
    ArrayStoreInstructionFields arrayStoreFields;
    strcpy(arrayStoreFields.type, type);

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayStoreInstructionFields = arrayStoreFields;

    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHRIKE_BT_ARRAY_STORE);
    _operandStack->pop();
    _operandStack->pop();
    _operandStack->pop();
}