//
// Created by Cijie Xia on 2019-09-23.
//

#include <string.h>
#include <stdlib.h>
#include "il/ILHelpers.hpp"
#include "optimizer/OWLMapper.hpp"
#include "optimizer/OWLJNIConfig.hpp"



/****** public ******/

void TR_OWLMapper::map(TR::Compilation* compilation) {

    TR::NodeChecklist instructionMappingVisited(compilation); // visited nodes for mapping the instruction

    printf("++++ process the tree ++++\n");
    for (TR::TreeTop *tt = compilation->getStartTree(); tt ; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        _processTree(node, instructionMappingVisited);
        //printf("==================\n");
    }
    printf("++++ adjust the branch target label ++++\n");

    _adjustOffset();

    printf("++++ log WALA instructions ++++\n");

    _logAllMappedInstructions();
}

TR_OWLMapper::TR_OWLMapper() {

    _con = new TR_OWLInstructionConstructor();
}

TR_OWLMapper::~TR_OWLMapper() {
    delete _con;
}

/****** private ******/
void TR_OWLMapper::_processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;

    visited.add(root);

    for (uint32_t i = 0; i < root->getNumChildren(); ++i) {
        _processTree(root->getChild(i), visited);
    }

    _instructionRouter(root);
}

void TR_OWLMapper::_adjustOffset() {
    /**** build OMR index to WALA offset table ****/
    std::unordered_map<uint32_t,uint32_t> OMR_indexTo_WALA_offsetTable;
    uint32_t offset = 0;
    for (uint32_t i = 0 ; i < _instructionInfoList.size(); i ++) {

        InstructionInfo instrInfo = _instructionInfoList[i];
        bool is_WALA_instruction = instrInfo.is_WALA_instruction;

        OMR_indexTo_WALA_offsetTable[instrInfo.OMR_GlobalIndex] = offset;
        if (is_WALA_instruction) {
            offset ++;
        }
    }
    
    offset = 0;
    /**** Adjust branch target label and instruction offsets ****/
    for (uint32_t i = 0 ; i < _instructionInfoList.size(); i ++) {
        InstructionInfo* instrInfo = &_instructionInfoList[i];
        bool is_WALA_instruction = instrInfo->is_WALA_instruction;
        WALA_Instruction instruction = instrInfo->instruction;
        
        if (is_WALA_instruction) {
            BranchTargetLabelAdjustType adjustType = instrInfo->branchTargetLabelAdjustType;
            WALA_InstructionFieldsUnion *fieldsUnion = &instrInfo->instructionFieldsUnion;
            uint32_t label;
            if (instruction == GOTO) {

                GotoInstructionFields *gotoFields = &fieldsUnion->gotoInstructionFields;

                switch (adjustType)
                {
                    case TABLE_MAP:
                        label = OMR_indexTo_WALA_offsetTable[gotoFields->label]; 
                        break;
                    case ADD_2:
                        label = offset + 2;
                        break;
                    case ADD_3:
                        label = offset + 3;
                        break;
                    default:
                        perror("No adjust type matched!\n");
                        exit(1);
                }

                gotoFields->label = label;
               
            }
            else if (instruction == CONDITIONAL_BRANCH) {
                ConditionalBranchInstructionFields *condiFields = &fieldsUnion->conditionalBranchInstructionFields;
                switch (adjustType)
                {
                    case TABLE_MAP:
                        label = OMR_indexTo_WALA_offsetTable[condiFields->label]; 
                        break;
                    case ADD_2:
                        label = offset + 2;
                        break;
                    case ADD_3:
                        label = offset + 3;
                        break;
                    default:
                        perror("No adjust type matched!\n");
                        exit(1);
                }

                condiFields->label = label;
            }
            
            instrInfo->WALA_offset = offset;
            offset ++;
        }
    }

}

