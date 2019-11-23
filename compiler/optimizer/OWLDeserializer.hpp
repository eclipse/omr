#ifndef OWL_DESERIALIZER_HPP
#define OWL_DESERIALIZER_HPP

#include "optimizer/OWLTypes.hpp"
#include <stdio.h>
#include <vector>

class TR_OWLDeserializer
{
private:
    std::vector<OWLInstruction> _owlInstructionList;
    MethodInfo _methodInfo;
    void _deserialize();
public:
    TR_OWLDeserializer();
    std::vector<OWLInstruction> getOWLInstructionList();
    MethodInfo getMethodInfo();
};

#endif