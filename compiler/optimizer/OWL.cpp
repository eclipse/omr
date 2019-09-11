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
    jclass cls = jniClient->getClass("com/ibm/wala/shrikeBT/ConstantInstruction");
//    jclass cls = env->FindClass( "com/ibm/wala/shrikeBT/ConstantInstruction");
//    if (cls == NULL) {
//        printf("Failed to find ConstantInstruction class");
//        return 1;
//    }
//
//    jmethodID mid = env->GetStaticMethodID(cls, "make", "(I)Lcom/ibm/wala/shrikeBT/ConstantInstruction;");
//    if (mid == NULL) {
//        printf("Failed to find make function");
//        return 1;
//    }
//    jmethodID mid2 = env->GetMethodID(cls, "toString", "()Ljava/lang/String;");
//    if (mid2 == NULL) {
//        printf("Failed to find toString function");
//        return 1;
//    }
//
//
//    printf("Successfully to find class and find method!!!!===========\n");
//
//    jobject res = env->CallStaticObjectMethod(cls,mid,1);
//    jstring stringObj =(jstring)(env->CallObjectMethod(res,mid2));
//    const char* string = env->GetStringUTFChars(stringObj,0);
//    printf("string %s\n",string);


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