void TR_OWLMapper::_logAllMappedInstructions() {

    for (uint32_t i = 0 ; i < _instructionInfoList.size(); i ++ ) {
        
        InstructionInfo instrInfo = _instructionInfoList[i];
        bool is_WALA_instruciton = instrInfo.is_WALA_instruction;
        if (is_WALA_instruciton){

            WALA_Instruction instruction = instrInfo.instruction;
            WALA_InstructionFieldsUnion instrUnion = instrInfo.instructionFieldsUnion;
            uint32_t offset = instrInfo.WALA_offset;

            jobject instructionObj;

            switch(instruction){
                case CONSTANT: {
                    ConstantInstructionFields constFields = instrUnion.constantInstructionFields;
                    instructionObj = _con->ConstantInstruction(constFields.type, constFields.value);
                    break;
                }
                case STORE: {
                    StoreInstructionFields storeFields = instrUnion.storeInstructionFields;
                    instructionObj = _con->StoreInstruction(storeFields.type, storeFields.symbolReference);
                    break;
                }
                case LOAD: {
                     LoadInstructionFields loadFields = instrUnion.loadInstructionFields;
                    instructionObj = _con->LoadInstruction(loadFields.type,loadFields.symbolReference);
                    break;
                }
                case BINARY_OP: {
                    BinaryOpInstructionFields binaryFields = instrUnion.binaryOpInstructionFields;
                    instructionObj = _con->BinaryOpInstruction(binaryFields.type,binaryFields.op);
                    break;
                }
                case UNARY_OP: {
                    UnaryOpInstructionFields unaryFields = instrUnion.unaryOpInstructionFields;
                    instructionObj = _con->UnaryOpInstruction(unaryFields.type);
                    break;
                }
                case RETURN: {
                    ReturnInstructionFields returnFields = instrUnion.returnInstructionFields;
                    instructionObj = _con->ReturnInstruction(returnFields.type);
                    break;
                }
                case GOTO: {
                    GotoInstructionFields gotoFields = instrUnion.gotoInstructionFields;
                    instructionObj = _con->GotoInstruction(gotoFields.label);
                    break;
                }
                case CONDITIONAL_BRANCH: {
                    ConditionalBranchInstructionFields condiFields = instrUnion.conditionalBranchInstructionFields;
                    instructionObj = _con->ConditionalBranchInstruction(condiFields.type,condiFields.op,condiFields.label);
                    break;
                }
                case COMPARISON: {
                    ComparisonInstructionFields compaFields = instrUnion.comparisonInstructionFields;
                    instructionObj = _con->ComparisonInstruction(compaFields.type,compaFields.op);
                    break;
                }
                case CONVERSION: {
                    ConversionInstructionFields conversionFields = instrUnion.conversionInstructionFields;
                    instructionObj = _con->ConversionInstruction(conversionFields.fromType,conversionFields.toType);
                    break;
                }
                case INVOKE: {
                    InvokeInstructionFields invokeFields = instrUnion.invokeInstructionFields;
                    instructionObj = _con->InvokeInstruction(invokeFields.type,invokeFields.className,invokeFields.methodName,invokeFields.disp);
                    break;
                }
                    
                default:
                    perror("No instruction matched inside logging instructions function!\n");
                    exit(1);
                    
            }

            _logSingleInstruction(instructionObj,instruction,offset);
        }
        
    }
}

void TR_OWLMapper::_logSingleInstruction(jobject instructionObj, WALA_Instruction instruction, uint32_t offset) {
    char* str = _con->getInstructionString(instructionObj, instruction);
    printf("%d: %s\n",offset, str);
    free(str);
}


