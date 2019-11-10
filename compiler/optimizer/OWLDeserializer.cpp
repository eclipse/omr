#include "optimizer/OWLDeserializer.hpp"

std::vector<OWLInstruction> deserialize(){
    FILE *file; 
    file = fopen("/Users/jackxia/Project/IBM/openj9-openjdk-jdk13/OWL.log","rb");
    if (file == NULL) {
        perror("Error in opening OWL.log when deserializing\n");
        exit(1);
    }

    uint64_t length;
    if (fread(&length, sizeof(uint64_t),1, file) != 1){
        perror("Error in reading array length from OWL.log\n");
        exit(1);
    }

    std::vector<OWLInstruction> owlInstructions;

    for (uint64_t i = 0 ; i < length; i ++){
        OWLInstruction owlInstruction;
        if (fread(&owlInstruction, sizeof(OWLInstruction),1,file) != 1){
            perror("Error in reading OWL instruction from OWL.log\n");
            exit(1);
        }
        owlInstructions.push_back(owlInstruction);
    }

    return owlInstructions;
}