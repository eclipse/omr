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


/****** public ******/
TR_OWLMapper::TR_OWLMapper(TR::Compilation* compilation) {
    _compilation = compilation;
    _debug = compilation->getDebug();
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
 * STEP 1: preprocessing the tree
 * STEP 2: map the current node
 * STEP 3: post-processing the tree
 * */
void TR_OWLMapper::_processTree(TR::Node *root, TR::Node *parent, TR::NodeChecklist &visited) {
        
    TR::ILOpCode opCode = root->getOpCode();
    TR::ILOpCodes opCodeValue = root->getOpCodeValue();
    TR::SymbolReference *symRef = root->getSymbolReference();

    /*** 1. Preprocessing ***/
    if (visited.contains(root) && !opCode.isLoadDirect() ){ // indicates this node has already been evaluated => load the value from local var table

        if (opCode.isNew()) {
            if (parent && !parent->getOpCode().isStoreDirect()){
                _createDupInstruction(root, 0);
            }
            return;
        }

        _createImplicitLoad(root);
        
        return;
    }

    visited.add(root);

    if (opCode.isTernary()){ // deal with ternary expression. Evaluate last two children first

        _processTree(root->getChild(1), root, visited);
        _processTree(root->getChild(2), root, visited);
        _processTree(root->getChild(0), root, visited);
        
    }
    else if (opCode.isCallIndirect()) { // for call indirect, skip the load reference indirect. 
        _processTree(root->getChild(1), root, visited); // only evaluate aload 
    }
    else if (opCodeValue == TR::newarray) { // skip the second child of new array (it indicates the type)
        _processTree(root->getChild(0), root, visited);
    }
    else if ( !opCode.isWrtBar() && opCode.isStoreIndirect() && symRef->getSymbol()->isArrayShadowSymbol()) { // if array store, only evaluate the node contains the corresponding index and the value to be stored
        _processTree(root->getChild(0)->getChild(0), root, visited); // ref
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited); //index
        _processTree(root->getChild(1), root, visited); //value
    }
    else if (opCode.isLoadIndirect() && symRef->getSymbol()->isArrayShadowSymbol()) {
        _processTree(root->getChild(0)->getChild(0), root, visited);
        _processTree(root->getChild(0)->getChild(1)->getChild(0)->getChild(0)->getChild(0), root, visited);
    }
    else if (opCode.isSwitch()){
        _processTree(root->getChild(0), root, visited);
    }
    else if (opCode.isWrtBar() && symRef->getSymbol()->isArrayShadowSymbol() ){
        return;
    }
    else if (opCode.isBndCheck() ) { // skip TR::BNDCheck
        return;
    }
    else if (opCode.isAnchor()) { // skip TR::compressedRefs
        return;
    }
    else { 
        for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
            _processTree(root->getChild(i), root, visited);
        }
    }

    /*** 2. processing the current node ***/

    _instructionRouter(root); // routed to corresponding mapping

    /*** 3. post-processsing ***/
    if (opCode.isLoadDirect()){ // no need to create implicit load or store for a load instruction
        return;
    }

    if (opCode.isNew()) {  // if it is new instruction, do not create implicit load or store, dup should be created later
        return;
    }

    // for those node reference count > 1, create implicit load or store 
    if (parent == NULL) { // if this node has no parent, just create an implicit store. 
        if (root->getReferenceCount() > 1){
            _createImplicitStore(root);
        }
    }
    else {

        if (root->getReferenceCount() > 1 && (parent->getOpCodeValue() == TR::treetop || parent->getOpCodeValue() == TR::BBStart || parent->getOpCode().isCheck()) ) {
            _createImplicitStore(root);
        }
        else if (root->getReferenceCount()> 1){
            _createImplicitStore(root);
            _createImplicitLoad(root);
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

            if (instruction == GOTO) {

                GotoInstructionFields *gotoFields = &instrUnion->gotoInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[gotoFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + owlInstr->branchTargetLabelAdjustAmount;
                }

                gotoFields->label = label;
               
            }
            else if (instruction == CONDITIONAL_BRANCH) {

                ConditionalBranchInstructionFields *condiFields = &instrUnion->conditionalBranchInstructionFields;

                if (adjustType == TABLE_MAP) {
                    label = offsetMappingTable[condiFields->label];
                }
                else if (adjustType == BY_VALUE) {
                    label = offset + owlInstr->branchTargetLabelAdjustAmount;
                }

                condiFields->label = label;
            }
            else if (instruction == SWITCH) {
                
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
    perror("No type matched!\n");
    exit(1);
}

void TR_OWLMapper::_createImplicitStore(TR::Node *node) {

    char* type = _getType(node->getOpCode());

    uint32_t omrGlobalIndex = node->getGlobalIndex();
    
    ImplicitStoreInstructionFields impStoreFields;
    strcpy(impStoreFields.type, type);
    impStoreFields.omrGlobalIndex = omrGlobalIndex;

    ShrikeBTInstructionFieldsUnion insUnion;

    insUnion.implicitStoreInstructionFields = impStoreFields;
    _createOWLInstruction(true, omrGlobalIndex,0, NO_ADJUST, 0, insUnion, IMPLICIT_STORE);
}

void TR_OWLMapper::_createImplicitLoad(TR::Node* node) {
    char* type = _getType(node->getOpCode());
    uint32_t omrGlobalIndex = node->getGlobalIndex();
    
    ImplicitLoadInstructionFields impLoadFields;
    strcpy(impLoadFields.type, type);
    impLoadFields.omrGloablIndex = omrGlobalIndex;

    ShrikeBTInstructionFieldsUnion insUnion;
    insUnion.implicitLoadInstructionFields = impLoadFields;
    
    _createOWLInstruction(true, omrGlobalIndex, 0, NO_ADJUST, 0, insUnion, IMPLICIT_LOAD);
}

void TR_OWLMapper::_createDupInstruction(TR::Node *node, uint16_t delta) {
    DupInstructionFields dupFields = {delta};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.dupInstructionFields = dupFields;

    _createOWLInstruction(true,node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, DUP);
}

void TR_OWLMapper::_createPopInstruction(TR::Node* node, uint16_t size) {
    PopInstructionFields popFields = {size};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.popInstructionFields = popFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, POP);
}

void TR_OWLMapper::_createSwapInstruction(TR::Node* node) {
    SwapInstructionFields swapFields = {};

    ShrikeBTInstructionFieldsUnion instrUnion;

    instrUnion.swapInstructionFields = swapFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, SWAP);
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
    else{
        perror("No Constant type matched!\n");
        exit(1);
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.constantInstructionFields = constFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, CONSTANT);
}