/****** instruction router, starting point ********/
void TR_OWLMapper::_instructionRouter(TR::Node *node) {
    TR::ILOpCodes opCodeValue = node->getOpCodeValue();

    /*** deal with the nodes that are tree top or BBStart, BBEnd ***/
    if ( opCodeValue == TR::treetop || opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd){
        InstructionInfo instructionInfo;
        instructionInfo.is_WALA_instruction = false;
        instructionInfo.OMR_GlobalIndex = node->getGlobalIndex();
        _instructionInfoList.push_back(instructionInfo);
        return;
    }

    printf("%d: %s\n ", node->getGlobalIndex(),node->getOpCode().getName());
    // printf("%u| ",node->getByteCodeIndex());

     TR::ILOpCode opCode = node->getOpCode();

     if (opCode.isLoadConst()) { //constant instruction
         _mapConstantInstruction(node);
     }
     else if (opCode.isStore()) { // store instruction
         _mapStoreInstruction(node);
     }
     else if (opCode.isLoad()) { // load instruction
         _mapLoadInstruction(node);
     }
     else if (opCode.isReturn()) { // return instruction
         _mapReturnInstruction(node);
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
     else if (opCode.isCall()) { // function call
        _mapInvokeInstruction(node);
     }
     else{ // for those have not been mapped
         printf("XXXXX");
         //_logInstruction(node->getOpCode().getName(), 0);
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

    perror("No type matched!\n");
    exit(1);
}

void TR_OWLMapper::_mapConstantInstruction(TR::Node *node) {

    ConstantInstructionFields constFields;

    TR::ILOpCode opCode = node->getOpCode();
    if (opCode.isInt()){
        constFields = {TYPE_int, _con->Integer(node->getInt())};
    }
    else if (opCode.isFloat()){
        constFields = {TYPE_float, _con->Float(node->getFloat())};
    }
    else if (opCode.isDouble()){
        constFields = {TYPE_double, _con->Double(node->getDouble())};
    }
    else if (opCode.isShort()){
        constFields = {TYPE_int, _con->Integer(node->getShortInt())};
    }
    else if (opCode.isLong()){
        constFields = {TYPE_long, _con->Long(node->getLongInt())};
    }
    else if (opCode.isByte()){
        constFields = {TYPE_int, _con->Integer(node->getByte())};
    }
    else{
        perror("No Constant type matched!\n");
        exit(1);
    }

    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.constantInstructionFields = constFields;
    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        CONSTANT
    };

    _instructionInfoList.push_back(instrInfo);

}

void TR_OWLMapper::_mapStoreInstruction(TR::Node *node) {
  
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    StoreInstructionFields storeFields = {type,node->getSymbolReference()};
    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.storeInstructionFields = storeFields;
    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        STORE
    };

    _instructionInfoList.push_back(instrInfo);

}

void TR_OWLMapper::_mapLoadInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    LoadInstructionFields loadFields = {type, node->getSymbolReference()};

    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.loadInstructionFields = loadFields;

    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        LOAD
    };

    _instructionInfoList.push_back(instrInfo);
}

void TR_OWLMapper::_mapReturnInstruction(TR::Node *node) {
    
    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    
    ReturnInstructionFields returnFields = {type};

    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.returnInstructionFields = returnFields;

    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        RETURN
    };

    _instructionInfoList.push_back(instrInfo);
}

void TR_OWLMapper::_mapArithmeticInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);
    WALA_Operator op;
    
    InstructionInfo instrInfo;

    /**** Unary op ****/
    if (opCode.isNeg()){

        UnaryOpInstructionFields unaryFields = {type};
        WALA_InstructionFieldsUnion instrUnion;
        instrUnion.unaryOpInstructionFields = unaryFields;

        instrInfo = {
            true,
            node->getGlobalIndex(),
            0,
            NO_ADJUST,
            instrUnion,
            UNARY_OP
        };

    }
    /**** Binary op ****/
    else{
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
        BinaryOpInstructionFields binaryFields = {type,op};
        WALA_InstructionFieldsUnion instrUnion;
        instrUnion.binaryOpInstructionFields = binaryFields;

        instrInfo = {
            true,
            node->getGlobalIndex(),
            0,
            NO_ADJUST,
            instrUnion,
            BINARY_OP
        };

    }

    _instructionInfoList.push_back(instrInfo);

}

void TR_OWLMapper::_mapGotoInstruction(TR::Node *node) {
    
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    GotoInstructionFields gotoFields = {index};
    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.gotoInstructionFields = gotoFields;

    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        TABLE_MAP,
        instrUnion,
        GOTO
    };

    _instructionInfoList.push_back(instrInfo);
}

