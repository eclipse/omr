#include "optimizer/OWLSerializer.hpp"

void TR_OWLSerializer::serialize(MethodInfo methodInfo, std::vector<OWLInstruction> owlInstructions) {
    uint64_t length = owlInstructions.size();

    FILE *file = fopen("/Users/jackxia/Project/IBM/openj9-openjdk-jdk13/OWL/OWL.log","wb");
    if (file == NULL) {
        perror("Error in opening OWL.log file\n");
        exit(1);
    }

    if (fwrite(&methodInfo,sizeof(MethodInfo),1,file) !=1){
        perror("Error in writing method info when serializing\n");
        exit(1);
    }

    if (fwrite(&length, sizeof(uint64_t),1,file) != 1){
        perror("Error in writing array length when serializing\n");
        exit(1);
    }

    //write each OWL instruction to file
    for (uint64_t i = 0 ; i < length; i ++){

        if (fwrite(&owlInstructions[i], sizeof(OWLInstruction), 1, file) != 1) {
            perror("Error in writing OWL instruction when serializing\n");
            exit(1);
        }
    }

    fclose(file);
    printf("=== Finish serializing to OWL.log ===\n");
}