void TR_OWLMapper::_mapDirectStoreInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    TR::SymbolReference* symRef = node->getSymbolReference();
    TR::Symbol* symbol = symRef->getSymbol();

    if (symbol->isAuto()) { //local var
        StoreInstructionFields storeFields;

        strcpy(storeFields.type, type);
        storeFields.referenceNumber = symRef->getReferenceNumber();

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.storeInstructionFields = storeFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion, STORE);
    }
    else if (symbol->isStatic()) { // static (PUT)

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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,PUT);
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
        loadFields.referenceNumber = symRef->getReferenceNumber();

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.loadInstructionFields = loadFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0, NO_ADJUST,0,instrUnion, LOAD);
    }
    else if (symbol->isStatic()) { // static (GET)
        printf("Static\n");
        GetInstructionFields getFields;

        char staticName[LARGE_BUFFER_SIZE];
        strcpy(staticName, _debug->getStaticName(symRef));
        printf("%s\n",staticName);
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

        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, GET);
    }

}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    
    ReturnInstructionFields returnFields;
    strcpy(returnFields.type, type);

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.returnInstructionFields = returnFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, RETURN);
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
        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, UNARY_OP);
    }
    /*** Shift ***/
    else if (opCode.isShift()) {
        ShiftInstructionFields shiftFields;
        strcpy(shiftFields.type, type);

        if (opCode.isLeftShift()) shiftFields.op = SHL;
        else if (opCode.isRightShift() && !opCode.isShiftLogical()) shiftFields.op = SHR;
        else shiftFields.op = USHR;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.shiftInstructionFields = shiftFields;
        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,SHIFT_);
    }
    /**** Binary op ****/
    else{
        ShrikeBTOperator op;

        if (opCode.isAdd()) op = ADD;
        else if (opCode.isSub()) op = SUB;
        else if (opCode.isMul()) op = MUL;
        else if (opCode.isDiv()) op = DIV;
        else if (opCode.isRem()) op = REM;
        else if (opCode.isAnd()) op = AND;
        else if (opCode.isOr()) op = OR;
        else if (opCode.isXor()) op = XOR;
        else{
            perror("No arithmetic operator matched!\n");
            exit(1);
        }
        BinaryOpInstructionFields binaryFields;
        strcpy(binaryFields.type, type);
        binaryFields.op = op;

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.binaryOpInstructionFields = binaryFields;

        _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, BINARY_OP);
    }
}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    GotoInstructionFields gotoFields = {index};

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.gotoInstructionFields = gotoFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, GOTO);
}

