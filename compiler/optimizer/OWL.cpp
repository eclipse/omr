//
// Created by Cijie Xia on 2019-09-08.
//
#include <vector>
#include <stdio.h>
#include <string.h>
#include "optimizer/OWL.hpp"
#include "il/OMRNode_inlines.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/OWLTypes.hpp"
#include "optimizer/OWLJNIClient.hpp"
#include "optimizer/OWLShrikeBTConstructor.hpp"
#include "optimizer/OWLSerializer.hpp"
#include "optimizer/OWLDeserializer.hpp"
#include "optimizer/OWLAnalyser.hpp"
#include "optimizer/OWLVerifier.hpp"

TR_OWL::TR_OWL(TR::OptimizationManager *manager)
        : TR::Optimization(manager)
{  
}


TR::Optimization *TR_OWL::create(TR::OptimizationManager *manager)
{
    return new (manager->allocator()) TR_OWL(manager);
}

int32_t TR_OWL::perform()
{   

    
    TR_OWLMapper *mapper = new TR_OWLMapper(comp());
    printf("=== Start mapping ===\n");
    std::vector<OWLInstruction>  owlInstructions = mapper->map();
    delete mapper;
    printf("==== Finish mapping ====\n");


    //save the current method info
    MethodInfo methodInfo;

    char* className = comp()->getMethodSymbol()->getResolvedMethod()->classNameChars();
    uint16_t classNameLength = comp()->getMethodSymbol()->getResolvedMethod()->classNameLength();
    strncpy(methodInfo.className,className, classNameLength);
    methodInfo.className[classNameLength] = '\0';

    char* methodName = comp()->getMethodSymbol()->getResolvedMethod()->nameChars();
    uint16_t methodNameLength = comp()->getMethodSymbol()->getResolvedMethod()->nameLength();
    strncpy(methodInfo.methodName, methodName, methodNameLength);
    methodInfo.methodName[methodNameLength] = '\0';

    char* signature = comp()->getMethodSymbol()->getResolvedMethod()->signatureChars();
    uint16_t signatureLength = comp()->getMethodSymbol()->getResolvedMethod()->signatureLength();
    strncpy(methodInfo.signature, signature, signatureLength);
    methodInfo.signature[signatureLength] = '\0';

    //Test if JVM can be started
    if (TR_OWLJNIClient::startJVM()){ // run the analysis directly
        TR_OWLJNIClient* jniClient = TR_OWLJNIClient::getInstance();

        TR_OWLShrikeBTConstructor *constructor = new TR_OWLShrikeBTConstructor(jniClient);
        std::vector<jobject> shrikeBTInstructions = constructor->constructShrikeBTInstructions(owlInstructions);
        

        TR_OWLVerifier *verifier = new TR_OWLVerifier(jniClient);
        TR_OWLAnalyser *analyser = new TR_OWLAnalyser(jniClient);

        bool status = verifier->verify(&shrikeBTInstructions[0],shrikeBTInstructions.size(), false, true, methodInfo.className, methodInfo.signature);
        if (!status) {
            printf("Fail to verify shrikeBT Instructions\n");
        }
        else {
            printf("Successful to verify shrikeBT Instructions\n")
        }

        analyser->analyse(shrikeBTInstructions);

        delete verifier;
        delete analyser;
        delete constructor;
    }
    else { // serialize OWL instructions to files
        printf("===JVM cannot be started. Serialize OWL Instruction to file===\n");
        TR_OWLSerializer *serializer = new TR_OWLSerializer();
        serializer->serialize(methodInfo, owlInstructions);
        delete serializer;
    }
    
}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}