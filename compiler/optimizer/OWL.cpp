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
#include "optimizer/OWLMapper.hpp"

TR_OWL::TR_OWL(TR::OptimizationManager *manager)
        : TR::Optimization(manager)
{   }

TR::Optimization *TR_OWL::create(TR::OptimizationManager *manager)
{
    return new (manager->allocator()) TR_OWL(manager);
}

int32_t TR_OWL::perform(){

    TR_OWLMapper *mapper = new TR_OWLMapper();
    mapper->map(comp());
    delete mapper;
}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}