void TR_OWLMapper::_mapConditionalBranchInstruction(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
    ShrikeBTOperator op;

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

    _createOWLInstruction(true, node->getGlobalIndex(), 0, TABLE_MAP, 0, instrUnion, CONDITIONAL_BRANCH);
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
    ShrikeBTOperator op;

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
        perror("No cmp type matched!\n");
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
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 3, instrUnion, CONDITIONAL_BRANCH);

    instrUnion.constantInstructionFields = const0Fields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONSTANT);
  
    instrUnion.gotoInstructionFields = gotoFields;
    _createOWLInstruction(true,node->getGlobalIndex(),0,BY_VALUE,2,instrUnion,GOTO);
   
    instrUnion.constantInstructionFields = const1Fields;
    _createOWLInstruction(true,node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONSTANT);
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
            perror("No type conversion matched!\n");
            exit(1);
    }

    if (fromType && toType){
        ConversionInstructionFields conversionFields;
        strcpy(conversionFields.fromType, fromType);
        strcpy(conversionFields.toType, toType);
        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.conversionInstructionFields = conversionFields;
        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,CONVERSION);
        
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

    }
    else if (methodKind == methodSymbol->Static) { // static method call
        strcpy(invokeFields.type, type);
        strcpy(invokeFields.className, className);
        strcpy(invokeFields.methodName, methodName);
        invokeFields.disp = STATIC;
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, INVOKE); 
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
    }
    else if (methodKind == methodSymbol->Static) {
        invokeFields.disp = STATIC;
    }
    else if (methodKind == methodSymbol->Interface) {
        invokeFields.disp = INTERFACE;
    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST, 0, instrUnion, INVOKE); 
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
    _createOWLInstruction(true, node->getGlobalIndex(), 0, NO_ADJUST,0,instrUnion, CONSTANT);

    instrUnion.conditionalBranchInstructionFields = condiFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 4, instrUnion, CONDITIONAL_BRANCH);

    _createSwapInstruction(node);

    _createPopInstruction(node,1);

    instrUnion.gotoInstructionFields = gotoFields;
    _createOWLInstruction(true, node->getGlobalIndex(), 0, BY_VALUE, 2, instrUnion, GOTO );

   _createPopInstruction(node,1);
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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,PUT);
    }
    else { // array
        ArrayStoreInstructionFields arrayStoreFields;
        strcpy(arrayStoreFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayStoreInstructionFields = arrayStoreFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,ARRAY_STORE);
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

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,GET);
    }
    else{ //array
        ArrayLoadInstructionFields arrayLoadFields;
        strcpy(arrayLoadFields.type, type);

        ShrikeBTInstructionFieldsUnion instrUnion;
        instrUnion.arrayLoadInstructionFields = arrayLoadFields;

        _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,ARRAY_LOAD);
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

    }

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.newInstructionFields = newFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,NEW); 
}

void TR_OWLMapper::_mapInstanceofInstruction(TR::Node *node) {
    InstanceofInstructionFields instanceofFields;

    TR::Node *loadAddr = node->getChild(1);

    int32_t len;
    const char* type = loadAddr->getSymbolReference()->getTypeSignature(len);

    strcpy(instanceofFields.type, type);
    
    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.instanceofInstructionFields = instanceofFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,INSTANCE_OF);
}

void TR_OWLMapper::_mapArrayLengthInstruction(TR::Node *node) {
    ArrayLengthInstructionFields arrayLenFields;

    ShrikeBTInstructionFieldsUnion instrUnion;
    instrUnion.arrayLengthInstructionFields = arrayLenFields;
    _createOWLInstruction(true, node->getGlobalIndex(),0,NO_ADJUST,0,instrUnion,ARRAY_LENGTH);
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

    _createOWLInstruction(true,node->getGlobalIndex(),0,TABLE_MAP,0,instrUnion,SWITCH);
}