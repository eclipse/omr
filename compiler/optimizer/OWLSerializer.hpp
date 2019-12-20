#ifndef OWL_SERIALIZER_HPP
#define OWL_SERIALIZER_HPP

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "optimizer/OWLTypes.hpp"

class TR_OWLSerializer
{
public:
    void serialize(char* filePath, MethodInfo methodInfo, TranslationUnit* translationUnits, uint64_t size);
};


#endif