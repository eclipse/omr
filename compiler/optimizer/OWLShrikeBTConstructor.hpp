//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLSHRIKEBTCONSTRUCTOR_HPP
#define OMR_OWLSHRIKEBTCONSTRUCTOR_HPP

#include <unordered_map>
#include "optimizer/OWLJNIClient.hpp"
#include "il/OMRSymbolReference.hpp"

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
    POP,
    ARRAY_STORE,
    ARRAY_LOAD

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

struct ArrayStoreInstructionFields {
    char* type;
};

struct ArrayLoadInstructionFields {
    char* type;
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
    ArrayStoreInstructionFields arrayStoreInstructionFields;
    ArrayLoadInstructionFields arrayLoadInstructionFields;
};


class TR_OWLShrikeBTConstructor
{
private:
    TR_OWLJNIClient *_jniClient;
    uint32_t _index;
    std::unordered_map<TR::SymbolReference *, uint32_t> _localVarTableBySymRef;
    std::unordered_map<uint32_t, uint32_t> _localVarTableByOmrIndex;

public:
    TR_OWLShrikeBTConstructor();
    ~TR_OWLShrikeBTConstructor();

    /*** helper methods ***/
    
    jobject Integer(int32_t i);
    jobject Float(float f);
    jobject Double(double d);
    jobject Long(int64_t l);
    jobject Short(int16_t s);
    jobject Operator(ShrikeBTOperator op);
    jobject Dispatch(ShrikeBTDispatch disp);

    /*** ShirkeBT Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject ImplicitStoreInstruction(char* type, uint32_t omrGlobalIndex); // implicit store
    jobject LoadInstruction(char* type, TR::SymbolReference * symbolReference);
    jobject ImplicitLoadInstruction(char* type, uint32_t omrGloablIndex); // implicit load
    jobject BinaryOpInstruction(char* type, ShrikeBTOperator op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(uint32_t label);
    jobject ConditionalBranchInstruction(char* type, ShrikeBTOperator op, uint32_t label);
    jobject ComparisonInstruction(char* type, ShrikeBTOperator op);
    jobject ConversionInstruction(char* fromType, char* toType);
    jobject UnaryOpInstruction(char* type);
    jobject InvokeInstruction(char* type, char* className, char* methodName, ShrikeBTDispatch disp);
    jobject SwapInstruction();
    jobject PopInstruction(uint16_t size);
    jobject ArrayStoreInstruction(char* type);
    jobject ArrayLoadInstruction(char* type);

};
#endif 
