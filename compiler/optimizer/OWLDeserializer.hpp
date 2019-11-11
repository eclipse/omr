#ifndef OWL_DESERIALIZER_HPP
#define OWL_DESERIALIZER_HPP

#include "optimizer/OWLTypes.hpp"
#include <stdio.h>
#include <vector>

class TR_OWLDeserializer
{
public:
    std::vector<OWLInstruction> deserialize();
};

#endif