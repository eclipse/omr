//
// Created by Cijie Xia on 2019-09-08.
//

#include "optimizer/OWL.hpp"
#include "il/OMRNode_inlines.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "launch.h"
#include "JNIClient.hpp"
#include <iostream>

TR_OWL::TR_OWL(TR::OptimizationManager *manager)
        : TR::Optimization(manager)
{   }


TR::Optimization *TR_OWL::create(TR::OptimizationManager *manager)
{
    return new (manager->allocator()) TR_OWL(manager);
}

static void processTree(TR::Node *root, TR::NodeChecklist &visited) {
    if (visited.contains(root))
        return;
    visited.add(root);
    for (int32_t i = 0; i < root->getNumChildren(); ++i) {
        processTree(root->getChild(i), visited);
    }
    //printf("%s\n",root->getOpCode().getName());
}

int32_t TR_OWL::perform(){





    TR_JNIClient *jniClient = TR_JNIClient::getInstance();
    jobject obj;
    MethodConfig methodConfig1 ={
            "com/ibm/wala/shrikeBT/ConstantInstruction",
            "make",
            "(F)Lcom/ibm/wala/shrikeBT/ConstantInstruction;"
    };
    jniClient->callStaticMethod(methodConfig1,&obj,1,3.14f);

    char *str;
    MethodConfig methodConfig2={
            "com/ibm/wala/shrikeBT/ConstantInstruction",
            "toString",
            "()Ljava/lang/String;"
    };
    jniClient->callMethod(methodConfig2,obj,&str,0);
    std::cout << str << std::endl;

    short opcode;
    MethodConfig methodConfig3 ={
            "com/ibm/wala/shrikeBT/ConstantInstruction",
            "getOpcode",
            "()S"
    };

    jniClient->callMethod(methodConfig3,obj,&opcode,0);
    std::cout << "opcode "<< opcode <<std::endl;

    TR::NodeChecklist visited(comp()); // visited nodes

    for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop()){
        TR::Node *node = tt->getNode();
        processTree(node, visited);

    }
}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}