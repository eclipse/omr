#ifndef OWL_DESERIALIZER_HPP
#define OWL_DESERIALIZER_HPP

#include "optimizer/OWLTypes.hpp"
#include <stdio.h>
#include <vector>

class TR_OWLDeserializer
{
private:
    std::vector<TranslationUnit> _translationsUnits;
    MethodInfo _methodInfo;
    
public:
    void deserialize(char* filePath);
    std::vector<TranslationUnit> getTranslationUnits();
    MethodInfo getMethodInfo();
};

#endif