void TR_OWLMapper::_mapConditionalBranchInstruction(TR::Node *node) {

    TR::ILOpCodes opCodeValue = node->getOpCodeValue();
    WALA_Operator op;

    if (TR::ILOpCode::isStrictlyLessThanCmp(opCodeValue)) op = LT;
    else if (TR::ILOpCode::isStrictlyGreaterThanCmp(opCodeValue)) op = GT;
    else if (TR::ILOpCode::isLessCmp(opCodeValue)) op = LE;
    else if (TR::ILOpCode::isGreaterCmp(opCodeValue)) op = GE;
    else if (TR::ILOpCode::isEqualCmp(opCodeValue)) op = EQ;
    else op = NE;

    TR::ILOpCode childOpCode = node->getFirstChild()->getOpCode();
    char* type = _getType(childOpCode);
    uint32_t index = node->getBranchDestination()->getNode()->getGlobalIndex();

    ConditionalBranchInstructionFields condiFields = {type,op,index};

    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.conditionalBranchInstructionFields = condiFields;

    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        TABLE_MAP,
        instrUnion,
        CONDITIONAL_BRANCH
    };
    
    _instructionInfoList.push_back(instrInfo);
}

/***
 * Since WALA comparison instruction does not have a full set of operators like OMR
 * We use conditional branch and create two const int 1 and const int 2
 * goto const int 1 if the condition is true, otherwise goto const int 2
 */

void TR_OWLMapper::_mapComparisonInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();
    char* type = _getType(opCode);

    TR_ComparisonTypes cmpType = TR::ILOpCode::getCompareType(opCodeValue);
    WALA_Operator op;

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

    uint32_t index = node->getGlobalIndex();

    ConditionalBranchInstructionFields condiFields = {type,op,index};
    ConstantInstructionFields const0Fields = {TYPE_int, _con->Integer(0)};
    GotoInstructionFields gotoFields = {index};
    ConstantInstructionFields const1Fields = {TYPE_int, _con->Integer(1)};

    WALA_InstructionFieldsUnion instrUnion;
    InstructionInfo instrInfo;
    instrUnion.conditionalBranchInstructionFields = condiFields;
    instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        ADD_3,
        instrUnion,
        CONDITIONAL_BRANCH
    };

    _instructionInfoList.push_back(instrInfo);

    instrUnion.constantInstructionFields = const0Fields;
    instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        CONSTANT
    };

    _instructionInfoList.push_back(instrInfo);

    instrUnion.gotoInstructionFields = gotoFields;
    instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        ADD_2,
        instrUnion,
        GOTO
    };

    _instructionInfoList.push_back(instrInfo);

    instrUnion.constantInstructionFields = const1Fields;
    instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        CONSTANT
    };

    _instructionInfoList.push_back(instrInfo);
    
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

    InstructionInfo instrInfo;

    if (fromType && toType){
        ConversionInstructionFields conversionFields = {fromType,toType};
        WALA_InstructionFieldsUnion instrUnion;
        instrUnion.conversionInstructionFields = conversionFields;
        instrInfo = {
            true,
            node->getGlobalIndex(),
            0,
            NO_ADJUST,
            instrUnion,
            CONVERSION
        };

        
    }
    else{ //for those unecessary conversion instructions. They still need to occupy a slot in the instruciton list for the offset adjustment
        
        instrInfo.is_WALA_instruction = false;
        instrInfo.OMR_GlobalIndex = node->getGlobalIndex();
    }

    _instructionInfoList.push_back(instrInfo);
}

void TR_OWLMapper::_mapInvokeInstruction(TR::Node *node) {

    TR::ILOpCode opCode = node->getOpCode();
    char* type = _getType(opCode);

    InvokeInstructionFields invokeFields = {type, "null", "null", STATIC};
    
    WALA_InstructionFieldsUnion instrUnion;
    instrUnion.invokeInstructionFields = invokeFields;

    InstructionInfo instrInfo = {
        true,
        node->getGlobalIndex(),
        0,
        NO_ADJUST,
        instrUnion,
        INVOKE
    };

    _instructionInfoList.push_back(instrInfo);
}
