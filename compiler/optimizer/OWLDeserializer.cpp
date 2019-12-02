#include "optimizer/OWLDeserializer.hpp"

void TR_OWLDeserializer::deserialize(char* filePath){
    FILE *file; 
    file = fopen(filePath,"rb");
    if (file == NULL) {
        perror("Error in opening OWL.log when deserializing\n");
        exit(1);
    }


    if (fread(&_methodInfo, sizeof(MethodInfo),1, file) != 1){
        perror("Error in reading method info from OWL.log\n");
        exit(1);
    }

    uint64_t length;
    if (fread(&length, sizeof(uint64_t),1, file) != 1){
        perror("Error in reading array length from OWL.log\n");
        exit(1);
    }

    for (uint64_t i = 0 ; i < length; i ++){
        TranslationUnit translationUnit;
        if (fread(&translationUnit, sizeof(TranslationUnit),1,file) != 1){
            perror("Error in reading OWL instruction from OWL.log\n");
            exit(1);
        }
        _translationsUnits.push_back(translationUnit);
    }
}

MethodInfo TR_OWLDeserializer::getMethodInfo() {
    return _methodInfo;
}

std::vector<TranslationUnit> TR_OWLDeserializer::getTranslationUnits(){
    return _translationsUnits;
}