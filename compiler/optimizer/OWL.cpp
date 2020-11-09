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
#include "optimizer/OWLSerializer.hpp"


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

    std::vector<TranslationUnit>  translationUnits = mapper->map();
    delete mapper;

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

    TR_OWLSerializer *serializer = new TR_OWLSerializer();
    char* filePath = "/home/jackxia/Project/IBM/openj9-openjdk-jdk13/OWL/OWL.log";
    serializer->serialize(filePath, methodInfo, &translationUnits[0], translationUnits.size());
    delete serializer;
       
}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}