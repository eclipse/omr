#include "optimizer/OWLSerializer.hpp"

void TR_OWLSerializer::serialize(char* filePath, MethodInfo methodInfo, TranslationUnit* translationUnits, uint64_t size) {

    FILE *file = fopen(filePath,"wb");
    if (file == NULL) {
        printf("Error in opening OWL.log file\n");
        exit(1);
    }

    if (fwrite(&methodInfo,sizeof(MethodInfo),1,file) !=1){
        printf("Error in writing method info when serializing\n");
        exit(1);
    }

    if (fwrite(&size, sizeof(uint64_t),1,file) != 1){
        printf("Error in writing array length when serializing\n");
        exit(1);
    }

    //write each OWL instruction to file
    for (uint64_t i = 0 ; i < size; i ++){

        if (fwrite(&translationUnits[i], sizeof(TranslationUnit), 1, file) != 1) {
            printf("Error in writing OWL instruction when serializing\n");
            exit(1);
        }
    }

    fclose(file);
}