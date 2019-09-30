//
// Created by Cijie Xia on 2019-09-08.
//
#include <iostream>

#include "optimizer/OWL.hpp"
#include "il/OMRNode_inlines.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "launch.h"
#include "OWLJNIClient.hpp"
#include "OWLMapper.hpp"

TR_OWL::TR_OWL(TR::OptimizationManager *manager)
        : TR::Optimization(manager)
{   }

TR::Optimization *TR_OWL::create(TR::OptimizationManager *manager)
{
    return new (manager->allocator()) TR_OWL(manager);
}

int32_t TR_OWL::perform(){

    TR_OWLJNIClient *jniClient = TR_OWLJNIClient::getInstance();
    TR_OWLMapper *mapper = new TR_OWLMapper();
    mapper->map(comp());

}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}