#include "optimizer/OWLDeserializer.hpp"

void TR_OWLDeserializer::_deserialize(){
    FILE *file; 
    file = fopen("/Users/jackxia/Project/IBM/openj9-openjdk-jdk13/OWL/OWL.log","rb");
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
        OWLInstruction owlInstruction;
        if (fread(&owlInstruction, sizeof(OWLInstruction),1,file) != 1){
            perror("Error in reading OWL instruction from OWL.log\n");
            exit(1);
        }
        _owlInstructionList.push_back(owlInstruction);
    }
}

TR_OWLDeserializer::TR_OWLDeserializer(){
    _deserialize();
}

MethodInfo TR_OWLDeserializer::getMethodInfo() {
    return _methodInfo;
}

std::vector<OWLInstruction> TR_OWLDeserializer::getOWLInstructionList(){
    return _owlInstructionList;
}