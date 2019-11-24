#ifndef OWL_TYPES
#define OWL_TYPES

#include <stdint.h>

#define BUFFER_SIZE 32
#define LARGE_BUFFER_SIZE 256
/* Types */
extern char *const TYPE_short;
extern char *const TYPE_long;
extern char *const TYPE_int;
extern char *const TYPE_float;
extern char *const TYPE_double;
extern char *const TYPE_char;
extern char *const TYPE_byte;
extern char *const TYPE_void;
extern char *const TYPE_boolean;
extern char *const TYPE_Object;

typedef enum ShrikeBTInstruction
{
    NOT_INSTRUCTION, // indicates this is not a shrikeBT instruction

    SHRIKE_BT_CONSTANT,
    SHRIKE_BT_STORE,
    IMPLICIT_STORE, // shrikeBT does not really have this instruction. Should be constructed as SHRIKE_BT_STORE in the end
    SHRIKE_BT_LOAD,
    IMPLICIT_LOAD, // shrikeBT does not really have this instruction. Should be constructed as SHRIKE_BT_LOAD in the end
    SHRIKE_BT_BINARY_OP,
    SHRIKE_BT_UNARY_OP,
    SHRIKE_BT_RETURN,
    SHRIKE_BT_GOTO,
    SHRIKE_BT_CONDITIONAL_BRANCH,
    SHRIKE_BT_COMPARISON,
    SHRIKE_BT_CONVERSION,
    SHRIKE_BT_INVOKE,
    SHRIKE_BT_SWAP,
    SHRIKE_BT_POP,
    SHRIKE_BT_ARRAY_STORE,
    SHRIKE_BT_ARRAY_LOAD,
    SHRIKE_BT_ARRAY_NEW,
    SHRIKE_BT_PUT,
    SHRIKE_BT_GET,
    SHRIKE_BT_DUP,
    SHRIKE_BT_INSTANCE_OF,
    SHRIKE_BT_ARRAY_LENGTH,
    SHRIKE_BT_SHIFT, //name conflict with macro SHIFT
    SHRIKE_BT_SWITCH

} ShrikeBTInstruction;

typedef enum ShrikeBTBinaryOperator
{
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    AND,
    OR,
    XOR
} ShrikeBTBinaryOperator;

typedef enum ShrikeBTUnaryOperator
{
    NEG
} ShrikeBTUnaryOperator;

typedef enum ShrikeBTConditionalBranchOperator
{
    EQ,
    NE,
    LT,
    GE,
    GT,
    LE
} ShrikeBTConditionalBranchOperator;

typedef enum ShrikeBTComparisonOperator
{
    CMP,
    CMPL,
    CMPG
} ShrikeBTComparisonOperator;

typedef enum ShrikeBTShiftOperator
{
    SHL,
    SHR,
    USHR
} ShrikeBTShiftOperator;

typedef enum ShrikeBTDispatch
{
    VIRTUAL,
    SPECIAL,
    INTERFACE,
    STATIC
} ShrikeBTDispatch;

typedef struct MethodInfo
{
    char methodSignature[LARGE_BUFFER_SIZE];
} MethodInfo;

typedef struct ConstantInstructionFields
{
    char type[BUFFER_SIZE];
    union {
        int32_t i;
        int64_t l;
        float f;
        double d;
    } value;
} ConstantInstructionFields;

typedef struct StoreInstructionFields
{
    char type[BUFFER_SIZE];
    int32_t referenceNumber;
} StoreInstructionFields;

/**
 * For those nodes whose reference count > 1
 * store the value into local variable table
 * value will be used later by other nodes
 */
typedef struct ImplicitStoreInstructionFields
{
    char type[BUFFER_SIZE];
    uint32_t omrGlobalIndex;
} ImplicitStoreInstructionFields;

typedef struct LoadInstructionFields
{
    char type[BUFFER_SIZE];
    int32_t referenceNumber;
} LoadInstructionFields;

