//
// Created by Cijie Xia on 2019-09-29.
//

#ifndef OMR_OWLSHRIKEBTCONSTRUCTOR_HPP
#define OMR_OWLSHRIKEBTCONSTRUCTOR_HPP

#include <unordered_map>
#include <vector>
#include "optimizer/OWLJNIClient.hpp"
#include "optimizer/OWLTypes.hpp"

class TR_OWLShrikeBTConstructor
{
private:
    TR_OWLJNIClient *_jniClient;
    uint32_t _index;
    std::unordered_map<int32_t, uint32_t> _localVarTableBySymRef;
    std::unordered_map<uint32_t, uint32_t> _localVarTableByOmrIndex;

public:
    TR_OWLShrikeBTConstructor(TR_OWLJNIClient* jniClient);
    ~TR_OWLShrikeBTConstructor();

    std::vector<jobject> constructShrikeBTInstructions(std::vector<OWLInstruction> owlInstructions);

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
    jobject StoreInstruction(char* type, int32_t referenceNumber);
    jobject ImplicitStoreInstruction(char* type, uint32_t omrGlobalIndex); // implicit store
    jobject LoadInstruction(char* type, int32_t referenceNumber);
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
