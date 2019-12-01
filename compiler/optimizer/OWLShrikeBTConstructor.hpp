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
    jobject BinaryOperator(ShrikeBTBinaryOperator op);
    jobject ConditionalBranchOperator(ShrikeBTConditionalBranchOperator op);
    jobject ComparisonOperator(ShrikeBTComparisonOperator op);
    jobject ShiftOperator(ShrikeBTShiftOperator op);

    jobject Dispatch(ShrikeBTDispatch disp);

    /*** ShirkeBT Instruction Constructors ***/
    jobject ConstantInstruction(char* type, jobject value);
    jobject StoreInstruction(char* type, uint32_t index);
    jobject ImplicitStoreInstruction(char* type, uint32_t omrGlobalIndex); // implicit store
    jobject LoadInstruction(char* type, uint32_t index);
    jobject ImplicitLoadInstruction(char* type, uint32_t omrGloablIndex); // implicit load
    jobject BinaryOpInstruction(char* type, ShrikeBTBinaryOperator op);
    jobject ReturnInstruction(char* type);
    jobject GotoInstruction(uint32_t label);
    jobject ConditionalBranchInstruction(char* type, ShrikeBTConditionalBranchOperator op, uint32_t label);
    jobject ComparisonInstruction(char* type, ShrikeBTComparisonOperator op);
    jobject ConversionInstruction(char* fromType, char* toType);
    jobject UnaryOpInstruction(char* type);
    jobject InvokeInstruction(char* type, char* className, char* methodName, ShrikeBTDispatch disp);
    jobject SwapInstruction();
    jobject PopInstruction(uint16_t size); // 1 or 2
    jobject DupInstruction(uint16_t delta); // delta 0 => dup, delta 1 => dup_x1
    jobject ArrayStoreInstruction(char* type);
    jobject ArrayLoadInstruction(char* type);
    jobject NewInstruction(char* type, int32_t arrayBoundsCount);
    jobject PutInstruction(char* type, char* className, char* fieldName, bool isStatic);
    jobject GetInstruction(char* type, char* className, char* fieldName, bool isStatic);
    jobject InstanceofInstruction(char* type);
    jobject ArrayLengthInstruction();
    jobject ShiftInstruction(char* type, ShrikeBTShiftOperator op);
    jobject SwitchInstruction(int* casesAndLabels, int length, int defaultLabel);
    jobject CheckCastInstruction(char* type);

};
#endif 
