#include "optimizer/OWLLogger.hpp"
#include "optimizer/OWLJNIConfig.hpp"

TR_OWLLogger::TR_OWLLogger() {
    _jniClient = TR_OWLJNIClient::getInstance();
}

TR_OWLLogger::~TR_OWLLogger() {

}

/*** WARNING: It is user's responsibility to free the memory whoever uses this method ***/
char * TR_OWLLogger::getInstructionString(jobject instructionObj, ShrikeBTInstruction ins) {
    MethodConfig methodConfig;
    switch(ins){
        case CONSTANT: methodConfig = ConstantInstructionToStringConfig; break;
        case BINARY_OP: methodConfig = BinaryOpInstructionToStringConfig; break;
        case UNARY_OP: methodConfig = UnaryOpInstructionToStringConfig; break;
        case IMPLICIT_LOAD:
        case LOAD: methodConfig = LoadInstructionToStringConfig; break;
        case IMPLICIT_STORE:
        case STORE: methodConfig = StoreInstructionToStringConfig; break;
        case RETURN: methodConfig = ReturnInstructionToStringConfig; break;
        case GOTO: methodConfig = GotoInstructionToStringConfig; break;
        case CONDITIONAL_BRANCH: methodConfig = ConstantInstructionToStringConfig; break;
        case COMPARISON: methodConfig = ComparisonInstructionToStringConfig; break;
        case CONVERSION: methodConfig = ConversionInstructionToStringConfig; break;
        case INVOKE: methodConfig = InvokeInstructionToStringConfig; break;
        case SWAP: methodConfig = SwapInstructionToStringConfig; break;
        case POP: methodConfig = PopInstructionToStringConfig; break;

        default: perror("Error: Instruction enum not found!\n"); exit(1); break;
    }

    char *instructionString = (char*)malloc(1024);

    _jniClient->callMethod
    (
        methodConfig,
        instructionObj,
        &instructionString,
        0
    );
    return instructionString;
}


void TR_OWLLogger::logSingleInstruction(jobject instructionObj, ShrikeBTInstruction instruction, uint32_t offset) {
    char* str = getInstructionString(instructionObj, instruction);
    printf("%d: %s\n",offset, str);
    free(str);
}

void TR_OWLLogger::logAllMappedInstructions(std::vector<MappedInstructionObject> mappedInstructionObjectList) {
    for (uint32_t i = 0 ; i < mappedInstructionObjectList.size(); i ++) {
        MappedInstructionObject mappedInstructionObject = mappedInstructionObjectList[i];
        logSingleInstruction(mappedInstructionObject.instructionObject, mappedInstructionObject.instruction, mappedInstructionObject.offset );
    }
    
}



