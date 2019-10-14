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


TR_OWL::TR_OWL(TR::OptimizationManager *manager)
        : TR::Optimization(manager)
{  
    _mapper = new TR_OWLMapper();
}

TR_OWL::~TR_OWL()
{
    delete _mapper;
}

TR::Optimization *TR_OWL::create(TR::OptimizationManager *manager)
{
    return new (manager->allocator()) TR_OWL(manager);
}

int32_t TR_OWL::perform()
{   
    _mapper->map(comp());
}

const char *
TR_OWL::optDetailString() const throw()
{
    return "O^O OWL: ";
}