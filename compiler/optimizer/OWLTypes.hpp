#ifndef OWL_TYPES
#define OWL_TYPES

#include <stdint.h>

#define BUFFER_SIZE 32
#define LARGE_BUFFER_SIZE 512
/* Types */
extern char* const TYPE_short;
extern char* const TYPE_long;
extern char* const TYPE_int;
extern char* const TYPE_float;
extern char* const TYPE_double;
extern char* const TYPE_char;
extern char* const TYPE_byte;
extern char* const TYPE_void;
extern char* const TYPE_boolean;
extern char* const TYPE_Object;

typedef enum ShrikeBTInstruction{
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
    POP,
    ARRAY_STORE,
    ARRAY_LOAD,
    NEW,
    PUT,
    GET,
    DUP,
    INSTANCE_OF,
    ARRAY_LENGTH

} ShrikeBTInstruction;

typedef enum ShrikeBTOperator {
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
} ShrikeBTOperator;

typedef enum ShrikeBTDispatch{
    VIRTUAL,
    SPECIAL,
    INTERFACE,
    STATIC 
} ShrikeBTDispatch;


typedef struct ConstantInstructionFields {
    char type[BUFFER_SIZE];
    union {
        int32_t i;
        int64_t l;
        float f;
        double d;
    } value;
} ConstantInstructionFields;

typedef struct StoreInstructionFields {
    char type[BUFFER_SIZE];
    int32_t referenceNumber;
} StoreInstructionFields;

/**
 * For those nodes whose reference count > 1
 * store the value into local variable table
 * value will be used later by other nodes
 */
typedef struct ImplicitStoreInstructionFields {
    char type[BUFFER_SIZE];
    uint32_t omrGlobalIndex;
} ImplicitStoreInstructionFields;

typedef struct LoadInstructionFields {
    char type[BUFFER_SIZE];
    int32_t referenceNumber;
} LoadInstructionFields;

/**
 * Load the value stored by implicit store from local variable table
 */
typedef struct ImplicitLoadInstructionFields {
    char type[BUFFER_SIZE];
    uint32_t omrGloablIndex;
} ImplicitLoadInstructionFields;

typedef struct BinaryOpInstructionFields {
    char type[BUFFER_SIZE];
    ShrikeBTOperator op;
} BinaryOpInstructionFields;

typedef struct ReturnInstructionFields {
    char type[BUFFER_SIZE];
} ReturnInstructionFields;

typedef struct GotoInstructionFields {
    uint32_t label;
} GotoInstructionFields;

typedef struct ConditionalBranchInstructionFields {
    char type[BUFFER_SIZE];
    ShrikeBTOperator op;
    uint32_t label;
} ConditionalBranchInstructionFields;

typedef struct ComparisonInstructionFields {
    char type[BUFFER_SIZE];
    ShrikeBTOperator op;
} ComparisonInstructionFields;

typedef struct ConversionInstructionFields {
    char fromType[BUFFER_SIZE];
    char toType[BUFFER_SIZE];
} ConversionInstructionFields;

typedef struct UnaryOpInstructionFields {
    char type[BUFFER_SIZE];
} UnaryOpInstructionFields;

typedef struct InvokeInstructionFields {
    char type[LARGE_BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE]; 
    char methodName[LARGE_BUFFER_SIZE];
    ShrikeBTDispatch disp;
} InvokeInstructionFields;

typedef struct SwapInstructionFields {} SwapInstructionFields;

typedef struct PopInstructionFields {
    uint16_t size; // can only be 1 or 2. Other sizes will cause exception in shrikeBT
} PopInstructionFields;

typedef struct DupInstructionFields {
    uint16_t delta;
} DupInstructionFields;
typedef struct ArrayStoreInstructionFields {
    char type[BUFFER_SIZE];
} ArrayStoreInstructionFields;

typedef struct ArrayLoadInstructionFields {
    char type[BUFFER_SIZE];
} ArrayLoadInstructionFields;

typedef struct NewInstructionFields {
    char type[LARGE_BUFFER_SIZE];
    int32_t arrayBoundsCount;
} NewInstructionFields;

typedef struct PutInstructionFields {
    char type[BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE];
    char fieldName[LARGE_BUFFER_SIZE];
    bool isStatic;
} PutInstructionFields;

typedef struct GetInstructionFields {
    char type[BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE];
    char fieldName[LARGE_BUFFER_SIZE];
    bool isStatic;
} GetInstructionFields;

typedef struct InstanceofInstructionFields {
    char type[LARGE_BUFFER_SIZE];
} InstanceofInstructionFields;

typedef struct ArrayLengthInstructionFields {} ArrayLengthInstructionFields;
typedef union ShrikeBTInstructionFieldsUnion {
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
    DupInstructionFields dupInstructionFields;
    ArrayStoreInstructionFields arrayStoreInstructionFields;
    ArrayLoadInstructionFields arrayLoadInstructionFields;
    NewInstructionFields newInstructionFields;
    PutInstructionFields putInstructionFields;
    GetInstructionFields getInstructionFields;
    InstanceofInstructionFields instanceofInstructionFields;
    ArrayLengthInstructionFields arrayLengthInstructionFields;
} ShrikeBTInstructionFieldsUnion;


/*** indicates how the offset of branch instruction should be adjusted ***/
typedef enum BranchTargetLabelAdjustType {
    NO_ADJUST, // no need to adjust the branch target
    TABLE_MAP, // get the target label from the omr index to shrikeBT offset mapping table
    BY_VALUE, // inc or dec the offset by certain amount
} BranchTargetLabelAdjustType;


/*** This structure contains all necessary information to do the OMR to ShrikeBT mapping ***/
typedef struct OWLInstruction {
    /*** Required ***/
    bool isShrikeBTInstruction; //if false, this indicates it can be an OMR treetop, BBStart, BBEnd or an eliminated OMR instruction
    uint32_t omrGlobalIndex;

    uint32_t shrikeBTOffset; // should be set to 0 initially and let the adjust function assign the correct offset
    BranchTargetLabelAdjustType branchTargetLabelAdjustType;
    int32_t branchTargetLabelAdjustAmount; // should be set to 0 if not using BY_VALUE adjust type
    ShrikeBTInstructionFieldsUnion instructionFieldsUnion;
    ShrikeBTInstruction instruction;
} OWLInstruction;

#endif