/**
 * Load the value stored by implicit store from local variable table
 */
typedef struct ImplicitLoadInstructionFields
{
    char type[BUFFER_SIZE];
    uint32_t omrGloablIndex;
} ImplicitLoadInstructionFields;

typedef struct BinaryOpInstructionFields
{
    char type[BUFFER_SIZE];
    ShrikeBTBinaryOperator op;
} BinaryOpInstructionFields;

typedef struct ReturnInstructionFields
{
    char type[BUFFER_SIZE];
} ReturnInstructionFields;

typedef struct GotoInstructionFields
{
    uint32_t label;
} GotoInstructionFields;

typedef struct ConditionalBranchInstructionFields
{
    char type[BUFFER_SIZE];
    ShrikeBTConditionalBranchOperator op;
    uint32_t label;
} ConditionalBranchInstructionFields;

typedef struct ComparisonInstructionFields
{
    char type[BUFFER_SIZE];
    ShrikeBTComparisonOperator op;
} ComparisonInstructionFields;

typedef struct ConversionInstructionFields
{
    char fromType[BUFFER_SIZE];
    char toType[BUFFER_SIZE];
} ConversionInstructionFields;

typedef struct UnaryOpInstructionFields
{
    char type[BUFFER_SIZE];
} UnaryOpInstructionFields;

typedef struct InvokeInstructionFields
{
    char type[LARGE_BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE];
    char methodName[LARGE_BUFFER_SIZE];
    ShrikeBTDispatch disp;
} InvokeInstructionFields;

typedef struct SwapInstructionFields
{
} SwapInstructionFields;

typedef struct PopInstructionFields
{
    uint16_t size; // can only be 1 or 2. Other sizes will cause exception in shrikeBT
} PopInstructionFields;

typedef struct DupInstructionFields
{
    uint16_t delta;
} DupInstructionFields;
typedef struct ArrayStoreInstructionFields
{
    char type[BUFFER_SIZE];
} ArrayStoreInstructionFields;

typedef struct ArrayLoadInstructionFields
{
    char type[BUFFER_SIZE];
} ArrayLoadInstructionFields;

typedef struct NewInstructionFields
{
    char type[LARGE_BUFFER_SIZE];
    int32_t arrayBoundsCount;
} NewInstructionFields;

typedef struct PutInstructionFields
{
    char type[BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE];
    char fieldName[LARGE_BUFFER_SIZE];
    bool isStatic;
} PutInstructionFields;

typedef struct GetInstructionFields
{
    char type[BUFFER_SIZE];
    char className[LARGE_BUFFER_SIZE];
    char fieldName[LARGE_BUFFER_SIZE];
    bool isStatic;
} GetInstructionFields;

typedef struct InstanceofInstructionFields
{
    char type[LARGE_BUFFER_SIZE];
} InstanceofInstructionFields;

typedef struct ShiftInstructionFields
{
    char type[BUFFER_SIZE];
    ShrikeBTShiftOperator op;
} ShiftInstructionFields;

typedef struct SwitchInstructionFields
{
    int32_t casesAndLabels[1024];
    uint32_t length;
    int32_t defaultLabel;
} SwitchInstructionFields;

typedef struct ArrayLengthInstructionFields
{
} ArrayLengthInstructionFields;

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
    ShiftInstructionFields shiftInstructionFields;
    SwitchInstructionFields switchInstructionFields;
} ShrikeBTInstructionFieldsUnion;

/*** indicates how the offset of branch instruction should be adjusted ***/
typedef enum BranchTargetLabelAdjustType
{
    NO_ADJUST, // no need to adjust the branch target
    TABLE_MAP, // get the target label from the omr index to shrikeBT offset mapping table
    BY_VALUE,  // inc or dec the offset by certain amount
} BranchTargetLabelAdjustType;

/*** This structure contains all necessary information to do the OMR to ShrikeBT mapping ***/
typedef struct OWLInstruction
{
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