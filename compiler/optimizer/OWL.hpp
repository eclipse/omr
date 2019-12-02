//
// Created by Cijie Xia on 2019-09-08.
//

#ifndef OMR_OWL_H
#define OMR_OWL_H

#include "optimizer/Optimization.hpp"
#include "optimizer/OWLMapper.hpp"

class TR_OWL: public TR::Optimization
{

public:
    TR_OWL(TR::OptimizationManager *manager);
    static TR::Optimization *create(TR::OptimizationManager *manager);

    virtual int32_t perform();
    virtual const char* optDetailString() const throw();
};

#endif //OMR_OWL_H
