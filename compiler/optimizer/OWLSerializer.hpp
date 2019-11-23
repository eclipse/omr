#ifndef OWL_SERIALIZER_HPP
#define OWL_SERIALIZER_HPP

#include <stdio.h>
#include <vector>
#include "optimizer/OWLTypes.hpp"

class TR_OWLSerializer
{
public:
    void serialize(MethodInfo methodInfo, std::vector<OWLInstruction> owlInstructions);
};


#endif