#ifndef OWL_MAPPER_TYPES
#define OWL_MAPPER_TYPES

#include "il/OMRSymbolReference.hpp"
#include "launch.h"

enum ShrikeBTInstruction {
    NOT_INSTRUCTION, // indicates this is not a shrikeBT instruction

    CONSTANT,
    STORE,
    IMPLICIT_STORE, // shrikeBT does not really have this instruction. Should be constructed as STORE in the end
    LOAD,
    IMPLICIT_LOAD, // shrikeBT does not really have this instruction. Should be constructed as LOAD in the end
    BINARY_OP,
    UNARY_OP,
    RETURN,
    GOTO,
    CONDITIONAL_BRANCH,
    COMPARISON,
    CONVERSION,
    INVOKE,
    SWAP,
    POP

};

enum ShrikeBTOperator {
    //binary op
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    AND,
    OR,
    XOR,
    //unary op
    NEG,
    //conditional branch op
    EQ,
    NE,
    LT,
    GE,
    GT,
    LE,
    //comparison op
    CMP,
    CMPL,
    CMPG
};

enum ShrikeBTDispatch {
    VIRTUAL,
    SPECIAL,
    INTERFACE,
    STATIC 
};

struct ConstantInstructionFields {
    char* type;
    jobject value;
};

struct StoreInstructionFields {
    char* type;
    TR::SymbolReference * symbolReference;
};

/**
 * For those nodes whose reference count > 1
 * store the value into local variable table
 * value will be used later by other nodes
 **/
struct ImplicitStoreInstructionFields {
    char* type;
    uint32_t omrGlobalIndex;
};

struct LoadInstructionFields {
    char* type;
    TR::SymbolReference * symbolReference;
};

/**
 * Load the value stored by implicit store from local variable table
 * */
struct ImplicitLoadInstructionFields {
    char* type;
    uint32_t omrGloablIndex;
};

struct BinaryOpInstructionFields {
    char* type;
    ShrikeBTOperator op;
};

struct ReturnInstructionFields {
    char* type;
};

struct GotoInstructionFields {
    uint32_t label;
};

struct ConditionalBranchInstructionFields {
    char* type;
    ShrikeBTOperator op;
    uint32_t label;
};

struct ComparisonInstructionFields {
    char* type;
    ShrikeBTOperator op;
};

struct ConversionInstructionFields {
    char* fromType;
    char* toType;
};

struct UnaryOpInstructionFields {
    char* type;
};

struct InvokeInstructionFields {
    char* type;
    char* className; 
    char* methodName;
    ShrikeBTDispatch disp;
};

struct SwapInstructionFields {};

struct PopInstructionFields {
    uint16_t size; // can only be 1 or 2. Other sizes will cause exception in shrikeBT
};

union ShrikeBTInstructionFieldsUnion {
    ConstantInstructionFields constantInstructionFields;
    StoreInstructionFields storeInstructionFields;
    ImplicitStoreInstructionFields implicitStoreInstructionFields;
    LoadInstructionFields loadInstructionFields;
    ImplicitLoadInstructionFields implicitLoadInstructionFields;
    BinaryOpInstructionFields binaryOpInstructionFields;
    ReturnInstructionFields returnInstructionFields;
    GotoInstructionFields gotoInstructionFields;
    ConditionalBranchInstructionFields conditionalBranchInstructionFields;
    ComparisonInstructionFields comparisonInstructionFields;
    ConversionInstructionFields conversionInstructionFields;
    UnaryOpInstructionFields unaryOpInstructionFields;
    InvokeInstructionFields invokeInstructionFields;
    SwapInstructionFields swapInstructionFields;
    PopInstructionFields popInstructionFields;
};

/*** indicates how the offset of branch instruction should be adjusted ***/
enum BranchTargetLabelAdjustType {
    NO_ADJUST, // no need to adjust the branch target
    TABLE_MAP, // get the target label from the omr index to shrikeBT offset mapping table
    BY_VALUE, // inc or dec the offset by certain amount
};

struct InstructionMeta {
    /*** if false, this indicates it can be an OMR treetop, BBStart, BBEnd or an eliminated OMR instruction ***/
    bool isShrikeBTInstruction;
    uint32_t omrGlobalIndex;
    
    uint32_t shrikeBTOffset; // should be set to 0 initially and let the adjust function assign the correct offset
    BranchTargetLabelAdjustType branchTargetLabelAdjustType;
    int32_t branchTargetLabelAdjustAmount; // should be set to 0 if not using BY_VALUE adjust type
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion;
    ShrikeBTInstruction instruction;
};

struct MappedInstructionObject {
    ShrikeBTInstruction instruction;
    jobject instructionObject;
    uint32_t offset;
};

#endif